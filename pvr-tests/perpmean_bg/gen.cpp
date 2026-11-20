/* The targets of pvr-tests/perpmean_bg (main.c says what the test does): 4 pages of 64, each a depth triple of a
   perpendicular triangle (or, kind 1, a flat 2x2 quad that is not perpendicular). Writes cases.h next to this file.
       g++ -O2 -o gen gen.cpp && ./gen
   Page 0: means whose sum overflows (2^128) or nearly does, both signs. Pages 1, 2: means below zero of size 64 ..
   2^20 (where a negative background is stored as itself: pvr-tests/bgneg), among them triples of perpmean_word
   scaled by a power of two (an exact change of every number in the sum) whose negated mean perpmean_neg read one
   step off the mirror image (NEG_PICK below). Page 3: the same kinds above zero, and flat 2x2 quads both sides. */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "../perpmean_word/cases.h"

static uint64_t S = 0x2545F4914F6CDD1Dull;
static uint32_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return (uint32_t)(S >> 16); }
static int irange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
static uint32_t F(int e, uint32_t m, bool neg = false) { return (neg ? 0x80000000u : 0) | (uint32_t)(e + 127) << 23 | (m & 0x7fffff); }
static uint32_t Rm() { return 0x800000u | (rnd() & 0x7fffff); }
static uint32_t R(int e, bool neg = false) { return F(e, Rm(), neg); }
static long double V(uint32_t b) { float f; memcpy(&f, &b, 4); return (long double)f; }
static uint32_t Shift(uint32_t b, int d)
{
    const uint32_t f = (b >> 23) & 255;
    if (f == 0) return b;
    return (b & 0x807fffffu) | (uint32_t)(f + d) << 23;
}

struct T { int kind; uint32_t z[3]; std::string note; };

// perpmean_word (tile, pixel) whose negated mean perpmean_neg read one step beyond the mirror image (2026-11-19)
static const int NEG_PICK[][2] = {
    {0, 18}, {0, 42}, {0, 63}, {2, 8}, {2, 12}, {2, 18}, {5, 17}, {5, 26}, {5, 66}, {6, 16}, {6, 21}, {6, 32}, {9, 66},
    {9, 67}, {9, 68}, {10, 2}, {10, 16}, {10, 21}, {11, 24}, {11, 34}, {11, 62}, {12, 14}, {12, 93}, {12, 109}, {13, 29},
    {13, 43}, {13, 113}, {15, 19}, {15, 31}, {15, 59},
};

