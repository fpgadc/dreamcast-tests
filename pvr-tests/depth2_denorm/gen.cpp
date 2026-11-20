/* The cases of pvr-tests/depth2_denorm (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   Each case is two planes P, Q over one tile, triangles (X, Y), (X + 64, Y), (X, Y + 64), corner depths
   z = n0 + nx x + ny y in units of 2^-146 (exact floats). The cases are chosen, from random ones, so that the
   grids a store of the positive depths below 2^-126 could have (2^g, g = -145 .. -128, truncated or rounded, or
   one word for all) give different pictures; the depths are refsw3's (pvr_isp_depth.h, DepthTile). */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <cmath>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
typedef pvrfp::i128 i128;
static uint64_t S = 0x9e3779b97f4a7c15ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
static int64_t irange(int64_t lo, int64_t hi) { return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1)); }

// n 2^-146 as a float word; only exact ones are used
static bool fbits(int64_t n, uint32_t* out)
{
    const uint32_t sign = n < 0 ? 0x80000000u : 0;
    uint64_t m = n < 0 ? (uint64_t)-n : (uint64_t)n;
    int e = -146;
    if (!m) { *out = sign; return true; }
    while (m >= (1ull << 24)) { if (m & 1) return false; m >>= 1; e++; }
    while (m < (1ull << 23)) { m <<= 1; e--; }
    const int f = e + 150;
    if (f <= 0) return false;                           // no denormal corners
    *out = sign | (uint32_t)f << 23 | (uint32_t)(m & 0x7fffffu);
    return true;
}
static uint32_t fint(int v) { float f = (float)v; uint32_t u; memcpy(&u, &f, 4); return u; }

struct Plane { int64_t n0, nx, ny; };
struct Case { int tx, ty; Plane p, q; };

// the 32 x 32 pixels of a plane in tile (tx, ty): covered, and the exact block value in units of 2^-160
struct Pix { bool in; int kind; i128 v; pvrisp::StoredDepth d; };
static bool Eval(const Plane& pl, int tx, int ty, Pix out[32][32])
{
    const int X = 32 * tx, Y = 32 * ty;
    uint32_t x[3] = { fint(X), fint(X + 64), fint(X) }, y[3] = { fint(Y), fint(Y), fint(Y + 64) }, z[3];
    if (!fbits(pl.n0, &z[0]) || !fbits(pl.n0 + 64 * pl.nx, &z[1]) || !fbits(pl.n0 + 64 * pl.ny, &z[2])) return false;
    static const pvrisp::DepthHW cfg;
    pvrisp::DepthTileT<pvrisp::DepthHW> Z;
    Z.Setup(x, y, z, X, Y, cfg);
    if (!Z.valid || !Z.block) return false;
    for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
        Pix& p = out[j][i];
        p.in = true;                                    // the triangle covers its whole first tile
        i128 t = Z.d0 + (2 * i) * Z.dx + (2 * j) * Z.dy;  // units 2^(unit - 1)
        const int s = (Z.unit - 1) - (-160);
        if (s < 0) return false;
        p.v = t << s;
        p.d = Z.At(i, j, true);
    }
    return true;
}

