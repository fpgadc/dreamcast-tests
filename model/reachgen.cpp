// reachgen: triangles that tell how far below the other an operand of the TSP setup's adders still counts
// (pvrtsp::PlaneConfig::reach), for pvr-tests/uvmeas (gen.py --mix list).
//
//   reachgen [--lo R] [--hi R] [--per N] [--seed S] [--tries N] > tris.json
//
// Every candidate is a triangle in a 32x32 cell, read the way uvmeas reads it (the 1024 wide texture, texel
// floor(U * 1024) mod 1024, pixels at least 1.5 inside, runs from the triangle's first pixel on the line). The
// model computes its readings with every reach from lo to hi; a candidate whose readings differ between reach R
// and R + 1 tells that boundary, and N candidates are kept for each boundary lo .. hi - 1. A console's readings
// then leave one reach (or a stretch of them, if some boundary has no candidate). Candidates: two corners nearly
// on one line (a few units of the last place apart in y, or in x), the third elsewhere in the cell, the
// attribute at the near pair large and at the third a power of two smaller (so that the setup's sums add a small
// operand to two large ones that cancel: polly2-data gundam_ingame's ground), with 1/w 1 or random; and random
// triangles with attributes of very different sizes.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <random>
#include <string>
#include <vector>
#include "pvr_isp_edge.h"
#include "pvr_tsp_plane.h"
#include "f32bits.h"

using pvrfp::Fp;

struct Tri { float x[3], y[3], z[3], u[3]; };

static int texel(double u)
{
    double uf = (double)(float)u;
    if (uf == 0 && u < 0)
        uf = -1e-30;
    double f = floor(uf * 1024.0 * 256.0);
    if (f >= 1073741824.0)
        f = 1073741824.0 + fmod(f, 67108864.0);
    else if (f < -1073741824.0)
        f = -1073741824.0 + fmod(fmod(f, 67108864.0) + 67108864.0, 67108864.0);
    if (!(f == f))
        f = 0;
    return (((int)f) >> 8) & 1023;
}

// the readings of a triangle in its cell (as pvr-tests/uvmeas/read.py picks them and model/tspprobe computes them)
static void readings(const Tri& tr, int reach, std::vector<int>& out)
{
    static const pvrisp::EdgeConfig ecfg;
    out.clear();
    pvrtsp::PlaneConfig cfg;
    cfg.reach = reach;
    pvrtsp::Plane w, u;
    w.Setup(F32s(tr.x), F32s(tr.y), F32s(tr.z), nullptr, cfg);
    u.Setup(F32s(tr.x), F32s(tr.y), F32s(tr.z), F32s(tr.u), cfg);
    if (w.bad || u.bad)
        return;
    Fp fx[3], fy[3];
    for (int k = 0; k < 3; k++) { fx[k] = Fp::FromFloat(tr.x[k]); fy[k] = Fp::FromFloat(tr.y[k]); }
    bool bad;
    int sgn = pvrisp::Winding(fx, fy, ecfg, &bad);
    if (bad)
        return;
    const double area = ((double)tr.x[1] - tr.x[0]) * ((double)tr.y[2] - tr.y[0]) - ((double)tr.x[2] - tr.x[0]) * ((double)tr.y[1] - tr.y[0]);
    const int tx = (int)floor(std::min(tr.x[0], std::min(tr.x[1], tr.x[2]))) / 32, ty = (int)floor(std::min(tr.y[0], std::min(tr.y[1], tr.y[2]))) / 32;
    pvrisp::EdgeTile e[3];
    for (int k = 0; k < 3; k++)
        e[k].Setup(fx[k], fy[k], fx[(k + 1) % 3], fy[(k + 1) % 3], 64 * tx + 1, 64 * ty + 1, sgn, ecfg);
    for (int j = 0; j < 32; j++) {
        int start = -1;
        for (int i = 0; i < 32; i++) {
            if (!(e[0].Inside(i, j) && e[1].Inside(i, j) && e[2].Inside(i, j)))
                continue;
            if (start < 0)
                start = i;
            // at least 1.5 pixels inside every edge (read.py's MARGIN)
            const double px = tx * 32 + i + 0.5, py = ty * 32 + j + 0.5;
            bool deep = true;
            for (int k = 0; k < 3; k++) {
                const int l = (k + 1) % 3;
                const double ex = (double)tr.x[l] - tr.x[k], ey = (double)tr.y[l] - tr.y[k];
                const double d = (ex * (py - tr.y[k]) - ey * (px - tr.x[k])) / hypot(ex, ey) * (area < 0 ? -1 : 1);
                if (!(d > 1.5)) deep = false;
            }
            if (!deep)
                continue;
            const int64_t x0h = 2 * (int64_t)(tx * 32 + start) + 1, yh = 2 * (int64_t)(ty * 32 + j) + 1;
            Fp wv = w.Value(x0h, yh, i - start, tx * 32, ty * 32), uv = u.Value(x0h, yh, i - start, tx * 32, ty * 32);
            out.push_back(texel(pvrtsp::Divide(uv, wv).toDouble()));
        }
    }
}

