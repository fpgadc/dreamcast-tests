/* Cases for covmeas: which pixels does the ISP take as inside a triangle, when an edge passes (almost)
   exactly through pixel centers?

       gen [--seed N] [--mix lattice|tile|axis|diag|skew|lean|leantile|big|sliver|degen|tiny]
           [--prim tri|strip|sprite|modvol|zprobe] [--list op|tr|pt] [--halfofs N] [--cull none|small|ccw|cw]
           [--z Z] [--zs Z0,Z1,Z2] [--vcol flat|gouraud] [--probez Z]         ->  cases.h, cases.json (here)
       gen --regen <set>.cases.json <folder>                                  ->  the tables of an archived set

   --prim: what carries the triangle's edges. tri: a triangle of its own (the default). strip: a strip of two
   triangles, the triangle and a second one across its edge v1 v2. sprite: a sprite whose first three corners
   are the triangle's. modvol: the triangle is a modifier volume of its own in front of a white polygon with
   cheap shadow, and darkens what it covers.
   zprobe: the triangle is red and writes its depth; a green polygon over the whole screen at depth --probez
   (default 0.5) with compare GREATER follows. Red: drawn. Green: the ISP did not cover the pixel. Black: the
   ISP covered it (the depth is there and rejects the green polygon) but nothing was shaded.
   --list: the list the polygons go to (opaque, translucent, punch-through).
   --halfofs N: the HALF_OFFSET register (default 7). With bit 0 clear the ISP samples at pixel corners, and the
   special edges are moved by half a pixel to pass through those.
   --cull: the culling mode of the polygons (default none; ccw is CullMode_Negative, cw CullMode_Positive).
   --z: the depth of the polygons (default 1.0). --zs: three depths, one per corner of a triangle (tri and
   zprobe), e.g. 0.2,0.5,0.8. --vcol: corners red, green, blue; flat or Gouraud shaded (tri).

   Every cell has one white triangle. One of its edges runs along a lattice direction (p, q), small coprime
   integers, through a pixel center P0, so that it passes through the centers P0 + k (p, q) as well: up to a
   handful of pixels per triangle whose edge function is zero or a few units of the last bit away from it. What
   the console draws there shows how it rounds the edge function. Kinds:
       exact      vertices on the line, as single precision allows (the line misses the centers by rounding only)
       ulp        as exact, with coordinates moved by a few units of their last bit
       sixteenth  vertices on the 1/16 pixel grid and exactly on the line: the edge function is exactly zero at
                  the centers in real arithmetic, and the tie rule decides
   The vertex order (which edge is the special one) and the winding are random.

   --mix axis / diag: only horizontal and vertical (or only 45 degree) lattice edges, kind exact: the simplest
   cases, many of them.
   --mix skew: horizontal and vertical lattice edges whose ends are moved across the line by a few units of the
   last bit: one slope is tiny against the other.
   --mix lean: one edge is nearly horizontal (or nearly vertical) and spans most of the tile, some reaching past
   its first or last pixel; from end to end it leans by anything from a quarter of the last kept bit of its
   slope to 1/16 pixel. Its height is chosen with the model (refsw2's edge unit, pvr_isp_edge.h, through
   ../model/gen_common.h) so that the edge function is close to zero somewhere on the row: either at the tile's
   first column (the whole row is then decided at once) or further along. The generator says on stderr how many
   edges it could place that way.
   --mix leantile: the same with triangles of 3x3 cells, the edge crossing two tile borders.
   --mix sliver: the third corner is 0.00001 to 0.05 pixel off the lattice edge (or exactly on it): triangles
   whose area is at or below what the setup's products resolve, so the determinant, and with it the winding, may
   come out zero or with the wrong sign.
   --mix degen: exactly degenerate triangles: a corner repeated (A A B, A B B, A B A), all three the same, or
   three corners exactly on one line (on the 1/16 grid, in any order); the line mostly a lattice line through
   pixel centers. With --prim strip the degenerate triangle is one of a strip's two (A A B C, A B C C, C A B A)
   or both (A B B C), the way strips are joined.
   --mix tiny: three corners within 1 unit of the last place to 0.1 pixel of one point (a pixel center, or
   anywhere): almost a point. The constants of its edges are then pure rounding, and whether it draws nothing or
   whole rows of its tile depends on how they fall.
   --mix big: triangles with two corners far outside the screen (40 to 30000 pixels), in the cells along the
   screen's border: a wedge that opens outward from a corner inside the cell, one side along a lattice direction
   through pixel centers. The products of the setup are large here and lose whole pixels' worth of the edge
   function. (Triangles only: --prim tri, modvol or zprobe.)
   --mix tile: triangles of several cells' size, the lattice edge crossing tile borders (is the edge function
   evaluated per tile from a start value, as the depth is?).

   A set is 8 pages of one triangle per cell: 300 cells; 30 regions of 3x3 cells for tile and leantile; the 66
   border cells for big. cases.json describes it (and is what gets archived with a console's readings, in
   fit/covmeas_data): the arguments, and per page and triangle
       x, y    the corners in the order they are sent (3; 4 for a strip or a sprite)
       group   the kind, as the evaluation counts them
       dir     the direction (p, q) of the special edge, p0 a pixel center on it (the sample point, with
               --halfofs), edge: which two corners it joins

   This replaces gen.py, which took the edge values of the lean mixes from a model of its own in Python
   (ops/model.py). The sets made by that one are in fit/covmeas_data and run as before (--regen); a seed gives
   other cases here than it gave there. */
#include <numeric>
#include "gen_common.h"

using namespace gen;

