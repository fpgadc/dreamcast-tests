/* Everything a test does to the PVR goes through the six calls below, and they have two implementations:

     on a PC         they print commands for nulldc3's refsw2-offline (the refsw2 renderer and the tile
                     accelerator without the rest of the emulator):   ./test <folder> | refsw2-offline -
     on a Dreamcast  they do the thing on the hardware, directly: registers, video memory, store queues.
                     KallistiOS's PVR driver is not used (and its functions are poisoned below, so that a
                     test cannot use them by accident); its video driver sets the display mode. The same
                     program runs in an emulator.

   So one test source gives the same register writes, the same video memory and the same tile accelerator
   input to the offline renderer, to a console and to an emulator. To show that it did, every call is also
   fed to a checksum, which pvrio_dump32() prints before it dumps: the lines "PVRIO crc ..." of two runs
   are the same when the two runs sent the same things in the same order. */
#ifndef PVRIO_H
#define PVRIO_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pvr_params.h"

/* events: bits of the normal interrupt status (SB_ISTNRM) */
#define PVRIO_RENDER_DONE       (1u << 2)
#define PVRIO_LIST_DONE(list)   ((list) == ListType_Punch_Through ? 1u << 21 : 1u << (7 + (list)))

static void pvrio_open(void);
static void pvrio_video(int fb_packmode, uint32_t fb);      /* display mode 640x480 showing the framebuffer at fb */
static void pvrio_reg(uint32_t addr, uint32_t value);       /* write a register (offset from 0x005f8000) */
static void pvrio_w32(uint32_t addr, const uint32_t *words, int count);    /* video memory, 32 bit area */
static void pvrio_w64(uint32_t addr, const uint32_t *words, int count);    /* video memory, 64 bit area (textures) */
static void pvrio_ta(const void *params, int words);        /* to the tile accelerator; a multiple of 8 words */
static void pvrio_wait(uint32_t events);                    /* until all of these have happened */
static void pvrio_vblank(void);                             /* until the next vertical blank (nothing on a PC) */
/* 32 bit area to a file: <folder>/<name>.off<suffix> on a PC, DUMP_DIR/<name>.hw<suffix> on a console (there
   with .rle added and as pairs of (count, word) when that is shorter) */
static void pvrio_dump32(uint32_t addr, int words, const char *name, const char *suffix);
/* the tile accelerator's state to a file, four words (ta-tests): TA_ISP_CURRENT, TA_NEXT_OPB, the list-done
   interrupts (SB_ISTNRM bits 7-10 and 21) and the error interrupts (SB_ISTERR bits) seen since the last call */
static void pvrio_dump_ta_state(const char *name, const char *suffix);
static uint32_t pvrio_ta_events(void);                      /* list-done bits seen so far (on a PC: all of them) */
static void pvrio_close(void);
#define PVRIO_TA_EVENTS         ((0xfu << 7) | (1u << 21))

/* ---- the checksum ---- */
static uint32_t pvrio_crc = 0;

static void pvrio_hash(uint32_t kind, uint32_t addr, const uint32_t *words, int count) {
    static uint32_t table[256];
    uint32_t head[3] = { kind, addr, (uint32_t)count }, c = ~pvrio_crc, v;
    int i, k, part;

    if(!table[1])
        for(i = 0; i < 256; i++) {
            v = (uint32_t)i;
            for(k = 0; k < 8; k++)
                v = (v & 1) ? 0xedb88320u ^ (v >> 1) : v >> 1;
            table[i] = v;
        }
    for(part = 0; part < 2; part++) {
        const uint32_t *w = part ? words : head;
        int n = part ? count : 3;
        for(i = 0; i < n; i++)
            for(k = 0; k < 32; k += 8)
                c = table[(c ^ (w[i] >> k)) & 255] ^ (c >> 8);
    }
    pvrio_crc = ~c;
}

static void pvrio_hw_reg(uint32_t addr, uint32_t value);
static void pvrio_hw_mem(int area64, uint32_t addr, const uint32_t *words, int count);
static void pvrio_hw_ta(const uint32_t *words, int count);
static void pvrio_hw_wait(uint32_t events);
static void pvrio_hw_dump32(uint32_t addr, int words, const char *name, const char *suffix);
static void pvrio_hw_dump_ta_state(const char *name, const char *suffix);
static void pvrio_hw_say(const char *text);