static std::string hexf(float f)
{
    char b[64];
    snprintf(b, sizeof(b), "%a", (double)f);
    return b;
}

int main(int argc, char** argv)
{
    int lo = 16, hi = 40, per = 12, tries = 2000000;
    unsigned seed = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--lo")) lo = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hi")) hi = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--per")) per = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed")) seed = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tries")) tries = atoi(argv[++i]);
    }
    std::mt19937_64 rng(seed);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    auto irnd = [&](int a, int b) { return (int)std::uniform_int_distribution<int>(a, b)(rng); };
    auto mant = [&](int bits) { return (double)irnd(1 << (bits - 1), (1 << bits) - 1) / (double)(1 << (bits - 1)); };
    const int nb = hi - lo;
    std::vector<int> have(nb, 0);
    std::vector<std::pair<Tri, int>> kept;         // triangle, the boundary it tells (R: R | R + 1)
    std::vector<int> keptPage;
    // the free cells of the 8 pages: a triangle is made in a free cell, and keeps it if it is kept
    std::vector<int> freeCells;
    for (int c = 0; c < 8 * 300; c++) freeCells.push_back(c);
    std::shuffle(freeCells.begin(), freeCells.end(), rng);
    std::vector<std::vector<int>> rd(hi - lo + 1);
    for (int t = 0; t < tries; t++) {
        int need = 0;
        for (int b = 0; b < nb; b++) need += have[b] < per;
        if (!need) break;
        // a free cell (anywhere on the screen: the size of the coordinates matters)
        if (freeCells.empty()) break;
        const int slot = freeCells.back(), cell = slot % 300;
        const double cx = (cell % 20) * 32.0, cy = (cell / 20) * 32.0;
        Tri tr;
        const int kind = irnd(0, 3);
        if (kind < 3) {
            // two corners nearly on a line: along x (kind 0, 2) or along y (kind 1)
            const bool alongX = kind != 1;
            const double a0 = uni(2, 30), b0 = uni(2, 12), a2 = uni(2, 30), c1 = uni(20, 30), d1 = uni(2, 30);
            float p0x = (float)(cx + (alongX ? a0 : b0)), p0y = (float)(cy + (alongX ? b0 : a0));
            float p1x = (float)(cx + (alongX ? d1 : c1)), p1y = (float)(cy + (alongX ? c1 : d1));
            float p2x = (float)(cx + (alongX ? a2 : b0)), p2y = (float)(cy + (alongX ? b0 : a2));
            // a few units of the last place apart
            const int k = irnd(-4, 4);
            if (alongX) p2y = nextafterf(p2y, k < 0 ? -1e30f : 1e30f), p2y = (k == 0) ? p0y : p2y;
            else p2x = nextafterf(p2x, k < 0 ? -1e30f : 1e30f), p2x = (k == 0) ? p0x : p2x;
            for (int q = 1; q < std::abs(k); q++) {
                if (alongX) p2y = nextafterf(p2y, k < 0 ? -1e30f : 1e30f);
                else p2x = nextafterf(p2x, k < 0 ? -1e30f : 1e30f);
            }
            if (fabs(((double)p1x - p0x) * ((double)p2y - p0y) - ((double)p2x - p0x) * ((double)p1y - p0y)) < 100)
                continue;
            const float X[3] = { p0x, p1x, p2x }, Y[3] = { p0y, p1y, p2y };
            // the order of the corners: any of the six
            int perm[3] = { 0, 1, 2 };
            std::shuffle(perm, perm + 3, rng);
            const int m = irnd(0, 12), s = irnd(0, 40);
            const double big = (irnd(0, 1) ? -1 : 1) * mant(irnd(8, 24)) * ldexp(1.0, m);
            const double small = (irnd(0, 1) ? -1 : 1) * mant(irnd(1, 24)) * ldexp(1.0, m - s);
            const double U[3] = { big, small, kind == 2 ? big * (1 + ldexp(uni(-1, 1), -irnd(10, 24))) : big };
            const double z13 = kind == 2 ? mant(irnd(1, 24)) * ldexp(1.0, -irnd(0, 6)) : 1.0;
            const double Z[3] = { z13, irnd(0, 1) ? z13 : mant(irnd(1, 24)) * ldexp(1.0, -irnd(0, 6)), z13 };
            for (int q = 0; q < 3; q++) { tr.x[q] = X[perm[q]]; tr.y[q] = Y[perm[q]]; tr.u[q] = (float)U[perm[q]]; tr.z[q] = (float)Z[perm[q]]; }
        } else {
            // any triangle, attributes of very different sizes
            for (int q = 0; q < 3; q++) {
                tr.x[q] = (float)(cx + uni(1, 31));
                tr.y[q] = (float)(cy + uni(1, 31));
                tr.u[q] = (float)((irnd(0, 1) ? -1 : 1) * mant(irnd(1, 24)) * ldexp(1.0, irnd(-30, 12)));
                tr.z[q] = (float)(mant(irnd(1, 24)) * ldexp(1.0, -irnd(0, 6)));
            }
            if (fabs(((double)tr.x[1] - tr.x[0]) * ((double)tr.y[2] - tr.y[0]) - ((double)tr.x[2] - tr.x[0]) * ((double)tr.y[1] - tr.y[0])) < 100)
                continue;
        }
        for (int r = lo; r <= hi; r++)
            readings(tr, r, rd[r - lo]);
        if (rd[0].size() < 8)
            continue;
        for (int b = 0; b < nb; b++)
            if (have[b] < per && rd[b] != rd[b + 1]) {
                have[b]++;
                kept.push_back({ tr, lo + b });
                keptPage.push_back(slot / 300);
                freeCells.pop_back();
                break;
            }
    }
    for (int b = 0; b < nb; b++)
        fprintf(stderr, "reach %d | %d: %d triangles\n", lo + b, lo + b + 1, have[b]);
    printf("[\n");
    for (size_t i = 0; i < kept.size(); i++) {
        const Tri& t = kept[i].first;
        printf("  {\"name\": \"reach %d|%d\", \"page\": %d, \"x\": [\"%s\", \"%s\", \"%s\"], \"y\": [\"%s\", \"%s\", \"%s\"], "
               "\"z\": [\"%s\", \"%s\", \"%s\"], \"a\": [\"%s\", \"%s\", \"%s\"]}%s\n",
               kept[i].second, kept[i].second + 1, keptPage[i],
               hexf(t.x[0]).c_str(), hexf(t.x[1]).c_str(), hexf(t.x[2]).c_str(),
               hexf(t.y[0]).c_str(), hexf(t.y[1]).c_str(), hexf(t.y[2]).c_str(),
               hexf(t.z[0]).c_str(), hexf(t.z[1]).c_str(), hexf(t.z[2]).c_str(),
               hexf(t.u[0]).c_str(), hexf(t.u[1]).c_str(), hexf(t.u[2]).c_str(), i + 1 < kept.size() ? "," : "");
    }
    printf("]\n");
}
