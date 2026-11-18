/* Cases for ops, "one operation at a time": inputs on which changing ONE number of the model changes what a
   console shows.

       gen [--seed N]                           ->  cases.h (for main.c) and cases.json (for analyze.py), here
       gen --regen <set>.cases.json <folder>    ->  cases.h and cases.json of a set that was kept, in <folder>
       gen --check <set>.cases.json             ->  does the model still predict what the set says?

   The model is refsw2's: the ISP's depth, the TSP's planes and the ISP's inside test as nulldc3 has them in
   refsw3/refpvr (pvr_isp_depth.h, pvr_tsp_plane.h, pvr_isp_edge.h, all on pvr_softfp.h), included
   through ../../model/gen_common.h. Each of the three has a configuration, a struct of the widths, reaches and
   shifts that were fitted to a console: pvrisp::DepthConfig, pvrtsp::PlaneConfig, pvrisp::EdgeConfig. An
   ALTERNATIVE is the model with one field of one configuration set to another value: two less, one less, one
   more and two more than the fitted one (Alternatives() below has the list and its exceptions). It is named
   by unit and field and described by the value: "depth productBits" / "25".

   Four kinds of case, one triangle each, inside one 32x32 cell (which is one tile):
       depth   the ISP depth at a probe pixel. The triangle is red; a green quad over the whole cell is drawn
               after it with compare EQUAL, at a depth chosen so that the model predicts "equal" at the probe
               pixel. Alternatives: the fields of DepthConfig.
       fog     the TSP's 1/w at a probe pixel, read through the fog table (see ../tspw). PlaneConfig.
       color   a Gouraud gray at a probe pixel. PlaneConfig.
       cover   whether a probe pixel next to an edge is inside the triangle. EdgeConfig.
   A case is found by trying: N random triangles per kind (four times as many for cover), each asked of the
   model and of every alternative of its kind. For every alternative the generator keeps up to PER_ALT cases
   per kind on which that alternative predicts something else than the model, taking the triangles that tell
   the most alternatives apart first; plus controls, cases picked at random whatever they tell apart.
   analyze.py then counts, per alternative, on how many of its cases the console shows what the model says,
   what the alternative says, or neither.

   Every case carries "expect", what the model predicts (depth: 1 when the quad compares equal; fog: the fog
   readout; color: the gray level; cover: 1 when the pixel is inside), and "tests": the alternatives that
   predict something else, each with its number in the list of alternatives and what it predicts. A control
   carries no tests. The depth kind has 40 negative controls besides: cases over again with the quad a little
   further up, where the model says "not equal".

   This replaces gen.py, which did the same with a model of its own in Python (model.py) and other
   alternatives: one or more per operation of that model, with rounding directions and sum orders, which the
   configurations of refsw2 do not have. The cases it made are kept in ../../fit/ops_python_cases.h and .json.
   A seed gives other cases here than it gave there. The triangles are made as gen.py made them, with one
   exception: the edge of a cover case, see CoverCandidates. */
#include <climits>
#include "gen_common.h"

using namespace gen;

static const int PER_ALT = 10;                  // cases wanted per alternative, in each kind it applies to
static const int N = 60000;                     // candidates per kind
static const int CONTROLS = 30;                 // cases per kind picked at random
static const int MAXP = 12;                     // pages main.c has (NUM_PAGES); a cell holds one case per page
static const long long SEED = 20261115;         // the seed gen.py had
static const Model model;                       // the fitted model

// ---------------------------------------------------------------------------------------------------------
// the alternatives
// ---------------------------------------------------------------------------------------------------------
struct Alt {
    std::string name, what;                     // "depth productBits", "25"
    Model m;                                    // the model with that one field changed
};

template <class C> static void Values(std::vector<Alt>& out, const char* unit, C Model::*part, const char* field, int C::*f, const std::vector<int>& values)
{
    for (int v : values) {
        Alt a{ std::string(unit) + " " + field, Format("%d", v), model };
        a.m.*part.*f = v;
        out.push_back(a);
    }
}

