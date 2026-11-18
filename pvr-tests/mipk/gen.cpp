/* mipk's cases (pvr-tests/mipk/main.c, mipmeas's program with these tables): the precision of K, the part of the
   mipmap level that belongs to the polygon (pvrtsp::MipSelect, refsw3-core pvr_tsp_plane.h).

   d = K * w^2 in arithmetic of 8 bits, K = the longer of the two lengths (along x, along y) of the numerators
   |A_a C_w - C_a A_w| (a = u, v), each length the longer numerator plus 27146 / 65536 of the shorter, times the
   texture's size and the bias, cut to 8 bits. A dump of Ready 2 Rumble (polly2-data r2r_ring) has a K of
   66.0000062: 2^-23 above a step of 8 bits, where the console takes the level below at the six pixels whose d the
   model makes exactly a power of two. How K loses its precision on the way is the question.

   Each case is one triangle in a cell of its own, corners (0, 0), (a, 0), (0, b) of the cell (a, b = 32, or 20 .. 32
   on pages 6 and 7), 1/w sloped across it (so that w, w^2 and d sweep through a range and the change of level
   crosses the triangle), the texture at any angle or along the axes (then one numerator of a direction is 0). One
   corner's u is moved by single steps of its float until K lies just above a step of 8 bits (pages 0, 2, 4, 6)
   or just below one (1, 3, 5, 7), by 2^-18 .. 2^-5 of a step; kept when K one step lower (or higher) changes the
   level of at least 3 covered pixels: the pixel where the level changes says which K the console has.

     page 0, 1  DCalcCtrl 0, bias 4/4      page 2, 3  DCalcCtrl 1, bias 4/4      page 4, 5  DCalcCtrl 0, bias 5 .. 7
     page 6, 7  DCalcCtrl 0 or 1, legs 20 .. 32 (determinants not a power of two)

   Plain bilinear filtering (FilterMode 1): a pixel shows its level's colour. Out: cases.h (mipmeas's format),
   cases.txt (a line a case: K, its distance from the step in steps, the two lengths). Build: bash model/build.sh mipk;
   run: pvr-tests/mipk/gen [seed] */
#include "gen_common.h"

using namespace gen;

static const int PAGES = 8, CELLS = 300, SIZE_LOG2 = 6;    // mipmeas's texture: 64 x 64

struct Tri {
    double x[3], y[3], z[3], u[3], v[3];
    int bias, dcalc;
};

static pvrtsp::MipChoice Choice(const Model& m, const Tri& t, float W)
{
    const float fx[3] = { (float)t.x[0], (float)t.x[1], (float)t.x[2] }, fy[3] = { (float)t.y[0], (float)t.y[1], (float)t.y[2] };
    const float fz[3] = { (float)t.z[0], (float)t.z[1], (float)t.z[2] };
    const float fu[3] = { (float)t.u[0], (float)t.u[1], (float)t.u[2] }, fv[3] = { (float)t.v[0], (float)t.v[1], (float)t.v[2] };
    pvrtsp::Plane pu, pv, pw;
    pu.Setup(fx, fy, fz, fu, m.plane);
    pv.Setup(fx, fy, fz, fv, m.plane);
    pw.Setup(fx, fy, fz, nullptr, m.plane);
    const float first[5] = { fu[0], fv[0], fz[0], fx[0], fy[0] };
    return pvrtsp::MipSelect(pu, pv, pw, pvrfp::Fp::FromFloat(W), SIZE_LOG2, t.bias, t.dcalc ? first : nullptr);
}

// K of the triangle (the model's, before its cut to 8 bits)
static double KOf(const Model& m, const Tri& t)
{
    pvrtsp::mipKVariant = 0;
    return Choice(m, t, (float)t.z[0]).kFull;
}

// the level of every covered pixel (-1: not covered), with the variant given
static std::vector<int> Levels(const Model& m, const Tri& t, int cx, int cy, int variant)
{
    pvrtsp::mipKVariant = variant;
    std::vector<int> out(1024, -1);
    for (int j = 0; j < 32; j++)
        for (int i = 0; i < 32; i++) {
            const double px = cx + i + 0.5, py = cy + j + 0.5;
            if (!Covered(m, t.x, t.y, 3, px, py))
                continue;
            const int start = SpanStart(m, t.x, t.y, px, py);
            const Fp w = TspPlane(m, t.x, t.y, t.z, nullptr, px, py, start);
            if (w.bad || w.zero())
                continue;
            out[j * 32 + i] = Choice(m, t, (float)w.toDouble()).level;
        }
    pvrtsp::mipKVariant = 0;
    return out;
}

