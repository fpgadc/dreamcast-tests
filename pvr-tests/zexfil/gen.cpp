/* The probes of common/zexfil.h: for every pixel of tile (0, 0) a triangle whose depth the ISP computes exactly.

       gen  ->  ../../common/zexfil_probes.h          (bash model/build.sh zexfil builds it; -fopenmp is faster)

   A flat polygon's depth goes through the setup's 24 bit truncating products: the slopes (depth times differences
   of the corners' coordinates) and the constant (depth times the cross products e = x_j y_k - x_k y_j of the
   corners), divided by the determinant, e1 + e2 + e3. At absolute screen coordinates the cross products have up to
   19 bits, so a 1x1 quad comes out some steps of the 19 bit stored depth off its nominal depth, more the further
   from the screen's top left (zexfil's first version: up to 27 steps of 2^-24 at x 32..160).

   The corners here have at most 5 significant bits in each coordinate and in each difference of coordinates, the
   three cross products and the sum of any two of them have at most 5 too, and the determinant is plus or minus a
   power of two. Then every product of a depth of 19 significant bits is exact in 24 bits, every sum is exact, the
   slopes are exactly zero, and the constant over the determinant is the depth itself, whatever the details of
   the arithmetic and the order of the corners (as zmeas's calibration quads). The cross products grow with the
   distance from the screen's origin, so such corners exist close to a pixel only near it: the probes are in tile
   (0, 0), and zexfil renders them in a keep Z entry at that tile after the scene's entry of the tile it reads.

   A probe covers its pixel's center and as few others as can be: those it does cover have to be drawn after it
   (each pixel's own probe last among the probes that cover it: zexfil gives every probe its pixel's colour, so
   the colour tells whose probe passed). The table is in that order. Checked with the model (refsw3-core's
   pvr_isp_edge.h and pvr_isp_depth.h): coverage the same with the corners in every order, and depths of 19 bits
   from 2^-100 to 2^100 exact in every order. */
#include "gen_common.h"
#include <map>

using namespace gen;

static int SigBits(double v)
{
    if (v == 0)
        return 0;
    float f = (float)v;
    if ((double)f != v)
        return 99;
    const uint32_t m = (Bits(f) & 0x7fffff) | 0x800000;
    return 24 - __builtin_ctz(m);
}
static bool Nice(double v) { return SigBits(v) <= 5; }
static bool PowerOfTwo(double v, int* k)
{
    int e;
    const double m = frexp(fabs(v), &e);
    *k = e - 1;
    return v != 0 && m == 0.5;
}

struct Pt { double x, y; };
struct Cand { Pt v[3]; int k, covers; std::vector<int> others; };

static const Model model;
static const int perms[6][3] = { { 0, 1, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 0, 2, 1 }, { 2, 1, 0 }, { 1, 0, 2 } };

static bool CoverSet(const Pt* v, const int* o, std::vector<int>* set)
{
    const double x[3] = { v[o[0]].x, v[o[1]].x, v[o[2]].x }, y[3] = { v[o[0]].y, v[o[1]].y, v[o[2]].y };
    Coverage c;
    c.Setup(model, x, y, 3, 0, 0, true, true);
    if (c.bad)
        return false;
    set->clear();
    for (int j = 0; j < 32; j++)
        for (int i = 0; i < 32; i++)
            if (c.Inside(i, j))
                set->push_back(j * 32 + i);
    return true;
}

// the conditions that make any 24 bit truncating setup exact for a flat depth of 19 significant bits
static bool Exactable(const Pt* v, int* k)
{
    for (int a = 0; a < 3; a++) {
        if (!Nice(v[a].x) || !Nice(v[a].y))
            return false;
        for (int b = a + 1; b < 3; b++)
            if (!Nice(v[a].x - v[b].x) || !Nice(v[a].y - v[b].y))
                return false;
    }
    const double e[3] = { v[1].x * v[2].y - v[2].x * v[1].y, v[2].x * v[0].y - v[0].x * v[2].y, v[0].x * v[1].y - v[1].x * v[0].y };
    for (int a = 0; a < 3; a++) {
        if (!Nice(e[a]) || !Nice(e[a] + e[(a + 1) % 3]))
            return false;
    }
    return PowerOfTwo(e[0] + e[1] + e[2], k);
}