// the fitted value - 2, - 1, + 1, + 2. least: the smallest value that means something (a width has at least 1 bit)
template <class C> static void Around(std::vector<Alt>& out, const char* unit, C Model::*part, const char* field, int C::*f, int least = 1)
{
    const int fitted = model.*part.*f;
    std::vector<int> values;
    for (int d : { -2, -1, 1, 2 })
        if (fitted + d >= least)
            values.push_back(fitted + d);
    Values(out, unit, part, field, f, values);
}

#define FIELD(C, name) #name, &C::name

// Every integer field of the three configurations, in the order of the structs; the first of each unit is at
// *depth, *plane, *edge, and the list ends with the edge unit's.
static std::vector<Alt> Alternatives(size_t* depth, size_t* plane, size_t* edge)
{
    typedef pvrisp::DepthConfig D;
    typedef pvrtsp::PlaneConfig P;
    typedef pvrisp::EdgeConfig E;
    std::vector<Alt> out;

    // the ISP's depth (depth cases). Two fields are in the list without a chance of a case:
    // storedBits 17 and 18: two depths that are equal with 19 bits are equal with fewer.
    // coefBits: what it cuts is cut again, to the 20 bits of startBits and slopeBits, before it is used (unless
    // the depth at the tile's first pixel is exactly zero), so that every value from 20 up gives the same depths.
    *depth = out.size();
    Around(out, "depth", &Model::depth, FIELD(D, productBits));
    Around(out, "depth", &Model::depth, FIELD(D, sumBits));
    Around(out, "depth", &Model::depth, FIELD(D, reach));
    Around(out, "depth", &Model::depth, FIELD(D, recipBits));
    Around(out, "depth", &Model::depth, FIELD(D, coefBits));
    Around(out, "depth", &Model::depth, FIELD(D, startBits));
    Around(out, "depth", &Model::depth, FIELD(D, slopeBits));
    Around(out, "depth", &Model::depth, FIELD(D, pairBits));
    Around(out, "depth", &Model::depth, FIELD(D, tileBits));
    Around(out, "depth", &Model::depth, FIELD(D, storedBits));

    // the TSP's planes (fog and color cases).
    // reach: the fitted 53 is not a measured reach, it stands for "the adder aligns over any distance" (see
    // PlaneConfig), and 51 to 55 are all that same thing. The values that say something else are those of an
    // adder like the ISP's, which drops an operand 25 exponents below the other: 25 and the two on each side.
    // (The triangles made here do not tell them apart: with corners whose 1/w are within a factor of five, the
    // terms of the setup's sums are of one order. That takes a test of its own.)
    *plane = out.size();
    Around(out, "plane", &Model::plane, FIELD(P, productBits));
    Around(out, "plane", &Model::plane, FIELD(P, sumBits));
    Values(out, "plane", &Model::plane, FIELD(P, reach), { 23, 24, 25, 26, 27 });
    Around(out, "plane", &Model::plane, FIELD(P, recipBits));
    Around(out, "plane", &Model::plane, FIELD(P, coefBits));
    Around(out, "plane", &Model::plane, FIELD(P, gridBits));
    Around(out, "plane", &Model::plane, FIELD(P, valueBits));

    // the ISP's inside test (cover cases).
    // guard: the fitted -1 means "no guard bits: the tile-start adds are exact before the cut"; the values
    // that say something else are numbers of guard bits, 0 to 3.
    // startAdd is an offset of an exponent, not a width: 0 and -1 mean something too.
    // overflowExp and saturateExp are in the list for completeness: nothing a triangle inside the screen can
    // reach (2^128), so they get no case here; pvr-tests/hugepos is their test. reach, tieShift and
    // lastColumnByStep get none either: they show on edges that are almost level or almost upright, which random
    // corners do not give (pvr-tests/covmeas, mix lean).
    *edge = out.size();
    Around(out, "edge", &Model::edge, FIELD(E, productBits));
    Around(out, "edge", &Model::edge, FIELD(E, sumBits));
    Around(out, "edge", &Model::edge, FIELD(E, reach));
    Values(out, "edge", &Model::edge, FIELD(E, guard), { 0, 1, 2, 3 });
    Around(out, "edge", &Model::edge, FIELD(E, startBits));
    Around(out, "edge", &Model::edge, FIELD(E, slopeBits));
    Around(out, "edge", &Model::edge, FIELD(E, pairBits));
    Around(out, "edge", &Model::edge, FIELD(E, blockAdd));
    Around(out, "edge", &Model::edge, FIELD(E, startAdd), INT_MIN);
    Around(out, "edge", &Model::edge, FIELD(E, zeroShift));
    Around(out, "edge", &Model::edge, FIELD(E, tieShift));
    Around(out, "edge", &Model::edge, FIELD(E, overflowExp));
    Around(out, "edge", &Model::edge, FIELD(E, saturateExp));
    Alt last{ "edge lastColumnByStep", model.edge.lastColumnByStep ? "false" : "true", model };
    last.m.edge.lastColumnByStep = !model.edge.lastColumnByStep;
    out.push_back(last);
    return out;
}