static void pvrio_reg(uint32_t addr, uint32_t value) {
    pvrio_hash('R', addr, &value, 1);
    pvrio_hw_reg(addr, value);
}
static void pvrio_w32(uint32_t addr, const uint32_t *words, int count) {
    pvrio_hash('M', addr, words, count);
    pvrio_hw_mem(0, addr, words, count);
}
static void pvrio_w64(uint32_t addr, const uint32_t *words, int count) {
    pvrio_hash('T', addr, words, count);
    pvrio_hw_mem(1, addr, words, count);
}
static void pvrio_ta(const void *params, int words) {
    pvrio_hash('A', 0, (const uint32_t *)params, words);
    pvrio_hw_ta((const uint32_t *)params, words);
}
static void pvrio_wait(uint32_t events) {
    pvrio_hash('W', events, 0, 0);
    pvrio_hw_wait(events);
}
static void pvrio_dump32(uint32_t addr, int words, const char *name, const char *suffix) {
    char line[160];
    snprintf(line, sizeof(line), "PVRIO crc %08x before %s", (unsigned)pvrio_crc, name);
    pvrio_hw_say(line);
    pvrio_hw_dump32(addr, words, name, suffix);
}
static void pvrio_dump_ta_state(const char *name, const char *suffix) {
    char line[160];
    snprintf(line, sizeof(line), "PVRIO crc %08x before %s", (unsigned)pvrio_crc, name);
    pvrio_hw_say(line);
    pvrio_hw_dump_ta_state(name, suffix);
}

#ifndef _arch_dreamcast
/* ======== on a PC: commands for refsw2-offline on standard output ======== */

static const char *pvrio_folder = ".";
static int pvrio_mute;                      /* print nothing (the test is only asked for its labels) */
#define printf(...) (pvrio_mute ? 0 : printf(__VA_ARGS__))

static const char *pvrio_reg_name(uint32_t addr) {
    switch(addr) {
#define PVR_REGISTER_CASE(name, offset) case offset: return #name;
        PVR_REGISTERS(PVR_REGISTER_CASE)
#undef PVR_REGISTER_CASE
    }
    return 0;
}

static void pvrio_open(void) {
    printf("reset\n");
}

static void pvrio_video(int fb_packmode, uint32_t fb) {
    /* what KallistiOS's video driver sets with a VGA cable; only refsw2-offline's "png" reads these */
    int words = fb_packmode == fb_packmode_0888 ? 640 : 320;        /* 32 bit words per line */
    int depth = fb_packmode == fb_packmode_0888 ? 3 : fb_packmode == fb_packmode_565 ? 1 : 0;
    printf("# display\n");
    printf("reg FB_R_CTRL %08x\n", (unsigned)(0x00800001 | depth << 2));
    printf("reg FB_R_SIZE %08x\n", (unsigned)(1 << 20 | 479 << 10 | (words - 1)));
    printf("reg FB_R_SOF1 %08x\nreg FB_R_SOF2 %08x\nreg SPG_CONTROL 00000100\n", (unsigned)fb, (unsigned)fb);
}

static void pvrio_hw_reg(uint32_t addr, uint32_t value) {
    const char *name = pvrio_reg_name(addr);
    if(name)
        printf("reg %s %08x\n", name, (unsigned)value);
    else
        printf("reg %04x %08x\n", (unsigned)addr, (unsigned)value);
}

static void pvrio_hw_mem(int area64, uint32_t addr, const uint32_t *words, int count) {
    int i;
    for(i = 0; i < count; i++) {
        if(i % 8 == 0)
            printf("%s%s %06x", i ? "\n" : "", area64 ? "w64" : "w32", (unsigned)(addr + i * 4));
        printf(" %08x", (unsigned)words[i]);
    }
    if(count)
        printf("\n");
}

static void pvrio_hw_ta(const uint32_t *words, int count) {
    int i;
    for(i = 0; i < count; i++)
        printf("%s%08x%s", i % 8 ? " " : "ta ", (unsigned)words[i], i % 8 == 7 ? "\n" : "");
}

