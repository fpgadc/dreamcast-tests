/* Cases for zmeas: measure the depth the ISP stores for a triangle, as a number.

       gen [--mix first|general|zero|scene] [--seed N] [--refine meas.json]    ->  cases.h, cases.json (here)
       gen --regen <set>.cases.json <folder>                                    ->  the tables of an archived set
       gen --check <set>.cases.json                                             ->  does the model still give the set's numbers?

   A page draws a list of polygons "A" (red; depth always passes and is written), in order, and then one probe
   "B" per measured cell: a flat quad over the whole 32x32 cell, compare GREATER. On page p, the depth of B is
   the p-th value of the ladder of that cell, so the 16 pages bracket the stored depth of A between two stored
   depths of B. --refine makes the ladder 16 consecutive stored values starting at the measured lower bound of
   each cell.

   The stored depth of B comes from the model: refsw2's depth unit, pvr_isp_depth.h, through
   ../../model/gen_common.h. Flat quads on cell corners are the family the model reproduces completely
   (pvr-tests/precision), and the "calibration" group checks it here: there A is itself a full-cell flat quad
   whose depth has at most 15 significant bits, which every product and sum of a 24 bit setup represents
   exactly, so its stored depth is its depth whatever the details of the arithmetic are.

   Mixes:
       first    one small triangle inside each cell; integer, 1/16 and float corners; flat, gentle and steep depth
       general  the same, weighted to sloped depth, a random probe pixel, wider depth range
       zero     small triangles whose depth plane crosses zero just outside the triangle, inside the tile: the
                depth changes exponent, and often sign, between the first pixel of the tile and the triangle
       scene    triangles of any size (corners up to 1500 pixels off the screen) drawn over each other, largest
                first; a cell measures the triangle that is on top there

   (An earlier version used a huge flat triangle confined to the tile by the user tile clip as an exact probe.
   On a console the clip is applied late - a polygon is clipped by the clip command that follows it - and with
   padding added some tiles still come out corrupted; see ../cliptest.)

   This replaces gen.py, which placed the ladders with a model of its own in Python. The sets made by that one
   are in fit/zmeas_data and run as before (--regen); a seed gives other cases here than it gave there. */
#include "gen_common.h"
#include "zmeas.h"

using namespace gen;

static const int SEARCH = 600;
static const Model model;

struct Group { const char* name; int count; const char* coords; const char* depth; };
static const std::vector<Group> groupsFirst = {
    { "calibration: full-cell quad, 15 bit depth", 20, "cal", "flat" },
    { "full-cell quad, upper triangle", 10, "quad0", "flat" },
    { "full-cell quad, lower triangle", 10, "quad1", "flat" },
    { "integer corners, flat, depth a power of two", 20, "int", "pow2" },
    { "integer corners, flat", 40, "int", "flat" },
    { "1/16 corners, flat", 40, "16", "flat" },
    { "float corners, flat", 40, "float", "flat" },
    { "integer corners, gentle slope", 30, "int", "gentle" },
    { "integer corners, steep", 30, "int", "steep" },
    { "float corners, gentle slope", 30, "float", "gentle" },
    { "float corners, steep", 30, "float", "steep" },
};
static const std::vector<Group> groupsGeneral = {
    { "calibration: full-cell quad, 15 bit depth", 10, "cal", "flat" },
    { "float corners, flat", 20, "float", "flat" },
    { "integer corners, steep", 30, "int", "steep" },
    { "1/16 corners, steep", 30, "16", "steep" },
    { "float corners, steep", 60, "float", "steep" },
    { "float corners, gentle slope", 40, "float", "gentle" },
    { "float corners, medium slope", 40, "float", "medium" },
    { "integer corners, medium slope", 20, "int", "medium" },
    { "float corners, depth over several octaves", 50, "float", "wide" },
};
static const std::vector<Group> groupsZero = {
    { "calibration: full-cell quad, 15 bit depth", 10, "cal", "flat" },
    { "zero line next to the triangle, float corners", 200, "float", "zero" },
    { "zero line next to the triangle, integer corners", 90, "int", "zero" },
};

