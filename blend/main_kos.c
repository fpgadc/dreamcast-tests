/* PowerVR2 blend unit test: every blend setting, to be compared between real hardware and an emulator.

   The screen is a grid of 32x32 cells, one tile each. Cell number c tests one setting:
       bits 0-2  source blend factor       (PVR_BLEND_ZERO .. PVR_BLEND_INVDESTALPHA)
       bits 3-5  destination blend factor
       bit 6     SrcSel: source = secondary accumulation buffer ("buffer 2")
       bit 7     DstSel: destination = buffer 2
   Each cell is four 16x16 squares, and in each square three flat-colored, untextured quads are drawn, in
   the translucent list in submission order:
       1. a color into the framebuffer            (ONE, ZERO)
       2. another color into buffer 2             (ONE, ZERO, DstSel)
       3. the test quad with the cell's setting
   The colors (with alpha) depend on the square and on the color set (SET, 0..NUM_SETS-1), not on the cell.
   Nothing is textured or interpolated, so the only thing under test is the blend unit.

   A full-screen pass then brings what is to be inspected into the framebuffer's color (VIEW):
       0  nothing: the framebuffer's color
       1  buffer 2's color          (source = buffer 2, ONE, ZERO)
       2  the framebuffer's alpha   (white, DESTALPHA, ZERO)
       3  buffer 2's alpha          (white, then source = buffer 2 with ZERO, SRCALPHA)

   Default build (hardware, under dc-load): goes through every set and view and writes the displayed
   framebuffer of each (640x480 words of 0RGB) to DUMP_DIR "/set<s>_view<v>.hw.raw" on the host, then exits.
   With -DSET=s -DVIEW=v: draws that one forever, for an emulator's screenshot. */
#include <kos.h>
#include <stdio.h>

#define COLS 20
#define NUM_CELLS 256
#define NUM_SETS 4
#define NUM_VIEWS 4
#ifndef DUMP_DIR
#define DUMP_DIR "/pc/tmp/pvr_tests/blend"
#endif

static pvr_poly_hdr_t hdr_fb, hdr_b2, hdr_test[NUM_CELLS], hdr_copy_b2, hdr_fb_alpha, hdr_white, hdr_b2_alpha;

/* The color of layer 0 (framebuffer), 1 (buffer 2) or 2 (test quad) in square q of a cell. A third of the
   bytes are edge values. The same function is in compare.py. */
static uint32_t color(int set, int q, int layer) {
    static const uint8_t edge[6] = { 0, 1, 127, 128, 254, 255 };
    uint32_t s = (uint32_t)(set * 12 + q * 3 + layer) * 2654435761u + 12345u, out = 0;
    int k;

    for(k = 0; k < 4; k++) {
        uint32_t b;
        s = s * 1664525u + 1013904223u;
        b = (s >> 16) & 0xff;
        if(b < 96)
            b = edge[b % 6];
        out = (out << 8) | b;
    }
    return out;        /* ARGB */
}

static void plain(pvr_poly_hdr_t *hdr, int src, int dst, int src_sel, int dst_sel) {
    pvr_poly_cxt_t cxt;

    pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.gen.alpha = PVR_ALPHA_ENABLE;
    cxt.gen.shading = PVR_SHADE_FLAT;
    cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    cxt.blend.src = src;
    cxt.blend.dst = dst;
    cxt.blend.src_enable = src_sel ? PVR_BLEND_ENABLE : PVR_BLEND_DISABLE;
    cxt.blend.dst_enable = dst_sel ? PVR_BLEND_ENABLE : PVR_BLEND_DISABLE;
    pvr_poly_compile(hdr, &cxt);
}

