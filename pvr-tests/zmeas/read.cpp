/* Turn the zmeas dumps into measured depths.

       read [folder, default /tmp/pvr_tests/zmeas] [hw | off]      ->  meas.json (hw) or meas.off.json next to the dumps

   hw: a console's dumps; off: the dumps of refsw2-offline.

   For every case and pixel: "lo", the largest stored depth of the probe that did not exceed A's (A's stored
   depth is at least this), and "hi", the smallest that did (A's is below this). When they are neighbours on
   the 19 bit grid, A's stored depth is known exactly: it is lo. The probe's stored depth at each pixel comes
   from the model (zmeas.h: refsw2's depth unit). */
#include "gen_common.h"
#include "zmeas.h"
#include <map>

using namespace gen;

struct Stats { int n = 0, exact = 0, zero = 0, bad = 0, below = 0, above = 0; std::map<long, int> deltas; };

int main(int argc, char** argv)
{
    static const Model model;
    std::string folder = "/tmp/pvr_tests/zmeas", kind = "hw";
    for (int a = 1; a < argc; a++) {
        const std::string arg = argv[a];
        if (arg == "hw" || arg == "off") kind = arg;
        else folder = arg;
    }
    // the run's own tables (run2.sh zmeas@<set>), else the ones in the test's folder
    std::string tables = folder + "/cases.json";
    if (access(tables.c_str(), R_OK) != 0)
        tables = Here() + "/cases.json";
    const Json data = Json::Parse(ReadFile(tables));
    const Json& cases = data.at("cases");
    const size_t P = cases[0].at("ladder").size();
    std::vector<std::vector<uint32_t>> pages(P, std::vector<uint32_t>(480 * 640));
    for (size_t p = 0; p < P; p++) {
        const std::string raw = ReadFile(Format("%s/page%zu.%s.raw", folder.c_str(), p, kind.c_str()));
        if (raw.size() != 480 * 640 * 4)
            Fail("not a 640 x 480 dump: page " + std::to_string(p));
        memcpy(pages[p].data(), raw.data(), raw.size());
    }
    Json out = Json::Array();
    std::vector<std::string> order;
    std::map<std::string, Stats> stats;
    for (const Json& c : cases.a) {
        const int cell = (int)c.at("cell").integer();
        Json res = Json::Array();
        Json probe;
        double probeLo = 0, probeHi = 0;
        bool probeHasLo = false, probeHasHi = false, probeMono = false, first = true;
        for (const Json& pt : c.at("pixels").a) {
            const double px = pt[0].num(), py = pt[1].num();
            double qx[3], qy[3];
            if (!zmeas::QuadTri(cell, px, py, qx, qy))
                continue;
            std::vector<double> b(P);                // the probe's stored depth here, per page
            std::vector<char> green(P);
            std::vector<size_t> idx(P);
            bool mono = true;
            for (size_t p = 0; p < P; p++) {
                b[p] = zmeas::QuadDepth(model, cell, px, py, c.at("ladder_in")[p].num());
                const uint32_t v = pages[p][(size_t)py * 640 + (size_t)px];
                green[p] = ((v >> 8) & 255) > 128;                  // green: B > A
                mono = mono && (green[p] || ((v >> 16) & 255) > 128);
                idx[p] = p;
            }
            std::stable_sort(idx.begin(), idx.end(), [&](size_t i, size_t j) { return b[i] < b[j]; });
            bool hasLo = false, hasHi = false;
            double lo = 0, hi = 0;
            for (size_t k : idx) {
                if (green[k]) {
                    if (!hasHi) { hi = b[k]; hasHi = true; }
                } else {
                    if (hasHi) mono = false;
                    lo = b[k]; hasLo = true;
                }
            }
            Json r = Json::Object().set("px", px).set("py", py).set("lo", hasLo ? Json(lo) : Json()).set("hi", hasHi ? Json(hi) : Json()).set("monotone", mono);
            if (first) {
                probe = r;
                probeLo = lo; probeHi = hi; probeHasLo = hasLo; probeHasHi = hasHi; probeMono = mono;
                first = false;
            }
            res.push(r);
        }
        // how many 19 bit steps hi is above lo
        auto steps = [](double lo, double hi) { int e; frexp(lo, &e); return nearbyint((hi - lo) / ldexp(1.0, e - 19)); };
        const bool exact = probeHasLo && probeHasHi && steps(probeLo, probeHi) == 1 && probeMono;
        long delta = 0;
        if (exact) {
            int e;
            const double predicted = c.at("predicted").num();
            frexp(predicted, &e);
            delta = (long)nearbyint((probeLo - predicted) / ldexp(1.0, e - 19));
        }
        probe.set("exact", exact).set("delta", exact ? Json(delta) : Json());
        out.push(Json::Object().set("probe", probe).set("pixels", res));
        const std::string group = c.at("group").str();
        if (!stats.count(group)) order.push_back(group);
        Stats& s = stats[group];
        s.n++; s.exact += exact; s.bad += !probeMono; s.below += !probeHasLo; s.above += !probeHasHi;
        if (exact) { s.zero += delta == 0; s.deltas[delta]++; }
    }
    WriteFile(folder + (kind == "hw" ? "/meas.json" : "/meas." + kind + ".json"), Json::Object().set("cases", out).dump());
    printf("%s; ladders %s\n", kind == "hw" ? "console" : "the offline renderer", data.has("refined") && data.at("refined").truthy() ? "refined" : "first pass");
    printf("%-46s cases  exact  = model   differences from the model in 19 bit steps (count)\n", "group");
    for (const std::string& name : order) {
        const Stats& s = stats[name];
        std::string hist, extra;
        for (auto& kv : s.deltas)
            if (kv.first != 0) hist += Format("%s%+ld: %d", hist.empty() ? "" : ", ", kv.first, kv.second);
        if (s.bad) extra += Format("   [%d not monotone]", s.bad);
        if (s.below) extra += Format("   [%d below the ladder]", s.below);
        if (s.above) extra += Format("   [%d above the ladder]", s.above);
        printf("%-46s %5d %6d %8d   %s%s\n", name.c_str(), s.n, s.exact, s.zero, hist.c_str(), extra.c_str());
    }
    return 0;
}
