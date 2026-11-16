#!/bin/bash
# kos_words.sh <program.c> [more .c ...]: build a small C program for the PC against KallistiOS's own PVR
# header and its header-compile code (pvr_prim.c), and run it. For finding out, when porting a test from
# KallistiOS's driver, exactly which words pvr_poly_compile() & co. made for a context:
#
#     #include <stdio.h>
#     #include <string.h>
#     #include <dc/pvr.h>
#     int main(void) {
#         pvr_poly_cxt_t cxt; pvr_poly_hdr_t hdr;
#         memset(&hdr, 0, sizeof(hdr));
#         pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
#         pvr_poly_compile(&hdr, &cxt);
#         printf("%08x %08x %08x %08x\n", hdr.cmd, hdr.mode1, hdr.mode2, hdr.mode3);    /* PCW ISP_TSP TSP TCW */
#     }
#
# A texture "pointer" is (pvr_ptr_t)(uintptr_t)(0xa4000000 + address in the 64 bit area). Only the header and
# pvr_prim.c exist here: nothing that touches the hardware (pvr_prim, pvr_txr_load, fog, palette...).
K=/opt/toolchains/dc/kos
HERE=$(cd "$(dirname "$0")" && pwd)
H=/tmp/pvrhost.$$; trap "rm -rf $H" EXIT
mkdir -p $H/dc $H/arch $H/kos $H/src
ln -s $K/kernel/arch/dreamcast/include/dc/pvr.h $H/dc/pvr.h
ln -s $K/kernel/arch/dreamcast/include/dc/pvr $H/dc/pvr
ln -s $K/include/kos/regfield.h $H/kos/regfield.h
ln -s $(find $K -name img.h | head -n 1) $H/kos/img.h
cp $HERE/koshost_types.h $H/arch/types.h
for f in arch/memory.h arch/cache.h dc/sq.h kos/platform.h kos/mutex.h kos/dbglog.h; do echo '#include <arch/types.h>' > $H/$f; done
cp $K/kernel/arch/dreamcast/hardware/pvr/pvr_prim.c $H/src/
echo '#include <dc/pvr.h>' > $H/src/pvr_internal.h
gcc -O1 -w -I$H -o $H/prog "$@" $H/src/pvr_prim.c -lm && $H/prog
