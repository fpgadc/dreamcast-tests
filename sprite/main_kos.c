/* Sprites (quads given by three corners and a fourth x, y; texture coordinates packed to 16 bit floats).

   page 0  untextured white sprites
     cells 0-15    a 16x16 sprite at (8 + i/16, 8 + i/16), i = 0..15
     cells 20-27   parallelograms: the fourth corner is implied by the first three; d is given as a + c - b
     cells 28-31   the fourth corner given somewhere else (not a parallelogram)
     cells 40-47   the corners in the other winding, and starting from each of the four corners
     cells 60-67   depth: a sprite whose z slopes (0.2 at a, 0.8 at b and c) against a gray quad at 0.5
   page 1  textured sprites: an 8x8, 8 bpp texture with an ARGB8888 palette (red = column, green = row),
           point sampled with POINT_FIX
     cells 0-7     32x32 sprite, whole texture; flipped in u; flipped in v; rotated 90 degrees (uv swapped);
                   half the texture; coordinates 0..2 (repeat); 16x16 sprite; 64x8 (2 cells wide)
     cells 20-35   16x16 sprite at 2x, shifted by i/16 pixel */
#define NUM_PAGES 2
#include "pvrtest.h"

static pvr_ptr_t pattern;
#define PAL8 (PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0))

static void test_setup(void) {
    uint8_t t[64];
    int i;

    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for(i = 0; i < 64; i++) {
        pvr_set_pal_entry(i, 0xff000000 | ((i % 8) * 36 << 16) | ((i / 8) * 36 << 8) | ((((i % 8) ^ (i / 8)) & 1) ? 255 : 0));
        t[i] = i;
    }
    pattern = pvr_mem_malloc(64);
    pvr_txr_load_ex(t, pattern, 8, 8, PVR_TXRLOAD_8BPP);
}

static void test_describe(void) {
    static const char *tex[8] = { "whole texture", "flipped u", "flipped v", "uv swapped", "half the texture", "coordinates 0..2",
                                  "16x16", "64x8" };
    int c;

    for(c = 0; c < 16; c++) {
        label(0, c, "sprite 16x16 at +%d/16", c);
        label(1, 20 + c, "textured sprite 16x16, shifted %d/16", c);
    }
    for(c = 0; c < 8; c++) {
        label(0, 20 + c, c < 4 ? "parallelogram %d" : "fourth corner elsewhere %d", c);
        label(0, 40 + c, "corner order %d", c);
        label(0, 60 + c, "sloped depth %d", c);
        label_span(1, c == 7 ? 8 : c, c == 7 ? 2 : 1, 1, "textured sprite: %s", tex[c]);
    }
}

