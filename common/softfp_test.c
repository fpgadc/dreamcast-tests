/* Check softfp.h against this PC's own single precision arithmetic (IEEE, round to nearest even):
       gcc -O1 -o /tmp/softfp_test softfp_test.c && /tmp/softfp_test
   Every result has to be the same bits (any NaN counts as any NaN). */
#include <stdio.h>
#include <string.h>
#include "softfp.h"

static uint64_t state = 88172645463325252ull;
static uint32_t rnd(void) {
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return (uint32_t)(state >> 16);
}
static float f(f32b b) { float x; memcpy(&x, &b, 4); return x; }
static f32b b(volatile float x) { f32b v; float y = x; memcpy(&v, &y, 4); return v; }
static int same(f32b x, f32b y) { return x == y || (sf_is_nan(x) && sf_is_nan(y)); }

/* numbers of several kinds: any bits, a narrow range of exponents (so that sums cancel and round), small
   integers and their fractions, subnormals */
static f32b pick(int kind) {
    uint32_t r = rnd();
    switch(kind % 6) {
    case 0: return r;
    case 1: return (r & 0x807fffffu) | ((120 + rnd() % 16) << 23);
    case 2: return sf_from_int((int32_t)(r % 4096) - 2048);
    case 3: return (r & 0x807fffffu) | ((rnd() % 3) << 23);
    case 4: return (r & 0x80000000u) | ((127 + rnd() % 24) << 23) | ((r & 0xff) << 15);
    default: return (r & 0x807fffffu) | ((100 + rnd() % 60) << 23);
    }
}

int main(void) {
    long n, bad = 0, total = 0;
    for(n = 0; n < 20000000; n++) {
        f32b x = pick((int)n), y = pick((int)(n / 6)), want[4], got[4];
        volatile float fx = f(x), fy = f(y);
        int k;
        want[0] = b(fx + fy); got[0] = sf_add(x, y);
        want[1] = b(fx - fy); got[1] = sf_sub(x, y);
        want[2] = b(fx * fy); got[2] = sf_mul(x, y);
        want[3] = b(fx / fy); got[3] = sf_div(x, y);
        for(k = 0; k < 4; k++, total++)
            if(!same(want[k], got[k]) && bad++ < 10)
                printf("%c: %08x %08x -> %08x, this PC %08x\n", "+-*/"[k], (unsigned)x, (unsigned)y, (unsigned)got[k], (unsigned)want[k]);
        if(n % 8 == 0) {
            int32_t p = (int32_t)rnd(), q = (int32_t)(rnd() >> (rnd() % 31));
            volatile float fp = (float)p, fq;
            volatile double dq;
            total += 3;
            if(!same(b(fp), sf_from_int(p)) && bad++ < 10)
                printf("int %d -> %08x, this PC %08x\n", (int)p, (unsigned)sf_from_int(p), (unsigned)b(fp));
            dq = (double)p / (double)(q ? q : 1);       /* exact enough? no: check with small numbers only */
            p %= 100000; q = q % 100000; if(!q) q = 7;
            dq = (double)p / (double)q;
            fq = (float)dq;                             /* double rounding cannot happen: p, q < 2^17 */
            if(!same(b(fq), sf_ratio(p, q)) && bad++ < 10)
                printf("ratio %d / %d -> %08x, this PC %08x\n", (int)p, (int)q, (unsigned)sf_ratio(p, q), (unsigned)b(fq));
            if(!same(b(fx * 8.0f), sf_scale(x, 3)) && bad++ < 10)
                printf("scale %08x\n", (unsigned)x);
        }
    }
    printf("softfp: %ld of %ld results are the PC's\n", total - bad, total);
    return bad != 0;
}
