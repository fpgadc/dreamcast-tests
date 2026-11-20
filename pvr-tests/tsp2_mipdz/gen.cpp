/* Cases for tsp2_mipdz: mipmapped triangles whose TSP determinant is zero (main.c).

       gen [--seed N]          ->  cases.h (here)

   Pages 0-1 are points (three equal corners): every sum of the setup is zero, the level's numerators too. Pages
   2-3 are almost points (covmeas mix tiny): three corners a few last places apart around the same place, kept only
   where the TSP's setup makes the determinant exactly zero (its products cut to 24 bits cancel) while some of the
   level's numerators are not zero (the corners' 1/w and U differ), and where the edge unit draws pixels on the row.
   For those 1/det = +0 (K a zero's, 2^(-127 + size)) and 1/det = infinity (K an infinity's, 2^(-128 + size)) give
   levels one apart where w is large enough: the 1/w of each slot is put there.
   Per slot (cell, j: row 2 j of the cell) the three corners' x, y, 1/w and U. */
#include "gen_common.h"
#include "pvr_tsp_plane.h"
#include "pvr_isp_edge.h"

using namespace gen;
using pvrfp::Fp;

typedef pvrtsp::PlaneT<pvrtsp::PlaneHW> Pl;

struct Slot { uint32_t x[3], y[3], z[3], u[3]; };

// rows[] of main.c: TexU, D adjust, DCalcCtrl
static const int TEXU[15] = { 3, 3, 3, 3, 3, 0, 0, 0, 5, 5, 5, 1, 2, 3, 3 };
static const int MIPD[15] = { 4, 4, 1, 15, 2, 4, 4, 15, 4, 4, 1, 4, 8, 4, 4 };
static const int DCALC[15] = { 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0 };

static pvrtsp::MipChoice Level(const Slot& s, const Fp& q, int row, int variant)
{
    static const pvrtsp::PlaneHW cfg;
    const uint32_t v[3] = { 0x3e800000u, 0x3e800000u, 0x3e800000u };
    Pl W, U, V;
    W.Setup(s.x, s.y, s.z, nullptr, cfg);
    U.Setup(s.x, s.y, s.z, s.u, cfg);
    V.Setup(s.x, s.y, s.z, v, cfg);
    const uint32_t first[5] = { s.u[0], v[0], s.z[0], s.x[0], s.y[0] };
    pvrtsp::mipDzVariant = variant;
    const auto m = pvrtsp::MipSelect(U, V, W, q, 3 + TEXU[row], MIPD[row], DCALC[row] ? first : nullptr);
    pvrtsp::mipDzVariant = 0;
    return m;
}

