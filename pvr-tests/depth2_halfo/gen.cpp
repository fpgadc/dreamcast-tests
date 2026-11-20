/* The cases of pvr-tests/depth2_halfo (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   Ordinary triangles over tile (0, 0) (corners within 16 pixels of it, on a 1/16 pixel grid, depths 0.5 .. 8, all
   above 0.25 at the targets), HALF_OFFSET 6 (the ISP samples at pixel corners): the target pixels are those where
   the plane's start evaluated at the tile's first pixel corner (pvr_isp_depth.h Setup with centers false) and at its
   centre with half steps back (refsw3 before, Setup with centers true) store different words. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
static uint64_t S = 0x9e3779b97f4a7c15ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
static double ur() { return (double)(rnd() >> 11) / 9007199254740992.0; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

static void Cover(const uint32_t X[3], const uint32_t Y[3], uint32_t mask[32])
{
    static const pvrisp::EdgeHW cfg;
    Fp FX[3], FY[3];
    for (int k = 0; k < 3; k++) { FX[k] = Fp::FromBits(X[k]); FY[k] = Fp::FromBits(Y[k]); }
    memset(mask, 0, 128);
    const int sgn = pvrisp::EdgeWinding(FX, FY, cfg);
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 0, 0, sgn, cfg);     // corners: halves 0
    int64_t rf, rl;
    pvrisp::RowRange(FY, 3, &rf, &rl);
    for (int k = 0; k < 3; k++) E[k].SetFirstRow(rf);
    for (int j = 0; j < 32; j++) {
        if (j < rf || j > rl) continue;
        for (int i = 0; i < 32; i++) if (E[0].Inside(i, j) && E[1].Inside(i, j) && E[2].Inside(i, j)) mask[j] |= 1u << i;
    }
}

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    static const pvrisp::DepthHW cfg;
    const int NTRI = 8, NTGT = 5;
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: triangles (x, y in 1/16 pixel, z words) and their target pixels; the words refsw3 stores at\n"
               "   a target with the start at the corner (new) and at the centre with half steps back (old) */\n");
    fprintf(f, "#define NTRI %d\n#define NTGT %d\nstatic const case_t CASES[NTRI] = {\n", NTRI, NTGT);
    int made = 0;
    while (made < NTRI) {
        int x16[3], y16[3];
        float z[3];
        for (int k = 0; k < 3; k++) { x16[k] = (int)(rnd() % (48 * 16)) - 16 * 16; y16[k] = (int)(rnd() % (48 * 16)) - 16 * 16; z[k] = (float)ldexp(1 + ur(), (int)(rnd() % 4) - 1); }
        uint32_t X[3], Y[3], Z[3];
        for (int k = 0; k < 3; k++) { X[k] = fb(x16[k] / 16.0f); Y[k] = fb(y16[k] / 16.0f); Z[k] = fb(z[k]); }
        uint32_t m[32];
        Cover(X, Y, m);
        pvrisp::DepthTileT<pvrisp::DepthHW> A, B;
        A.Setup(X, Y, Z, 0, 0, cfg, true);
        B.Setup(X, Y, Z, 0, 0, cfg, false);
        if (!A.valid || !A.block) continue;
        std::vector<int> t;
        for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) if (m[j] >> i & 1) {
            const auto a = A.At(i, j, false), b = B.At(i, j, false);
            if (a != b && a.bits >= 0x3e800000u && b.bits >= 0x3e800000u && a.bits < 0x7f000000u && b.bits < 0x7f000000u) t.push_back(j * 32 + i);
        }
        if ((int)t.size() < NTGT) continue;
        fprintf(f, "    { { %d, %d, %d }, { %d, %d, %d }, { 0x%08xu, 0x%08xu, 0x%08xu }, {", x16[0], x16[1], x16[2], y16[0], y16[1], y16[2], Z[0], Z[1], Z[2]);
        std::vector<int> pick;
        for (int n = 0; n < NTGT; n++) pick.push_back(t[(size_t)n * t.size() / NTGT]);
        for (int p : pick) fprintf(f, " { %d, %d, 0x%08xu, 0x%08xu },", p % 32, p / 32, B.At(p % 32, p / 32, false).bits, A.At(p % 32, p / 32, false).bits);
        fprintf(f, " } },\n");
        fprintf(stderr, "triangle %d: %zu pixels differ\n", made, t.size());
        made++;
    }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
