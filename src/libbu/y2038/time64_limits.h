/*
   Maximum and minimum inputs your system's respective time functions
   can correctly handle.  time64.h will use your system functions if
   the input falls inside these ranges and corresponding USE_SYSTEM_*
   constant is defined.
*/

#ifndef TIME64_LIMITS_H
#define TIME64_LIMITS_H

/* Max/min for localtime() */
#define SYSTEM_LOCALTIME_MAX     2147483647
#define SYSTEM_LOCALTIME_MIN    -2147483647-1

/* Max/min for gmtime() */
#define SYSTEM_GMTIME_MAX        2147483647
#define SYSTEM_GMTIME_MIN       -2147483647-1

/* MODIFIED: Moved max/min for mktime() to time64.c to avoid having to
   initialize it with system-dependent field ordering.
*/

/* MODIFIED: Removed SYSTEM_TIMEGM_MAX/MIN since they are not used
   anywhere
*/

#endif /* TIME64_LIMITS_H */
