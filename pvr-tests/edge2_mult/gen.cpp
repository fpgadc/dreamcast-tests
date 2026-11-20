// edge2_mult: writes cases.h (one triangle per tile), each kept where two hypotheses of hyp.h differ in its tile.
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
#define BASE "O9=27 O13=3 O18=1 O16=2 "
static const Variant V[] = {
    { "MULT126", BASE "O21=4" },            // 0: the model this test was made for
    { "MULT125", BASE "O21=7" }, { "MULT124", BASE "O21=9" }, { "END", BASE },
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
// the pixels each variant has against the first (FAR131)
static std::string pixels(const uint32_t* x, const uint32_t* y, int tx, int ty)
{
    uint32_t r0[32], r[32]; setOpts(V[0].opts); cover(x, y, tx, ty, r0);
    int n0 = 0; for (int j = 0; j < 32; j++) n0 += __builtin_popcount(r0[j]);
    std::string s = std::string(V[0].name) + " " + std::to_string(n0) + " px;";
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

// one page of up to 300 tiles, the tiles taken by whichever candidate finds them free: each a triangle of mk where a
// pair differs by at least minPx pixels in its tile (pairs taken in turn); the rest of the page empty
static void pageAny(const char* fam, Maker mk, std::vector<std::pair<int, int>> pairs, int minPx, long budget)
{
    first.push_back((int)cases.size());
    std::vector<Case> slot(300); std::vector<int> used(300, 0);
    std::vector<long> perPair(pairs.size(), 0);
    const int pageNo = (int)first.size();
    long tried = 0; int filled = 0;
    while (filled < 300 && tried < budget) {
        const long batch = 60000;
        #pragma omp parallel for schedule(dynamic, 500) num_threads(6)
        for (long k = 0; k < batch; k++) {
            rs = 0x51ed2701ull ^ ((uint64_t)pageNo << 40) ^ (uint64_t)(tried + k) * 0x9e3779b97f4a7c15ull; rnd(); rnd();
            const int t = (int)(rnd() % 300), tx = t % 20, ty = t / 20;
            uint32_t x[3], y[3], ra[32], rb[32]; mk(tx, ty, x, y);
            for (size_t q = 0; q < pairs.size(); q++) {
                setOpts(V[pairs[q].first].opts); cover(x, y, tx, ty, ra);
                setOpts(V[pairs[q].second].opts); cover(x, y, tx, ty, rb);
                if (diffPx(ra, rb) < minPx) continue;
                #pragma omp critical
                {
                    // a pair with fewer cases first: take the tile when free, or when this pair has fewer than the slot's
                    if (!used[t]) {
                        used[t] = 1 + (int)q; filled++; perPair[q]++;
                        Case& c = slot[t]; c.tx = tx; c.ty = ty; memcpy(c.x, x, 12); memcpy(c.y, y, 12);
                        c.label = std::string(fam) + " " + V[pairs[q].first].name + "/" + V[pairs[q].second].name + ": " + fmt(x, y) + "; " + pixels(x, y, tx, ty);
                    }
                }
                break;
            }
        }
        tried += batch;
    }
    for (int t = 0; t < 300; t++) {
        if (!used[t]) {      // nothing found: a triangle outside the tile (the background)
            Case& c = slot[t]; c.tx = t % 20; c.ty = t / 20;
            for (int k = 0; k < 3; k++) { c.x[k] = F(-100.0f - k); c.y[k] = F(-100.0f - 2 * k); }
            c.label = std::string(fam) + " empty";
        }
        cases.push_back(slot[t]);
    }
    fprintf(stderr, "%s: %d of 300 tiles after %ld candidates;", fam, filled, tried);
    for (size_t q = 0; q < pairs.size(); q++) fprintf(stderr, " %s/%s %ld", V[pairs[q].first].name, V[pairs[q].second].name, perPair[q]);
    fprintf(stderr, "\n");
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

// Y: a far corner at y = +-Y exactly (the near corners' y below the adder's reach: a column step of exactly -+Y), its x
// at +-2^124 .. 2^128 (a row step past 2^124), two near corners
static const double* gY; static int gNY;
static void mkY(int tx, int ty, uint32_t* x, uint32_t* y)
{
    const double cx = 32 * tx + 16, cy = 32 * ty + 16;
    uint32_t px[3], py[3];
    py[0] = F((float)(gY[rnd() % gNY] * ((rnd() & 1) ? -1 : 1)));
    px[0] = big(124 + rnd() % 4, rnd() & 1);
    px[1] = near(cx, 64); py[1] = near(cy, 64); px[2] = near(cx, 64); py[2] = near(cy, 64);
    const int o = rnd() % 6; static const int ord[6][3] = { {0,1,2},{1,0,2},{0,2,1},{2,0,1},{1,2,0},{2,1,0} };
    for (int k = 0; k < 3; k++) { x[k] = px[ord[o][k]]; y[k] = py[ord[o][k]]; }
}
int main(int argc, char** argv)
{
    static const double y125[4] = { std::ldexp(1.0, 125), std::ldexp(3.0, 125), std::ldexp(5.0, 125), std::ldexp(7.0, 125) };
    static double y124[8];
    for (int k = 0; k < 8; k++) y124[k] = std::ldexp(2.0 * k + 1, 124);
    const int M = vid("MULT126");
    if (argc > 1 && !strcmp(argv[1], "rate")) {
        gY = y125; gNY = 4; rate("Y", mkY, M, vid("MULT125"));
        gY = y124; gNY = 8; rate("Y", mkY, M, vid("MULT124"));
        return 0;
    }
    gY = y125; gNY = 4;
    pageAny("Y", mkY, { { M, vid("MULT125") } }, 2, 6000000);
    gY = y124; gNY = 8;
    pageAny("Y", mkY, { { M, vid("MULT124") } }, 2, 6000000);
    first.push_back((int)cases.size());
    const int npages = (int)first.size() - 1;
    printf("/* written by pvr-tests/edge2_mult/gen.cpp: one triangle per tile (tile x, tile y, x[3], y[3]) */\n");
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
