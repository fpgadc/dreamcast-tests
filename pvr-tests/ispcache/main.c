/* The translucent sort cache and "ISP out of cache" (SB_ISTERR bit 0), TODO A4 (2026-11-19). 25 of the 64 older
   polly2 dumps and shenmue_intro report the error on the same tiles in every render, with nothing visible; what fills
   the cache, what the core does when it is full, and what it costs are not known. Every page fills tiles 0-19 (the
   first row) with L translucent layers, sorted per pixel by the hardware (autosort), each half transparent
   (alpha 0x40) in a colour of its own, the layers' depths in a shuffled order: a layer dropped or drawn out of order
   changes the colour. The rest of the screen is background. pvrio prints "the PVR reports errors <bits>" in the
   console's log after a render that set any (bit 0: out of cache), so the log says which pages overflowed.
     pages 0 .. 8    full-tile sprites, L = 1, 2, 4, 8, 16, 32, 64, 128, 256 (L fragments on every pixel)
     pages 9 .. 17   L triangles of 8x8 pixels at shuffled places in the tile (fewer fragments a pixel, the same count
                     of primitives)
   Run it again with the cache size changed (ISP_FEED_CFG bits 23..14; 0x00800408 has 512):
     run2.sh ispcache+098=<(size << 14) | 0x408 in hex>, sizes 8 .. 512; and with discard off (bit 3).
   refsw3 has no cache limit: offline is the picture with every layer sorted. */
#define NUM_PAGES 18
#define TEST_AUTOSORT
#include "pvrtest2.h"

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; v *= 3266489917u; v ^= v >> 16; return v; }

static void test_setup(void) { }

static void test_describe(void)
{
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 20; c++)
            label(p, c, "%d %s", 1 << (p % 9), p < 9 ? "full-tile layers" : "8x8 triangles");
}

static void header(int sprite)
{
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = sprite ? ParamType_Sprite : ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = ListType_Translucent;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_SrcAlpha;
    tsp.DstInstr = BlendInstr_InvSrcAlpha;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    if(!sprite) {
        ta_polygon(pcw, isp, tsp, tcw);
        return;
    }
    (void)tcw;
}

static void sprite(int x, int y, int w, int h, float z, uint32_t col)
{
    TA_SpriteParam sp;
    TA_Sprite0 v;
    memset(&sp, 0, sizeof(sp));
    sp.pcw.ParaType = ParamType_Sprite;
    sp.pcw.ListType = ListType_Translucent;
    sp.pcw.Group_En = 1;
    sp.isp.DepthMode = DepthMode_GreaterEqual;
    sp.isp.CullMode = CullMode_None;
    sp.tsp.SrcInstr = BlendInstr_SrcAlpha;
    sp.tsp.DstInstr = BlendInstr_InvSrcAlpha;
    sp.tsp.FogCtrl = FogCtrl_None;
    sp.tsp.UseAlpha = 1;
    sp.BaseCol = col;
    ta_send(&sp, sizeof(sp));
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(1);
    v.x0 = f32b_int(x);     v.y0 = f32b_int(y);     v.z0 = F(z);
    v.x1 = f32b_int(x + w); v.y1 = f32b_int(y);     v.z1 = F(z);
    v.x2 = f32b_int(x + w); v.y2 = f32b_int(y + h); v.z2 = F(z);
    v.x3 = f32b_int(x);     v.y3 = f32b_int(y + h);
    ta_send(&v, sizeof(v));
}

static void test_page(int page)
{
    const int L = 1 << (page % 9), tris = page >= 9;
    static int order[256];
    int c, i, j;
    pvr_frame.bg_color = 0x000000;
    ta_list_begin(ListType_Translucent);
    for(c = 0; c < 20; c++) {
        const int x = cell_px(c), y = cell_py(c);
        for(i = 0; i < L; i++) order[i] = i;
        for(i = L - 1; i > 0; i--) {            /* the depths in a shuffled order */
            j = (int)(hsh((uint32_t)(page * 7919 + c * 131 + i)) % (uint32_t)(i + 1));
            { const int t = order[i]; order[i] = order[j]; order[j] = t; }
        }
        for(i = 0; i < L; i++) {
            const float z = 0.25f + 0.5f * (float)order[i] / (float)L;          /* distinct depths, 1/w 0.25 .. 0.75 */
            const uint32_t h = hsh((uint32_t)(page * 65537 + c * 4099 + i));
            const uint32_t col = 0x40000000u | (h & 0x00ffffffu);
            if(!tris)
                sprite(x, y, 32, 32, z, col);
            else {
                const int bx = x + (int)(h >> 24) % 25, by = y + (int)(h >> 16 & 0xff) % 25;
                header(0);
                ta_vertex0(f32b_int(bx), f32b_int(by), F(z), col, 0);
                ta_vertex0(f32b_int(bx + 8), f32b_int(by), F(z), col, 0);
                ta_vertex0(f32b_int(bx), f32b_int(by + 8), F(z), col, 1);
            }
        }
    }
    ta_list_end();
}
