/* The YUV converter (TA_YUV_TEX_BASE 0x148, TA_YUV_TEX_CTRL 0x14c, TA_YUV_TEX_CNT 0x150; 2026-11-19, TODO B).
   Macroblocks go to the TA's YUV input (0x10800000, store queues, 32 bytes at a time through pvrio's FIFO guard)
   after TA_YUV_TEX_BASE and TA_YUV_TEX_CTRL are written; the converter writes YUV422 texels to texture memory.
   Every input byte is known (cases.h: three variants, whose pair (v0, v1) names the input byte and v2 checks it),
   so the dumps show where each byte went, whether YUV420's chroma is repeated or averaged, the byte order, the
   layout for YUV_Tex 0 and 1, what happens past the texture's macroblocks and when TA_YUV_TEX_BASE is written
   in the middle of one.

   Per case and variant: YUV_AREA bytes from YUV_DEST cleared to YUV_FILL (64 bit area), the registers written,
   the macroblocks sent; after each one TA_YUV_TEX_CNT and the YUV-done interrupts seen so far are printed
   ("Y <case> <variant> <macroblock> cnt <n> done <events>", macroblock -1: right after the registers); then the
   area is dumped as its two 32 bit halves (texture memory interleaves them every 4 bytes): c<case>v<variant>lo
   and c<case>v<variant>hi (.hw.raw on a console). The PC build sends no YUV data (refsw3-offline has no
   converter): its dumps are the clear pattern, so that run2.sh off runs.

   Scored by agents/nulldc3/prep/yuvscore (against nulldc3's pvr_yuv.cpp and the other candidate layouts). */
#define NUM_PAGES 72            /* dumps: 12 cases x 3 variants x 2 halves */
#include "pvr_params.h"
#include "pvrio.h"
#include "cases.h"

#ifdef _arch_dreamcast
#include <dc/sq.h>
static volatile uint32_t yuv_done;
static void yuv_event(uint32_t code, void *data) {
    (void)code; (void)data;
    yuv_done++;
}
static uint32_t yuv_cnt(void) {
    pvrio_guard_read();
    return *(volatile uint32_t *)(0xa05f8000 + TA_YUV_TEX_CNT_addr);
}
/* bytes to the converter, 32 at a time, while the FIFO has room */
static void yuv_send(const uint8_t *bytes, int n) {
    static uint32_t block[8] __attribute__((aligned(32)));
    int i;
    for(i = 0; i < n; i += 32) {
        memcpy(block, bytes + i, 32);
        if(!pvrio_guard(2, 0)) {
            printf("Y stall at byte %d\n", i);
            return;
        }
        sq_cpy((void *)0x10800000, block, 32);
    }
}
/* until TA_YUV_TEX_CNT is not before (5 ms at most) */
static uint32_t yuv_wait(uint32_t before) {
    uint64_t t0 = timer_us_gettime64();
    uint32_t c;
    while((c = yuv_cnt()) == before && timer_us_gettime64() - t0 < 5000)
        ;
    return c;
}
#endif

static void clear_area(void) {
    static uint32_t fill[YUV_AREA / 4];
    unsigned i;
    for(i = 0; i < YUV_AREA / 4; i++)
        fill[i] = YUV_FILL;
    pvrio_w64(YUV_DEST, fill, YUV_AREA / 4);
}

static void run_case(int c, int v) {
    const yuv_case_t *k = &yuv_cases[c];
    static uint8_t mb[512] __attribute__((aligned(32)));
    char name[32];
    const int nbytes = yuv_mb_bytes(k->form);
    int m, i;

    clear_area();
    pvrio_reg(TA_YUV_TEX_BASE_addr, YUV_DEST);
    pvrio_reg(TA_YUV_TEX_CTRL_addr, (uint32_t)(k->form << 24 | k->tex << 16 | k->vsize << 8 | k->usize));
#ifdef _arch_dreamcast
    (void)*(volatile uint32_t *)(0xa05f8000 + TA_YUV_TEX_CTRL_addr);     /* read back, as the KallistiOS examples do */
    yuv_done = 0;
    {
        uint32_t cnt = yuv_cnt();
        const int first = k->special == YS_REBASE ? 1 : 0;
        printf("Y %d %d -1 cnt %lu done %lu\n", c, v, (unsigned long)cnt, (unsigned long)yuv_done);
        if(k->special == YS_REBASE) {           /* half of macroblock 0, then the converter restarted */
            for(i = 0; i < nbytes; i++)
                mb[i] = yuv_byte(c, v, 0, i);
            yuv_send(mb, nbytes / 2);
            pvrio_reg(TA_YUV_TEX_BASE_addr, YUV_DEST + YUV_REBASE);
            (void)*(volatile uint32_t *)(0xa05f8000 + TA_YUV_TEX_BASE_addr);
            cnt = yuv_cnt();
            printf("Y %d %d half cnt %lu done %lu\n", c, v, (unsigned long)cnt, (unsigned long)yuv_done);
        }
        for(m = first; m < first + k->nmb; m++) {   /* macroblock m: its bytes yuv_byte(c, v, m, .) */
            for(i = 0; i < nbytes; i++)
                mb[i] = yuv_byte(c, v, m, i);
            yuv_send(mb, nbytes);
            cnt = yuv_wait(cnt);
            printf("Y %d %d %d cnt %lu done %lu\n", c, v, m, (unsigned long)cnt, (unsigned long)yuv_done);
        }
    }
#else
    (void)mb; (void)nbytes; (void)m; (void)i;
#endif
    snprintf(name, sizeof(name), "c%02dv%dlo", c, v);
    pvrio_dump32(YUV_DEST / 2, YUV_AREA / 8, name, ".raw");
    snprintf(name, sizeof(name), "c%02dv%dhi", c, v);
    pvrio_dump32(0x400000 + YUV_DEST / 2, YUV_AREA / 8, name, ".raw");
}

int main(int argc, char **argv) {
    int c, v;
#ifndef _arch_dreamcast
    if(argc > 1 && !strcmp(argv[1], "--labels")) {
        for(c = 0; c < YUV_NCASES; c++)
            printf("case %d: %s\n", c, yuv_cases[c].what);
        return 0;
    }
    if(argc > 1)
        pvrio_folder = argv[1];
#else
    (void)argc; (void)argv;
#endif
    pvrio_open();
#ifdef _arch_dreamcast
    asic_evt_set_handler(ASIC_EVT_PVR_YUV_DONE, yuv_event, NULL);
    asic_evt_enable(ASIC_EVT_PVR_YUV_DONE, ASIC_IRQ_DEFAULT);
#endif
    pvrio_reg(SOFTRESET_addr, 1);           /* the TA (and with it the converter) from a known state */
    pvrio_reg(SOFTRESET_addr, 0);
    for(c = 0; c < YUV_NCASES; c++)
        for(v = 0; v < YUV_VARIANTS; v++)
            run_case(c, v);
    pvrio_close();
    return 0;
}
