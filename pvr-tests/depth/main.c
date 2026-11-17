/* Depth: the compare modes, depth writes, and how fine a depth difference is resolved.

   page 0  Each cell: A, a red quad over the whole cell at z = 0.5 (always passes, writes depth);
           B, green, the left 24 pixels, at z = 0.25 / 0.5 / 0.75 with the compare mode under test and
           depth write on or off; C, blue, the top 16 rows at z = 0.6 with GEQUAL (it is hidden wherever
           B wrote 0.75). 8 modes x 3 depths x write on/off = 48 cells per list:
           cells 0-47 opaque list, 60-107 punch-through list, 120-167 translucent list (in order).
   page 1  precision. A red at z, then B green at z * (1 + 2^-k) with GREATER (cells: k = 1..32).
           Green means the difference was resolved.
           cells 0-31: z = 1, 40-71: z = 1000, 80-111: z = 0.001 (translucent list, in order);
           cells 120-151, 160-191, 200-231: the same in the opaque list.

   The PVR is driven directly (../../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's driver. */
#define NUM_PAGES 2
#include "pvrtest2.h"

static const char *mode_name[8] = { "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always" };
static const char *b_depth_str[3] = { "0.25", "0.50", "0.75" };
static const f32b b_depth[3] = { 0x3e800000, 0x3f000000, 0x3f400000 }   /* 0.25, 0.5, 0.75 */;
static const char *z_base_str[3] = { "1", "1000", "0.001" };
static const f32b z_base[3] = { 0x3f800000, 0x447a0000, 0x3a83126f }   /* 1, 1000, 0.001f */;

static void test_setup(void) {
}

static void test_describe(void) {
    static const char *lists[3] = { "OP", "PT", "TR" };
    int l, c, k, b;

    for(l = 0; l < 3; l++)
        for(c = 0; c < 48; c++)
            label(0, l * 60 + c, "%s %s, B at %s, write %s", lists[l], mode_name[c & 7], b_depth_str[(c >> 3) % 3],
                  c >= 24 ? "off" : "on");
    for(l = 0; l < 2; l++)
        for(b = 0; b < 3; b++)
            for(k = 1; k <= 32; k++)
                label(1, l * 120 + b * 40 + k - 1, "%s z = %s, B at z * (1 + 2^-%d)", l ? "OP" : "TR", z_base_str[b], k);
}

/* A flat polygon: no culling, source colour only; the depth compare and write are the point. */
static void flat(int list, int cmp, int write) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 1;
    isp.DepthMode = cmp;
    isp.ZWriteDis = !write;
    isp.CullMode = CullMode_None;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
}

static void modes(int list, int first) {
    int c;

    for(c = 0; c < 48; c++) {
        int x = cell_px(first + c), y = cell_py(first + c);
        flat(list, DepthMode_Always, 1);
        quad0(x, y, 32, 32, F32B_HALF, 0xffff0000);
        flat(list, c & 7, c < 24);
        quad0(x, y, 24, 32, b_depth[(c >> 3) % 3], 0xff00ff00);
        flat(list, DepthMode_GreaterEqual, 0);
        quad0(x, y, 32, 16, F(0.6f), 0xff0000ff);
    }
}

static void precision(int list, int first) {
    int i, k;

    for(i = 0; i < 3; i++) {
        for(k = 1; k <= 32; k++) {
            int c = first + i * 40 + k - 1;
            f32b zb = sf_add(z_base[i], sf_scale(z_base[i], -k));      /* z * (1 + 2^-k): exact sum, rounded once */
            flat(list, DepthMode_Always, 1);
            quad0(cell_px(c), cell_py(c), 32, 32, z_base[i], 0xffff0000);
            flat(list, DepthMode_Greater, 0);
            quad0(cell_px(c) + 4, cell_py(c) + 4, 24, 24, zb, 0xff00ff00);
        }
    }
}

static void test_page(int page) {
    ta_list_begin(ListType_Opaque);
    if(page == 0) modes(ListType_Opaque, 0);
    else precision(ListType_Opaque, 120);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    if(page == 0) modes(ListType_Translucent, 120);
    else precision(ListType_Translucent, 0);
    ta_list_end();
    ta_list_begin(ListType_Punch_Through);
    if(page == 0) modes(ListType_Punch_Through, 60);
    ta_list_end();
}
