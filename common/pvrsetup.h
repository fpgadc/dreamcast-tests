/* The PVR set up for a test with nothing left to chance: every register the tile accelerator and the core
   read is written here, and so are the structures they read in video memory. No driver is involved; the
   calls go through pvrio.h, so the same thing happens in the offline renderer, on a console and in an
   emulator.

   The values are the ones a console has when a test runs under KallistiOS's driver (measured with
   ../regdump on a console with a VGA cable), and the region array is laid out the way KallistiOS does it,
   including the 18 zero words and the entry that draws nothing in front of it: without that entry the
   hardware sometimes hangs.

   Video memory (32 bit area addresses; the two halves are the two memory chips):
       000000  framebuffer, which is also what is displayed (one buffer: the dump reads what was rendered)
       400000  parameters written by the tile accelerator (PARAM_BASE, TA_ISP_BASE), up to 4fff00
       4fff00  the background plane's parameters
       500000  object lists (TA_OL_BASE): one block per tile and list, then three times as much for more
               blocks; after them the region array
   Textures: 64 bit area from 400000 (that is 200000-3fffff of both chips). */
#ifndef PVRSETUP_H
#define PVRSETUP_H

#include "pvrio.h"

#define SETUP_TILES_X       20
#define SETUP_TILES_Y       15
#define SETUP_FB            0x000000u
#define SETUP_PARAMS        0x400000u
#define SETUP_PARAMS_LIMIT  0x4fff00u
#define SETUP_BACKGROUND    0x4fff00u
#define SETUP_OL            0x500000u
#define SETUP_TEXTURES      0x400000u       /* 64 bit area */

typedef struct pvr_setup_t {
    int opb_words[5];       /* object pointer block of each list, words per tile: 0 (no such list), 8, 16 or 32 */
    int pre_sort;           /* 1: translucent polygons are drawn in the order sent; 0: sorted per pixel */
    int fb_packmode;        /* fb_packmode_0888, fb_packmode_565 or fb_packmode_0555 */
    int fb_dither;
} pvr_setup_t;

/* written at every render; a test may change them before pvr_frame_render() */
static struct {
    uint32_t bg_color;                      /* background plane, 0RGB */
    f32b bg_depth;                          /* ISP_BACKGND_D */
    uint32_t FB_X_CLIP, FB_Y_CLIP;
} pvr_frame = { 0x00000000, 0x38d1b717 /* 0.0001 */, 639u << 16, 479u << 16 };

static pvr_setup_t pvr_cfg;
static uint32_t pvr_ol_bytes;               /* the object lists' first blocks, all tiles and lists */
static uint32_t pvr_region;                 /* REGION_BASE */
static uint32_t pvr_lists_sent;
static int pvr_list_open = -1;
static uint32_t pvr_tex_next = SETUP_TEXTURES;

static uint32_t pvr_list_base[5];

/* The region array: 18 zero words, an entry that draws nothing, and the 300 tiles. order 0: column by column,
   as KallistiOS writes it (what every test uses). The others are for tests of the order itself:
   1 columns from the right; 2 row by row; 3 row by row from the bottom; 4 as 0 with the last column first;
   5 as 0 without the last column. */