// ---------------------------------------------------------------------------------------------------------
// the candidates
// ---------------------------------------------------------------------------------------------------------
struct Cand {
    int cell = 0;
    double x[3], y[3], z[3];                    // the triangle, in the order the rasterizer gets it
    double px = 0, py = 0;                      // the center of the probe pixel
    double bz = 0;                              // depth: the depth of the quad
    double col[3] = { 0, 0, 0 };                // color: the gray level at each corner
    double value = 0;                           // depth: the depth the model stores at the probe pixel
    int expect = 0;                             // what the model predicts
    std::vector<std::pair<int, int>> tests;     // the alternatives that predict something else, and what
};

// A random triangle inside one cell. The corners are 1 to 31 pixels into the cell; of half of the triangles
// they are on the 1/16 pixel grid, of the other half any single precision number. The depth (1/w) is around
// z0 = 1.02 to 180: with equal chances flat, gently sloped (the corners within 2^-4 to 2^-15 of z0,
// relatively) or steep (every corner 0.2 to 1 times z0). For fog the corners stay within 1.01 to 250, where
// the fog table tells depths apart; for the other kinds the whole triangle is scaled by e^-3 to e^2.
static Cand RandomTriangle(Rng& rng, bool fog)
{
    Cand c;
    c.cell = (int)rng.integers(0, 300);
    const double cx = CellX(c.cell), cy = CellY(c.cell);
    double p[3][2];
    for (int k = 0; k < 3; k++)
        for (int q = 0; q < 2; q++) p[k][q] = rng.uniform(1.0, 31.0);
    const bool fine = rng.chance(0.5);
    for (int k = 0; k < 3; k++) {
        if (!fine)
            for (int q = 0; q < 2; q++) p[k][q] = nearbyint(p[k][q] * 16) / 16;
        c.x[k] = F32(cx + p[k][0]); c.y[k] = F32(cy + p[k][1]);
    }
    const double z0 = rng.logUniform(1.02, 180.0);
    const int kind = (int)rng.integers(0, 3);
    const double gentle = ldexp(1.0, -(int)rng.integers(4, 16));
    const double scale = fog ? 1.0 : exp(rng.uniform(-3, 2));
    for (int k = 0; k < 3; k++) {
        const double g = 1.0 + rng.uniform(-1, 1) * gentle, s = rng.uniform(0.2, 1.0);
        const double z = z0 * (kind == 0 ? 1.0 : kind == 1 ? g : s);
        c.z[k] = F32(fog ? std::min(std::max(z, 1.01), 250.0) : z * scale);
    }
    return c;
}

