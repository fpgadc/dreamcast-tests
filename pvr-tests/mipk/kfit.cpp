// kfit: which arithmetic of K (pvrtsp::MipSelect) gives the console's 8 bit K in every mipk cell, and the model's in
// every triangle of mipmeas's archived sets (the model is the console's on all of them), and r2r_ring's triangle.
// Each mipk cell's observation (fit/mipk_data/<set>.observed.txt: page cell delta) is the console's K8 against the
// model's: 0, -1, +1. A grid of rules (where K's arithmetic is cut, and how); the best first.
//   g++ -O2 -std=c++17 -fopenmp -I <refsw3/refcore> -I model -I pvr-tests/mipk pvr-tests/mipk/kfit.cpp -o pvr-tests/mipk/kfit
//   pvr-tests/mipk/kfit fit/mipk_data/seed1.observed.txt      [KFIT_PATHS=1: numerators from the undivided sums]
// The baseline (base, and the observations' 0) is the old model's length, the longer plus 27146 / 65536 of the shorter.
// KFIT_FORM=1 with KFIT_ONE="9827858 24 ...": the console's length (mipk4; refsw3-core's model since): (longer + shorter)
// - 0x95F612 / 2^24 x shorter, each step cut to 24 bits. KFIT_LIST=1 (2: each cell, 3: with the numerators, 4: also
// mipmeas's triangles and r2r): the cells one rule misses. KFIT_ONE's extras: KFIT_REACH (the length's adder),
// KFIT_FMODES "sum product difference scale [widths]" (modes 0 toward zero, 2 nearest), KFIT_PATH (3: the console's
// numerators: of the undivided sums, the lengths of those, then once times 1/det^2; KFIT_IW), KFIT_RW / KFIT_RM / KFIT_CB
// / KFIT_CBM (coefficients made again: the reciprocal's and the product's width and mode). KFIT_NUM: a grid of the
// numerators' cuts. KFIT_REACHED=<cases.txt>: observations of stairs (analyze.py observe: 1, 0, ?) against each row's step.
// The console's K (refsw3-core): KFIT_PATH=3 KFIT_REACH=25 KFIT_FORM=1 KFIT_ONE="9827858 24 0 0 62 0 24 0 24 0 2".
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
#ifndef KFIT_CASES
#define KFIT_CASES "cases.h"
#endif
#include KFIT_CASES
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
    int path = 0;        // 0: divided coefficients; 1: undivided sums, then times inv and times inv; 2: times inv^2; 3: the length of the undivided ones, then times inv^2
    int iw = 62;         // those multiplications cut to iw bits
    int rw = 0, cb = 0;  // path 0 with rw: the coefficients made again for K, sum * Recip(det, rw) cut to cb bits
    int rm = 0, cbm = 0; // and the modes of those two cuts (0 toward zero, 2 to nearest)
    int64_t cnum = 27146; int cbits = 16;               // the shorter length's factor, cnum / 2^cbits (sqrt(2) - 1)
    int lg = 0, lgm = 0; // the shorter on a grid lg bits under the longer before its multiplication (0: not)
    int hg = 0;          // the longer on its own grid hg bits under its top (0: the part's grid gw, as before; 62: exact)
    int shiftadd = 0;    // the part as a sum of the shorter shifted by each 1 bit of cnum, each term on the grid (gw) first
    int sc = 62, scm = 0, ptm = 0, gm = 0;   // the sum cut to sc bits of its own (mode scm); the part's cut mode; the grid's mode
    int dv = 0;          // DCalcCtrl 1: 0 the planes at the first vertex exactly; 1 the vertex's own u1 z1, v1 z1, z1; 2 the planes there in 24 bits
    int fs = 0, fq = 0, fd = 0, fk = 0, fsb = 24, fqb = 24, fdb = 24;   // form 1: the modes (0 toward zero, 2 nearest) and widths of its sum,
                         // product and difference, and of path 3's scaling
    int reach = 200;     // form 1: the reach of its adder (an operand this many exponents below the other is dropped)
    int form = 0;        // 1: the console's length (mipk4): (longer + shorter) - cnum / 2^cbits x shorter, each step cut to 24 bits
    std::string Name() const {
        char b[200];
        snprintf(b, sizeof b, "form %d higrid %d shiftadd %d logrid %d/%d sum %d/%d partmode %d gridmode %d dv %d const %lld/2^%d path %d inv %d recip %d coefprod %d coef %d/%d prod %d/%d diff %d/%d hi %d lo %d part %d grid %d", form, hg, shiftadd, lg, lgm, sc, scm, ptm, gm, dv, (long long)cnum, cbits, path, iw, rw, cb, cw, cm, pw, pm, sw, sm, hw, lw, partw, gw);
        return b;
    }
};

struct Cell {
    Fp uA, uB, uC, vA, vB, vC, wA, wB, wC;     // A0, B0, C0 of the three planes (or the first vertex's values in C)
    bool dcalc;
    float first[5];
    float vx[3], vy[3], vz[3], vu[3], vv[3];      // the corners
    int bias, sizeLog2, obs, page;
    double stepK = 0;   // KFIT_REACHED: the row's step
    pvrtsp::Plane pu, pv, pw;
    Fp inv;
};

static Cell MakeCell(const float* x, const float* y, const float* z, const float* u, const float* v, int dcalc, int bias, int sizeLog2, int obs, int page)
{
    pvrtsp::PlaneConfig pc;
    Cell c;
    c.pu.Setup(F32s(x), F32s(y), F32s(z), F32s(u), pc); c.pv.Setup(F32s(x), F32s(y), F32s(z), F32s(v), pc); c.pw.Setup(F32s(x), F32s(y), F32s(z), nullptr, pc);
    c.uA = c.pu.A0; c.uB = c.pu.B0; c.uC = c.pu.C0; c.vA = c.pv.A0; c.vB = c.pv.B0; c.vC = c.pv.C0;
    c.wA = c.pw.A0; c.wB = c.pw.B0; c.wC = c.pw.C0;
    c.inv = pvrfp::Recip(c.pw.Sdet, 24);
    c.dcalc = dcalc; c.bias = bias; c.sizeLog2 = sizeLog2; c.obs = obs; c.page = page;
    c.first[0] = u[0]; c.first[1] = v[0]; c.first[2] = z[0]; c.first[3] = x[0]; c.first[4] = y[0];
    for (int k = 0; k < 3; k++) { c.vx[k] = x[k]; c.vy[k] = y[k]; c.vz[k] = z[k]; c.vu[k] = u[k]; c.vv[k] = v[k]; }
    return c;
}