static void pvr_region_write(int order) {
    static uint32_t region[18 + 6 * (1 + SETUP_TILES_X * SETUP_TILES_Y)];
    RegionArrayEntry e;
    uint32_t bytes;
    unsigned i;
    int x, y, n, k, tile, count;

    memset(region, 0, sizeof(region));
    n = 18;                                     /* 18 zero words, not part of the array */
    memset(&e, 0, sizeof(e));                   /* the entry that draws nothing */
    e.control.no_writeout = 1;
    e.opaque.empty = e.opaque_mod.empty = e.trans.empty = e.trans_mod.empty = e.puncht.empty = 1;
    memcpy(&region[n], &e, sizeof(e));
    n += 6;
    count = order == 5 ? (SETUP_TILES_X - 1) * SETUP_TILES_Y : SETUP_TILES_X * SETUP_TILES_Y;
    for(k = 0; k < count; k++, n += 6) {
        ListPointer *p[5] = { &e.opaque, &e.opaque_mod, &e.trans, &e.trans_mod, &e.puncht };
        switch(order) {
        default: x = k / SETUP_TILES_Y;                         y = k % SETUP_TILES_Y; break;
        case 1:  x = SETUP_TILES_X - 1 - k / SETUP_TILES_Y;     y = k % SETUP_TILES_Y; break;
        case 2:  x = k % SETUP_TILES_X;                         y = k / SETUP_TILES_X; break;
        case 3:  x = k % SETUP_TILES_X;                         y = SETUP_TILES_Y - 1 - k / SETUP_TILES_X; break;
        case 4:  x = (k / SETUP_TILES_Y + SETUP_TILES_X - 1) % SETUP_TILES_X; y = k % SETUP_TILES_Y; break;
        }
        memset(&e, 0, sizeof(e));
        tile = SETUP_TILES_X * y + x;
        e.control.tilex = x;
        e.control.tiley = y;
        e.control.pre_sort = pvr_cfg.pre_sort;
        e.control.last_region = k == count - 1;
        for(i = 0; i < 5; i++) {
            bytes = (uint32_t)pvr_cfg.opb_words[i] * 4;
            if(bytes)
                p[i]->ptr_in_words = (pvr_list_base[i] + bytes * tile) >> 2;
            else
                p[i]->empty = 1;
        }
        memcpy(&region[n], &e, sizeof(e));
    }
    pvrio_w32(pvr_region - 18 * 4, region, n);
}

static void pvr_setup(const pvr_setup_t *cfg) {
    /* fixed registers: (address, value under KallistiOS on a console) */
    static const struct { uint16_t addr; uint32_t value; } fixed[] = {
        { SPAN_SORT_CFG_addr,   0x00000101 },
        { FPU_SHAD_SCALE_addr,  0x00000001 },
        { FPU_CULL_VAL_addr,    0x3f800000 },   /* 1.0: left by the boot ROM; KallistiOS never writes it */
        { FPU_PARAM_CFG_addr,   0x0027df77 },   /* bit 21: region array entries of six words */
        { HALF_OFFSET_addr,     0x00000007 },
        { FPU_PERP_VAL_addr,    0x00000000 },
        { ISP_FEED_CFG_addr,    0x00800408 },
        { SDRAM_REFRESH_addr,   0x00000020 },
        { SDRAM_ARB_CFG_addr,   0x0000001f },
        { SDRAM_CFG_addr,       0x15d1c951 },
        { FOG_COL_RAM_addr,     0x007f7f7f },
        { FOG_COL_VERT_addr,    0x007f7f7f },
        { FOG_DENSITY_addr,     0x0000ff07 },
        { FOG_CLAMP_MAX_addr,   0xffffffff },
        { FOG_CLAMP_MIN_addr,   0x00000000 },
        { TEXT_CONTROL_addr,    0x00000000 },
        { SCALER_CTL_addr,      0x00000400 },   /* no scaling (KallistiOS: 0x401 unless the cable is VGA) */
        { PAL_RAM_CTRL_addr,    0x00000000 },   /* the boot ROM leaves 3; KallistiOS writes it on request */
        { FB_BURSTCTRL_addr,    0x00093f39 },
        { Y_COEFF_addr,         0x00008040 },
        { PT_ALPHA_REF_addr,    0x000000ff },
    };
    uint32_t at = SETUP_OL;
    FB_W_CTRL_type fbw = { .full = 0 };
    unsigned i;
    int n;

    pvr_cfg = *cfg;
    pvrio_open();
    pvrio_video(cfg->fb_packmode, SETUP_FB);
    pvrio_reg(SOFTRESET_addr, 0xffffffff);
    pvrio_reg(SOFTRESET_addr, 0x00000000);
    for(i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++)
        pvrio_reg(fixed[i].addr, fixed[i].value);
    fbw.fb_packmode = cfg->fb_packmode;
    fbw.fb_dither = cfg->fb_dither;
    pvrio_reg(FB_W_CTRL_addr, fbw.full);
    pvrio_reg(FB_W_LINESTRIDE_addr, cfg->fb_packmode == fb_packmode_0888 ? 640 * 4 / 8 : 640 * 2 / 8);
    pvrio_reg(FB_W_SOF1_addr, SETUP_FB);
    pvrio_reg(FB_W_SOF2_addr, SETUP_FB);
    pvrio_reg(FB_X_CLIP_addr, pvr_frame.FB_X_CLIP);
    pvrio_reg(FB_Y_CLIP_addr, pvr_frame.FB_Y_CLIP);
    for(i = 0; i < 128; i++)                    /* fog table and palette: defined, whatever was there */
        pvrio_reg(FOG_TABLE_START_addr + i * 4, 0);
    for(i = 0; i < 1024; i++)
        pvrio_reg(PALETTE_RAM_START_addr + i * 4, 0);

    /* object lists, one after the other; then the region array, 128 byte aligned */
    for(n = 0; n < 5; n++) {
        pvr_list_base[n] = at;
        at += (uint32_t)cfg->opb_words[n] * 4 * SETUP_TILES_X * SETUP_TILES_Y;
    }
    pvr_ol_bytes = at - SETUP_OL;
    at = (SETUP_OL + pvr_ol_bytes * 4 + 127) & ~127u;
    pvr_region = at + 18 * 4;
    pvr_region_write(0);
}