static const int PAGES = 8;
static const Model model;
static double CENTER = 0.5;         // where the ISP samples a pixel: 0.5, or 0 with HALF_OFFSET bit 0 clear

static const std::vector<std::string> MIXES = { "lattice", "tile", "axis", "diag", "skew", "lean", "leantile", "big", "sliver", "degen", "tiny" };
static const std::vector<std::string> PRIMS = { "tri", "strip", "sprite", "modvol", "zprobe" };        // COV_PRIM is the index
static const std::vector<std::string> LISTS = { "op", "tr", "pt" };
static const char* const LIST_NAMES[3] = { "ListType_Opaque", "ListType_Translucent", "ListType_Punch_Through" };
static const std::vector<std::string> CULLS = { "none", "small", "ccw", "cw" };
static const char* const CULL_NAMES[4] = { "CullMode_None", "CullMode_Small", "CullMode_Negative", "CullMode_Positive" };
static const std::vector<std::string> VCOLS = { "", "flat", "gouraud" };                                // COV_VCOL is the index

// A point, with the arithmetic the placement needs. Every random number is drawn in a statement of its own:
// the order in which C++ evaluates the operands of an expression is not fixed, and a set has to come out
// again from its seed.
struct V2 {
    double x, y;
    V2 operator+(const V2& o) const { return { x + o.x, y + o.y }; }
    V2 operator-(const V2& o) const { return { x - o.x, y - o.y }; }
    V2 operator*(double s) const { return { x * s, y * s }; }
    V2 operator/(double s) const { return { x / s, y / s }; }
};
static V2 operator*(double s, const V2& v) { return { s * v.x, s * v.y }; }
static double Cross(const V2& a, const V2& b) { return a.x * b.y - a.y * b.x; }
static double Dot(const V2& a, const V2& b) { return a.x * b.x + a.y * b.y; }
static double Length(const V2& v) { return hypot(v.x, v.y); }
static V2 Single(const V2& v) { return { F32(v.x), F32(v.y) }; }         // as it goes to the hardware

// is any of the points outside x0 .. x1, y0 .. y1?
static bool Outside(const V2* pts, int n, double x0, double x1, double y0, double y1)
{
    for (int k = 0; k < n; k++)
        if (pts[k].x < x0 || pts[k].x > x1 || pts[k].y < y0 || pts[k].y > y1)
            return true;
    return false;
}

// The lattice directions (p, q): coprime integers up to 5, one of each pair of opposite directions.
struct Dir { int p, q; };
static std::vector<Dir> Lattice()
{
    std::vector<Dir> dirs;
    for (int p = -5; p <= 5; p++)
        for (int q = 0; q <= 5; q++)
            if ((p || q) && std::gcd(abs(p), q) == 1 && (q > 0 || p > 0))
                dirs.push_back({ p, q });
    return dirs;
}
static const std::vector<Dir> DIRS = Lattice();
static const std::vector<Dir> AXES = { { 1, 0 }, { 0, 1 } }, DIAGONALS = { { 1, 1 }, { -1, 1 } };
static Json Whole(const Dir& d) { return Json::Array().push(d.p).push(d.q); }

// One case, as cases.json holds it (see the top).
struct Case {
    std::vector<double> x, y;
    std::string group;
    Json dir;
    V2 p0;
    int edge[2] = { 0, 1 };
    // lean and leantile: what the search of the edge's height was asked for (Goal), and whether it got there
    int wanted = -1;
    bool met = false;
};

static Json CaseJson(const Case& t)
{
    return Json::Object().set("x", Json::List(t.x)).set("y", Json::List(t.y)).set("group", t.group).set("dir", t.dir)
        .set("p0", Json::Array().push(t.p0.x).push(t.p0.y)).set("edge", Json::Array().push(t.edge[0]).push(t.edge[1]));
}

// v moved by up to k units of its last place, either way
static double UlpMove(Rng& rng, double v, int k)
{
    return Step(v, (int)rng.integers(-k, k + 1));
}

// Kind "ulp": each coordinate of the special edge's two ends (pts[0], pts[1]) is moved, with probability
// 0.6, by up to 3 units of its last place.
static void UlpEnds(Rng& rng, V2* pts)
{
    for (int i = 0; i < 2; i++) {
        if (rng.chance(0.6)) pts[i].x = UlpMove(rng, pts[i].x, 3);
        if (rng.chance(0.6)) pts[i].y = UlpMove(rng, pts[i].y, 3);
    }
}

// The three corners in a random order: the special edge (pts[0] to pts[1]) is then any of the triangle's three
// and the winding either one. "edge" says where its two ends went.
static Case Shuffled(Rng& rng, const V2* pts, const std::string& group, const Json& dir, const V2& p0)
{
    const std::vector<int> order = rng.permutation(3);
    Case t;
    for (int k = 0; k < 3; k++) {
        t.x.push_back(pts[order[k]].x); t.y.push_back(pts[order[k]].y);
        if (order[k] < 2) t.edge[order[k]] = k;
    }
    t.group = group; t.dir = dir; t.p0 = p0;
    return t;
}

