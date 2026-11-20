/* Cases for tsp2_zadd: how the TSP's setup adds a zero (main.c).

       gen [--seed N]          ->  cases.h (here)

   The ISP's depth setup adds a zero as "+0 against 2^-126 .. 1.5 2^-126 of the other sign, otherwise exact, and
   every zero sum +0" (refsw3 pvr_isp_depth.h, dreamcast-tests depthgaps_zadd); the TSP's planes (pvr_tsp_plane.h
   Setup) had "a zero is 2^-127 with its sign in a sum's last addition, a zero in the first, -0 + -0 = -0", fitted
   on the sign of 26 slopes that underflow. Here the slope of a U / w plane where the two differ, with the slope
   itself in range, so that its magnitude shows: a sliver of determinant about 1 on one row of a tile. Two corners
   (the base) on one line y = yb, the third (the apex) at ya, |ya - yb| = dy; the row's pixel centres lie between,
   near the base. The apex's term of the x slope's sum, U_a z (Y_j - Y_l), is then a zero with the sign of U_a z
   (Y_j - Y_l = +0), and the two others are U z (+-dy). 1/w is z at every corner (its slopes cancel to +0).
     class 0  apex third (the zero in the last addition): t0 + t1 = (U1 - U0) z dy of 1 .. 2 2^-126, either sign
     class 1  apex first or second (the zero in the first addition), the other first term of 1 .. 2 2^-126
     class 2  two zeros: a product below 2^-126 (with its sign) and the apex's zero, in either addition
     class 3  controls: the same shapes with nothing at 2^-126 (both rules agree)
   Every case is kept only if the two rules give different 16 bit coefficients (classes 0-2) or the same (3); the
   picture then shows U = (U / w) / (1 / w) along the row through the 1024 texel number texture (uvmeas). */
#include "gen_common.h"
#include "pvr_tsp_plane.h"

using namespace gen;
using pvrfp::Fp;

static const int PAGES = 2, ROWS = 16, CELLS = 300;

struct Case { uint32_t x[3], y[3], z, u[3]; int cls; };

static bool Coefs(const Case& c, int rule, Fp out[3])
{
    static const pvrtsp::PlaneHW cfg;
    pvrtsp::tspZeroVariant = rule;
    pvrtsp::PlaneT<pvrtsp::PlaneHW> U;
    const uint32_t z[3] = { c.z, c.z, c.z };
    U.Setup(c.x, c.y, z, c.u, cfg);
    out[0] = U.A; out[1] = U.B; out[2] = U.C;
    pvrtsp::tspZeroVariant = 0;
    return !U.bad;
}

static bool SameFp(const Fp& a, const Fp& b) { return a.bad == b.bad && a.neg == b.neg && a.toDouble() == b.toDouble(); }

