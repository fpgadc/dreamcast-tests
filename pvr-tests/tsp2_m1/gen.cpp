/* Cases for tsp2_m1: the mipmap level, U and the colours at a pixel whose 1/w is exactly -1 grid unit (main.c).

       gen          ->  cases.h (here); gen -v: per cell what refsw3 has at the first pixel

   texfar_q256 / texfar_m1's triangle: TL (x0, y0 + 15) z, TR (x0 + 64, y0 + 15) z t, BL (x0, y0 + 16) -z, so that
   1/w at the first pixel of the cell's line 15 is exactly -1 unit of the plane's grid (all ones: Plane::Value's
   -2^-143). The divider reads it as 0xFFFF 2^-143 (texfar_m1); what the mipmap level makes of it (MipSelect's w
   is Divide(1, q)) and what a colour's quotient is there were open. Here t is searched per row for that -1 unit
   (1 - 2^-21 for texfar_m1's rows; tiny 1/w need another), and per cell the attributes:
     rows 0-4   U and V flat (the level from w alone: K = 2^(-127 + size)), z = 2^k
     rows 5-9   U sloped along x by s per cell (K from about 2^-126 up), the same z
     rows 10-14 Gouraud colours (a colour per corner, columns 14-19 one colour at all three), z = 2^k with k =
                -100, -60, -20, 30, 100: the colour / w at the first pixel is many units of its own grid, so the
                quotient is 0 or 255 by its sign (an 8 bit colour with slopes in range does not get it below 2^-127
                there); -1 unit of it too where the colour is one at all three corners */
#include "gen_common.h"
#include "pvr_tsp_plane.h"

using namespace gen;
using pvrfp::Fp;

static const int KROW[15] = { -20, 0, 30, 60, 100, -20, 0, 30, 60, 100, -100, -60, -20, 30, 100 };
// TR's x offset: 64 (texfar_m1's), 4 on the colour rows (so that the x slopes of tiny planes stay in range)
static int TRDX(int cell) { (void)cell; return 64; }

struct Probe { Fp w, num; };
// the 1/w plane (attr null) or an attribute plane at the first pixel of the cell's line 15
static Fp At(int cell, const uint32_t z[3], const uint32_t* attr)
{
    const int cx = (cell % 20) * 32, cy = (cell / 20) * 32;
    const float X[3] = { (float)cx, cx + (float)TRDX(cell), (float)cx }, Y[3] = { cy + 15.0f, cy + 15.0f, cy + 16.0f };
    static const pvrtsp::PlaneHW cfg;
    uint32_t sx[3], sy[3];
    for (int i = 0; i < 3; i++) { sx[i] = Bits(X[i]); sy[i] = Bits(Y[i]); }
    pvrtsp::PlaneT<pvrtsp::PlaneHW> P;
    P.Setup(sx, sy, z, attr, cfg);
    return P.Value(2 * (int64_t)cx + 1, 2 * (int64_t)(cy + 15) + 1, 0, cx & ~31, cy & ~31);
}
static bool MinusOne(const Fp& v) { return v.neg && v.mant == 1 && v.exp == -143; }

