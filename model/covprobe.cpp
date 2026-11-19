// covprobe: the ISP's inside test and depth, and the TSP's colour, as refsw3 has them (nulldc3 refsw3/refcore:
// pvr_isp_edge.h, pvr_isp_depth.h, pvr_tsp_plane.h, pvr_softfp.h), against archived console runs of pvr-tests/covmeas.
//
//   covprobe [name=value ...] [--scan name=v1,v2,...] [--misses N] [--dump folder] <set>.cov ...
//
// name: a field of pvrisp::EdgeConfig (productBits, sumBits, reach, startBits, slopeBits, pairBits, blockAdd,
// startAdd, zeroShift, tieShift, overflowExp, saturateExp, lastColumnByStep), rows (0: no row range) or shade (0: no
// TSP, see below). Every polygon of a run is rasterized into its page with the model, and the page is compared with
// what the console drew. --scan repeats that for each value of one parameter. --dump writes the model's pages of
// the triangle, strip and depth probe sets into a folder: <set>.page<n>.rgb.raw (0RGB), .z.raw (the depth buffer's
// words), .tag.raw (the set's polygon at each pixel; -1 the probe, -2 none), 640 x 480 little endian words like
// run2.sh's page<n>.hw.raw. Sets: python export_cov.py.
//
// The triangle and strip sets and the depth probe sets (prim 0, 1, 4) are rendered page by page as refsw3 renders
// them (renderPage): the ISP's depth compares and writes in list order, then (shade 1, the default) the TSP's colour
// of each pixel, and read as covmeas read.py reads a console's page. The colour decides at slivers: their planes are
// garbage, and so is their colour, black (not drawn, to read.py) or with a green of 128 or more (to the depth probe,
// "the probe drawn"). With shade 0 a polygon's pixels are its colour: the probe of before (the ISP alone), which
// misses those pixels, on zs25_* and zs25f_* 601 (51 a set at the probes up to 0.25), on zs25f_tri 1442 (the
// slivers drawn black), on sliver26, 28, 29 1, 3, 3 (black). On a console the ISP keeps the sliver at those pixels, at
// the model's depth, and the colour is the TSP's: dreamcast-tests/pvr-tests/probes_zs25z read the depth buffer out
// in the 18 tiles that hold zs25f_e4's 51 pixels and in 2 more, the model's on 20,480 of 20,480 pixels (0.265 ..
// 2.06 at the 51, none below 1e-4; depthprobe on the 283 sliver pixels among them: 283 of 283); covmeas@zs25f_e4,
// @zs25f_tri, @zs25_005, @sliver26, @sliver28, @sliver29 run again: each page the model's colour on 2,457,600 of
// 2,457,600 pixels (the 51: 27 cyan 0x00ffff, 24 white; the probe's green 0x00ff00 at none; the 7 sliver pixels
// black), the archived readings again. Sprites and modifier volumes (prim 2, 3): the ISP alone, as before.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <algorithm>
#include "pvr_isp_edge.h"
#include "pvr_isp_depth.h"
#include "pvr_tsp_plane.h"
#include "f32bits.h"

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