int main(int argc, char** argv)
{
    uint64_t seed = 1;
    for (int k = 1; k < argc; k++) {
        if (!strcmp(argv[k], "--seed") && k + 1 < argc) seed = strtoull(argv[++k], nullptr, 10);
        else Fail("gen [--seed N]");
    }
    Rng rng(seed);
    std::vector<std::string> h;
    h.push_back("/* tsp2_zadd: gen.cpp --seed " + std::to_string(seed) + " */");
    h.push_back("typedef struct { uint32_t x[3], y[3], z, u[3]; uint8_t cls; } zcase_t;");
    h.push_back("static const zcase_t cases[" + std::to_string(PAGES * CELLS * ROWS) + "] = {");
    int counts[4] = { 0, 0, 0, 0 }, tries = 0;
    for (int p = 0; p < PAGES; p++)
        for (int cell = 0; cell < CELLS; cell++)
            for (int r = 0; r < ROWS; r++) {
                const int want = (r % 8 == 7) ? 3 : (r % 7) % 3;        // about 2 / 7 of each class 0-2, 1 / 8 controls
                Case c;
                for (;;) {
                    tries++;
                    const double cx = CellX(cell), cy = CellY(cell), y0 = cy + 2 * r + 0.5;
                    const int dyExp = p == 0 ? -3 : -(int)rng.integers(2, 5);
                    const double dy = ldexp(1.0, dyExp), eps = ldexp(1.0, dyExp - 5);
                    const bool below = rng.chance(0.5);                 // the apex below the base (larger y)
                    const double yb = below ? y0 - eps : y0 + eps, ya = below ? y0 - eps + dy : y0 + eps - dy;
                    const int ze = p == 0 ? (int)rng.integers(-116, -111) : (int)rng.integers(-118, -107);
                    const double z = (p == 1 && rng.chance(0.25) ? -1 : 1) * ldexp(1.0, ze) * (rng.chance(0.5) ? 1.0 : 1.0 + rng.integers(1, 16) / 16.0);
                    const int cls = want;
                    const int a = (cls == 0 || cls == 3) ? 2 : (cls == 1 ? (int)rng.integers(0, 2) : (int)rng.integers(0, 3));
                    const bool swapBase = rng.chance(0.5);              // the winding
                    int bi[2] = { -1, -1 }, n = 0;
                    for (int k = 0; k < 3; k++) if (k != a) bi[n++] = k;
                    if (swapBase) std::swap(bi[0], bi[1]);
                    double X[3], Y[3], U[3];
                    X[a] = cx + 6.25; Y[a] = ya;
                    X[bi[0]] = cx + 2; Y[bi[0]] = yb;
                    X[bi[1]] = cx + 10; Y[bi[1]] = yb;
                    // s_k = z (Y_j - Y_l): the factor of U_k in the x slope's sum
                    double s[3];
                    for (int k = 0; k < 3; k++) s[k] = z * (Y[(k + 1) % 3] - Y[(k + 2) % 3]);
                    auto mant = [&](int bits) { return 1.0 + (double)rng.integers(0, 1 << bits) / (1 << bits); };
                    const double v = ldexp(1.0, -126) * (rng.chance(0.4) ? 1.0 + rng.integers(0, 8) / 8.0 : mant(6));   // 1 .. 2 2^-126
                    const double vs = rng.chance(0.5) ? 1 : -1;
                    const double base = mant(8) * (rng.chance(0.5) ? 1 : 2);   // an ordinary U: 1 .. 4
                    U[a] = (rng.chance(0.3) ? -1 : 1) * (rng.chance(0.5) ? base : mant(8));
                    if (cls == 0 || cls == 3) {
                        // U0 s0 + U1 s1 with s1 = -s0: (U1 - U0) s1 = vs v (class 3: 4 .. 64 times that)
                        const int i0 = 0, i1 = 1;
                        const double t = vs * v * (cls == 3 ? ldexp(1.0, (int)rng.integers(2, 7)) : 1.0);
                        U[i0] = base;
                        U[i1] = base + t / s[i1];
                    } else if (cls == 1) {
                        const int b = 1 - a;                            // the other operand of the first addition
                        U[b] = vs * v / s[b];
                        U[2] = base;
                    } else {
                        // a product below 2^-126 (with its sign) and the apex's zero; the third ordinary or tiny too
                        int o[2], m = 0;
                        for (int k = 0; k < 3; k++) if (k != a) o[m++] = k;
                        const int tiny = o[rng.integers(0, 2)], other = o[0] == tiny ? o[1] : o[0];
                        U[tiny] = (rng.chance(0.5) ? 1 : -1) * ldexp(1.0, -127) * mant(4) / fabs(s[tiny]);
                        U[other] = rng.chance(0.5) ? (rng.chance(0.5) ? 1 : -1) * ldexp(1.0, -127) * mant(4) / fabs(s[other]) : base;
                    }
                    for (int k = 0; k < 3; k++) {
                        c.x[k] = Bits((float)X[k]); c.y[k] = Bits((float)Y[k]); c.u[k] = Bits((float)U[k]);
                        if ((double)FromBits(c.u[k]) != U[k] || !std::isfinite(U[k]) || fabs(U[k]) > 64) goto again;
                    }
                    c.z = Bits((float)z);
                    c.cls = cls;
                    {
                        Fp f0[3], f1[3];
                        if (!Coefs(c, 0, f0) || !Coefs(c, 1, f1)) goto again;
                        bool same = true;
                        for (int k = 0; k < 3; k++) same = same && SameFp(f0[k], f1[k]);
                        if ((cls == 3) != same) goto again;
                    }
                    break;
                again:;
                }
                counts[c.cls]++;
                char line[300];
                snprintf(line, sizeof line, "    { { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, 0x%08xu, { 0x%08xu, 0x%08xu, 0x%08xu }, %d },",
                         c.x[0], c.x[1], c.x[2], c.y[0], c.y[1], c.y[2], c.z, c.u[0], c.u[1], c.u[2], c.cls);
                h.push_back(line);
            }
    h.push_back("};");
    WriteLines(Here() + "/cases.h", h);
    fprintf(stderr, "classes: %d %d %d %d (%d tries)\n", counts[0], counts[1], counts[2], counts[3], tries);
    return 0;
}
