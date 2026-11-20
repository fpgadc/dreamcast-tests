/* The cases of pvr-tests/depth2_regmix (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   Each tile of a set holds a pair: P (DepthMode Always) then Q (the page's mode). One of the two is a triangle
   whose plane's slope overflows (regime C: zword's TL +-inf / C1, zovf_creg's C0 with a finite slope F a row), the
   other an ordinary block whose start the overflowing constant makes 0 (depthgaps_ties' B(L, Z): TL at z0, TR and
   BL at z1, z2, legs L, at a tile away from the origin: the depths -(i A + j B), E from the larger slope), or
   another register triangle. The pairs are chosen, from random ones, so that their pixels lie between the same two
   float words (where the ISP's order is decided by the representation of depths outside their block's top binade,
   pvr_isp_depth.h StoredDepth::Neg / RegDepth), with as many distinct (E, side) combinations as possible.
   Tile (0, 0) holds a regime-B pair: zword's TL +inf against a triangle whose start is -2^127 (depthgaps_bigpos). */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <string>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
static uint64_t S = 0x9e3779b97f4a7c15ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
static int64_t irange(int64_t lo, int64_t hi) { return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1)); }
static double urand() { return (double)(rnd() >> 11) / 9007199254740992.0; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

// a triangle as sent: three corners (x, y as small integers, z bits)
struct Tri { int x[3], y[3]; uint32_t z[3]; int kind; };
enum { ZW, CREG, BTRI, RB };
struct Pix { bool in; pvrisp::StoredDepth d; };

static void Eval(const Tri& t, int tx, int ty, Pix out[32][32])
{
    static const pvrisp::EdgeHW ecfg;
    static const pvrisp::DepthHW dcfg;
    const int left = 32 * tx, top = 32 * ty;
    uint32_t X[3], Y[3];
    Fp FX[3], FY[3];
    for (int k = 0; k < 3; k++) { X[k] = fb((float)t.x[k]); Y[k] = fb((float)t.y[k]); FX[k] = Fp::FromBits(X[k]); FY[k] = Fp::FromBits(Y[k]); }
    bool bad = false;
    pvrisp::Winding(FX, FY, ecfg, &bad);
    const int sgn = pvrisp::EdgeWinding(FX, FY, ecfg);
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 2 * (int64_t)left + 1, 2 * (int64_t)top + 1, sgn, ecfg);
    int64_t rf, rl;
    pvrisp::RowRange(FY, 3, &rf, &rl);
    if (rf - top == 31 && (E[0].big || E[1].big || E[2].big)) rf--;
    for (int k = 0; k < 3; k++) E[k].SetFirstRow(rf - top);
    pvrisp::DepthTileT<pvrisp::DepthHW> Z;
    Z.Setup(X, Y, t.z, left, top, dcfg);
    for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
        Pix& p = out[j][i];
        p.in = !bad && top + j >= rf && top + j <= rl && E[0].Inside(i, j) && E[1].Inside(i, j) && E[2].Inside(i, j);
        if (p.in) p.d = Z.At(i, j, true);
    }
}

// the category of a stored depth: which pair of float words it lies between (or the word), and for a depth
// between words, from which side of its block's window (the E of the representation)
static std::string Cat(const pvrisp::StoredDepth& d)
{
    char b[64];
    const int64_t w = d.k >> 32;
    const int64_t off = d.k & 0xffffffffLL;
    if (off == 0) { snprintf(b, sizeof b, "w%08llx", (unsigned long long)(uint32_t)d.bits); return b; }
    if ((uint32_t)w == 0x7f800000u || w >= 0x7f800000LL) { snprintf(b, sizeof b, "big%lld", (long long)(off >> 23)); return b; }
    // between key w and w + 1: name by the field of the word w (negative: inverted fraction)
    const uint32_t word = (uint32_t)(w < 0 ? ((uint32_t)w ^ 0x007fffffu) : (uint32_t)w);
    snprintf(b, sizeof b, "f%d:%s", (int)((word >> 23) & 0xff), off >= ((int64_t)1 << 31) ? "below" : "above");
    return b;
}