/* ---- a frame: pvr_frame_begin, lists, pvr_frame_render ---- */

static void pvr_frame_begin(void) {
    static const uint8_t code[33] = { [8] = 1, [16] = 2, [32] = 3 };
    TA_ALLOC_CTRL_type alloc = { .full = 0 };
    TA_GLOB_TILE_CLIP_type clip = { .full = 0 };

    alloc.O_OPB = code[pvr_cfg.opb_words[ListType_Opaque]];
    alloc.OM_OPB = code[pvr_cfg.opb_words[ListType_Opaque_Modifier_Volume]];
    alloc.T_OPB = code[pvr_cfg.opb_words[ListType_Translucent]];
    alloc.TM_OPB = code[pvr_cfg.opb_words[ListType_Translucent_Modifier_Volume]];
    alloc.PT_OPB = code[pvr_cfg.opb_words[ListType_Punch_Through]];
    clip.tile_x_num = SETUP_TILES_X - 1;
    clip.tile_y_num = SETUP_TILES_Y - 1;
    pvr_lists_sent = 0;
    pvr_list_open = -1;
    pvrio_reg(TA_OL_BASE_addr, SETUP_OL);
    pvrio_reg(TA_NEXT_OPB_INIT_addr, SETUP_OL + pvr_ol_bytes);
    pvrio_reg(TA_OL_LIMIT_addr, SETUP_OL + pvr_ol_bytes * 4);
    pvrio_reg(TA_ISP_BASE_addr, SETUP_PARAMS);
    pvrio_reg(TA_ISP_LIMIT_addr, SETUP_PARAMS_LIMIT);
    pvrio_reg(TA_GLOB_TILE_CLIP_addr, clip.full);
    pvrio_reg(TA_ALLOC_CTRL_addr, alloc.full);
    pvrio_reg(TA_LIST_INIT_addr, 0x80000000);
}

/* one or more whole parameters */
static void ta_send(const void *params, int bytes) {
    pvrio_ta(params, bytes / 4);
}

/* A list ends with a polygon parameter without vertices and then the end of list parameter, as under
   KallistiOS: a list with nothing in it is an error for the hardware. */
static void ta_list_end(void) {
    TA_PolyParam0 blank;
    uint32_t end[8] = { 0 };

    memset(&blank, 0, sizeof(blank));
    blank.pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
    blank.pcw.ListType = pvr_list_open;
    blank.pcw.Group_En = 1;
    blank.pcw.Strip_Len = 1;
    blank.pcw.Col_Type = Col_Type_Floating;
    blank.pcw.Gouraud = 1;
    ta_send(&blank, sizeof(blank));
    ta_send(end, sizeof(end));
    pvr_lists_sent |= 1u << pvr_list_open;
    pvr_list_open = -1;
}

