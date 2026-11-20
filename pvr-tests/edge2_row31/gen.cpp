// edge2_row31/gen.cpp: writes cases.h for pvr-tests/edge2_row31 (2026-11-20).
//   g++ -O1 -o gen gen.cpp && ./gen > cases.h
// One polygon per tile on one page: a point (three equal corners) or a tiny triangle (corners 1/1024 pixel
// apart) with its top corner at y = 32 ty + r + f, r = 28 .. 31, f = 0, 1/2, 3/4, drawn with no user tile clip,
// with the user tile clip of its own tile, or with one of the 3x3 tiles around it (cut to the screen).
// Every value is a small dyadic number, exact in a float on any host.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
static uint32_t F(double v) { float f = (float)v; uint32_t u; memcpy(&u, &f, 4); return u; }
int main()
{
    static const double fr[3] = { 0, 0.5, 0.75 };
    static const char* modeName[3] = { "noclip", "clip1", "clip3x3" };
    printf("/* written by pvr-tests/edge2_row31/gen.cpp: one polygon per tile (tile x, tile y, clip mode, x[3], y[3]) */\n");
    printf("typedef struct { unsigned char tx, ty, mode; uint32_t x[3], y[3]; } fc_case_t;\n");
    printf("static const fc_case_t fc_cases[300] = {\n");
    std::vector<std::string> labels;
    for (int t = 0; t < 300; t++) {
        const int tx = t % 20, ty = t / 20, r = 28 + t % 4, mode = (t / 4) % 3, kind = (t / 12) % 2, f = (t / 24) % 3;
        const double X = 32 * tx + 2.25 + (t % 7) * 4, Y = 32 * ty + r + fr[f];
        uint32_t x[3], y[3];
        for (int k = 0; k < 3; k++) { x[k] = F(X); y[k] = F(Y); }
        if (kind == 1) { x[1] = F(X + 1.0 / 1024); y[2] = F(Y + 1.0 / 1024); }
        printf("  { %d, %d, %d, { 0x%08xu, 0x%08xu, 0x%08xu }, { 0x%08xu, 0x%08xu, 0x%08xu } },\n", tx, ty, mode, x[0], x[1], x[2], y[0], y[1], y[2]);
        char buf[160];
        snprintf(buf, sizeof buf, "%s %s row %d +%g x %g", kind ? "tiny" : "point", modeName[mode], r, fr[f], X - 32 * tx);
        labels.push_back(buf);
    }
    printf("};\n#ifndef _arch_dreamcast\nstatic const char *const fc_label[300] = {\n");
    for (auto& l : labels) printf("  \"%s\",\n", l.c_str());
    printf("};\n#endif\n");
}