static void pvrio_hw_wait(uint32_t events) {
    printf("wait %08x\n", (unsigned)events);
}

static void pvrio_hw_dump32(uint32_t addr, int words, const char *name, const char *suffix) {
    printf("dump32 %x %x %s/%s.off%s\n", (unsigned)addr, (unsigned)words, pvrio_folder, name, suffix);
}

static void pvrio_hw_dump_ta_state(const char *name, const char *suffix) {
    printf("tastate %s/%s.off%s\n", pvrio_folder, name, suffix);
}

static uint32_t pvrio_ta_events(void) {
    return PVRIO_TA_EVENTS;
}

static void pvrio_hw_say(const char *text) {
    printf("echo %s\n", text);
}

static void pvrio_vblank(void) {
}

static void pvrio_close(void) {
    fflush(stdout);
}
#undef printf

#else
/* ======== on a Dreamcast: the hardware ======== */
#include <kos.h>

/* not to be used with this interface */
#pragma GCC poison pvr_init pvr_init_defaults pvr_shutdown pvr_scene_begin pvr_scene_finish pvr_list_begin
#pragma GCC poison pvr_list_finish pvr_prim pvr_wait_ready pvr_poly_compile pvr_poly_cxt_col pvr_poly_cxt_txr
#pragma GCC poison pvr_sprite_compile pvr_sprite_cxt_col pvr_sprite_cxt_txr pvr_mod_compile pvr_mem_malloc
#pragma GCC poison pvr_txr_load pvr_txr_load_ex pvr_set_bg_color pvr_set_pal_entry pvr_set_pal_format

#ifndef DUMP_DIR
#define DUMP_DIR "/pc/tmp/pvr_tests/out"
#endif

static volatile uint32_t pvrio_events, pvrio_errors;
static volatile uint32_t pvrio_seen_events, pvrio_seen_errors;     /* not cleared by pvrio_wait (ta-tests) */

/* called by KallistiOS's interrupt handler, which acknowledges every pending event itself: so the events
   have to be collected here and cannot be polled from the status register */
static void pvrio_event(uint32_t code, void *data) {
    (void)data;
    if((code >> 8) == 2) {
        pvrio_errors |= 1u << (code & 31);
        pvrio_seen_errors |= 1u << (code & 31);
    } else {
        pvrio_events |= 1u << (code & 31);
        pvrio_seen_events |= 1u << (code & 31);
    }
}

static void pvrio_open(void) {
    static const uint16_t codes[] = {
        ASIC_EVT_PVR_RENDERDONE_TSP, ASIC_EVT_PVR_OPAQUEDONE, ASIC_EVT_PVR_OPAQUEMODDONE, ASIC_EVT_PVR_TRANSDONE,
        ASIC_EVT_PVR_TRANSMODDONE, ASIC_EVT_PVR_PTDONE, ASIC_EVT_PVR_ISP_OUTOFMEM, ASIC_EVT_PVR_STRIP_HALT,
        ASIC_EVT_PVR_PARAM_OUTOFMEM, ASIC_EVT_PVR_OPB_OUTOFMEM, ASIC_EVT_PVR_TA_INPUT_ERR, ASIC_EVT_PVR_TA_INPUT_OVERFLOW
    };
    unsigned i;
    for(i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        asic_evt_set_handler(codes[i], pvrio_event, NULL);
        asic_evt_enable(codes[i], ASIC_IRQ_DEFAULT);
    }
}

static void pvrio_video(int fb_packmode, uint32_t fb) {
    vid_set_mode(DM_640x480, fb_packmode == fb_packmode_0888 ? PM_RGB0888 : fb_packmode == fb_packmode_565 ? PM_RGB565 : PM_RGB555);
    vid_set_start(fb);
}

static void pvrio_hw_reg(uint32_t addr, uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)(0xa05f8000 + addr);
    *reg = value;
    if(addr == TA_LIST_INIT_addr)
        (void)*reg;                         /* as KallistiOS does: read it back before sending anything */
}

