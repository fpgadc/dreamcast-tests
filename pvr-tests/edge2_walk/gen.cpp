// edge2_walk: writes cases.h (one triangle per tile), each kept where two hypotheses of hyp.h differ in its tile.
//   g++ -O2 -fopenmp -std=c++17 -I<nulldc3>/refsw3/refcore gen.cpp -o gen && ./gen > cases.h   (./gen rate: hit rates)
#include "hyp.h"
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
using pvrfp::Fp;

static void setOpts(const std::string& s)
{
    optInit();
    for (int i = 0; i < 64; i++) g_opt[i] = 0;
    std::istringstream is(s); std::string t;
    while (is >> t) { int k, v; if (sscanf(t.c_str(), "O%d=%d", &k, &v) == 2) g_opt[k] = v; }
}
static int tileOf(uint32_t b)          // refta TileOf
{
    const int e = b >> 23 & 255; const bool neg = b >> 31;
    if (e == 255) return neg ? -1 : 0x3ffffff;
    if (neg) return -1;
    if (e >= 127 + 31) return 0x3ffffff;
    if (e < 127) return 0;
    const uint64_t m = (b & 0x7fffff) | 0x800000; const int sh = e - 127 - 23;
    const int64_t v = sh >= 0 ? (int64_t)(m << sh) : (int64_t)(m >> -sh);
    return (int)(v >> 5);
}
static bool binned(const uint32_t* X, const uint32_t* Y, int tx, int ty)
{
    int x1 = 1 << 30, x2 = -(1 << 30), y1 = 1 << 30, y2 = -(1 << 30);
    for (int k = 0; k < 3; k++) { int a = tileOf(X[k]), b = tileOf(Y[k]); x1 = std::min(x1, a); x2 = std::max(x2, a); y1 = std::min(y1, b); y2 = std::max(y2, b); }
    x1 = std::max(x1, 0); x2 = std::min(x2, 19); y1 = std::max(y1, 0); y2 = std::min(y2, 14);
    if (x2 < 0 || x1 >= 20 || y2 < 0 || y1 >= 15) return false;
    return tx >= x1 && tx <= x2 && ty >= y1 && ty <= y2;
}
// refsw_tile.cpp RasterizeTriangle's coverage (opaque, HALF_OFFSET 7, no culling)
static void cover(const uint32_t* X, const uint32_t* Y, int tx, int ty, uint32_t rows[32])
{
    static const pvrisp::EdgeHW cfg;
    for (int j = 0; j < 32; j++) rows[j] = 0;
    if (!binned(X, Y, tx, ty)) return;
    const Fp FX[4] = { Fp::FromBits(X[0]), Fp::FromBits(X[1]), Fp::FromBits(X[2]), Fp() };
    const Fp FY[4] = { Fp::FromBits(Y[0]), Fp::FromBits(Y[1]), Fp::FromBits(Y[2]), Fp() };
    bool bad = false;
    pvrisp::Winding(FX, FY, cfg, &bad);
    const int sgn = pvrisp::EdgeWinding(FX, FY, cfg);
    if (bad) return;
    const int left = 32 * tx, top = 32 * ty;
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 2 * left + 1, 2 * top + 1, sgn, cfg);
    int64_t rowFirst, rowLast;
    pvrisp::RowRange(FY, 3, &rowFirst, &rowLast);
    const bool anyBig = E[0].big || E[1].big || E[2].big;
    if (rowFirst - top == 31 && (anyBig || OPT(13) == 3)) rowFirst--;
    for (auto& e : E) e.SetFirstRow(rowFirst - top);
    for (int y = 0; y < 32; y++) {
        if (top + y < rowFirst || top + y > rowLast) continue;
        for (int x = 0; x < 32; x++)
            if (E[0].Inside(x, y) && E[1].Inside(x, y) && E[2].Inside(x, y)) rows[y] |= 1u << x;
    }
}
static int diffPx(const uint32_t* a, const uint32_t* b) { int n = 0; for (int j = 0; j < 32; j++) n += __builtin_popcount(a[j] ^ b[j]); return n; }

