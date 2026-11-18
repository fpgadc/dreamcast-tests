// lenfit: the console's length of K (pvrtsp::MipSelect) from the cells of mipk3 and mipk4 whose numerators are exact
// (pvr-tests/mipk/lencells.py > cells.txt: set row hi lo_num lo_exp bias tk reached, in units of the longer numerator's
// 24 bit grid; reached iff K = L * bias / 4 reaches 130 * 2^tk). Rules of the form
//     L = (hi + lo) - k2 * lo
// with a cut on the sum, on the product, on the difference and on K (none, n bits toward zero or to nearest, or a grid),
// k2 between 1 - 27146 / 65536 and 2 - sqrt(2) at 2^16 .. 2^26; the best first. FIX=1: every cut 24 bits toward zero
// (the product also 23 nearest, 25 toward zero) and k2 at 2^-28: the constants that give every cell.
//   g++ -O2 -march=native -std=c++17 -fopenmp pvr-tests/mipk/lenfit.cpp -o pvr-tests/mipk/lenfit
//   python3 pvr-tests/mipk/lencells.py > cells.txt; FIX=1 pvr-tests/mipk/lenfit cells.txt       (the whole grid: ~4 min)
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
typedef __int128 i128;
static const int FB = 64;
struct Cell { int set, row; i128 hi, lo, T; int bias; bool reached; };
static int TopBit(i128 x) { int n = -1; while (x > 0) { x >>= 1; n++; } return n; }
static i128 GridAbs(i128 x, int s, int mode) {          // to a grid of 2^s (in scaled units); mode 0 floor, 1 nearest
    if (s <= 0) return x;
    const i128 one = (i128)1 << s;
    if (mode == 1) x += one >> 1;
    return x & ~(one - 1);
}
static i128 Sig(i128 x, int n, int mode) { const int t = TopBit(x); return t < 0 ? x : GridAbs(x, t - n + 1, mode); }
struct Cut { int type, p, mode; };   // 0 exact, 1 p significant bits, 2 grid 2^p units
static i128 Apply(i128 x, const Cut& c) {
    if (c.type == 1) return Sig(x, c.p, c.mode);
    if (c.type == 2) return GridAbs(x, FB + c.p, c.mode);
    return x;
}
static std::string Name(const Cut& c) {
    char b[48];
    if (c.type == 0) return "exact";
    snprintf(b, sizeof b, "%s%d%s", c.type == 1 ? "sig" : "grid2^", c.p, c.mode ? "n" : "f");
    return b;
}
int main(int argc, char** argv)
{
    std::vector<Cell> cells;
    FILE* f = fopen(argv[1], "r");
    int set, row, bias, tk, reached; long long hi, ln; int le;
    while (fscanf(f, "%d %d %lld %lld %d %d %d %d", &set, &row, &hi, &ln, &le, &bias, &tk, &reached) == 8) {
        Cell c; c.set = set; c.row = row; c.hi = (i128)hi << FB; c.lo = (i128)ln << (FB + le); c.bias = bias;
        c.T = ((i128)130 << tk) << FB; c.reached = reached;
        cells.push_back(c);
    }
    fclose(f);
    const char* only = getenv("ONLY");                  // "3" or "4": one set
    if (only) cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const Cell& c) { return c.set != atoi(only); }), cells.end());
    printf("%zu cells\n", cells.size());
    std::vector<Cut> sumCuts = { { 0, 0, 0 } }, qCuts = { { 0, 0, 0 } }, dCuts = { { 0, 0, 0 } }, kCuts = { { 0, 0, 0 } };
    if (getenv("FIX")) {          // every cut 24 bits toward zero
        sumCuts = { { 1, 24, 0 } }; qCuts = { { 1, 24, 0 }, { 1, 23, 1 }, { 1, 25, 0 } }; dCuts = { { 1, 24, 0 } };
    } else {
    for (int n = 22; n <= 26; n++) for (int m = 0; m < 2; m++) sumCuts.push_back({ 1, n, m });
    for (int n = 16; n <= 26; n++) for (int m = 0; m < 2; m++) qCuts.push_back({ 1, n, m });
    for (int g = -3; g <= 1; g++) for (int m = 0; m < 2; m++) qCuts.push_back({ 2, g, m });
    for (int n = 22; n <= 26; n++) for (int m = 0; m < 2; m++) dCuts.push_back({ 1, n, m });
    for (int n = 23; n <= 25; n++) for (int m = 0; m < 2; m++) kCuts.push_back({ 1, n, m });
    }
    struct Cst { int64_t C; int n; };
    std::vector<Cst> csts;
    const std::vector<int> ns = getenv("FIX") ? std::vector<int>{ 28 } : std::vector<int>{ 16, 18, 20, 21, 22, 23, 24, 25, 26 };
    for (int n : ns) {
        const int64_t c0 = (int64_t)floor((38390.0 / 65536) * ldexp(1, n)) - 3, c1 = (int64_t)ceil((2 - sqrt(2.0)) * ldexp(1, n)) + 3;
        for (int64_t C = c0; C <= c1; C++) if (n == 16 || n == 28 || (C & 1)) csts.push_back({ C, n });
    }
    const long nS = sumCuts.size(), nC = csts.size(), nQ = qCuts.size(), nD = dCuts.size(), nK = kCuts.size();
    const long nRules = nS * nC * nQ * nD * nK;
    printf("%ld rules (%ld constants)\n", nRules, nC);
    std::vector<int> score(nRules), rowsOk(nRules);
    auto decode = [&](long x, const Cut*& sc, const Cst*& cs, const Cut*& qc, const Cut*& dc, const Cut*& kc) {
        kc = &kCuts[x % nK]; x /= nK; dc = &dCuts[x % nD]; x /= nD; qc = &qCuts[x % nQ]; x /= nQ; cs = &csts[x % nC]; x /= nC; sc = &sumCuts[x];
    };
