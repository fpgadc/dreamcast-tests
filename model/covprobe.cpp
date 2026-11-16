// covprobe: the ISP's inside test (nulldc3 refsw2: pvr_isp_edge.h, pvr_softfp.h) against archived console runs.
//
//   covprobe [name=value ...] [--scan name=v1,v2,...] [--misses N] <set>.cov ...
//
// name: a field of pvrisp::EdgeConfig (productBits, sumBits, reach, startBits, slopeBits, pairBits, blockAdd,
// startAdd, zeroShift, tieShift, overflowExp, saturateExp, lastColumnByStep) or rows (0: no row range).
// Every polygon of a run is rasterized into its page with the model, and the page is compared with what the
// console drew. --scan repeats that for each value of one parameter. Sets: python export_cov.py.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "pvr_isp_edge.h"
#include "pvr_isp_depth.h"

using pvrfp::Fp;

struct Poly { int page, n; float x[4], y[4]; };
struct Set {
    std::string name;
    int prim = 0, halfofs = 7, pages = 0;
    float zs[3] = { 1, 1, 1 }, probez = 0.5f;
    std::vector<Poly> polys;
    std::vector<uint8_t> cov;       // pages * 480 * 640
};

static bool load(const char* path, Set& s)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
    char magic[8];
    int32_t h[4];
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "PVRCOV", 6) || fread(h, 4, 4, f) != 4) { fclose(f); return false; }
    if (magic[6] == '2' && (fread(s.zs, 4, 3, f) != 3 || fread(&s.probez, 4, 1, f) != 1)) { fclose(f); return false; }
    s.prim = h[0]; s.halfofs = h[1]; s.pages = h[2];
    s.polys.resize(h[3]);
    for (auto& p : s.polys) {
        int32_t pn[2];
        if (fread(pn, 4, 2, f) != 2 || fread(p.x, 4, 4, f) != 4 || fread(p.y, 4, 4, f) != 4) { fclose(f); return false; }
        p.page = pn[0]; p.n = pn[1];
    }
    s.cov.resize((size_t)s.pages * 480 * 640);
    bool ok = fread(s.cov.data(), 1, s.cov.size(), f) == s.cov.size();
    fclose(f);
    s.name = path;
    size_t a = s.name.find_last_of("/\\");
    if (a != std::string::npos) s.name = s.name.substr(a + 1);
    if (s.name.size() > 4) s.name.resize(s.name.size() - 4);
    return ok;
}

// corners k[0..n-1] of p as one polygon (3 or 4 edges) into page. zs: for the depth probe, the corners' depths: a
// pixel then counts when the polygon's depth there keeps the probe polygon out (compare GREATER at probez).
static void raster(const Poly& p, const int* k, int n, bool rows, int half, const pvrisp::EdgeConfig& cfg, uint8_t* page,
                   const float* zs = nullptr, float probez = 0)
{
    Fp x[4], y[4];
    float fy[4];
    float xmin = 1e30f, xmax = -1e30f, ymin = 1e30f, ymax = -1e30f;
    for (int i = 0; i < n; i++) {
        x[i] = Fp::FromFloat(p.x[k[i]]); y[i] = Fp::FromFloat(p.y[k[i]]);
        fy[i] = p.y[k[i]];
        if (p.x[k[i]] < xmin) xmin = p.x[k[i]];
        if (p.x[k[i]] > xmax) xmax = p.x[k[i]];
        if (p.y[k[i]] < ymin) ymin = p.y[k[i]];
        if (p.y[k[i]] > ymax) ymax = p.y[k[i]];
    }
    bool bad;
    int sgn = pvrisp::Winding(x, y, cfg, &bad);
    if (bad)
        return;
    double first = -1e300, last = 1e300;
    if (rows)
        pvrisp::RowRange(fy, n, &first, &last);
    // the tiles the corners' bounding box touches
    if (xmax < 0 || ymax < 0 || xmin >= 640 || ymin >= 480)
        return;
    int tx0 = xmin < 0 ? 0 : (int)(xmin / 32), tx1 = xmax >= 640 ? 19 : (int)(xmax / 32);
    int ty0 = ymin < 0 ? 0 : (int)(ymin / 32), ty1 = ymax >= 480 ? 14 : (int)(ymax / 32);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            pvrisp::EdgeTile e[4];
            for (int i = 0; i < n; i++)
                e[i].Setup(x[i], y[i], x[(i + 1) % n], y[(i + 1) % n], 64 * tx + half, 64 * ty + half, sgn, cfg);
            pvrisp::DepthTile depth;
            if (zs) {
                static const pvrisp::DepthConfig dcfg;
                const float px[3] = { p.x[k[0]], p.x[k[1]], p.x[k[2]] }, py[3] = { p.y[k[0]], p.y[k[1]], p.y[k[2]] };
                depth.Setup(px, py, zs, tx * 32, ty * 32, dcfg);
            }
            for (int j = 0; j < 32; j++) {
                int row = ty * 32 + j;
                if (row < first || row > last)
                    continue;
                bool out = false;
                for (int i = 0; i < n; i++)
                    out |= e[i].RowOutside(j);
                if (out)
                    continue;
                for (int i = 0; i < 32; i++) {
                    bool in = true;
                    for (int q = 0; q < n && in; q++)
                        in = e[q].Inside(i, j);
                    if (in && zs) {
                        float d = depth.At(i, j, half != 0);
                        // the probe polygon: a strip over the whole screen (TL, TR, BL, BR), flat at probez; what it
                        // stores at this pixel comes out of the same depth unit
                        static const pvrisp::DepthConfig dcfg;
                        const float X = tx * 32 + i + 0.5f, Y = row + 0.5f;
                        const bool upper = 480.0f * X + 640.0f * Y < 307200.0f;
                        const float qx[3] = { upper ? 0.0f : 640.0f, upper ? 640.0f : 0.0f, upper ? 0.0f : 640.0f };
                        const float qy[3] = { 0.0f, upper ? 0.0f : 480.0f, 480.0f };
                        const float qz[3] = { probez, probez, probez };
                        pvrisp::DepthTile probe;
                        probe.Setup(qx, qy, qz, tx * 32, ty * 32, dcfg);
                        float pd = probe.At(i, j, half != 0);
                        in = !(d < 0) && !(pd > d);             // not drawn when negative; the probe wins when it is greater
                    }
                    if (in)
                        page[row * 640 + tx * 32 + i] = 1;
                }
            }
        }
}