static std::vector<int> Ladder(const std::string& mix)
{
    if (mix == "first") return { -64, -32, -16, -8, -4, -2, -1, 0, 1, 2, 3, 5, 9, 17, 33, 65 };
    if (mix == "general") return { -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    return { -64, -16, -6, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 7, 17, 65 };       // scene, zero
}

struct Tri { double x[3], y[3], z[3]; };
typedef std::pair<double, double> Pt;

static void Depths(Rng& rng, const std::string& kind, double z0, double z[3])
{
    if (kind == "pow2") {
        double v = ldexp(1.0, (int)rng.integers(-5, 8));
        z[0] = z[1] = z[2] = v;
    } else if (kind == "flat")
        z[0] = z[1] = z[2] = z0;
    else if (kind == "gentle" || kind == "medium") {
        double u[3] = { rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1) };
        double s = kind == "gentle" ? ldexp(1.0, -(int)rng.integers(8, 15)) : ldexp(1.0, -(int)rng.integers(3, 8));
        for (int k = 0; k < 3; k++) z[k] = z0 * (1.0 + u[k] * s);
    } else if (kind == "wide")
        for (int k = 0; k < 3; k++) z[k] = z0 * pow(2.0, rng.uniform(-6, 0));
    else
        for (int k = 0; k < 3; k++) z[k] = z0 * rng.uniform(0.25, 1.0);
    for (int k = 0; k < 3; k++) z[k] = F32(z[k]);
}

// signed distance of a point from the nearest edge of the triangle, positive inside
static double EdgeDistance(const Tri& t, double px, double py)
{
    double area = (t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]);
    double sg = area > 0 ? 1 : area < 0 ? -1 : 0, d = HUGE_VAL;
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        double ex = t.x[j] - t.x[i], ey = t.y[j] - t.y[i];
        d = std::min(d, (ex * (py - t.y[i]) - ey * (px - t.x[i])) / hypot(ex, ey) * sg);
    }
    return d;
}

// pixel centers at least `margin` inside the triangle
static std::vector<Pt> Interior(const Tri& t, double margin = 1.0)
{
    std::vector<Pt> out;
    double xmin = std::min({ t.x[0], t.x[1], t.x[2] }), xmax = std::max({ t.x[0], t.x[1], t.x[2] });
    double ymin = std::min({ t.y[0], t.y[1], t.y[2] }), ymax = std::max({ t.y[0], t.y[1], t.y[2] });
    for (double py = floor(ymin) + 0.5; py < ymax; py += 1.0)
        for (double px = floor(xmin) + 0.5; px < xmax; px += 1.0)
            if (EdgeDistance(t, px, py) > margin)
                out.push_back({ px, py });
    return out;
}

// at most 23 of them, spread over the list
static std::vector<Pt> Thin(const std::vector<Pt>& rest)
{
    if (rest.size() <= 23) return rest;
    std::vector<Pt> out;
    for (size_t k = 0; k < 23; k++) out.push_back(rest[k * (rest.size() - 1) / 22]);
    return out;
}

static Json Points(const Pt& first, const std::vector<Pt>& rest)
{
    Json a = Json::Array();
    a.push(Json::Array().push(first.first).push(first.second));
    for (auto& p : rest) a.push(Json::Array().push(p.first).push(p.second));
    return a;
}

static Json CaseJson(const std::string& group, bool quad, int cell, const Tri& t)
{
    return Json::Object().set("group", group).set("quad", quad).set("cell", cell)
        .set("x", Json::List(t.x, 3)).set("y", Json::List(t.y, 3)).set("z", Json::List(t.z, 3));
}