static void pvrio_hw_mem(int area64, uint32_t addr, const uint32_t *words, int count) {
    volatile uint32_t *dst = (volatile uint32_t *)((area64 ? 0xa4000000 : 0xa5000000) + addr);
    int i;
    for(i = 0; i < count; i++)
        dst[i] = words[i];
}

static void pvrio_hw_ta(const uint32_t *words, int count) {
    sq_cpy((void *)0x10000000, words, count * 4);
}

static void pvrio_hw_wait(uint32_t events) {
    uint64_t start = timer_ms_gettime64();
    int old;
    while((pvrio_events & events) != events)
        if(timer_ms_gettime64() - start > 2000) {
            printf("pvrio: still waiting for events %08x after 2 s (have %08x, errors %08x); going on\n",
                   (unsigned)(events & ~pvrio_events), (unsigned)pvrio_events, (unsigned)pvrio_errors);
            if(events & ~pvrio_events & PVRIO_RENDER_DONE) {
                /* a render that does not end leaves the core unable to render again: reset the core (and only
                   the core: SOFTRESET bit 1), so that the pages after this one are drawn */
                printf("pvrio: the render did not end; resetting the core\n");
                pvrio_hw_reg(0x008, 2);
                pvrio_hw_reg(0x008, 0);
            }
            break;
        }
    if(pvrio_errors)
        printf("pvrio: the PVR reports errors %08x\n", (unsigned)pvrio_errors);
    old = irq_disable();
    pvrio_events &= ~events;
    pvrio_errors = 0;
    irq_restore(old);
}

static void pvrio_hw_dump32(uint32_t addr, int words, const char *name, const char *suffix) {
    static uint32_t copy[640 * 480], pairs[8192];
    static int complained;
    const volatile uint32_t *src = (const volatile uint32_t *)(0xa5000000 + addr);
    char path[200];
    FILE *f;
    int i, j, n, runs = 1, rle;

    if(words > 640 * 480)
        words = 640 * 480;
    for(i = 0; i < words; i++)
        copy[i] = src[i];
    for(i = 1; i < words; i++)
        runs += copy[i] != copy[i - 1];
    rle = runs * 2 < words;
    snprintf(path, sizeof(path), DUMP_DIR "/%s.hw%s%s", name, suffix, rle ? ".rle" : "");
    f = fopen(path, "wb");
    if(!f) {
        if(!complained++)
            printf("pvrio: cannot write %s (no host file system: an emulator?)\n", path);
        return;
    }
    if(!rle) {
        fwrite(copy, 4, words, f);
    } else {
        for(i = 0, n = 0; i < words; i = j) {
            for(j = i + 1; j < words && copy[j] == copy[i]; j++)
                ;
            pairs[n++] = (uint32_t)(j - i);
            pairs[n++] = copy[i];
            if(n == 8192) {
                fwrite(pairs, 4, n, f);
                n = 0;
            }
        }
        fwrite(pairs, 4, n, f);
    }
    fclose(f);
}

static void pvrio_hw_dump_ta_state(const char *name, const char *suffix) {
    uint32_t w[4];
    char path[200];
    FILE *f;
    int old;

    w[0] = *(volatile uint32_t *)(0xa05f8000 + TA_ISP_CURRENT_addr);
    w[1] = *(volatile uint32_t *)(0xa05f8000 + TA_NEXT_OPB_addr);
    old = irq_disable();
    w[2] = pvrio_seen_events & PVRIO_TA_EVENTS;
    w[3] = pvrio_seen_errors;
    pvrio_seen_events = pvrio_seen_errors = 0;
    irq_restore(old);
    snprintf(path, sizeof(path), DUMP_DIR "/%s.hw%s", name, suffix);
    f = fopen(path, "wb");
    if(!f)
        return;
    fwrite(w, 4, 4, f);
    fclose(f);
}

static uint32_t pvrio_ta_events(void) {
    return pvrio_seen_events & PVRIO_TA_EVENTS;
}

static void pvrio_hw_say(const char *text) {
    printf("%s\n", text);
}

static void pvrio_vblank(void) {
    vid_waitvbl();
}

static void pvrio_close(void) {
}
#endif

#endif
