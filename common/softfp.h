/* Single precision floating point in integer operations: IEEE 754 results (round to nearest, ties to even),
   bit for bit the same on every CPU and with every compiler. The tests use it for the numbers they have to
   calculate at run time (a coordinate from a cell number, a texture coordinate from a texel number), so that
   a PC and the SH4 always send the same bits to the PVR. softfp_test.c checks it against a PC's own floats.

   A float is carried as its bit pattern (f32b). This is the arithmetic of the PROGRAM that feeds the PVR,
   not the PVR's own (that is nulldc3's pvr_softfp.h). */
#ifndef SOFTFP_H
#define SOFTFP_H

#include <stdint.h>

typedef uint32_t f32b;                      /* a single precision float, as its bits */

/* a float constant written in the source: the compiler converts it, nothing is calculated at run time */
static inline f32b F(float x) {
    union { float f; uint32_t u; } v = { x };
    return v.u;
}

#define SF_SIGN 0x80000000u
#define SF_INF  0x7f800000u
#define SF_NAN  0x7fc00000u

static inline int sf_is_nan(f32b a) { return (a & 0x7fffffffu) > SF_INF; }
static inline int sf_is_inf(f32b a) { return (a & 0x7fffffffu) == SF_INF; }
static inline int sf_is_zero(f32b a) { return (a & 0x7fffffffu) == 0; }
static inline f32b sf_neg(f32b a) { return a ^ SF_SIGN; }
static inline f32b sf_abs(f32b a) { return a & 0x7fffffffu; }

/* the value sign * m * 2^e (plus something smaller than the last bit of m if sticky), rounded to a float */
static inline f32b sf_round(uint32_t sign, uint64_t m, int e, int sticky) {
    uint32_t q;
    uint64_t rest, half;
    int shift, E;

    if(!m)
        return sign;
    while(!(m >> 63)) { m <<= 1; e--; }
    E = e + 63;                             /* the value is in [2^E, 2^(E+1)) */
    shift = 40;                             /* 24 bits stay */
    if(E < -126)                            /* subnormal: fewer bits stay */
        shift += -126 - E;
    if(shift > 63) {                        /* everything is below the last bit; at most half of it */
        rest = shift == 64 ? m : 0;
        sticky |= shift == 64 ? 0 : 1;
        q = 0;
        half = (uint64_t)1 << 63;
        if(shift > 64)
            return sign;                    /* less than half of the smallest number */
    } else {
        q = (uint32_t)(m >> shift);
        rest = m & (((uint64_t)1 << shift) - 1);
        half = (uint64_t)1 << (shift - 1);
    }
    if(rest > half || (rest == half && (sticky || (q & 1))))
        q++;
    if(E < -126)
        return sign | q;                    /* subnormal; q = 2^23 is the smallest normal number */
    if(q == 0x01000000u) { q >>= 1; E++; }
    if(E > 127)
        return sign | SF_INF;
    return sign | ((uint32_t)(E + 127) << 23) | (q & 0x007fffffu);
}

/* mantissa and exponent of a finite number: |a| = m * 2^e */
static inline uint32_t sf_unpack(f32b a, int *e) {
    uint32_t exp = (a >> 23) & 0xff, frac = a & 0x007fffffu;
    if(exp) { *e = (int)exp - 150; return frac | 0x00800000u; }
    *e = -149;
    return frac;
}

static inline f32b sf_from_int(int32_t n) {
    return sf_round(n < 0 ? SF_SIGN : 0, n < 0 ? (uint64_t)0 - (uint64_t)(int64_t)n : (uint64_t)n, 0, 0);
}

static inline f32b sf_mul(f32b a, f32b b) {
    uint32_t sign = (a ^ b) & SF_SIGN, ma, mb;
    int ea, eb;
    if(sf_is_nan(a) || sf_is_nan(b))
        return SF_NAN;
    if(sf_is_inf(a) || sf_is_inf(b))
        return (sf_is_zero(a) || sf_is_zero(b)) ? SF_NAN : sign | SF_INF;
    ma = sf_unpack(a, &ea);
    mb = sf_unpack(b, &eb);
    return sf_round(sign, (uint64_t)ma * mb, ea + eb, 0);
}