// A lattice edge in the region of span x span cells at (cx, cy): through the pixel center P0, at least 4
// pixels inside the region, from a = P0 - s1 d to b = P0 + s2 d, d = (p, q) one of dirs. s1 and s2 are anything
// from 0.6 to 12 (times span), or, sixteenth, whole sixteenths from 0.5 to 16 (times span), which puts a and b
// on the 1/16 pixel grid. n: the unit normal, to either side.
struct LatticeEdge { Dir dir; V2 P0, d, a, b, n; };
static LatticeEdge MakeEdge(Rng& rng, double cx, double cy, int span, const std::vector<Dir>& dirs, bool sixteenth)
{
    LatticeEdge e;
    const int W = 32 * span;
    e.dir = dirs[rng.integers(0, (int64_t)dirs.size())];
    const double px = cx + (double)rng.integers(4, W - 4) + CENTER;
    const double py = cy + (double)rng.integers(4, W - 4) + CENTER;
    e.P0 = { px, py };
    e.d = { (double)e.dir.p, (double)e.dir.q };
    double s1, s2;
    if (sixteenth) {
        s1 = (double)rng.integers(8, 16 * 16 * span) / 16.0;
        s2 = (double)rng.integers(8, 16 * 16 * span) / 16.0;
    } else {
        s1 = rng.uniform(0.6, 12.0 * span);
        s2 = rng.uniform(0.6, 12.0 * span);
    }
    e.a = e.P0 - s1 * e.d;
    e.b = e.P0 + s2 * e.d;
    const double side = rng.sign();
    e.n = V2{ -e.d.y, e.d.x } / Length(e.d) * side;
    return e;
}

// A triangle with a lattice edge; the third corner is 7 to 24 pixels (times span) off the edge, a sixteenth
// one on the 1/16 grid as well. span: the size of the region the triangle may use, in cells (1: inside its own
// cell). kinds: the weights of exact, ulp, sixteenth. skew: not zero for an axis-aligned edge whose ends are
// moved across the line by up to that many units of the last place, with probability 0.75 each.
static Case Triangle(Rng& rng, int cell, int span, const std::vector<Dir>& dirs, const std::vector<double>& kinds, int skew = 0)
{
    static const char* const names[3] = { "exact", "ulp", "sixteenth" };
    const double cx = CellX(cell), cy = CellY(cell), W = 32.0 * span;
    const size_t kind = rng.choice(kinds);
    for (int tries = 0; tries < 1000; tries++) {
        const LatticeEdge e = MakeEdge(rng, cx, cy, span, dirs, kind == 2);
        const double along = rng.uniform(-0.4, 0.4);
        const double off = rng.uniform(7, 24 * span);
        V2 c = (e.a + e.b) / 2 + along * (e.b - e.a) + e.n * off;
        if (kind == 2)
            c = { nearbyint(c.x * 16) / 16, nearbyint(c.y * 16) / 16 };
        V2 pts[3] = { e.a, e.b, c };
        if (Outside(pts, 3, cx + 0.25, cx + W - 0.25, cy + 0.25, cy + W - 0.25))
            continue;
        if (Length(e.b - e.a) < 5)
            continue;
        for (V2& p : pts) p = Single(p);
        if (kind == 1)
            UlpEnds(rng, pts);
        if (skew) {
            for (int i = 0; i < 2; i++)
                if (rng.chance(0.75)) {
                    if (e.dir.q == 0) pts[i].y = UlpMove(rng, pts[i].y, skew);
                    else pts[i].x = UlpMove(rng, pts[i].x, skew);
                }
        }
        return Shuffled(rng, pts, skew ? "skew" : names[kind], Whole(e.dir), e.P0);
    }
    Fail("no triangle");
}

// The special edge of a lean triangle as the search looks at it: the model's values of the edge from corner a
// to the corner after it (in the order the corners are sent) at 32 pixels in a line, from the pixel `first` of
// its tile to the right, or, down, downward. They come from refsw2's edge unit as the ISP's inside test of the
// triangle sets it up for that tile (gen::Coverage): the value at pixel (i, j) of the tile is
// t0 + tx * i + ty * j, turned so that inside is positive, and zero2 is the zero zone in the same units. Here
// their sizes are counted in sixty-fourths of the zero zone (`unit`): at most 64 is a zero.
// false when the tile has no values (pvrisp::EdgeTile::constant).
struct Along { int64_t unit, first, least, leastLater; };       // of pixel 0, the least of all 32, the least of pixels 2 .. 29
static bool EdgeAlong(const double* x, const double* y, int a, const Pixel& first, bool down, Along& v)
{
    Coverage inside;
    inside.Setup(model, x, y, 3, first.tx, first.ty, CENTER != 0);
    const pvrisp::EdgeTile& e = inside.e[a];
    if (inside.bad || e.constant)
        return false;
    v.unit = e.zero2 / 64;
    v.first = v.least = v.leastLater = INT64_MAX;
    for (int n = 0; n < 32; n++) {
        const int64_t size = llabs(e.t0 + e.tx * (first.i + (down ? 0 : n)) + e.ty * (first.j + (down ? n : 0)));
        if (n == 0) v.first = size;
        if (n >= 2 && n < 30) v.leastLater = std::min(v.leastLater, size);
        v.least = std::min(v.least, size);
    }
    return true;
}

// What the search of a lean edge's height is asked for, in sixty-fourths of the zero zone (EdgeAlong):
//   Row     (a nearly horizontal edge, 4 in 10) the value at the tile's first column is at most 70: zero, or
//           next to it. A row that starts at zero is decided as a whole.
//   Later   (the other nearly horizontal ones) the first column is not zero (above 64), and one of the columns
//           2 to 29 is (at most 64)
//   Column  (a nearly vertical edge) one of the 32 pixels of the column comes within 150
// A height that only comes within 400 somewhere is kept in case none of the 80 tried gets there.
enum Goal { Row, Later, Column };