int main(int argc, char** argv)
{
    uint64_t seed = 1;
    for (int k = 1; k < argc; k++) {
        if (!strcmp(argv[k], "--seed") && k + 1 < argc) seed = strtoull(argv[++k], nullptr, 10);
        else Fail("gen [--seed N]");
    }
    Rng rng(seed);
    static const pvrtsp::PlaneHW cfg;
    static const pvrisp::EdgeConfig ecfg;
    std::vector<std::string> h;
    h.push_back("/* tsp2_mipdz: gen.cpp --seed " + std::to_string(seed) + " */");
    h.push_back("typedef struct { uint32_t x[3], y[3], z[3], u[3]; } dzslot_t;");
    h.push_back("static const dzslot_t slots[2][300 * 16] = {");
    int differ = 0, drawnTotal = 0, tries = 0, points = 0;
    for (int set = 0; set < 2; set++) {
        h.push_back("  {");
        for (int cell = 0; cell < 300; cell++)
            for (int j = 0; j < 16; j++) {
                const int row = cell / 20, col = cell % 20;
                const int tx = cell % 20, ty = cell / 20;
                // the slot's 1/w: 2^-(56 + col / 2) and a mantissa from j and the column's parity (points: every
                // corner; almost points: the mean, the corners 1, 1.5, 1.25 times a base)
                const double base = ldexp(1.0 + ((col & 1) * 16 + j) / 32.0, -(56 + col / 2));
                Slot s;
                const double px = CellX(cell) + 16.25, py = CellY(cell) + 2 * j + 0.25;
                if (set == 0) {
                    for (int k = 0; k < 3; k++) {
                        s.x[k] = Bits((float)px); s.y[k] = Bits((float)py); s.z[k] = Bits((float)(base * (row == 14 ? -1 : 1)));
                        s.u[k] = 0x3e800000u;
                    }
                } else {
                    static const double zm[3] = { 1.0, 1.5, 1.25 };
                    static const uint32_t u1[3] = { 0, 0x3f800000u, 0x3f000000u }, u2[3] = { 0, 0x49800000u, 0x35800000u };
                    bool ok = false;
                    for (int t = 0; t < 4000 && !ok; t++) {
                        tries++;
                        // a sliver: two corners 2^-14 .. 2^-12 above the row's pixel centres, the third as far below
                        // (or the other way), each moved by a few last places, so that the products of the setup,
                        // cut to 24 bits, can cancel to a determinant of exactly zero
                        const double y0 = CellY(cell) + 2 * j + 0.5;
                        const double e1 = ldexp(1.0, -(int)rng.integers(12, 15)), e2 = ldexp(1.0, -(int)rng.integers(12, 15));
                        const double sg = rng.chance(0.5) ? 1 : -1;
                        const double X[3] = { CellX(cell) + 2, CellX(cell) + 30, CellX(cell) + 16.25 };
                        const double Y[3] = { y0 - sg * e1, y0 - sg * e1, y0 + sg * e2 };
                        const int perm = (int)rng.integers(0, 6);
                        static const int P[6][3] = { { 0, 1, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 1, 0, 2 }, { 0, 2, 1 }, { 2, 1, 0 } };
                        for (int k = 0; k < 3; k++) {
                            const int c = P[perm][k];
                            s.x[k] = (uint32_t)((int32_t)Bits((float)X[c]) + (int32_t)rng.integers(-2, 3));
                            s.y[k] = (uint32_t)((int32_t)Bits((float)Y[c]) + (int32_t)rng.integers(-2, 3));
                            s.z[k] = Bits((float)(base * zm[k] * (row == 14 ? -1 : 1)));
                            s.u[k] = row == 13 ? u2[k] : u1[k];
                        }
                        Pl W;
                        W.Setup(s.x, s.y, s.z, nullptr, cfg);
                        if (!W.Sdet.zero() || W.Sdet.bad) continue;
                        // the edge unit: pixels of row 2 j of the tile
                        Fp fx[3], fy[3];
                        for (int k = 0; k < 3; k++) { fx[k] = Fp::FromFloat(FromBits(s.x[k])); fy[k] = Fp::FromFloat(FromBits(s.y[k])); }
                        bool bad;
                        const int sgn = pvrisp::Winding(fx, fy, ecfg, &bad);
                        if (bad) continue;
                        pvrisp::EdgeTile e[3];
                        for (int k = 0; k < 3; k++)
                            e[k].Setup(fx[k], fy[k], fx[(k + 1) % 3], fy[(k + 1) % 3], 64 * tx + 1, 64 * ty + 1, sgn, ecfg);
                        int drawn = 0, at = -1;
                        for (int i = 0; i < 32; i++)
                            if (e[0].Inside(i, 2 * j) && e[1].Inside(i, 2 * j) && e[2].Inside(i, 2 * j)) { drawn++; if (at < 0) at = i; }
                        if (drawn < 4) continue;
                        const Fp q = W.Value(2 * (int64_t)(tx * 32 + at) + 1, 2 * (int64_t)(ty * 32 + 2 * j) + 1, 0, tx * 32, ty * 32);
                        const auto a = Level(s, q, row, 1), b = Level(s, q, row, 2);
                        // slots where the two differ (after 15000 tries any)
                        if (a.level == b.level && a.k == b.k && t < 3000) continue;
                        differ += a.level != b.level || a.k != b.k;
                        drawnTotal += drawn;
                        ok = true;
                    }
                    if (!ok) {                          // none: a point, as on pages 0-1
                        points++;
                        for (int k = 0; k < 3; k++) {
                            s.x[k] = Bits((float)px); s.y[k] = Bits((float)py); s.z[k] = Bits((float)(base * zm[k] * (row == 14 ? -1 : 1)));
                        }
                    }
                }
                char b[400];
                snprintf(b, sizeof b, "    { { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu } },",
                         s.x[0], s.x[1], s.x[2], s.y[0], s.y[1], s.y[2], s.z[0], s.z[1], s.z[2], s.u[0], s.u[1], s.u[2]);
                h.push_back(b);
            }
        h.push_back("  },");
    }
    h.push_back("};");
    WriteLines(Here() + "/cases.h", h);
    fprintf(stderr, "slivers: %d of 4800 slots where the two readings of 1/det differ, %d pixels (by the model), %d tries, %d points instead\n", differ, drawnTotal, tries, points);
    return 0;
}
