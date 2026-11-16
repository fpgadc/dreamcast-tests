/* The PVR's parameter words, region array and registers, with the names nulldc3 uses (hw/pvr/ta_structs.h,
   hw/pvr/pvr_regs.h, rend/refsw2/refsw_lists_regtypes.h). A test sets the fields itself: there is no
   "context" that fills in defaults.

   No floating point instruction anywhere: a float is carried as its bit pattern (f32b). It comes precomputed
   from the test's generator, or is calculated with softfp.h (IEEE results from integer operations), so that
   the result is the same bits on the SH4 and on a PC whatever the compiler and its options. */
#ifndef PVR_PARAMS_H
#define PVR_PARAMS_H

#include <stdint.h>
#include "softfp.h"

#define F32B_ZERO   0x00000000u
#define F32B_ONE    0x3f800000u             /* 1.0 */
#define F32B_HALF   0x3f000000u             /* 0.5 */

/* n * 2^exp2 as a float, exactly; n may have at most 24 significant bits (checked by the caller's data, not
   here: more bits are cut off, toward zero) */
static inline f32b f32b_scaled(int32_t n, int exp2) {
    uint32_t sign = n < 0 ? 0x80000000u : 0, m = n < 0 ? 0u - (uint32_t)n : (uint32_t)n;
    int e = 150 + exp2;                     /* biased exponent when the mantissa's top bit is bit 23 */
    if(!m)
        return 0;
    while(m >= 0x01000000u) { m >>= 1; e++; }
    while(m < 0x00800000u) { m <<= 1; e--; }
    return sign | ((uint32_t)e << 23) | (m & 0x007fffffu);
}
static inline f32b f32b_int(int32_t n) { return f32b_scaled(n, 0); }

/* ---- parameter control word ---- */
typedef union PCW {
    struct {
        uint32_t UV_16bit    : 1;           /* obj control */
        uint32_t Gouraud     : 1;
        uint32_t Offset      : 1;
        uint32_t Texture     : 1;
        uint32_t Col_Type    : 2;
        uint32_t Volume      : 1;
        uint32_t Shadow      : 1;
        uint32_t Reserved    : 8;
        uint32_t User_Clip   : 2;           /* group control */
        uint32_t Strip_Len   : 2;
        uint32_t Res_2       : 3;
        uint32_t Group_En    : 1;
        uint32_t ListType    : 3;           /* para control */
        uint32_t Res_1       : 1;
        uint32_t EndOfStrip  : 1;
        uint32_t ParaType    : 3;
    };
    uint32_t full;
} PCW;

enum { ListType_Opaque = 0, ListType_Opaque_Modifier_Volume = 1, ListType_Translucent = 2,
       ListType_Translucent_Modifier_Volume = 3, ListType_Punch_Through = 4 };
enum { ParamType_End_Of_List = 0, ParamType_User_Tile_Clip = 1, ParamType_Object_List_Set = 2,
       ParamType_Polygon_or_Modifier_Volume = 4, ParamType_Sprite = 5, ParamType_Vertex_Parameter = 7 };
enum { Col_Type_Packed = 0, Col_Type_Floating = 1, Col_Type_Intensity1 = 2, Col_Type_Intensity2 = 3 };
enum { User_Clip_Disable = 0, User_Clip_Inside = 2, User_Clip_Outside = 3 };

/* ---- ISP/TSP instruction word ---- */
typedef union ISP_TSP {
    struct {
        uint32_t Reserved    : 20;
        uint32_t DCalcCtrl   : 1;
        uint32_t CacheBypass : 1;
        uint32_t UV_16b      : 1;           /* these four are replaced by the TA with the ones of the PCW */
        uint32_t Gouraud     : 1;
        uint32_t Offset      : 1;
        uint32_t Texture     : 1;
        uint32_t ZWriteDis   : 1;
        uint32_t CullMode    : 2;
        uint32_t DepthMode   : 3;
    };
    struct {
        uint32_t res         : 27;
        uint32_t CullMode    : 2;
        uint32_t VolumeMode  : 3;           /* 0 normal polygon, 1 inside last, 2 outside last */
    } modvol;
    uint32_t full;
} ISP_TSP;

enum { DepthMode_Never = 0, DepthMode_Less = 1, DepthMode_Equal = 2, DepthMode_LessEqual = 3, DepthMode_Greater = 4,
       DepthMode_NotEqual = 5, DepthMode_GreaterEqual = 6, DepthMode_Always = 7 };
