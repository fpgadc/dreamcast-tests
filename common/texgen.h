/* Texture memory for every way a texture can be stored, from one function that gives the texels.

   A test says what the texels are (tex_fn: the 16 bit word, or the palette index, of texel (x, y) of the level
   that is `size` texels wide) and how they are stored (tex_layout_t); tex_build lays them out and uploads
   them. What the polygon's TCW then says about the texture is the test's business: normally the same, but a
   test may set bits that contradict the layout to see what the hardware reads.

     TEX_TWIDDLED   one level, twiddled (not square: one twiddled square after the other)
     TEX_RASTER     one level, row by row; `stride` texels per row in memory (0: the width)
     TEX_MIP        square, all levels from 1x1 up, each twiddled, at the offsets of a mipmapped texture
     TEX_VQ         a code book of 256 entries of 64 bits, then one byte for every 64 bits of texels, twiddled
     TEX_VQ_MIP     the code book, then the levels' bytes at the offsets of a mipmapped VQ texture
   bits: 16 (565, 1555, 4444, YUV422, bump map), 8 or 4 (palette indices).

   VQ: an entry is 64 bits of texels in twiddled order, so 2x2 texels of 16 bits, 8 texels of 8 bits (2 wide,
   4 tall) or 4x4 texels of 4 bits. The texel at place o of the texture (its level's offset plus its twiddled
   index) is element o % n of the entry whose number is byte o / n behind the code book, n the texels in an
   entry. (For 16 bit texels that is the 2x2 blocks everyone knows; for palette textures it is what a console
   reads, pvr-tests/mip page 17 and the palette VQ variants of sample.) */
#ifndef TEXGEN_H
#define TEXGEN_H

enum { TEX_TWIDDLED, TEX_RASTER, TEX_MIP, TEX_VQ, TEX_VQ_MIP };
typedef struct { int layout, bits, w, h, stride; } tex_layout_t;
typedef uint32_t (*tex_fn)(const void *ctx, int size, int x, int y);

/* where the level that is 1 << k texels wide starts in a mipmapped texture, in texels (a VQ texture's bytes: / 4) */
static const uint32_t tex_mip_point[11] = { 0x3, 0x4, 0x8, 0x18, 0x58, 0x158, 0x558, 0x1558, 0x5558, 0x15558, 0x55558 };

static uint8_t tex_mem[1 << 19];
static int tex_len;                         /* bytes of tex_mem in use */
static uint8_t tex_code[256][8];
static int tex_codes, tex_code_overflow;    /* overflow: a texture had more than 256 different entries */
#define TEX_VQ_PLACES 4096
static uint16_t tex_vq_el[TEX_VQ_PLACES][16];

static void tex_put(int bits, uint32_t index, uint32_t value) {
    uint32_t end;
    if(bits == 16) {
        tex_mem[2 * index] = (uint8_t)value;
        tex_mem[2 * index + 1] = (uint8_t)(value >> 8);
        end = 2 * index + 2;
    } else if(bits == 8) {
        tex_mem[index] = (uint8_t)value;
        end = index + 1;
    } else {                                /* texel 0 is the low nibble of byte 0 */
        uint8_t *b = &tex_mem[index >> 1];
        *b = (index & 1) ? (uint8_t)((*b & 0x0f) | ((value & 15) << 4)) : (uint8_t)((*b & 0xf0) | (value & 15));
        end = (index >> 1) + 1;
    }
    if((int)end > tex_len)
        tex_len = (int)end;
}

/* the place of texel (x, y) in a twiddled level of w x h texels */
static uint32_t tex_tw(int x, int y, int w, int h) {
    const int min = w < h ? w : h, mask = min - 1;
    return tw_index((uint32_t)(x & mask), (uint32_t)(y & mask)) + (uint32_t)(x / min + y / min) * (uint32_t)(min * min);
}

static int tex_vq_code(const uint8_t b[8]) {
    int i;
    for(i = 0; i < tex_codes; i++)
        if(!memcmp(tex_code[i], b, sizeof(tex_code[i])))
            return i;
    if(tex_codes == 256) {
        tex_code_overflow = 1;
        return 255;
    }
    memcpy(tex_code[tex_codes], b, sizeof(tex_code[0]));
    return tex_codes++;
}

/* builds the texture and uploads it; the address in the 64 bit area (TCW.TexAddr is this / 8) */
static uint32_t tex_build(const tex_layout_t *l, tex_fn fn, const void *ctx) {
    const int mip = l->layout == TEX_MIP || l->layout == TEX_VQ_MIP;
    int x, y, s, k, i;
    uint32_t at;
    (void)i;

    memset(tex_mem, 0, (size_t)tex_len);
    tex_len = 0;
    if(l->layout == TEX_TWIDDLED || l->layout == TEX_RASTER) {
        const int row = l->stride ? l->stride : l->w;
        for(y = 0; y < l->h; y++)
            for(x = 0; x < l->w; x++)
                tex_put(l->bits, l->layout == TEX_RASTER ? (uint32_t)(y * row + x) : tex_tw(x, y, l->w, l->h), fn(ctx, l->w, x, y));
    } else if(l->layout == TEX_MIP) {
        for(s = 1, k = 0; s <= l->w; s *= 2, k++)
            for(y = 0; y < s; y++)
                for(x = 0; x < s; x++)
                    tex_put(l->bits, tex_mip_point[k] + tw_index((uint32_t)x, (uint32_t)y), fn(ctx, s, x, y));
    } else {
        const int n = 64 / l->bits;         /* texels in an entry */
        int places = 0, e;
        uint32_t o;
        tex_codes = 0;
        memset(tex_vq_el, 0, sizeof(tex_vq_el));
        for(s = mip ? 1 : l->w, k = 0; s <= l->w; s *= 2, k++) {
            const int lh = mip ? s : l->h;
            for(y = 0; y < lh; y++)
                for(x = 0; x < s; x++) {
                    o = (mip ? tex_mip_point[k] : 0) + tex_tw(x, y, s, lh);
                    if(o / (uint32_t)n >= TEX_VQ_PLACES)
                        continue;
                    tex_vq_el[o / (uint32_t)n][o % (uint32_t)n] = (uint16_t)fn(ctx, s, x, y);
                    if((int)(o / (uint32_t)n) >= places)
                        places = (int)(o / (uint32_t)n) + 1;
                }
        }
        for(i = 0; i < places; i++) {
            uint8_t b[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            for(e = 0; e < n; e++) {
                const uint16_t v = tex_vq_el[i][e];
                if(l->bits == 16) { b[2 * e] = (uint8_t)v; b[2 * e + 1] = (uint8_t)(v >> 8); }
                else if(l->bits == 8) b[e] = (uint8_t)v;
                else b[e >> 1] |= (uint8_t)((v & 15) << ((e & 1) * 4));
            }
            tex_put(8, 2048 + (uint32_t)i, (uint32_t)tex_vq_code(b));
        }
        for(k = 0; k < 256; k++)
            for(i = 0; i < 8; i++)
                tex_put(8, (uint32_t)(k * 8 + i), k < tex_codes ? tex_code[k][i] : 0);
    }
    at = pvr_tex_alloc(tex_len);
    pvr_tex_load(at, tex_mem, tex_len);
    return at;
}

#endif