// One polygon per cell. Every case carries its own polygon.
static Json MakeCases(Rng& rng, const std::string& mix)
{
    const bool first = mix == "first";
    const std::vector<Group>& groups = first ? groupsFirst : mix == "general" ? groupsGeneral : groupsZero;
    std::vector<int> cells = rng.permutation(300);
    Json cases = Json::Array();
    int n = 0;
    for (const Group& g : groups)
        for (int c = 0; c < g.count; c++) {
            const int cell = cells[n++];
            const double cx = CellX(cell), cy = CellY(cell);
            const std::string coords = g.coords, depth = g.depth;
            Tri t;
            std::vector<Pt> full;
            Pt pix;
            for (;;) {
                const double z0 = first ? rng.logUniform(0.02, 200.0) : rng.logUniform(1e-3, 1e3);
                if (coords == "cal" || coords == "quad0" || coords == "quad1") {
                    if (coords == "quad1") {
                        t = { { cx, cx + 32, cx + 32 }, { cy + 32, cy, cy + 32 }, {} };
                        pix = { cx + 22.5, cy + 24.5 };
                    } else {
                        t = { { cx, cx + 32, cx }, { cy, cy, cy + 32 }, {} };
                        pix = { cx + 8.5, cy + 9.5 };
                    }
                    double z = z0;
                    if (coords == "cal") {              // at most 15 significant bits
                        int e;
                        double m = frexp(z0, &e);
                        z = ldexp(floor(m * 32768) / 32768, e);
                    }
                    t.z[0] = t.z[1] = t.z[2] = F32(z);
                    for (int i = 2; i < 30; i += 5)
                        for (int j = 2; j < 30; j += 5)
                            if (i + j + 1 != 32 && (coords == "cal" || ((i + j + 1 < 32) == (coords == "quad0"))))
                                full.push_back({ cx + i + 0.5, cy + j + 0.5 });
                    break;
                }
                double th = 0;
                for (int k = 0; k < 3; k++) {
                    double px = rng.uniform(1.0, 31.0), py = rng.uniform(1.0, 31.0);
                    if (coords == "int") { px = nearbyint(px); py = nearbyint(py); }
                    else if (coords == "16") { px = nearbyint(px * 16) / 16; py = nearbyint(py * 16) / 16; }
                    t.x[k] = F32(cx + px); t.y[k] = F32(cy + py);
                }
                if (depth == "zero") {
                    // depth = distance from a line that passes 0.05 .. 8 pixels outside the triangle
                    th = rng.uniform(0, 2 * M_PI);
                    double s[3], smin = HUGE_VAL;
                    for (int k = 0; k < 3; k++) { s[k] = cos(th) * t.x[k] + sin(th) * t.y[k]; smin = std::min(smin, s[k]); }
                    const double gap = rng.logUniform(0.05, 8.0);
                    for (int k = 0; k < 3; k++) t.z[k] = F32(z0 / 16 * (s[k] - smin + gap));
                } else
                    Depths(rng, depth, z0, t.z);
                const double area = fabs((t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]));
                if (area <= 150)
                    continue;
                full.clear();
                for (auto& p : Interior(t))
                    if ((p.first - cx) + (p.second - cy) != 32.0)       // not on the diagonal of the probe quad
                        full.push_back(p);
                if (full.size() < 12)
                    continue;
                auto nearest = [&](auto key) {
                    size_t best = 0;
                    for (size_t k = 1; k < full.size(); k++)
                        if (key(full[k]) < key(full[best])) best = k;
                    return full[best];
                };
                if (depth == "zero" && rng.chance(0.5))                  // the pixel nearest the zero line
                    pix = nearest([&](const Pt& p) { return cos(th) * p.first + sin(th) * p.second; });
                else if (first) {
                    const double gx = (t.x[0] + t.x[1] + t.x[2]) / 3, gy = (t.y[0] + t.y[1] + t.y[2]) / 3;
                    pix = nearest([&](const Pt& p) { return (p.first - gx) * (p.first - gx) + (p.second - gy) * (p.second - gy); });
                } else
                    pix = full[rng.integers(0, (int64_t)full.size())];
                break;
            }
            std::vector<Pt> rest;
            for (auto& p : full)
                if (p != pix) rest.push_back(p);
            cases.push(CaseJson(g.name, coords == "cal", cell, t).set("pixels", Points(pix, Thin(rest))));
        }
    return cases;
}