int main()
{
    std::vector<T> P[4];
    const uint32_t P127 = 0x7f000000u, P126 = 0x7e800000u, MAX = 0x7f7fffffu, N = 0x80000000u;
    auto add = [&](int p, int kind, uint32_t a, uint32_t b, uint32_t c, const char* note) { P[p].push_back({kind, {a, b, c}, note}); };
    // ---- page 0: overflow
    add(0, 0, P127, P127, P127, "2^127 x3");
    add(0, 0, 0x7f400000u, 0x7f400000u, 0x7f400000u, "1.5 2^127 x3");
    add(0, 0, MAX, MAX, MAX, "max x3");
    add(0, 0, P127, P127, P126, "2^127, 2^127, 2^126");
    add(0, 0, P126, P126, P127, "2^126, 2^126, 2^127");
    add(0, 0, P126, P127, P126, "2^126, 2^127, 2^126");
    add(0, 0, P127, P127, P127 | N, "2^127, 2^127, -2^127");
    add(0, 0, P127, P127 | N, P127, "2^127, -2^127, 2^127");
    add(0, 0, P127 | N, P127, P127, "-2^127, 2^127, 2^127");
    add(0, 0, P127 | N, P127 | N, P127 | N, "-2^127 x3");
    add(0, 0, MAX | N, MAX | N, MAX | N, "-max x3");
    add(0, 0, P127 | N, P127 | N, P127, "-2^127, -2^127, 2^127");
    add(0, 0, P126, P126, P126, "2^126 x3");
    add(0, 0, 0x7ea00000u, 0x7ea00000u, 0x7ea00000u, "1.25 2^126 x3");
    add(0, 0, 0x7eaaaaaau, 0x7eaaaaaau, 0x7eaaaaaau, "0x7eaaaaaa x3 (3 k just below 2^128)");
    add(0, 0, 0x7eaaaaabu, 0x7eaaaaabu, 0x7eaaaaabu, "0x7eaaaaab x3 (3 k just above 2^128)");
    add(0, 0, 0x7ef33333u, 0x7ef33333u, 0x7ef33333u, "1.9 2^126 x3");
    add(0, 0, MAX, MAX, MAX | N, "max, max, -max");
    add(0, 0, MAX, MAX | N, MAX, "max, -max, max");
    add(0, 0, P127, P127, 0, "2^127, 2^127, 0");
    add(0, 0, 0, P127, P127, "0, 2^127, 2^127");
    add(0, 0, P127, 0, P127, "2^127, 0, 2^127");
    add(0, 0, 0x7eaaaaabu | N, 0x7eaaaaabu | N, 0x7eaaaaabu | N, "-0x7eaaaaab x3");
    add(0, 0, 0x7eaaaaaau | N, 0x7eaaaaaau | N, 0x7eaaaaaau | N, "-0x7eaaaaaa x3");
    add(0, 0, P126 | N, P126 | N, P126 | N, "-2^126 x3");
    add(0, 0, P127, P126 | N, P126 | N, "2^127, -2^126, -2^126");
    add(0, 0, P127 | N, P127 | N, 0x7e000000u, "-2^127, -2^127, 2^125");
    add(0, 0, 0x7f7ffffeu, 0x7f7ffffeu, 0x7f7ffffeu, "max - ulp x3");
    add(0, 0, P127, P127, 0x3f800000u, "2^127, 2^127, 1");
    add(0, 0, 0x3f800000u, P127, P127, "1, 2^127, 2^127");
    add(0, 0, 0x7e7fffffu, 0x7e7fffffu, 0x7f000000u, "2^126 - ulp x2, 2^127");
    add(0, 0, 0x7effffffu, 0x7effffffu, 0x7e800000u, "2^127 - ulp x2, 2^126");
    // flat quads at the same depths (not perpendicular): the plane's own overflow
    add(0, 1, P127, P127, P127, "quad 2^127");
    add(0, 1, MAX, MAX, MAX, "quad max");
    add(0, 1, P127 | N, P127 | N, P127 | N, "quad -2^127");
    add(0, 1, P126, P126, P126, "quad 2^126");
    while (P[0].size() < 64) {                     // random large triples, both signs
        int e = irange(124, 127);
        add(0, 0, R(e, rnd() & 1), R(irange(120, 127), rnd() & 1), R(irange(120, 127), rnd() & 1), "random 2^120 .. 2^127");
    }
    // ---- pages 1, 2: means below zero, 64 .. 2^20 in size
    auto negmean = [](const uint32_t* z) { return (V(z[0]) + V(z[1]) + V(z[2])) / 3; };
    auto fit = [&](uint32_t* z) {                   // scale so that -mean is in [2^7, 2^19]
        long double m = negmean(z);
        if (m == 0) return false;
        int e; frexpl(fabsl(m), &e);
        int d = irange(8, 19) - e;
        for (int i = 0; i < 3; i++) {
            const int f = (z[i] >> 23) & 255;
            if (f && (f + d < 1 || f + d > 254)) return false;
            z[i] = Shift(z[i], d);
        }
        if (negmean(z) > 0) for (int i = 0; i < 3; i++) z[i] ^= 0x80000000u;
        return negmean(z) < -64;
    };
    for (int i = 0; i < 12; i++) { uint32_t z = R(irange(7, 19), true); add(1, 0, z, z, z, "flat, 24 bits, below zero"); }
    for (int i = 0; i < 8; i++) { uint32_t z = R(irange(7, 19), true) & ~0x1fu; add(1, 0, z, z, z, "flat, 19 bits, below zero"); }
    for (int i = 0; i < 16; i++) { int e = irange(7, 18); add(1, 0, R(e, true), R(e, true), R(e, true), "one binade, below zero"); }
    for (int i = 0; i < 16; i++) {
        uint32_t z[3]; int e = irange(9, 14);
        do { for (int k = 0; k < 3; k++) z[k] = R(e + irange(-12, 12), true); } while (!fit(z));
        add(1, 0, z[0], z[1], z[2], "binades within 24, below zero");
    }
    for (int i = 0; i < 12; i++) {                  // mixed signs, the mean below zero
        uint32_t z[3]; int w = i % 3;
        do { for (int k = 0; k < 3; k++) z[k] = R(irange(8, 14), k != w); } while (!fit(z));
        add(1, 0, z[0], z[1], z[2], "one depth above zero, the mean below");
    }
    for (const auto& pk : NEG_PICK) {               // perpmean_word triples, negated and scaled
        const int k = pk[1];
        uint32_t z[3] = { tz[pk[0]][k][0] ^ N, tz[pk[0]][k][1] ^ N, tz[pk[0]][k][2] ^ N };
        if (fit(z))
            add(2, 0, z[0], z[1], z[2], "perpmean_word triple, negated and scaled");
    }
    {
        static const int perm[6][3] = { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0} };
        for (int t = 0; t < 2; t++) {
            uint32_t z[3]; int e = irange(9, 14);
            do { for (int k = 0; k < 3; k++) z[k] = R(e + irange(-12, 12), true); } while (!fit(z));
            for (int p = 0; p < 6; p++) add(2, 0, z[perm[p][0]], z[perm[p][1]], z[perm[p][2]], "a triple below zero in its six orders");
        }
    }
    while (P[2].size() < 56) {                      // cancelling pairs, the mean below zero
        uint32_t z[3]; int e = irange(10, 16);
        uint32_t a = R(e);
        z[0] = a; z[1] = (a - (rnd() % 4096)) | N; z[2] = R(e - irange(2, 14), true);
        if (!fit(z)) continue;
        add(2, 0, z[0], z[1], z[2], "two opposite depths cancel, below zero");
    }
    while (P[2].size() < 64) { uint32_t z = R(irange(7, 19), true); add(2, 0, z, 0, 0x80000000u, "a depth below zero and two zeros"); }
    // ---- page 3: above zero (perpmean_word's rule), flat quads
    for (int i = 0; i < 16; i++) { uint32_t z = R(irange(-1, 19)); add(3, 0, z, z, z, "flat, 24 bits, above zero"); }
    for (int i = 0; i < 16; i++) {
        uint32_t z[3]; int e = irange(9, 14);
        do { for (int k = 0; k < 3; k++) z[k] = R(e + irange(-12, 12), true); } while (!fit(z));
        for (int k = 0; k < 3; k++) z[k] ^= N;
        add(3, 0, z[0], z[1], z[2], "binades within 24, above zero");
    }
    for (int i = 0; i < 16; i++) { uint32_t z = R(irange(7, 19), true); add(3, 1, z, z, z, "flat 2x2 quad, 24 bits, below zero"); }
    for (int i = 0; i < 8; i++) { uint32_t z = R(irange(7, 19), true) & ~0x1fu; add(3, 1, z, z, z, "flat 2x2 quad, 19 bits, below zero"); }
    for (int i = 0; i < 8; i++) { uint32_t z = R(irange(-1, 19)); add(3, 1, z, z, z, "flat 2x2 quad, 24 bits, above zero"); }

    FILE* f = fopen("cases.h", "w");
    fprintf(f, "/* Generated by pvr-tests/perpmean_bg/gen (gen.cpp) - do not edit. Floats are bit patterns. */\n");
    fprintf(f, "#define NTARGET 64\n");
    fprintf(f, "/* per page, per target: kind (0 a perpendicular triangle, 1 a flat 2x2 quad), the corners' depths in the order sent */\n");
    fprintf(f, "static const struct { unsigned char kind; uint32_t z[3]; } targets[4][NTARGET] = {\n");
    for (int p = 0; p < 4; p++) {
        if (P[p].size() != 64) { fprintf(stderr, "page %d: %zu targets\n", p, P[p].size()); return 1; }
        fprintf(f, "  {\n");
        for (auto& t : P[p]) fprintf(f, "    { %d, { 0x%08xu, 0x%08xu, 0x%08xu } },   /* %s */\n", t.kind, t.z[0], t.z[1], t.z[2], t.note.c_str());
        fprintf(f, "  },\n");
    }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
