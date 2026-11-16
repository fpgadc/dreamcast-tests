/* Sprites (quads given by three corners and a fourth x, y; texture coordinates packed to 16 bit floats).

   page 0  untextured white sprites
     cells 0-15    a 16x16 sprite at (8 + i/16, 8 + i/16), i = 0..15
     cells 20-27   parallelograms: the fourth corner is implied by the first three; d is given as a + c - b
     cells 28-31   the fourth corner given somewhere else (not a parallelogram)
     cells 40-47   the corners in the other winding, and starting from each of the four corners
     cells 60-67   depth: a sprite whose z slopes (0.2 at a, 0.8 at b and c) against a gray quad at 0.5
   page 1  textured sprites: an 8x8, 8 bpp texture with an ARGB8888 palette (red = column, green = row),
           point sampled
     cells 0-7     32x32 sprite, whole texture; flipped in u; flipped in v; rotated 90 degrees (uv swapped);
                   half the texture; coordinates 0..2 (repeat); 16x16 sprite; 64x8 (2 cells wide)
     cells 20-35   16x16 sprite at 2x, shifted by i/16 pixel

   The PVR is driven directly (../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's driver.
   The sprite parameter words are what its pvr_sprite_compile made (checked with common/kos_words.sh). */
#define NUM_PAGES 2
#include "pvrtest2.h"

static uint32_t pattern;       /* 64 bit area address of the texture */