// Triangles of all sizes drawn over each other; each cell measures the one that ends up on top of it.
static Json MakeScene(Rng& rng, Json& tris)
{
    struct Class { const char* name; int count; double h; };
    static const Class classes[4] = { { "huge", 10, 1500.0 }, { "large", 20, 400.0 }, { "medium", 35, 150.0 }, { "small", 45, 60.0 } };
    static const char* cornerKinds[3] = { "int", "16", "float" };
    static const char* depthKinds[5] = { "flat", "gentle", "medium", "steep", "wide" };
    std::vector<int> owner(480 * 640, -1);
    std::vector<char> clean(480 * 640, 0);
    int load[15][20] = {};
    std::vector<Tri> list;
    std::vector<std::string> detail, cls;
    tris = Json::Array();
    for (const Class& c : classes)
        for (int made = 0; made < c.count;) {
            const double ox = rng.uniform(0, 640), oy = rng.uniform(0, 480);
            double p[3][2];
            for (int k = 0; k < 3; k++) { p[k][0] = ox + rng.uniform(-c.h, c.h); p[k][1] = oy + rng.uniform(-c.h, c.h); }
            const size_t ck = rng.choice({ 0.2, 0.2, 0.6 });
            Tri t;
            for (int k = 0; k < 3; k++) {
                for (int q = 0; q < 2; q++)
                    p[k][q] = ck == 0 ? nearbyint(p[k][q]) : ck == 1 ? nearbyint(p[k][q] * 16) / 16 : p[k][q];
                t.x[k] = F32(p[k][0]); t.y[k] = F32(p[k][1]);
            }
            const double area = (t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]);
            if (fabs(area) < c.h * c.h / 3)
                continue;
            const double xmin = std::min({ t.x[0], t.x[1], t.x[2] }), xmax = std::max({ t.x[0], t.x[1], t.x[2] });
            const double ymin = std::min({ t.y[0], t.y[1], t.y[2] }), ymax = std::max({ t.y[0], t.y[1], t.y[2] });
            if (xmax < 0 || ymax < 0 || xmin >= 640 || ymin >= 480)
                continue;
            auto tile = [](double v, int top) { return (int)std::min((double)top, std::max(0.0, floor(v / 32))); };
            const int tx0 = tile(xmin, 19), tx1 = tile(xmax, 19), ty0 = tile(ymin, 14), ty1 = tile(ymax, 14);
            int most = 0;
            for (int ty = ty0; ty <= ty1; ty++)
                for (int tx = tx0; tx <= tx1; tx++) most = std::max(most, load[ty][tx]);
            if (most >= 40)                 // the tiles its bounding box touches have enough polygons
                continue;
            for (int ty = ty0; ty <= ty1; ty++)
                for (int tx = tx0; tx <= tx1; tx++) load[ty][tx]++;
            const size_t dk = rng.choice({ 0.15, 0.2, 0.2, 0.3, 0.15 });
            Depths(rng, depthKinds[dk], rng.logUniform(1e-3, 1e3), t.z);
            for (int y = 0; y < 480; y++)
                for (int x = 0; x < 640; x++) {
                    const double d = EdgeDistance(t, x + 0.5, y + 0.5);
                    if (d > 1.0) { owner[y * 640 + x] = (int)list.size(); clean[y * 640 + x] = 1; }
                    else if (d > -1.0) clean[y * 640 + x] = 0;
                }
            list.push_back(t);
            cls.push_back(c.name);
            detail.push_back(Format("scene: %s, %s corners, %s depth", c.name, cornerKinds[ck], depthKinds[dk]));
            tris.push(CaseJson(detail.back(), false, 0, t).set("cls", c.name));
            made++;
        }
    Json cases = Json::Array();
    for (int cell = 0; cell < 300; cell++) {
        const int cx = (cell % 20) * 32, cy = (cell / 20) * 32;
        std::vector<int> count(list.size(), 0);
        for (int j = 0; j < 32; j++)
            for (int i = 0; i < 32; i++) {
                const int at = (cy + j) * 640 + cx + i;
                if (clean[at] && i + j != 31 && owner[at] >= 0) count[owner[at]]++;       // not on the probe quad's diagonal
            }
        int t = -1;
        for (size_t k = 0; k < count.size(); k++)
            if (count[k] > (t < 0 ? 0 : count[t])) t = (int)k;
        if (t < 0 || count[t] < 12)
            continue;
        std::vector<Pt> full;
        for (int j = 0; j < 32; j++)
            for (int i = 0; i < 32; i++) {
                const int at = (cy + j) * 640 + cx + i;
                if (clean[at] && i + j != 31 && owner[at] == t) full.push_back({ cx + i + 0.5, cy + j + 0.5 });
            }
        const Pt pix = full[rng.integers(0, (int64_t)full.size())];
        std::vector<Pt> rest;
        for (auto& p : full)
            if (p != pix) rest.push_back(p);
        cases.push(Json::Object().set("group", "scene: " + cls[t]).set("detail", detail[t]).set("tri", t).set("quad", false).set("cell", cell)
                       .set("x", Json::List(list[t].x, 3)).set("y", Json::List(list[t].y, 3)).set("z", Json::List(list[t].z, 3))
                       .set("pixels", Points(pix, Thin(rest))));
    }
    return cases;
}

