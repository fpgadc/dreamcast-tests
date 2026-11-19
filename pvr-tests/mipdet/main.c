/* The mipmap level when the setup's determinant is huge (2026-11-19; found on polly2-data sydney_race and
   sydney_stadium, pvr-tests/mipdump). The level's length is multiplied once by 1/det^2, the determinant's 24 bit
   reciprocal squared and cut to 24 bits; refsw3 took that as any number, the console takes it as a number of the
   setup's range: below 2^-126, a determinant above about 2^63, it is zero, and the level is the largest map
   without blending whatever the numerators are.

   Every cell: a right triangle whose legs leave the screen (so it covers its cell), drawn only in its own tile (a
   user clip of that tile), flat 1/w = 2^-20 (the numerators stay near 2^83, far from overflowing), U along x and V
   along y at s texels a pixel (s = k / 16, k = 24 .. 96, every cell its own; with D 4/4 the levels 1 .. 2 and a
   fraction), a 64x64 mipmapped texture whose every level is one colour (pvr-tests/mipdump), point sampled, so a cell
   shows its level; the trilinear pages show 255 times the fraction (pass B of white levels). The corners are
   computed in soft float (common/softfp.h), so the PC and the console send the same bits.
   Legs: "up-left" from the cell's bottom right corner, "down-right" from its top left (the other sign of the
   determinant).
     page 0  square legs, log2 det = 61 + c / 100 (cell c), up-left, the level
     page 1  page 0, trilinear B: the fraction
     page 2  page 0 down-right
     page 3  log2 det = 63 + c / 100 (63 .. 66), up-left, the level
     page 4  legs 2^31 and 2^32 + j 2^12 (j = c - 150): det = 2^63 + j 2^43, up-left, the level
     page 5  page 4, trilinear B
     page 6  page 4 down-right */
#define NUM_PAGES 7
#include "pvrtest2.h"
#include "texgen.h"

static uint32_t tex_col, tex_white;

static uint32_t hsh(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; v ^= v >> 13; return v; }
static uint32_t level_colour(const void *ctx, int size, int x, int y)
{
    (void)ctx; (void)x; (void)y;
    return hsh((uint32_t)size * 31u + 7u) & 0xffff;
}
static uint32_t white(const void *ctx, int size, int x, int y) { (void)ctx; (void)size; (void)x; (void)y; return 0xffff; }

static void test_setup(void)
{
    const tex_layout_t l = { TEX_MIP, 16, 64, 64, 0 };
    tex_col = tex_build(&l, level_colour, NULL);
    tex_white = tex_build(&l, white, NULL);
}

/* 2^(t / 200) * 2^23 for t = 0 .. 199: a mantissa (8388608 .. 16777215) for half a step of log2 det */
static uint32_t mant_half(int t)
{
    /* 2^(i/200), i = 0 .. 199, times 2^23, computed once on the PC and written here would be the usual way; a
       product of the powers 2^(1/200), 2^(2/200), .. in integer arithmetic gives the same numbers on both sides */
    static const uint32_t r = 8417726u;          /* 2^(1/200) * 2^23, rounded */
    uint64_t m = 8388608u;
    int i;
    for(i = 0; i < t; i++)
        m = (m * r) >> 23;
    return (uint32_t)m;
}

/* a cell's legs as (mantissa, exponent) pairs: Lx = mx * 2^ex, Ly = my * 2^ey */
static void legs(int page, int c, uint32_t *mx, int *ex, uint32_t *my, int *ey)
{
    if(page >= 4) {                         /* 2^31 and 2^32 + j 2^12 */
        *mx = 1u << 23; *ex = 31 - 23;
        *my = (1u << 20) + (uint32_t)(c - 150); *ey = 12;   /* (2^20 + j) * 2^12 */
        return;
    }
    {
        /* log2 det = base + c / 100: each leg 2^((base + c / 100) / 2) = 2^(h / 200) with h = 100 base + c */
        const int h = (page == 3 ? 6300 : 6100) + c;
        *mx = *my = mant_half(h % 200);
        *ex = *ey = h / 200 - 23;
    }
}

static int scale_k(int c) { return 24 + (int)(hsh((uint32_t)c + 99u) % 73u); }    /* s = k / 16 texels a pixel */

