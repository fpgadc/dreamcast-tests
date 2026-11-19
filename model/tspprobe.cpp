// tspprobe: the TSP's planes and division (nulldc3 refsw2: pvr_tsp_plane.h, pvr_isp_edge.h, pvr_softfp.h)
// against archived console runs of uvmeas / uvspan-like sets.
//
//   tspprobe [name=value ...] [--scan name=v1,v2,...] [--misses N] [--cells] <set>.uvm ...
//
// name: a field of pvrtsp::PlaneConfig (see fields[] below). Every reading of a set (a pixel of a triangle, and
// the number of the texel the console showed there) is computed with the model: the triangle's 1/w and U/w
// planes, evaluated at the run of the triangle's pixels on that line of the tile, divided, and turned into a
// texel of the 1024 wide repeating texture. --scan repeats the scoring for each value of one parameter;
// --cells prints one character per triangle (. all readings right, x not); --summary prints a line per
// triangle: its number, readings, how many are right, and the console's and the model's texel at its first,
// middle and last reading. Sets: python export_uv.py.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "pvr_isp_edge.h"
#include "pvr_tsp_plane.h"
#include "f32bits.h"

using pvrfp::Fp;

struct Tri { float x[3], y[3], u[3], z[3]; };
struct Set {
    std::string name;
    std::vector<Tri> tris;
    std::vector<int32_t> ti;
    std::vector<int16_t> px, py, t;
};

static bool load(const char* path, Set& s)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
    char magic[8];
    int32_t h[2];
    bool ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "PVRUVM1", 7) && fread(h, 4, 2, f) == 2;
    if (ok) {
        s.tris.resize(h[0]); s.ti.resize(h[1]); s.px.resize(h[1]); s.py.resize(h[1]); s.t.resize(h[1]);
        ok = fread(s.tris.data(), sizeof(Tri), h[0], f) == (size_t)h[0] && fread(s.ti.data(), 4, h[1], f) == (size_t)h[1] &&
             fread(s.px.data(), 2, h[1], f) == (size_t)h[1] && fread(s.py.data(), 2, h[1], f) == (size_t)h[1] &&
             fread(s.t.data(), 2, h[1], f) == (size_t)h[1];
    }
    fclose(f);
    s.name = path;
    size_t a = s.name.find_last_of("/\\");
    if (a != std::string::npos) s.name = s.name.substr(a + 1);
    if (s.name.size() > 4) s.name.resize(s.name.size() - 4);
    return ok;
}

// the texel of a 1024 wide repeating texture that a coordinate selects (refsw_tile.cpp: TexCoordFixed, point sampling)
static int texel(double u)
{
    double uf = (double)(float)u;
    if (uf == 0 && u < 0)
        uf = -1e-30;                 // a quotient below zero stays below zero however small: the texel before 0
    double f = floor(uf * 1024.0 * 256.0);
    if (f >= 1073741824.0)
        f = 1073741824.0 + fmod(f, 67108864.0);
    else if (f < -1073741824.0)
        f = -1073741824.0 + fmod(fmod(f, 67108864.0) + 67108864.0, 67108864.0);
    if (!(f == f))
        f = 0;                       // a quotient that is not a number, or too large to wrap: texel 0
    return (((int)f) >> 8) & 1023;
}

struct Fields { const char* name; int pvrtsp::PlaneConfig::* p; };
static const Fields fields[] = {
    { "productBits", &pvrtsp::PlaneConfig::productBits }, { "sumBits", &pvrtsp::PlaneConfig::sumBits },
    { "reach", &pvrtsp::PlaneConfig::reach }, { "recipBits", &pvrtsp::PlaneConfig::recipBits },
    { "coefBits", &pvrtsp::PlaneConfig::coefBits }, { "gridBits", &pvrtsp::PlaneConfig::gridBits },
    { "valueBits", &pvrtsp::PlaneConfig::valueBits },
    { "range", &pvrtsp::PlaneConfig::range }, { "expLow", &pvrtsp::PlaneConfig::expLow }, { "expHigh", &pvrtsp::PlaneConfig::expHigh },
    { "zeroReach", &pvrtsp::PlaneConfig::zeroReach }, { "tileLocal", &pvrtsp::PlaneConfig::tileLocal }, { "macOrder", &pvrtsp::PlaneConfig::macOrder },
    { "valueFixed", &pvrtsp::PlaneConfig::valueFixed }, { "sumA", &pvrtsp::PlaneConfig::sumA }, { "sumW", &pvrtsp::PlaneConfig::sumW },
#ifdef PVRTSP_RANGE_FIELDS
    PVRTSP_RANGE_FIELDS
#endif
};

