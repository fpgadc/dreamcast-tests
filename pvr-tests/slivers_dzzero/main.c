/* slivers_dzzero: a zero depth (exponent field 0, any mantissa) in the mean of a determinant of zero (2026-11-19).
   slivers_detzero's points and readouts (U with the third corner's U 64, 1/w by fog, U, colour; pages 4 s + m for
   set s) with the depth triples of gen.cpp here.

   Results (one console, 2026-11-19): 9,600 of 9,600 triples with the rule: against a depth p of 2^-126 .. 2^-125 of
   the other sign the zero counts as (1 + m 2^-23) 2^-127 (m its mantissa); if p less that is below 2^-126 the sum is
   +0, otherwise it is p unchanged (831 triples exactly on the boundary: p kept). Taking the zero off a kept p: 6,711;
   the zero as its denormal value: 7,562; as 2^-127: 7,564; a zero as zero (before): 6,834. Same signs, or a larger
   p: the zero is nothing. The mean's last addition with a zero is not told apart (kinds 5, 6: their means are
   below 2^-126, zero, whatever the rule). */
#define NUM_PAGES 8
#define CASES_H "../slivers_dzzero/cases.h"
#include "../slivers_detzero/main.c"
