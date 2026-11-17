/* ta-probe: questions about the tile accelerator for nulldc3's refta, one case each, on the harness of
   ../ta-tests (common/pvrio.h; its main.c, with these cases in place of its own: probes.c).

   Each case answers one question that the cases of ta-tests leave open: how many tiles the TA keeps lists for
   (grids past 300 and 600 tiles, past 480 lines), what TA_OL_POINTERS (0x005f8600 .. 0x005f8f5c) holds,
   where the default user clip rectangle is, what the modifier volume bounds leak into, sprite parameter
   layouts, object pointer block alignment and limits, what TA_LIST_CONT and TA_LIST_INIT keep, and (with
   TA_ERRORS 1, a run of its own) what bad input does and what the TA does after it.

   Dumps per case, besides those of ta-tests (c<n>, c<n>s): c<n>r, the 600 words of TA_OL_POINTERS after the
   case; c<n>m<k>, c<n>m<k>s, c<n>m<k>r, the same in the middle of the case. Cases marked SCAN fill all of
   video memory and print "PVRIO scan c<n> <address> <word>" for every word written outside their area.

       bash run2.sh ta-probe off ; bash run2.sh ta-probe hw=<ip>
       python3 ta-tests/compare_dumps.py --test ta-probe hw off       */
#define NUM_PAGES 300               /* about: three or more dump files a case */
#define TA_LONG 0
#define TA_PROBES "../ta-probe/probes.c"
#include "../ta-tests/main.c"
