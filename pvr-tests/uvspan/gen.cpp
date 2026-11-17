/* Cases for uvspan: where does the TSP start a line of a triangle when part of the line is hidden?

       gen [--seed N] [--list op|tr]              ->  cases.h, cases.json (here)
       gen --regen <set>.cases.json <folder>      ->  the tables of an archived set

   uvmeas found that the TSP evaluates a plane once per line of a triangle in a tile, at the first pixel the
   triangle covers there, and steps from it (every step rounds down, so the start is visible in the values).
   Here an opaque red band, nearer than the triangle, hides some columns of the cell. Is the start still the
   first pixel the triangle covers (H1), or the first visible pixel of each run of the triangle's pixels (H2)?

   Every cell: one textured triangle as in uvmeas --mix general (depth 1, steep U), and by cell number mod 4:
   no band / a band over the left part of the triangle / a band in its middle / two bands. With --list op all
   of it is in the opaque list with depth test and write, with --list tr in the translucent list. A triangle
   is picked, among a few random ones, for the number of pixels on which the two hypotheses give different
   readings.

   The texture is 1024 texels wide and texel i holds the number i (main.c), so a pixel reads
   floor(frac(U) * 1024). What U is at a pixel under either hypothesis comes from the model, which is refsw2's
   own headers (through ../../model/gen_common.h): which pixels of the cell the triangle covers (pvr_isp_edge.h),
   the planes U / w and 1 / w evaluated from the first pixel of the pixel's run (pvr_tsp_plane.h), and the
   TSP's division of the two.

   (What a console answered is in the README: in the opaque list every run of visible pixels starts anew, H2;
   in the translucent list the triangle is shaded whole before the band is drawn over it, H1.)

   This replaces gen.py, which took the same three things from a copy of the model in Python (pvr-tests/ops/model.py).
   The sets made by that one are in fit/uvspan_data and run as before (--regen); a seed gives other cases
   here than it gave there. */
#include "gen_common.h"

using namespace gen;

static const int PAGES = 8;         // of 300 cells each
static const int TRIES = 4;         // the random triangles a cell picks its own from
static const Model model;           // the fitted model

static const char* const groupNames[4] = { "no band", "left band", "middle band", "two bands" };

struct Tri {
    double x[3], y[3], u[3], z[3];              // single precision values; the corners in rasterizer order
    int m;                                      // U is around 2^m
    int kind;                                   // its group: an index into groupNames
    std::vector<std::pair<int, int>> bands;     // each hides the columns first .. second - 1 of the cell
};

// The pixels of a triangle's cell, [row][column].
struct Cell {
    int left, top;                  // the first pixel of the cell, which is a tile
    bool cov[32][32];               // the triangle covers the pixel (the ISP's inside test)
    bool inside[32][32];            // the pixel's center is more than 1.5 pixels inside the triangle: read.py takes these
    bool hid[32][32];               // a band is over the pixel
};

static double Mant(Rng& rng, int bits)          // a random mantissa of that many bits, 1 <= r < 2
{
    return (double)rng.integers((int64_t)1 << (bits - 1), (int64_t)1 << bits) / (double)((int64_t)1 << (bits - 1));
}

static void Pixels(const Tri& t, Cell& c)
{
    c.left = (int)(floor(std::min({ t.x[0], t.x[1], t.x[2] }) / 32) * 32);
    c.top = (int)(floor(std::min({ t.y[0], t.y[1], t.y[2] }) / 32) * 32);
    Coverage cov;
    cov.Setup(model, t.x, t.y, 3, c.left, c.top);
    const double area = (t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]);
    const double sg = area > 0 ? 1 : area < 0 ? -1 : 0;
    for (int j = 0; j < 32; j++)
        for (int i = 0; i < 32; i++) {
            const double px = c.left + i + 0.5, py = c.top + j + 0.5;
            double d = HUGE_VAL;                // the distance of the center from the nearest edge, positive inside
            for (int k = 0; k < 3; k++) {
                const int n = (k + 1) % 3;
                const double ex = t.x[n] - t.x[k], ey = t.y[n] - t.y[k];
                d = std::min(d, (ex * (py - t.y[k]) - ey * (px - t.x[k])) / hypot(ex, ey) * sg);
            }
            c.cov[j][i] = cov.Inside(i, j);
            c.inside[j][i] = d > 1.5;
            c.hid[j][i] = false;
        }
    for (const auto& b : t.bands)
        for (int j = 0; j < 32; j++)
            for (int i = std::max(b.first, 0); i < std::min(b.second, 32); i++)
                c.hid[j][i] = true;
}

// Where the run of a pixel starts, as a column of the cell. h1: the first pixel of its line that the triangle
// covers (0 for a line the triangle is not on). h2: the first pixel of its run of covered pixels that no band
// hides (0 for a pixel that is in no such run).
static void Starts(const Cell& c, int h1[32], int h2[32][32])
{
    for (int j = 0; j < 32; j++) {
        h1[j] = 0;
        for (int i = 31; i >= 0; i--)
            if (c.cov[j][i]) h1[j] = i;
        int run = -1;
        for (int i = 0; i < 32; i++) {
            const bool visible = c.cov[j][i] && !c.hid[j][i];
            if (!visible) run = -1;
            else if (run < 0) run = i;
            h2[j][i] = visible ? run : 0;
        }
    }
}

