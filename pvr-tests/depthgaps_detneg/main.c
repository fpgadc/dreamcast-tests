/* The stored depth of a triangle whose determinant is zero, below zero (2026-11-20, hunt depthgaps round 2).

   covmeas (README): every pixel of a triangle of determinant zero gets the mean of its three corners' depths;
   slivers_detzero: the TSP's 1/w there is ((z1 + z2) + z3) * 0x3eaaaaab. refsw3 stored the ISP's mean as a float
   word (StoredDepth::FromFp), where a depth below zero from the depth unit is stored on its block's grid
   (StoredDepth::Neg), and a triangle of determinant zero with an infinite or NaN corner as its first corner's depth,
   all 23 fraction bits. Here 32 such triangles, three corners at one point each (at (16.25, row + 1/4): the ISP
   draws the whole row of the tile; not row 31: one at y = 31.25 draws rows 30 and 31 on a console, which refsw3 does
   not have, a question of coverage), row r with the depths T[r], read out over tile (0, 0) by
   depthgaps_search/search.h (cell 0); cell 1 the same with T2[r]: means below zero with low bits under the 0x100
   grid of the word (stored as a flat polygon's start of 20 bits, its low 4 bits cut before the magnitude is rounded
   up, or not). */
#define NUM_PAGES 1
#define TEST_DRAW_PAGE
#include "pvrtest2.h"
#include "texgen.h"
#include "../depthgaps_search/search.h"

static const uint32_t T[32][3] = {
    { 0x42c80000u, 0x42c80000u, 0x42c80000u },     /* 100 */
    { 0xc2c80000u, 0xc2c80000u, 0xc2c80000u },     /* -100 */
    { 0x3fc00000u, 0x3fc00000u, 0x3fc00000u },     /* 1.5 */
    { 0xbfc00000u, 0xbfc00000u, 0xbfc00000u },     /* -1.5 */
    { 0xc2c80400u, 0xc2c80400u, 0xc2c80400u },     /* -100.0078125 */
    { 0xc2c80020u, 0xc2c80020u, 0xc2c80020u },     /* -(100 + 2^-12) */
    { 0x42c80020u, 0x42c80020u, 0x42c80020u },     /* 100 + 2^-12 */
    { 0xc2800020u, 0xc2800020u, 0xc2800020u },     /* -64 (1 + 2^-18) */
    { 0xa6a00000u, 0xa6a00000u, 0xa6a00000u },     /* -1.25 2^-50 */
    { 0xecb00000u, 0xecb00000u, 0xecb00000u },     /* -1.375 2^90 */
    { 0xbf400000u, 0xbf400000u, 0xbf400000u },     /* -0.75 */
    { 0xba800020u, 0xba800020u, 0xba800020u },     /* -2^-10 (1 + 2^-18) */
    { 0xbf800000u, 0xc0000000u, 0xc0800000u },     /* -1, -2, -4 */
    { 0xc2800000u, 0xc2800000u, 0xc2820000u },     /* -64, -64, -65 */
    { 0xc47a0000u, 0x43fa0000u, 0x42c80000u },     /* -1000, 500, 100 */
    { 0x3f800000u, 0x40000000u, 0x40800000u },     /* 1, 2, 4 */
    { 0xbdcccccdu, 0xbe4ccccdu, 0xbe99999au },     /* -0.1, -0.2, -0.3 */
    { 0xc0400000u, 0x00000000u, 0x00000000u },     /* -3, 0, 0 */
    { 0xc3960000u, 0xc3960000u, 0xc3964000u },     /* -300, -300, -300.5 */
    { 0x0d800000u, 0x8e000000u, 0x00000000u },     /* 2^-100, -2^-99, 0 */
    { 0xbf800000u, 0x3f800000u, 0x00000000u },     /* -1, 1, 0 */
    { 0xc2c80000u, 0xc2c80000u, 0xc2c80400u },     /* -100, -100, -100.0078125 */
    { 0xc2c83efau, 0x7f800000u, 0x7f800000u },     /* -100.123, +inf, +inf */
    { 0xbf800001u, 0x7fc00000u, 0x00000000u },     /* -(1 + 2^-23), NaN, 0 */
    { 0x3f9e0419u, 0x7f800000u, 0x00000000u },     /* 1.2345, +inf, 0 */
    { 0xc2800001u, 0xff800000u, 0x00000000u },     /* -64 (1 + 2^-23), -inf, 0 */
    { 0x42c83efau, 0x42c80000u, 0x7f800000u },     /* 100.123, 100, +inf */
    { 0xc0a00000u, 0xc0a00000u, 0xc0a00000u },     /* -5 */
    { 0xf1c00000u, 0xf1c00000u, 0xf1c00000u },     /* -1.5 2^100 */
    { 0xf1800000u, 0xf1800000u, 0xf2000000u },     /* -2^100, -2^100, -2^101 */
    { 0x8d802000u, 0x8d802000u, 0x8d802000u },     /* -2^-100 (1 + 2^-10) */
    { 0xc2c83efau, 0xc2c83efau, 0xc2c83efau },     /* -100.123 */
};