// A triangle with a nearly axis-aligned edge along a row (or a column, 35 in 100) of pixel centers, in the
// region of span x span cells. "along" is the coordinate the edge runs along, "across" the other one.
//   - the edge runs along row (column) j of the region, from lo to W - hi along it: lo is 0.6 to 7, or, 1 in 4,
//     0.27 to 0.48, short of the first pixel center; hi alike (0.6 to 7.5; 3 in 10: 0.26 to 0.48)
//   - one end is k units of the last place of the row's coordinate higher than the other: the lean, 0.25 to
//     4000 units of the 21st bit under the edge's length (2^-16 pixel for a length of 16 to 32), either way
//   - both ends together are m units off the row, m one of 80 values out of -60 .. 60 in random order: the
//     first that the model says meets the Goal, else the first that comes near (and another try if none does)
//   - the third corner is 6 to 22 pixels (times span) to one side, as the region allows
static Case LeanTriangle(Rng& rng, int cell, int span = 1)
{
    const double cx = CellX(cell), cy = CellY(cell), W = 32.0 * span;
    for (int tries = 0; tries < 400; tries++) {
        const bool vert = rng.chance(0.35);
        const double oAlong = vert ? cy : cx, oAcross = vert ? cx : cy;
        const int j = (int)rng.integers(2, 32 * span - 2);
        const bool shortLo = rng.chance(0.25);
        const double lo = shortLo ? rng.uniform(0.27, 0.48) : rng.uniform(0.6, 7.0);
        const bool shortHi = rng.chance(0.3);
        const double hi = W - (shortHi ? rng.uniform(0.26, 0.48) : rng.uniform(0.6, 7.5));
        const double aAlong = F32(oAlong + lo), bAlong = F32(oAlong + hi);
        const double across = oAcross + j + CENTER;
        const double ulp = Step(across, 1) - across;
        int exponent;
        frexp(bAlong - aAlong, &exponent);
        const double G = ldexp(1.0, exponent - 21);
        const double size = rng.logUniform(0.25, 4000.0);
        const double lean = size * G * rng.sign();
        int k = (int)nearbyint(lean / ulp);
        if (k == 0)
            k = rng.sign();
        const int side = rng.sign();
        const double room = side < 0 ? j - 0.5 : W - j - 1.5;
        const double far = across + side * rng.uniform(6.0, room > 6.5 ? std::min(22.0 * span, room) : 6.4);
        if (far < oAcross + 0.3 || far > oAcross + W - 0.3)
            continue;
        const double cAlong = rng.uniform(oAlong + lo + 2.0, oAlong + hi - 2.0);
        const std::vector<int> order = rng.permutation(3);
        const Goal goal = rng.chance(0.4) ? Row : Later;
        const std::vector<int> ms = rng.permutation(121);

        // the special edge in the order the corners are sent: from corner ia to the one after it
        int ia = 0, ib = 0;
        for (int c = 0; c < 3; c++) {
            if (order[c] == 0) ia = c;
            if (order[c] == 1) ib = c;
        }
        if ((ib - ia + 3) % 3 != 1)
            std::swap(ia, ib);
        // the row (column) of the edge in the tile next to the edge's low end: the 32 pixels the search looks at
        const Pixel first(vert ? cx + j + CENTER : cx + CENTER, vert ? cy + CENTER : cy + j + CENTER, CENTER != 0);

        Case t;
        bool found = false;
        for (int n = 0; n < 80 && !t.met; n++) {
            const int m = ms[n] - 60;
            V2 pts[3] = { { aAlong, across + m * ulp }, { bAlong, across + (m + k) * ulp }, { cAlong, far } };
            if (vert)
                for (V2& p : pts) std::swap(p.x, p.y);
            double x[3], y[3];
            for (int c = 0; c < 3; c++) {
                x[c] = F32(pts[order[c]].x); y[c] = F32(pts[order[c]].y);
            }
            Along v;
            if (!EdgeAlong(x, y, ia, first, vert, v))
                continue;
            const bool good = vert ? v.least <= 150 * v.unit : goal == Row ? v.first <= 70 * v.unit : v.first > 64 * v.unit && v.leastLater <= 64 * v.unit;
            if (good || (!found && v.least <= 400 * v.unit)) {
                t.x.assign(x, x + 3); t.y.assign(y, y + 3);
                t.met = good;
                found = true;
            }
        }
        if (!found)
            continue;
        t.group = vert ? "lean-v" : "lean-h";
        t.dir = Whole(vert ? Dir{ 0, 1 } : Dir{ 1, 0 });
        t.p0 = vert ? V2{ cx + j + CENTER, cy + 16 + CENTER } : V2{ cx + 16 + CENTER, cy + j + CENTER };
        t.edge[0] = ia; t.edge[1] = ib;
        t.wanted = vert ? Column : goal;
        return t;
    }
    Fail("no triangle");
}

// A lattice edge through pixel centers and a third corner almost on it: 0.00001 to 0.05 pixel off the edge
// (or, 1 in 10, exactly on the line, as far as double precision puts it there), 0.1 to 0.9 of the way along.
static Case SliverTriangle(Rng& rng, int cell)
{
    static const char* const names[3] = { "exact", "ulp", "sixteenth" };
    const double cx = CellX(cell), cy = CellY(cell);
    const size_t kind = rng.choice({ 0.4, 0.2, 0.4 });
    for (int tries = 0; tries < 1000; tries++) {
        const LatticeEdge e = MakeEdge(rng, cx, cy, 1, DIRS, kind == 2);
        const double delta = rng.chance(0.1) ? 0.0 : pow(10.0, rng.uniform(-5.0, -1.3));
        const double along = rng.uniform(0.1, 0.9);
        const V2 c = e.a + along * (e.b - e.a) + e.n * delta;
        V2 pts[3] = { e.a, e.b, c };
        if (Outside(pts, 3, cx + 0.25, cx + 31.75, cy + 0.25, cy + 31.75))
            continue;
        if (Length(e.b - e.a) < 5)
            continue;
        for (V2& p : pts) p = Single(p);
        if (kind == 1)
            UlpEnds(rng, pts);
        return Shuffled(rng, pts, std::string("sliver-") + names[kind], Whole(e.dir), e.P0);
    }
    Fail("no triangle");
}