// v cut to 19 significant bits toward zero: a number the depth buffer can hold
static double Stored19(double v)
{
    if (v == 0 || !std::isfinite(v))
        return v;
    int e;
    const double m = frexp(v, &e);
    return ldexp(trunc(m * 524288.0) / 524288.0, e);
}

// The ladder of every case: 16 depths for the probe B whose stored depths (by the model) step around the
// stored depth the model predicts for A at the probe pixel, or, refining, upward from the measured lower bound.
static int Ladders(Json& cases, const std::string& mix, const Json* refine)
{
    int missed = 0;
    for (size_t n = 0; n < cases.a.size(); n++) {
        Json& c = cases.a[n];
        const std::vector<double> x = c.at("x").nums(), y = c.at("y").nums(), z = c.at("z").nums();
        const double px = c.at("pixels")[0][0].num(), py = c.at("pixels")[0][1].num();
        const int cell = (int)c.at("cell").integer();
        const double pred = IspDepth(model, x.data(), y.data(), z.data(), px, py);
        double center = pred;
        std::vector<int> offs = Ladder(mix);
        if (refine) {
            const Json& probe = refine->at("cases")[n].at("probe");
            if (probe.has("lo")) {
                center = probe.at("lo").num();
                for (int k = 0; k < zmeas::P; k++) offs[k] = k;
            } else if (probe.has("hi")) {
                center = probe.at("hi").num();
                for (int k = 0; k < zmeas::P; k++) offs[k] = k - zmeas::P;
            }
        }
        int e;
        frexp(center, &e);
        const double unit = ldexp(1.0, e - 19);
        Json in = Json::Array(), out = Json::Array();
        for (int o : offs) {
            // an input depth for B whose stored depth is the target: the nearest one, searched in single
            // precision steps around the target itself
            const double target = Stored19(center + o * unit);
            int hit = INT32_MAX, near = 0;
            double nearErr = HUGE_VAL, nearGot = 0, hitGot = 0;
            for (int k = -SEARCH; k <= SEARCH; k++) {
                const double got = zmeas::QuadDepth(model, cell, px, py, Step(target, k));
                if (got == target && abs(k) < abs(hit)) { hit = k; hitGot = got; }
                const double err = fabs(got - target);
                if (err < nearErr) { nearErr = err; near = k; nearGot = got; }
            }
            if (hit == INT32_MAX) {
                missed++;
                hit = near; hitGot = nearGot;
            }
            in.push(Step(target, hit));
            out.push(hitGot);
        }
        c.set("predicted", pred).set("ladder_in", in).set("ladder", out);
    }
    return missed;
}

// cases.h and cases.json from the description of a set. Every float goes into cases.h as the bit pattern of
// the single precision number: the test program never converts or calculates one, so a PC and the console
// send the same bits. A scene has more polygons than measured cells: they are in "tris".
static void Write(const std::string& folder, const Json& data)
{
    const Json& cases = data.at("cases");
    if (!data.has("tris") && data.has("mix") && data.at("mix").str() == "scene")
        Fail("a scene without its polygons (\"tris\")");
    const Json& tris = data.has("tris") ? data.at("tris") : cases;
    std::vector<std::string> lines = {
        "/* Generated by pvr-tests/zmeas/gen (gen.cpp) - do not edit. Floats are bit patterns. */", "#include <stdint.h>",
        Format("#define NUM_TRIS %zu", tris.size()), Format("#define NUM_PROBES %zu", cases.size()), Format("#define LADDER %d", zmeas::P),
        "typedef struct { unsigned short cell; unsigned char is_quad; uint32_t v[3][3]; } tri_t;    /* x, y, z; quad is a macro on the console */",
        "typedef struct { unsigned short cell; uint32_t bz[LADDER]; } probe_t;",
        "static const tri_t tris[NUM_TRIS] = {" };
    for (const Json& t : tris.a) {
        std::string v;
        for (int k = 0; k < 3; k++)
            v += std::string(k ? ", " : "") + "{ " + Hex(t.at("x")[k].num()) + ", " + Hex(t.at("y")[k].num()) + ", " + Hex(t.at("z")[k].num()) + " }";
        lines.push_back(Format("    { %d, %d, { %s } },", (int)t.at("cell").integer(), t.at("quad").truthy() ? 1 : 0, v.c_str()));
    }
    lines.push_back("};");
    lines.push_back("static const probe_t probes[NUM_PROBES] = {");
    for (const Json& c : cases.a) {
        std::string v;
        for (size_t k = 0; k < c.at("ladder_in").size(); k++)
            v += std::string(k ? ", " : "") + Hex(c.at("ladder_in")[k].num());
        lines.push_back(Format("    { %d, { %s } },", (int)c.at("cell").integer(), v.c_str()));
    }
    lines.push_back("};");
    WriteLines(folder + "/cases.h", lines);
    WriteFile(folder + "/cases.json", data.dump());
}

