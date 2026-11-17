/* What is a run of pixels for the TSP, when a modifier volume, punch-through passes or the per-pixel sort of
   translucent polygons cut a triangle's line into parts? Read with pvr_tests/uvmeas's texture (texel i holds the
   number i, so a pixel shows the texture coordinate the TSP computed there) and scored with model/tspprobe:
   a run that starts anew where a part starts gives other numbers to the right of that place than one that goes
   on (the coordinate is the run's first pixel's plus a step per pixel, rounded down on the way).

   Every cell has one textured triangle of uvmeas's steep mix (cases.h, gen.py: slopes along x with 24 bits) and bands over
   its columns 8-11 and 18-23. Two pages of triangles per kind (page = 2 kind + 0 or 1):
     kind 0  reference: translucent list in the order sent, no bands (uvmeas as it is)
     kind 1  opaque list, cheap shadow (shadow bit; FPU_SHAD_SCALE 0x180), an opaque modifier volume over each band
     kind 2  opaque list, two volumes (the second TSP, TCW, UV and colour the same as the first), volumes over the bands
     kind 3  the same two-volume polygons without the volumes (control)
     kind 4  punch-through list; the bands are untextured polygons in front with alpha 0, which fails the alpha test
             (PT_ALPHA_REF 0xff): the triangle under them is shaded in a later pass
     kind 5  translucent list sorted per pixel (pre-sort bit 0); the bands are polygons behind the triangle, blend
             ONE, ZERO for both: where a band is, the triangle is the second layer of the pixel
   The PVR is driven directly (../common/pvrtest2.h). */
#define KINDS 6
#define NUM_PAGES (2 * KINDS)
#define TEST_MODIFIERS
#include "pvr_params.h"
#include "cases.h"
#include "pvrtest2.h"

static uint32_t texture;        /* 64 bit area address */
static const int bands[2][2] = { { 8, 4 }, { 18, 6 } };     /* first column, width */

typedef struct {                /* vertex, textured, packed colour, 32 bit UV, with two volumes (64 bytes) */
    PCW pcw;
    f32b xyz[3];
    f32b u0, v0;
    uint32_t BaseCol0, OffsCol0;
    f32b u1, v1;
    uint32_t BaseCol1, OffsCol1;
    uint32_t ignore[4];
} vertex11_t;

static void test_setup(void) {
    static uint32_t words[1024 * 8 / 2];
    int i, j;
    for(j = 0; j < 8; j++)
        for(i = 0; i < 1024; i += 2)
            words[(j * 1024 + i) / 2] = (uint32_t)i | (uint32_t)(i + 1) << 16;
    texture = pvr_tex_alloc(sizeof(words));
    pvrio_w64(texture, words, sizeof(words) / 4);
}

static void test_describe(void) {
    int p;
    for(p = 0; p < NUM_PAGES; p++)
        label_span(p, 0, 20, 15, "kind %d page %d", p / 2, p);
}

static void triangle(int kind, const tri_t *t) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    const int list = kind == 0 || kind == 5 ? ListType_Translucent : kind == 4 ? ListType_Punch_Through : ListType_Opaque;
    int k;

    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    pcw.Texture = 1;
    pcw.Shadow = kind >= 1 && kind <= 3;
    pcw.Volume = kind == 2 || kind == 3;
    isp.DepthMode = kind == 0 ? DepthMode_Always : DepthMode_GreaterEqual;
    isp.ZWriteDis = kind == 0;
    isp.Texture = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    tsp.ShadInstr = ShadInstr_Decal;
    tsp.FilterMode = FilterMode_Point;
    tsp.MipMapD = 4;
    tsp.TexU = 7;                               /* 1024 */
    tsp.TexV = 0;                               /* 8 */
    tcw.PixelFmt = PixelFmt_565;
    tcw.ScanOrder = 1;
    tcw.TexAddr = texture >> 3;
    if(pcw.Volume) {
        TA_PolyParam3 p;
        memset(&p, 0, sizeof(p));
        p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw; p.tsp1 = tsp; p.tcw1 = tcw;
        ta_send(&p, sizeof(p));
        for(k = 0; k < 3; k++) {
            vertex11_t v;
            memset(&v, 0, sizeof(v));
            v.pcw = pcw_vertex(k == 2);
            v.xyz[0] = t->v[k][0]; v.xyz[1] = t->v[k][1]; v.xyz[2] = t->v[k][3];
            v.u0 = v.u1 = t->v[k][2];
            v.v0 = v.v1 = F32B_HALF;
            v.BaseCol0 = v.BaseCol1 = 0xffffffffu;
            ta_send(&v, sizeof(v));
        }
        return;
    }
    ta_polygon(pcw, isp, tsp, tcw);
    for(k = 0; k < 3; k++)                      /* v: x, y, u, z */
        ta_vertex3(t->v[k][0], t->v[k][1], t->v[k][3], t->v[k][2], F32B_HALF, 0xffffffff, 0, k == 2);
}