// The corners of an exactly degenerate triangle (or, strip, of a strip of two triangles with one in it).
// A and B are on a lattice line through P0, whole eighths of (p, q) from it; M is their middle or a third point
// of the line (on the 1/16 grid either way); C is 7 to 20 pixels off the line, on the 1/16 grid. P0 is a pixel
// center, or, 1 in 4, up to 7/16 pixel off it in x and in y. The group names the corners sent, in order.
static Case DegenTriangle(Rng& rng, int cell, bool strip)
{
    static const char* const repeated[4] = { "aab", "abb", "aba", "aaa" };
    static const char* const onLine[4] = { "amb", "abm", "mab", "bma" };
    static const char* const strips[6] = { "aabc", "abcc", "abbc", "caba", "abmc", "camb" };
    const double cx = CellX(cell), cy = CellY(cell);
    const size_t kind = rng.choice({ 0.2, 0.2, 0.2, 0.1, 0.3 });        // the four of `repeated`, or three on a line
    for (int tries = 0; tries < 1000; tries++) {
        const Dir dir = DIRS[rng.integers(0, (int64_t)DIRS.size())];
        const V2 d = { (double)dir.p, (double)dir.q };
        const double px = cx + (double)rng.integers(6, 26) + CENTER;
        const double py = cy + (double)rng.integers(6, 26) + CENTER;
        V2 P0 = { px, py };
        if (rng.chance(0.25)) {         // off the pixel centers
            const double mx = (double)rng.integers(-7, 8) / 16.0;
            const double my = (double)rng.integers(-7, 8) / 16.0;
            P0 = P0 + V2{ mx, my };
        }
        double s[3];                    // eighths: the middle of two is on the 1/16 grid
        for (double& v : s) v = (double)rng.integers(-12 * 8, 12 * 8 + 1);
        std::sort(s, s + 3);
        for (double& v : s) v /= 8.0;
        const V2 a = P0 + s[0] * d, b = P0 + s[2] * d;
        const V2 mid = rng.chance(0.5) ? (a + b) / 2 : P0 + s[1] * d;
        const double side = rng.sign();
        const V2 n = V2{ -d.y, d.x } / Length(d) * side;
        const double along = rng.uniform(-0.3, 0.3);
        const double off = rng.uniform(7, 20);
        const V2 f = ((a + b) / 2 + along * (b - a) + n * off) * 16;
        const V2 far = { nearbyint(f.x) / 16, nearbyint(f.y) / 16 };
        const V2 all[4] = { a, b, mid, far };
        if (Outside(all, 4, cx + 0.3, cx + 31.7, cy + 0.3, cy + 31.7) || Length(b - a) < 4)
            continue;
        const std::string order = strip ? strips[rng.integers(0, 6)] : kind < 4 ? repeated[kind] : onLine[rng.integers(0, 4)];
        Case t;
        for (char ch : order) {
            const V2 p = Single(ch == 'a' ? a : ch == 'b' ? b : ch == 'm' ? mid : far);
            t.x.push_back(p.x); t.y.push_back(p.y);
        }
        t.group = (strip ? "strip " : "degenerate ") + order;
        t.dir = Whole(dir); t.p0 = P0;
        return t;
    }
    Fail("no triangle");
}

// Three corners almost at one point P: a pixel center, or, 1 in 2, up to half a pixel from one in x and in y.
// Half of them have every coordinate within k units of the last place of P's (k one of 1, 2, 4, 16, 64), the
// others within r pixels, r from 0.00001 to 0.1.
static Case TinyTriangle(Rng& rng, int cell)
{
    static const int steps[5] = { 1, 2, 4, 16, 64 };
    const double cx = CellX(cell), cy = CellY(cell);
    const double px = cx + (double)rng.integers(3, 29) + CENTER;
    const double py = cy + (double)rng.integers(3, 29) + CENTER;
    V2 P = { px, py };
    if (rng.chance(0.5)) {
        const double mx = rng.uniform(-0.5, 0.5);
        const double my = rng.uniform(-0.5, 0.5);
        P = P + V2{ mx, my };
    }
    P = Single(P);
    Case t;
    if (rng.chance(0.5)) {              // a few units of the last place apart
        const int k = steps[rng.integers(0, 5)];
        for (int c = 0; c < 3; c++) {
            const double x = UlpMove(rng, P.x, k);
            const double y = UlpMove(rng, P.y, k);
            t.x.push_back(x); t.y.push_back(y);
        }
        t.group = Format("tiny, within %d ulp", k);
    } else {
        const double r = pow(10.0, rng.uniform(-5.0, -1.0));
        for (int c = 0; c < 3; c++) {
            const double x = F32(P.x + rng.uniform(-r, r));
            const double y = F32(P.y + rng.uniform(-r, r));
            t.x.push_back(x); t.y.push_back(y);
        }
        t.group = Format("tiny, within 1e%d px", (int)ceil(log10(r)));
    }
    t.dir = Whole({ 1, 0 }); t.p0 = P;
    return t;
}