int main()
{
    // the numbers a corner may have: at most 5 significant bits, -8 .. 72, in steps of 1/64 at most
    std::vector<double> grid;
    for (int n = -8 * 64; n <= 72 * 64; n++)
        if (Nice(n / 64.0))
            grid.push_back(n / 64.0);
    std::vector<Pt> points;
    for (double x : grid)
        for (double y : grid)
            points.push_back({ x, y });

    // depths to check: 19 significant bits, 2^-100 .. 2^100, and some given
    std::vector<float> depths;
    {
        Rng r(1);
        for (int k = 0; k < 160; k++) {
            const uint32_t e = (uint32_t)(127 + r.integers(-100, 101)), m = (uint32_t)r.integers(0, 1 << 18);
            depths.push_back(FromBits(e << 23 | m << 5));
        }
        for (uint32_t b : { 0x3f800000u, 0x3f7fffe0u, 0x3e800000u, 0x35800000u, 0x38d00000u, 0x3f000020u })
            depths.push_back(FromBits(b));
    }

    std::vector<std::vector<Cand>> cands(1024);
#pragma omp parallel for schedule(dynamic)
    for (int p = 0; p < 1024; p++) {
        const double sx = (p % 32) + 0.5, sy = (p / 32) + 0.5, rs = hypot(sx, sy);
        std::vector<Cand> found;
        auto consider = [&](const Pt& a, const Pt& b, const Pt& c) {
            const double det = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
            if (det <= 0)
                return;
            // the center strictly inside
            const double l1 = ((b.x - sx) * (c.y - sy) - (c.x - sx) * (b.y - sy)) / det;
            const double l2 = ((c.x - sx) * (a.y - sy) - (a.x - sx) * (c.y - sy)) / det;
            const double l3 = 1 - l1 - l2;
            if (!(l1 > 0 && l2 > 0 && l3 > 0))
                return;
            Cand cd{ { a, b, c }, 0, 0, {} };
            if (!Exactable(cd.v, &cd.k) || cd.k < -4 || cd.k > 6)
                return;
            std::vector<int> set, s2;
            if (!CoverSet(cd.v, perms[0], &set) || !std::binary_search(set.begin(), set.end(), p))
                return;
            for (int o = 1; o < 6; o++)
                if (!CoverSet(cd.v, perms[o], &s2) || s2 != set)
                    return;
            for (int q : set)
                if (q != p)
                    cd.others.push_back(q);
            cd.covers = (int)cd.others.size();
            found.push_back(cd);
        };
        // small triangles around the center: corners on the half pixel grid within 2.5 pixels
        std::vector<Pt> box;
        for (auto& q : points)
            if (fabs(q.x - sx) <= 2.5 && fabs(q.y - sy) <= 2.5 && q.x * 2 == floor(q.x * 2) && q.y * 2 == floor(q.y * 2))
                box.push_back(q);
        for (size_t a = 0; a < box.size(); a++)
            for (size_t b = 0; b < box.size(); b++)
                for (size_t c = a + 1; c < box.size(); c++)
                    if (b != a && b != c)
                        consider(box[a], box[b], box[c]);
        // fans from the origin
        std::vector<Pt> near;
        for (auto& q : points) {
            const double cr = q.x * sy - q.y * sx, r = hypot(q.x, q.y);
            if (fabs(cr) > 0 && fabs(cr) < 64 && r >= rs && r <= 2.5 * rs + 8)
                near.push_back(q);
        }
        for (auto& a : near)
            for (auto& b : near)
                consider(Pt{ 0, 0 }, a, b);
        std::sort(found.begin(), found.end(), [](const Cand& u, const Cand& v) {
            if (u.covers != v.covers) return u.covers < v.covers;
            return abs(u.k) < abs(v.k);
        });
        std::vector<Cand> kept;
        for (auto& c : found) {
            bool exact = true;
            for (float d : depths) {
                for (auto& o : perms) {
                    const double px[3] = { c.v[o[0]].x, c.v[o[1]].x, c.v[o[2]].x }, py[3] = { c.v[o[0]].y, c.v[o[1]].y, c.v[o[2]].y };
                    const double pz[3] = { d, d, d };
                    if (IspDepth(model, px, py, pz, sx, sy) != (double)d) { exact = false; break; }
                }
                if (!exact) break;
            }
            if (exact)
                kept.push_back(c);
            if (kept.size() == 12)
                break;
        }
        cands[p] = kept;
    }

    // one candidate each, in an order where every pixel's own probe comes after all others that cover it
    std::vector<int> choice(1024, 0), missing;
    for (int p = 0; p < 1024; p++)
        if (cands[p].empty())
            missing.push_back(p);
    if (!missing.empty()) {
        fprintf(stderr, "gen: no probe for %d pixels:", (int)missing.size());
        for (int p : missing) fprintf(stderr, " (%d, %d)", p % 32, p / 32);
        fprintf(stderr, "\n");
        return 1;
    }
    std::vector<int> order;
    for (int round = 0; round < 64; round++) {
        std::vector<std::vector<int>> after(1024);
        std::vector<int> indeg(1024, 0);
        for (int p = 0; p < 1024; p++)
            for (int q : cands[p][choice[p]].others) {     // p's probe covers q: p before q
                after[p].push_back(q);
                indeg[q]++;
            }
        std::vector<int> ready, out;
        for (int p = 0; p < 1024; p++)
            if (!indeg[p])
                ready.push_back(p);
        while (!ready.empty()) {
            std::sort(ready.begin(), ready.end(), std::greater<int>());
            const int p = ready.back();
            ready.pop_back();
            out.push_back(p);
            for (int q : after[p])
                if (!--indeg[q])
                    ready.push_back(q);
        }
        if (out.size() == 1024) {
            order = out;
            break;
        }
        int changed = 0;                                    // in a cycle: the next candidate of every pixel left
        for (int p = 0; p < 1024; p++)
            if (indeg[p] && choice[p] + 1 < (int)cands[p].size()) {
                choice[p]++;
                changed++;
            }
        fprintf(stderr, "gen: round %d: %d pixels in cycles, %d changed\n", round, 1024 - (int)out.size(), changed);
        if (!changed)
            break;
    }
    if (order.empty()) {
        fprintf(stderr, "gen: no order\n");
        return 1;
    }

    int maxCovers = 0, alone = 0;
    std::map<int, int> ks;
    for (int p = 0; p < 1024; p++) {
        const Cand& c = cands[p][choice[p]];
        maxCovers = std::max(maxCovers, c.covers);
        alone += c.covers == 0;
        ks[c.k]++;
    }
    std::string h;
    h += "/* Generated by pvr-tests/zexfil/gen (gen.cpp, which says why): the probes of zexfil.h, one per pixel of tile\n"
         "   (0, 0), in the order they are drawn. A probe is a triangle at a flat depth; with a depth of 19 significant bits\n"
         "   the ISP's depth at the pixel's center (HALF_OFFSET bit 0) is that depth exactly.\n";
    if (alone == 1024)
        h += "   Every one covers its own pixel only. */\n";
    else
        h += Format("   %d of the 1024 cover their own pixel only, the others at most %d more, all of them drawn after it. */\n", alone, maxCovers);
    h += "#ifndef ZEXFIL_PROBES_H\n#define ZEXFIL_PROBES_H\n\n";
    h += "static const struct { uint16_t pixel; uint32_t x[3], y[3]; } zexfil_probe[1024] = {\n";
    for (int n = 0; n < 1024; n++) {
        const int p = order[n];
        const Cand& c = cands[p][choice[p]];
        h += Format("    { %4d, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu } },\n", p,
                    (unsigned)Bits((float)c.v[0].x), (unsigned)Bits((float)c.v[1].x), (unsigned)Bits((float)c.v[2].x),
                    (unsigned)Bits((float)c.v[0].y), (unsigned)Bits((float)c.v[1].y), (unsigned)Bits((float)c.v[2].y));
    }
    h += "};\n\n#endif\n";
    WriteFile(Here() + "/../../common/zexfil_probes.h", h);
    std::string kd;
    for (auto& e : ks)
        kd += Format(" 2^%d: %d", e.first, e.second);
    printf("zexfil probes: %d alone, at most %d more covered; determinants%s\n", alone, maxCovers, kd.c_str());
    return 0;
}