static void ta_list_begin(int list) {
    if(pvr_list_open >= 0)
        ta_list_end();
    pvr_list_open = list;
}

static void pvr_frame_render(void) {
    ISP_TSP isp = { .full = 0 };
    TSP tsp = { .full = 0 };
    ISP_BACKGND_T_type tag = { .full = 0 };
    uint32_t bg[15], events = 0;
    int i;

    if(pvr_list_open >= 0)
        ta_list_end();
    for(i = 0; i < 5; i++)                      /* every list the region array points to has to be sent */
        if(pvr_cfg.opb_words[i]) {
            if(!(pvr_lists_sent & (1u << i))) {
                ta_list_begin(i);
                ta_list_end();
            }
            events |= PVRIO_LIST_DONE(i);
        }
    pvrio_wait(events);

    /* the background plane: three corners of the screen at depth FLT_EPSILON, as KallistiOS sends it */
    isp.DepthMode = DepthMode_Greater;
    isp.CullMode = CullMode_Negative;
    isp.Gouraud = 1;
    tsp.SrcInstr = BlendInstr_One;
    tsp.FogCtrl = FogCtrl_None;
    tsp.MipMapD = 4;
    tsp.ShadInstr = ShadInstr_Modulate;
    bg[0] = isp.full; bg[1] = tsp.full; bg[2] = 0;
    bg[3] = f32b_int(0);   bg[4] = f32b_int(480);  bg[5] = 0x34000000;  bg[6] = pvr_frame.bg_color;
    bg[7] = f32b_int(0);   bg[8] = f32b_int(0);    bg[9] = 0x34000000;  bg[10] = pvr_frame.bg_color;
    bg[11] = f32b_int(640); bg[12] = f32b_int(480); bg[13] = 0x34000000; bg[14] = pvr_frame.bg_color;
    pvrio_w32(SETUP_BACKGROUND, bg, 15);
    tag.param_offs_in_words = (SETUP_BACKGROUND - SETUP_PARAMS) >> 2;
    tag.skip = 1;

    pvrio_reg(REGION_BASE_addr, pvr_region);
    pvrio_reg(PARAM_BASE_addr, SETUP_PARAMS);
    pvrio_reg(FB_W_SOF1_addr, SETUP_FB);
    pvrio_reg(ISP_BACKGND_T_addr, tag.full);
    pvrio_reg(ISP_BACKGND_D_addr, pvr_frame.bg_depth);
    pvrio_reg(FB_X_CLIP_addr, pvr_frame.FB_X_CLIP);
    pvrio_reg(FB_Y_CLIP_addr, pvr_frame.FB_Y_CLIP);
    pvrio_reg(STARTRENDER_addr, 0xffffffff);
    pvrio_wait(PVRIO_RENDER_DONE);
}

/* the rendered picture to a file: 640x480 words of 0RGB, or 16 bit pixels in pairs */
static void pvr_frame_dump(const char *name) {
    int wide = pvr_cfg.fb_packmode == fb_packmode_0888;
    pvrio_dump32(SETUP_FB, wide ? 640 * 480 : 640 * 480 / 2, name, wide ? ".raw" : "16.raw");
}

/* ---- parameters ---- */

static void ta_polygon(PCW pcw, ISP_TSP isp, TSP tsp, TCW tcw) {
    TA_PolyParam0 p;
    memset(&p, 0, sizeof(p));
    p.pcw = pcw; p.isp = isp; p.tsp = tsp; p.tcw = tcw;
    ta_send(&p, sizeof(p));
}

static PCW pcw_vertex(int EndOfStrip) {
    PCW pcw = { .full = 0 };
    pcw.ParaType = ParamType_Vertex_Parameter;
    pcw.EndOfStrip = EndOfStrip;
    return pcw;
}

static void ta_vertex0(f32b x, f32b y, f32b z, uint32_t BaseCol, int EndOfStrip) {
    TA_Vertex0 v;
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(EndOfStrip);
    v.xyz[0] = x; v.xyz[1] = y; v.xyz[2] = z;
    v.BaseCol = BaseCol;
    ta_send(&v, sizeof(v));
}