// The probe pixel of a depth, fog or color case: the pixel the centroid is in. false when its center is not
// more than `margin` pixels inside every edge, or the triangle has 40 square pixels or less.
static bool ProbePixel(Cand& c, double margin)
{
    const double* x = c.x;
    const double* y = c.y;
    c.px = floor((x[0] + x[1] + x[2]) / 3) + 0.5;
    c.py = floor((y[0] + y[1] + y[2]) / 3) + 0.5;
    const double area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);      // twice the area, signed
    const double sg = area > 0 ? 1 : area < 0 ? -1 : 0;
    bool ok = fabs(area) > 80;
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const double ex = x[j] - x[i], ey = y[j] - y[i];
        const double d = (ex * (c.py - y[i]) - ey * (c.px - x[i])) / hypot(ex, ey) * sg;
        ok = ok && d > margin;
    }
    return ok;
}

// The stored depth, at a pixel, of the quad main.c draws over a whole cell with depth z (a strip TL, TR, BL,
// BR). Of its two triangles the one that holds the pixel is asked, in rasterizer order: the first (TL, TR,
// BL) above the diagonal, the second (BL, TR, BR) on the diagonal and below it.
static double QuadDepth(const Model& m, int cell, double px, double py, double z)
{
    const double cx = CellX(cell), cy = CellY(cell), zs[3] = { z, z, z };
    if ((px - cx) + (py - cy) < 32.0) {
        const double x[3] = { cx, cx + 32, cx }, y[3] = { cy, cy, cy + 32 };
        return IspDepth(m, x, y, zs, px, py);
    }
    const double x[3] = { cx, cx + 32, cx + 32 }, y[3] = { cy + 32, cy, cy + 32 };
    return IspDepth(m, x, y, zs, px, py);
}

// What main.c's fog setup shows for a TSP 1/w: a black polygon, white fog, density 1, and every entry of the
// fog table 0x00ff. The table blends its two bytes by 8 bits of 1/w, those below the 4 mantissa bits that
// (with the exponent) choose the entry, so with these entries the fog's weight is those 8 bits times
// 255 / 256, and the white comes out as that weight times 255 / 256.
// This is the test's readout of refsw2's fog lookup (refsw_tile.cpp: LookupFogTable, FogUnit), taken over from
// model.py as it is. It is the one piece of arithmetic here that does not come from the refsw2 headers.
static int FogReadout(double iw)
{
    const float w = (float)(iw > 255.999985 ? 255.999985 : iw > 1.0 ? iw : 1.0);
    const uint32_t blend = (Bits(w) >> 11) & 255, alpha = (255 * blend) >> 8;
    return (int)((255 * alpha) >> 8);
}

// ---------------------------------------------------------------------------------------------------------
// choosing the cases
// ---------------------------------------------------------------------------------------------------------
static const char* const kinds[4] = { "depth", "fog", "color", "cover" };
enum Kind { DEPTH, FOG, COLOR, COVER };

// What a model predicts for a case: what analyze.py reads off the picture at the probe pixel.
//   depth   1 when the triangle's stored depth and the quad's are equal (the pixel is green)
//   fog     the fog readout of the TSP's 1/w
//   color   the gray level
//   cover   1 when the pixel is inside the triangle
// start (fog, color): the column in the tile where the TSP starts the line, see PlaneCandidates.
static int Predict(const Model& m, Kind kind, const Cand& c, int start)
{
    switch (kind) {
    case DEPTH: return IspDepth(m, c.x, c.y, c.z, c.px, c.py) == QuadDepth(m, c.cell, c.px, c.py, c.bz);
    case FOG: return FogReadout(TspPlane(m, c.x, c.y, c.z, nullptr, c.px, c.py, start).toDouble());
    case COLOR: return TspColor(m, c.x, c.y, c.z, c.col, c.px, c.py, start);
    default: return Covered(m, c.x, c.y, 3, c.px, c.py);
    }
}

// The model's prediction of a candidate, and those of the alternatives first .. end - 1 that differ from it.
static void Ask(Cand& c, Kind kind, int start, const std::vector<Alt>& alts, size_t first, size_t end)
{
    c.expect = Predict(model, kind, c, start);
    for (size_t a = first; a < end; a++) {
        const int pred = Predict(alts[a].m, kind, c, start);
        if (pred != c.expect) c.tests.push_back({ (int)a, pred });
    }
}

