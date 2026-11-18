/* zstart's cases (pvr-tests/zstart/main.c): triangles whose plane crosses zero next to their tile's first pixel, so
   that the depth there, the tile's start, is smaller than the slopes. refsw3-core cuts the start to the slopes'
   grid as well (DepthConfig::startOnSlopes): this test is where that rule was put on a console, and where its open
   half was decided (a positive start is cut too: page 1).

   Each case is a triangle in a tile of its own (drawn alone, read out by common/zexfil.h). Its plane is given by its
   start s0 at the tile's first pixel center and a gradient g (depth per pixel); two corners lie on the line where
   the plane is c, far apart, the third far on the side the plane rises, so the triangle covers the part of the
   tile where the depth is above c. Corners at 1/16 or anywhere a float lands (single precision), in a random
   order, each corner's depth the plane's there rounded to single precision. All three corners' depths positive.

     page 0  negative start: s0 = -|g| * 2^-10 .. 2, c a little above 0. Kept when the rule changes the stored
             depth of covered pixels (startOnSlopes 1 against 0, the model before it).
     page 1  positive start, smaller than a quarter of |g|, the gradient into the tile (the whole tile above
             zero), c between 0 and s0: kept when cutting a positive start too (1) and cutting only a negative one
             (2) store different depths at covered pixels (small depths, whose stored step is finer than the
             slopes' grid: near the tile's first pixel).
     page 2  either sign, |s0| = |g| * 2^-12 .. 4, kept as they come (the rule where it may change nothing).

   Every covered pixel has a positive depth in all three variants (the readout measures positive depths).
   Build: bash model/build.sh zstart; run: pvr-tests/zstart/gen [seed] -> pvr-tests/zstart/cases.h */
#include "gen_common.h"

using namespace gen;

static const int PAGES = 3, CASES = 16, WIN_X = 1, WIN_Y = 1, WIN_W = 4;

struct Case {
    float x[3], y[3], z[3];
    double s0, g;
    int covered, d01, d12;
};

static bool Evaluate(const Model& model, Case& c, int left, int top)
{
    pvrisp::DepthTile d[3];
    for (int v = 0; v < 3; v++) {
        pvrisp::DepthConfig cfg = model.depth;
        cfg.startOnSlopes = v;
        d[v].Setup(c.x, c.y, c.z, left, top, cfg);
        if (!d[v].block)
            return false;
    }
    const double xd[3] = { c.x[0], c.x[1], c.x[2] }, yd[3] = { c.y[0], c.y[1], c.y[2] };
    Coverage cov;
    cov.Setup(model, xd, yd, 3, left, top);
    c.covered = c.d01 = c.d12 = 0;
    for (int j = 0; j < 32; j++)
        for (int i = 0; i < 32; i++) {
            if (!cov.Inside(i, j))
                continue;
            float v[3];
            for (int k = 0; k < 3; k++) {
                v[k] = d[k].At(i, j, true);
                if (!(v[k] > 0) || !std::isfinite(v[k]))
                    return false;
            }
            c.covered++;
            c.d01 += v[0] != v[1];
            c.d12 += v[1] != v[2];
        }
    return true;
}

