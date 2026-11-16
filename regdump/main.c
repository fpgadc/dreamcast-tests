/* Print what the PVR registers and the render structures in video memory hold while a test of this folder
   runs under KallistiOS: the reference for the explicit initialization in ../common/pvrx.h, which has to
   leave the chip in the same state without KallistiOS's driver (so that the same test can also feed the
   offline renderer).

       bash regdump/run.sh <dreamcast ip>        ->  /tmp/pvr_tests/regdump/hw.log

   -DTEST_MODIFIERS / -DTEST_AUTOSORT as in pvrtest.h. */
#include <kos.h>
#include <stdio.h>

static void frame(void) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t v;
    int k;

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
    v.z = 1.0f; v.u = v.v = 0.0f; v.argb = 0xff204060; v.oargb = 0;
    for(k = 0; k < 3; k++) {
        v.flags = (k == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = 100.0f + (k == 1) * 100.0f;
        v.y = 100.0f + (k == 2) * 100.0f;
        pvr_prim(&v, sizeof(v));
    }
    pvr_list_finish();
    pvr_scene_finish();
}

int main(int argc, char **argv) {
    static pvr_init_params_t params = {
#ifdef TEST_MODIFIERS
        { PVR_BINSIZE_32, PVR_BINSIZE_16, PVR_BINSIZE_32, PVR_BINSIZE_16, PVR_BINSIZE_32 },
#else
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
#endif
        1024 * 1024, 0, 0,
#ifdef TEST_AUTOSORT
        0,
#else
        1,
#endif
        3, 0
    };
    const volatile uint32_t *reg = (const volatile uint32_t *)0xa05f8000;
    const volatile uint32_t *ram = (const volatile uint32_t *)0xa5000000;
    uint32_t before[128], region, param, tag;
    int i;

    (void)argc; (void)argv;
    for(i = 0; i < 128; i++)
        before[i] = reg[i];
    vid_set_mode(DM_640x480, PM_RGB0888);
    printf("CABLE %d\n", vid_check_cable());
    for(i = 0; i < 128; i++)
        printf("BOOT %03x %08x\n", i * 4, (unsigned)before[i]);
    for(i = 0; i < 128; i++)
        printf("VID %03x %08x\n", i * 4, (unsigned)reg[i]);
    pvr_init(&params);
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    for(i = 0; i < 6; i++)
        frame();
    for(i = 0; i < 4; i++)
        vid_waitvbl();
    for(i = 0; i < 128; i++)
        printf("REG %03x %08x\n", i * 4, (unsigned)reg[i]);
    for(i = 0; i < 128; i++)
        printf("FOG %03x %08x\n", 0x200 + i * 4, (unsigned)reg[0x80 + i]);
    for(i = 0; i < 1024; i += 64)
        printf("PAL %04x %08x\n", 0x1000 + i * 4, (unsigned)reg[0x400 + i]);
    region = reg[0x2c / 4] & 0x00fffffc;
    param = reg[0x20 / 4] & 0x00f00000;
    tag = reg[0x8c / 4];
    printf("REGION_BASE %08x PARAM_BASE %08x BGTAG %08x\n", (unsigned)region, (unsigned)param, (unsigned)tag);
    for(i = -18; i < 6 * 5; i++)
        printf("RGN %d %08x\n", i, (unsigned)ram[region / 4 + i]);
    for(i = 6 * 299; i < 6 * 302; i++)
        printf("RGN %d %08x\n", i, (unsigned)ram[region / 4 + i]);
    for(i = 0; i < 16; i++)
        printf("BGP %d %08x\n", i, (unsigned)ram[param / 4 + ((tag >> 3) & 0x1fffff) + i]);
    for(i = 0; i < 24; i++)
        printf("PRM %d %08x\n", i, (unsigned)ram[param / 4 + i]);
    printf("ISTNRM %08x IML2 %08x IML4 %08x IML6 %08x\n", (unsigned)*(volatile uint32_t *)0xa05f6900,
           (unsigned)*(volatile uint32_t *)0xa05f6910, (unsigned)*(volatile uint32_t *)0xa05f6920, (unsigned)*(volatile uint32_t *)0xa05f6930);
    printf("REGDUMP DONE\n");
    pvr_shutdown();
    return 0;
}