static Json CaseJson(Kind kind, const Cand& c, bool withTests)
{
    Json j = Json::Object().set("kind", kinds[kind]).set("cell", c.cell).set("x", Json::List(c.x, 3)).set("y", Json::List(c.y, 3)).set("z", Json::List(c.z, 3));
    if (kind == DEPTH) j.set("bz", c.bz);
    if (kind == COLOR) j.set("col", Json::List(c.col, 3));
    Json tests = Json::Object();
    if (withTests)
        for (auto& t : c.tests) tests.set(Format("%d", t.first), t.second);
    return j.set("px", c.px).set("py", c.py).set("expect", c.expect).set("tests", tests);
}

// The cases of one kind, out of its candidates. Greedily: the candidates are gone through in the order of how
// many alternatives they tell from the model, most first, and one is taken when it tells an alternative apart
// that has fewer than PER_ALT cases so far. Then the controls: CONTROLS candidates picked at random; one that
// is taken already stays as it is, a new one carries no tests. Returns, per alternative, how many of the
// taken cases tell it apart.
static std::vector<int> Select(Rng& rng, Kind kind, const std::vector<Cand>& cand, size_t alternatives, Json& cases)
{
    std::vector<int> count(alternatives, 0);
    std::vector<size_t> order(cand.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return cand[a].tests.size() > cand[b].tests.size(); });
    std::vector<char> taken(cand.size(), 0);
    for (size_t i : order) {
        bool wanted = false;
        for (auto& t : cand[i].tests) wanted = wanted || count[t.first] < PER_ALT;
        if (!wanted)
            continue;
        for (auto& t : cand[i].tests) count[t.first]++;
        taken[i] = 1;
        cases.push(CaseJson(kind, cand[i], true));
    }
    const std::vector<int> pick = rng.permutation((int)cand.size());
    for (size_t k = 0; k < pick.size() && k < (size_t)CONTROLS; k++)
        if (!taken[pick[k]]) {
            taken[pick[k]] = 1;
            cases.push(CaseJson(kind, cand[pick[k]], false));
        }
    return count;
}

// depth: the triangle against the quad, compared EQUAL at the pixel at the triangle's centroid. The quad gets
// the lowest single precision depth, within 200 steps of the depth the model stores for the triangle, that
// the model stores as that same depth: the model predicts "equal". An alternative predicts for itself, the
// triangle's depth against the quad's with its own configuration.
static std::vector<Cand> DepthCandidates(Rng& rng, const std::vector<Alt>& alts, size_t first, size_t end)
{
    std::vector<Cand> out;
    for (int n = 0; n < N; n++) {
        Cand c = RandomTriangle(rng, false);
        if (!ProbePixel(c, 1.5))
            continue;
        c.value = IspDepth(model, c.x, c.y, c.z, c.px, c.py);
        if (!(c.value > 0) || !std::isfinite(c.value))
            continue;
        bool found = false;
        for (int k = -200; k <= 200 && !found; k++) {
            c.bz = Step(c.value, k);
            found = QuadDepth(model, c.cell, c.px, c.py, c.bz) == c.value;
        }
        if (!found)
            continue;
        Ask(c, DEPTH, 0, alts, first, end);
        out.push_back(c);
    }
    return out;
}