typedef pvrtsp::PlaneT<pvrtsp::PlaneHW> Pl;
static Pl Plane(int cell, const uint32_t z[3], const uint32_t* attr)
{
    const int cx = (cell % 20) * 32, cy = (cell / 20) * 32;
    const float X[3] = { (float)cx, cx + (float)TRDX(cell), (float)cx }, Y[3] = { cy + 15.0f, cy + 15.0f, cy + 16.0f };
    static const pvrtsp::PlaneHW cfg;
    uint32_t sx[3], sy[3];
    for (int i = 0; i < 3; i++) { sx[i] = Bits(X[i]); sy[i] = Bits(Y[i]); }
    Pl P;
    P.Setup(sx, sy, z, attr, cfg);
    return P;
}
static Fp First(int cell, const Pl& P)
{
    const int cx = (cell % 20) * 32, cy = (cell / 20) * 32;
    return P.Value(2 * (int64_t)cx + 1, 2 * (int64_t)(cy + 15) + 1, 0, cx & ~31, cy & ~31);
}
// the column's texture size (TexU), D adjust and DCalcCtrl (rows 0-9)
static void Column(int c, int& texU, int& mipD, int& dcalc)
{
    static const int D[3] = { 1, 4, 15 };
    texU = c < 18 ? c % 6 : (c == 18 ? 3 : 0);
    mipD = c < 18 ? D[c / 6] : (c == 18 ? 4 : 15);
    dcalc = c >= 18;
}
static pvrtsp::MipChoice Mip(int cell, const uint32_t z[3], const uint32_t u[3], const uint32_t v[3])
{
    int texU, mipD, dcalc;
    Column(cell % 20, texU, mipD, dcalc);
    const Pl W = Plane(cell, z, nullptr), U = Plane(cell, z, u), V = Plane(cell, z, v);
    const int cx = (cell % 20) * 32, cy = (cell / 20) * 32;
    const uint32_t first[5] = { u[0], v[0], z[0], Bits((float)cx), Bits(cy + 15.0f) };
    return pvrtsp::MipSelect(U, V, W, First(cell, W), 3 + texU, mipD, dcalc ? first : nullptr);
}

