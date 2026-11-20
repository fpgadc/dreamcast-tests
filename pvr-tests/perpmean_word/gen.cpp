/* The corner depths of pvr-tests/perpmean_word (main.c says what the test does): 16 tiles of 1024 triangles, one per
   pixel, each a family of depth triples. Writes cases.h next to this file.
       g++ -O2 -o gen gen.cpp && ./gen [seed]
   Floats are bit patterns; every triple's exact mean is positive and within 2^-90 .. 2^90 (zexfil.h reads depths of
   2^-100 .. 2^100 exactly, and none below zero). */
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

static uint64_t S = 0x9e3779b97f4a7c15ull;
static uint32_t rnd() { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return (uint32_t)(S >> 16); }
static int irange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

// value m 2^(e - 23), m in [2^23, 2^24)
static uint32_t F(int e, uint32_t m, bool neg = false) { return (neg ? 0x80000000u : 0) | (uint32_t)(e + 127) << 23 | (m & 0x7fffff); }
static uint32_t Rm() { return 0x800000u | (rnd() & 0x7fffff); }
static uint32_t R(int e, bool neg = false) { return F(e, Rm(), neg); }
static long double V(uint32_t b) { float f; memcpy(&f, &b, 4); return (long double)f; }

struct Tile { int tx, ty, mirror; const char* name; std::vector<uint32_t> z; };     // z: 3 per pixel

static bool ok(uint32_t a, uint32_t b, uint32_t c)
{
    // the exact sum: long double has 64 bits, enough for three floats within 40 binades; beyond that the sign is
    // that of the largest anyway
    long double s = V(a) + V(b) + V(c);
    return s > 0 && s / 3 > ldexpl(1, -90) && s / 3 < ldexpl(1, 90);
}

