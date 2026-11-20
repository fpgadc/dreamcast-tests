/* The cases of pvr-tests/depth2_regbelow (main.c says what the test does). Writes cases.h next to this file.
       g++ -O2 -std=c++17 -I <nulldc3>/refsw3/refcore -o gen gen.cpp && ./gen
   An ordinary block of E 125 .. 127 has depths below its window (beyond -2^E) only where a corner's depth is there
   too, and such a corner's products in the setup (z times a coordinate difference, z times an edge x_j y_k - x_k y_j)
   stay below 2^128 only for a tiny triangle near the origin. So: per target pixel a tiny triangle (corners 1 .. 4
   units of 2^-10 .. 2^-6 pixel around the pixel's centre, or, half the tries, a thin one along the plane's level line
   through it: corners 2^-9 .. 2^-6 either way along it, 2^-14 .. 2^-12 up and down the slope; the pixel the only one
   inside) whose corners' depths make a plane
   through a chosen value v at the pixel, with slopes below 2^(E - 5) that put the start, at the tile's first pixel,
   in the window of E (-2^E .. -2^(E - 1)): a block of E with this one pixel below its window. Against a register
   triangle in the same tile (pvr_isp_depth.h DepthTileT::RegDepth, StoredDepth::Neg), at the pixels where the register
   lies between the same two float words:
     tile (0, 0): zword's TL +inf with legs 64 (m = -(i + j) mod 32): m = 22 at i + j = 42 (E 127, M = -2^17) and
       m = 19 at i + j = 45 (E 126, M = -2^17), whole words that a block of E 127 / 126 reaches there and nowhere else
     the other tiles: zovf_creg's C0 with a small finite slope F < 0 a row (below 2^118: |F| = (1 + k/16) 2^e + m
       2^102, e 108 .. 117, m < 2^9, so its values are off the grid 2^(E - 16) of the block below them; a larger F
       steps the rows by whole steps too): its start 0 or -1 step, so just below m = 23 (E 127 below its window,
       between m = 22 and 23), and just below m = 20, 18, 17 (the register E 126, 125, 124 above its window, against
       a block of E 127, 126, 125 below its window: the same two words)
   v is searched so that the block's M (StoredDepth::Neg) is the register's M (rounded down) + d, d mostly -2 .. 2
   (the register below the window of the same E), or anywhere below the window (the register above that of E - 1).
   Tiles whose triangles the setup cannot keep in range (too far from the origin) are left empty. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include "pvr_isp_depth.h"
#include "pvr_isp_edge.h"

using pvrfp::Fp;
static uint64_t S = 0x6a09e667f3bcc909ull;
static uint64_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
static int64_t irange(int64_t lo, int64_t hi) { return lo + (int64_t)(rnd() % (uint64_t)(hi - lo + 1)); }
static double urand() { return (double)(rnd() >> 11) / 9007199254740992.0; }
static uint32_t fb(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

struct Tri { uint32_t x[3], y[3], z[3]; };
struct Pix { bool in; pvrisp::StoredDepth d; };
using DT = pvrisp::DepthTileT<pvrisp::DepthHW>;

// coverage and depths of a triangle in tile (tx, ty), as refsw_tile.cpp (pixel centres); the number of pixels inside
static int Eval(const Tri& t, int tx, int ty, Pix out[32][32], DT* zo)
{
    static const pvrisp::EdgeHW ecfg;
    static const pvrisp::DepthHW dcfg;
    const int left = 32 * tx, top = 32 * ty;
    Fp FX[3], FY[3];
    for (int k = 0; k < 3; k++) { FX[k] = Fp::FromBits(t.x[k]); FY[k] = Fp::FromBits(t.y[k]); }
    bool bad = false;
    pvrisp::Winding(FX, FY, ecfg, &bad);
    const int sgn = pvrisp::EdgeWinding(FX, FY, ecfg);
    pvrisp::EdgeTile E[3];
    for (int k = 0; k < 3; k++) E[k].Setup(FX[k], FY[k], FX[(k + 1) % 3], FY[(k + 1) % 3], 2 * (int64_t)left + 1, 2 * (int64_t)top + 1, sgn, ecfg);
    int64_t rf, rl;
    pvrisp::RowRange(FY, 3, &rf, &rl);
    if (rf - top == 31 && (E[0].big || E[1].big || E[2].big)) rf--;
    for (int k = 0; k < 3; k++) E[k].SetFirstRow(rf - top);
    DT& Z = *zo;
    Z.Setup(t.x, t.y, t.z, left, top, dcfg);
    int n = 0;
    for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
        Pix& p = out[j][i];
        p.in = !bad && top + j >= rf && top + j <= rl && E[0].Inside(i, j) && E[1].Inside(i, j) && E[2].Inside(i, j);
        if (p.in) { p.d = Z.At(i, j, true); n++; }
    }
    return n;
}

// a depth between the words of fields E + 125 and E + 126 from below E's window: its M (else no)
static bool BelowM(const pvrisp::StoredDepth& d, int64_t* M)
{
    const int64_t off = d.k & 0xffffffffLL;
    if (off < ((int64_t)1 << 31)) return false;
    *M = off - ((int64_t)3 << 30);
    return true;
}
static uint32_t FieldOf(const pvrisp::StoredDepth& d)
{
    const int64_t w = d.k >> 32;
    const uint32_t word = (uint32_t)(w < 0 ? ((uint32_t)w ^ 0x007fffffu) : (uint32_t)w);
    return (word >> 23) & 0xff;
}

static Tri CReg(int X, int Y, double F)
{
    const double zB = -ldexp(1, 118), zR = ldexp(1, 123), zD = zB + 32 * F;
    Tri t{ { fb((float)X), fb((float)(X + 32)), fb((float)X) }, { fb((float)Y), fb((float)Y), fb((float)(Y + 32)) },
           { fb((float)zB), fb((float)zR), fb((float)zD) } };
    return t;
}
static bool Exact(double v) { return (double)(float)v == v; }

// a tiny triangle around the centre of pixel (i, j) of tile (tx, ty): a plane through v there with slopes A, B
static Tri Tiny(int tx, int ty, int i, int j, double v, double A, double B, int gexp, const int o[6])
{
    const double px = 32 * tx + i + 0.5, py = 32 * ty + j + 0.5, g = ldexp(1, -gexp);
    Tri t;
    for (int k = 0; k < 3; k++) {
        const double dx = g * o[2 * k], dy = g * o[2 * k + 1];
        t.x[k] = fb((float)(px + dx));
        t.y[k] = fb((float)(py + dy));
        t.z[k] = fb((float)(v + A * dx + B * dy));
    }
    return t;
}

// the same along the plane's level line through the centre: two corners L either way along it and eta up the slope,
// one eps down it (where the depth is lowest: the centre's value only eps |slope| above it)
static Tri Thin(int tx, int ty, int i, int j, double v, double A, double B, double L, double eta, double eps)
{
    const double px = 32 * tx + i + 0.5, py = 32 * ty + j + 0.5, g = sqrt(A * A + B * B);
    const double nx = A / g, ny = B / g, lx = -ny, ly = nx;
    const double cx[3] = { px + L * lx + eta * nx, px - L * lx + eta * nx, px - eps * nx };
    const double cy[3] = { py + L * ly + eta * ny, py - L * ly + eta * ny, py - eps * ny };
    Tri t;
    for (int k = 0; k < 3; k++) {
        const float fx = (float)cx[k], fy = (float)cy[k];
        t.x[k] = fb(fx);
        t.y[k] = fb(fy);
        t.z[k] = fb((float)(v + A * ((double)fx - px) + B * ((double)fy - py)));
    }
    return t;
}

struct Target { int i, j, E; bool same; };
struct TileCase { int tx, ty; bool regFirst; Tri reg; std::vector<Tri> tiny; };

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    const int NSETS = 4;
    std::vector<std::vector<TileCase>> sets(NSETS);
    static Pix R[32][32], B[32][32];
    std::map<std::string, long> stat;
    long pairs = 0;
    for (int set = 0; set < NSETS; set++) {
        for (int ty = 0; ty < 7; ty++) for (int tx = 0; tx < 8; tx++) {
            if (tx + ty == 1) continue;                 // tiles (1, 0), (0, 1): under zword's legs 64 from (0, 0)
            const int X = 32 * tx, Y = 32 * ty;
            TileCase tc{ tx, ty, (bool)irange(0, 1), {}, {} };
            DT Zr;
            if (tx == 0 && ty == 0) {
                // zword's TL +inf with legs 64: m = -(i + j) mod 32 (depthgaps_bigpos, depth2_regmix)
                tc.reg = Tri{ { fb(0.f), fb(64.f), fb(0.f) }, { fb(0.f), fb(0.f), fb(64.f) }, { 0x7f800000u, 0x3f000000u, 0x3f000000u } };
            } else {
                // C0 with a small finite slope F < 0 a row, off the grid of the block below
                double F = 0;
                for (int c = 0; c < 100; c++) {
                    F = -(ldexp(1 + (double)irange(0, 15) / 16, (int)irange(108, 117)) + ldexp((double)irange(1, 511), 102));
                    if (Exact(-ldexp(1, 118) + 32 * F)) break;
                }
                tc.reg = CReg(X, Y, F);
            }
            Eval(tc.reg, tx, ty, R, &Zr);
            if (Zr.ovf != 2 || Zr.sb != (tx || ty ? 0 : -1)) { fprintf(stderr, "tile %d %d: not the register expected (ovf %d sb %d)\n", tx, ty, Zr.ovf, Zr.sb); continue; }
            // targets: every pixel of the register between the words of fields E + 125 and E + 126 for a block of E
            // 125 .. 127 below its window: the register below the window of E (same) or above that of E - 1
            std::vector<Target> tg;
            for (int j = 0; j < 32; j++) for (int i = 0; i < 32; i++) {
                if (!R[j][i].in) continue;
                const auto& d = R[j][i].d;
                const int64_t off = d.k & 0xffffffffLL;
                if (off == 0) continue;
                const uint32_t fld = FieldOf(d);
                const bool below = off >= ((int64_t)1 << 31);
                const int E = (int)fld - 125;                   // the block's E: below its window it is between field E + 125's
                                                                // words and E + 126's (the register: E's below, E - 1's above)
                if (E < 125 || E > 127) continue;
                // a block of E reaches below its window at most (i + j) slopes of 2^(E - 5) beyond its start (above
                // -2^E): M from -2^16 - (i + j) 2^11
                int64_t Mr;
                if (below && BelowM(d, &Mr) && Mr < -65536 - (int64_t)(i + j) * 1986) continue;
                tg.push_back({ i, j, E, below });
            }
            // at most 16 of the same E and 8 others a tile
            for (size_t a = tg.size(); a > 1; a--) std::swap(tg[a - 1], tg[(size_t)irange(0, (int64_t)a - 1)]);
            {
                std::vector<Target> keep;
                int ns = 0, nc = 0;
                const int capS = tx == 0 && ty == 0 ? 48 : 16;  // tile (0, 0): the words m = 22 and 19
                for (const Target& t : tg) if (t.same ? ns++ < capS : nc++ < 8) keep.push_back(t);
                tg = keep;
            }
            for (const Target& T : tg) {
                if (!R[T.j][T.i].in) continue;
                const pvrisp::StoredDepth dr = R[T.j][T.i].d;
                int64_t Mr = 0;
                const bool rBelow = BelowM(dr, &Mr);
                if (T.same && !rBelow) continue;
                // the wanted block M: the register's + d (same E), or anywhere below the window (the register above)
                int64_t want;
                if (T.same) {
                    const int r = (int)irange(0, 9);
                    const int64_t d = r < 3 ? 0 : r < 5 ? 1 : r < 7 ? -1 : r == 7 ? (irange(0, 1) ? 2 : -2) : (irange(0, 1) ? 1 : -1) * irange(3, 300);
                    want = Mr + d;
                } else
                    want = -(int64_t)irange(65537, 100000);
                if (want >= -65536) continue;
                const int E = T.E;
                const double unitM = ldexp(1, E - 16);
                Tri best{}; int64_t bestErr = -1;
                double v = (want + 0.5) * unitM;
                for (int c = 0; c < 4000 && bestErr != 0; c++) {
                    // the start in the window of E, the slopes below 2^(E - 5)
                    const double s = -ldexp(1 + 0.02 + 0.97 * urand(), E - 1);
                    const double need = v - s;
                    const double lim = ldexp(0.97, E - 5);
                    const double alpha = std::min(1.0, std::max(0.0, (double)T.i / (T.i + T.j) + 0.3 * (urand() - 0.5)));
                    double A = T.i ? alpha * need / T.i : 0, Bs = T.j ? (1 - alpha) * need / T.j : 0;
                    if (!T.i) Bs = need / T.j;
                    if (!T.j) A = need / T.i;
                    if (fabs(A) >= lim || fabs(Bs) >= lim) continue;
                    const int gexp = (int)irange(6, 10);
                    int o[6];
                    // corners around the centre, the centre inside: one in each of three directions
                    o[0] = -(int)irange(1, 4); o[1] = -(int)irange(1, 4);
                    o[2] = (int)irange(1, 4);  o[3] = -(int)irange(-1, 4);
                    o[4] = -(int)irange(-1, 2); o[5] = (int)irange(1, 4);
                    const Tri t = irange(0, 1) ? Tiny(tx, ty, T.i, T.j, v, A, Bs, gexp, o)
                                               : Thin(tx, ty, T.i, T.j, v, A, Bs, ldexp(1 + urand(), -(int)irange(7, 9)), ldexp(1 + urand(), -(int)irange(12, 14)), ldexp(1 + urand(), -(int)irange(12, 14)));
                    DT Zb;
                    const int n = Eval(t, tx, ty, B, &Zb);
                    if (n != 1 || !B[T.j][T.i].in) continue;
                    if (!(Zb.valid && Zb.block && Zb.ovf == 0 && Zb.E == E)) continue;
                    int64_t Mb;
                    if (!BelowM(B[T.j][T.i].d, &Mb)) continue;
                    if (FieldOf(B[T.j][T.i].d) != FieldOf(dr)) continue;          // between the same two words
                    const int64_t err = Mb > want ? Mb - want : want - Mb;
                    if (bestErr < 0 || err < bestErr) { bestErr = err; best = t; }
                    // aim again: the value moves the block's M about one for one
                    if (err > 0 && (c % 8) == 7) v += (double)(want - Mb) * unitM * (0.5 + urand());
                }
                {
                    char tk[64]; snprintf(tk, sizeof tk, "tried E%d %s%s", T.E, T.same ? "same" : "cross", bestErr < 0 ? " none" : "");
                    stat[tk]++;
                }
                if (bestErr < 0) continue;
                tc.tiny.push_back(best);
                DT Zb;
                Eval(best, tx, ty, B, &Zb);
                int64_t Mb = 0;
                BelowM(B[T.j][T.i].d, &Mb);
                const auto &db = B[T.j][T.i].d;
                const int64_t diff = (db.k - dr.k);
                char key[96];
                snprintf(key, sizeof key, "E%d %s %s", E, T.same ? "same E" : "reg E-1 above", diff == 0 ? "=" : llabs(diff) <= 2 ? "+-1..2" : llabs(diff) <= 300 ? "+-3..300" : "far");
                stat[key]++;
                pairs++;
            }
            if (!tc.tiny.empty()) sets[set].push_back(tc);
        }
    }
    fprintf(stderr, "%ld pixel pairs\n", pairs);
    for (auto& e : stat) fprintf(stderr, "  %s: %ld\n", e.first.c_str(), e.second);
    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* written by gen.cpp: per set, per tile: x, y, the register triangle first (1) or the tiny ones, the register\n"
               "   triangle, the number of tiny ones and their offset into TINY; corners x y z as float words */\n#define NSETS %d\n", NSETS);
    size_t mx = 0, nt = 0;
    for (auto& s : sets) { mx = std::max(mx, s.size()); for (auto& t : s) nt += t.tiny.size(); }
    fprintf(f, "#define MAXTILES %zu\n#define NTINY %zu\nstatic const int NTILES[NSETS] = {", mx, nt);
    for (auto& s : sets) fprintf(f, " %zu,", s.size());
    fprintf(f, " };\n");
    auto tri = [&](const Tri& t) {
        fprintf(f, "{ { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu } }", t.x[0], t.x[1], t.x[2], t.y[0], t.y[1], t.y[2], t.z[0], t.z[1], t.z[2]);
    };
    fprintf(f, "static const tile_t TILES[NSETS][MAXTILES] = {\n");
    size_t off = 0;
    for (auto& s : sets) {
        fprintf(f, "  {\n");
        for (auto& t : s) { fprintf(f, "    { %d, %d, %d, ", t.tx, t.ty, (int)t.regFirst); tri(t.reg); fprintf(f, ", %zu, %zu },\n", t.tiny.size(), off); off += t.tiny.size(); }
        fprintf(f, "  },\n");
    }
    fprintf(f, "};\nstatic const tri_t TINY[NTINY] = {\n");
    for (auto& s : sets) for (auto& t : s) for (auto& q : t.tiny) { fprintf(f, "  "); tri(q); fprintf(f, ",\n"); }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