// fog and color: the TSP's 1/w through the fog table, or a Gouraud gray, at the pixel at the centroid. The
// TSP evaluates its planes from the first pixel of the triangle on the line (SpanStart; nothing hides any of
// it here). Which pixel that is, is the inside test's business, so it is the same for every alternative of
// the planes.
static std::vector<Cand> PlaneCandidates(Rng& rng, Kind kind, const std::vector<Alt>& alts, size_t first, size_t end)
{
    std::vector<Cand> out;
    for (int n = 0; n < N; n++) {
        Cand c = RandomTriangle(rng, kind == FOG);
        if (kind == COLOR) {
            // any three levels; of three triangles in ten, all corners 255
            for (int k = 0; k < 3; k++) c.col[k] = (double)rng.integers(0, 256);
            if (rng.chance(0.3)) c.col[0] = c.col[1] = c.col[2] = 255;
        }
        if (!ProbePixel(c, 1.5))
            continue;
        const int start = SpanStart(model, c.x, c.y, c.px, c.py);
        if (kind == FOG) {
            const double iw = TspPlane(model, c.x, c.y, c.z, nullptr, c.px, c.py, start).toDouble();
            if (!(iw > 1.01 && iw < 250))               // where the fog table tells depths apart
                continue;
        }
        Ask(c, kind, start, alts, first, end);
        out.push_back(c);
    }
    return out;
}

// cover: a probe pixel very near an edge. The probe is the pixel the middle of the triangle's first edge
// (corners 1 and 2) is in; the two corners are then moved together, square to the edge, so that the edge
// passes the pixel's center at a distance d of up to 2^-6 to 2^-21 pixels, on either side (as near as
// single precision corners can put it: their last place is 2^-15 pixels in the middle of the screen). Kept
// when the triangle is still inside its cell, the edge is longer than 6 pixels and the triangle has more than
// 30 square pixels.
// (gen.py moved the corners the other way, a slip of the sign: its edge ended up twice as far from the pixel's
// center as it happened to be, half a pixel on average, and only the few candidates whose edge was near the
// center to begin with could tell an alternative apart.)
static std::vector<Cand> CoverCandidates(Rng& rng, const std::vector<Alt>& alts, size_t first, size_t end)
{
    std::vector<Cand> out;
    for (int n = 0; n < 4 * N; n++) {
        Cand c = RandomTriangle(rng, false);
        const double cx = CellX(c.cell), cy = CellY(c.cell);
        c.px = floor((c.x[0] + c.x[1]) / 2) + 0.5;
        c.py = floor((c.y[0] + c.y[1]) / 2) + 0.5;
        const double d = ldexp(rng.uniform(-1, 1), -(int)rng.integers(6, 22));
        const double ex = c.x[1] - c.x[0], ey = c.y[1] - c.y[0], ln = hypot(ex, ey);
        if (!(ln > 6))
            continue;
        // The center is this far from the edge, along the edge's normal (-ey, ex) / ln; less d is how far along
        // the normal the edge has to move.
        const double move = (ex * (c.py - c.y[0]) - ey * (c.px - c.x[0])) / ln - d;
        for (int k = 0; k < 2; k++) {
            c.x[k] = F32(c.x[k] + move * (-ey / ln));
            c.y[k] = F32(c.y[k] + move * (ex / ln));
        }
        bool inside = true;
        for (int k = 0; k < 3; k++)
            inside = inside && c.x[k] >= cx && c.x[k] <= cx + 32 && c.y[k] >= cy && c.y[k] <= cy + 32;
        const double area = fabs((c.x[1] - c.x[0]) * (c.y[2] - c.y[0]) - (c.x[2] - c.x[0]) * (c.y[1] - c.y[0]));
        if (!inside || !(area > 60))
            continue;
        Ask(c, COVER, 0, alts, first, end);
        out.push_back(c);
    }
    return out;
}

// how a kind came out: the alternatives that did not get their PER_ALT cases, and how many they got
static void Report(Kind kind, const std::vector<Alt>& alts, size_t first, size_t end, size_t candidates, const std::vector<int>& count)
{
    std::string below;
    int n = 0;
    for (size_t a = first; a < end; a++)
        if (count[a] < PER_ALT) {
            below += Format("%s%s / %s: %d", n ? ", " : "", alts[a].name.c_str(), alts[a].what.c_str(), count[a]);
            n++;
        }
    printf("%s: %zu usable candidates; %d of %zu alternatives have fewer than %d cases%s%s\n", kinds[kind], candidates, n, end - first, PER_ALT,
           n ? ": " : "", below.c_str());
}