/* corners a, b, c, d in order around the quad */
static void sprite_col(pvr_list_t list, const float p[4][2], const float z[3], uint32_t argb) {
    pvr_sprite_cxt_t cxt;
    pvr_sprite_hdr_t hdr;
    pvr_sprite_col_t v;

    pvr_sprite_cxt_col(&cxt, list);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    pvr_sprite_compile(&hdr, &cxt);
    hdr.argb = argb;
    pvr_prim(&hdr, sizeof(hdr));
    memset(&v, 0, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.ax = p[0][0]; v.ay = p[0][1]; v.az = z[0];
    v.bx = p[1][0]; v.by = p[1][1]; v.bz = z[1];
    v.cx = p[2][0]; v.cy = p[2][1]; v.cz = z[2];
    v.dx = p[3][0]; v.dy = p[3][1];
    pvr_prim(&v, sizeof(v));
}

static void sprite_txr(float x, float y, float w, float h, const float uv[3][2]) {
    pvr_sprite_cxt_t cxt;
    pvr_sprite_hdr_t hdr;
    pvr_sprite_txr_t v;
    float s = POINT_FIX / 8.0f;

    pvr_sprite_cxt_txr(&cxt, PVR_LIST_TR_POLY, PAL8, 8, 8, pattern, PVR_FILTER_NONE);
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    pvr_sprite_compile(&hdr, &cxt);
    hdr.argb = 0xffffffff;
    pvr_prim(&hdr, sizeof(hdr));
    memset(&v, 0, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.ax = x;     v.ay = y;     v.az = 1.0f;
    v.bx = x + w; v.by = y;     v.bz = 1.0f;
    v.cx = x + w; v.cy = y + h; v.cz = 1.0f;
    v.dx = x;     v.dy = y + h;
    v.auv = PVR_PACK_16BIT_UV(uv[0][0] + s, uv[0][1] + s);
    v.buv = PVR_PACK_16BIT_UV(uv[1][0] + s, uv[1][1] + s);
    v.cuv = PVR_PACK_16BIT_UV(uv[2][0] + s, uv[2][1] + s);
    pvr_prim(&v, sizeof(v));
}

static void test_page(int page) {
    static const float flat[3] = { 1.0f, 1.0f, 1.0f };
    int c, k;

    if(page == 0) {
        pvr_poly_cxt_t cxt;
        pvr_poly_hdr_t hdr;
        pvr_list_begin(PVR_LIST_OP_POLY);
        cxt_plain(&cxt, PVR_LIST_OP_POLY);
        cxt.depth.write = PVR_DEPTHWRITE_ENABLE;
        pvr_poly_compile(&hdr, &cxt);
        quad(&hdr, 0.0f, 96.0f, 256.0f, 32.0f, 0.5f, 0xff606060);
        pvr_list_finish();
    }
    pvr_list_begin(PVR_LIST_TR_POLY);
    if(page == 0) {
        for(c = 0; c < 16; c++) {
            float x = cell_x(c) + 8.0f + c / 16.0f, y = cell_y(c) + 8.0f + c / 16.0f;
            float p[4][2] = { { x, y }, { x + 16.0f, y }, { x + 16.0f, y + 16.0f }, { x, y + 16.0f } };
            sprite_col(PVR_LIST_TR_POLY, p, flat, 0xffffffff);
        }
        for(c = 0; c < 8; c++) {
            static const float shape[4][3][2] = { { { 4, 4 }, { 28, 8 }, { 24, 28 } }, { { 16, 2 }, { 30, 16 }, { 16, 30 } },
                                                  { { 2, 10 }, { 20, 4 }, { 30, 22 } }, { { 8.5f, 3.25f }, { 27.75f, 9.5f }, { 21.25f, 29.5f } } };
            static const float fourth[4][2] = { { 4, 28 }, { 16, 16 }, { 2, 30 }, { 30, 2 } };
            float x = cell_x(20 + c), y = cell_y(20 + c), p[4][2];
            for(k = 0; k < 3; k++) {
                p[k][0] = x + shape[c % 4][k][0];
                p[k][1] = y + shape[c % 4][k][1];
            }
            if(c < 4) {
                p[3][0] = p[0][0] + p[2][0] - p[1][0];
                p[3][1] = p[0][1] + p[2][1] - p[1][1];
            }
            else {
                p[3][0] = x + fourth[c - 4][0];
                p[3][1] = y + fourth[c - 4][1];
            }
            sprite_col(PVR_LIST_TR_POLY, p, flat, 0xffffffff);
        }
        for(c = 0; c < 8; c++) {
            static const float corner[4][2] = { { 6, 6 }, { 26, 6 }, { 26, 22 }, { 6, 22 } };
            float p[4][2];
            for(k = 0; k < 4; k++) {
                int i = (c < 4) ? (c + k) % 4 : (c + 4 - k) % 4;
                p[k][0] = cell_x(40 + c) + corner[i][0];
                p[k][1] = cell_y(40 + c) + corner[i][1];
            }
            sprite_col(PVR_LIST_TR_POLY, p, flat, 0xffffffff);
        }
        for(c = 0; c < 8; c++) {
            static const float zs[8][3] = { { 0.2f, 0.8f, 0.8f }, { 0.8f, 0.2f, 0.2f }, { 0.2f, 0.2f, 0.8f }, { 0.8f, 0.8f, 0.2f },
                                            { 0.4f, 0.6f, 0.8f }, { 0.8f, 0.6f, 0.4f }, { 0.49f, 0.51f, 0.51f }, { 0.6f, 0.6f, 0.6f } };
            float x = cell_x(60 + c), y = cell_y(60 + c);
            float p[4][2] = { { x, y }, { x + 32.0f, y }, { x + 32.0f, y + 32.0f }, { x, y + 32.0f } };
            sprite_col(PVR_LIST_TR_POLY, p, zs[c], 0xffffffff);
        }
    }
    else {
        static const float uvs[8][3][2] = { { { 0, 0 }, { 1, 0 }, { 1, 1 } }, { { 1, 0 }, { 0, 0 }, { 0, 1 } }, { { 0, 1 }, { 1, 1 }, { 1, 0 } },
                                            { { 0, 0 }, { 0, 1 }, { 1, 1 } }, { { 0, 0 }, { 0.5f, 0 }, { 0.5f, 0.5f } }, { { 0, 0 }, { 2, 0 }, { 2, 2 } },
                                            { { 0, 0 }, { 1, 0 }, { 1, 1 } }, { { 0, 0 }, { 1, 0 }, { 1, 1 } } };
        for(c = 0; c < 8; c++) {
            float w = (c == 6) ? 16.0f : (c == 7) ? 64.0f : 32.0f, h = (c == 6) ? 16.0f : (c == 7) ? 8.0f : 32.0f;
            sprite_txr(cell_x(c == 7 ? 8 : c), cell_y(c), w, h, uvs[c]);
        }
        for(c = 0; c < 16; c++)
            sprite_txr(cell_x(20 + c) + 8.0f + c / 16.0f, cell_y(20 + c) + 8.0f + c / 16.0f, 16.0f, 16.0f, uvs[0]);
    }
    pvr_list_finish();
}