static void test_setup(void) {
    uint8_t t[64];
    int i;

    pvr_pal_format(PAL_RAM_CTRL_ARGB8888);
    for(i = 0; i < 64; i++) {
        pvr_pal_entry(i, 0xff000000 | ((i % 8) * 36 << 16) | ((i / 8) * 36 << 8) | ((((i % 8) ^ (i / 8)) & 1) ? 255 : 0));
        t[i] = i;
    }
    pattern = pvr_tex_alloc(64);
    pvr_tex_load_twiddled8(pattern, t, 8, 8);
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

#define UV0 0x00000000u    /* 0 */
#define UVH 0x3f000000u    /* 0.5 */
#define UV1 0x3f800000u    /* 1 */
#define UV2 0x40000000u    /* 2 */

/* corners a, b, c, d in order around the quad; untextured, translucent list, depth compare GEQUAL */
static void sprite_col(const f32b p[4][2], const f32b z[3], uint32_t argb) {
    TA_SpriteParam s;
    TA_Sprite0 v;

    memset(&s, 0, sizeof(s));
    s.pcw.ParaType = ParamType_Sprite;
    s.pcw.ListType = ListType_Translucent;
    s.pcw.UV_16bit = 1;
    s.isp.DepthMode = DepthMode_GreaterEqual;
    s.isp.CullMode = CullMode_None;
    s.tsp.SrcInstr = BlendInstr_SrcAlpha;
    s.tsp.DstInstr = BlendInstr_InvSrcAlpha;
    s.tsp.FogCtrl = FogCtrl_None;
    s.tsp.UseAlpha = 1;
    s.BaseCol = argb;
    ta_send(&s, sizeof(s));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = p[0][0]; v.y0 = p[0][1]; v.z0 = z[0];
    v.x1 = p[1][0]; v.y1 = p[1][1]; v.z1 = z[1];
    v.x2 = p[2][0]; v.y2 = p[2][1]; v.z2 = z[2];
    v.x3 = p[3][0]; v.y3 = p[3][1];
    ta_send(&v, sizeof(v));
}

/* a textured sprite at (x, y), w x h, z = 1, with the 8 bpp texture, point sampled, depth compare ALWAYS */
static void sprite_txr(f32b x, f32b y, int w, int h, const f32b uv[3][2]) {
    TA_SpriteParam s;
    TA_Sprite1 v;
    f32b xw = sf_add(x, f32b_int(w)), yh = sf_add(y, f32b_int(h));

    memset(&s, 0, sizeof(s));
    s.pcw.ParaType = ParamType_Sprite;
    s.pcw.ListType = ListType_Translucent;
    s.pcw.Texture = 1;
    s.pcw.UV_16bit = 1;
    s.isp.DepthMode = DepthMode_Always;
    s.isp.CullMode = CullMode_None;
    s.isp.Texture = 1;
    s.tsp.SrcInstr = BlendInstr_SrcAlpha;
    s.tsp.DstInstr = BlendInstr_InvSrcAlpha;
    s.tsp.FogCtrl = FogCtrl_None;
    s.tsp.UseAlpha = 1;
    s.tsp.ShadInstr = ShadInstr_ModulateAlpha;
    s.tsp.FilterMode = FilterMode_Point;
    s.tsp.MipMapD = 4;
    s.tsp.TexU = 0;                             /* 8 texels */
    s.tsp.TexV = 0;
    s.tcw.PixelFmt = PixelFmt_Pal8;
    s.tcw.PalSelect = 0;
    s.tcw.TexAddr = pattern >> 3;
    s.BaseCol = 0xffffffff;
    ta_send(&s, sizeof(s));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = x;  v.y0 = y;  v.z0 = F32B_ONE;
    v.x1 = xw; v.y1 = y;  v.z1 = F32B_ONE;
    v.x2 = xw; v.y2 = yh; v.z2 = F32B_ONE;
    v.x3 = x;  v.y3 = yh;
    v.uv0 = uv16(uv[0][0], uv[0][1]);
    v.uv1 = uv16(uv[1][0], uv[1][1]);
    v.uv2 = uv16(uv[2][0], uv[2][1]);
    ta_send(&v, sizeof(v));
}

static void test_page(int page) {
    static const f32b flat[3] = { F32B_ONE, F32B_ONE, F32B_ONE };
    int c, k;

    if(page == 0) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };

        ta_list_begin(ListType_Opaque);
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Gouraud = 1;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        ta_polygon(pcw, isp, tsp, tcw);
        quad0(0, 96, 256, 32, F32B_HALF, 0xff606060);
        ta_list_end();
    }
    ta_list_begin(ListType_Translucent);
    if(page == 0) {
        for(c = 0; c < 16; c++) {
            f32b x = sf_add(sf_add(f32b_int(cell_px(c)), F(8.0f)), f32b_scaled(c, -4));
            f32b y = sf_add(sf_add(f32b_int(cell_py(c)), F(8.0f)), f32b_scaled(c, -4));
            f32b x16 = sf_add(x, F(16.0f)), y16 = sf_add(y, F(16.0f));
            f32b p[4][2] = { { x, y }, { x16, y }, { x16, y16 }, { x, y16 } };
            sprite_col(p, flat, 0xffffffff);
        }
        for(c = 0; c < 8; c++) {
            /* the corners in quarter pixels */
            static const int shape[4][3][2] = { { { 16, 16 }, { 112, 32 }, { 96, 112 } }, { { 64, 8 }, { 120, 64 }, { 64, 120 } },
                                                { { 8, 40 }, { 80, 16 }, { 120, 88 } }, { { 34, 13 }, { 111, 38 }, { 85, 118 } } };
            static const int fourth[4][2] = { { 16, 112 }, { 64, 64 }, { 8, 120 }, { 120, 8 } };
            f32b x = f32b_int(cell_px(20 + c)), y = f32b_int(cell_py(20 + c)), p[4][2];
            for(k = 0; k < 3; k++) {
                p[k][0] = sf_add(x, f32b_scaled(shape[c % 4][k][0], -2));
                p[k][1] = sf_add(y, f32b_scaled(shape[c % 4][k][1], -2));
            }
            if(c < 4) {
                p[3][0] = sf_sub(sf_add(p[0][0], p[2][0]), p[1][0]);
                p[3][1] = sf_sub(sf_add(p[0][1], p[2][1]), p[1][1]);
            }
            else {
                p[3][0] = sf_add(x, f32b_scaled(fourth[c - 4][0], -2));
                p[3][1] = sf_add(y, f32b_scaled(fourth[c - 4][1], -2));
            }
            sprite_col(p, flat, 0xffffffff);
        }
        for(c = 0; c < 8; c++) {
            static const int corner[4][2] = { { 6, 6 }, { 26, 6 }, { 26, 22 }, { 6, 22 } };
            f32b p[4][2];
            for(k = 0; k < 4; k++) {
                int i = (c < 4) ? (c + k) % 4 : (c + 4 - k) % 4;
                p[k][0] = f32b_int(cell_px(40 + c) + corner[i][0]);
                p[k][1] = f32b_int(cell_py(40 + c) + corner[i][1]);
            }
            sprite_col(p, flat, 0xffffffff);
        }
        for(c = 0; c < 8; c++) {
            static const f32b zs[8][3] = { { 0x3e4ccccd, 0x3f4ccccd, 0x3f4ccccd }, { 0x3f4ccccd, 0x3e4ccccd, 0x3e4ccccd },
                                           { 0x3e4ccccd, 0x3e4ccccd, 0x3f4ccccd }, { 0x3f4ccccd, 0x3f4ccccd, 0x3e4ccccd },
                                           { 0x3ecccccd, 0x3f19999a, 0x3f4ccccd }, { 0x3f4ccccd, 0x3f19999a, 0x3ecccccd },
                                           { 0x3efae148, 0x3f028f5c, 0x3f028f5c }, { 0x3f19999a, 0x3f19999a, 0x3f19999a } };
            f32b x = f32b_int(cell_px(60 + c)), y = f32b_int(cell_py(60 + c));
            f32b x32 = sf_add(x, F(32.0f)), y32 = sf_add(y, F(32.0f));
            f32b p[4][2] = { { x, y }, { x32, y }, { x32, y32 }, { x, y32 } };
            sprite_col(p, zs[c], 0xffffffff);
        }
    }
    else {
        static const f32b uvs[8][3][2] = { { { UV0, UV0 }, { UV1, UV0 }, { UV1, UV1 } }, { { UV1, UV0 }, { UV0, UV0 }, { UV0, UV1 } },
                                           { { UV0, UV1 }, { UV1, UV1 }, { UV1, UV0 } }, { { UV0, UV0 }, { UV0, UV1 }, { UV1, UV1 } },
                                           { { UV0, UV0 }, { UVH, UV0 }, { UVH, UVH } }, { { UV0, UV0 }, { UV2, UV0 }, { UV2, UV2 } },
                                           { { UV0, UV0 }, { UV1, UV0 }, { UV1, UV1 } }, { { UV0, UV0 }, { UV1, UV0 }, { UV1, UV1 } } };
        for(c = 0; c < 8; c++) {
            int w = (c == 6) ? 16 : (c == 7) ? 64 : 32, h = (c == 6) ? 16 : (c == 7) ? 8 : 32;
            sprite_txr(f32b_int(cell_px(c == 7 ? 8 : c)), f32b_int(cell_py(c)), w, h, uvs[c]);
        }
        for(c = 0; c < 16; c++)
            sprite_txr(sf_add(sf_add(f32b_int(cell_px(20 + c)), F(8.0f)), f32b_scaled(c, -4)),
                       sf_add(sf_add(f32b_int(cell_py(20 + c)), F(8.0f)), f32b_scaled(c, -4)), 16, 16, uvs[0]);
    }
    ta_list_end();
}