int main(int argc, char** argv)
{
    if (argc > 1) S ^= strtoull(argv[1], 0, 0) * 0x2545F4914F6CDD1Dull;
    std::vector<Tile> T;
    auto add = [&](int tx, int ty, int mirror, const char* name, auto fn) {
        Tile t{tx, ty, mirror, name, {}};
        for (int k = 0; k < 1024; k++) {
            uint32_t z[3];
            do fn(k, z); while (!ok(z[0], z[1], z[2]));
            t.z.insert(t.z.end(), z, z + 3);
        }
        T.push_back(t);
    };
    // 0: flat, 24 bit depths, any exponent of -20 .. 20
    add(3, 2, 0, "flat, 24 bit depths", [](int, uint32_t* z) { z[0] = z[1] = z[2] = R(irange(-20, 20)); });
    // 1: flat, 19 bit depths (and a few chosen ones: 1/3 and 2/3 of powers of two, the binade's ends)
    add(5, 2, 0, "flat, 19 bit and chosen depths", [](int k, uint32_t* z) {
        static const uint32_t m[] = { 0x800000, 0x800001, 0xffffff, 0xfffffe, 0xaaaaaa, 0xaaaaab, 0xaaaaa9, 0xaaaaac,
                                      0xaaaaa0, 0xaaaac0, 0xc00000, 0xc00001, 0xbfffff, 0x800020, 0xffffe0, 0x555555 };
        if (k < 256) z[0] = F(k / 16 - 8, m[k % 16]);
        else z[0] = F(irange(-20, 20), Rm() & ~0x1fu);
        z[1] = z[2] = z[0];
    });
    // 2: three depths in one binade
    add(7, 2, 0, "one binade", [](int, uint32_t* z) { int e = irange(-6, 6); for (int i = 0; i < 3; i++) z[i] = R(e); });
    // 3: the same, the triangles mirrored (the other winding)
    add(9, 2, 1, "one binade, mirrored", [](int, uint32_t* z) { int e = irange(-6, 6); for (int i = 0; i < 3; i++) z[i] = R(e); });
    // 4: exponents within 4 of each other
    add(3, 4, 0, "binades within 4", [](int, uint32_t* z) { int e = irange(-6, 6); for (int i = 0; i < 3; i++) z[i] = R(e + irange(-2, 2)); });
    // 5: exponents within 24: the adder's alignment
    add(5, 4, 0, "binades within 24", [](int, uint32_t* z) { int e = irange(-4, 4); for (int i = 0; i < 3; i++) z[i] = R(e + irange(-12, 12)); });
    // 6: one depth far below the others: 16 .. 40 binades
    add(7, 4, 0, "one depth 16 .. 40 binades below", [](int k, uint32_t* z) {
        int e = irange(-4, 4), d = irange(16, 40), w = k % 3;
        for (int i = 0; i < 3; i++) z[i] = R(i == w ? e - d : e + irange(-1, 1));
    });
    // 7: two depths far below: 16 .. 40 binades
    add(9, 4, 0, "two depths 16 .. 40 binades below", [](int k, uint32_t* z) {
        int e = irange(-4, 4), w = k % 3;
        for (int i = 0; i < 3; i++) z[i] = R(i == w ? e : e - irange(16, 40));
    });
    // 8, 9: a triple in its six orders, one binade / binades within 24
    static const int perm[6][3] = { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0} };
    {
        uint32_t base[3];
        add(3, 6, 0, "one binade, a triple in its six orders", [&](int k, uint32_t* z) {
            if (k % 6 == 0 || !ok(base[0], base[1], base[2])) { int e = irange(-6, 6); for (int i = 0; i < 3; i++) base[i] = R(e); }
            for (int i = 0; i < 3; i++) z[i] = base[perm[k % 6][i]];
        });
        add(5, 6, 0, "binades within 24, a triple in its six orders", [&](int k, uint32_t* z) {
            if (k % 6 == 0 || !ok(base[0], base[1], base[2])) { int e = irange(-4, 4); for (int i = 0; i < 3; i++) base[i] = R(e + irange(-12, 12)); }
            for (int i = 0; i < 3; i++) z[i] = base[perm[k % 6][i]];
        });
    }
    // 10: one depth below zero, the mean above
    add(7, 6, 0, "one depth below zero", [](int k, uint32_t* z) {
        int e = irange(-4, 4), w = k % 3;
        for (int i = 0; i < 3; i++) z[i] = R(e + irange(-3, 3), i == w);
    });
    // 11: cancellation: two depths of opposite sign almost equal, a third small
    add(9, 6, 0, "two opposite depths cancel", [](int k, uint32_t* z) {
        int e = irange(-2, 6), w = k % 3, u = (w + 1 + (int)(rnd() & 1)) % 3, v = 3 - w - u;
        uint32_t a = R(e), m = (a & 0x7fffff) | 0x800000;
        int d = irange(0, 24);
        uint32_t m2 = m - (rnd() % (1u << (d < 23 ? d : 23)));          // -b a little below a
        if (m2 < 0x800000) m2 = 0x800000;
        z[u] = a; z[v] = F(e, m2, true);
        z[w] = R(e - irange(0, 30));
    });
    // 12: sums that carry: the first two near a power of two
    add(3, 8, 0, "pair sums near a power of two", [](int k, uint32_t* z) {
        int e = irange(-4, 4);
        uint32_t a = R(e);
        long double target = ldexpl(1, e + 1) + (long double)((int)(rnd() % 64) - 32) * ldexpl(1, e - 23 - irange(0, 3));
        long double b = target - V(a);
        if (b <= 0) b = V(a);
        float bf = (float)b; uint32_t bb; memcpy(&bb, &bf, 4);
        int o = k % 3;
        uint32_t c = R(e - irange(-2, 26));
        uint32_t t[3] = { a, bb, c };
        for (int i = 0; i < 3; i++) z[i] = t[(i + o) % 3];
    });
    // 13: zeros among the corners
    add(5, 8, 0, "zeros among the corners", [](int k, uint32_t* z) {
        int e = irange(-6, 6);
        z[0] = R(e); z[1] = (k & 8) ? R(e + irange(-12, 12)) : 0; z[2] = 0;
        if (k & 16) z[1] = z[1] ? z[1] : 0x80000000u;
        uint32_t t[3] = { z[0], z[1], z[2] };
        for (int i = 0; i < 3; i++) z[i] = t[perm[k % 6][i]];
    });
    // 14, 15: general: binades within 8, and within 40
    add(7, 8, 0, "binades within 8", [](int, uint32_t* z) { int e = irange(-8, 8); for (int i = 0; i < 3; i++) z[i] = R(e + irange(-4, 4)); });
    add(9, 8, 0, "binades within 40", [](int, uint32_t* z) { int e = irange(-8, 8); for (int i = 0; i < 3; i++) z[i] = R(e + irange(-20, 20)); });

    FILE* f = fopen((std::string(argc > 2 ? argv[2] : ".") + "/cases.h").c_str(), "w");
    fprintf(f, "/* Generated by pvr-tests/perpmean_word/gen (gen.cpp) - do not edit. Floats are bit patterns. */\n");
    fprintf(f, "#define NT %zu\n", T.size());
    fprintf(f, "static const struct { unsigned char tx, ty, mirror; const char *name; } tiles[NT] = {\n");
    for (auto& t : T) fprintf(f, "    { %d, %d, %d, \"%s\" },\n", t.tx, t.ty, t.mirror, t.name);
    fprintf(f, "};\n/* per tile, per pixel (row by row): the depths of the corners in the order sent */\n");
    fprintf(f, "static const uint32_t tz[NT][1024][3] = {\n");
    for (auto& t : T) {
        fprintf(f, "  {\n");
        for (int k = 0; k < 1024; k++)
            fprintf(f, "    { 0x%08xu, 0x%08xu, 0x%08xu },\n", t.z[3 * k], t.z[3 * k + 1], t.z[3 * k + 2]);
        fprintf(f, "  },\n");
    }
    fprintf(f, "};\n");
    fclose(f);
    return 0;
}