// The cells along the screen's border, each with the unit vector to the outside (the corner cells go with the
// top and the bottom row).
struct Place { int cell, ox, oy; };
static std::vector<Place> BorderCells()
{
    std::vector<Place> v;
    for (int c = 0; c < 20; c++) v.push_back({ c, 0, -1 });
    for (int c = 0; c < 20; c++) v.push_back({ 280 + c, 0, 1 });
    for (int r = 1; r < 14; r++) v.push_back({ 20 * r, -1, 0 });
    for (int r = 1; r < 14; r++) v.push_back({ 20 * r + 19, 1, 0 });
    return v;
}

// A wedge from a corner inside the border cell outward (ox, oy: the unit vector to the outside).
//   - each side has a direction of o steps outward and t along the border, coprime, o up to 7 and t no more
//     than o: at most 45 degrees off "outward"
//   - the special side passes through the pixel center P0, 8 to 28 pixels from the border: from A, up to 23/16
//     pixel further inside than P0, to B, a whole number of lattice steps beyond P0, 40 to 30000 pixels away
//   - the other side goes from A to C, 40 to 30000 pixels away
//   - A is inside the cell, and both sides leave the cell through its outer side, half a pixel or more from
//     that side's ends
// Kinds: exact, and ulp (the coordinates of A and B moved by up to 3 units of their last place).
static Case BigTriangle(Rng& rng, const Place& place)
{
    static const char* const names[2] = { "exact", "ulp" };
    const double cx = CellX(place.cell), cy = CellY(place.cell);
    const V2 out = { (double)place.ox, (double)place.oy }, tan = { -out.y, out.x };
    const size_t kind = rng.choice({ 0.6, 0.4 });
    for (int tries = 0; tries < 4000; tries++) {
        V2 d[2];
        bool usable = true;
        for (V2& side : d) {
            const int o = (int)rng.integers(1, 8);
            const int t = (int)rng.integers(-7, 8);
            if (abs(t) > o || std::gcd(o, abs(t)) != 1)
                usable = false;
            side = (double)o * out + (double)t * tan;
        }
        const V2 d1 = d[0], d2 = d[1];
        if (!usable || fabs(Cross(d1, d2)) < 1e-9)
            continue;
        const int r = (int)rng.integers(8, 29);             // of P0: pixels from the border ...
        const int along = (int)rng.integers(5, 27);         // ... and along it
        V2 P0;
        if (out.y != 0)
            P0 = { cx + along + CENTER, cy + (out.y < 0 ? r : 31 - r) + CENTER };
        else
            P0 = { cx + (out.x < 0 ? r : 31 - r) + CENTER, cy + along + CENTER };
        const V2 corner = { cx + (out.x > 0 ? 32.0 : 0.0), cy + (out.y > 0 ? 32.0 : 0.0) };      // a point of the cell's outer side
        const double back = (double)rng.integers(0, 24) / 16.0;
        const V2 A = P0 - d1 * back / std::max(fabs(d1.x), fabs(d1.y));
        const double reach1 = rng.logUniform(40.0, 30000.0);
        const V2 B = P0 + d1 * (double)(long long)(reach1 / Length(d1) + 1);
        const double reach2 = rng.logUniform(40.0, 30000.0);
        const V2 C = A + d2 * reach2 / Length(d2);
        V2 pts[3] = { Single(A), Single(B), Single(C) };
        const V2 lo = { cx + 0.3, cy + 0.3 }, hi = { cx + 31.7, cy + 31.7 }, across = { fabs(tan.x), fabs(tan.y) };
        if (pts[0].x < lo.x || pts[0].y < lo.y || pts[0].x > hi.x || pts[0].y > hi.y)
            continue;
        bool ok = true;
        for (int c = 1; c < 3 && ok; c++) {                 // where the side leaves the cell through its outer side
            const double k = Dot(corner - pts[0], out) / Dot(pts[c] - pts[0], out);
            if (!(0 < k && k < 1)) {
                ok = false;
                break;
            }
            const V2 e = pts[0] + k * (pts[c] - pts[0]);
            ok = Dot(e, across) > Dot(lo, across) + 0.5 && Dot(e, across) < Dot(hi, across) - 0.5;
        }
        if (!ok)
            continue;
        if (kind == 1)
            UlpEnds(rng, pts);
        return Shuffled(rng, pts, std::string("big-") + names[kind], Json::Array().push(d1.x).push(d1.y), P0);
    }
    Fail("no big triangle");
}

// The fourth corner of a strip or a sprite, for the triangle t in the region of span x span cells.
// strip: across the edge v1 v2 from v0. sprite: v0 + v2 - v1 (a parallelogram), moved a little.
// false when it does not fit into the region.
static bool Fourth(Rng& rng, const Case& t, int cell, int span, bool strip, V2& fourth)
{
    static const double scales[5] = { 1.0, 0.75, 0.5, 0.35, 0.2 };
    const double cx = CellX(cell), cy = CellY(cell), W = 32.0 * span;
    const V2 p[3] = { { t.x[0], t.y[0] }, { t.x[1], t.y[1] }, { t.x[2], t.y[2] } };
    for (double scale : scales) {
        const double range = strip ? 1.5 : 1.0;
        const double mx = rng.uniform(-range, range);
        const double my = rng.uniform(-range, range);
        V2 d;
        if (strip) {
            const V2 m = (p[1] + p[2]) / 2;
            d = m + scale * (m - p[0]) + V2{ mx, my } * scale;
        } else
            d = p[0] + scale * (p[2] - p[1]) + V2{ mx, my };
        d = Single(d);
        if (!(cx + 0.3 <= d.x && d.x <= cx + W - 0.3 && cy + 0.3 <= d.y && d.y <= cy + W - 0.3))
            continue;
        if (!strip) {                   // convex, the same way round at every corner, and no corner nearly straight
            const V2 q[4] = { p[0], p[1], p[2], d };
            bool left = true, right = true;
            for (int k = 0; k < 4; k++) {
                const double c = Cross(q[(k + 1) % 4] - q[k], q[(k + 2) % 4] - q[(k + 1) % 4]);
                left = left && c > 4;
                right = right && c < -4;
            }
            if (!(left || right))
                continue;
        } else if (fabs(Cross(p[2] - p[1], d - p[1])) < 8 || Cross(p[1] - p[0], p[2] - p[0]) * Cross(p[2] - p[1], d - p[1]) > 0)
            continue;                   // the second triangle must lie across the edge
        fourth = d;
        return true;
    }
    return false;
}