// ---- numbers ----
static thread_local uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
static double uni() { return (rnd() & 0xffffff) / 16777216.0; }
static uint32_t F(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
static float B(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
// r 2^e with a random 23 bit fraction, r in [1, 2)
static uint32_t big(int e, bool neg) { return (neg ? 0x80000000u : 0) | (uint32_t)(e + 127) << 23 | (rnd() & 0x7fffff); }
static uint32_t near(double c, double spread) { return F((float)(c + (uni() * 2 - 1) * spread)); }

struct Variant { const char* name; const char* opts; };
static const Variant V[] = {
    { "U27", "O9=27" },                     // 0: the model this test was made for
    { "U26", "O9=26" }, { "U28", "O9=28" }, { "EP25", "O14=25" }, { "EP26", "O14=26" }, { "EP27", "O14=27" },
    { "FLOOR", "O9=27 O15=1" }, { "OLD", "" },
    { "SATNOEND", "O9=27 O11=2" }, { "SATNOEND0", "O9=27 O11=5" }, { "SATNOENDW", "O9=27 O11=6" }, { "EXNOEND", "O9=27 O6=5" },
    { "ROW31ALL", "O9=27 O13=3" },
};
static const int NV = sizeof V / sizeof V[0];
static int vid(const char* n) { for (int i = 0; i < NV; i++) if (!strcmp(V[i].name, n)) return i; return -1; }

struct Case { int tx, ty; uint32_t x[3], y[3]; std::string label; };
static std::vector<Case> cases;
static std::vector<int> first;

static std::string fmt(const uint32_t* x, const uint32_t* y)
{
    char b[256]; snprintf(b, sizeof b, "(%.7g, %.7g) (%.7g, %.7g) (%.7g, %.7g)", B(x[0]), B(y[0]), B(x[1]), B(y[1]), B(x[2]), B(y[2]));
    return b;
}
// the pixels each variant has against the first (U27)
static std::string pixels(const uint32_t* x, const uint32_t* y, int tx, int ty)
{
    uint32_t r0[32], r[32]; setOpts(V[0].opts); cover(x, y, tx, ty, r0);
    int n0 = 0; for (int j = 0; j < 32; j++) n0 += __builtin_popcount(r0[j]);
    std::string s = "U27 " + std::to_string(n0) + " px;";
    for (int v = 1; v < NV; v++) { setOpts(V[v].opts); cover(x, y, tx, ty, r); int d = diffPx(r0, r); if (d) s += std::string(" ") + V[v].name + ":" + std::to_string(d); }
    return s;
}

typedef void (*Maker)(int tx, int ty, uint32_t* x, uint32_t* y);
// one page of 300 tiles: each tile a triangle of mk where variants a and b differ by at least minPx pixels
static void page(const char* fam, Maker mk, std::vector<std::pair<int, int>> pairs, int minPx = 3, long tries = 2000000)
{
    first.push_back((int)cases.size());
    std::vector<Case> pc(300);
    std::vector<int> okv(300, 0), missv(300, 0);
    const int pageNo = (int)first.size();
    #pragma omp parallel for schedule(dynamic) num_threads(6)
    for (int t = 0; t < 300; t++) {
        rs = 0x9e3779b97f4a7c15ull ^ ((uint64_t)pageNo << 32) ^ (uint64_t)(t * 7919 + 1);
        for (int w = 0; w < 4; w++) rnd();
        const int tx = t % 20, ty = t / 20;
        auto pr = pairs[t % pairs.size()];
        uint32_t x[3], y[3]; bool ok = false;
        for (long k = 0; k < tries && !ok; k++) {
            mk(tx, ty, x, y);
            uint32_t ra[32], rb[32];
            setOpts(V[pr.first].opts); cover(x, y, tx, ty, ra);
            setOpts(V[pr.second].opts); cover(x, y, tx, ty, rb);
            ok = diffPx(ra, rb) >= minPx;
        }
        if (!ok) {      // the pair has no case here: any of the pairs
            missv[t] = 1;
            for (long k = 0; k < tries / 8 && !ok; k++) {
                mk(tx, ty, x, y);
                uint32_t ra[32], rb[32];
                for (auto& p2 : pairs) {
                    setOpts(V[p2.first].opts); cover(x, y, tx, ty, ra);
                    setOpts(V[p2.second].opts); cover(x, y, tx, ty, rb);
                    if (diffPx(ra, rb) >= minPx) { ok = true; pr = p2; break; }
                }
            }
        }
        if (!ok) mk(tx, ty, x, y);
        okv[t] = ok;
        Case& c = pc[t]; c.tx = tx; c.ty = ty; memcpy(c.x, x, 12); memcpy(c.y, y, 12);
        c.label = std::string(fam) + " " + V[pr.first].name + "/" + V[pr.second].name + ": " + fmt(x, y) + "; " + pixels(x, y, tx, ty);
    }
    int got = 0, miss = 0;
    for (int t = 0; t < 300; t++) { got += okv[t]; miss += missv[t]; cases.push_back(pc[t]); }
    fprintf(stderr, "%s: %d of 300 tiles separate a pair (%d not the tile's own pair)\n", fam, got, miss);
}

// how often a family separates a pair: argv "rate"
static void rate(const char* fam, Maker mk, int a, int b, int n = 200000)
{
    long hit = 0;
    #pragma omp parallel for reduction(+:hit) num_threads(6)
    for (int k = 0; k < n; k++) {
        rs = 0x1234567ull + (uint64_t)k * 0x9e3779b97f4a7c15ull; rnd(); rnd();
        const int tx = rnd() % 20, ty = rnd() % 15;
        uint32_t x[3], y[3], ra[32], rb[32]; mk(tx, ty, x, y);
        setOpts(V[a].opts); cover(x, y, tx, ty, ra);
        setOpts(V[b].opts); cover(x, y, tx, ty, rb);
        if (diffPx(ra, rb) >= 2) hit++;
    }
    fprintf(stderr, "%s %s/%s: %ld of %d\n", fam, V[a].name, V[b].name, hit, n);
}

// S: two far corners on opposite sides in y (|dy| from 2^128: a saturated column step) and a near one
static void mkSat(int tx, int ty, uint32_t* x, uint32_t* y)
{
    const double cx = 32 * tx + 16, cy = 32 * ty + 16;
    uint32_t px[3], py[3];
    const bool up = rnd() & 1;
    py[0] = big(127, !up); py[1] = big(127, up);                          // 2^127 .. 2^128 each: |dy| >= 2^128
    for (int k = 0; k < 2; k++) {
        const int m = rnd() % 3;
        px[k] = m == 0 ? near(cx, 400) : big(118 + rnd() % 10, rnd() & 1);
    }
    px[2] = near(cx, 80); py[2] = near(cy, 80);
    const int o = rnd() % 6; static const int ord[6][3] = { {0,1,2},{1,0,2},{0,2,1},{2,0,1},{1,2,0},{2,1,0} };
    for (int k = 0; k < 3; k++) { x[k] = px[ord[o][k]]; y[k] = py[ord[o][k]]; }
}
// E: a far corner at y = +-2^127 exactly (a column step of exactly 2^127, not saturated) and two near ones
static void mkExact(int tx, int ty, uint32_t* x, uint32_t* y)
{
    const double cx = 32 * tx + 16, cy = 32 * ty + 16;
    uint32_t px[3], py[3];
    py[0] = (rnd() & 1 ? 0x80000000u : 0) | 0x7f000000u;
    const int m = rnd() % 3;
    px[0] = m == 0 ? near(cx, 400) : big(118 + rnd() % 10, rnd() & 1);
    px[1] = near(cx, 64); py[1] = near(cy, 64); px[2] = near(cx, 64); py[2] = near(cy, 64);
    const int o = rnd() % 6; static const int ord[6][3] = { {0,1,2},{1,0,2},{0,2,1},{2,0,1},{1,2,0},{2,1,0} };
    for (int k = 0; k < 3; k++) { x[k] = px[ord[o][k]]; y[k] = py[ord[o][k]]; }
}
// V: one or two corners at 2^121 .. 2^128 (either coordinate, either sign), the others near the tile
static void mkV(int tx, int ty, uint32_t* x, uint32_t* y)
{
    const double cx = 32 * tx + 16, cy = 32 * ty + 16;
    uint32_t px[3], py[3];
    const int nfar = 1 + (rnd() & 1);
    for (int k = 0; k < 3; k++) {
        px[k] = near(cx, 96); py[k] = near(cy, 96);
        if (k < nfar) {
            const int w = rnd() % 3;
            if (w != 1) px[k] = big(121 + rnd() % 7, rnd() & 1);
            if (w != 0) py[k] = big(121 + rnd() % 7, rnd() & 1);
        }
    }
    const int o = rnd() % 6; static const int ord[6][3] = { {0,1,2},{1,0,2},{0,2,1},{2,0,1},{1,2,0},{2,1,0} };
    for (int k = 0; k < 3; k++) { x[k] = px[ord[o][k]]; y[k] = py[ord[o][k]]; }
}

// H: a near-horizontal edge from a near corner to a far one at x = +-2^124 .. 2^128 (a row step past 2^124: E cut)
// whose column step 2^(e - 20) .. 2^(e - 3) has bits below the units, the near corner just off a row of sample points
static void mkH(int tx, int ty, uint32_t* x, uint32_t* y)
{
    const double cx = 32 * tx + 16, cy = 32 * ty;
    uint32_t px[3], py[3];
    const int e1 = 124 + rnd() % 4, e2 = e1 - 3 - rnd() % 18;
    const int j0 = (rnd() & 1) ? 0 : rnd() % 32;            // row 0: the start itself is near zero
    const double d = (uni() * 2 - 1) * std::ldexp(1.0, e2 - e1 + 5);
    px[0] = near(cx, 24); py[0] = F((float)(cy + j0 + 0.5 + d));
    px[1] = big(e1, rnd() & 1); py[1] = big(e2, rnd() & 1);
    if (rnd() & 1) { px[2] = near(cx, 96); py[2] = near(cy + 16, 96); }
    else { px[2] = big(e1 - rnd() % 3, rnd() & 1); py[2] = big(e2 - 2 + rnd() % 5, rnd() & 1); }
    const int o = rnd() % 6; static const int ord[6][3] = { {0,1,2},{1,0,2},{0,2,1},{2,0,1},{1,2,0},{2,1,0} };
    for (int k = 0; k < 3; k++) { x[k] = px[ord[o][k]]; y[k] = py[ord[o][k]]; }
}

int main(int argc, char** argv)
{
    const int U27 = 0;
    if (argc > 1 && !strcmp(argv[1], "rate")) {
        rate("S", mkSat, U27, vid("SATNOEND")); rate("S", mkSat, vid("SATNOEND"), vid("SATNOEND0")); rate("S", mkSat, vid("SATNOEND"), vid("SATNOENDW"));
        rate("E", mkExact, U27, vid("EXNOEND"));
        for (const char* n : { "U26", "U28", "OLD", "EP25", "EP26", "EP27", "FLOOR" }) rate("H", mkH, U27, vid(n));
        return 0;
    }
    // pages 0-2: a saturated column step and the end of the edge
    page("S", mkSat, { { U27, vid("SATNOEND") } }, 3, 300000);
    page("S", mkSat, { { vid("SATNOEND"), vid("SATNOEND0") }, { vid("SATNOEND"), vid("SATNOENDW") }, { U27, vid("SATNOEND") } }, 3, 300000);
    // page 2: a column step of exactly 2^127, not saturated
    page("E", mkExact, { { U27, vid("EXNOEND") } }, 2, 60000);
    // pages 3-5: the units of an edge whose E was cut
    page("H", mkH, { { U27, vid("U26") }, { U27, vid("U28") }, { U27, vid("OLD") } }, 2, 300000);
    page("H", mkH, { { U27, vid("EP25") }, { U27, vid("EP26") }, { U27, vid("EP27") } }, 2, 300000);
    page("H", mkH, { { U27, vid("FLOOR") }, { U27, vid("U28") }, { U27, vid("U26") }, { U27, vid("EP27") } }, 2, 300000);
    // page 6: points (three equal corners: every edge inside) at rows 28 .. 31 of their tile, and the same
    // with the corners a 1/1024 pixel apart: an ordinary polygon whose first row is the tile's last
    first.push_back((int)cases.size());
    static const double fr[5] = { 0, 0.25, 0.5, 0.75, 0.9990234375 };
    for (int t = 0; t < 300; t++) {
        const int tx = t % 20, ty = t / 20, r = 28 + (t % 4), f = (t / 4) % 5, kind = (t / 20) % 3;
        const double X = 32 * tx + 3.25 + (t % 7) * 4, Y = 32 * ty + r + fr[f];
        Case c; c.tx = tx; c.ty = ty;
        for (int k = 0; k < 3; k++) { c.x[k] = F((float)X); c.y[k] = F((float)Y); }
        if (kind == 1) { c.x[1] = F((float)(X + 1.0 / 1024)); c.y[2] = F((float)(Y + 1.0 / 1024)); }
        if (kind == 2) { c.x[1] = F((float)(X + 6)); c.y[2] = F((float)(Y + 0.5)); }      // a small ordinary triangle
        c.label = std::string("P ") + (kind == 0 ? "point" : kind == 1 ? "tiny" : "small") + " row " + std::to_string(r) + " +" + std::to_string(fr[f]) + ": " + fmt(c.x, c.y) + "; " + pixels(c.x, c.y, tx, ty);
        cases.push_back(c);
    }
    first.push_back((int)cases.size());
    const int npages = (int)first.size() - 1;
    printf("/* written by pvr-tests/edge2_walk/gen.cpp: one triangle per tile (tile x, tile y, x[3], y[3]) */\n");
    printf("#define FC_PAGES %d\n", npages);
    printf("typedef struct { unsigned char tx, ty; uint32_t x[3], y[3]; } fc_case_t;\n");
    printf("static const fc_case_t fc_cases[%zu] = {\n", cases.size());
    for (auto& c : cases) printf("  { %d, %d, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu } },\n", c.tx, c.ty, c.x[0], c.x[1], c.x[2], c.y[0], c.y[1], c.y[2]);
    printf("};\nstatic const int fc_first[%d] = {", npages + 1);
    for (int p = 0; p <= npages; p++) printf(" %d%s", first[p], p < npages ? "," : " };\n");
    printf("#ifndef _arch_dreamcast\nstatic const char *const fc_label[%zu] = {\n", cases.size());
    for (auto& c : cases) printf("  \"%s\",\n", c.label.c_str());
    printf("};\n#endif\n");
}
