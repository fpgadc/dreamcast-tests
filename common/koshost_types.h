/* PC stand-in for KallistiOS's <arch/types.h>: the same names with the sizes they have on the console. */
#ifndef PVRHOST_ARCH_TYPES_H
#define PVRHOST_ARCH_TYPES_H
#include <stddef.h>
#include <stdint.h>
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef char int8;
typedef volatile uint32 vuint32;
typedef volatile uint16 vuint16;
typedef volatile uint8 vuint8;
typedef uint32 ptr_t;
#ifndef __depr
#define __depr(m)
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __nonnull
#define __nonnull(x)
#endif
#ifndef __nonnull_all
#define __nonnull_all
#endif
#ifndef __no_inline
#define __no_inline
#endif
#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif
#ifndef __is_defined
#define __is_defined(x) 0
#endif
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#define __END_DECLS
#endif
/* what KallistiOS's pvr_prim.c uses besides the C library */
/* on the console: allocate the cache line and store the first word in it */
#define dcache_alloc_block(p, v) (*(uint32_t *)(p) = (v))
#define dcache_pref_block(...) ((void)0)
#define assert_msg(e, m) assert(e)
#endif