static int IndexOf(const std::vector<std::string>& names, const std::string& name, const char* what)
{
    for (size_t k = 0; k < names.size(); k++)
        if (names[k] == name) return (int)k;
    Fail(std::string("no ") + what + " \"" + name + "\"");
}

static double ToNumber(const std::string& text, const char* what)
{
    char* end;
    const double v = strtod(text.c_str(), &end);
    if (end == text.c_str() || *end)
        Fail(std::string(what) + ": \"" + text + "\" is not a number");
    return v;
}

static long long ToWhole(const std::string& text, const char* what)
{
    char* end;
    const long long v = strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end)
        Fail(std::string(what) + ": \"" + text + "\" is not a whole number");
    return v;
}

// cases.h and cases.json from the description of a set (--regen: an archived set, to run it again).
//
// The sets in fit/covmeas_data come from many versions of the generator: the oldest have only "seed", "mix" and
// "pages". What a set does not say is what the generator did when the set was made: a triangle of its own in
// the opaque list, HALF_OFFSET 7, no culling, depth 1 at every corner, white, and the zprobe polygon at 0.5.
//
// Every float goes into cases.h as the bit pattern of the single precision number, so that the test program
// never converts or calculates one: a PC and the console then send the same bits. A polygon has four corners
// there; a triangle's fourth is its first again.
static void Write(const std::string& folder, const Json& data)
{
    auto text = [&](const char* key, const char* otherwise) { return data.has(key) ? data.at(key).str() : std::string(otherwise); };
    auto number = [&](const char* key, double otherwise) { return data.has(key) ? data.at(key).num() : otherwise; };
    const Json& pages = data.at("pages");
    const long long halfofs = data.has("halfofs") ? data.at("halfofs").integer() : 7;
    const double z = number("z", 1.0);
    std::string zs = text("zs", ""), depths;
    if (zs.empty())                     // no depth per corner: the depth of the polygons, three times
        depths = Hex(z) + ", " + Hex(z) + ", " + Hex(z);
    else
        for (size_t from = 0, to; from <= zs.size(); from = to + 1) {
            to = std::min(zs.find(',', from), zs.size());
            depths += (from ? ", " : "") + Hex(ToNumber(zs.substr(from, to - from), "zs"));
        }
    size_t count = 0;
    for (const Json& page : pages.a) count += page.size();
    std::vector<std::string> lines = {
        "/* Generated by covmeas/gen (gen.cpp) - do not edit. Floats are bit patterns. */", "#include <stdint.h>",
        Format("#define NUM_TRIS %zu", count),
        Format("#define COV_PRIM %d", IndexOf(PRIMS, text("prim", "tri"), "prim")),
        Format("#define COV_LIST %s", LIST_NAMES[IndexOf(LISTS, text("list", "op"), "list")]),
        Format("#define COV_HALFOFS %lld", halfofs),
        "#define COV_PROBE_Z " + Hex(number("probez", 0.5)),
        Format("#define COV_CULL %s", CULL_NAMES[IndexOf(CULLS, text("cull", "none"), "cull")]),
        "#define COV_Z " + Hex(z),
        "#define COV_ZS { " + depths + " }",
        Format("#define COV_VCOL %d", IndexOf(VCOLS, text("vcol", ""), "vcol")),
        "typedef struct { unsigned char page; uint32_t v[4][2]; } tri_t;", "static const tri_t tris[NUM_TRIS] = {" };
    for (size_t p = 0; p < pages.size(); p++)
        for (const Json& t : pages[p].a) {
            const Json& x = t.at("x");
            const Json& y = t.at("y");
            std::string v;
            for (size_t k = 0; k < std::max(x.size(), (size_t)4); k++) {
                const size_t c = k < x.size() ? k : 0;
                v += std::string(k ? ", " : "") + "{ " + Hex(x[c].num()) + ", " + Hex(y[c].num()) + " }";
            }
            lines.push_back(Format("    { %zu, { %s } },", p, v.c_str()));
        }
    lines.push_back("};");
    WriteLines(folder + "/cases.h", lines);
    WriteFile(folder + "/cases.json", data.dump());
}