// KFIT_LIST=3: what the last K8Of on this thread computed (each numerator's two products and the difference; lx, ly)
struct Diag { double p1[4], p2[4], n[4]; double lx, ly; int nm; };
static thread_local Diag diag;

static int K8Of(const Cell& c, const Rule& r, double* kOut = nullptr)
{
    diag.nm = 0;
    auto co = [&](const Fp& x) { return CutM(x, r.cw, r.cm); };
    // the coefficients: the planes' own (A0, B0, C0), or made again for K with another reciprocal and product
    Fp uA = c.uA, uB = c.uB, uC = c.uC, vA = c.vA, vB = c.vB, vC = c.vC, wA = c.wA, wB = c.wB, wC = c.wC;
    if (r.rw) {
        auto below = [](const Fp& x) { return !x.zero() && x.top() - 1 < -126; };     // the setup's range: flushed to 0
        Fp inv = CutM(pvrfp::Recip(c.pw.Sdet, 32), r.rw, r.rm);
        if (below(inv)) inv = Fp();
        auto m = [&](const Fp& sum) { Fp x = CutM(pvrfp::Mul(sum, inv, 62), r.cb, r.cbm); return below(x) ? Fp() : x; };
        uA = m(c.pu.Sa); uB = m(c.pu.Sb); uC = m(c.pu.Sc); vA = m(c.pv.Sa); vB = m(c.pv.Sb); vC = m(c.pv.Sc);
        wA = m(c.pw.Sa); wB = m(c.pw.Sb); wC = m(c.pw.Sc);
    }
    // the values where the numerators are taken: the planes' constants (DCalcCtrl 0) or at the first vertex
    Fp pu = uC, pv = vC, pw = wC;
    if (c.dcalc) {
        const Fp x1 = Fp::FromFloat(c.first[3]), y1 = Fp::FromFloat(c.first[4]);
        auto val1 = [&](const Fp& A, const Fp& B, const Fp& C) { return pvrfp::Add(pvrfp::Add(pvrfp::Mul(A, x1, 62), pvrfp::Mul(B, y1, 62), 62, 200), C, 62, 200); };
        pu = val1(uA, uB, uC); pv = val1(vA, vB, vC); pw = val1(wA, wB, wC);
        if (r.dv == 1) {
            const Fp z1 = Fp::FromFloat(c.first[2]);
            pu = pvrfp::Mul(Fp::FromFloat(c.first[0]), z1, 24); pv = pvrfp::Mul(Fp::FromFloat(c.first[1]), z1, 24); pw = z1;
        } else if (r.dv == 2) {
            auto val24 = [&](const Fp& A, const Fp& B, const Fp& C) { return pvrfp::Add(pvrfp::Add(pvrfp::Mul(A, x1, 24), pvrfp::Mul(B, y1, 24), 24, 25), C, 24, 25); };
            pu = val24(uA, uB, uC); pv = val24(vA, vB, vC); pw = val24(wA, wB, wC);
        }
    }
    auto minor = [&](const Fp& a1, const Fp& b1, const Fp& a2, const Fp& b2) {
        Fp p1 = CutM(pvrfp::Mul(co(a1), co(b2), 62), r.pw, r.pm), p2 = CutM(pvrfp::Mul(co(b1), co(a2), 62), r.pw, r.pm);
        Fp n = CutM(pvrfp::Sub(p1, p2, 62, 200), r.sw, r.sm);
        if (diag.nm < 4) { diag.p1[diag.nm] = p1.toDouble(); diag.p2[diag.nm] = p2.toDouble(); diag.n[diag.nm] = n.toDouble(); diag.nm++; }
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
        if (r.lg) lo = GridM(lo, hi.top() - r.lg, r.lgm);
        if (lo.zero()) return hi;
        if (r.form == 1) {
            Fp k2 = Fp::FromHalves(2 * r.cnum);
            k2.exp -= r.cbits;
            const Fp s = CutM(pvrfp::Add(hi, lo, 62, r.reach), r.fsb, r.fs), q = CutM(pvrfp::Mul(lo, k2, 62), r.fqb, r.fq);
            return CutM(pvrfp::Sub(s, q, 62, r.reach), r.fdb, r.fd);
        }
        Fp part;
        if (r.shiftadd) {
            const int g = hi.top() - r.gw;
            for (int i = 0; i < 62; i++)
                if ((r.cnum >> i) & 1) {
                    Fp t = lo;
                    t.exp += i - r.cbits;
                    part = pvrfp::Add(part, GridM(t, g, 0), 62, 200);
                }
        } else {
            part = pvrfp::Mul(lo, Fp::FromHalves(2 * r.cnum), 62);
            part.exp -= r.cbits;
        }
        part = CutM(part, r.partw, r.ptm);
        Fp sum;
        if (r.gw < 62) {
            const int g = hi.top() - r.gw;
            const Fp h = r.hg == 0 ? GridM(hi, g, r.gm) : r.hg >= 62 ? hi : GridM(hi, hi.top() - r.hg, 0);
            sum = pvrfp::Add(h, GridM(part, g, r.gm), 62, 200);
        } else
            sum = pvrfp::Add(hi, part, 62, 200);
        return CutM(sum, r.sc, r.scm);
    };
    Fp lx, ly;
    if (r.path == 0) {
        lx = length(minor(pu, uA, pw, wA), minor(pv, vA, pw, wA));
        ly = length(minor(uB, pu, wB, pw), minor(vB, pv, wB, pw));
    } else {
        Fp su = c.pu.Sc, sv = c.pv.Sc, sw = c.pw.Sc;
        if (c.dcalc) {
            const Fp x1 = Fp::FromFloat(c.first[3]), y1 = Fp::FromFloat(c.first[4]);
            auto at1 = [&](const pvrtsp::Plane& pl) {
                if (r.dv == 2)                          // in the setup's 24 bit arithmetic
                    return pvrfp::Add(pvrfp::Add(pvrfp::Mul(pl.Sa, x1, 24), pvrfp::Mul(pl.Sb, y1, 24), 24, 53), pl.Sc, 24, 53);
                return pvrfp::Add(pvrfp::Add(pvrfp::Mul(pl.Sa, x1, 62), pvrfp::Mul(pl.Sb, y1, 62), 62, 200), pl.Sc, 62, 200);
            };
            su = at1(c.pu); sv = at1(c.pv); sw = at1(c.pw);
        }
        lx = length(minor(su, c.pu.Sa, sw, c.pw.Sa), minor(sv, c.pv.Sa, sw, c.pw.Sa));
        ly = length(minor(c.pu.Sb, su, c.pw.Sb, sw), minor(c.pv.Sb, sv, c.pw.Sb, sw));
    }
    diag.lx = lx.toDouble(); diag.ly = ly.toDouble();
    Fp best = lx.toDouble() >= ly.toDouble() ? lx : ly;
    if (r.path == 3)        // the lengths of the undivided numerators, then once times 1/det^2 (inv * inv, each cut to 24 bits)
        best = CutM(pvrfp::Mul(best, CutM(pvrfp::Mul(c.inv, c.inv, 62), 24, 0), 62), 24, r.fk);
    Fp k = pvrfp::Mul(best, Fp::FromHalves(2 * (int64_t)c.bias), 62);
    k.exp += c.sizeLog2 - 2;
    if (kOut) *kOut = k.toDouble();
    Fp k8 = k.cut(8);
    // the 8 bit value as one integer: mantissa and exponent together
    int b = Fp::Bits(k8.mant);
    return (k8.exp + b) * 128 + ((int)(k8.mant << (8 - b)) - 128);
}

