/* accbuf's races: the cases whose console picture depends on when the write-out runs against the TSP (different
   from run to run, or the same every run but not modelled). accbuf's original pages 0, 1, 3, 5 and 7, whole;
   see ../accbuf/main.c. Left out of regress.sh unless named. */
#define NUM_PAGES 5
#define ACCBUF_RACES
#include "../accbuf/main.c"