// The tiles a polygon goes into (the TA's: floor(x / 32) of its corners' bounding box) and, in each, the ISP's inside
// test as refsw3's RasterizeTriangle does it (Winding, EdgeTile, RowRange). corners: the polygon's corners in rasterizer
// order (3, or 4 for a sprite); zs: their depths, or null; inside(tx, ty, i, j, z) for every pixel inside, z the depth
// the ISP stores there (pvrisp::DepthTile, refsw3's DepthTileT; zero without zs).
template<class F>
static void raster(const float* px, const float* py, int n, const float* zs, bool rows, int half, const pvrisp::EdgeConfig& cfg, F inside)
{
    Fp x[4], y[4];
    float xmin = 1e30f, xmax = -1e30f, ymin = 1e30f, ymax = -1e30f;
    for (int i = 0; i < n; i++) {
        x[i] = Fp::FromFloat(px[i]); y[i] = Fp::FromFloat(py[i]);
        if (px[i] < xmin) xmin = px[i];
        if (px[i] > xmax) xmax = px[i];
        if (py[i] < ymin) ymin = py[i];
        if (py[i] > ymax) ymax = py[i];
    }
    bool bad;
    int sgn = pvrisp::Winding(x, y, cfg, &bad);
    if (bad)
        return;
    int64_t first = INT64_MIN, last = INT64_MAX;
    if (rows)
        pvrisp::RowRange(y, n, &first, &last);
    // the tiles the corners' bounding box touches
    if (xmax < 0 || ymax < 0 || xmin >= 640 || ymin >= 480)
        return;
    int tx0 = xmin < 0 ? 0 : (int)(xmin / 32), tx1 = xmax >= 640 ? 19 : (int)(xmax / 32);
    int ty0 = ymin < 0 ? 0 : (int)(ymin / 32), ty1 = ymax >= 480 ? 14 : (int)(ymax / 32);
    static const pvrisp::DepthConfig dcfg;
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            pvrisp::EdgeTile e[4];
            for (int i = 0; i < n; i++)
                e[i].Setup(x[i], y[i], x[(i + 1) % n], y[(i + 1) % n], 64 * tx + half, 64 * ty + half, sgn, cfg);
            pvrisp::DepthTile depth;
            if (zs)
                depth.Setup(F32s(px), F32s(py), F32s(zs), tx * 32, ty * 32, dcfg);
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
                    if (in)
                        inside(tx, ty, i, j, zs ? depth.At(i, j, half != 0) : pvrisp::StoredDepth());
                }
            }
        }
}

// corners k[0..n-1] of p as one polygon (3 or 4 edges) into page: 1 where it is inside
static void raster(const Poly& p, const int* k, int n, bool rows, int half, const pvrisp::EdgeConfig& cfg, uint8_t* page)
{
    float px[4], py[4];
    for (int i = 0; i < n; i++) {
        px[i] = p.x[k[i]]; py[i] = p.y[k[i]];
    }
    raster(px, py, n, nullptr, rows, half, cfg, [&](int tx, int ty, int i, int j, pvrisp::StoredDepth) {
        page[(ty * 32 + j) * 640 + tx * 32 + i] = 1;
    });
}

// One polygon of a page as the core takes it: corners in rasterizer order, their depths, a flat colour (ARGB), its
// depth compare (ALWAYS or GREATER) and whether it writes its depth.
struct Prim { float x[3], y[3], z[3]; uint32_t argb; bool greater, zwrite; };

static std::string dumpDir;      // --dump: where the model's pages go

