/* gzip members inflated (RFC 1952 around RFC 1951), for a test that reads a PVR core dump (.pvrdmp: "pvrcoredump", a
   gzip member of video memory, a gzip member of the register file; nulldc3 libswirl/hw/pvr/pvr_coredump.h) on a
   console as well as on a PC: KallistiOS has no zlib, and both sides have to come to the same bytes. In the manner of
   zlib's contrib/puff (canonical Huffman codes decoded a bit at a time): small, and fast enough for 8 MB on the SH4.
   The member's CRC-32 and length are checked.

     long gz_member(const uint8_t *in, size_t len, size_t *pos, uint8_t *out, size_t cap)

   inflates the member at in[*pos] into out (at most cap bytes) and moves *pos past it: the number of bytes, or -1
   (not a gzip member, a broken stream, more than cap bytes, a CRC or length that does not match). */
#ifndef GUNZIP_H
#define GUNZIP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const uint8_t *in;
    size_t len, pos;
    uint32_t bitbuf;
    int bitcnt;
    uint8_t *out;
    size_t cap, n;
    int bad;
} gz_state;

typedef struct {
    short count[16];        /* codes of each length */
    short symbol[320];      /* the symbols in canonical order */
} gz_huff;

static int gz_bits(gz_state *s, int need) {
    uint32_t v = s->bitbuf;
    while(s->bitcnt < need) {
        if(s->pos >= s->len) {
            s->bad = 1;
            return 0;
        }
        v |= (uint32_t)s->in[s->pos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1u << need) - 1));
}

static int gz_decode(gz_state *s, const gz_huff *h) {
    int code = 0, first = 0, index = 0, len, count;
    for(len = 1; len < 16; len++) {
        code |= gz_bits(s, 1);
        count = h->count[len];
        if(code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->bad = 1;
    return 0;
}

/* the code of these lengths; 0 when it is complete or incomplete in a way inflate allows, -1 when oversubscribed */
static int gz_construct(gz_huff *h, const short *length, int n) {
    short offs[16];
    int sym, len, left = 1;
    for(len = 0; len < 16; len++)
        h->count[len] = 0;
    for(sym = 0; sym < n; sym++)
        h->count[length[sym]]++;
    if(h->count[0] == n)
        return 0;
    for(len = 1; len < 16; len++) {
        left <<= 1;
        left -= h->count[len];
        if(left < 0)
            return -1;
    }
    offs[1] = 0;
    for(len = 1; len < 15; len++)
        offs[len + 1] = (short)(offs[len] + h->count[len]);
    for(sym = 0; sym < n; sym++)
        if(length[sym] != 0)
            h->symbol[offs[length[sym]]++] = (short)sym;
    return left;
}

static void gz_put(gz_state *s, uint8_t b) {
    if(s->n >= s->cap) {
        s->bad = 1;
        return;
    }
    s->out[s->n++] = b;
}

static int gz_codes(gz_state *s, const gz_huff *lencode, const gz_huff *distcode) {
    static const short lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83,
                                     99, 115, 131, 163, 195, 227, 258 };
    static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const short dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025,
                                     1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
    static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
                                    12, 13, 13 };
    for(;;) {
        int sym = gz_decode(s, lencode);
        if(s->bad)
            return -1;
        if(sym < 256)
            gz_put(s, (uint8_t)sym);
        else if(sym == 256)
            return 0;
        else {
            size_t len, dist;
            sym -= 257;
            if(sym >= 29)
                return -1;
            len = (size_t)(lbase[sym] + gz_bits(s, lext[sym]));
            sym = gz_decode(s, distcode);
            if(s->bad || sym >= 30)
                return -1;
            dist = (size_t)(dbase[sym] + gz_bits(s, dext[sym]));
            if(s->bad || dist > s->n || s->n + len > s->cap)
                return -1;
            while(len--) {
                s->out[s->n] = s->out[s->n - dist];
                s->n++;
            }
        }
        if(s->bad)
            return -1;
    }
}

static int gz_stored(gz_state *s) {
    size_t len;
    s->bitbuf = 0;
    s->bitcnt = 0;                                  /* to the next byte */
    if(s->pos + 4 > s->len)
        return -1;
    len = s->in[s->pos] | (size_t)s->in[s->pos + 1] << 8;
    if((size_t)(s->in[s->pos + 2] | s->in[s->pos + 3] << 8) != (~len & 0xffff))
        return -1;
    s->pos += 4;
    if(s->pos + len > s->len || s->n + len > s->cap)
        return -1;
    while(len--)
        s->out[s->n++] = s->in[s->pos++];
    return 0;
}