#pragma omp parallel for schedule(dynamic, 1024)
    for (long ri = 0; ri < nRules; ri++) {
        const Cut *sc, *qc, *dc, *kc; const Cst* cs;
        decode(ri, sc, cs, qc, dc, kc);
        int ok = 0;
        for (const Cell& c : cells) {
            const i128 s = Apply(c.hi + c.lo, *sc);
            const i128 q = Apply((c.lo >> cs->n) * cs->C + (((c.lo & (((i128)1 << cs->n) - 1)) * cs->C) >> cs->n), *qc);
            const i128 L = Apply(s - q, *dc);
            const i128 K = Apply(c.bias == 4 ? L : (L * c.bias) >> 2, *kc);
            ok += (K >= c.T) == c.reached;
        }
        score[ri] = ok;
    }
    std::vector<long> idx(nRules);
    for (long i = 0; i < nRules; i++) idx[i] = i;
    if (getenv("FIX")) {            // the constants that explain every cell, per product cut
        for (long i = 0; i < nRules; i++) if (score[i] == (int)cells.size()) {
            const Cut *sc, *qc, *dc, *kc; const Cst* cs; decode(i, sc, cs, qc, dc, kc);
            printf("all | k2 %lld/2^%d = 0x%llx/2^28 (%.10f), product %s\n", (long long)cs->C, cs->n, (long long)cs->C, cs->C / ldexp(1, cs->n), Name(*qc).c_str());
        }
        return 0;
    }
    const int show = 30;
    std::partial_sort(idx.begin(), idx.begin() + show, idx.end(), [&](long a, long b) { return score[a] > score[b]; });
    for (int k = 0; k < show; k++) {
        const Cut *sc, *qc, *dc, *kc; const Cst* cs;
        decode(idx[k], sc, cs, qc, dc, kc);
        printf("%d/%zu | sum %s, k2 %lld/2^%d (%.9f), product %s, difference %s, K %s\n", score[idx[k]], cells.size(), Name(*sc).c_str(),
               (long long)cs->C, cs->n, cs->C / ldexp(1, cs->n), Name(*qc).c_str(), Name(*dc).c_str(), Name(*kc).c_str());
    }
    return 0;
}