// The two planes of a triangle the TSP makes U of: U / w and 1 / w (1 / w is the z of the corners).
struct Planes {
    pvrtsp::Plane u, w;

    void Setup(const Tri& t)
    {
        float fx[3], fy[3], fz[3], fu[3];
        for (int k = 0; k < 3; k++) {
            fx[k] = (float)t.x[k]; fy[k] = (float)t.y[k]; fz[k] = (float)t.z[k]; fu[k] = (float)t.u[k];
        }
        u.Setup(fx, fy, fz, fu, model.plane);
        w.Setup(fx, fy, fz, nullptr, model.plane);
    }

    // The texel number the test reads at pixel (i, j) of the cell when the pixel's run starts at column
    // `start`: both planes are evaluated at the center of that pixel and stepped to this one, and divided.
    int Reading(const Cell& c, int i, int j, int start) const
    {
        if (u.bad || w.bad)
            return 0;
        const int64_t x0 = 2 * (int64_t)(c.left + start) + 1, y = 2 * (int64_t)(c.top + j) + 1;      // in half pixels
        const double U = TspDivide(u.Value(x0, y, i - start, c.left, c.top), w.Value(x0, y, i - start, c.left, c.top));
        if (!std::isfinite(U))
            return 0;
        return (int)((long long)floor((U - floor(U)) * 1024) % 1024);
    }
};

// What a triangle is picked for: the number of pixels the reader takes (inside, and not under a band) on which
// the two hypotheses give different readings. Without a band they cannot differ: there, the number of pixels
// the reader takes.
static int Score(const Tri& t)
{
    Cell c;
    Pixels(t, c);
    int n = 0;
    if (t.bands.empty()) {
        for (int j = 0; j < 32; j++)
            for (int i = 0; i < 32; i++)
                n += c.inside[j][i] && !c.hid[j][i];
        return n;
    }
    int h1[32], h2[32][32];
    Starts(c, h1, h2);
    Planes planes;
    planes.Setup(t);
    for (int j = 0; j < 32; j++)
        for (int i = 0; i < 32; i++)
            if (c.inside[j][i] && !c.hid[j][i] && planes.Reading(c, i, j, h1[j]) != planes.Reading(c, i, j, h2[j][i]))
                n++;
    return n;
}

// A random triangle for a cell, with the bands of the cell's group.
static Tri Candidate(Rng& rng, int cell)
{
    const double cx = CellX(cell), cy = CellY(cell);
    const size_t ck = rng.choice({ 0.25, 0.25, 0.5 });          // the corners: integers, multiples of 1/16, any float
    Tri t;
    double px[3], py[3];                                        // the corners in the cell, 0 .. 32
    for (;;) {
        for (int k = 0; k < 3; k++) {
            px[k] = rng.uniform(0.0, 32.0);
            py[k] = rng.uniform(0.0, 32.0);
            if (ck == 0) { px[k] = nearbyint(px[k]); py[k] = nearbyint(py[k]); }
            else if (ck == 1) { px[k] = nearbyint(px[k] * 16) / 16; py[k] = nearbyint(py[k] * 16) / 16; }
            t.x[k] = F32(cx + px[k]); t.y[k] = F32(cy + py[k]);
        }
        if (fabs((t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0])) > 500)      // an area above 250 pixels, a quarter of the cell
            break;
    }
    // U: 2^m times a 24 bit mantissa, and at each corner up to a half of it off (3 of 10: up to 1/4 .. 1/32)
    t.m = (int)rng.integers(6, 10);
    const double u0 = ldexp(Mant(rng, 24), t.m);
    const double rel = rng.chance(0.7) ? 0.5 : ldexp(1.0, -(int)rng.integers(2, 6));
    for (int k = 0; k < 3; k++) {
        t.u[k] = F32(u0 * (1.0 + rng.uniform(-1, 1) * rel));
        t.z[k] = 1.0;
    }
    // the bands keep away from the ends of the triangle (lo, hi: two columns inside its extent). A triangle
    // too narrow for them would get none; none is, as an area that large takes a width above 15
    const int lo = (int)std::min({ px[0], px[1], px[2] }) + 2, hi = (int)std::max({ px[0], px[1], px[2] }) - 2;
    t.kind = cell % 4;
    if (t.kind == 0 || hi - lo < 8)
        t.kind = 0;
    else if (t.kind == 1)                                       // from the cell's left edge into the triangle
        t.bands.push_back({ 0, (int)rng.integers(lo + 2, (lo + hi) / 2 + 1) });
    else if (t.kind == 2) {                                     // 1 to 3 columns in the middle
        const int a = (int)rng.integers(lo + 2, hi - 4);
        t.bands.push_back({ a, a + (int)rng.integers(1, 4) });
    } else {                                                    // one column, and two columns further right
        const int a = (int)rng.integers(lo + 2, (lo + hi) / 2);
        const int b = a + 3 < hi - 1 ? (int)rng.integers(a + 3, hi - 1) : a + 3;
        t.bands.push_back({ a, a + 1 });
        t.bands.push_back({ b, b + 2 });
    }
    return t;
}