int main(int argc, char** argv)
{
    static const char* const usage =
        "gen [--seed N] [--mix lattice|tile|axis|diag|skew|lean|leantile|big|sliver|degen|tiny] [--prim tri|strip|sprite|modvol|zprobe]\n"
        "    [--list op|tr|pt] [--halfofs N] [--cull none|small|ccw|cw] [--z Z] [--zs Z0,Z1,Z2] [--vcol flat|gouraud] [--probez Z]\n"
        "gen --regen <set>.cases.json <folder>";
    long long seed = 1;
    int halfofs = 7;
    double probez = 0.5, z = 1.0;
    std::string mix = "lattice", prim = "tri", list = "op", cull = "none", zs, vcol;
    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a], value;
        if (arg == "--regen" && a + 2 < argc) {
            Write(argv[a + 2], Json::Parse(ReadFile(argv[a + 1])));
            return 0;
        }
        const size_t eq = arg.find('=');
        if (eq != std::string::npos) {              // --name=value
            value = arg.substr(eq + 1);
            arg = arg.substr(0, eq);
        } else if (a + 1 < argc)
            value = argv[++a];
        else
            Fail(usage);
        if (arg == "--seed") seed = ToWhole(value, "--seed");
        else if (arg == "--mix") mix = MIXES[IndexOf(MIXES, value, "mix")];
        else if (arg == "--prim") prim = PRIMS[IndexOf(PRIMS, value, "prim")];
        else if (arg == "--list") list = LISTS[IndexOf(LISTS, value, "list")];
        else if (arg == "--halfofs") halfofs = (int)ToWhole(value, "--halfofs");
        else if (arg == "--probez") probez = ToNumber(value, "--probez");
        else if (arg == "--cull") cull = CULLS[IndexOf(CULLS, value, "cull")];
        else if (arg == "--z") z = ToNumber(value, "--z");
        else if (arg == "--zs") zs = value;
        else if (arg == "--vcol") vcol = VCOLS[IndexOf(VCOLS, value, "vcol")];
        else Fail(usage);
    }
    const bool three = prim == "tri" || prim == "modvol" || prim == "zprobe";
    if (mix == "big" && !three)
        Fail("--mix big has triangles only (--prim tri, modvol or zprobe): its far corners leave no room for a fourth");
    Rng rng((uint64_t)seed);
    CENTER = halfofs & 1 ? 0.5 : 0.0;

    // where the triangles go: a cell each; a region of 3x3 cells each; the border cells
    const int span = mix == "tile" || mix == "leantile" ? 3 : 1;
    std::vector<Place> places;
    if (span == 3) {
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 6; c++) places.push_back({ 3 * r * 20 + 3 * c, 0, 0 });
    } else if (mix == "big")
        places = BorderCells();
    else
        for (int cell = 0; cell < 300; cell++) places.push_back({ cell, 0, 0 });

    static const std::vector<double> anyKind = { 0.4, 0.3, 0.3 }, exact = { 1.0, 0.0, 0.0 };
    static const int skews[4] = { 1, 2, 4, 12 };
    auto plain = [&](const Place& place) -> Case {
        const int cell = place.cell;
        if (mix == "lattice") return Triangle(rng, cell, 1, DIRS, anyKind);
        if (mix == "axis") return Triangle(rng, cell, 1, AXES, exact);
        if (mix == "skew") {
            const int skew = skews[rng.integers(0, 4)];
            return Triangle(rng, cell, 1, AXES, exact, skew);
        }
        if (mix == "diag") return Triangle(rng, cell, 1, DIAGONALS, exact);
        if (mix == "lean") return LeanTriangle(rng, cell);
        if (mix == "big") return BigTriangle(rng, place);
        if (mix == "sliver") return SliverTriangle(rng, cell);
        if (mix == "degen") return DegenTriangle(rng, cell, prim == "strip");
        if (mix == "tiny") return TinyTriangle(rng, cell);
        if (mix == "tile") return Triangle(rng, cell, 3, DIRS, anyKind);
        return LeanTriangle(rng, cell, 3);              // leantile
    };
    // A strip or a sprite needs a fourth corner that fits: if none does, another triangle. The degenerate mix
    // has its own strips, and its sprites stay at three corners (the table repeats the first as the fourth).
    auto make = [&](const Place& place) -> Case {
        for (int tries = 0; tries < 200; tries++) {
            Case t = plain(place);
            V2 d = { 0, 0 };
            if (three || mix == "degen")
                return t;
            if (Fourth(rng, t, place.cell, span, prim == "strip", d)) {
                t.x.push_back(d.x); t.y.push_back(d.y);
                return t;
            }
        }
        Fail("no fourth corner");
    };

    Json pages = Json::Array();
    size_t count = 0;
    long made[3] = { 0, 0, 0 }, met[3] = { 0, 0, 0 };
    for (int p = 0; p < PAGES; p++) {
        Json page = Json::Array();
        for (const Place& place : places) {
            const Case t = make(place);
            if (t.wanted >= 0) {
                made[t.wanted]++;
                met[t.wanted] += t.met;
            }
            page.push(CaseJson(t));
            count++;
        }
        pages.push(std::move(page));
    }
    Json data = Json::Object().set("seed", seed).set("mix", mix).set("prim", prim).set("list", list).set("halfofs", halfofs)
        .set("probez", probez).set("cull", cull).set("z", z).set("zs", zs).set("vcol", vcol).set("pages", std::move(pages));
    Write(Here(), data);
    if (made[Row] + made[Later] + made[Column])         // the lean mixes: did the model find the heights asked for?
        fprintf(stderr, "lean edges where the search wanted them: %ld of %ld (zero at the tile's first column %ld of %ld, at a later one %ld of %ld, "
                        "near zero on a column %ld of %ld); the others come near\n", met[Row] + met[Later] + met[Column],
                made[Row] + made[Later] + made[Column], met[Row], made[Row], met[Later], made[Later], met[Column], made[Column]);
    printf("seed %lld mix %s prim %s list %s halfofs %d: %d pages, %zu triangles\n", seed, mix.c_str(), prim.c_str(), list.c_str(), halfofs, PAGES, count);
    return 0;
}