static int wFixed = -1;            // wfixed=n: valueFixed for the 1/w plane only
static bool set_field(pvrtsp::PlaneConfig& cfg, const std::string& name, int value)
{
    for (auto& f : fields)
        if (name == f.name) { cfg.*(f.p) = value; return true; }
    return false;
}

// the model's reading for every reading of the set; returns how many are the console's
static bool summary = false;
static int splitRuns = 0;
static std::vector<int> breakCols;     // breaks=a,b,..: runs also start at these columns of the tile
static long skipped = 0;               // readings that are not a texel's colour (-1): not scored      // split=N: runs also start at every N-th column of the tile (to test that they do not)
static long zeroDen = 0, zeroDenRight = 0, zeroNum = 0, zeroNumRight = 0;     // readings where the model's 1/w, U/w is exactly 0
static int onlyFrom = 0, onlyTo = 1 << 30;      // --only a-b: score these triangles only
static long score(const Set& s, const pvrtsp::PlaneConfig& cfg, int misses, bool cells)
{
    static const pvrisp::EdgeConfig ecfg;
    long right = 0;
    size_t r = 0;
    std::string map;
    while (r < s.ti.size()) {
        int t = s.ti[r];
        const Tri& tr = s.tris[t];
        pvrtsp::Plane w, u;
        pvrtsp::PlaneConfig wc = cfg;
        if (wFixed >= 0) wc.valueFixed = wFixed;
        w.Setup(F32s(tr.x), F32s(tr.y), F32s(tr.z), nullptr, wc);
        u.Setup(F32s(tr.x), F32s(tr.y), F32s(tr.z), F32s(tr.u), cfg);
        Fp fx[3], fy[3];
        for (int k = 0; k < 3; k++) { fx[k] = Fp::FromFloat(tr.x[k]); fy[k] = Fp::FromFloat(tr.y[k]); }
        bool bad;
        int sgn = pvrisp::Winding(fx, fy, ecfg, &bad);
        int tile = -1, start[32];
        pvrisp::EdgeTile e[3];
        long before = right;
        size_t first = r;
        std::vector<int> gots;
        for (; r < s.ti.size() && s.ti[r] == t; r++) {
            if (t < onlyFrom || t > onlyTo) { right++; continue; }
            if (s.t[r] < 0) { skipped++; right++; continue; }
            int tx = s.px[r] / 32, ty = s.py[r] / 32, i = s.px[r] % 32, j = s.py[r] % 32;
            if (ty * 20 + tx != tile) {
                tile = ty * 20 + tx;
                for (int k = 0; k < 3; k++)
                    e[k].Setup(fx[k], fy[k], fx[(k + 1) % 3], fy[(k + 1) % 3], 64 * tx + 1, 64 * ty + 1, sgn, ecfg);
                for (int q = 0; q < 32; q++) start[q] = -1;
            }
            if (start[j] < 0) {                         // the first pixel of the triangle on this line of the tile
                start[j] = i;
                for (int q = 0; q < i; q++)
                    if (e[0].Inside(q, j) && e[1].Inside(q, j) && e[2].Inside(q, j)) { start[j] = q; break; }
            }
            int got = -9;
            Fp dbgW, dbgU;
            if (!w.bad && !u.bad && !bad) {
                int s0 = start[j];
                if (splitRuns > 0 && i - i % splitRuns > s0)
                    s0 = i - i % splitRuns;
                for (int b : breakCols)
                    if (b <= i && b > s0)
                        s0 = b;
                int64_t x0h = 2 * (int64_t)(tx * 32 + s0) + 1, yh = 2 * (int64_t)(ty * 32 + j) + 1;
                Fp wv = w.Value(x0h, yh, i - s0, tx * 32, ty * 32), uv = u.Value(x0h, yh, i - s0, tx * 32, ty * 32);
                dbgW = wv; dbgU = uv;
                if (cfg.range) {
                    got = texel(pvrtsp::Divide(uv, wv).toDouble());
                } else {                                    // before the limits were measured
                    double den = wv.toDouble(), num = uv.toDouble();
                    double q = (!(den > 0) || num == 0) ? num / den : pvrtsp::Divide(uv, wv).toDouble();
                    got = !(den > 0) ? 1023 : texel(q);
                }
            }
            if (dbgW.zero() && !w.bad) { zeroDen++; zeroDenRight += got == s.t[r]; }
            if (dbgU.zero() && !u.bad) { zeroNum++; zeroNumRight += got == s.t[r]; }
            if (summary)
                gots.push_back(got);
            if (got == s.t[r])
                right++;
            else if (misses-- > 0)
                printf("    %s triangle %d pixel (%d, %d): model %d, console %d   1/w %s%llu * 2^%d   U/w %s%llu * 2^%d\n", s.name.c_str(), t, s.px[r], s.py[r], got, s.t[r],
                       dbgW.neg ? "-" : "+", (unsigned long long)dbgW.mant, dbgW.exp, dbgU.neg ? "-" : "+", (unsigned long long)dbgU.mant, dbgU.exp);
        }
        if (cells)
            map += (right - before == (long)(r - first)) ? '.' : 'x';
        if (summary && r > first) {
            size_t n = r - first, mid = n / 2;
            printf("S %d %zu %ld  %d %d %d  %d %d %d\n", t, n, right - before, s.t[first], s.t[first + mid], s.t[r - 1], gots[0], gots[mid], gots[n - 1]);
        }
    }
    if (cells)
        for (size_t k = 0; k < map.size(); k += 100)
            printf("    %4zu %s\n", k, map.substr(k, 100).c_str());
    return right;
}