static void quad(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, uint32_t argb) {
    pvr_vertex_t v;
    int k;

    pvr_prim(hdr, sizeof(*hdr));
    v.argb = argb;
    v.oargb = 0;
    v.u = v.v = 0.0f;
    v.z = 1.0f;
    for(k = 0; k < 4; k++) {
        v.flags = (k == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x + (k & 1) * w;
        v.y = y + (k >> 1) * h;
        pvr_prim(&v, sizeof(v));
    }
}

static void draw_frame(int set, int view) {
    int c, q;

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    for(c = 0; c < NUM_CELLS; c++) {
        for(q = 0; q < 4; q++) {
            float x = (c % COLS) * 32.0f + (q & 1) * 16.0f, y = (c / COLS) * 32.0f + (q >> 1) * 16.0f;
            quad(&hdr_fb, x, y, 16.0f, 16.0f, color(set, q, 0));
            quad(&hdr_b2, x, y, 16.0f, 16.0f, color(set, q, 1));
            quad(&hdr_test[c], x, y, 16.0f, 16.0f, color(set, q, 2));
        }
    }
    if(view == 1)
        quad(&hdr_copy_b2, 0.0f, 0.0f, 640.0f, 480.0f, 0xffffffff);
    else if(view == 2)
        quad(&hdr_fb_alpha, 0.0f, 0.0f, 640.0f, 480.0f, 0xffffffff);
    else if(view == 3) {
        quad(&hdr_white, 0.0f, 0.0f, 640.0f, 480.0f, 0xffffffff);
        quad(&hdr_b2_alpha, 0.0f, 0.0f, 640.0f, 480.0f, 0xffffffff);
    }
    pvr_list_finish();
    pvr_scene_finish();
}

#ifndef SET
static void dump_frame(int set, int view) {
    static uint32_t copy[640 * 480];
    const volatile uint32_t *fb;
    char name[128];
    FILE *f;
    int i;

    for(i = 0; i < 4; i++)                              /* let the last submitted frame reach the screen */
        vid_waitvbl();
    fb = (const volatile uint32_t *)(0xa5000000 + (PVR_GET(PVR_FB_ADDR) & 0x007ffffc));
    for(i = 0; i < 640 * 480; i++)                      /* video memory is read 32 bits at a time */
        copy[i] = fb[i];
    sprintf(name, DUMP_DIR "/set%d_view%d.hw.raw", set, view);
    f = fopen(name, "wb");
    if(!f) {
        printf("dump: cannot open %s\n", name);
        return;
    }
    fwrite(copy, 1, sizeof(copy), f);
    fclose(f);
    printf("dump: wrote %s\n", name);
}
#endif

int main(int argc, char **argv) {
    static pvr_init_params_t params = {
        { PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 },
        1024 * 1024, 0, 0,
        1,              /* translucent autosort off: drawn in submission order */
        3, 0
    };
    int c, frame;

    (void)argc;
    (void)argv;
    (void)frame;
    vid_set_mode(DM_640x480, PM_RGB0888);
    pvr_init(&params);
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);

    plain(&hdr_fb, PVR_BLEND_ONE, PVR_BLEND_ZERO, 0, 0);
    plain(&hdr_b2, PVR_BLEND_ONE, PVR_BLEND_ZERO, 0, 1);
    for(c = 0; c < NUM_CELLS; c++)
        plain(&hdr_test[c], c & 7, (c >> 3) & 7, (c >> 6) & 1, (c >> 7) & 1);
    plain(&hdr_copy_b2, PVR_BLEND_ONE, PVR_BLEND_ZERO, 1, 0);
    plain(&hdr_fb_alpha, PVR_BLEND_DESTALPHA, PVR_BLEND_ZERO, 0, 0);
    plain(&hdr_white, PVR_BLEND_ONE, PVR_BLEND_ZERO, 0, 0);
    plain(&hdr_b2_alpha, PVR_BLEND_ZERO, PVR_BLEND_SRCALPHA, 1, 0);

#ifdef SET
    for(;;)
        draw_frame(SET, VIEW);
#else
    {
        int set, view;
        for(set = 0; set < NUM_SETS; set++) {
            for(view = 0; view < NUM_VIEWS; view++) {
                for(frame = 0; frame < 6; frame++)
                    draw_frame(set, view);
                dump_frame(set, view);
            }
        }
    }
#endif
    pvr_shutdown();
    return 0;
}
