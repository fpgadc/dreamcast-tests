/* Modifier volumes.

   Each cell has a polygon over the whole cell at z = 0.5 and modifier triangles ("sheets") over its left
   24 pixels. A pixel is inside a volume when an odd number of the volume's sheets are in front of it.

   Column = arrangement of sheets (z of each; 0.75 is in front of the polygon, 0.25 behind):
       0  one sheet in front              1  one sheet behind
       2  two sheets in front             3  one in front, one behind
       4  three sheets in front           5  two separate volumes, one sheet in front each
       6  one sheet in front, closed as an exclusion volume
       7  no sheets
   Row = kind of polygon:
       0  opaque, cheap shadow            1  opaque, two-volume (color 0x4080c0 outside, 0xc04020 inside)
       2  opaque, modifiers disabled      3-5  the same three in the translucent list
   Pages 0, 1, 2: cheap shadow intensity register 0x180, 0x100, 0x1ff. Page 3: 0x080 (intensity mode off). */
#define NUM_PAGES 4
#define TEST_MODIFIERS
#include "pvrtest2.h"

static const uint32_t shadow_reg[NUM_PAGES] = { 0x180, 0x100, 0x1ff, 0x080 };

static void test_setup(void) {
}

static void test_describe(void) {
    static const char *arr[8] = { "1 front", "1 behind", "2 front", "front + behind", "3 front", "2 volumes", "1 front, exclude",
                                  "none" };
    static const char *kind[6] = { "OP cheap shadow", "OP two-volume", "OP no modifiers", "TR cheap shadow", "TR two-volume", "TR no modifiers" };
    int p, r, c;

    for(p = 0; p < NUM_PAGES; p++)
        for(r = 0; r < 6; r++)
            for(c = 0; c < 8; c++)
                label(p, r * COLS + c, "reg %03x, %s, %s", (unsigned)shadow_reg[p], kind[r], arr[c]);
}

/* a 24x32 rectangle as two triangles; the last triangle carries the volume's mode (VolumeMode_*) */
static void sheet(int list, int mode, int x, int y, f32b z) {
    int k;

    for(k = 0; k < 2; k++) {
        TA_ModVolParam m;
        TA_ModVol v;

        memset(&m, 0, sizeof(m));
        m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        m.pcw.ListType = list;
        m.isp.modvol.VolumeMode = k ? mode : VolumeMode_Normal;
        m.isp.modvol.CullMode = CullMode_None;
        ta_send(&m, sizeof(m));
        memset(&v, 0, sizeof(v));
        v.pcw = pcw_vertex(1);
        v.x0 = f32b_int(x);       v.y0 = f32b_int(y);                  v.z0 = z;
        v.x1 = f32b_int(x + 24);  v.y1 = f32b_int(k ? y + 32 : y);     v.z1 = z;
        v.x2 = f32b_int(k ? x : x + 24);  v.y2 = f32b_int(y + 32);     v.z2 = z;
        ta_send(&v, sizeof(v));
    }
}

static void volumes(int list, int row) {
    const f32b Z25 = F(0.25f), Z75 = F(0.75f), Z80 = F(0.8f), Z85 = F(0.85f);
    int c;

    for(c = 0; c < 7; c++) {
        int x = cell_px(row * COLS + c), y = cell_py(row * COLS + c);
        int inc = VolumeMode_Inside_Last;
        switch(c) {
            case 0: sheet(list, inc, x, y, Z75); break;
            case 1: sheet(list, inc, x, y, Z25); break;
            case 2: sheet(list, VolumeMode_Normal, x, y, Z75); sheet(list, inc, x, y, Z80); break;
            case 3: sheet(list, VolumeMode_Normal, x, y, Z75); sheet(list, inc, x, y, Z25); break;
            case 4: sheet(list, VolumeMode_Normal, x, y, Z75); sheet(list, VolumeMode_Normal, x, y, Z80);
                    sheet(list, inc, x, y, Z85); break;
            case 5: sheet(list, inc, x, y, Z75); sheet(list, inc, x, y, Z80); break;
            case 6: sheet(list, VolumeMode_Outside_Last, x, y, Z75); break;
        }
    }
}

/* k = 0: cheap shadow, 1: two volumes (inside and outside a modifier volume), 2: modifiers disabled */
static void polys(int list, int first_row) {
    int k, c, i;

    for(k = 0; k < 3; k++) {
        for(c = 0; c < 8; c++) {
            PCW pcw = { .full = 0 };
            ISP_TSP isp = { .full = 0 };
            TSP tsp = { .full = 0 };
            TCW tcw = { .full = 0 };
            int x = cell_px((first_row + k) * COLS + c), y = cell_py((first_row + k) * COLS + c);

            pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
            pcw.ListType = list;
            pcw.Group_En = 1;
            pcw.Strip_Len = 1;
            pcw.Gouraud = 0;                                    /* flat */
            pcw.Shadow = k < 2;                                 /* affected by modifier volumes */
            pcw.Volume = k == 1;                                /* with a second color for inside */
            isp.DepthMode = DepthMode_Always;
            isp.ZWriteDis = 0;                                  /* writes its depth */
            isp.CullMode = CullMode_None;
            tsp.SrcInstr = BlendInstr_One;
            tsp.DstInstr = BlendInstr_Zero;
            tsp.FogCtrl = FogCtrl_None;
            tsp.UseAlpha = 1;
            if(k != 1) {
                ta_polygon(pcw, isp, tsp, tcw);
                quad0(x, y, 32, 32, F32B_HALF, 0xff4080c0);
                continue;
            }
            {
                TA_PolyParam3 p;
                memset(&p, 0, sizeof(p));
                p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw;
                p.tsp1 = tsp; p.tcw1 = tcw;                     /* inside: the same */
                ta_send(&p, sizeof(p));
            }
            for(i = 0; i < 4; i++) {
                TA_Vertex9 v;
                memset(&v, 0, sizeof(v));
                v.pcw = pcw_vertex(i == 3);
                v.xyz[0] = f32b_int(x + (i & 1) * 32);
                v.xyz[1] = f32b_int(y + (i >> 1) * 32);
                v.xyz[2] = F32B_HALF;
                v.BaseCol0 = 0xff4080c0;
                v.BaseCol1 = 0xffc04020;
                ta_send(&v, sizeof(v));
            }
        }
    }
}

static void test_page(int page) {
    int r;

    pvrio_reg(FPU_SHAD_SCALE_addr, shadow_reg[page]);
    ta_list_begin(ListType_Opaque);
    polys(ListType_Opaque, 0);
    ta_list_end();
    ta_list_begin(ListType_Opaque_Modifier_Volume);
    for(r = 0; r < 3; r++)
        volumes(ListType_Opaque_Modifier_Volume, r);
    ta_list_end();
    ta_list_begin(ListType_Translucent);
    polys(ListType_Translucent, 3);
    ta_list_end();
    /* the original's translucent modifier volume list (WITH_TR_MOD) gave a black frame on hardware: not sent */
}