// Does the model still give what a set was made with? The predicted depth of every case and the stored depth of
// every ladder value, against the numbers in the set (sets from gen.py hold the Python model's).
static int Check(const Json& data)
{
    long same = 0, all = 0;
    for (const Json& c : data.at("cases").a) {
        const std::vector<double> x = c.at("x").nums(), y = c.at("y").nums(), z = c.at("z").nums();
        const double px = c.at("pixels")[0][0].num(), py = c.at("pixels")[0][1].num();
        const int cell = (int)c.at("cell").integer();
        const double pred = IspDepth(model, x.data(), y.data(), z.data(), px, py), was = c.at("predicted").num();
        all++; same += pred == was || (pred != pred && was != was);
        for (size_t k = 0; k < c.at("ladder_in").size(); k++) {
            const double got = zmeas::QuadDepth(model, cell, px, py, c.at("ladder_in")[k].num()), had = c.at("ladder")[k].num();
            all++; same += got == had || (got != got && had != had);
        }
    }
    printf("the model gives %ld of the set's %ld predicted and ladder depths\n", same, all);
    return same == all ? 0 : 1;
}

int main(int argc, char** argv)
{
    std::string mix = "first", refinePath;
    long long seed = 1234567;
    for (int a = 1; a < argc; a++) {
        const std::string arg = argv[a];
        if (arg == "--regen" && a + 2 < argc) {
            Write(argv[a + 2], Json::Parse(ReadFile(argv[a + 1])));
            return 0;
        }
        if (arg == "--check" && a + 1 < argc)
            return Check(Json::Parse(ReadFile(argv[a + 1])));
        if (arg == "--mix" && a + 1 < argc) mix = argv[++a];
        else if (arg == "--seed" && a + 1 < argc) seed = atoll(argv[++a]);
        else if (arg == "--refine" && a + 1 < argc) refinePath = argv[++a];
        else Fail("gen [--mix first|general|zero|scene] [--seed N] [--refine meas.json]  |  gen --regen <set>.cases.json <folder>");
    }
    if (mix != "first" && mix != "general" && mix != "zero" && mix != "scene")
        Fail("no mix " + mix);
    Rng rng((uint64_t)seed);
    Json tris, cases = mix == "scene" ? MakeScene(rng, tris) : MakeCases(rng, mix);
    Json refine;
    if (!refinePath.empty())
        refine = Json::Parse(ReadFile(refinePath));
    const int missed = Ladders(cases, mix, refinePath.empty() ? nullptr : &refine);
    const size_t polygons = mix == "scene" ? tris.size() : cases.size(), measured = cases.size();
    Json data = Json::Object().set("mix", mix).set("seed", seed).set("refined", !refinePath.empty()).set("cases", std::move(cases));
    if (mix == "scene")
        data.set("tris", std::move(tris));
    Write(Here(), data);
    printf("%s seed %lld: %zu polygons, %zu measured cells, %d pages; %s; ladder targets with no exact input: %d\n", mix.c_str(), seed, polygons,
           measured, zmeas::P, refinePath.empty() ? "first pass" : "refined ladders", missed);
    return 0;
}