static const uint32_t T2[32][3] = {
    { 0xc2c80001u, 0xc2c80001u, 0xc2c80001u },     /* -100 - 0x1 ulp */
    { 0xc2c80004u, 0xc2c80004u, 0xc2c80004u },     /* -100 - 0x4 ulp */
    { 0xc2c80008u, 0xc2c80008u, 0xc2c80008u },     /* -100 - 0x8 ulp */
    { 0xc2c8000cu, 0xc2c8000cu, 0xc2c8000cu },     /* -100 - 0xc ulp */
    { 0xc2c8000fu, 0xc2c8000fu, 0xc2c8000fu },     /* -100 - 0xf ulp */
    { 0xc2c80010u, 0xc2c80010u, 0xc2c80010u },     /* -100 - 0x10 ulp */
    { 0xc2c80011u, 0xc2c80011u, 0xc2c80011u },     /* -100 - 0x11 ulp */
    { 0xc2c80018u, 0xc2c80018u, 0xc2c80018u },     /* -100 - 0x18 ulp */
    { 0xc2c8001fu, 0xc2c8001fu, 0xc2c8001fu },     /* -100 - 0x1f ulp */
    { 0xc2c80020u, 0xc2c80020u, 0xc2c80020u },     /* -100 - 0x20 ulp */
    { 0xc2c800f0u, 0xc2c800f0u, 0xc2c800f0u },     /* -100 - 0xf0 ulp */
    { 0xc2c800ffu, 0xc2c800ffu, 0xc2c800ffu },     /* -100 - 0xff ulp */
    { 0xc2c8010fu, 0xc2c8010fu, 0xc2c8010fu },     /* -100 - 0x10f ulp */
    { 0xc2c80110u, 0xc2c80110u, 0xc2c80110u },     /* -100 - 0x110 ulp */
    { 0xbf9e0408u, 0xbf9e0408u, 0xbf9e0408u },     /* 0xbf9e0400 + 0x8 */
    { 0xbf9e040fu, 0xbf9e040fu, 0xbf9e040fu },     /* 0xbf9e0400 + 0xf */
    { 0xbf9e0410u, 0xbf9e0410u, 0xbf9e0410u },     /* 0xbf9e0400 + 0x10 */
    { 0xbf9e041fu, 0xbf9e041fu, 0xbf9e041fu },     /* 0xbf9e0400 + 0x1f */
    { 0x42c80008u, 0x42c80008u, 0x42c80008u },     /* 100 + 0x8 ulp */
    { 0x42c8001fu, 0x42c8001fu, 0x42c8001fu },     /* 100 + 0x1f ulp */
    { 0x42c80020u, 0x42c80020u, 0x42c80020u },     /* 100 + 0x20 ulp */
    { 0xf1800008u, 0xf1800008u, 0xf1800008u },     /* -2^100 - 8 ulp */
    { 0x8d800008u, 0x8d800008u, 0x8d800008u },     /* -2^-100 - 8 ulp */
    { 0xc2800008u, 0xc2800008u, 0xc2800008u },     /* -64 - 8 ulp */
    { 0xc2fffff8u, 0xc2fffff8u, 0xc2fffff8u },     /* 0xc2fffff8 */
    { 0xc2fffff0u, 0xc2fffff0u, 0xc2fffff0u },     /* 0xc2fffff0 */
    { 0xbf400008u, 0xbf400008u, 0xbf400008u },     /* -0.75 - 8 ulp */
    { 0xbf800000u, 0xbf800000u, 0xbf800018u },     /* -1, -1, -1 - 24 ulp */
    { 0xc2c80000u, 0xc2c80000u, 0xc2c80018u },     /* -100, -100, -100 - 24 ulp */
    { 0xc2c80010u, 0xc2c80000u, 0xc2c80000u },     /* -100 - 16 ulp, -100, -100 */
    { 0xc2c80000u, 0xc2c80020u, 0xc2c80000u },     /* -100, -100 - 32 ulp, -100 */
    { 0x42c80000u, 0x42c80000u, 0x42c80018u },     /* 100, 100, 100 + 24 ulp */
};
static int cur;

static void test_setup(void) { srch_setup(); }

static void test_describe(void)
{
    label(0, 0, "32 triangles of determinant zero, one per row of tile (0, 0): stored depths read out (T)");
    label(0, 1, "32 more (T2: the low bits of the mean below zero): stored depths read out");
}

static void points(void)
{
    int r, k;
    for(r = 0; r < 31; r++) {             /* row 31: none (a point at y = 31.25 draws rows 30 and 31 on a console) */
        srch_begin(DepthMode_Always, srch_texG);
        for(k = 0; k < 3; k++)
            srch_vtx(f32b_scaled(65, -2), f32b_scaled(4 * r + 1, -2), cur ? T2[r][k] : T[r][k], k == 2);
    }
}

static void test_page(int page) { (void)page; }

static void test_draw_page(int page)
{
    (void)page;
    for(cur = 0; cur < 2; cur++) {
        srch_run(points, 0);
        srch_keep(cur);
    }
    srch_store(2, 0);
}