int main(int argc, char** argv)
{
    std::map<std::pair<int, int>, int> obs;
    FILE* f = fopen(argv[1], "r");
    int p, cl; char o[8];
    // KFIT_REACHED=<cases.txt> (stairs, pvr-tests/mipk/analyze.py observe): "page cell 1|0|?", whether the console's K8
    // reaches the row's step; the step (the model's K at it) from the cases.txt given, its last column
    const bool reachedMode = getenv("KFIT_REACHED") != nullptr;
    while (fscanf(f, "%d %d %7s", &p, &cl, o) == 3)
        obs[{ p, cl }] = reachedMode ? (!strcmp(o, "1") ? 1 : !strcmp(o, "0") ? 0 : 9) : !strcmp(o, "0") ? 0 : !strcmp(o, "-1") ? -1 : !strcmp(o, "+1") ? 1 : 9;
    fclose(f);
    std::map<std::pair<int, int>, double> stepK;
    if (reachedMode) {
        FILE* t = fopen(getenv("KFIT_REACHED"), "r");
        char line[512];
        while (fgets(line, sizeof line, t)) {
            if (line[0] == '#') continue;
            int pg, c; char* e = line; pg = (int)strtol(e, &e, 10); c = (int)strtol(e, &e, 10);
            const char* last = strrchr(line, ' ');
            stepK[{ pg, c }] = atof(last + 1);
            (void)e;
        }
        fclose(t);
    }
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
        c.pu.Setup(F32s(x), F32s(y), F32s(z), F32s(u), pc); c.pv.Setup(F32s(x), F32s(y), F32s(z), F32s(v), pc); c.pw.Setup(F32s(x), F32s(y), F32s(z), nullptr, pc);
        c.uA = c.pu.A0; c.uB = c.pu.B0; c.uC = c.pu.C0; c.vA = c.pv.A0; c.vB = c.pv.B0; c.vC = c.pv.C0;
        c.wA = c.pw.A0; c.wB = c.pw.B0; c.wC = c.pw.C0;
        c.inv = pvrfp::Recip(c.pw.Sdet, 24);
        c.dcalc = q.dcalc; c.bias = q.bias; c.sizeLog2 = 6; c.page = q.page;
        c.first[0] = u[0]; c.first[1] = v[0]; c.first[2] = z[0]; c.first[3] = x[0]; c.first[4] = y[0];
        for (int k = 0; k < 3; k++) { c.vx[k] = x[k]; c.vy[k] = y[k]; c.vz[k] = z[k]; c.vu[k] = u[k]; c.vv[k] = v[k]; }
        c.obs = obs.count({ q.page, q.cell }) ? obs[{ q.page, q.cell }] : 9;
        if (reachedMode) c.stepK = stepK[{ q.page, q.cell }];
        cells.push_back(c);
    }
    // r2r_ring's triangle 13084C48 (size 128): the console's K8 is one step lower
    {
        Cell c;
        float x[3] = { 0x1.55abe4p+6f, 0x1.660df4p+6f, 0x1.5a3fa4p+6f }, y[3] = { 0x1.bcec4p+8f, 0x1.bb4acap+8f, 0x1.b8c50cp+8f };
        float z[3] = { 0x1.6ae9c6p+5f, 0x1.6d18c2p+5f, 0x1.6d0f18p+5f }, u[3] = { 0x1.60281cp-2f, 0x1.93711p-2f, 0x1.51be6ep-2f };
        float v[3] = { 0x1.01a24p-1f, 0x1.a7996p-2f, 0x1.a23314p-2f };
        c.pu.Setup(F32s(x), F32s(y), F32s(z), F32s(u), pc); c.pv.Setup(F32s(x), F32s(y), F32s(z), F32s(v), pc); c.pw.Setup(F32s(x), F32s(y), F32s(z), nullptr, pc);
        c.uA = c.pu.A0; c.uB = c.pu.B0; c.uC = c.pu.C0; c.vA = c.pv.A0; c.vB = c.pv.B0; c.vC = c.pv.C0;
        c.wA = c.pw.A0; c.wB = c.pw.B0; c.wC = c.pw.C0;
        c.inv = pvrfp::Recip(c.pw.Sdet, 24);
        c.dcalc = false; c.bias = 4; c.sizeLog2 = 7; c.obs = -1; c.page = 99;
        for (int k = 0; k < 3; k++) { c.vx[k] = x[k]; c.vy[k] = y[k]; c.vz[k] = z[k]; c.vu[k] = u[k]; c.vv[k] = v[k]; }
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
        Rule best = { 62, 0, 24, 0, 26, 2, 62, 23, 62, 24 };
        best.cnum = 3474679; best.cbits = 23;
        FILE* o = fopen(getenv("KFIT_DUMP"), "w");
        fprintf(o, "page obs dist dcalc bias best_ok model_k nux nvx nuy nvy cux cvx cuy cvy pred\n");
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
            const int pred = K8Of(c, best) - base[i], ok = pred == c.obs;
            fprintf(o, "%d %d %.6g %d %d %d %.12g %.9g %.9g %.9g %.9g %.4g %.4g %.4g %.4g %d\n", c.page, c.obs, above ? frac : 1 - frac, (int)c.dcalc, c.bias, ok, k, nux, nvx, nuy, nvy, cux, cvx, cuy, cvy, pred);
        }
        fclose(o);
        return 0;
    }
    if (getenv("KFIT_SHOW")) {
        // the misses of one rule: KFIT_SHOW="cnum cbits lo part grid" (exact mix)
        Rule r = model;
        long long cn; int cbt, lw, pt, gw, lg = 0, lgm = 0;
        sscanf(getenv("KFIT_SHOW"), "%lld %d %d %d %d %d %d", &cn, &cbt, &lw, &pt, &gw, &lg, &lgm);
        r.cnum = cn; r.cbits = cbt; r.lw = lw; r.partw = pt; r.gw = gw; r.lg = lg; r.lgm = lgm;
        printf("page obs pred | hi lo | need/lo  c*lo-need (in 2^-24 of hi)  hi's exponent  lo/hi\n");
        for (size_t i = 0; i < nMipk; i++) {
            const Cell& c = cells[i];
            const int pred = K8Of(c, r) - base[i];
            if (pred == c.obs || c.page < 2 || c.page > 5) continue;
            const long double nu = fabsl((long double)c.uA.toDouble() * c.wC.toDouble()), nv = fabsl((long double)c.vA.toDouble() * c.wC.toDouble());
            const long double hi = nu > nv ? nu : nv, lo = nu > nv ? nv : nu;
            double kModel; K8Of(c, model, &kModel);
            const long double scale = kModel / (hi + lo * 27146.0L / 65536.0L);
            int e; const double m = frexp(kModel, &e) * 256;
            const long double stepK = ldexpl(c.page % 2 == 0 ? floor(m) : floor(m) + 1, e - 8), need = stepK / scale - hi;
            int eh; frexpl(hi, &eh);
            // in units of the grid 23 bits under the longer: the shorter, the need, and what the rule adds
            const long double g = ldexpl(1, eh - 23);
            printf("%d %+d %+d | hi %.1Lf lo %.4Lf need %.4Lf c*lo %.4Lf (grid units) | lo/hi %.4Lf dcalc %d\n", c.page, c.obs, pred, hi / g, lo / g, need / g,
                   lo * ((long double)cn / ldexpl(1, cbt)) / g, lo / hi, (int)c.dcalc);
        }
        return 0;
    }
    if (getenv("KFIT_STAIR")) {
        // mipk3: each row's shorter (units of the 24 bit grid under the longer), the model's part, and the console's part
        // from where the row turns: lo, D = T - hi(step 0), s (first step reached; 99: none), f in [D - s, D - s + 1)
        std::map<int, std::vector<std::pair<int, int>>> rowsObs;              // row -> (step, reached)
        for (size_t i = 0; i < nMipk; i++) {
            const Cell& c = cells[i];
            if (c.page > 7) continue;
            const int cellNo = (int)(i % 300), step = cellNo % 20 - 10, row = c.page * 15 + cellNo / 20;
            const bool up = step >= 0;
            rowsObs[row].push_back({ step, (up && c.obs == 0) || (!up && c.obs == 1) });
        }
        printf("row lo D model_part s console_part_lo deficit(model - console, units)\n");
        for (auto& [row, v] : rowsObs) {
            const size_t i0 = (size_t)(row / 15) * 300 + (size_t)(row % 15) * 20 + 10;         // step 0
            const Cell& c = cells[i0];
            const long double nu = fabsl((long double)c.uA.toDouble() * c.wC.toDouble()), nv = fabsl((long double)c.vA.toDouble() * c.wC.toDouble());
            const long double hi = nu > nv ? nu : nv, lo = nu > nv ? nv : nu;
            double kModel; K8Of(c, model, &kModel);
            const long double scale = kModel / (hi + lo * 27146.0L / 65536.0L);
            int es; const long double fs = frexpl(scale, &es);
            const long double sc2 = ldexpl(fs > 0.75L ? 1.0L : 0.5L, es);
            int eh; frexpl(hi, &eh);
            const long double g = ldexpl(1, eh - 24);
            const long double T = ldexpl(130, -6 - 7 + 7) ;   // placeholder
            (void)T;
            // the step: 130 * 2^n in K, the nearest to the model's K
            int ek; const double mk = frexp(kModel, &ek) * 256;
            const long double stepK = ldexpl(floor(mk), ek - 8);                     // the model's K8 at step 0: the step crossed
            const long double D = (stepK / sc2 - hi) / g;
            int s = 99;
            for (auto& [st, r] : v) if (r) { s = std::min(s, st); }
            const long double mp = lo / g * 27146.0L / 65536.0L;
            printf("%3d %.4Lf %.4Lf %.4Lf %d %.4Lf %.2Lf %.1Lf\n", row, lo / g, D, mp, s, D - s, s == 99 ? -1.0L : mp - (D - s), hi / g);
        }
        return 0;
    }
    if (getenv("KFIT_BAND")) {
        // the exact cells whose need / shorter is in the band where the bounds conflict, in units of 2^(e - 24), e the
        // longer one's exponent: its own value, the shorter, the need; reached or not
        printf("obs need/lo | hi lo need (grid 24 units) | frac(lo) lo/hi\n");
        std::vector<std::pair<long double, std::string>> rows;
        for (size_t i = 0; i < nMipk; i++) {
            const Cell& c = cells[i];
            if (c.page < 2 || c.page > 5) continue;
            const long double nu = fabsl((long double)c.uA.toDouble() * c.wC.toDouble()), nv = fabsl((long double)c.vA.toDouble() * c.wC.toDouble());
            const long double hi = nu > nv ? nu : nv, lo = nu > nv ? nv : nu;
            double kModel; K8Of(c, model, &kModel);
            int e; const double m = frexp(kModel, &e) * 256;
            // the step in length units: the model's K / its length is exactly the scale (a power of two times the bias)
            const long double scale = kModel / (hi + lo * 27146.0L / 65536.0L);
            int es; const long double fs = frexpl(scale, &es);
            const long double sc2 = ldexpl(fs > 0.75L ? 1.0L : 0.5L, es) ;          // round the scale to its power of two
            const long double stepK = ldexpl(c.page % 2 == 0 ? floor(m) : floor(m) + 1, e - 8), need = stepK / sc2 - hi;
            const long double r = need / lo;
            if (getenv("KFIT_BAND_ALL") == nullptr && (r < 0.4142110L || r > 0.4142145L)) continue;
            int eh; frexpl(hi, &eh);
            const long double g = ldexpl(1, eh - 24);
            char b[200];
            const bool reached = c.page % 2 == 0 ? c.obs == 0 : c.obs == 1;
            snprintf(b, sizeof b, "%s %.9Lf | %.2Lf %.4Lf %.4Lf | %.4Lf %.4Lf", reached ? "reached" : "short  ", r, hi / g, lo / g, need / g, lo / g - floorl(lo / g), lo / hi);
            rows.push_back({ r, b });
        }
        std::sort(rows.begin(), rows.end());
        for (auto& x : rows) printf("%s\n", x.second.c_str());
        return 0;
    }
    if (getenv("KFIT_BOUNDS")) {
        // the exact mix (mipk2): the console's part of the shorter numerator, against T - longer: the bounds on c in
        // part = c * shorter that every cell allows
        long double lb = 0, ub = 1;
        int nlb = 0, nub = 0;
        for (size_t i = 0; i < nMipk; i++) {
            const Cell& c = cells[i];
            if (c.page < 2 || c.page > 5) continue;
            // the two numerators of x (the longer direction here): exact
            const long double nu = fabsl((long double)c.uA.toDouble() * c.wC.toDouble()), nv = fabsl((long double)c.vA.toDouble() * c.wC.toDouble());
            const long double hi = nu > nv ? nu : nv, lo = nu > nv ? nv : nu;
            double kModel;
            K8Of(c, model, &kModel);
            const long double scale = kModel / (hi + lo * 27146.0L / 65536.0L);       // K per unit of length
            int e;
            const double m = frexp(kModel, &e) * 256;                                 // 128 .. 256
            const bool above = c.page % 2 == 0;
            const long double stepK = ldexpl(above ? floor(m) : floor(m) + 1, e - 8);
            const long double need = stepK / scale - hi;                              // the part that reaches the step
            const long double r = need / lo;
            if (above && c.obs == 0) { if (r > lb) lb = r; nlb++; }                   // reached: c >= r
            else { if (r < ub) ub = r; nub++; }                                        // did not: c < r
        }
        printf("c in [%.12Lf, %.12Lf) from %d + %d cells: 27146/65536 = %.12f, sqrt(2) - 1 = %.12f\n", lb, ub, nlb, nub, 27146 / 65536.0, sqrt(2.0) - 1);
        printf("  in units of 2^-24: [%.3Lf, %.3Lf)\n", lb * 16777216.0L, ub * 16777216.0L);
        return 0;
    }
    if (getenv("KFIT_CHECK")) {
        Rule r = model; r.rw = 24; r.cb = 24;
        int bad = 0;
        for (size_t i = 0; i < cells.size(); i++) {
            double k1, k2;
            const int a = K8Of(cells[i], model, &k1), b = K8Of(cells[i], r, &k2);
            if (a != b && bad++ < 5)
                printf("cell %zu page %d dcalc %d: model K %.12g, made again %.12g; wA %.9g / %.9g\n", i, cells[i].page, (int)cells[i].dcalc, k1, k2,
                       cells[i].wA.toDouble(), pvrfp::Mul(cells[i].pw.Sa, pvrfp::Recip(cells[i].pw.Sdet, 24), 24).toDouble());
        }
        printf("%d of %zu cells differ\n", bad, cells.size());
        return 0;
    }
    // the rules
    std::vector<Rule> rules;
    const int cws[] = { 62, 24, 23, 22 }, pws[] = { 62, 32, 26, 24 }, sws[] = { 24, 23, 22, 21, 20, 19, 18, 17, 16 };
    const int hws[] = { 62, 24, 20 }, lws[] = { 62, 24, 20, 16 }, parts[] = { 62, 24, 20, 16 }, gws[] = { 62, 26, 24, 22, 20 };
    if (getenv("KFIT_HG")) {
        const int64_t cs[][2] = { { 6949355, 24 }, { 6949357, 24 }, { 6949358, 24 }, { 6949359, 24 }, { 6949360, 24 }, { 6949361, 24 }, { 6949363, 24 },
                                  { 434333, 20 }, { 434334, 20 }, { 434335, 20 }, { 1737338, 22 }, { 1737339, 22 }, { 1737340, 22 },
                                  { 13898716, 25 }, { 13898718, 25 }, { 13898720, 25 }, { 27797433, 26 }, { 27797437, 26 }, { 27797441, 26 } };
        const int lgH[] = { 0, 25, 24, 23, 22 }, gwH[] = { 25, 24, 23, 22 }, hgH[] = { 0, 62, 24, 23 }, scH[] = { 62, 25, 24, 23 };
        for (auto& cc : cs)
        for (int lg : lgH) for (int lgm = 0; lgm < (lg ? 3 : 1); lgm += 2)
        for (int gw : gwH) for (int gm = 0; gm < 3; gm += 2)
        for (int hg : hgH) for (int sc : scH) for (int scm = 0; scm < (sc < 62 ? 3 : 1); scm += 2) {
            Rule r = { 62, 0, 24, 0, 26, 2, 62, 62, 62, gw };
            r.cnum = cc[0]; r.cbits = (int)cc[1]; r.lg = lg; r.lgm = lgm; r.gm = gm; r.hg = hg; r.sc = sc; r.scm = scm; r.dv = 2;
            rules.push_back(r);
        }
    } else if (getenv("KFIT_WIDE")) {
        const int lgW[] = { 0, 26, 25, 24, 23, 22, 21 }, gwW[] = { 62, 26, 25, 24, 23, 22, 21 };
        for (int bits = 16; bits <= 26; bits++) {
            const int64_t n0 = (int64_t)floor(0.41421 * (double)(1LL << bits)), n1 = (int64_t)ceil(0.41422 * (double)(1LL << bits));
            const int64_t stepN = bits > 20 ? (int64_t)1 << (bits - 20) : 1;           // at most ~650 constants a width
            for (int64_t n = n0; n <= n1; n += stepN)
            for (int lg : lgW) for (int lgm = 0; lgm < (lg ? 3 : 1); lgm += 2)
            for (int gw : gwW) for (int gm = 0; gm < (gw < 62 ? 3 : 1); gm += 2) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, 62, 62, gw };
                r.cnum = n; r.cbits = bits; r.lg = lg; r.lgm = lgm; r.gm = gm;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_SQRT2")) {
        // sqrt(2) - 1 to 22 .. 28 bits, both truncated and rounded, with rounding on the way (the part, the grid, the sum)
        const int gwQ[] = { 62, 26, 25, 24, 23 }, lgQ[] = { 0, 26, 25, 24, 23 }, ptQ[] = { 62, 26, 25, 24, 23 }, scQ[] = { 62, 26, 25, 24 };
        for (int bits = 22; bits <= 28; bits++) {
            const int64_t lo0 = (int64_t)floor((sqrt(2.0) - 1) * (double)(1LL << bits));
            for (int64_t n = lo0 - 1; n <= lo0 + 2; n++)
            for (int gw : gwQ) for (int gm = 0; gm < (gw < 62 ? 3 : 1); gm += 2)
            for (int lg : lgQ) for (int lgm = 0; lgm < (lg ? 3 : 1); lgm += 2)
            for (int pt : ptQ) for (int ptm = 0; ptm < (pt < 62 ? 3 : 1); ptm += 2)
            for (int sc : scQ) for (int scm = 0; scm < (sc < 62 ? 3 : 1); scm += 2) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, 62, pt, gw };
                r.cnum = n; r.cbits = bits; r.gm = gm; r.lg = lg; r.lgm = lgm; r.ptm = ptm; r.sc = sc; r.scm = scm;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_SHIFT")) {
        const int gwS[] = { 26, 25, 24, 23, 22, 21, 20 }, lgS[] = { 0, 26, 25, 24, 23, 22 };
        for (int bits = 16; bits <= 26; bits++) {
            const int64_t mid = (int64_t)llround((sqrt(2.0) - 1) * (double)(1LL << bits));
            for (int64_t n = mid - 4; n <= mid + 12; n++)
            for (int gw : gwS) for (int lg : lgS) for (int lgm = 0; lgm < (lg ? 3 : 1); lgm += 2) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, 62, 62, gw };
                r.cnum = n; r.cbits = bits; r.shiftadd = 1; r.lg = lg; r.lgm = lgm;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_LG")) {
        const int lgs[] = { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28 }, gwA[] = { 62, 26, 25, 24, 23, 22, 21, 20 }, ptA[] = { 62, 24, 22 };
        for (int bits = 24; bits <= 26; bits++) {
            const int64_t n0 = (int64_t)floor(0.4142136 * (double)(1LL << bits)), n1 = (int64_t)ceil(0.4142146 * (double)(1LL << bits));
            for (int64_t n = n0; n <= n1; n++)
            for (int lg : lgs) for (int lgm = 0; lgm < 3; lgm += 2)
            for (int pt : ptA) for (int gw : gwA) for (int gm = 0; gm < (gw < 62 ? 3 : 1); gm += 2) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, 62, pt, gw };
                r.cnum = n; r.cbits = bits; r.lg = lg; r.lgm = lgm; r.gm = gm;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_SUM")) {
        const int lw9[] = { 62, 24, 23, 22 }, pt9[] = { 62, 24, 23, 22 }, gw9[] = { 62, 26, 25, 24, 23, 22 }, sc9[] = { 62, 26, 25, 24, 23, 22, 21, 20 };
        for (int bits = 24; bits <= 26; bits++) {
            const int64_t n0 = (int64_t)floor(0.4142138 * (double)(1LL << bits)), n1 = (int64_t)ceil(0.4142144 * (double)(1LL << bits));
            for (int64_t n = n0; n <= n1; n++)
            for (int lw : lw9) for (int pt : pt9) for (int ptm = 0; ptm < (pt < 62 ? 3 : 1); ptm += 2)
            for (int gw : gw9) for (int gm = 0; gm < (gw < 62 ? 3 : 1); gm += 2)
            for (int sc : sc9) for (int scm = 0; scm < (sc < 62 ? 3 : 1); scm += 2) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, lw, pt, gw };
                r.cnum = n; r.cbits = bits; r.sc = sc; r.scm = scm; r.ptm = ptm; r.gm = gm;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_NUM")) {
        // the numerators' arithmetic with the console's length (set KFIT_FORM=1 too): the coefficients cut (16 bits: the
        // TSP's own), the two products, their difference, DCalcCtrl 1's values, the numerators before the length
        const int cw9[] = { 62, 24, 23, 22, 21, 20, 18, 16 }, pw9[] = { 62, 26, 25, 24, 23 }, sw9[] = { 62, 26, 25, 24, 23, 22 }, nw9[] = { 62, 24 };
        for (int cw : cw9) for (int cm = 0; cm < (cw < 62 ? 3 : 1); cm += 2)
        for (int pw : pw9) for (int pm = 0; pm < (pw < 62 ? 3 : 1); pm++)
        for (int sw : sw9) for (int sm = 0; sm < (sw < 62 ? 3 : 1); sm++)
        for (int dv = 0; dv <= 2; dv++) for (int nw : nw9) {
            Rule r = { cw, cm, pw, pm, sw, sm, nw, nw, 62, 62 };
            r.dv = dv;
            rules.push_back(r);
        }
    } else if (getenv("KFIT_EXACT")) {
        // the length alone (mipk2): constants from 0.4142135 to 0.4142146 at 22 .. 26 bits, the cuts of the shorter,
        // of its part and of the sum's grid under the longer
        const int lw8[] = { 62, 26, 24, 23, 22, 21, 20, 18, 16 }, pt8[] = { 62, 26, 24, 23, 22, 21, 20, 18, 16 }, gw8[] = { 62, 28, 26, 25, 24, 23, 22, 21, 20 };
        for (int bits = 22; bits <= 26; bits++) {
            const int64_t n0 = (int64_t)floor(0.4142135 * (double)(1LL << bits)), n1 = (int64_t)ceil(0.4142146 * (double)(1LL << bits));
            for (int64_t n = n0; n <= n1; n++)
            for (int lw : lw8) for (int pt : pt8) for (int gw : gw8) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, lw, pt, gw };
                r.cnum = n; r.cbits = bits;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_DV")) {
        const int pw7[] = { 62, 32, 28, 26, 24 }, sw7[] = { 62, 28, 26, 25, 24, 23 }, lw7[] = { 24, 23, 22 }, gw7[] = { 25, 24 };
        for (int dv = 0; dv <= 2; dv++)
        for (int64_t n = 3474677; n <= 3474681; n++)
        for (int lw : lw7) for (int gw : gw7)
        for (int pw : pw7) for (int pm = 0; pm < (pw < 62 ? 3 : 1); pm++)
        for (int sw : sw7) for (int sm = 0; sm < (sw < 62 ? 3 : 1); sm++) {
            Rule r = { 62, 0, pw, pm, sw, sm, 62, lw, 62, gw };
            r.cnum = n; r.cbits = 23; r.dv = dv;
            rules.push_back(r);
        }
    } else if (getenv("KFIT_CONST3")) {
        const double c0 = 1737339.0 / (1 << 22);
        const int lw6[] = { 24, 23, 22, 21 }, gw6[] = { 25, 24, 23 }, pw6[] = { 62, 26, 24 }, sw6[] = { 62, 26, 24, 23, 22 }, pt6[] = { 62, 24 };
        for (int bits = 20; bits <= 24; bits++) {
            const int64_t mid = (int64_t)llround(c0 * (double)(1LL << bits));
            for (int64_t n = mid - 3; n <= mid + 3; n++)
            for (int lw : lw6) for (int gw : gw6) for (int pt : pt6)
            for (int pw : pw6) for (int pm = 0; pm < (pw < 62 ? 3 : 1); pm += 2)
            for (int sw : sw6) for (int sm = 0; sm < (sw < 62 ? 3 : 1); sm += 2) {
                Rule r = { 62, 0, pw, pm, sw, sm, 62, lw, pt, gw };
                r.cnum = n; r.cbits = bits;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_CONST2")) {
        const double c = 0.41421356237309504880;
        const int lw5[] = { 62, 24, 23, 22, 21, 20, 19, 18 }, pt5[] = { 62, 24, 22, 20, 18 }, gw5[] = { 62, 28, 26, 25, 24, 23, 22 };
        for (int bits = 17; bits <= 24; bits++) {
            const int64_t mid = (int64_t)llround(c * (double)(1LL << bits));
            for (int64_t n = mid - 6; n <= mid + 6; n++)
            for (int lw : lw5) for (int pt : pt5) for (int gw : gw5) {
                Rule r = { 62, 0, 62, 0, 62, 0, 62, lw, pt, gw };
                r.cnum = n; r.cbits = bits;
                rules.push_back(r);
            }
        }
    } else if (getenv("KFIT_CONST")) {
        const int64_t cs[][2] = { { 27145, 16 }, { 27146, 16 }, { 54291, 17 }, { 108583, 18 }, { 108584, 18 }, { 217167, 19 }, { 434334, 20 },
                                  { 434335, 20 }, { 868669, 21 }, { 1737338, 22 }, { 3474675, 23 }, { 6949350, 24 }, { 6949349, 24 },
                                  { 1779033703952, 42 } };     // the last: sqrt(2) - 1 to 42 bits
        const int pw4[] = { 62, 26, 24 }, sw4[] = { 62, 26, 24, 22 }, lw4[] = { 62, 24, 22, 20 }, pt4[] = { 62, 24, 22, 20 }, gw4[] = { 62, 26, 24, 22 };
        for (auto& cc : cs)
        for (int pw : pw4) for (int sw : sw4) for (int lw : lw4) for (int pt : pt4) for (int gw : gw4) {
            Rule r = { 62, 0, pw, 0, sw, 0, 62, lw, pt, gw };
            r.cnum = cc[0]; r.cbits = (int)cc[1];
            rules.push_back(r);
        }
    } else if (getenv("KFIT_COEF")) {
        const int rws[] = { 24, 25, 26, 28, 32 }, cbs[] = { 24, 25, 26, 28, 32 }, pw3[] = { 62, 32, 26, 24 }, sw3[] = { 62, 26, 24, 22 };
        const int lw3[] = { 62, 24, 20 }, pt3[] = { 62, 24, 20 }, gw3[] = { 62, 24, 22 };
        for (int rw : rws) for (int cb : cbs)
        for (int pw : pw3) for (int pm = 0; pm < (pw < 62 ? 3 : 1); pm++)
        for (int sw : sw3) for (int sm = 0; sm < (sw < 62 ? 3 : 1); sm++)
        for (int lw : lw3) for (int pt : pt3) for (int gw : gw3) {
            Rule r = { 62, 0, pw, pm, sw, sm, 62, lw, pt, gw };
            r.rw = rw; r.cb = cb;
            rules.push_back(r);
        }
    } else if (getenv("KFIT_PATHS")) {
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
    if (getenv("KFIT_ONE")) {
        // one rule: "cnum cbits lg lgm gw gm pw pm sw sm dv"
        rules.clear();
        Rule r = model;
        long long cn; int cbt;
        sscanf(getenv("KFIT_ONE"), "%lld %d %d %d %d %d %d %d %d %d %d", &cn, &cbt, &r.lg, &r.lgm, &r.gw, &r.gm, &r.pw, &r.pm, &r.sw, &r.sm, &r.dv);
        r.cnum = cn; r.cbits = cbt;
        if (getenv("KFIT_FORM")) r.form = atoi(getenv("KFIT_FORM"));
        if (getenv("KFIT_REACH")) r.reach = atoi(getenv("KFIT_REACH"));
        if (getenv("KFIT_FMODES")) sscanf(getenv("KFIT_FMODES"), "%d %d %d %d %d %d %d", &r.fs, &r.fq, &r.fd, &r.fk, &r.fsb, &r.fqb, &r.fdb);
        if (getenv("KFIT_RW")) { r.rw = atoi(getenv("KFIT_RW")); r.cb = getenv("KFIT_CB") ? atoi(getenv("KFIT_CB")) : 24;
                                 r.rm = getenv("KFIT_RM") ? atoi(getenv("KFIT_RM")) : 0; r.cbm = getenv("KFIT_CBM") ? atoi(getenv("KFIT_CBM")) : 0; }
        if (getenv("KFIT_PATH")) { r.path = atoi(getenv("KFIT_PATH")); r.iw = getenv("KFIT_IW") ? atoi(getenv("KFIT_IW")) : 62; }
        rules.push_back(r);
    }
    if (getenv("KFIT_FORM") && !getenv("KFIT_ONE"))          // every rule of the grid with the console's length
        for (Rule& r : rules) { r.form = atoi(getenv("KFIT_FORM")); r.cnum = 9827858; r.cbits = 24; }
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
                double kk;
                const int d = K8Of(cells[i], rules[k], &kk) - base[i];
                if (cells[i].page == 99) { r2r[k] = d == -1; continue; }
                if (reachedMode ? (cells[i].obs != 9 && (kk >= cells[i].stepK) == (cells[i].obs == 1)) : d == cells[i].obs) s++;
            }
        score[k] = s - 10000 * v;
    }
    if (getenv("KFIT_LIST") && rules.size() == 1) {      // the mipk cells the one rule misses: page, observed, predicted
        std::map<int, int> perPage;
        for (size_t i = 0; i < nMipk; i++) {
            if (reachedMode) {
                double kk; K8Of(cells[i], rules[0], &kk);
                if (cells[i].page != 99 && cells[i].obs != 9 && (kk >= cells[i].stepK) != (cells[i].obs == 1)) {
                    perPage[cells[i].page]++;
                    if (getenv("KFIT_LIST")[0] >= '2') printf("miss page %d cell %zu dcalc %d bias %d console %s, rule K %.9g step %.9g (%.1f ulp24)\n", cells[i].page, i % 300, (int)cells[i].dcalc, cells[i].bias, cells[i].obs ? "reached" : "short", kk, cells[i].stepK, (kk - cells[i].stepK) / cells[i].stepK * 16777216);
                }
                continue;
            }
            const int d = K8Of(cells[i], rules[0]) - base[i];
            if (d != cells[i].obs && cells[i].obs != 9 && getenv("KFIT_LIST")[0] == '3') {
                double k; K8Of(cells[i], rules[0], &k);
                int e; const double m = frexp(k, &e) * 256, fr = m - floor(m);
                const bool x = diag.lx >= diag.ly;
                auto canc = [&](int j) { const double big = std::max(fabs(diag.p1[j]), fabs(diag.p2[j])); return diag.n[j] != 0 ? log2(big / fabs(diag.n[j])) : 99.0; };
                const int j0 = x ? 0 : 2;
                printf("miss page %d cell %3zu dcalc %d obs %+d pred %+d | K %.9g, %.1f ulp24 %s the step | %s: numerators %.6g (cancel %.1f bits) %.6g (cancel %.1f) ly/lx %.3f\n",
                       cells[i].page, i % 300, (int)cells[i].dcalc, cells[i].obs, d, k, (fr < 0.5 ? fr : 1 - fr) * 65536, fr < 0.5 ? "above" : "below",
                       x ? "x" : "y", fabs(diag.n[j0]), canc(j0), fabs(diag.n[j0 + 1]), canc(j0 + 1), x ? diag.ly / diag.lx : diag.lx / diag.ly);
            }
            if (d != cells[i].obs && cells[i].obs != 9) { perPage[cells[i].page]++; if (getenv("KFIT_LIST")[0] == '2') printf("miss page %d cell %zu obs %+d pred %+d dcalc %d bias %d\n", cells[i].page, i % 300, cells[i].obs, d, (int)cells[i].dcalc, cells[i].bias); }
        }
        for (auto& [pg, n] : perPage) printf("page %d: %d missed\n", pg, n);
        std::map<int, int> perSet;
        int shown = 0;
        for (size_t i = nMipk; i < cells.size(); i++) {
            double kk;
            const int d = K8Of(cells[i], rules[0], &kk) - base[i];
            if (d != 0) {
                perSet[cells[i].page]++;
                if (getenv("KFIT_LIST")[0] == '4' && shown++ < 12) printf("mipmeas set %d triangle %zu: rule %+d, K %.9g, dcalc %d, 1/w %g %g %g\n", cells[i].page - 100, i, d, kk, (int)cells[i].dcalc, cells[i].vz[0], cells[i].vz[1], cells[i].vz[2]);
            }
        }
        for (auto& [st, n] : perSet) printf("mipmeas set %d: %d triangles not the model's\n", st - 100, n);
        { double kk; const size_t i = nMipk - 1; const int d = K8Of(cells[i], rules[0], &kk) - base[i]; printf("r2r: rule %+d (console -1), K %.9g\n", d, kk); }
    }
    std::vector<size_t> order(rules.size());
    for (size_t k = 0; k < order.size(); k++) order[k] = k;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return score[a] > score[b]; });
    printf("%zu rules over %zu cells (model: %d)\n", rules.size(), cells.size(), [&] { int s = 0; for (size_t i = 0; i < cells.size(); i++) s += cells[i].obs == 0; return s; }());
    for (int t = 0; t < 25 && t < (int)order.size(); t++)
        printf("  mipk %4d  mipmeas violations %2d%s  %s  %s\n", score[order[t]] + 10000 * viol[order[t]], viol[order[t]], viol[order[t]] > 20 ? "+" : " ", r2r[order[t]] ? "r2r ok " : "r2r BAD", rules[order[t]].Name().c_str());
}
