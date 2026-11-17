/* Fog.

   256 flat squares (four per cell), color 0x204080. For the table modes square n is at
   1/z = w = 2^(n/16 - 4), i.e. 1/16 .. about 60000; the fog color is (1.0, 0.9, 0.5, 0.1).
   cells 0-63 are in the translucent list, cells 80-143 the same in the opaque list.

   page 0  table fog, linear table from 1 to 200, far depth as KOS sets it
   page 1  table fog, custom table (a sawtooth, so the interpolation between entries shows), far depth 100
   page 2  vertex fog: the fog amount is the offset color's alpha, n = 0..255; fog color 0x33cc66
   page 3  table fog mode 2 with the table of page 1
   page 4  no fog (reference)

   The PVR is driven directly (../../common/pvrtest2.h); main_kos.c is the earlier version on KallistiOS's
   driver. The fog registers are the values KallistiOS's pvr_fog_table_color / pvr_fog_table_linear /
   pvr_fog_far_depth / pvr_fog_table_custom wrote (pvr_fog.c compiled on a PC with PVR_SET printing, for
   the linear table and for the sawtooth ((i * 37) % 128) / 127 with far depth 100). */
#define NUM_PAGES 5
#include "pvrtest2.h"

#define FOG_COL_RAM_VALUE   0xffe57f19u         /* pvr_fog_table_color(1.0, 0.9, 0.5, 0.1) */
#define FOG_COL_VERT_VALUE  0x0033cc66u
#define FOG_DENSITY_LINEAR  0x0000c807u         /* pvr_fog_table_linear(1.0, 200.0): far depth 200 */
#define FOG_DENSITY_SAW     0x0000c806u         /* pvr_fog_far_depth(100.0) */

static const uint32_t fog_table_linear[128] = {
    0x0000fff0, 0x0000f0e2, 0x0000e2d6, 0x0000d6cc, 0x0000ccc2, 0x0000c2b9, 0x0000b9b1, 0x0000b1aa,
    0x0000aaa3, 0x0000a39c, 0x00009c97, 0x00009791, 0x0000918c, 0x00008c87, 0x00008783, 0x0000837f,
    0x00007f78, 0x00007871, 0x0000716b, 0x00006b66, 0x00006661, 0x0000615c, 0x00005c58, 0x00005854,
    0x00005451, 0x0000514e, 0x00004e4b, 0x00004b48, 0x00004846, 0x00004644, 0x00004441, 0x0000413f,
    0x00003f3b, 0x00003b38, 0x00003835, 0x00003533, 0x00003330, 0x0000302e, 0x00002e2c, 0x00002c2a,
    0x00002a28, 0x00002827, 0x00002725, 0x00002524, 0x00002423, 0x00002321, 0x00002120, 0x0000201f,
    0x00001f1e, 0x00001e1c, 0x00001c1a, 0x00001a19, 0x00001918, 0x00001817, 0x00001716, 0x00001615,
    0x00001514, 0x00001413, 0x00001312, 0x00001212, 0x00001211, 0x00001111, 0x00001110, 0x0000100f,
    0x00000f0e, 0x00000e0e, 0x00000e0d, 0x00000d0c, 0x00000c0c, 0x00000c0b, 0x00000b0b, 0x00000b0a,
    0x00000a0a, 0x00000a09, 0x00000909, 0x00000909, 0x00000908, 0x00000808, 0x00000808, 0x00000807,
    0x00000707, 0x00000707, 0x00000706, 0x00000606, 0x00000606, 0x00000605, 0x00000505, 0x00000505,
    0x00000505, 0x00000504, 0x00000404, 0x00000404, 0x00000404, 0x00000404, 0x00000404, 0x00000403,
    0x00000303, 0x00000303, 0x00000303, 0x00000303, 0x00000303, 0x00000302, 0x00000202, 0x00000202,
    0x00000202, 0x00000202, 0x00000202, 0x00000202, 0x00000202, 0x00000202, 0x00000202, 0x00000201,
    0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101,
    0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000101, 0x00000100,
};