enum { CullMode_None = 0, CullMode_Small = 1, CullMode_Negative = 2, CullMode_Positive = 3 };
enum { VolumeMode_Normal = 0, VolumeMode_Inside_Last = 1, VolumeMode_Outside_Last = 2 };

/* ---- TSP instruction word ---- */
typedef union TSP {
    struct {
        uint32_t TexV        : 3;
        uint32_t TexU        : 3;
        uint32_t ShadInstr   : 2;
        uint32_t MipMapD     : 4;
        uint32_t SupSample   : 1;
        uint32_t FilterMode  : 2;
        uint32_t ClampV      : 1;
        uint32_t ClampU      : 1;
        uint32_t FlipV       : 1;
        uint32_t FlipU       : 1;
        uint32_t IgnoreTexA  : 1;
        uint32_t UseAlpha    : 1;
        uint32_t ColorClamp  : 1;
        uint32_t FogCtrl     : 2;
        uint32_t DstSelect   : 1;           /* secondary accumulation buffer */
        uint32_t SrcSelect   : 1;
        uint32_t DstInstr    : 3;
        uint32_t SrcInstr    : 3;
    };
    uint32_t full;
} TSP;

enum { BlendInstr_Zero = 0, BlendInstr_One = 1, BlendInstr_OtherColor = 2, BlendInstr_InvOtherColor = 3,
       BlendInstr_SrcAlpha = 4, BlendInstr_InvSrcAlpha = 5, BlendInstr_DstAlpha = 6, BlendInstr_InvDstAlpha = 7 };
enum { FogCtrl_Table = 0, FogCtrl_Vertex = 1, FogCtrl_None = 2, FogCtrl_Table2 = 3 };
enum { ShadInstr_Decal = 0, ShadInstr_Modulate = 1, ShadInstr_DecalAlpha = 2, ShadInstr_ModulateAlpha = 3 };
enum { FilterMode_Point = 0, FilterMode_Bilinear = 1, FilterMode_TrilinearA = 2, FilterMode_TrilinearB = 3 };

/* ---- texture control word ---- */
typedef union TCW {
    struct {
        uint32_t TexAddr     : 21;          /* in 8 byte units, 64 bit area */
        uint32_t Reserved    : 4;
        uint32_t StrideSel   : 1;
        uint32_t ScanOrder   : 1;           /* 1: not twiddled */
        uint32_t PixelFmt    : 3;
        uint32_t VQ_Comp     : 1;
        uint32_t MipMapped   : 1;
    };
    struct {
        uint32_t pading_0    : 21;
        uint32_t PalSelect   : 6;
    };
    uint32_t full;
} TCW;

enum { PixelFmt_1555 = 0, PixelFmt_565 = 1, PixelFmt_4444 = 2, PixelFmt_YUV422 = 3, PixelFmt_BumpMap = 4,
       PixelFmt_Pal4 = 5, PixelFmt_Pal8 = 6 };

/* ---- what is sent to the tile accelerator: 32 bytes each (64 for sprite and modifier volume vertices) ---- */
typedef struct TA_PolyParam0 {              /* polygon, packed or floating colour */
    PCW pcw;
    ISP_TSP isp;
    TSP tsp;
    TCW tcw;
    uint32_t ign1, ign2;
    uint32_t SDMA_SIZE, SDMA_ADDR;
} TA_PolyParam0;

typedef struct TA_ModVolParam {
    PCW pcw;
    ISP_TSP isp;
    uint32_t ign[6];
} TA_ModVolParam;

typedef struct TA_SpriteParam {
    PCW pcw;
    ISP_TSP isp;
    TSP tsp;
    TCW tcw;
    uint32_t BaseCol, OffsCol;
    uint32_t SDMA_SIZE, SDMA_ADDR;
} TA_SpriteParam;

typedef struct TA_Vertex0 {                 /* non-textured, packed colour */
    PCW pcw;
    f32b xyz[3];
    uint32_t ignore_1, ignore_2;
    uint32_t BaseCol;
    uint32_t ignore_3;
} TA_Vertex0;

typedef struct TA_Vertex3 {                 /* textured, packed colour, 32 bit UV */
    PCW pcw;
    f32b xyz[3];
    f32b u, v;
    uint32_t BaseCol, OffsCol;
} TA_Vertex3;

typedef struct TA_Sprite0 {                 /* non-textured sprite: four corners, the fourth without a depth */
    PCW pcw;
    f32b x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3;
    uint32_t ignore[4];
} TA_Sprite0;