// hypotheses for a positive depth below 2^-126 (v in units of 2^-160): 0 the model (refsw3), 1 + k: truncated to
// 2^(-145 + k), 20 + k: rounded (half up) to 2^(-145 + k), k = 0 .. 17; 40: one word for all; 41: exponent field 0
// with the 18 fraction bits of the normalized value (not monotone across binades)
static const int NH = 42, NHS = 41;   // NHS: the hypotheses the first set was chosen with
static i128 Key(const Pix& p, int h)
{
    const i128 lim = (i128)1 << 34;                     // 2^-126
    if (h == 0 || p.v <= 0 || p.v >= lim) return (i128)p.d.k;
    // a key between those of 0 and of 2^-126 (both StoredDepth words: 0 and 0x00800000 << 32)
    i128 q;
    if (h == 40) q = 1;
    else if (h == 41) { int b = 0; i128 x = p.v; while (x) { b++; x >>= 1; } const i128 f = (b > 19 ? p.v >> (b - 19) : p.v << (19 - b)) & ((1 << 18) - 1); q = f << 10; }
    else if (h < 20) { const int g = 15 + (h - 1); q = (p.v >> g) << g; }
    else { const int g = 15 + (h - 20); q = ((p.v + ((i128)1 << (g - 1))) >> g) << g; if (q >= lim) q = lim - 1; }
    return q * 2 + 1;   // inside (0, 2^-126): above 0, below 2^-126 (below)
}
static int Rel(const Pix& a, const Pix& b, int h)    // b against a
{
    auto key = [&](const Pix& p) -> i128 {
        const i128 lim = (i128)1 << 34;
        // the model's key, scaled so that the hypotheses' keys of (0, 2^-126) (1 .. 2^35) fit between 0 and the
        // key of 2^-126 (0x00800000 << 32)
        if (h == 0 || p.v <= 0 || p.v >= lim) return (i128)p.d.k * ((i128)1 << 20);
        return Key(p, h);
    };
    const i128 ka = key(a), kb = key(b);
    return kb < ka ? -1 : kb > ka ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    std::vector<Case> out;
    std::map<std::pair<int, int>, int> sep;             // hypothesis pairs: pixels that tell them apart
    static Pix A[32][32], B[32][32];
    int tiles = 0, tinyPix = 0, farPix = 0, nearNorm = 0;
    std::map<int, int> farSep;
    for (int ty = 0; ty < 14; ty += 2) for (int tx = 0; tx < 20; tx += 2) {
        Case best{}; long bestScore = -1;
        for (int c = 0; c < 400; c++) {
            Case k{tx, ty, {}, {}};
            // P: the plane 2^-126 + s at the tile's first pixel, A = -(2^-126 + a), B = 2^-126 + b (units 2^-146): the
            // pixels j = i - 1 at v_i = s - b + i (b - a), below 2^-126; Q: P with n0, a, b moved a little
            const int64_t U = 1 << 20;
            const bool sweep = irange(0, 2) == 0;
            int64_t g = sweep ? irange(1, 1 << irange(8, 15)) : irange(0, 16);
            if (irange(0, 1)) g = -g;
            g &= ~(int64_t)1;
            const int64_t v1 = (int64_t)exp2(6.0 + 14.0 * (double)(rnd() % 100000) / 100000.0);
            int64_t b = irange(0, U / 4) & ~(int64_t)1, a = b - g;
            if (a < 0 || a >= U) continue;
            // s - b + g = v1 (the first pixel of the diagonal); n0 = 2^-126 + s - (nx + ny) / 2
            const int64_t s = v1 + b - g;
            k.p.nx = -(U + a); k.p.ny = U + b;
            k.p.n0 = (U + s - (k.p.nx + k.p.ny) / 2) & ~(int64_t)7;
            k.q = k.p;
            if (irange(0, 1)) k.q.n0 += 8 * irange(-2, 2);
            if (irange(0, 1)) { const int64_t e = 2 * irange(-2, 2); k.q.nx -= e; k.q.ny += irange(0, 1) ? e : 0; }
            if (irange(0, 3) == 0) { const int64_t e = 2 * irange(-3, 3); k.q.ny += e; }
            auto ok = [&](int64_t n) { const int64_t m = n < 0 ? -n : n; return m >= U && m < 2 * U; };
            if (!ok(k.p.nx) || !ok(k.p.ny) || !ok(k.q.nx) || !ok(k.q.ny)) continue;
            if (!Eval(k.p, tx, ty, A) || !Eval(k.q, tx, ty, B)) continue;
            // the score: hypothesis pairs newly told apart (counting up to 4 pixels each)
            std::map<std::pair<int, int>, int> add;
            long score = 0;
            int den = 0;
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                const Pix &pa = A[j][i], &pb = B[j][i];
                const bool tiny = (pa.v > 0 && pa.v < ((i128)1 << 34)) || (pb.v > 0 && pb.v < ((i128)1 << 34));
                if (!tiny) continue;
                den++;
                int r[NHS];
                for (int h = 0; h < NHS; h++) r[h] = Rel(pa, pb, h);
                for (int h1 = 0; h1 < NHS; h1++) for (int h2 = h1 + 1; h2 < NHS; h2++)
                    if (r[h1] != r[h2]) add[{h1, h2}]++;
            }
            for (auto& e : add) { const int have = sep[e.first]; score += std::max(0, std::min(30, have + e.second) - have) * 100; }
            score += den;
            if (score > bestScore) { bestScore = score; best = k; }
        }
        if (bestScore < 0) { fprintf(stderr, "tile %d %d: no case\n", tx, ty); return 1; }
        Eval(best.p, tx, ty, A); Eval(best.q, tx, ty, B);
        for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
            const Pix &pa = A[j][i], &pb = B[j][i];
            const bool tiny = (pa.v > 0 && pa.v < ((i128)1 << 34)) || (pb.v > 0 && pb.v < ((i128)1 << 34));
            if (!tiny) continue;
            int r[NHS];
            for (int h = 0; h < NHS; h++) r[h] = Rel(pa, pb, h);
            for (int h1 = 0; h1 < NHS; h1++) for (int h2 = h1 + 1; h2 < NHS; h2++)
                if (r[h1] != r[h2]) sep[{h1, h2}]++;
            tinyPix++;
            if (Rel(pa, pb, 41) != Rel(pa, pb, 0)) nearNorm++;
        }
        out.push_back(best);
        tiles++;
        if (getenv("GEN_VERBOSE")) {
            int n = 0; double lo = 1e9, hi = -1e9; std::map<long long, int> dd;
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                const Pix &pa = A[j][i], &pb = B[j][i];
                if (!((pa.v > 0 && pa.v < ((i128)1 << 34)) || (pb.v > 0 && pb.v < ((i128)1 << 34)))) continue;
                n++; const double l = log2((double)pa.v) - 160; if (l < lo) lo = l; if (l > hi) hi = l;
                dd[(long long)((pb.v - pa.v) >> 15)]++;
            }
            fprintf(stderr, "tile %2d %2d: %2d tiny, log2 %.2f .. %.2f, Q - P (2^-145):", tx, ty, n, lo, hi);
            int c = 0; for (auto& e : dd) { if (c++ < 6) fprintf(stderr, " %lld x%d", e.first, e.second); }
            fprintf(stderr, "\n");
        }
    }
    // the second set: Q the same plane moved by 2^-141 .. 2^-126 (either way): pairs across binades (2^-128 against
    // 2^-127 against 1.5 2^-127 ..)
    std::vector<Case> far;
    for (int ty = 0; ty < 14; ty += 2) for (int tx = 0; tx < 20; tx += 2) {
        for (int c = 0; c < 1000; c++) {
            Case k{tx, ty, {}, {}};
            const int64_t U = 1 << 20;
            int64_t g = irange(1, 1 << irange(8, 15));
            if (irange(0, 1)) g = -g;
            g &= ~(int64_t)1;
            const int64_t v1 = (int64_t)exp2(6.0 + 14.0 * (double)(rnd() % 100000) / 100000.0);
            int64_t b = irange(0, U / 4) & ~(int64_t)1, a = b - g;
            if (a < 0 || a >= U) continue;
            const int64_t s = v1 + b - g;
            k.p.nx = -(U + a); k.p.ny = U + b;
            k.p.n0 = (U + s - (k.p.nx + k.p.ny) / 2) & ~(int64_t)7;
            k.q = k.p;
            k.q.n0 += (irange(0, 1) ? 1 : -1) * ((int64_t)8 << irange(2, 17));
            if (!Eval(k.p, tx, ty, A) || !Eval(k.q, tx, ty, B)) continue;
            int both = 0;
            std::map<int, int> sepHere;
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++)
                if (A[j][i].v > 0 && A[j][i].v < ((i128)1 << 34) && B[j][i].v > 0 && B[j][i].v < ((i128)1 << 34)) {
                    both++;
                    for (int h = 1; h < NH; h++) if (Rel(A[j][i], B[j][i], h) != Rel(A[j][i], B[j][i], 0)) sepHere[h]++;
                }
            if (both < 8) continue;
            for (auto& e : sepHere) farSep[e.first] += e.second;
            farPix += both;
            far.push_back(k);
            break;
        }
    }
    fprintf(stderr, "far set: %zu tiles, %d pixel pairs both between 0 and 2^-126; differ from the model: one word %d, normalized fraction %d, 2^-140 grid %d\n",
            far.size(), farPix, farSep[40], farSep[41], farSep[6]);
    // how well the hypotheses are told apart
    int weak = 0;
    for (int h1 = 0; h1 < NHS; h1++) for (int h2 = h1 + 1; h2 < NHS; h2++) {
        // T_-145 (h 1) and R_-145 (20) are the model on this grid (values are on 2^-145): skip those
        auto same = [](int h) { return h == 0 || h == 1 || h == 20; };
        if (same(h1) && same(h2)) continue;
        if (sep[{h1, h2}] < 3) { weak++; fprintf(stderr, "weak: %d %d: %d\n", h1, h2, sep[{h1, h2}]); }
    }
    fprintf(stderr, "%d tiles, %d weak hypothesis pairs; %d pixel pairs with a depth in (0, 2^-126)\n", tiles, weak, tinyPix);
    // per hypothesis: the pixels (of the three pages: each pair once) where it differs from refsw3's (the model)
    fprintf(stderr, "differ from the model:");
    for (int h = 1; h < NHS; h++) fprintf(stderr, " %s%d:%d", h < 20 ? "T" : h < 40 ? "R" : "one", h < 20 ? -145 + (h - 1) : h < 40 ? -145 + (h - 20) : 0, sep[{0, h}]);
    fprintf(stderr, " normalized-fraction:%d", nearNorm);
    fprintf(stderr, "\n");
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: tile x, y; P n0 nx ny; Q n0 nx ny (2^-146) */\n#define NCASES %zu\n", out.size());
    fprintf(f, "static const case_t CASES[NCASES] = {\n");
    for (auto& k : out)
        fprintf(f, "    { %d, %d, { %lldLL, %lldLL, %lldLL }, { %lldLL, %lldLL, %lldLL } },\n", k.tx, k.ty, (long long)k.p.n0, (long long)k.p.nx, (long long)k.p.ny,
                (long long)k.q.n0, (long long)k.q.nx, (long long)k.q.ny);
    fprintf(f, "};\n#define NCASES2 %zu\nstatic const case_t CASES2[NCASES2] = {\n", far.size());
    for (auto& k : far)
        fprintf(f, "    { %d, %d, { %lldLL, %lldLL, %lldLL }, { %lldLL, %lldLL, %lldLL } },\n", k.tx, k.ty, (long long)k.p.n0, (long long)k.p.nx, (long long)k.p.ny,
                (long long)k.q.n0, (long long)k.q.nx, (long long)k.q.ny);
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