static const uint32_t fog_table_saw[128] = {
    0x0000004a, 0x00004a94, 0x000094de, 0x0000de28, 0x00002872, 0x000072bc, 0x0000bc06, 0x00000650,
    0x0000509a, 0x00009ae4, 0x0000e42e, 0x00002e78, 0x000078c2, 0x0000c20c, 0x00000c56, 0x000056a0,
    0x0000a0ea, 0x0000ea34, 0x0000347e, 0x00007ec8, 0x0000c812, 0x0000125c, 0x00005ca6, 0x0000a6f0,
    0x0000f03a, 0x00003a84, 0x000084ce, 0x0000ce18, 0x00001862, 0x000062ac, 0x0000acf6, 0x0000f640,
    0x0000408a, 0x00008ad4, 0x0000d41e, 0x00001e68, 0x000068b2, 0x0000b2fc, 0x0000fc46, 0x00004690,
    0x000090da, 0x0000da24, 0x0000246e, 0x00006eb8, 0x0000b802, 0x0000024c, 0x00004c96, 0x000096e0,
    0x0000e02a, 0x00002a74, 0x000074be, 0x0000be08, 0x00000852, 0x0000529c, 0x00009ce6, 0x0000e630,
    0x0000307a, 0x00007ac4, 0x0000c40e, 0x00000e58, 0x000058a2, 0x0000a2ec, 0x0000ec36, 0x00003680,
    0x000080ca, 0x0000ca14, 0x0000145e, 0x00005ea8, 0x0000a8f2, 0x0000f23c, 0x00003c86, 0x000086d0,
    0x0000d01a, 0x00001a64, 0x000064ae, 0x0000aef8, 0x0000f842, 0x0000428c, 0x00008cd6, 0x0000d620,
    0x0000206a, 0x00006ab4, 0x0000b4ff, 0x0000ff48, 0x00004892, 0x000092dc, 0x0000dc26, 0x00002670,
    0x000070ba, 0x0000ba04, 0x0000044e, 0x00004e98, 0x000098e2, 0x0000e22c, 0x00002c76, 0x000076c0,
    0x0000c00a, 0x00000a54, 0x0000549e, 0x00009ee8, 0x0000e832, 0x0000327c, 0x00007cc6, 0x0000c610,
    0x0000105a, 0x00005aa4, 0x0000a4ee, 0x0000ee38, 0x00003882, 0x000082cc, 0x0000cc16, 0x00001660,
    0x000060aa, 0x0000aaf4, 0x0000f43e, 0x00003e88, 0x000088d2, 0x0000d21c, 0x00001c66, 0x000066b0,
    0x0000b0fa, 0x0000fa44, 0x0000448e, 0x00008ed8, 0x0000d822, 0x0000226c, 0x00006cb6, 0x0000b600,
};

static f32b depth_w[256];

static void test_setup(void) {
    int i;
    depth_w[0] = sf_div(F32B_ONE, F(16.0f));    /* w = 2^(n/16 - 4) */
    for(i = 1; i < 256; i++)
        depth_w[i] = (i % 16) ? sf_mul(depth_w[i - 1], F(1.0442737824f)) : sf_mul(depth_w[i - 16], F(2.0f));
}

static void test_describe(void) {
    static const char *name[NUM_PAGES] = { "table fog, linear", "table fog, sawtooth", "vertex fog", "table fog mode 2", "no fog" };
    int p, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 64; c++) {
            label(p, c, "TR %s, squares %d..%d", name[p], c * 4, c * 4 + 3);
            label(p, 80 + c, "OP %s, squares %d..%d", name[p], c * 4, c * 4 + 3);
        }
}

static void squares(int list, int first, int page) {
    static const int type[NUM_PAGES] = { FogCtrl_Table, FogCtrl_Table, FogCtrl_Vertex, FogCtrl_Table2, FogCtrl_None };
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    int n, k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Gouraud = 0;                            /* flat */
    pcw.Offset = (page == 2);
    isp.DepthMode = DepthMode_Always;
    isp.ZWriteDis = 1;
    isp.CullMode = CullMode_None;
    isp.Gouraud = 0;
    isp.Offset = (page == 2);
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = type[page];
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    for(n = 0; n < 256; n++) {
        int x = cell_px(first + n / 4) + (n & 1) * 16, y = cell_py(first + n / 4) + ((n >> 1) & 1) * 16;
        f32b z = page == 2 ? F32B_ONE : sf_div(F32B_ONE, depth_w[n]);
        for(k = 0; k < 4; k++) {
            TA_Vertex0 v;                       /* the offset color's word (alpha = fog amount) is one the struct calls ignored */
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(k == 3);
            v.xyz[0] = f32b_int(x + (k & 1) * 16);
            v.xyz[1] = f32b_int(y + (k >> 1) * 16);
            v.xyz[2] = z;
            v.BaseCol = 0xff204080;
            v.ignore_3 = (uint32_t)n << 24;
            ta_send(&v, sizeof(v));
        }
    }
}

static void test_page(int page) {
    int i;

    pvrio_reg(FOG_COL_RAM_addr, FOG_COL_RAM_VALUE);
    pvrio_reg(FOG_COL_VERT_addr, FOG_COL_VERT_VALUE);
    pvrio_reg(FOG_DENSITY_addr, page == 0 ? FOG_DENSITY_LINEAR : FOG_DENSITY_SAW);
    for(i = 0; i < 128; i++)
        pvrio_reg(FOG_TABLE_START_addr + 4 * i, page == 0 ? fog_table_linear[i] : fog_table_saw[i]);
    ta_list_begin(ListType_Opaque);
    squares(ListType_Opaque, 80, page);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    squares(ListType_Translucent, 0, page);
    ta_list_end();
}
