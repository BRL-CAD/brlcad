/*
Copyright (c) 2007-2010  Michael G Schwern

The MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef TIME64_H
#    define TIME64_H

#include "common.h"

#ifndef Y2038_EXPORT
#  if defined(Y2038_DLL_EXPORTS) && defined(Y2038_DLL_IMPORTS)
#    error "Only Y2038_DLL_EXPORTS or Y2038_DLL_IMPORTS can be defined, not both."
#  elif defined(STATIC_BUILD)
#    define Y2038_EXPORT
#  elif defined(Y2038_DLL_EXPORTS)
#    define Y2038_EXPORT __declspec(dllexport)
#  elif defined(Y2038_DLL_IMPORTS)
#    define Y2038_EXPORT __declspec(dllimport)
#  else
#    define Y2038_EXPORT
#  endif
#endif

#include <time.h>
#include "time64_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set our custom types */
typedef INT_64_T        Int64;
typedef Int64           Time64_T;
typedef Int64           Year;

#ifndef PRId64
# if (__WORDSIZE == 64) && !defined(__APPLE__)
#  define PRId64 "ld"
# else
#  define PRId64 "lld"
# endif
#endif

/* A copy of the tm struct but with a 64 bit year */
struct TM64 {
        int     tm_sec;
        int     tm_min;
        int     tm_hour;
        int     tm_mday;
        int     tm_mon;
        Year    tm_year;
        int     tm_wday;
        int     tm_yday;
        int     tm_isdst;

#ifdef HAS_TM_TM_GMTOFF
        long    tm_gmtoff;
#endif

#ifdef HAS_TM_TM_ZONE
        const char *tm_zone;
#endif
};

/* Decide which tm struct to use */
#ifdef USE_TM64
#define TM      TM64
#else
#define TM      tm
#endif   

/* Declare public functions */
Y2038_EXPORT struct TM *gmtime64_r    (const Time64_T *, struct TM *);
Y2038_EXPORT struct TM *localtime64_r (const Time64_T *, struct TM *);
Y2038_EXPORT struct TM *gmtime64      (const Time64_T *);
Y2038_EXPORT struct TM *localtime64   (const Time64_T *);

Y2038_EXPORT char *asctime64          (const struct TM *);
Y2038_EXPORT char *asctime64_r        (const struct TM *, char *);

Y2038_EXPORT char *ctime64            (const Time64_T*);
Y2038_EXPORT char *ctime64_r          (const Time64_T*, char*);

Y2038_EXPORT Time64_T   timegm64      (const struct TM *);
Y2038_EXPORT Time64_T   mktime64      (struct TM *);
Y2038_EXPORT Time64_T   timelocal64   (struct TM *);


/* Not everyone has gm/localtime_r(), provide a replacement */
#ifdef HAS_LOCALTIME_R
#    define LOCALTIME_R(clock, result) localtime_r(clock, result)
#else
#    define LOCALTIME_R(clock, result) fake_localtime_r(clock, result)
#endif
#ifdef HAS_GMTIME_R
#    define GMTIME_R(clock, result)    gmtime_r(clock, result)
#else
#    define GMTIME_R(clock, result)    fake_gmtime_r(clock, result)
#endif


/* Use a different asctime format depending on how big the year is */
#ifdef USE_TM64
    #define TM64_ASCTIME_FORMAT "%.3s %.3s%3d %.2d:%.2d:%.2d %"PRId64"\n"
#else
    #define TM64_ASCTIME_FORMAT "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n"
#endif

#ifdef __cplusplus
  }
#endif

#endif