// A page as refsw3 renders it, tile by tile, into rgb (640 x 480, 0RGB). The ISP: the depth buffer starts at the
// background's depth, the polygons follow in list order, each at the pixels it is inside of (raster above), its
// depth (pvrisp::DepthTile) compared as refsw3 compares depths, as the ISP's integers (pvrisp::StoredDepth: a depth
// below zero is below every positive one), the last one to pass at a pixel is the pixel's. shade: the TSP then
// shades every pixel with its polygon as refsw3 does for an untextured polygon without fog, blended ONE / ZERO
// (refsw_tile.cpp RenderParamTags, PixelFlush_tsp, InterpolateBase): per span (SpanGen: the pixels of one polygon next
// to each other on a line of the tile; a polygon on top cuts it, as in the opaque list) the 1/w plane and the colour
// planes (pvrtsp::PlaneT; a flat polygon's colour at each corner, in 2^-8 units: refsw_tile.h ColorExp) evaluated from
// the span's first pixel, divided (pvrtsp::Divide), rounded (pvrtsp::ColorRounded, to 65535) and clamped to 255. A
// sliver's planes are garbage, and so is its colour. Without shade a polygon's pixels are its colour. (The .cov sets
// do not say which list they were drawn in; tr18, sliver37 were translucent in the order sent, whose polygons the
// TSP shades one at a time, spans whole, and pt19 punch-through: with the opaque list's spans all three are the
// console's.)
static void renderPage(const std::vector<Prim>& prims, bool rows, int halfofs, bool shade, const pvrisp::EdgeConfig& cfg, uint32_t* rgb,
                       uint32_t* depth = nullptr, int32_t* tags = nullptr)
{
    const int half = halfofs & 1;
    // ISP_BACKGND_D of pvr-tests/common/pvrsetup.h, 0x38d1b717 (1e-4), as refsw3 stores it (refsw_lists.cpp, the
    // tile's clear: from exponent field 113 the 17 fraction bits under 2^-20 go)
    const pvrisp::StoredDepth background = pvrisp::StoredDepth::Bits(0x38d1b717u & ~0x1ffffu);
    std::vector<pvrisp::StoredDepth> z(480 * 640, background);
    std::vector<int> tag(480 * 640, -1);
    for (size_t k = 0; k < prims.size(); k++) {
        const Prim& q = prims[k];
        raster(q.x, q.y, 3, q.z, rows, half, cfg, [&](int tx, int ty, int i, int j, pvrisp::StoredDepth d) {
            const int at = (ty * 32 + j) * 640 + tx * 32 + i;
            if (q.greater && !(d > z[at]))
                return;
            if (q.zwrite)
                z[at] = d;
            tag[at] = (int)k;
        });
    }
    for (int at = 0; at < 480 * 640; at++) {
        rgb[at] = tag[at] < 0 ? 0 : prims[tag[at]].argb & 0xffffff;
        if (depth)
            depth[at] = z[at].bits;
        if (tags)
            tags[at] = tag[at];
    }
    if (!shade)
        return;
    static const pvrtsp::PlaneHW pcfg;
    const int th = (halfofs >> 2) & 1;              // HALF_OFFSET bit 2: the TSP's sample points
    for (int ty = 0; ty < 15; ty++)
        for (int tx = 0; tx < 20; tx++) {
            const int left = tx * 32, top = ty * 32;
            std::map<int, std::vector<pvrtsp::PlaneT<pvrtsp::PlaneHW>>> planes;       // 1/w, then R, G, B
            for (int j = 0; j < 32; j++)
                for (int i = 0; i < 32; ) {
                    const int at0 = (top + j) * 640 + left + i, k = tag[at0];
                    int len = 1;
                    while (i + len < 32 && tag[at0 + len] == k)
                        len++;
                    if (k >= 0) {
                        const Prim& q = prims[k];
                        auto& pl = planes[k];
                        if (pl.empty()) {
                            pl.resize(4);
                            pl[0].Setup(F32s(q.x), F32s(q.y), F32s(q.z), nullptr, pcfg);
                            for (int c = 0; c < 3; c++) {
                                const uint32_t a = pvrfp::SingleBits((q.argb >> (16 - 8 * c)) & 255, -8), at3[3] = { a, a, a };
                                pl[1 + c].Setup(F32s(q.x), F32s(q.y), F32s(q.z), at3, pcfg);
                            }
                        }
                        const int64_t x0h = 2 * (int64_t)(left + i) + th, yh = 2 * (int64_t)(top + j) + th;
                        for (int n = 0; n < len; n++) {
                            const Fp w = pl[0].Value(x0h, yh, n, left, top);
                            uint32_t px = 0;
                            for (int c = 0; c < 3; c++) {
                                const int fb = (q.argb >> (16 - 8 * c)) & 255;
                                int v = pvrtsp::ColorRounded(pvrtsp::Divide(pl[1 + c].Value(x0h, yh, n, left, top), w), -8, 65535, fb);
                                px |= (uint32_t)(v > 255 ? 255 : v) << (16 - 8 * c);
                            }
                            rgb[at0 + n] = px;
                        }
                    }
                    i += len;
                }
        }
}