struct Score { long pixels = 0, drawn = 0, consoleOnly = 0, modelOnly = 0; };

static Score run(const Set& s, const pvrisp::EdgeConfig& cfg, bool rowsOn, int listMisses)
{
    std::vector<uint8_t> img(s.cov.size(), 0);
    int half = (s.halfofs & 1) ? 1 : 0;
    static const int tri0[3] = { 0, 1, 2 }, tri1[3] = { 1, 2, 3 }, quad[4] = { 0, 1, 2, 3 };
    for (const auto& p : s.polys) {
        uint8_t* page = img.data() + (size_t)p.page * 480 * 640;
        bool rows = rowsOn && s.prim != 3;
        if (s.prim == 2)
            raster(p, quad, 4, rows, half, cfg, page);
        else {
            raster(p, tri0, 3, rows, half, cfg, page, s.prim == 4 ? s.zs : nullptr, s.probez);
            if (s.prim == 1)
                raster(p, tri1, 3, rows, half, cfg, page);
        }
    }
    Score sc;
    sc.pixels = (long)img.size();
    for (size_t i = 0; i < img.size(); i++) {
        sc.drawn += s.cov[i];
        if (img[i] != s.cov[i]) {
            (s.cov[i] ? sc.consoleOnly : sc.modelOnly)++;
            if (listMisses-- > 0)
                printf("      page %zu pixel (%zu, %zu): console %s\n", i / (480 * 640), i % 640, (i / 640) % 480, s.cov[i] ? "in" : "out");
        }
    }
    return sc;
}

int main(int argc, char** argv)
{
    pvrisp::EdgeConfig cfg;
    int rows = 1, lastCol = 1, misses = 0;
    std::map<std::string, int*> fields = {
        { "productBits", &cfg.productBits }, { "sumBits", &cfg.sumBits }, { "reach", &cfg.reach }, { "guard", &cfg.guard }, { "startBits", &cfg.startBits },
        { "slopeBits", &cfg.slopeBits }, { "pairBits", &cfg.pairBits }, { "blockAdd", &cfg.blockAdd }, { "startAdd", &cfg.startAdd },
        { "zeroShift", &cfg.zeroShift }, { "tieShift", &cfg.tieShift }, { "overflowExp", &cfg.overflowExp },
        { "saturateExp", &cfg.saturateExp }, { "lastColumnByStep", &lastCol }, { "rows", &rows } };
    std::string scanName;
    std::vector<int> scanValues;
    std::vector<Set> sets;
    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        if (arg == "--scan" && a + 1 < argc) {
            std::string v = argv[++a];
            size_t eq = v.find('=');
            scanName = v.substr(0, eq);
            for (char* t = strtok(&v[eq + 1], ","); t; t = strtok(nullptr, ","))
                scanValues.push_back(atoi(t));
        } else if (arg == "--misses" && a + 1 < argc)
            misses = atoi(argv[++a]);
        else if (arg.find('=') != std::string::npos) {
            size_t eq = arg.find('=');
            auto it = fields.find(arg.substr(0, eq));
            if (it == fields.end()) { fprintf(stderr, "no parameter %s\n", arg.c_str()); return 1; }
            *it->second = atoi(arg.c_str() + eq + 1);
        } else {
            sets.emplace_back();
            if (!load(argv[a], sets.back())) { fprintf(stderr, "bad set %s\n", argv[a]); return 1; }
        }
    }
    if (!scanName.empty() && !fields.count(scanName)) { fprintf(stderr, "no parameter %s\n", scanName.c_str()); return 1; }
    if (scanValues.empty())
        scanValues.push_back(0);
    for (int v : scanValues) {
        if (!scanName.empty())
            *fields[scanName] = v;
        cfg.lastColumnByStep = lastCol != 0;
        long total = 0;
        std::string line;
        for (const auto& s : sets) {
            Score sc = run(s, cfg, rows != 0, scanName.empty() ? misses : 0);
            total += sc.consoleOnly + sc.modelOnly;
            if (scanName.empty())
                printf("%-12s pixels %9ld  drawn on the console %7ld  model != console %6ld  (console only %ld, model only %ld)\n",
                       s.name.c_str(), sc.pixels, sc.drawn, sc.consoleOnly + sc.modelOnly, sc.consoleOnly, sc.modelOnly);
            else
                line += " " + s.name + ":" + std::to_string(sc.consoleOnly + sc.modelOnly);
        }
        if (!scanName.empty())
            printf("%s = %-4d  model != console %7ld  |%s\n", scanName.c_str(), v, total, line.c_str());
    }
    return 0;
}