int main(int argc, char** argv)
{
    pvrtsp::PlaneConfig cfg;
    std::string scan_name;
    std::vector<int> scan_values;
    std::vector<Set> sets;
    int misses = 0;
    bool cells = false;
    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        size_t eq = arg.find('=');
        if (arg == "--scan" && a + 1 < argc) {
            std::string v = argv[++a];
            size_t q = v.find('=');
            scan_name = v.substr(0, q);
            for (size_t at = q + 1; at <= v.size();) {
                size_t c = v.find(',', at);
                if (c == std::string::npos) c = v.size();
                scan_values.push_back(atoi(v.substr(at, c - at).c_str()));
                at = c + 1;
            }
        } else if (arg == "--misses" && a + 1 < argc) {
            misses = atoi(argv[++a]);
        } else if (arg == "--cells") {
            cells = true;
        } else if (arg.rfind("breaks=", 0) == 0) {
            for (size_t at = 7; at < arg.size();) {
                size_t c = arg.find(',', at);
                if (c == std::string::npos) c = arg.size();
                breakCols.push_back(atoi(arg.substr(at, c - at).c_str()));
                at = c + 1;
            }
        } else if (arg.rfind("wfixed=", 0) == 0) {
            wFixed = atoi(arg.c_str() + 7);
        } else if (arg.rfind("split=", 0) == 0) {
            splitRuns = atoi(arg.c_str() + 6);
        } else if (arg == "--summary") {
            summary = true;
        } else if (arg == "--only" && a + 1 < argc) {
            sscanf(argv[++a], "%d-%d", &onlyFrom, &onlyTo);
        } else if (eq != std::string::npos && arg.find('/') == std::string::npos) {
            if (!set_field(cfg, arg.substr(0, eq), atoi(arg.c_str() + eq + 1))) { fprintf(stderr, "no parameter %s\n", arg.c_str()); return 1; }
        } else {
            sets.emplace_back();
            if (!load(argv[a], sets.back())) { fprintf(stderr, "cannot read %s\n", argv[a]); return 1; }
        }
    }
    if (scan_values.empty()) scan_values.push_back(0);
    for (int v : scan_values) {
        pvrtsp::PlaneConfig c = cfg;
        if (!scan_name.empty() && !set_field(c, scan_name, v)) { fprintf(stderr, "no parameter %s\n", scan_name.c_str()); return 1; }
        long right = 0, total = 0;
        for (auto& s : sets) {
            long n = score(s, c, misses, cells);
            right += n; total += (long)s.ti.size();
            if (scan_name.empty())
                printf("%-12s model = console on %ld of %zu readings%s\n", s.name.c_str(), n, s.ti.size(), n == (long)s.ti.size() ? "" : "   <--");
        }
        if (skipped)
            printf("readings that are not a texel's colour (not scored): %ld\n", skipped);
        if (zeroDen || zeroNum)
            printf("readings with 1/w exactly 0: %ld (%ld right); with U/w exactly 0: %ld (%ld right)\n", zeroDen, zeroDenRight, zeroNum, zeroNumRight);
        if (!scan_name.empty())
            printf("%s = %d: %ld of %ld\n", scan_name.c_str(), v, right, total);
        else if (sets.size() > 1)
            printf("all: %ld of %ld\n", right, total);
    }
    return 0;
}