// The page of a depth probe set (prim 4: pvr-tests/covmeas COV_PRIM 4) or of a triangle set: the page's triangles in
// list order (prim 4: red, depth written; else white, depth not written; compare ALWAYS), for prim 4 then the probe, a
// strip over the whole screen (TL, TR, BL, BR: the triangles TL TR BL and BL TR BR, as refsw3 takes a strip's second
// triangle), green, flat at probez, compare GREATER, depth not written. Read as covmeas read.py reads the console's
// page: prim 4 1 where green is below 128 (isp: the probe is not drawn there), else 1 where the pixel is not black.
static void trianglePage(const Set& s, int pageNo, bool rows, bool shade, const pvrisp::EdgeConfig& cfg, uint8_t* page)
{
    std::vector<Prim> prims;
    std::vector<int32_t> from;          // the set's polygon of each (-1: the probe)
    static const int tris[2][3] = { { 0, 1, 2 }, { 2, 1, 3 } };     // a strip's second triangle as refsw3 takes it
    for (const auto& p : s.polys) {
        if (p.page != pageNo)
            continue;
        for (int t = 0; t < (s.prim == 1 ? 2 : 1); t++) {
            Prim q;
            for (int i = 0; i < 3; i++) {
                q.x[i] = p.x[tris[t][i]]; q.y[i] = p.y[tris[t][i]];
                q.z[i] = s.prim == 1 ? s.zs[0] : s.zs[i];       // a strip: COV_Z at every corner
            }
            q.argb = s.prim == 4 ? 0xffff0000u : 0xffffffffu;
            q.greater = false;
            q.zwrite = s.prim == 4;
            prims.push_back(q);
            from.push_back((int32_t)(&p - s.polys.data()));
        }
    }
    if (s.prim == 4) {
        const float qx[2][3] = { { 0, 640, 0 }, { 0, 640, 640 } }, qy[2][3] = { { 0, 0, 480 }, { 480, 0, 480 } };
        for (int t = 0; t < 2; t++) {
            Prim q;
            for (int i = 0; i < 3; i++) {
                q.x[i] = qx[t][i]; q.y[i] = qy[t][i]; q.z[i] = s.probez;
            }
            q.argb = 0xff00ff00u;
            q.greater = true;
            q.zwrite = false;
            prims.push_back(q);
            from.push_back(-1);
        }
    }
    std::vector<uint32_t> rgb(480 * 640), depth(480 * 640);
    std::vector<int32_t> tags(480 * 640);
    renderPage(prims, rows, s.halfofs, shade, cfg, rgb.data(), depth.data(), tags.data());
    if (!dumpDir.empty()) {
        // the model's page (0RGB), its depth buffer (the ISP's words) and the polygon of each pixel (its number in
        // the set, -1 the probe, -2 none), 640 x 480 32 bit words, little endian
        for (int at = 0; at < 480 * 640; at++)
            tags[at] = tags[at] < 0 ? -2 : from[tags[at]];
        for (int k = 0; k < 3; k++) {
            static const char* const what[3] = { ".rgb.raw", ".z.raw", ".tag.raw" };
            const std::string f = dumpDir + "/" + s.name + ".page" + std::to_string(pageNo) + what[k];
            FILE* o = fopen(f.c_str(), "wb");
            const void* data = k == 0 ? (const void*)rgb.data() : k == 1 ? (const void*)depth.data() : (const void*)tags.data();
            if (!o || fwrite(data, 4, 480 * 640, o) != 480 * 640)
                fprintf(stderr, "cannot write %s\n", f.c_str());
            if (o)
                fclose(o);
        }
    }
    for (int at = 0; at < 480 * 640; at++)
        page[at] = s.prim == 4 ? ((rgb[at] >> 8) & 255) < 128 : (rgb[at] & 0xffffff) != 0;
}

struct Score { long pixels = 0, drawn = 0, consoleOnly = 0, modelOnly = 0; };

