/* The cases of pvr-tests/depth2_biggrid (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   Pairs of small triangles in tile (0, 0), V0 = (X, Y) at 0, V1 = (X + 4, Y) at z1, V2 = (X, Y + 4) at z2: the
   constant overflows (-(z1 X + z2 Y) 4), the tile's start is -2^127, the pixels -2^127 + i A + j B with A = z1 / 4,
   B = z2 / 4 (depthgaps_bigpos, _bigord). P and Q differ a little in z1, z2. The pairs are chosen, from random
   ones, so that the grids a store of the depths of 2^128 and more could have (n = 12 .. 26 significant bits, cut or
   rounded; a fixed grid of 2^102 .. 2^118) give different pictures; the depths are refsw3's (pvr_isp_depth.h). */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
typedef pvrfp::i128 i128;
static uint64_t S = 0x9e3779b97f4a7c15ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
static int64_t irange(int64_t lo, int64_t hi) { return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1)); }
static uint32_t fint(int v) { float f = (float)v; uint32_t u; memcpy(&u, &f, 4); return u; }

struct Tri { int X, Y; uint32_t z1, z2; };
struct Pix { bool in; i128 v; pvrisp::StoredDepth d; };      // v: units of 2^100

static void Cover(const Tri& t, uint32_t mask[32])
{
    static const pvrisp::EdgeHW cfg;
    const Fp FX[3] = { Fp::FromBits(fint(t.X)), Fp::FromBits(fint(t.X + 4)), Fp::FromBits(fint(t.X)) };
    const Fp FY[3] = { Fp::FromBits(fint(t.Y)), Fp::FromBits(fint(t.Y)), Fp::FromBits(fint(t.Y + 4)) };
    memset(mask, 0, 128);
    const int sgn = pvrisp::EdgeWinding(FX, FY, cfg);
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 1, 1, sgn, cfg);
    int64_t rf, rl;
    pvrisp::RowRange(FY, 3, &rf, &rl);
    for (int k = 0; k < 3; k++) E[k].SetFirstRow(rf);
    for (int j = 0; j < 32; j++) {
        if (j < rf || j > rl) continue;
        for (int i = 0; i < 32; i++) if (E[0].Inside(i, j) && E[1].Inside(i, j) && E[2].Inside(i, j)) mask[j] |= 1u << i;
    }
}
static bool Eval(const Tri& t, Pix out[32][32])
{
    uint32_t x[3] = { fint(t.X), fint(t.X + 4), fint(t.X) }, y[3] = { fint(t.Y), fint(t.Y), fint(t.Y + 4) }, z[3] = { 0, t.z1, t.z2 };
    static const pvrisp::DepthHW cfg;
    pvrisp::DepthTileT<pvrisp::DepthHW> Z;
    Z.Setup(x, y, z, 0, 0, cfg);
    if (!Z.valid || !Z.block || Z.unit - 1 < 100) return false;
    uint32_t m[32];
    Cover(t, m);
    for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
        Pix& p = out[j][i];
        p.in = m[j] >> i & 1;
        if (!p.in) continue;
        p.v = (Z.d0 + (2 * i) * Z.dx + (2 * j) * Z.dy) << (Z.unit - 1 - 100);
        p.d = Z.At(i, j, true);
    }
    return true;
}
static int Bits(i128 v) { int b = 0; while (v) { b++; v >>= 1; } return b; }
// hypotheses for a depth of 2^128 or more: 0 the model, 1 + (n - 12): n bits cut, 16 + (n - 12): n bits rounded
// (n = 12 .. 26), 31 + (g - 102): a fixed grid of 2^g cut (g = 102 .. 118)
static const int NH = 48;
static i128 Key(const Pix& p, int h)
{
    const i128 big = (i128)1 << 28;                     // 2^128 in units of 2^100
    if (h == 0 || p.v < big) return (i128)p.d.k >> 32;      // below 2^128: the float's word (above it every Big)
    i128 q;
    if (h < 16) { const int n = 12 + (h - 1), s = Bits(p.v) - n; q = s > 0 ? (p.v >> s) << s : p.v; }
    else if (h < 31) { const int n = 12 + (h - 16), s = Bits(p.v) - n; q = s > 0 ? ((p.v + ((i128)1 << (s - 1))) >> s) << s : p.v; }
    else { const int g = 102 + (h - 31) - 100; q = (p.v >> g) << g; }
    return ((i128)1 << 40) + q;                         // above every float word's key (at most 2^31)
}
static int Rel(const Pix& a, const Pix& b, int h)
{
    if (h == 0) return b.d < a.d ? -1 : b.d > a.d ? 1 : 0;
    const i128 ka = Key(a, h), kb = Key(b, h);
    return kb < ka ? -1 : kb > ka ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    // the model's own hypothesis (h 0) must be h 1 + 7 (19 bits cut)
    const int NSETS = 10;
    struct Slot { int X, Y; };
    std::vector<Slot> slots;
    for (int Y = 0; Y <= 25; Y += 5) for (int X = 0; X <= 25; X += 5) if (X + Y >= 20) slots.push_back({ X + 1, Y + 1 });
    std::map<std::pair<int, int>, int> sep;
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: per set, the pairs X, Y, P z1, z2, Q z1, z2 (float words) */\n#define NSETS %d\n#define NSLOTS %zu\n", NSETS, slots.size());
    fprintf(f, "static const pair_t PAIRS[NSETS][NSLOTS] = {\n");
    static Pix A[32][32], B[32][32];
    int bigpix = 0, check = 0;
    for (int set = 0; set < NSETS; set++) {
        fprintf(f, "  {\n");
        for (auto& sl : slots) {
            Tri bp{}, bq{}; long best = -1;
            for (int c = 0; c < 300; c++) {
                // A, B of P: multiples of 2^104 in [2^122, 2^124) (B also 0); Q: A, B moved by up to 2^6 units, or z1, z2 by ulps
                auto slope = [&]() -> int64_t { return irange((int64_t)1 << 18, ((int64_t)1 << 20) - 1); };     // units of 2^104
                int64_t a = slope(), b = irange(0, 3) == 0 ? 0 : slope();
                const int sh = irange(0, 2);
                int64_t da = 0, db = 0;
                switch (irange(0, 3)) {
                case 0: da = irange(-1, 1) * ((int64_t)1 << irange(0, 6)); break;
                case 1: db = irange(-1, 1) * ((int64_t)1 << irange(0, 6)); break;
                case 2: da = irange(-64, 64); db = irange(-64, 64); break;
                default: da = irange(-4, 4) * ((int64_t)1 << sh); db = irange(-4, 4); break;
                }
                auto word = [](int64_t u) -> uint32_t {          // 4 u 2^104 as a float word (exact: u < 2^20)
                    if (u <= 0) return 0;
                    Fp v; v.mant = (uint64_t)u; v.exp = 106;
                    return pvrisp::StoredDepth::FromFp(v).bits;
                };
                Tri p{ sl.X, sl.Y, word(a), word(b) }, q{ sl.X, sl.Y, word(a + da), word(b + db) };
                if (a + da < ((int64_t)1 << 18) || a + da >= ((int64_t)1 << 20) || (b && (b + db < ((int64_t)1 << 18) || b + db >= ((int64_t)1 << 20)))) continue;
                if (!b && db) continue;
                if (!Eval(p, A) || !Eval(q, B)) continue;
                std::map<std::pair<int, int>, int> add;
                int nb = 0;
                for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                    if (!A[j][i].in || !B[j][i].in) continue;
                    if (A[j][i].v < ((i128)1 << 28) && B[j][i].v < ((i128)1 << 28)) continue;
                    nb++;
                    int r[NH];
                    for (int h = 0; h < NH; h++) r[h] = Rel(A[j][i], B[j][i], h);
                    for (int h1 = 0; h1 < NH; h1++) for (int h2 = h1 + 1; h2 < NH; h2++) if (r[h1] != r[h2]) add[{h1, h2}]++;
                }
                long score = nb;
                for (auto& e : add) { const int have = sep[e.first]; score += std::max(0, std::min(12, have + e.second) - have) * 100; }
                if (score > best) { best = score; bp = p; bq = q; }
            }
            if (best < 0) { fprintf(stderr, "slot %d %d: none\n", sl.X, sl.Y); return 1; }
            Eval(bp, A); Eval(bq, B);
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                if (!A[j][i].in || !B[j][i].in) continue;
                if (A[j][i].v < ((i128)1 << 28) && B[j][i].v < ((i128)1 << 28)) continue;
                bigpix++;
                int r[NH];
                for (int h = 0; h < NH; h++) r[h] = Rel(A[j][i], B[j][i], h);
                if (r[0] != r[8]) check++;
                for (int h1 = 0; h1 < NH; h1++) for (int h2 = h1 + 1; h2 < NH; h2++) if (r[h1] != r[h2]) sep[{h1, h2}]++;
            }
            fprintf(f, "    { %d, %d, 0x%08xu, 0x%08xu, 0x%08xu, 0x%08xu },\n", bp.X, bp.Y, bp.z1, bp.z2, bq.z1, bq.z2);
        }
        fprintf(f, "  },\n");
    }
    fprintf(f, "};\n");
    fclose(f);
    int weak = 0;
    for (int h1 = 1; h1 < NH; h1++) for (int h2 = h1 + 1; h2 < NH; h2++) if (sep[{h1, h2}] < 3) { weak++; if (getenv("GEN_VERBOSE")) fprintf(stderr, "weak %d %d: %d\n", h1, h2, sep[{h1, h2}]); }
    fprintf(stderr, "%d pixels of 2^128 or more, %d weak hypothesis pairs, model vs 19 bits cut: %d differ\n", bigpix, weak, check);
    fprintf(stderr, "differ from the model:");
    for (int h = 1; h < NH; h++) fprintf(stderr, " %s%d:%d", h < 16 ? "cut" : h < 31 ? "round" : "grid", h < 16 ? 12 + h - 1 : h < 31 ? 12 + h - 16 : 102 + h - 31, sep[{0, h}]);
    fprintf(stderr, "\n");
    return 0;
}
