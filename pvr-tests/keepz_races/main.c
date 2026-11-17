/* keepz's race: an empty keep Z entry after a keep Z entry that is not written out, then a written tile (cell 10
   gets that tile's picture, as if its write-out came late). keepz's page 2 whole; see ../keepz/main.c. Left out
   of regress.sh unless named. */
#define NUM_PAGES 1
#define KEEPZ_RACES
#include "../keepz/main.c"
