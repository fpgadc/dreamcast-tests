// kfit: which arithmetic of K (pvrtsp::MipSelect) gives the console's 8 bit K in every mipk cell, and the model's in
// every triangle of mipmeas's archived sets (the model is the console's on all of them), and r2r_ring's triangle.
// Each mipk cell's observation (fit/mipk_data/<set>.observed.txt: page cell delta) is the console's K8 against the
// model's: 0, -1, +1. A grid of rules (where K's arithmetic is cut, and how); the best first.
//   g++ -O2 -std=c++17 -fopenmp -I <refsw3/refcore> -I model -I pvr-tests/mipk pvr-tests/mipk/kfit.cpp -o pvr-tests/mipk/kfit
//   pvr-tests/mipk/kfit fit/mipk_data/seed1.observed.txt      [KFIT_PATHS=1: numerators from the undivided sums]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include "gen_common.h"
#include <glob.h>
typedef uint32_t f32b;
#define quad_t mipk_quad_t
#include "cases.h"
#undef quad_t

using pvrfp::Fp;

static float F(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

// x cut to `bits` significant bits: mode 0 toward zero, 1 toward minus infinity, 2 to nearest
static Fp CutM(Fp x, int bits, int mode)
{
    if (bits >= 62 || x.zero()) return x;
    int n = Fp::Bits(x.mant);
    if (n <= bits) return x;
    int s = n - bits;
    uint64_t lost = x.mant & ((1ull << s) - 1);
    x.mant >>= s; x.exp += s;
    if (mode == 1 && x.neg && lost) x.mant++;
    if (mode == 2 && (lost >> (s - 1)) & 1) x.mant++;
    return x;
}
// x cut below 2^g (a grid): same modes
static Fp GridM(Fp x, int g, int mode)
{
    if (x.zero() || x.exp >= g) return x;
    int s = g - x.exp;
    uint64_t lost = s >= 64 ? x.mant : x.mant & ((1ull << s) - 1);
    x.mant = s >= 64 ? 0 : x.mant >> s; x.exp = g;
    if (mode == 1 && x.neg && lost) x.mant++;
    if (mode == 2 && s <= 64 && ((s == 64 ? lost >> 63 : lost >> (s - 1)) & 1)) x.mant++;
    if (x.mant == 0) x.neg = false;
    return x;
}

struct Rule {
    int cw, cm;          // coefficients (A0, B0, C0) cut to cw bits, mode
    int pw, pm;          // products
    int sw, sm;          // the numerator (difference)
    int hw, lw;          // the longer / shorter length cut before combining
    int partw;           // the shorter's 27146/65536 part cut
    int gw;              // the sum on a grid gw bits under the longer one (62: none)
    int path = 0;        // 0: divided coefficients; 1: undivided sums, then times inv and times inv; 2: times inv^2
    int iw = 62;         // those multiplications cut to iw bits
    std::string Name() const {
        char b[200];
        snprintf(b, sizeof b, "path %d inv %d coef %d/%d prod %d/%d diff %d/%d hi %d lo %d part %d grid %d", path, iw, cw, cm, pw, pm, sw, sm, hw, lw, partw, gw);
        return b;
    }
};

struct Cell {
    Fp uA, uB, uC, vA, vB, vC, wA, wB, wC;     // A0, B0, C0 of the three planes (or the first vertex's values in C)
    bool dcalc;
    float first[5];
    int bias, sizeLog2, obs, page;
    pvrtsp::Plane pu, pv, pw;
    Fp inv;
};

static Cell MakeCell(const float* x, const float* y, const float* z, const float* u, const float* v, int dcalc, int bias, int sizeLog2, int obs, int page)
{
    pvrtsp::PlaneConfig pc;
    Cell c;
    c.pu.Setup(x, y, z, u, pc); c.pv.Setup(x, y, z, v, pc); c.pw.Setup(x, y, z, nullptr, pc);
    c.uA = c.pu.A0; c.uB = c.pu.B0; c.uC = c.pu.C0; c.vA = c.pv.A0; c.vB = c.pv.B0; c.vC = c.pv.C0;
    c.wA = c.pw.A0; c.wB = c.pw.B0; c.wC = c.pw.C0;
    c.inv = pvrfp::Recip(c.pw.Sdet, 24);
    c.dcalc = dcalc; c.bias = bias; c.sizeLog2 = sizeLog2; c.obs = obs; c.page = page;
    c.first[0] = u[0]; c.first[1] = v[0]; c.first[2] = z[0]; c.first[3] = x[0]; c.first[4] = y[0];
    return c;
}

static int K8Of(const Cell& c, const Rule& r, double* kOut = nullptr)
{
    auto co = [&](const Fp& x) { return CutM(x, r.cw, r.cm); };
    // the values where the numerators are taken: the planes' constants (DCalcCtrl 0) or at the first vertex
    Fp pu = c.uC, pv = c.vC, pw = c.wC;
    if (c.dcalc) {
        const Fp x1 = Fp::FromFloat(c.first[3]), y1 = Fp::FromFloat(c.first[4]);
        auto val1 = [&](const pvrtsp::Plane& pl) { return pvrfp::Add(pvrfp::Add(pvrfp::Mul(pl.A0, x1, 62), pvrfp::Mul(pl.B0, y1, 62), 62, 200), pl.C0, 62, 200); };
        pu = val1(c.pu); pv = val1(c.pv); pw = val1(c.pw);
    }
    auto minor = [&](const Fp& a1, const Fp& b1, const Fp& a2, const Fp& b2) {
        Fp p1 = CutM(pvrfp::Mul(co(a1), co(b2), 62), r.pw, r.pm), p2 = CutM(pvrfp::Mul(co(b1), co(a2), 62), r.pw, r.pm);
        Fp n = CutM(pvrfp::Sub(p1, p2, 62, 200), r.sw, r.sm);
        n.neg = false;
        if (r.path == 1)
            n = CutM(pvrfp::Mul(CutM(pvrfp::Mul(n, c.inv, 62), r.iw, 0), c.inv, 62), r.iw, 0);
        else if (r.path == 2)
            n = CutM(pvrfp::Mul(n, CutM(pvrfp::Mul(c.inv, c.inv, 62), r.iw, 0), 62), r.iw, 0);
        n.neg = false;
        return n;
    };
    auto length = [&](const Fp& p, const Fp& q) {
        const bool pBig = p.toDouble() >= q.toDouble();
        Fp hi = CutM(pBig ? p : q, r.hw, 0), lo = CutM(pBig ? q : p, r.lw, 0);
        if (lo.zero()) return hi;
        Fp part = pvrfp::Mul(lo, Fp::FromHalves(2 * 27146), 62);
        part.exp -= 16;
        part = CutM(part, r.partw, 0);
        if (r.gw < 62) {
            const int g = hi.top() - r.gw;
            return pvrfp::Add(GridM(hi, g, 0), GridM(part, g, 0), 62, 200);
        }
        return pvrfp::Add(hi, part, 62, 200);
    };
    Fp lx, ly;
    if (r.path == 0) {
        lx = length(minor(pu, c.uA, pw, c.wA), minor(pv, c.vA, pw, c.wA));
        ly = length(minor(c.uB, pu, c.wB, pw), minor(c.vB, pv, c.wB, pw));
    } else {
        Fp su = c.pu.Sc, sv = c.pv.Sc, sw = c.pw.Sc;
        if (c.dcalc) {
            const Fp x1 = Fp::FromFloat(c.first[3]), y1 = Fp::FromFloat(c.first[4]);
            auto at1 = [&](const pvrtsp::Plane& pl) { return pvrfp::Add(pvrfp::Add(pvrfp::Mul(pl.Sa, x1, 62), pvrfp::Mul(pl.Sb, y1, 62), 62, 200), pl.Sc, 62, 200); };
            su = at1(c.pu); sv = at1(c.pv); sw = at1(c.pw);
        }
        lx = length(minor(su, c.pu.Sa, sw, c.pw.Sa), minor(sv, c.pv.Sa, sw, c.pw.Sa));
        ly = length(minor(c.pu.Sb, su, c.pw.Sb, sw), minor(c.pv.Sb, sv, c.pw.Sb, sw));
    }
    Fp best = lx.toDouble() >= ly.toDouble() ? lx : ly;
    Fp k = pvrfp::Mul(best, Fp::FromHalves(2 * (int64_t)c.bias), 62);
    k.exp += c.sizeLog2 - 2;
    if (kOut) *kOut = k.toDouble();
    Fp k8 = k.cut(8);
    // the 8 bit value as one integer: mantissa and exponent together
    int b = Fp::Bits(k8.mant);
    return (k8.exp + b) * 256 + (int)(k8.mant << (8 - b));
}

int main(int argc, char** argv)
{
    std::map<std::pair<int, int>, int> obs;
    FILE* f = fopen(argv[1], "r");
    int p, cl; char o[8];
    while (fscanf(f, "%d %d %7s", &p, &cl, o) == 3)
        obs[{ p, cl }] = !strcmp(o, "0") ? 0 : !strcmp(o, "-1") ? -1 : !strcmp(o, "+1") ? 1 : 9;
    fclose(f);
    pvrtsp::PlaneConfig pc;
    std::vector<Cell> cells;
    for (int i = 0; i < NUM_QUADS; i++) {
        const mipk_quad_t& q = quads[i];
        Cell c;
        const float cx = (q.cell % 20) * 32.0f, cy = (q.cell / 20) * 32.0f;
        float x[3], y[3], z[3], u[3], v[3];
        for (int k = 0; k < 3; k++) {
            x[k] = cx + q.tri[k][0]; y[k] = cy + q.tri[k][1]; z[k] = F(q.z[k]); u[k] = F(q.uv[k][0]); v[k] = F(q.uv[k][1]);
        }
        c.pu.Setup(x, y, z, u, pc); c.pv.Setup(x, y, z, v, pc); c.pw.Setup(x, y, z, nullptr, pc);
        c.uA = c.pu.A0; c.uB = c.pu.B0; c.uC = c.pu.C0; c.vA = c.pv.A0; c.vB = c.pv.B0; c.vC = c.pv.C0;
        c.wA = c.pw.A0; c.wB = c.pw.B0; c.wC = c.pw.C0;
        c.inv = pvrfp::Recip(c.pw.Sdet, 24);
        c.dcalc = q.dcalc; c.bias = q.bias; c.sizeLog2 = 6; c.page = q.page;
        c.first[0] = u[0]; c.first[1] = v[0]; c.first[2] = z[0]; c.first[3] = x[0]; c.first[4] = y[0];
        c.obs = obs.count({ q.page, q.cell }) ? obs[{ q.page, q.cell }] : 9;
        cells.push_back(c);
    }
    // r2r_ring's triangle 13084C48 (size 128): the console's K8 is one step lower
    {
        Cell c;
        float x[3] = { 0x1.55abe4p+6f, 0x1.660df4p+6f, 0x1.5a3fa4p+6f }, y[3] = { 0x1.bcec4p+8f, 0x1.bb4acap+8f, 0x1.b8c50cp+8f };
        float z[3] = { 0x1.6ae9c6p+5f, 0x1.6d18c2p+5f, 0x1.6d0f18p+5f }, u[3] = { 0x1.60281cp-2f, 0x1.93711p-2f, 0x1.51be6ep-2f };
        float v[3] = { 0x1.01a24p-1f, 0x1.a7996p-2f, 0x1.a23314p-2f };
        c.pu.Setup(x, y, z, u, pc); c.pv.Setup(x, y, z, v, pc); c.pw.Setup(x, y, z, nullptr, pc);
        c.uA = c.pu.A0; c.uB = c.pu.B0; c.uC = c.pu.C0; c.vA = c.pv.A0; c.vB = c.pv.B0; c.vC = c.pv.C0;
        c.wA = c.pw.A0; c.wB = c.pw.B0; c.wC = c.pw.C0;
        c.inv = pvrfp::Recip(c.pw.Sdet, 24);
        c.dcalc = false; c.bias = 4; c.sizeLog2 = 7; c.obs = -1; c.page = 99;
        cells.push_back(c);
    }
    // mipmeas's archived sets: the model is the console's on every one, so a rule has to give the model's K8 there
    size_t nMipk = cells.size();
    {
        glob_t g;
        glob((gen::Here() + "/../../fit/mipmeas_data/*.cases.json").c_str(), 0, nullptr, &g);
        for (size_t f = 0; f < g.gl_pathc; f++) {
            const gen::Json d = gen::Json::Parse(gen::ReadFile(g.gl_pathv[f]));
            const gen::Json& pages = d.at("pages");
            for (size_t p = 0; p < pages.size(); p++)
                for (size_t ci = 0; ci < pages[p].size(); ci++) {
                    const gen::Json& q = pages[p][ci];
                    const float cx = (ci % 20) * 32.0f, cy = (ci / 20) * 32.0f;
                    float z4[4], u4[4], v4[4];
                    for (int k = 0; k < 4; k++) {
                        z4[k] = q.at("z").type == gen::Json::Arr ? (float)q.at("z")[k].num() : (float)q.at("z").num();
                        u4[k] = (float)q.at("uv")[k][0].num(); v4[k] = (float)q.at("uv")[k][1].num();
                    }
                    const int dc = q.has("dcalc") ? (int)q.at("dcalc").num() : 0, bias = (int)q.at("bias").num();
                    if (q.has("legs") || q.has("tri")) {
                        float x[3], y[3];
                        if (q.has("tri")) for (int k = 0; k < 3; k++) { x[k] = cx + (float)q.at("tri")[k][0].num(); y[k] = cy + (float)q.at("tri")[k][1].num(); }
                        else { const float a = (float)q.at("legs")[0].num(), b = (float)q.at("legs")[1].num(); x[0] = cx; y[0] = cy; x[1] = cx + a; y[1] = cy; x[2] = cx; y[2] = cy + b; }
                        cells.push_back(MakeCell(x, y, z4, u4, v4, dc, bias, 6, 0, 100 + (int)f));
                    } else {
                        const float X[4] = { cx, cx + 32, cx, cx + 32 }, Y[4] = { cy, cy, cy + 32, cy + 32 };
                        const int tri[2][3] = { { 0, 1, 2 }, { 2, 1, 3 } };
                        for (int t = 0; t < 2; t++) {
                            float x[3], y[3], z[3], u[3], v[3];
                            for (int k = 0; k < 3; k++) { int i = tri[t][k]; x[k] = X[i]; y[k] = Y[i]; z[k] = z4[i]; u[k] = u4[i]; v[k] = v4[i]; }
                            cells.push_back(MakeCell(x, y, z, u, v, dc, bias, 6, 0, 100 + (int)f));
                        }
                    }
                }
        }
        globfree(&g);
    }
    printf("%zu mipk cells (and r2r), %zu triangles of mipmeas sets\n", nMipk, cells.size() - nMipk);
    // the model's own K8 per cell
    Rule model = { 62, 0, 62, 0, 62, 0, 62, 62, 62, 62 };
    std::vector<int> base(cells.size());
    for (size_t i = 0; i < cells.size(); i++) base[i] = K8Of(cells[i], model);
    if (getenv("KFIT_DUMP")) {
        Rule best = { 23, 0, 24, 0, 22, 0, 62, 20, 20, 22 };
        FILE* o = fopen(getenv("KFIT_DUMP"), "w");
        fprintf(o, "page obs dist dcalc bias best_ok model_k nux nvx nuy nvy cux cvx cuy cvy\n");
        for (size_t i = 0; i < cells.size(); i++) {
            const Cell& c = cells[i];
            double k; K8Of(c, model, &k);
            int e; double mant = frexp(k, &e) * 256; double frac = mant - floor(mant);
            const bool above = c.page % 2 == 0 || c.page == 99;
            // the four numerators with the model and how much their products cancel (|p1 - p2| / max(|p1|, |p2|))
            Fp pu = c.uC, pv = c.vC, pw = c.wC;
            if (c.dcalc) {
                const Fp x1 = Fp::FromFloat(c.first[3]), y1 = Fp::FromFloat(c.first[4]);
                auto val1 = [&](const pvrtsp::Plane& pl) { return pvrfp::Add(pvrfp::Add(pvrfp::Mul(pl.A0, x1, 62), pvrfp::Mul(pl.B0, y1, 62), 62, 200), pl.C0, 62, 200); };
                pu = val1(c.pu); pv = val1(c.pv); pw = val1(c.pw);
            }
            auto mn = [&](const Fp& a1, const Fp& b1, const Fp& a2, const Fp& b2, double& canc) {
                const double p1 = pvrfp::Mul(a1, b2, 62).toDouble(), p2 = pvrfp::Mul(b1, a2, 62).toDouble();
                canc = fabs(p1 - p2) / std::max(1e-300, std::max(fabs(p1), fabs(p2)));
                return fabs(p1 - p2);
            };
            double cux, cvx, cuy, cvy;
            const double nux = mn(pu, c.uA, pw, c.wA, cux), nvx = mn(pv, c.vA, pw, c.wA, cvx);
            const double nuy = mn(c.uB, pu, c.wB, pw, cuy), nvy = mn(c.vB, pv, c.wB, pw, cvy);
            const int ok = (K8Of(c, best) - base[i]) == c.obs;
            fprintf(o, "%d %d %.6g %d %d %d %.12g %.9g %.9g %.9g %.9g %.4g %.4g %.4g %.4g\n", c.page, c.obs, above ? frac : 1 - frac, (int)c.dcalc, c.bias, ok, k, nux, nvx, nuy, nvy, cux, cvx, cuy, cvy);
        }
        fclose(o);
        return 0;
    }
    // the rules
    std::vector<Rule> rules;
    const int cws[] = { 62, 24, 23, 22 }, pws[] = { 62, 32, 26, 24 }, sws[] = { 24, 23, 22, 21, 20, 19, 18, 17, 16 };
    const int hws[] = { 62, 24, 20 }, lws[] = { 62, 24, 20, 16 }, parts[] = { 62, 24, 20, 16 }, gws[] = { 62, 26, 24, 22, 20 };
    if (getenv("KFIT_PATHS")) {
        const int pw2[] = { 62, 32, 24, 20 }, sw2[] = { 62, 24, 22, 20, 18 }, iws[] = { 62, 24, 22, 20, 18, 16 };
        const int hw2[] = { 62, 24 }, lw2[] = { 62, 24, 20, 16 }, pt2[] = { 62, 24, 20 }, gw2[] = { 62, 24, 22, 20 };
        for (int path = 1; path <= 2; path++) for (int iw : iws)
        for (int pw : pw2) for (int pm = 0; pm < (pw < 62 ? 3 : 1); pm++)
        for (int sw : sw2) for (int sm = 0; sm < (sw < 62 ? 3 : 1); sm++)
        for (int hw : hw2) for (int lw : lw2) for (int pt : pt2) for (int gw : gw2) {
            Rule r = { 62, 0, pw, pm, sw, sm, hw, lw, pt, gw };
            r.path = path; r.iw = iw;
            rules.push_back(r);
        }
    } else
    for (int cw : cws) for (int cm = 0; cm < (cw < 62 ? 3 : 1); cm++)
    for (int pw : pws) for (int pm = 0; pm < (pw < 62 ? 3 : 1); pm++)
    for (int sw : sws) for (int sm = 0; sm < (sw < 62 ? 3 : 1); sm++)
    for (int hw : hws) for (int lw : lws) for (int pt : parts) for (int gw : gws)
        rules.push_back({ cw, cm, pw, pm, sw, sm, hw, lw, pt, gw });
    std::vector<int> score(rules.size()), r2r(rules.size()), viol(rules.size());
    #pragma omp parallel for schedule(dynamic, 64)
    for (size_t k = 0; k < rules.size(); k++) {
        int v = 0;
        for (size_t i = nMipk; i < cells.size() && v <= 20; i++)
            v += K8Of(cells[i], rules[k]) != base[i];
        viol[k] = v;
        int s = 0;
        if (v <= 20)
            for (size_t i = 0; i < nMipk; i++) {
                const int d = K8Of(cells[i], rules[k]) - base[i];
                if (d == cells[i].obs) s++;
                if (cells[i].page == 99) r2r[k] = d == -1;
            }
        score[k] = s - 10000 * v;
    }
    std::vector<size_t> order(rules.size());
    for (size_t k = 0; k < order.size(); k++) order[k] = k;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return score[a] > score[b]; });
    printf("%zu rules over %zu cells (model: %d)\n", rules.size(), cells.size(), [&] { int s = 0; for (size_t i = 0; i < cells.size(); i++) s += cells[i].obs == 0; return s; }());
    for (int t = 0; t < 25 && t < (int)order.size(); t++)
        printf("  mipk %4d  mipmeas violations %2d%s  %s  %s\n", score[order[t]] + 10000 * viol[order[t]], viol[order[t]], viol[order[t]] > 20 ? "+" : " ", r2r[order[t]] ? "r2r ok " : "r2r BAD", rules[order[t]].Name().c_str());
}