typedef struct TA_ModVol {                  /* one triangle of a modifier volume */
    PCW pcw;
    f32b x0, y0, z0, x1, y1, z1, x2, y2, z2;
    uint32_t ignore[6];
} TA_ModVol;

typedef struct TA_UserTileClip {            /* tiles, inclusive */
    PCW pcw;
    uint32_t ign[3];
    uint32_t xmin, ymin, xmax, ymax;
} TA_UserTileClip;

typedef struct TA_PolyParam3 {              /* polygon with two volumes: second TSP and TCW for inside a modifier volume */
    PCW pcw;
    ISP_TSP isp;
    TSP tsp;
    TCW tcw;
    TSP tsp1;
    TCW tcw1;
    uint32_t SDMA_SIZE, SDMA_ADDR;
} TA_PolyParam3;

typedef struct TA_Vertex9 {                 /* non-textured, packed colour, with two volumes */
    PCW pcw;
    f32b xyz[3];
    uint32_t BaseCol0, BaseCol1;
    uint32_t ignore_1, ignore_2;
} TA_Vertex9;

typedef struct TA_Vertex4 {                 /* textured, packed colour, 16 bit UV */
    PCW pcw;
    f32b xyz[3];
    uint32_t uv;                            /* uv16(u, v) */
    uint32_t ignore_1;
    uint32_t BaseCol, OffsCol;
} TA_Vertex4;

typedef struct TA_Sprite1 {                 /* textured sprite: the fourth corner without depth and UV */
    PCW pcw;
    f32b x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3;
    uint32_t ignore_1;
    uint32_t uv0, uv1, uv2;                 /* uv16(u, v) */
} TA_Sprite1;

/* 16 bit UV: the top halves of the two floats, U in the top half of the word */
static inline uint32_t uv16(f32b u, f32b v) { return (u & 0xffff0000u) | (v >> 16); }

/* ---- region array ---- */
typedef union RegionArrayEntryControl {
    struct {
        uint32_t res0        : 2;
        uint32_t tilex       : 6;
        uint32_t tiley       : 6;
        uint32_t res1        : 14;
        uint32_t no_writeout : 1;
        uint32_t pre_sort    : 1;
        uint32_t z_keep      : 1;
        uint32_t last_region : 1;
    };
    uint32_t full;
} RegionArrayEntryControl;

typedef union ListPointer {
    struct {
        uint32_t pad0         : 2;
        uint32_t ptr_in_words : 22;
        uint32_t pad1         : 7;
        uint32_t empty        : 1;
    };
    uint32_t full;
} ListPointer;

typedef struct RegionArrayEntry {
    RegionArrayEntryControl control;
    ListPointer opaque, opaque_mod, trans, trans_mod, puncht;
} RegionArrayEntry;

/* ---- register fields ---- */
typedef union ISP_BACKGND_T_type {
    struct {
        uint32_t tag_offset          : 3;
        uint32_t param_offs_in_words : 21;
        uint32_t skip                : 3;
        uint32_t shadow              : 1;
        uint32_t cache_bypass        : 1;
    };
    uint32_t full;
} ISP_BACKGND_T_type;

typedef union FB_W_CTRL_type {
    struct {
        uint32_t fb_packmode        : 3;
        uint32_t fb_dither          : 1;
        uint32_t pad0               : 4;
        uint32_t fb_kval            : 8;
        uint32_t fb_alpha_threshold : 8;
        uint32_t pad1               : 8;
    };
    uint32_t full;
} FB_W_CTRL_type;

enum { PAL_RAM_CTRL_ARGB1555 = 0, PAL_RAM_CTRL_RGB565 = 1, PAL_RAM_CTRL_ARGB4444 = 2, PAL_RAM_CTRL_ARGB8888 = 3 };

enum { fb_packmode_0555 = 0, fb_packmode_565 = 1, fb_packmode_4444 = 2, fb_packmode_1555 = 3, fb_packmode_888 = 4,
       fb_packmode_0888 = 5, fb_packmode_8888 = 6 };

typedef union TA_GLOB_TILE_CLIP_type {
    struct {
        uint32_t tile_x_num : 6;            /* number of tiles - 1 */
        uint32_t reserved   : 10;
        uint32_t tile_y_num : 4;
        uint32_t reserved2  : 12;
    };
    uint32_t full;
} TA_GLOB_TILE_CLIP_type;

