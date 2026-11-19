// depthprobe: the ISP's depth (nulldc3 refsw3/refcore: pvr_isp_depth.h, pvr_softfp.h) against archived console runs.
//
//   depthprobe [name=value ...] [--scan name=v1,v2,...] [--misses N] <set>.zme ...
//
// name: a field of pvrisp::DepthConfig. A pixel is right when the model's stored depth is inside the bracket the
// console gave (at least lo, below hi). The depth is refsw3's: pvrisp::DepthTile set up for the pixel's tile (the
// overflow regimes included), At() its word as the ISP stores it, compared with the bracket's ends as the ISP
// compares depths (pvrisp::StoredDepth: the words as integers, a depth below zero below every positive one, an
// overflow word an ordinary number), not as floats. Sets: python export_z.py.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include "pvr_isp_depth.h"
#include "f32bits.h"

struct Rec { float x[3], y[3], z[3], px, py; double lo, hi; };

static bool load(const char* path, std::vector<Rec>& out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[8];
    int32_t n;
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, "PVRZME1", 7) || fread(&n, 4, 1, f) != 1) { fclose(f); return false; }
    for (int i = 0; i < n; i++) {
        Rec r;
        if (fread(r.x, 4, 3, f) != 3 || fread(r.y, 4, 3, f) != 3 || fread(r.z, 4, 3, f) != 3 || fread(&r.px, 4, 1, f) != 1 ||
            fread(&r.py, 4, 1, f) != 1 || fread(&r.lo, 8, 1, f) != 1 || fread(&r.hi, 8, 1, f) != 1) { fclose(f); return false; }
        out.push_back(r);
    }
    fclose(f);
    return true;
}

int main(int argc, char** argv)
{
    pvrisp::DepthConfig cfg;
    int misses = 0;
    std::map<std::string, int*> fields = {
        { "productBits", &cfg.productBits }, { "sumBits", &cfg.sumBits }, { "reach", &cfg.reach }, { "recipBits", &cfg.recipBits },
        { "coefBits", &cfg.coefBits }, { "startBits", &cfg.startBits }, { "slopeBits", &cfg.slopeBits }, { "pairBits", &cfg.pairBits },
        { "tileBits", &cfg.tileBits }, { "storedBits", &cfg.storedBits }, { "startOnSlopes", &cfg.startOnSlopes } };
    std::string scanName;
    std::vector<int> scanValues;
    std::vector<Rec> recs;
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
        } else if (!load(argv[a], recs)) { fprintf(stderr, "bad set %s\n", argv[a]); return 1; }
    }
    if (!scanName.empty() && !fields.count(scanName)) { fprintf(stderr, "no parameter %s\n", scanName.c_str()); return 1; }
    if (scanValues.empty())
        scanValues.push_back(0);
    for (int v : scanValues) {
        if (!scanName.empty())
            *fields[scanName] = v;
        long right = 0, both = 0, bothRight = 0;
        int left = misses;
        for (const auto& r : recs) {
            int tx = (int)floor(r.px / 32) * 32, ty = (int)floor(r.py / 32) * 32;
            pvrisp::DepthTile d;
            d.Setup(F32s(r.x), F32s(r.y), F32s(r.z), tx, ty, cfg);
            const pvrisp::StoredDepth m = d.At((int)(r.px - tx), (int)(r.py - ty), true);
            // a bracket's end: a probe depth (a float; NaN: no such end)
            auto end = [](double v) { const float f = (float)v; return pvrisp::StoredDepth::Bits(F32s(&f, 1)[0]); };
            bool ok = (std::isnan(r.lo) || m >= end(r.lo)) && (std::isnan(r.hi) || m < end(r.hi));
            right += ok;
            if (!std::isnan(r.lo) && !std::isnan(r.hi)) { both++; bothRight += ok; }
            if (!ok && left-- > 0)
                printf("   pixel (%.1f, %.1f): console in [%.9g, %.9g), model %.9g (%08x)\n", r.px, r.py, r.lo, r.hi, m.toDouble(), m.bits);
        }
        if (scanName.empty())
            printf("pixels %zu: model inside the console's bracket on %ld (%ld of %ld with both bounds)\n", recs.size(), right, bothRight, both);
        else
            printf("%s = %-3d  outside the bracket: %ld of %zu\n", scanName.c_str(), v, (long)recs.size() - right, recs.size());
    }
    return 0;
}