struct Pair { int tx, ty; Tri p, q; std::string name; };
static char Type(const Tri& t) { return t.kind == ZW ? 'Z' : t.kind == CREG ? 'C' : t.kind == RB ? 'R' : 'B'; }
// a category: the two depths' types and places (sorted), and their order
static std::string Key(int, const Tri& ta, const Tri& tb, const pvrisp::StoredDepth& da, const pvrisp::StoredDepth& db, bool)
{
    std::string a = std::string(1, Type(ta)) + Cat(da), b = std::string(1, Type(tb)) + Cat(db);
    char rel = da < db ? '<' : da == db ? '=' : '>';
    if (b < a) { std::swap(a, b); rel = rel == '<' ? '>' : rel == '>' ? '<' : '='; }
    return a + " " + rel + " " + b;
}

static Tri ZWord(int X, int Y, uint32_t s, bool tr, uint32_t others)
{
    Tri t{ { X, X + 32, X }, { Y, Y, Y + 32 }, { s, others, others }, ZW };
    if (tr) { t.z[0] = others; t.z[1] = s; }
    return t;
}
static Tri CReg(int X, int Y, double F)
{
    const double zB = -ldexp(1, 118), zR = ldexp(1, 123), zD = zB + 32 * F;
    Tri t{ { X, X + 32, X }, { Y, Y, Y + 32 }, { fb((float)zB), fb((float)zR), fb((float)zD) }, CREG };
    return t;
}
static bool Exact(double v) { return (double)(float)v == v; }

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    const int NSETS = 4;
    std::map<std::string, int> seen;                    // category pairs measured so far
    std::vector<std::vector<Pair>> sets(NSETS);
    static Pix A[32][32], B[32][32], A_[32][32];
    long interesting = 0;
    for (int set = 0; set < NSETS; set++) {
        for (int ty = 0; ty < 15; ty++) for (int tx = 0; tx < 20; tx++) {
            if (tx + ty <= 1) continue;                 // tiles (0, 0), (1, 0), (0, 1): the regime-B pairs (below)
            const int X = 32 * tx, Y = 32 * ty;
            Pair best{}; long bestScore = -1;
            for (int c = 0; c < 300; c++) {
                // the register triangle
                Tri r;
                const int rk = irange(0, 9);
                if (rk < 3) r = ZWord(X, Y, 0x7f800000u, false, 0x3f000000u);            // m = -(i + j)
                else if (rk < 4) r = ZWord(X, Y, 0xff800000u, false, 0x3f000000u);       // m = i + j
                else if (rk < 5) r = ZWord(X, Y, 0x7f800000u, false, 0x7b800000u);       // C1: m = -(i + j) - 1
                else {
                    // C0 with a finite slope F a row: r = i steps + j F
                    double F = ldexp(1 + (double)irange(0, 15) / 16, irange(100, 121));
                    if (irange(0, 1)) F = -F;
                    if (!Exact(-ldexp(1, 118) + 32 * F)) continue;
                    r = CReg(X, Y, F);
                }
                // the ordinary block: TL z0 huge, TR z1, BL z2, legs L; or another register triangle
                Tri b;
                if (irange(0, 7) == 0) {
                    double F = ldexp(1 + (double)irange(0, 15) / 16, irange(100, 121));
                    if (irange(0, 1)) F = -F;
                    if (!Exact(-ldexp(1, 118) + 32 * F)) continue;
                    b = CReg(X, Y, F);
                } else {
                    const int E = irange(122, 128);                  // the block's E: the larger slope's top + 5
                    const double sl = ldexp(1 + urand(), E - 6);     // the larger slope's size
                    const int L = std::max(2, std::min(31, (int)floor(sqrt(pow(2.0, 127.9) / sl))));
                    const double z0 = ldexp(1 + urand(), irange(122, 126));
                    // A = (z1 - z0) / L, B = (z2 - z0) / L; one of them about -sl, the other anything up to its size
                    double Aa = -sl, Bb = sl * (2 * urand() - 1);
                    if (irange(0, 1)) std::swap(Aa, Bb);
                    double z1 = z0 + L * Aa, z2 = z0 + L * Bb;
                    z1 = (double)(float)z1; z2 = (double)(float)z2;
                    if (fabs(z1) >= ldexp(1, 128) || fabs(z2) >= ldexp(1, 128)) continue;
                    b = Tri{ { X, X + L, X }, { Y, Y, Y + L }, { fb((float)z0), fb((float)z1), fb((float)z2) }, BTRI };
                }
                Eval(r, tx, ty, A);
                Eval(b, tx, ty, B);
                // the block must be an ordinary block from a start of 0 (or a register triangle)
                {
                    static const pvrisp::DepthHW dcfg;
                    pvrisp::DepthTileT<pvrisp::DepthHW> Z;
                    uint32_t xx[3], yy[3];
                    for (int k = 0; k < 3; k++) { xx[k] = fb((float)b.x[k]); yy[k] = fb((float)b.y[k]); }
                    Z.Setup(xx, yy, b.z, X, Y, dcfg);
                    if (b.kind == BTRI && !(Z.valid && Z.block && Z.d0 == 0)) continue;
                    if (b.kind == CREG && Z.ovf != 2) continue;
                    uint32_t rx[3], ry[3];
                    for (int k = 0; k < 3; k++) { rx[k] = fb((float)r.x[k]); ry[k] = fb((float)r.y[k]); }
                    pvrisp::DepthTileT<pvrisp::DepthHW> R;
                    R.Setup(rx, ry, r.z, X, Y, dcfg);
                    if (R.ovf != 2 || R.r0 != 0) continue;
                }
                long score = 0;
                std::map<std::string, int> add;
                for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                    if (!A[j][i].in || !B[j][i].in) continue;
                    const auto &da = A[j][i].d, &db = B[j][i].d;
                    if ((da.k >> 32) != (db.k >> 32)) continue;      // not between the same two words
                    if ((da.k & 0xffffffffLL) == 0 && (db.k & 0xffffffffLL) == 0) continue;     // two float words
                    add[Key(best.p.kind, r, b, da, db, false)]++;
                }
                for (auto& e : add) {
                    // a block against a register triangle counts most, register against register little
                    const int have = seen[e.first];
                    const bool hasB = e.first[0] == 'B' || e.first.find(" B") != std::string::npos;
                    const bool both = e.first[0] == 'B' && e.first.find(" B") != std::string::npos;
                    if (hasB && !both) score += std::max(0, std::min(40, have + e.second) - have) * 1000 + e.second * 10;
                    else score += std::max(0, std::min(6, have + e.second) - have) * 10;
                }
                if (score > bestScore) {
                    bestScore = score;
                    const bool flip = irange(0, 1);
                    best = Pair{ tx, ty, flip ? b : r, flip ? r : b, "" };
                }
            }
            if (bestScore <= 0) continue;
            Eval(best.p, tx, ty, A); Eval(best.q, tx, ty, B);
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                if (!A[j][i].in || !B[j][i].in) continue;
                const auto &da = A[j][i].d, &db = B[j][i].d;
                if ((da.k >> 32) != (db.k >> 32)) continue;
                if ((da.k & 0xffffffffLL) == 0 && (db.k & 0xffffffffLL) == 0) continue;
                seen[Key(0, best.p, best.q, da, db, true)]++;
                interesting++;
            }
            sets[set].push_back(best);
        }
    }
    // tile (0, 0): zword's TL +inf with legs 64 (Q) over small regime-B triangles (P): V0 = (X, Y) at 0, V1 =
    // (X + L, Y) at z1, V2 = (X, Y + L) at z2; the constant overflows, the tile's start is -2^127 (depthgaps_bigpos)
    std::vector<std::vector<Tri>> slots0(NSETS);
    const Tri zw64{ { 0, 64, 0 }, { 0, 0, 64 }, { 0x7f800000u, 0x3f000000u, 0x3f000000u }, ZW };
    Eval(zw64, 0, 0, B);
    for (int set = 0; set < NSETS; set++) {
        for (int sy = 0; sy < 5; sy++) for (int sx = 0; sx < 5; sx++) {
            const int X = 1 + 6 * sx, Y = 1 + 6 * sy;
            Tri best{}; long bestScore = -1;
            for (int c = 0; c < 400; c++) {
                const int L = irange(2, 5);
                const double lim = pow(2.0, 127.95) / (L * L);                // a = L z1 below 2^128: |A| L^2
                auto pick = [&]() { double v = ldexp(1 + urand(), irange(118, 126)); if (v >= lim) v = lim * urand(); return irange(0, 1) ? v : -v; };
                const double A = pick(), Bv = irange(0, 3) ? pick() : 0;
                const double z1 = (double)(float)(A * L), z2 = (double)(float)(Bv * L);
                const Tri t{ { X, X + L, X }, { Y, Y, Y + L }, { 0, fb((float)z1), fb((float)z2) }, RB };
                {
                    static const pvrisp::DepthHW dcfg;
                    pvrisp::DepthTileT<pvrisp::DepthHW> Z;
                    uint32_t xx[3], yy[3];
                    for (int k = 0; k < 3; k++) { xx[k] = fb((float)t.x[k]); yy[k] = fb((float)t.y[k]); }
                    Z.Setup(xx, yy, t.z, 0, 0, dcfg);
                    if (!(Z.valid && Z.block && Z.E == 128 && Z.d0 < 0)) continue;     // the start -2^127
                }
                Eval(t, 0, 0, A_);
                long score = 0;
                std::map<std::string, int> add;
                for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                    if (!A_[j][i].in || !B[j][i].in) continue;
                    const auto &da = A_[j][i].d, &db = B[j][i].d;
                    if ((da.k >> 32) != (db.k >> 32)) continue;
                    add[Key(0, t, zw64, da, db, false)]++;
                }
                for (auto& e : add) { const int have = seen[e.first]; score += std::max(0, std::min(30, have + e.second) - have) * 1000 + e.second; }
                if (score > bestScore) { bestScore = score; best = t; }
            }
            if (bestScore <= 0) continue;
            Eval(best, 0, 0, A_);
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                if (!A_[j][i].in || !B[j][i].in) continue;
                const auto &da = A_[j][i].d, &db = B[j][i].d;
                if ((da.k >> 32) != (db.k >> 32)) continue;
                seen[Key(0, best, zw64, da, db, true)]++;
                interesting++;
            }
            slots0[set].push_back(best);
        }
    }
    fprintf(stderr, "%ld pixel pairs between the same two words, %zu categories\n", interesting, seen.size());
    if (getenv("GEN_VERBOSE")) for (auto& e : seen) fprintf(stderr, "  %s: %d\n", e.first.c_str(), e.second);
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: per set, tile x, y, then P and Q: kind, corners x y z (z as float words) */\n#define NSETS %d\n", NSETS);
    size_t mx = 0;
    for (auto& s : sets) mx = std::max(mx, s.size());
    fprintf(f, "#define MAXPAIRS %zu\nstatic const int NPAIRS[NSETS] = {", mx);
    for (auto& s : sets) fprintf(f, " %zu,", s.size());
    fprintf(f, " };\nstatic const pair_t PAIRS[NSETS][MAXPAIRS] = {\n");
    auto tri = [&](const Tri& t) {
        fprintf(f, "{ %d, { %d, %d, %d }, { %d, %d, %d }, { 0x%08xu, 0x%08xu, 0x%08xu } }", t.kind, t.x[0], t.x[1], t.x[2], t.y[0], t.y[1], t.y[2], t.z[0], t.z[1], t.z[2]);
    };
    for (auto& s : sets) {
        fprintf(f, "  {\n");
        for (auto& p : s) { fprintf(f, "    { %d, %d, ", p.tx, p.ty); tri(p.p); fprintf(f, ", "); tri(p.q); fprintf(f, " },\n"); }
        fprintf(f, "  },\n");
    }
    fprintf(f, "};\n");
    size_t ms = 0;
    for (auto& s0 : slots0) ms = std::max(ms, s0.size());
    fprintf(f, "#define MAXSLOTS %zu\nstatic const int NSLOTS[NSETS] = {", ms);
    for (auto& s0 : slots0) fprintf(f, " %zu,", s0.size());
    fprintf(f, " };\nstatic const tri_t SLOTS[NSETS][MAXSLOTS] = {\n");
    for (auto& s0 : slots0) {
        fprintf(f, "  {\n");
        for (auto& t : s0) { fprintf(f, "    "); tri(t); fprintf(f, ",\n"); }
        fprintf(f, "  },\n");
    }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
