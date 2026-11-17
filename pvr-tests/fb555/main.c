/* The 16 bit framebuffer test in RGB555. See ../fb565/main.c (which includes pvrtest2.h; this file names it
   so that run2.sh accepts the test). */
#define NUM_PAGES 2
#define TEST_FB_PACKMODE fb_packmode_0555
#include "../fb565/main.c"