int main(int argc, char** argv)
{
    const uint64_t seed = argc > 1 ? strtoull(argv[1], 0, 0) : 1;
    Rng rng(seed);
    Model model;
    std::vector<std::string> out;
    out.push_back(Format("/* made by pvr-tests/zstart/gen %llu (gen.cpp says what the pages are) */", (unsigned long long)seed));
    out.push_back(Format("#define ZS_PAGES %d", PAGES));
    out.push_back(Format("#define ZS_CASES %d", CASES));
    out.push_back(Format("#define ZS_WIN_X %d", WIN_X));
    out.push_back(Format("#define ZS_WIN_Y %d", WIN_Y));
    out.push_back(Format("#define ZS_WIN_W %d", WIN_W));
    out.push_back("static const struct { f32b x[3], y[3], z[3]; int covered, d01, d12; } zs_case[ZS_PAGES][ZS_CASES] = {");
    const double Pi = 3.14159265358979323846;
    for (int page = 0; page < PAGES; page++) {
        out.push_back("  {");
        long tries = 0, d01 = 0, d12 = 0;
        for (int n = 0; n < CASES; n++) {
            const int tx = WIN_X + n % WIN_W, ty = WIN_Y + n / WIN_W, left = tx * 32, top = ty * 32;
            Case c;
            for (;;) {
                if (++tries > 10000000)
                    Fail(Format("page %d: no case after %ld tries", page, tries));
                const double g = ldexp(1.0, 0) * rng.logUniform(ldexp(1.0, -16), ldexp(1.0, -3));
                const double th = page == 1 ? rng.uniform(0, Pi / 2) : rng.uniform(-Pi / 9, Pi / 2 + Pi / 9);
                const double gx = g * cos(th), gy = g * sin(th);
                double s0, cl;
                if (page == 0) {
                    s0 = -g * rng.logUniform(ldexp(1.0, -10), 2);
                    cl = g * rng.uniform(0.02, 0.5);
                } else if (page == 1) {
                    s0 = g * rng.logUniform(ldexp(1.0, -14), 0.25);
                    cl = s0 * rng.uniform(0.05, 0.95);
                } else {
                    s0 = rng.sign() * g * rng.logUniform(ldexp(1.0, -12), 4);
                    cl = std::max(s0, 0.0) + g * rng.uniform(-0.5, 0.5);
                    if (cl <= 0)
                        cl = g * rng.uniform(0.02, 0.5);
                }
                const double p0x = left + 0.5, p0y = top + 0.5;
                const double qx = p0x + (cl - s0) / (g * g) * gx, qy = p0y + (cl - s0) / (g * g) * gy;
                const double dx = -gy / g, dy = gx / g;
                const double l1 = rng.uniform(150, 700), l2 = rng.uniform(150, 700), l3 = rng.uniform(150, 700), w = rng.uniform(-200, 200);
                double px[3] = { qx - l1 * dx, qx + l2 * dx, qx + l3 * gx / g + w * dx };
                double py[3] = { qy - l1 * dy, qy + l2 * dy, qy + l3 * gy / g + w * dy };
                const bool sixteenths = rng.chance(0.5);
                const std::vector<int> order = rng.permutation(3);
                bool ok = true;
                for (int k = 0; k < 3; k++) {
                    double X = px[order[k]], Y = py[order[k]];
                    if (sixteenths) { X = round(X * 16) / 16; Y = round(Y * 16) / 16; }
                    c.x[k] = (float)X; c.y[k] = (float)Y;
                    c.z[k] = (float)(s0 + gx * ((double)c.x[k] - p0x) + gy * ((double)c.y[k] - p0y));
                    if (!(c.z[k] > 0))
                        ok = false;
                }
                c.s0 = s0; c.g = g;
                if (!ok || !Evaluate(model, c, left, top) || c.covered < 64)
                    continue;
                if (page == 0 && c.d01 < 2)
                    continue;
                if (page == 1 && c.d12 < 2)
                    continue;
                break;
            }
            d01 += c.d01; d12 += c.d12;
            out.push_back(Format("    { { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu }, %d, %d, %d },"
                                 "   /* start %+.3g slopes, |g| 2^%.1f */",
                                 Bits(c.x[0]), Bits(c.x[1]), Bits(c.x[2]), Bits(c.y[0]), Bits(c.y[1]), Bits(c.y[2]),
                                 Bits(c.z[0]), Bits(c.z[1]), Bits(c.z[2]), c.covered, c.d01, c.d12, c.s0 / c.g, log2(c.g)));
        }
        out.push_back("  },");
        printf("page %d: %d cases (%ld tries); covered pixels the rule changes: %ld (1 against 0), %ld (1 against 2)\n", page, CASES, tries, d01, d12);
    }
    out.push_back("};");
    WriteLines(Here() + "/cases.h", out);
    printf("wrote %s/cases.h\n", Here().c_str());
    return 0;
}