static Score run(const Set& s, const pvrisp::EdgeConfig& cfg, bool rowsOn, bool shadeOn, int listMisses, std::string* missText)
{
    std::vector<uint8_t> img(s.cov.size(), 0);
    int half = (s.halfofs & 1) ? 1 : 0;
    static const int tri0[3] = { 0, 1, 2 }, tri1[3] = { 1, 2, 3 }, quad[4] = { 0, 1, 2, 3 };
    if (s.prim == 4 || ((s.prim == 0 || s.prim == 1) && shadeOn))
        for (int pg = 0; pg < s.pages; pg++)
            trianglePage(s, pg, rowsOn, shadeOn, cfg, img.data() + (size_t)pg * 480 * 640);
    else
        for (const auto& p : s.polys) {
            uint8_t* page = img.data() + (size_t)p.page * 480 * 640;
            bool rows = rowsOn && s.prim != 3;
            if (s.prim == 2)
                raster(p, quad, 4, rows, half, cfg, page);
            else {
                raster(p, tri0, 3, rows, half, cfg, page);
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
            if (listMisses-- > 0) {
                char line[96];
                snprintf(line, sizeof(line), "      page %zu pixel (%zu, %zu): console %s\n", i / (480 * 640), i % 640, (i / 640) % 480, s.cov[i] ? "in" : "out");
                *missText += line;
            }
        }
    }
    return sc;
}

int main(int argc, char** argv)
{
    pvrisp::EdgeConfig cfg;
    int rows = 1, lastCol = 1, shade = 1, misses = 0;
    std::map<std::string, int*> fields = {
        { "productBits", &cfg.productBits }, { "sumBits", &cfg.sumBits }, { "reach", &cfg.reach }, { "guard", &cfg.guard }, { "startBits", &cfg.startBits },
        { "slopeBits", &cfg.slopeBits }, { "pairBits", &cfg.pairBits }, { "blockAdd", &cfg.blockAdd }, { "startAdd", &cfg.startAdd },
        { "zeroShift", &cfg.zeroShift }, { "tieShift", &cfg.tieShift }, { "overflowExp", &cfg.overflowExp },
        { "saturateExp", &cfg.saturateExp }, { "lastColumnByStep", &lastCol }, { "rows", &rows }, { "shade", &shade } };
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
        else if (arg == "--dump" && a + 1 < argc)
            dumpDir = argv[++a];
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
        // the sets side by side, a thread each at a time; printed in the order given
        std::vector<Score> scores(sets.size());
        std::vector<std::string> missText(sets.size());
        std::atomic<size_t> next{ 0 };
        auto work = [&] {
            for (size_t k; (k = next++) < sets.size(); )
                scores[k] = run(sets[k], cfg, rows != 0, shade != 0, scanName.empty() ? misses : 0, &missText[k]);
        };
        std::vector<std::thread> threads;
        const size_t nt = std::min<size_t>(sets.size(), std::max(1u, std::thread::hardware_concurrency()));
        for (size_t t = 1; t < nt; t++)
            threads.emplace_back(work);
        work();
        for (auto& t : threads)
            t.join();
        for (size_t k = 0; k < sets.size(); k++) {
            const Set& s = sets[k];
            const Score& sc = scores[k];
            total += sc.consoleOnly + sc.modelOnly;
            if (scanName.empty())
                printf("%s%-12s pixels %9ld  drawn on the console %7ld  model != console %6ld  (console only %ld, model only %ld)\n",
                       missText[k].c_str(), s.name.c_str(), sc.pixels, sc.drawn, sc.consoleOnly + sc.modelOnly, sc.consoleOnly, sc.modelOnly);
            else
                line += " " + s.name + ":" + std::to_string(sc.consoleOnly + sc.modelOnly);
        }
        if (!scanName.empty())
            printf("%s = %-4d  model != console %7ld  |%s\n", scanName.c_str(), v, total, line.c_str());
    }
    return 0;
}