typedef union TA_ALLOC_CTRL_type {          /* object pointer block sizes: 0 no list, 1 8, 2 16, 3 32 words */
    struct {
        uint32_t O_OPB    : 2;
        uint32_t pad0     : 2;
        uint32_t OM_OPB   : 2;
        uint32_t pad1     : 2;
        uint32_t T_OPB    : 2;
        uint32_t pad2     : 2;
        uint32_t TM_OPB   : 2;
        uint32_t pad3     : 2;
        uint32_t PT_OPB   : 2;
        uint32_t pad4     : 2;
        uint32_t OPB_Mode : 1;              /* 0: further blocks at increasing addresses */
        uint32_t pad5     : 11;
    };
    uint32_t full;
} TA_ALLOC_CTRL_type;

/* ---- registers: offsets from 0x005f8000 ---- */
#define PVR_REGISTERS(R) \
    R(ID, 0x000) R(REVISION, 0x004) R(SOFTRESET, 0x008) R(STARTRENDER, 0x014) R(TEST_SELECT, 0x018) \
    R(PARAM_BASE, 0x020) R(REGION_BASE, 0x02c) R(SPAN_SORT_CFG, 0x030) R(VO_BORDER_COL, 0x040) \
    R(FB_R_CTRL, 0x044) R(FB_W_CTRL, 0x048) R(FB_W_LINESTRIDE, 0x04c) R(FB_R_SOF1, 0x050) R(FB_R_SOF2, 0x054) \
    R(FB_R_SIZE, 0x05c) R(FB_W_SOF1, 0x060) R(FB_W_SOF2, 0x064) R(FB_X_CLIP, 0x068) R(FB_Y_CLIP, 0x06c) \
    R(FPU_SHAD_SCALE, 0x074) R(FPU_CULL_VAL, 0x078) R(FPU_PARAM_CFG, 0x07c) R(HALF_OFFSET, 0x080) \
    R(FPU_PERP_VAL, 0x084) R(ISP_BACKGND_D, 0x088) R(ISP_BACKGND_T, 0x08c) R(ISP_FEED_CFG, 0x098) \
    R(SDRAM_REFRESH, 0x0a0) R(SDRAM_ARB_CFG, 0x0a4) R(SDRAM_CFG, 0x0a8) R(FOG_COL_RAM, 0x0b0) \
    R(FOG_COL_VERT, 0x0b4) R(FOG_DENSITY, 0x0b8) R(FOG_CLAMP_MAX, 0x0bc) R(FOG_CLAMP_MIN, 0x0c0) \
    R(SPG_TRIGGER_POS, 0x0c4) R(SPG_HBLANK_INT, 0x0c8) R(SPG_VBLANK_INT, 0x0cc) R(SPG_CONTROL, 0x0d0) \
    R(SPG_HBLANK, 0x0d4) R(SPG_LOAD, 0x0d8) R(SPG_VBLANK, 0x0dc) R(SPG_WIDTH, 0x0e0) R(TEXT_CONTROL, 0x0e4) \
    R(VO_CONTROL, 0x0e8) R(VO_STARTX, 0x0ec) R(VO_STARTY, 0x0f0) R(SCALER_CTL, 0x0f4) R(PAL_RAM_CTRL, 0x108) \
    R(SPG_STATUS, 0x10c) R(FB_BURSTCTRL, 0x110) R(FB_C_SOF, 0x114) R(Y_COEFF, 0x118) R(PT_ALPHA_REF, 0x11c) \
    R(TA_OL_BASE, 0x124) R(TA_ISP_BASE, 0x128) R(TA_OL_LIMIT, 0x12c) R(TA_ISP_LIMIT, 0x130) R(TA_NEXT_OPB, 0x134) \
    R(TA_ISP_CURRENT, 0x138) R(TA_GLOB_TILE_CLIP, 0x13c) R(TA_ALLOC_CTRL, 0x140) R(TA_LIST_INIT, 0x144) \
    R(TA_YUV_TEX_BASE, 0x148) R(TA_YUV_TEX_CTRL, 0x14c) R(TA_YUV_TEX_CNT, 0x150) R(TA_LIST_CONT, 0x160) \
    R(TA_NEXT_OPB_INIT, 0x164)
#define PVR_REGISTER_ENUM(name, offset) name##_addr = offset,
enum { PVR_REGISTERS(PVR_REGISTER_ENUM) FOG_TABLE_START_addr = 0x200, PALETTE_RAM_START_addr = 0x1000 };

#endif