static inline f32b sf_div(f32b a, f32b b) {
    uint32_t sign = (a ^ b) & SF_SIGN, ma, mb;
    uint64_t n;
    int ea, eb;
    if(sf_is_nan(a) || sf_is_nan(b))
        return SF_NAN;
    if(sf_is_inf(a))
        return sf_is_inf(b) ? SF_NAN : sign | SF_INF;
    if(sf_is_inf(b))
        return sign;
    if(sf_is_zero(b))
        return sf_is_zero(a) ? SF_NAN : sign | SF_INF;
    ma = sf_unpack(a, &ea);
    mb = sf_unpack(b, &eb);
    if(!ma)
        return sign;
    while(!(ma & 0x00800000u)) { ma <<= 1; ea--; }      /* so that the quotient has enough bits */
    n = (uint64_t)ma << 40;
    return sf_round(sign, n / mb, ea - eb - 40, n % mb != 0);
}

static inline f32b sf_add(f32b a, f32b b) {
    uint32_t sa = a & SF_SIGN, sb = b & SF_SIGN;
    uint64_t ma, mb, lost;
    int ea, eb, d, sticky = 0;
    if(sf_is_nan(a) || sf_is_nan(b))
        return SF_NAN;
    if(sf_is_inf(a))
        return (sf_is_inf(b) && sa != sb) ? SF_NAN : a;
    if(sf_is_inf(b))
        return b;
    ma = sf_unpack(a, &ea);
    mb = sf_unpack(b, &eb);
    if(!ma && !mb)
        return sa & sb;                     /* -0 only from -0 + -0 */
    if(!ma) return b;
    if(!mb) return a;
    if(ea < eb || (ea == eb && ma < mb)) {  /* a is the one with the larger magnitude */
        uint64_t tm = ma; int te = ea; uint32_t ts = sa;
        ma = mb; ea = eb; sa = sb;
        mb = tm; eb = te; sb = ts;
    }
    ma <<= 32; ea -= 32;
    mb <<= 32; eb -= 32;
    d = ea - eb;
    if(d > 60) {
        mb = 0;
        sticky = 1;
    } else if(d) {
        lost = mb & (((uint64_t)1 << d) - 1);
        mb >>= d;
        sticky = lost != 0;
    }
    if(sa == sb)
        return sf_round(sa, ma + mb, ea, sticky);
    if(ma == mb && !sticky)
        return 0;                           /* exact cancellation: +0 */
    /* with sticky the true difference is a little less than ma - mb: one unit down, the rest still below */
    return sf_round(sa, ma - mb - (sticky ? 1 : 0), ea, sticky);
}

static inline f32b sf_sub(f32b a, f32b b) { return sf_add(a, sf_is_nan(b) ? b : sf_neg(b)); }

/* a * 2^k */
static inline f32b sf_scale(f32b a, int k) {
    int e;
    uint32_t m;
    if(sf_is_nan(a) || sf_is_inf(a) || sf_is_zero(a))
        return a;
    m = sf_unpack(a, &e);
    return sf_round(a & SF_SIGN, m, e + k, 0);
}

/* p / q of two integers, rounded once */
static inline f32b sf_ratio(int32_t p, int32_t q) {
    uint64_t n = p < 0 ? (uint64_t)0 - (uint64_t)(int64_t)p : (uint64_t)p, den = q < 0 ? (uint64_t)0 - (uint64_t)(int64_t)q : (uint64_t)q;
    uint32_t sign = ((p < 0) != (q < 0)) ? SF_SIGN : 0;
    int e = 0;
    if(!den)
        return n ? sign | SF_INF : SF_NAN;
    if(!n)
        return sign;
    while(n < ((uint64_t)1 << 62)) { n <<= 1; e--; }
    return sf_round(sign, n / den, e, n % den != 0);
}

static inline int sf_less(f32b a, f32b b) {             /* a < b, for finite numbers */
    int64_t x = (a & SF_SIGN) ? -(int64_t)(a & 0x7fffffffu) : (int64_t)a;
    int64_t y = (b & SF_SIGN) ? -(int64_t)(b & 0x7fffffffu) : (int64_t)b;
    return x < y;
}

#endif