static void test_describe(void)
{
    static const char *what[NUM_PAGES] = { "level, square legs up-left, log2 det 61 + c/100", "fraction (trilinear B), as page 0",
        "level, down-right, as page 0", "level, square legs up-left, log2 det 63 + c/100", "level, legs 2^31 x (2^32 + (c - 150) 2^12), up-left",
        "fraction (trilinear B), as page 4", "level, down-right, as page 4" };
    int p, c;
    for(p = 0; p < NUM_PAGES; p++)
        for(c = 0; c < 300; c++)
            label(p, c, "%s; s = %d/16 texels a pixel", what[p], scale_k(c));
}

static void test_page(int page)
{
    const int downright = page == 2 || page == 6, tri = page == 1 || page == 5;
    int c, k;
    pvr_frame.bg_color = 0x000040;
    ta_list_begin(ListType_Opaque);
    for(c = 0; c < 300; c++) {
        PCW pcw = { .full = 0 };
        ISP_TSP isp = { .full = 0 };
        TSP tsp = { .full = 0 };
        TCW tcw = { .full = 0 };
        TA_UserTileClip clip;
        uint32_t mx, my;
        int ex, ey;
        const int x0 = cell_px(c), y0 = cell_py(c), kk = scale_k(c);
        f32b X[3], Y[3], U[3], V[3];
        legs(page, c, &mx, &ex, &my, &ey);
        memset(&clip, 0, sizeof(clip));
        clip.pcw.ParaType = ParamType_User_Tile_Clip;
        clip.xmin = clip.xmax = (uint32_t)(x0 / 32);
        clip.ymin = clip.ymax = (uint32_t)(y0 / 32);
        ta_send(&clip, sizeof(clip));
        {
            /* the right angle's corner, the leg along x, the leg along y */
            const f32b ox = f32b_int(downright ? x0 : x0 + 32), oy = f32b_int(downright ? y0 : y0 + 32);
            const f32b lx = f32b_scaled((int32_t)mx, ex), ly = f32b_scaled((int32_t)my, ey);
            const int sgn = downright ? 1 : -1;
            X[0] = ox; Y[0] = oy;
            X[1] = downright ? sf_add(ox, lx) : sf_sub(ox, lx); Y[1] = oy;
            X[2] = ox; Y[2] = downright ? sf_add(oy, ly) : sf_sub(oy, ly);
            /* U = (x - ox) s / 64 + 0.25, V = (y - oy) s / 64 + 0.25: at the far corners leg * k / 1024 (* sign) */
            U[0] = U[2] = F32B_HALF; V[0] = V[1] = F32B_HALF;
            U[1] = sf_add(F32B_HALF, sf_mul(f32b_scaled(sgn * kk, -10), lx));
            V[2] = sf_add(F32B_HALF, sf_mul(f32b_scaled(sgn * kk, -10), ly));
            V[2] = V[2]; V[1] = F32B_HALF; U[2] = F32B_HALF;
        }
        pcw.ParaType = ParamType_Polygon_or_Modifier_Volume;
        pcw.ListType = ListType_Opaque;
        pcw.Group_En = 1;
        pcw.Strip_Len = 1;
        pcw.Texture = 1;
        pcw.User_Clip = User_Clip_Inside;
        isp.DepthMode = DepthMode_Always;
        isp.CullMode = CullMode_None;
        isp.Texture = 1;
        tsp.SrcInstr = BlendInstr_One;
        tsp.DstInstr = BlendInstr_Zero;
        tsp.FogCtrl = FogCtrl_None;
        tsp.UseAlpha = 1;
        tsp.ShadInstr = ShadInstr_Decal;
        tsp.FilterMode = tri ? FilterMode_TrilinearB : FilterMode_Point;
        tsp.MipMapD = 4;
        tsp.TexU = 3;
        tsp.TexV = 3;
        tcw.PixelFmt = PixelFmt_565;
        tcw.MipMapped = 1;
        tcw.TexAddr = (tri ? tex_white : tex_col) >> 3;
        ta_polygon(pcw, isp, tsp, tcw);
        for(k = 0; k < 3; k++)
            ta_vertex3(X[k], Y[k], f32b_scaled(1, -20), U[k], V[k], 0xffffffff, 0, k == 2);
    }
    ta_list_end();
}