// ---------------------------------------------------------------------------------------------------------
// a set: what cases.json holds
// ---------------------------------------------------------------------------------------------------------
static Kind KindOf(const Json& c)
{
    for (int k = 0; k < 4; k++)
        if (c.at("kind").str() == kinds[k])
            return (Kind)k;
    Fail("cases.json: no kind " + c.at("kind").str());
}

// cases.h and cases.json from the description of a set. Every float goes into cases.h as the bit pattern of
// the single precision number: the test program never converts or calculates one, so a PC and the console
// send the same bits.
static void Write(const std::string& folder, const Json& data)
{
    const Json& cases = data.at("cases");
    int pages = 0;
    for (const Json& c : cases.a) pages = std::max(pages, (int)c.at("page").integer() + 1);
    std::vector<std::string> lines = {
        "/* Generated by pvr-tests/ops/gen (gen.cpp) - do not edit. Floats are bit patterns. */", "#include <stdint.h>",
        Format("#define OPS_PAGES %d", pages), Format("#define NUM_CASES %zu", cases.size()),
        "typedef struct { unsigned char kind, page; unsigned short cell; uint32_t v[3][3]; unsigned char col[3]; uint32_t bz; } case_t;    /* v: x, y, z */",
        "static const case_t cases[NUM_CASES] = {" };
    for (const Json& c : cases.a) {
        std::string v;
        for (int k = 0; k < 3; k++)
            v += std::string(k ? ", " : "") + "{ " + Hex(c.at("x")[k].num()) + ", " + Hex(c.at("y")[k].num()) + ", " + Hex(c.at("z")[k].num()) + " }";
        int col[3] = { 0, 0, 0 };
        if (c.has("col"))
            for (int k = 0; k < 3; k++) col[k] = (int)c.at("col")[k].num();
        lines.push_back(Format("    { %d, %d, %d, { %s }, { %d, %d, %d }, %s },", (int)KindOf(c), (int)c.at("page").integer(), (int)c.at("cell").integer(),
                               v.c_str(), col[0], col[1], col[2], Hex(c.has("bz") ? c.at("bz").num() : 0.0).c_str()));
    }
    lines.push_back("};");
    WriteLines(folder + "/cases.h", lines);
    WriteFile(folder + "/cases.json", data.dump());
}

// Does the model still predict what a set says ("expect")? And, when the set's alternatives are the ones of
// this generator, do they predict what its "tests" say? (A set made by gen.py holds the predictions of the
// Python model and has other alternatives: for it only the first question is asked.)
static int Check(const Json& data, const std::vector<Alt>& alts)
{
    bool mine = data.at("alternatives").size() == alts.size();
    for (size_t a = 0; mine && a < alts.size(); a++)
        mine = data.at("alternatives")[a][0].str() == alts[a].name && data.at("alternatives")[a][1].str() == alts[a].what;
    int same[4] = { 0, 0, 0, 0 }, all[4] = { 0, 0, 0, 0 }, tests = 0, testsSame = 0;
    for (const Json& j : data.at("cases").a) {
        const Kind kind = KindOf(j);
        Cand c;
        c.cell = (int)j.at("cell").integer();
        for (int k = 0; k < 3; k++) {
            c.x[k] = j.at("x")[k].num(); c.y[k] = j.at("y")[k].num(); c.z[k] = j.at("z")[k].num();
            if (j.has("col")) c.col[k] = j.at("col")[k].num();
        }
        c.px = j.at("px").num(); c.py = j.at("py").num();
        if (j.has("bz")) c.bz = j.at("bz").num();
        const int start = kind == FOG || kind == COLOR ? SpanStart(model, c.x, c.y, c.px, c.py) : 0;
        all[kind]++;
        same[kind] += Predict(model, kind, c, start) == j.at("expect").integer();
        if (mine)
            for (auto& t : j.at("tests").o) {
                tests++;
                testsSame += Predict(alts[atoi(t.first.c_str())].m, kind, c, start) == t.second.integer();
            }
    }
    printf("the model predicts what the set expects on: depth %d of %d, fog %d of %d, color %d of %d, cover %d of %d cases\n", same[0], all[0], same[1],
           all[1], same[2], all[2], same[3], all[3]);
    if (mine)
        printf("the alternatives predict what the set's tests say on %d of %d\n", testsSame, tests);
    else
        printf("the set has other alternatives than this generator: its tests are not checked\n");
    return same[0] == all[0] && same[1] == all[1] && same[2] == all[2] && same[3] == all[3] && testsSame == tests ? 0 : 1;
}