int main(int argc, char**)
{
    const bool verbose = argc > 1;
    std::vector<std::string> h;
    h.push_back("/* tsp2_m1: gen.cpp */");
    h.push_back("static const uint32_t Z[300][3] = {");
    std::vector<std::string> al;
    for (int cell = 0; cell < 300; cell++) {
        const int row = cell / 20, k = KROW[row];
        const uint32_t a = (uint32_t)(127 + k) << 23;
        uint32_t z[3] = { 0, 0, 0 };
        bool found = false;
        // TR = z t: t = 1 - m 2^-23 for m = 4 (texfar_m1's 2^-21), then any other m
        for (int m = 4; m < 4096 && !found; m++) {
            const uint32_t zz[3] = { a, a - (uint32_t)m, a | 0x80000000u };
            if (MinusOne(At(cell, zz, nullptr))) { memcpy(z, zz, sizeof z); found = true; }
        }
        // tiny 1/w: TR = z (1 - m 2^-23) with m large enough that the x slope stays in range, BL = -(z + n ulp):
        // a bisection for where the pixel's 1/w changes sign, then a scan around it (it is not monotone at the
        // grid's scale)
        auto key = [](const Fp& v) { return v.zero() ? 0.0 : (v.neg ? -1.0 : 1.0) * ldexp((double)v.mant, v.exp + 200); };
        for (int m = 1 << 15; m <= (1 << 20) && !found; m *= 2) {
            auto zs = [&](int64_t n, uint32_t* zz) { zz[0] = a; zz[1] = a - (uint32_t)m; zz[2] = (uint32_t)((int64_t)a + n) | 0x80000000u; };
            uint32_t zz[3];
            int64_t lo = -(1 << 22), hi = 1 << 22;
            zs(lo, zz); const double vlo = key(At(cell, zz, nullptr));
            zs(hi, zz); const double vhi = key(At(cell, zz, nullptr));
            if (!(vlo > 0 && vhi < 0)) continue;
            while (hi - lo > 1) {
                const int64_t mid = (lo + hi) / 2;
                zs(mid, zz);
                if (key(At(cell, zz, nullptr)) >= 0) lo = mid; else hi = mid;
            }
            for (int64_t d = 0; d < (1 << 16) && !found; d++)
                for (int sgn = -1; sgn <= 1 && !found; sgn += 2) {
                    zs(lo + sgn * d, zz);
                    if (MinusOne(At(cell, zz, nullptr))) { memcpy(z, zz, sizeof z); found = true; }
                }
        }
        if (!found) { fprintf(stderr, "cell %d: no -1 unit\n", cell); z[0] = z[1] = z[2] = 0; }
        char b[400];
        snprintf(b, sizeof b, "    { 0x%08xu, 0x%08xu, 0x%08xu },", z[0], z[1], z[2]);
        h.push_back(b);
        // the attributes
        uint32_t u[3] = { 0x3e800000u, 0x3e800000u, 0x3e800000u }, v[3] = { 0x3e800000u, 0x3e800000u, 0x3e800000u };
        uint32_t col[3] = { 0, 0, 0 }, ofs[3] = { 0, 0, 0 };
        char what[200] = "";
        if (row < 10) {
            int texU, mipD, dcalc;
            Column(cell % 20, texU, mipD, dcalc);
            if (row >= 5) {
                // U at TR 0.25 + s: s searched so that the level at the first pixel is the last - (column % 4), or as
                // near as it gets, with the fraction varying
                const int last = 3 + texU, want = last - (cell % 4);
                int best = 1 << 30;
                uint32_t bestU = u[1];
                for (int e = -140; e <= 40; e++)
                    for (int f = 0; f < 8; f++) {
                        const double sv = ldexp(1.0 + f / 8.0 + (cell % 7) / 64.0, e) + 0.25;
                        const uint32_t uu[3] = { u[0], Bits((float)sv), u[2] };
                        if (!std::isfinite((float)sv)) continue;
                        const auto m = Mip(cell, z, uu, v);
                        const int score = abs(m.level - want) * 1000 + abs(m.k - 37 * (cell % 7));
                        if (score < best) { best = score; bestU = uu[1]; }
                    }
                u[1] = bestU;
            }
            const auto m = Mip(cell, z, u, v);
            snprintf(what, sizeof what, "TexU %d D %d dcalc %d: level %d k %d (K %c%llx*2^%d)", texU, mipD, dcalc, m.level, m.k,
                     m.kFull.neg ? '-' : '+', (unsigned long long)m.kFull.mant, m.kFull.exp);
        } else {
            // colours: random at every corner (columns 0-13), or one colour at all three (14-19: the colour / w is
            // the colour times the 1/w plane, -1 or -2 units of its grid at the first pixel)
            Rng rng(1000 + cell);
            for (int i = 0; i < 3; i++) {
                col[i] = (uint32_t)rng.next(); ofs[i] = (uint32_t)rng.next();
                if (cell % 20 >= 14 && i) { col[i] = col[0]; ofs[i] = ofs[0]; }
            }
            int bestScore = 0;
            const Fp q = First(cell, Plane(cell, z, nullptr));
            std::string pred;
            for (int which = 0; which < 2; which++)
                for (int ch = 0; ch < 4; ch++) {
                    const uint32_t* src = which ? ofs : col;
                    uint32_t at[3];
                    for (int i = 0; i < 3; i++) at[i] = pvrfp::SingleBits((src[i] >> (8 * ch)) & 255, -8);
                    const Fp num = First(cell, Plane(cell, z, at));
                    const int got = pvrtsp::ColorRounded(pvrtsp::Divide(num, q), -8, 255, 0);
                    bestScore += got > 0 && got < 255;
                    char t[64];
                    snprintf(t, sizeof t, " %d(%s%s)", got, num.neg ? "-" : "+", MinusOne(num) ? "1u" : "");
                    pred += t;
                }
            snprintf(what + 0, sizeof what, "%s", pred.c_str());
            (void)bestScore;
        }
        snprintf(b, sizeof b, "    { { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu } },",
                 u[0], u[1], u[2], v[0], v[1], v[2], col[0], col[1], col[2], ofs[0], ofs[1], ofs[2]);
        al.push_back(b);
        if (verbose) printf("cell %3d k %4d: TR %08x BL %08x  %s\n", cell, k, z[1], z[2], what);
    }
    h.push_back("};");
    h.push_back("static const struct { uint32_t u[3], v[3], col[3], ofs[3]; } ATT[300] = {");
    for (auto& l : al) h.push_back(l);
    h.push_back("};");
    WriteLines(Here() + "/cases.h", h);
    return 0;
}