static int gz_fixed(gz_state *s) {
    static gz_huff lencode, distcode;
    static int built;
    if(!built) {
        short lengths[288];
        int sym;
        for(sym = 0; sym < 144; sym++) lengths[sym] = 8;
        for(; sym < 256; sym++) lengths[sym] = 9;
        for(; sym < 280; sym++) lengths[sym] = 7;
        for(; sym < 288; sym++) lengths[sym] = 8;
        gz_construct(&lencode, lengths, 288);
        for(sym = 0; sym < 30; sym++) lengths[sym] = 5;
        gz_construct(&distcode, lengths, 30);
        built = 1;
    }
    return gz_codes(s, &lencode, &distcode);
}

static int gz_dynamic(gz_state *s) {
    static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    short lengths[320];
    gz_huff lencode, distcode;
    int nlen = gz_bits(s, 5) + 257, ndist = gz_bits(s, 5) + 1, ncode = gz_bits(s, 4) + 4, index, err;
    if(s->bad || nlen > 286 || ndist > 30)
        return -1;
    for(index = 0; index < ncode; index++)
        lengths[order[index]] = (short)gz_bits(s, 3);
    for(; index < 19; index++)
        lengths[order[index]] = 0;
    if(s->bad || gz_construct(&lencode, lengths, 19) != 0)
        return -1;
    for(index = 0; index < nlen + ndist;) {
        int sym = gz_decode(s, &lencode), len = 0, rep;
        if(s->bad)
            return -1;
        if(sym < 16) {
            lengths[index++] = (short)sym;
            continue;
        }
        if(sym == 16) {
            if(index == 0)
                return -1;
            len = lengths[index - 1];
            rep = 3 + gz_bits(s, 2);
        } else if(sym == 17)
            rep = 3 + gz_bits(s, 3);
        else
            rep = 11 + gz_bits(s, 7);
        if(s->bad || index + rep > nlen + ndist)
            return -1;
        while(rep--)
            lengths[index++] = (short)len;
    }
    if(lengths[256] == 0)
        return -1;
    err = gz_construct(&lencode, lengths, nlen);
    if(err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
        return -1;
    err = gz_construct(&distcode, lengths + nlen, ndist);
    if(err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
        return -1;
    return gz_codes(s, &lencode, &distcode);
}

static uint32_t gz_crc32(const uint8_t *p, size_t n) {
    static uint32_t table[256];
    static int built;
    uint32_t c = 0xffffffffu;
    size_t i;
    if(!built) {
        uint32_t k, j, v;
        for(k = 0; k < 256; k++) {
            v = k;
            for(j = 0; j < 8; j++)
                v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            table[k] = v;
        }
        built = 1;
    }
    for(i = 0; i < n; i++)
        c = table[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

static long gz_member(const uint8_t *in, size_t len, size_t *pos, uint8_t *out, size_t cap) {
    gz_state s;
    size_t p = *pos;
    int flags, last;
    uint32_t crc, size;
    if(p + 18 > len || in[p] != 0x1f || in[p + 1] != 0x8b || in[p + 2] != 8)
        return -1;
    flags = in[p + 3];
    p += 10;
    if(flags & 4) {                                 /* FEXTRA */
        if(p + 2 > len) return -1;
        p += 2 + (in[p] | (size_t)in[p + 1] << 8);
    }
    if(flags & 8) { while(p < len && in[p]) p++; p++; }      /* FNAME */
    if(flags & 16) { while(p < len && in[p]) p++; p++; }     /* FCOMMENT */
    if(flags & 2) p += 2;                           /* FHCRC */
    if(p >= len)
        return -1;
    s.in = in; s.len = len; s.pos = p; s.bitbuf = 0; s.bitcnt = 0; s.out = out; s.cap = cap; s.n = 0; s.bad = 0;
    do {
        int type, err;
        last = gz_bits(&s, 1);
        type = gz_bits(&s, 2);
        if(s.bad)
            return -1;
        err = type == 0 ? gz_stored(&s) : type == 1 ? gz_fixed(&s) : type == 2 ? gz_dynamic(&s) : -1;
        if(err || s.bad)
            return -1;
    } while(!last);
    if(s.pos + 8 > len)                             /* the trailer starts at the next byte */
        return -1;
    crc = s.in[s.pos] | (uint32_t)s.in[s.pos + 1] << 8 | (uint32_t)s.in[s.pos + 2] << 16 | (uint32_t)s.in[s.pos + 3] << 24;
    size = s.in[s.pos + 4] | (uint32_t)s.in[s.pos + 5] << 8 | (uint32_t)s.in[s.pos + 6] << 16 | (uint32_t)s.in[s.pos + 7] << 24;
    if(size != (uint32_t)s.n || crc != gz_crc32(out, s.n))
        return -1;
    *pos = s.pos + 8;
    return (long)s.n;
}

#endif