int main(int argc, char** argv)
{
    size_t depth, plane, edge;
    const std::vector<Alt> alts = Alternatives(&depth, &plane, &edge);
    long long seed = SEED;
    for (int a = 1; a < argc; a++) {
        const std::string arg = argv[a];
        if (arg == "--regen" && a + 2 < argc) {
            Write(argv[a + 2], Json::Parse(ReadFile(argv[a + 1])));
            return 0;
        }
        if (arg == "--check" && a + 1 < argc)
            return Check(Json::Parse(ReadFile(argv[a + 1])), alts);
        if (arg == "--seed" && a + 1 < argc) seed = atoll(argv[++a]);
        else Fail("gen [--seed N]  |  gen --regen <set>.cases.json <folder>  |  gen --check <set>.cases.json");
    }
    Rng rng((uint64_t)seed);
    Json found = Json::Array();
    {
        const std::vector<Cand> cand = DepthCandidates(rng, alts, depth, plane);
        Report(DEPTH, alts, depth, plane, cand.size(), Select(rng, DEPTH, cand, alts.size(), found));
        // negative controls: the first 40 candidates again, with the quad 97 single precision steps (three
        // steps of the stored depth) further up, where the model says "not equal"
        for (size_t i = 0; i < cand.size() && i < 40; i++) {
            Cand c = cand[i];
            c.bz = Step(c.bz, 97);
            c.expect = 0;
            if (QuadDepth(model, c.cell, c.px, c.py, c.bz) != c.value)
                found.push(CaseJson(DEPTH, c, false));
        }
    }
    {
        const std::vector<Cand> cand = PlaneCandidates(rng, FOG, alts, plane, edge);
        Report(FOG, alts, plane, edge, cand.size(), Select(rng, FOG, cand, alts.size(), found));
    }
    {
        const std::vector<Cand> cand = PlaneCandidates(rng, COLOR, alts, plane, edge);
        Report(COLOR, alts, plane, edge, cand.size(), Select(rng, COLOR, cand, alts.size(), found));
    }
    {
        const std::vector<Cand> cand = CoverCandidates(rng, alts, edge, alts.size());
        Report(COVER, alts, edge, alts.size(), cand.size(), Select(rng, COVER, cand, alts.size(), found));
    }

    // lay out: a cell holds one case per page, in the order found; what is beyond the last page is left out
    int seen[300] = {}, pages = 0, number[4] = { 0, 0, 0, 0 };
    Json cases = Json::Array();
    for (Json& c : found.a) {
        const int page = seen[c.at("cell").integer()]++;
        if (page >= MAXP)
            continue;
        pages = std::max(pages, page + 1);
        number[KindOf(c)]++;
        cases.push(c.set("page", page));
    }
    Json names = Json::Array();
    for (const Alt& a : alts) names.push(Json::Array().push(a.name).push(a.what));
    const size_t all = found.size(), kept = cases.size();
    Write(Here(), Json::Object().set("seed", seed).set("alternatives", std::move(names)).set("cases", std::move(cases)));
    printf("%zu cases on %d pages: depth %d, fog %d, color %d, cover %d", kept, pages, number[0], number[1], number[2], number[3]);
    if (all != kept)
        printf(" (%zu more did not fit on %d pages)", all - kept, MAXP);
    printf("\n");
    return 0;
}