/* an untextured quad over band b of the cell at (x, y), depth z, colour argb, in the list */
static void band_quad(int list, int x, int y, int b, f32b z, uint32_t argb) {
    PCW pcw = { .full = 0 };
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    TCW tcw = { .full = 0 };
    pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    pcw.ListType = list;
    pcw.Group_En = 1;
    pcw.Strip_Len = 1;
    isp.DepthMode = DepthMode_GreaterEqual;
    tsp.SrcInstr = BlendInstr_One;
    tsp.DstInstr = BlendInstr_Zero;
    tsp.FogCtrl = FogCtrl_None;
    tsp.UseAlpha = 1;
    ta_polygon(pcw, isp, tsp, tcw);
    quad0(x + bands[b][0], y, bands[b][1], 32, z, argb);
}

/* an opaque modifier volume over band b: a rectangle of two triangles in front of everything, closed "inside" */
static void band_volume(int x, int y, int b) {
    const int x0 = x + bands[b][0], x1 = x0 + bands[b][1];
    int k;
    for(k = 0; k < 2; k++) {
        TA_ModVolParam m;
        TA_ModVol v;
        memset(&m, 0, sizeof(m));
        m.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        m.pcw.ListType = ListType_Opaque_Modifier_Volume;
        m.isp.modvol.VolumeMode = k ? VolumeMode_Inside_Last : VolumeMode_Normal;
        m.isp.modvol.CullMode = CullMode_None;
        ta_send(&m, sizeof(m));
        memset(&v, 0, sizeof(v));
        v.pcw = pcw_vertex(1);
        v.x0 = f32b_int(x0); v.y0 = f32b_int(y);                v.z0 = F(2.0f);
        v.x1 = f32b_int(x1); v.y1 = f32b_int(k ? y + 32 : y);   v.z1 = F(2.0f);
        v.x2 = f32b_int(k ? x0 : x1); v.y2 = f32b_int(y + 32);  v.z2 = F(2.0f);
        ta_send(&v, sizeof(v));
    }
}

static void test_page(int page) {
    const int kind = page / 2;
    const int list = kind == 0 || kind == 5 ? ListType_Translucent : kind == 4 ? ListType_Punch_Through : ListType_Opaque;
    int i, b, n;

    pvr_cfg.pre_sort = kind == 5 ? 0 : 1;
    pvr_region_write(0);
    pvrio_reg(FPU_SHAD_SCALE_addr, kind == 1 ? 0x180u : 0x001u);
    ta_list_begin(list);
    for(i = 0, n = 0; i < NUM_TRIS; i++) {
        const tri_t *t = &tris[i];
        if(t->page != page)
            continue;
        triangle(kind, t);
        if(kind == 4 || kind == 5) {
            const int x = cell_px(n), y = cell_py(n);       /* a page's triangles come in the order of the cells */
            for(b = 0; b < 2; b++)
                band_quad(list, x, y, b, kind == 4 ? F(2.0f) : F(0.5f), kind == 4 ? 0x00ff0000u : 0xffff0000u);
        }
        n++;
    }
    ta_list_end();
    if(kind == 1 || kind == 2) {
        ta_list_begin(ListType_Opaque_Modifier_Volume);
        for(i = 0, n = 0; i < NUM_TRIS; i++) {
            if(tris[i].page != page)
                continue;
            for(b = 0; b < 2; b++)
                band_volume(cell_px(n), cell_py(n), b);
            n++;
        }
        ta_list_end();
    }
}
