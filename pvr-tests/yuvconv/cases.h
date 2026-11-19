/* yuvconv's cases and input bytes: shared by main.c (the console test) and the scorer
   (agents/nulldc3/prep/yuvscore.cpp), so that both know every byte the converter was given. */
#ifndef YUVCONV_CASES_H
#define YUVCONV_CASES_H
#include <stdint.h>

#define YUV_DEST        0x200000u       /* TA_YUV_TEX_BASE: an offset in the 64 bit area (texture memory) */
#define YUV_AREA        0x4000u         /* bytes from YUV_DEST cleared before each run and dumped after it */
#define YUV_REBASE      0x2000u         /* YS_REBASE: the second TA_YUV_TEX_BASE, YUV_DEST + this */
#define YUV_FILL        0xeeeeeeeeu     /* the clear pattern: a byte that is 0xee in all three variants was not written */
#define YUV_VARIANTS    3               /* each case three times, with three maps of the input bytes */

/* YS_REBASE: half of macroblock 0, TA_YUV_TEX_BASE written again (YUV_DEST + YUV_REBASE), then whole macroblocks
   1 .. nmb */
enum { YS_NONE = 0, YS_REBASE = 1 };

typedef struct {
    int form;           /* TA_YUV_TEX_CTRL bit 24, YUV_Form: 0 YUV420 input (384 bytes a macroblock), 1 YUV422 (512) */
    int tex;            /* bit 16, YUV_Tex: 0 one texture of (usize + 1) x (vsize + 1) macroblocks, 1 that many 16x16 */
    int usize, vsize;   /* bits 5-0 and 13-8: macroblocks across and down, less one */
    int nmb;            /* macroblocks sent (more than the texture holds: where the rest goes) */
    int special;
    const char *what;
} yuv_case_t;

static const yuv_case_t yuv_cases[] = {
    { 0, 0, 0, 0,  1, YS_NONE,   "420, one texture 16x16, 1 macroblock" },
    { 0, 0, 1, 1,  4, YS_NONE,   "420, one texture 32x32, 4 macroblocks" },
    { 0, 0, 3, 0,  4, YS_NONE,   "420, one texture 64x16, 4 macroblocks in a row" },
    { 0, 0, 2, 1,  6, YS_NONE,   "420, one texture 48x32 (not a power of two), 6 macroblocks" },
    { 0, 0, 7, 1, 16, YS_NONE,   "420, one texture 128x32, 16 macroblocks" },
    { 1, 0, 0, 0,  1, YS_NONE,   "422, one texture 16x16, 1 macroblock" },
    { 1, 0, 1, 1,  4, YS_NONE,   "422, one texture 32x32, 4 macroblocks" },
    { 0, 1, 1, 1,  4, YS_NONE,   "420, 4 textures of 16x16" },
    { 1, 1, 1, 1,  4, YS_NONE,   "422, 4 textures of 16x16" },
    { 0, 0, 0, 0,  2, YS_NONE,   "420, one texture 16x16, 2 macroblocks: one more than it holds" },
    { 0, 0, 1, 0,  3, YS_NONE,   "420, one texture 32x16, 3 macroblocks: one more than it holds" },
    { 0, 0, 1, 0,  2, YS_REBASE, "420, 32x16: half a macroblock, TA_YUV_TEX_BASE again (+0x2000), then macroblocks 1, 2" },
};
#define YUV_NCASES ((int)(sizeof(yuv_cases) / sizeof(yuv_cases[0])))

static int yuv_mb_bytes(int form) { return form ? 512 : 384; }

/* byte i of macroblock m of case c in variant v: v0 the index's low byte, v1 its high bit and the macroblock
   (0x80 set: never the clear byte), v2 a hash; the pair (v0, v1) names the input byte, v2 checks it */
static uint8_t yuv_byte(int c, int v, int m, int i) {
    uint32_t h;
    switch(v) {
    case 0: return (uint8_t)(i & 0xff);
    case 1: return (uint8_t)(0x80 | ((m & 31) << 1) | ((i >> 8) & 1));
    default:
        h = (uint32_t)(c * 7919 + m * 104729 + i * 2654435761u);
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (uint8_t)h;
    }
}
#endif
