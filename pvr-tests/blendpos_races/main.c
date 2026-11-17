/* blendpos's races: buffer 2's views of the variants with empty tiles after content, damaged somewhere else in
   every console run (see ../blendpos/main.c and the README). Left out of regress.sh unless named. */
#define NUM_PAGES 6
#define BLENDPOS_RACES
#include "../blendpos/main.c"