static void ta_vertex3(f32b x, f32b y, f32b z, f32b u, f32b v_, uint32_t BaseCol, uint32_t OffsCol, int EndOfStrip) {
    TA_Vertex3 v;
    v.pcw = pcw_vertex(EndOfStrip);
    v.xyz[0] = x; v.xyz[1] = y; v.xyz[2] = z;
    v.u = u; v.v = v_;
    v.BaseCol = BaseCol; v.OffsCol = OffsCol;
    ta_send(&v, sizeof(v));
}

static void ta_vertex4(f32b x, f32b y, f32b z, f32b u, f32b v_, uint32_t BaseCol, uint32_t OffsCol, int EndOfStrip) {
    TA_Vertex4 v;
    memset(&v, 0, sizeof(v));
    v.pcw = pcw_vertex(EndOfStrip);
    v.xyz[0] = x; v.xyz[1] = y; v.xyz[2] = z;
    v.uv = uv16(u, v_);
    v.BaseCol = BaseCol; v.OffsCol = OffsCol;
    ta_send(&v, sizeof(v));
}

/* ---- palette ---- */

static void pvr_pal_format(int PAL_RAM_CTRL_value) { pvrio_reg(PAL_RAM_CTRL_addr, (uint32_t)PAL_RAM_CTRL_value); }
static void pvr_pal_entry(int index, uint32_t value) { pvrio_reg(PALETTE_RAM_START_addr + 4 * (uint32_t)index, value); }

/* ---- textures ---- */

/* video memory for a texture: its address in the 64 bit area (TCW.TexAddr is this / 8) */
static uint32_t pvr_tex_alloc(int bytes) {
    uint32_t at = pvr_tex_next;
    pvr_tex_next += ((uint32_t)bytes + 31) & ~31u;
    return at;
}

/* bytes to a texture address, as they are */
static void pvr_tex_load(uint32_t addr, const void *data, int bytes) {
    static uint32_t words[1 << 18];
    const uint8_t *b = (const uint8_t *)data;
    int i, n = (bytes + 3) / 4;

    if(n > (int)(sizeof(words) / 4))
        n = sizeof(words) / 4;
    for(i = 0; i < n; i++)
        words[i] = 0;
    for(i = 0; i < bytes && i < (int)sizeof(words); i++)
        words[i / 4] |= (uint32_t)b[i] << (8 * (i % 4));
    pvrio_w64(addr, words, n);
}

/* the place of texel (x, y) in a twiddled texture, in texels */
static uint32_t tw_spread(uint32_t x) {
    uint32_t r = 0;
    int k;
    for(k = 0; k < 10; k++)
        r |= (x & (1u << k)) << k;
    return r;
}
static uint32_t tw_index(uint32_t x, uint32_t y) { return tw_spread(y) | (tw_spread(x) << 1); }

/* A w x h picture of 16 bit texels (row by row) to a texture address, twiddled; not square: one twiddled
   square after the other, as KallistiOS's pvr_txr_load_ex does it. */
static void pvr_tex_load_twiddled16(uint32_t addr, const uint16_t *pixels, int w, int h) {
    static uint16_t out[1 << 18];
    int x, y, min = w < h ? w : h, mask = min - 1;

    for(y = 0; y < h; y++)
        for(x = 0; x < w; x++)
            out[tw_index(x & mask, y & mask) + (x / min + y / min) * min * min] = pixels[y * w + x];
    pvr_tex_load(addr, out, w * h * 2);
}

/* the same for 8 bit texels (palette indices) */
static void pvr_tex_load_twiddled8(uint32_t addr, const uint8_t *pixels, int w, int h) {
    static uint8_t out[1 << 18];
    int x, y, min = w < h ? w : h, mask = min - 1;

    for(y = 0; y < h; y++)
        for(x = 0; x < w; x++)
            out[tw_index(x & mask, y & mask) + (x / min + y / min) * min * min] = pixels[y * w + x];
    pvr_tex_load(addr, out, w * h);
}

#endif
