/* rle_config.h
 * 
 * All of the relevant #defines that the Utah Raster Toolkit needs should be
 * done in "machine.h" when the toolkit is compiled with BRLCAD
 * 
 */
#include "machine.h"

#define CONST_DECL const

#if __STDC__
#	define VOID_STAR		/* for the Utah Raster Toolkit */
#endif

#if __STDC__
#	define USE_STDARG	1	/* Use <stdarg.h> not <varargs.h> */
#endif

#if BSD && !SYSV
#  define SYS_TIME_H	/* time_t is defined through sys/time.h not time.h */
#endif

#if !BSD && SYSV
#	define rindex strrchr
#endif