static Json TriJson(const Tri& t)
{
    Json bands = Json::Array();
    for (const auto& b : t.bands)
        bands.push(Json::Array().push(b.first).push(b.second));
    return Json::Object().set("x", Json::List(t.x, 3)).set("y", Json::List(t.y, 3)).set("u", Json::List(t.u, 3)).set("z", Json::List(t.z, 3))
        .set("m", t.m).set("bands", std::move(bands)).set("group", groupNames[t.kind]);
}

// cases.h and cases.json from the description of a set (also used to run an archived set again).
// Every float goes into cases.h as the bit pattern of the single precision number: the test program never
// converts or calculates one, so a PC and the console send the same bits.
static void Write(const std::string& folder, const Json& data)
{
    const Json& pages = data.at("pages");
    size_t count = 0;
    for (const Json& tris : pages.a) count += tris.size();
    const bool tr = data.has("list") && data.at("list").str() == "tr";
    std::vector<std::string> lines = {
        "/* Generated by pvr-tests/uvspan/gen (gen.cpp) - do not edit. Floats are bit patterns. */", "#include <stdint.h>",
        Format("#define NUM_TRIS %zu", count), Format("#define LIST_TR %d", tr ? 1 : 0),
        "typedef struct { unsigned char page; uint32_t v[3][4]; } tri_t;   /* x, y, u, z */", "static const tri_t tris[NUM_TRIS] = {" };
    std::vector<std::string> bands;
    for (size_t p = 0; p < pages.size(); p++)
        for (size_t cell = 0; cell < pages[p].size(); cell++) {
            const Json& t = pages[p][cell];
            std::string v;
            for (int k = 0; k < 3; k++)
                v += std::string(k ? ", " : "") + "{ " + Hex(t.at("x")[k].num()) + ", " + Hex(t.at("y")[k].num()) + ", " + Hex(t.at("u")[k].num()) + ", "
                     + Hex(t.at("z")[k].num()) + " }";
            lines.push_back(Format("    { %zu, { %s } },", p, v.c_str()));
            for (const Json& b : t.at("bands").a)
                bands.push_back(Format("    { %zu, %zu, %d, %d },", p, cell, (int)b[0].integer(), (int)b[1].integer()));
        }
    lines.push_back("};");
    lines.push_back(Format("#define NUM_BANDS %zu", bands.size()));
    lines.push_back("typedef struct { unsigned char page; short cell, a, b; } band_t;   /* columns a .. b-1 of the cell */");
    lines.push_back("static const band_t bands[NUM_BANDS] = {");
    lines.insert(lines.end(), bands.begin(), bands.end());
    lines.push_back("};");
    WriteLines(folder + "/cases.h", lines);
    WriteFile(folder + "/cases.json", data.dump());
}

int main(int argc, char** argv)
{
    std::string list = "op";
    long long seed = 1;
    for (int a = 1; a < argc; a++) {
        const std::string arg = argv[a];
        if (arg == "--regen" && a + 2 < argc) {
            Write(argv[a + 2], Json::Parse(ReadFile(argv[a + 1])));
            return 0;
        }
        if (arg == "--seed" && a + 1 < argc) seed = atoll(argv[++a]);
        else if (arg == "--list" && a + 1 < argc) list = argv[++a];
        else Fail("gen [--seed N] [--list op|tr]  |  gen --regen <set>.cases.json <folder>");
    }
    if (list != "op" && list != "tr")
        Fail("no list " + list + " (op, tr)");
    Rng rng((uint64_t)seed);
    Json pages = Json::Array();
    size_t triangles = 0, bands = 0;
    long total = 0;
    for (int p = 0; p < PAGES; p++) {
        Json tris = Json::Array();
        for (int cell = 0; cell < 300; cell++) {
            Tri best;
            int most = -1;
            for (int k = 0; k < TRIES; k++) {
                const Tri t = Candidate(rng, cell);
                const int n = Score(t);
                if (n > most) { most = n; best = t; }
            }
            if (!best.bands.empty())
                total += most;
            bands += best.bands.size();
            tris.push(TriJson(best));
        }
        triangles += tris.size();
        pages.push(std::move(tris));
    }
    Write(Here(), Json::Object().set("seed", seed).set("list", list).set("pages", std::move(pages)));
    puts(Format("seed %lld: %d pages, %zu triangles, %zu bands, %ld pixels on which the hypotheses differ", seed, PAGES, triangles, bands, total).c_str());
    return 0;
}