// K's place between steps of 8 bits: 0 .. 1 (0: on a step)
static double StepFraction(double k)
{
    int e;
    const double mant = frexp(k, &e) * 256;         // 128 .. 256
    return mant - floor(mant);
}

int main(int argc, char** argv)
{
    const uint64_t seed = argc > 1 ? strtoull(argv[1], 0, 0) : 1;
    // mix exact (pvr-tests/mipk2): 1/w flat, legs 32 x 32, u = v = 0 at the first corner, so that every numerator is
    // exact (1/w a power of two: pages 0 .. 5) or one product (6, 7): what is left of K's arithmetic is the length's
    // (the constant of the shorter one, its cuts, the adder). The whole triangle has one w, so one step of K8 is
    // seen: the first whose K8 * w^2 reaches 2^15 (130 for a power of two). Pages 0, 1: the shorter numerator 0;
    // 2, 3: shorter / longer 0.02 .. 1; 4, 5: 0.5 .. 1; 6, 7: 1/w not a power of two, the shorter 0. Even pages above
    // that step, odd below.
    // mix stair (pvr-tests/mipk3): exact's cells (1/w = 1, so one step of K8 seen: 130) in rows of 20 that share the
    // shorter numerator: across a row the longer one rises by one unit of its 24 bit grid a cell (one step of its float),
    // around the model's crossing of the step; where the console's row turns says f(shorter) to a unit. 15 rows a page,
    // shorter / longer from 0.005 to 0.99 (log), the shorter a float of any mantissa or of few bits
    const bool stair = argc > 2 && !strcmp(argv[2], "stair");
    const bool exact = stair || (argc > 2 && !strcmp(argv[2], "exact"));
    Tri row;                                            // stair: the row's cell 10, at the model's crossing
    const std::string outDir = argc > 3 ? argv[3] : Here();
    Rng rng(seed);
    Model model;
    std::vector<std::string> h, txt;
    h.push_back(Format("/* Generated by pvr-tests/mipk/gen %llu%s - do not edit. Floats are bit patterns. */", (unsigned long long)seed, exact ? " exact" : ""));
    h.push_back("#include <stdint.h>");
    h.push_back(Format("#define NUM_QUADS %d", PAGES * CELLS));
    h.push_back("typedef struct { unsigned char page, bias, mode, dcalc, tri[3][2]; unsigned short cell; uint32_t z[4]; uint32_t uv[4][2]; } quad_t;   /* corners TL, TR, BL, BR; tri (not all zero): a triangle with these corners, in pixels from the cell's, instead */");
    h.push_back("static const quad_t quads[NUM_QUADS] = {");
    txt.push_back(stair ? "# page cell dcalc bias legs K row step(-10..9: the longer's units from the model's crossing) 0 0"
                        : "# page cell dcalc bias legs K above(1)/below(0) distance-from-the-step(steps) K8-1-changes K8+1-changes");
    const double Pi = 3.14159265358979323846;
    long tries = 0;
    for (int page = 0; page < PAGES; page++) {
        const bool above = page % 2 == 0;
        for (int cell = 0; cell < CELLS; cell++) {
            const int cx = (cell % 20) * 32, cy = (cell / 20) * 32;
            Tri t;
            int a = 32, b = 32;
            for (;;) {
                if (++tries > 50000000)
                    Fail("no case");
                if (stair && cell % 20 != 0) {
                    t = row;
                    t.x[0] = cx; t.y[0] = cy; t.x[1] = cx + 32; t.y[1] = cy; t.x[2] = cx; t.y[2] = cy + 32;
                    t.u[1] = Step(row.u[1], cell % 20 - 10);
                    const double k = KOf(model, t);
                    txt.push_back(Format("%d %d %d %d 32x32 %.12g %d %d %d %d", page, cell, t.dcalc, t.bias, k, cell / 20, cell % 20 - 10, 0, 0));
                    break;
                }
                if (exact) {
                    t.dcalc = (int)rng.integers(0, 2);
                    t.bias = 4;
                    t.x[0] = cx; t.y[0] = cy; t.x[1] = cx + 32; t.y[1] = cy; t.x[2] = cx; t.y[2] = cy + 32;
                    const double z = page < 6 ? ldexp(1.0, (int)rng.integers(-1, 2)) : F32(rng.uniform(0.5, 2.0));
                    for (int k = 0; k < 3; k++) t.z[k] = z;
                    // w^2 of this 1/w, 8 bits, and the step of K8 that changes the level: the first m with m * ww >= 2^15
                    const Fp w8 = pvrtsp::Divide(Fp::FromHalves(2), Fp::FromFloat((float)z)).cut(8);
                    Fp ww = pvrfp::Mul(w8, w8, 8);
                    const int wb = Fp::Bits(ww.mant), wwm = (int)(wb >= 8 ? ww.mant >> (wb - 8) : ww.mant << (8 - wb));
                    const int mT = (32768 + wwm - 1) / wwm;
                    if (mT < 129 || mT > 255) continue;
                    double ratio = page < 2 || page >= 6 ? 0 : page < 4 ? rng.logUniform(0.02, 1) : rng.uniform(0.5, 1);
                    if (stair) {
                        ratio = rng.logUniform(0.005, 0.99);
                        for (int k = 0; k < 3; k++) t.z[k] = 1;
                    }
                    const double s1 = rng.chance(0.5) ? 1 : -1, s2 = rng.chance(0.5) ? 1 : -1;
                    t.u[0] = 0; t.v[0] = 0;
                    t.u[1] = s1; t.v[1] = s2 * ratio;
                    if (stair && rng.chance(0.5))       // a shorter of few bits
                    {
                        const int fb = (int)rng.integers(4, 12);
                        const double few = ldexp(round(ldexp(ratio, fb)), -fb);
                        if (few > 0) t.v[1] = s2 * few;
                    }
                    t.u[2] = rng.uniform(-0.3, 0.3); t.v[2] = rng.uniform(-0.3, 0.3);
                    if (!stair && rng.chance(0.5)) { std::swap(t.u[1], t.v[1]); std::swap(t.u[2], t.v[2]); }     // the longer from v
                    const double k1 = KOf(model, t);
                    if (!(k1 > 0)) continue;
                    // scale every coordinate so that K lies a little under mT steps of 2^s, with d = K * w^2 near 2^L (level
                    // L = 1 .. 4: a level change there is one of mipmeas's colours to another)
                    const int L = (int)rng.integers(1, 5);
                    const int sK = (int)lround(L - log2(mT * ww.toDouble()));
                    const double want = (mT - rng.uniform(0.1, 0.7)) * ldexp(1.0, sK);
                    const double f = want / k1;
                    for (int k = 1; k < 3; k++) { t.u[k] = F32(t.u[k] * f); t.v[k] = F32(t.v[k] * f); }
                } else {
                if (page >= 6) { a = (int)rng.integers(20, 33); b = (int)rng.integers(20, 33); }
                t.dcalc = page == 2 || page == 3 ? 1 : page >= 6 ? (int)rng.integers(0, 2) : 0;
                t.bias = page == 4 || page == 5 ? (int)rng.integers(5, 8) : 4;
                t.x[0] = cx; t.y[0] = cy; t.x[1] = cx + a; t.y[1] = cy; t.x[2] = cx; t.y[2] = cy + b;
                // 1/w: a base and a slope (w sweeps through some 10 to 40 percent across the triangle)
                const double z0 = rng.logUniform(0.3, 4);
                const double gx = rng.uniform(-0.2, 0.2) / 32, gy = rng.uniform(-0.2, 0.2) / 32;
                for (int k = 0; k < 3; k++)
                    t.z[k] = F32(z0 * (1 + gx * (t.x[k] - cx) + gy * (t.y[k] - cy)));
                // texels a pixel near a change of level (2^(L) .. 2^(L + 1) for L = 1, 2, 3), at an angle or along the axes
                const double rate = ldexp(1.0, (int)rng.integers(0, 3)) * rng.uniform(1.0, 2.0) / 64;
                const double th = rng.chance(0.3) ? (rng.chance(0.5) ? 0 : Pi / 2) : rng.uniform(0, 2 * Pi);
                const double aniso = rng.chance(0.5) ? 1 : rng.uniform(0.3, 1);
                const double u0 = rng.uniform(0, 1), v0 = rng.uniform(0, 1);
                for (int k = 0; k < 3; k++) {
                    const double dx = t.x[k] - cx, dy = t.y[k] - cy;
                    t.u[k] = F32(u0 + (dx * cos(th) - dy * sin(th)) * rate);
                    t.v[k] = F32(v0 + (dx * sin(th) + dy * cos(th)) * rate * aniso);
                }
                }
                // move u of corner 1 by single steps until K just crosses the next step of 8 bits
                const double k0 = KOf(model, t);
                const bool dbg = getenv("GEN_DEBUG") && tries < 30;
                if (dbg) printf("try %ld: z %g u1 %g v1 %g u2 %g v2 %g K %.9g\n", tries, t.z[0], t.u[1], t.v[1], t.u[2], t.v[2], k0);
                if (!(k0 > 0) || !std::isfinite(k0))
                    continue;
                int e;
                frexp(k0, &e);
                const double step = ldexp(1.0, e - 8);
                const double target = (floor(k0 / step) + 1) * step;          // the step above
                const bool moveV = exact && fabs(t.v[1]) > fabs(t.u[1]);
                auto kAt = [&](int64_t n) {
                    Tri q = t;
                    if (moveV) q.v[1] = Step(t.v[1], (int)n); else q.u[1] = Step(t.u[1], (int)n);
                    return KOf(model, q);
                };
                const int dir = kAt(1 << 12) > k0 ? 1 : -1;
                int64_t lo = 0, hi = 1;
                while (hi < (1 << 26) && kAt(dir * hi) < target)
                    hi *= 2;
                if (hi >= (1 << 26)) { if (dbg) printf("  no crossing of %.9g (dir %d)\n", target, dir); continue; }
                while (hi - lo > 1) {
                    const int64_t mid = (lo + hi) / 2;
                    (kAt(dir * mid) < target ? lo : hi) = mid;
                }
                // hi: the first above the step; walk away from it to the distance wanted
                const double perStep = (kAt(dir * hi) - kAt(dir * lo)) / step;
                if (!(perStep > 0))
                    continue;
                if (stair) {
                    row = t;
                    row.u[1] = Step(t.u[1], (int)(dir * hi));
                    t = row;
                    t.u[1] = Step(row.u[1], -10);                // the row's first cell
                    const double k = KOf(model, t);
                    txt.push_back(Format("%d %d %d %d 32x32 %.12g %d %d %d %d", page, cell, t.dcalc, t.bias, k, cell / 20, cell % 20 - 10, 0, 0));
                    break;
                }
                const double want = rng.logUniform(ldexp(1.0, -18), ldexp(1.0, -5));
                const int64_t moves = (int64_t)floor(want / perStep);
                const int64_t n = above ? hi + moves : lo - moves;
                Tri q = t;
                if (moveV) q.v[1] = Step(t.v[1], (int)(dir * n)); else q.u[1] = Step(t.u[1], (int)(dir * n));
                const double k = KOf(model, q);
                const double frac = StepFraction(k);
                const double dist = above ? frac : 1 - frac;
                if (dist > ldexp(1.0, -4)) { if (dbg) printf("  too far: K %.9g dist %g\n", k, dist); continue; }
                // the level of the pixels with K, with K one step lower and one step higher
                const auto l0 = Levels(model, q, cx, cy, 0), lm = Levels(model, q, cx, cy, 99), lp = Levels(model, q, cx, cy, 98);
                int nm = 0, np = 0, covered = 0;
                for (int p = 0; p < 1024; p++) {
                    covered += l0[p] >= 0;
                    nm += l0[p] != lm[p];
                    np += l0[p] != lp[p];
                }
                if (covered < 100 || (above ? nm : np) < 3) { if (dbg) printf("  K %.9g dist %g covered %d nm %d np %d\n", k, dist, covered, nm, np); continue; }
                t = q;
                txt.push_back(Format("%d %d %d %d %dx%d %.12g %d %.6g %d %d", page, cell, t.dcalc, t.bias, a, b, k, above ? 1 : 0, dist, nm, np));
                break;
            }
            h.push_back(Format("    { %d, %d, 1, %d, { { 0, 0 }, { %d, 0 }, { 0, %d } }, %d, { %s, %s, %s, %s }, { { %s, %s }, { %s, %s }, { %s, %s }, { 0, 0 } } },",
                               page, t.bias, t.dcalc, a, b, cell, Hex(t.z[0]).c_str(), Hex(t.z[1]).c_str(), Hex(t.z[2]).c_str(), Hex(0).c_str(),
                               Hex(t.u[0]).c_str(), Hex(t.v[0]).c_str(), Hex(t.u[1]).c_str(), Hex(t.v[1]).c_str(), Hex(t.u[2]).c_str(), Hex(t.v[2]).c_str()));
        }
        printf("page %d: %d cases (%ld tries so far)\n", page, CELLS, tries);
    }
    h.push_back("};");
    WriteLines(outDir + "/cases.h", h);
    WriteLines(outDir + "/cases.txt", txt);
    printf("wrote %s/cases.h, cases.txt\n", outDir.c_str());
    return 0;
}
