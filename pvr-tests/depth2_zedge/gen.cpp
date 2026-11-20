/* The cases of pvr-tests/depth2_zedge (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   Random triangles near the origin (tile (0, 0)): coordinates among +-0, +-(1 .. 1.5) 2^-126 and a few more tiny
   ones, +-0.25 .. 32; depths among 0.5 .. 2^127 of both signs, the largest float, +-inf, +-2^-126. Kept: those that
   cover at least 4 pixels and where refsw3 (pvr_isp_depth.h) stores other words at 4 and a quarter of them or more
   without the zero rule in the depth setup's edges (x_j y_k - x_k y_j) and determinant (DepthConfig zeroRuleEdges
   0): an edge of +0 or of +-(1 .. 1.5) 2^-126, times an infinite depth in the constant, or the determinant. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
static uint64_t S = 0x1234567887654321ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }

static int Cover(const uint32_t X[3], const uint32_t Y[3], uint32_t mask[32])
{
    static const pvrisp::EdgeHW cfg;
    Fp FX[3], FY[3];
    for (int k = 0; k < 3; k++) { FX[k] = Fp::FromBits(X[k]); FY[k] = Fp::FromBits(Y[k]); }
    bool bad = false;
    pvrisp::Winding(FX, FY, cfg, &bad);
    memset(mask, 0, 128);
    if (bad) return 0;
    const int sgn = pvrisp::EdgeWinding(FX, FY, cfg);
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 1, 1, sgn, cfg);
    int64_t rf, rl;
    pvrisp::RowRange(FY, 3, &rf, &rl);
    for (int k = 0; k < 3; k++) E[k].SetFirstRow(rf);
    int n = 0;
    for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) if (j >= rf && j <= rl && E[0].Inside(i, j) && E[1].Inside(i, j) && E[2].Inside(i, j)) { n++; mask[j] |= 1u << i; }
    return n;
}

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    const uint32_t T = 0x00800000u;
    const uint32_t vals[] = { 0, 0x80000000u, T, T | 0x200000u, T | 0x3fffffu, T | 0x400000u, 0x80000000u | T, 0x80000000u | T | 0x200000u, 0x80000000u | T | 0x3fffffu,
                              0x3f800000u, 0x3fa00000u, 0x3fc00000u, 0x3fbfffffu, 0x40000000u, 0x40800000u, 0x41000000u, 0x41800000u, 0x42000000u, 0x41f00000u, 0x40a00000u, 0x40400000u,
                              0x00c00000u, 0x01000000u, 0x00900000u, 0x3f000000u, 0x3e800000u, 0x01800000u, 0x3f400000u };
    const int NV = sizeof vals / 4;
    const uint32_t zs[] = { 0x3f800000u, 0x3f000000u, 0x40000000u, 0x7f000000u, 0x7e800000u, 0x7f7fffffu, 0x7effffffu, 0xff000000u, 0xff7fffffu, 0x40400000u,
                            0x7f800000u, 0xff800000u, 0x00800000u, 0x80800000u, 0x5f000000u, 0x00c00000u };
    const int NZ = sizeof zs / 4;
    static const pvrisp::DepthHW hw;
    pvrisp::DepthConfig alt;
    alt.zeroRuleEdges = 0;
    const int NC = 13;          // the 14th (the next random one) hung a console against a background of 2^127 (main.c)
    std::vector<std::vector<uint32_t>> out;
    std::set<std::vector<uint32_t>> seen;
    long tries = 0;
    while ((int)out.size() < NC) {
        uint32_t X[3], Y[3], Z[3];
        for (int k = 0; k < 3; k++) {
            X[k] = vals[rnd() % NV]; Y[k] = vals[rnd() % NV]; Z[k] = zs[rnd() % NZ];
            if (rnd() & 1) X[k] ^= 0x80000000u;
            if (rnd() & 1) Y[k] ^= 0x80000000u;
        }
        tries++;
        uint32_t m[32];
        const int n = Cover(X, Y, m);
        if (n < 4) continue;
        pvrisp::DepthTileT<pvrisp::DepthHW> A;
        pvrisp::DepthTile B;
        A.Setup(X, Y, Z, 0, 0, hw);
        B.Setup(X, Y, Z, 0, 0, alt);
        int d = 0;
        for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) if ((m[j] >> i & 1) && A.At(i, j, true) != B.At(i, j, true)) d++;
        if (d < 4 || 4 * d < n) continue;
        std::vector<uint32_t> v = { X[0], X[1], X[2], Y[0], Y[1], Y[2], Z[0], Z[1], Z[2] };
        if (!seen.insert(v).second) continue;
        out.push_back(v);
        fprintf(stderr, "case %zu: %d of %d pixels differ without the rule, after %ld tries\n", out.size() - 1, d, n, tries);
    }
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: x, y, z of the three corners (float words) */\n#define NCASES %d\nstatic const uint32_t CASES[NCASES][9] = {\n", NC);
    for (auto& v : out) { fprintf(f, "    {"); for (auto u : v) fprintf(f, " 0x%08xu,", u); fprintf(f, " },\n"); }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
