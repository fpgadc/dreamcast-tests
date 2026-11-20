/* The cases of pvr-tests/depth2_zcoord (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   Random triangles near the origin (tile (0, 0)): one coordinate of two corners among +-0 and +-(1 .. 1.5) 2^-126
   (x or y), the rest whole pixels 0 .. 39, depths among 0.5 .. 2^127 of both signs, the largest float, +-inf. Kept:
   those where a coordinate difference of the depth setup meets the zero rule (a zero minus a number of 2^-126 .. 1.5
   2^-126 of the same sign, or plus one of the other: +0) where it multiplies a depth of 2^126 or more or an infinity,
   that cover at least 16 pixels, and where refsw3 (pvr_isp_depth.h) stores other words at 8 and an eighth of them
   or more without the rule there (DepthConfig zeroRuleCoords 0); half of them with an infinite corner. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
static uint64_t S = 88172645463325252ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static int Cover(const uint32_t X[3], const uint32_t Y[3], uint32_t mask[32])
{
    static const pvrisp::EdgeHW cfg;
    Fp FX[3], FY[3];
    for (int k = 0; k < 3; k++) { FX[k] = Fp::FromBits(X[k]); FY[k] = Fp::FromBits(Y[k]); }
    bool bad = false;
    pvrisp::Winding(FX, FY, cfg, &bad);
    if (bad) return 0;
    const int sgn = pvrisp::EdgeWinding(FX, FY, cfg);
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 1, 1, sgn, cfg);
    int64_t rf, rl;
    pvrisp::RowRange(FY, 3, &rf, &rl);
    for (int k = 0; k < 3; k++) E[k].SetFirstRow(rf);
    int n = 0;
    memset(mask, 0, 128);
    for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) if (j >= rf && j <= rl && E[0].Inside(i, j) && E[1].Inside(i, j) && E[2].Inside(i, j)) { n++; mask[j] |= 1u << i; }
    return n;
}
// does p - q meet the zero rule (one of them zero, the other of 2^-126 .. 1.5 2^-126, the zero's sign not the result's)
static bool Rule(uint32_t p, uint32_t q)
{
    auto zero = [](uint32_t u) { return (u & 0x7f800000u) == 0; };
    auto small = [](uint32_t u) { return ((u >> 23) & 0xff) == 1 && !(u & 0x400000u); };
    if (zero(p) && small(q)) return (p >> 31) == (q >> 31);        // p - q: -q's sign is not p's
    if (zero(q) && small(p)) return (q >> 31) == (p >> 31);        // p + (-q): -q's sign not p's
    return false;
}

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    const uint32_t T = 0x00800000u;
    const uint32_t tiny[] = { 0, 0x80000000u, T, T | 0x200000u, T | 0x3fffffu, 0x80000000u | T, 0x80000000u | T | 0x200000u, 0x80000000u | T | 0x3fffffu };
    const uint32_t zfin[] = { 0x3f800000u, 0x3f000000u, 0x40000000u, 0x7f000000u, 0x7e800000u, 0x7f7fffffu, 0x7effffffu, 0xff000000u, 0xff7fffffu, 0x40400000u, 0x3fc00000u };
    const uint32_t zinf[] = { 0x7f800000u, 0xff800000u };
    static const pvrisp::DepthHW cfg;
    const int NC = 16;
    int made[2] = { 0, 0 };
    std::vector<std::vector<uint32_t>> out;
    std::set<std::vector<uint32_t>> seen;
    long tries = 0;
    while (made[0] + made[1] < NC) {
        const bool swapxy = rnd() & 1, withInf = (made[1] < NC / 2) && (rnd() & 1);
        if (!withInf && made[0] >= NC / 2) continue;
        uint32_t a[3], b[3], X[3], Y[3], Z[3];
        for (int k = 0; k < 3; k++) { a[k] = fb((float)(rnd() % 40)); b[k] = (rnd() % 3) ? tiny[rnd() % 8] : fb((float)(rnd() % 40)); }
        for (int k = 0; k < 3; k++) { X[k] = swapxy ? b[k] : a[k]; Y[k] = swapxy ? a[k] : b[k]; Z[k] = zfin[rnd() % 11]; }
        tries++;
        if (withInf) Z[rnd() % 3] = zinf[rnd() % 2];
        // the difference of corners k + 1, k + 2 multiplies corner k's depth (a = sum z_k (y_k+1 - y_k+2), b = sum z_k
        // (x_k+2 - x_k+1)): that depth an infinity or of 2^126 and more, so that the product is a number or not
        const uint32_t* C = swapxy ? X : Y;
        bool hit = false;
        for (int k = 0; k < 3; k++) {
            const uint32_t p = C[(k + 1) % 3], q = C[(k + 2) % 3];
            if ((Rule(p, q) || Rule(q, p)) && ((Z[k] >> 23) & 0xff) >= 253) hit = true;
        }
        if (!hit) continue;
        { uint32_t m0[32]; if (Cover(X, Y, m0) < 16) continue; }
        // the depths with the rule (refsw3) and without it in the coordinate differences: kept when they differ at
        // 8 pixels and an eighth of them or more
        pvrisp::DepthTileT<pvrisp::DepthHW> D;
        D.Setup(X, Y, Z, 0, 0, cfg);
        pvrisp::DepthConfig alt;
        alt.zeroRuleCoords = 0;
        pvrisp::DepthTile Dn;
        Dn.Setup(X, Y, Z, 0, 0, alt);
        uint32_t m[32];
        const int n = Cover(X, Y, m);
        int d = 0;
        for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) if ((m[j] >> i & 1) && D.At(i, j, true) != Dn.At(i, j, true)) d++;
        if (d < 8 || 8 * d < n) continue;
        std::vector<uint32_t> v = { X[0], X[1], X[2], Y[0], Y[1], Y[2], Z[0], Z[1], Z[2] };
        if (!seen.insert(v).second) continue;
        out.push_back(v);
        made[withInf]++;
        fprintf(stderr, "case %d (%s): %d of %d pixels differ without the rule, after %ld tries\n", made[0] + made[1] - 1, withInf ? "infinite corner" : "finite", d, n, tries);
    }
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: x, y, z of the three corners (float words) */\n#define NCASES %d\nstatic const uint32_t CASES[NCASES][9] = {\n", NC);
    for (auto& v : out) { fprintf(f, "    {"); for (auto u : v) fprintf(f, " 0x%08xu,", u); fprintf(f, " },\n"); }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
