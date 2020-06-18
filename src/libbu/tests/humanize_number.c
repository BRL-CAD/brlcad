/*-
 * Copyright 2012 Clifton Royston
 * Copyright 2013 John-Mark Gurney
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "common.h"

#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#else
#  include "pinttypes.h"
#endif
#include <limits.h>
#include <string.h>

#include "bu/exit.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/units.h"

#define	MAX_STR_FLAGS_RESULT	80
#define MAX_INT_STR_DIGITS	12

static const int64_t halfExabyte = (int64_t)500*1000*1000*1000*1000*1000L;

static struct {
    int retval;
    const char *res;
    int64_t num;
    int flags;
    int scale;
} test_args[] = {
    /* tests 0-13 test 1000 suffixes */
    { 1, "0", (int64_t)0L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 k", (int64_t)500L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 M", (int64_t)500*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 G", (int64_t)500*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 T", (int64_t)500*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 P", (int64_t)500*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 E", (int64_t)500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 1, "1", (int64_t)1L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 k", (int64_t)1500L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 M", (int64_t)1500*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 G", (int64_t)1500*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 T", (int64_t)1500*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 P", (int64_t)1500*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 E", (int64_t)1500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },

    /* tests 14-27 test 1024 suffixes */
    { 1, "0", (int64_t)0L, 0, BU_HN_AUTOSCALE },
    { 3, "1 K", (int64_t)512L, 0, BU_HN_AUTOSCALE },
    { 3, "1 M", (int64_t)512*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "1 G", (int64_t)512*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "1 T", (int64_t)512*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "1 P", (int64_t)512*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "1 E", (int64_t)512*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 1, "1", (int64_t)1L, 0, BU_HN_AUTOSCALE },
    { 3, "2 K", (int64_t)1536L, 0, BU_HN_AUTOSCALE },
    { 3, "2 M", (int64_t)1536*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "2 G", (int64_t)1536*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "2 T", (int64_t)1536*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "2 P", (int64_t)1536*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "2 E", (int64_t)1536*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },

    /* tests 28-37 test rounding */
    { 3, "0 M", (int64_t)500*1000L-1, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 M", (int64_t)500*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "1 M", (int64_t)1000*1000L + 500*1000L-1, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "2 M", (int64_t)1000*1000L + 500*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 3, "0 K", (int64_t)512L-1, 0, BU_HN_AUTOSCALE },
    { 3, "1 K", (int64_t)512L, 0, BU_HN_AUTOSCALE },
    { 3, "0 M", (int64_t)512*1024L-1, 0, BU_HN_AUTOSCALE },
    { 3, "1 M", (int64_t)512*1024L, 0, BU_HN_AUTOSCALE },
    { 3, "1 M", (int64_t)1024*1024L + 512*1024L-1, 0, BU_HN_AUTOSCALE },
    { 3, "2 M", (int64_t)1024*1024L + 512*1024L, 0, BU_HN_AUTOSCALE },

    /* tests 38-61 test specific scale factors with 1000 divisor */
    { 3, "0 k", (int64_t)0L, BU_HN_DIVISOR_1000, 1 },
    { 3, "1 k", (int64_t)500L, BU_HN_DIVISOR_1000, 1 },
    { 3, "0 M", (int64_t)500L, BU_HN_DIVISOR_1000, 2 },
    { 3, "1 M", (int64_t)500*1000L, BU_HN_DIVISOR_1000, 2 },
    { 3, "0 G", (int64_t)500*1000L, BU_HN_DIVISOR_1000, 3 },
    { 3, "1 G", (int64_t)500*1000*1000L, BU_HN_DIVISOR_1000, 3 },
    { 3, "0 T", (int64_t)500*1000*1000L, BU_HN_DIVISOR_1000, 4 },
    { 3, "1 T", (int64_t)500*1000*1000*1000L, BU_HN_DIVISOR_1000, 4 },
    { 3, "0 P", (int64_t)500*1000*1000*1000L, BU_HN_DIVISOR_1000, 5 },
    { 3, "1 P", (int64_t)500*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 5 },
    { 3, "0 E", (int64_t)500*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },
    { 3, "1 E", (int64_t)500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },
    { 3, "0 k", (int64_t)1L, BU_HN_DIVISOR_1000, 1 },
    { 3, "2 k", (int64_t)1500L, BU_HN_DIVISOR_1000, 1 },
    { 3, "0 M", (int64_t)1500L, BU_HN_DIVISOR_1000, 2 },
    { 3, "2 M", (int64_t)1500*1000L, BU_HN_DIVISOR_1000, 2 },
    { 3, "0 G", (int64_t)1500*1000L, BU_HN_DIVISOR_1000, 3 },
    { 3, "2 G", (int64_t)1500*1000*1000L, BU_HN_DIVISOR_1000, 3 },
    { 3, "0 T", (int64_t)1500*1000*1000L, BU_HN_DIVISOR_1000, 4 },
    { 3, "2 T", (int64_t)1500*1000*1000*1000L, BU_HN_DIVISOR_1000, 4 },
    { 3, "0 P", (int64_t)1500*1000*1000*1000L, BU_HN_DIVISOR_1000, 5 },
    { 3, "2 P", (int64_t)1500*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 5 },
    { 3, "0 E", (int64_t)1500*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },
    { 3, "2 E", (int64_t)1500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },

    /* tests 62-85 test specific scale factors with 1024 divisor */
    { 3, "0 K", (int64_t)0L, 0, 1 },
    { 3, "1 K", (int64_t)512L, 0, 1 },
    { 3, "0 M", (int64_t)512L, 0, 2 },
    { 3, "1 M", (int64_t)512*1024L, 0, 2 },
    { 3, "0 G", (int64_t)512*1024L, 0, 3 },
    { 3, "1 G", (int64_t)512*1024*1024L, 0, 3 },
    { 3, "0 T", (int64_t)512*1024*1024L, 0, 4 },
    { 3, "1 T", (int64_t)512*1024*1024*1024L, 0, 4 },
    { 3, "0 P", (int64_t)512*1024*1024*1024L, 0, 5 },
    { 3, "1 P", (int64_t)512*1024*1024*1024*1024L, 0, 5 },
    { 3, "0 E", (int64_t)512*1024*1024*1024*1024L, 0, 6 },
    { 3, "1 E", (int64_t)512*1024*1024*1024*1024*1024L, 0, 6 },
    { 3, "0 K", (int64_t)1L, 0, 1 },
    { 3, "2 K", (int64_t)1536L, 0, 1 },
    { 3, "0 M", (int64_t)1536L, 0, 2 },
    { 3, "2 M", (int64_t)1536*1024L, 0, 2 },
    { 3, "0 G", (int64_t)1536*1024L, 0, 3 },
    { 3, "2 G", (int64_t)1536*1024*1024L, 0, 3 },
    { 3, "0 T", (int64_t)1536*1024*1024L, 0, 4 },
    { 3, "2 T", (int64_t)1536*1024*1024*1024L, 0, 4 },
    { 3, "0 P", (int64_t)1536*1024*1024*1024L, 0, 5 },
    { 3, "2 P", (int64_t)1536*1024*1024*1024*1024L, 0, 5 },
    { 3, "0 E", (int64_t)1536*1024*1024*1024*1024L, 0, 6 },
    { 3, "2 E", (int64_t)1536*1024*1024*1024*1024*1024L, 0, 6 },

    /* tests 86-99 test invalid specific scale values of < 0 or >= 7 with
       and without BU_HN_DIVISOR_1000 set */
    /*  all should return errors with new code; with old, the latter 3
	are instead processed as if having AUTOSCALE and/or GETSCALE set */
    { -1, "", (int64_t)1L, 0, 7 },
    { -1, "", (int64_t)1L, BU_HN_DIVISOR_1000, 7 },
    { -1, "", (int64_t)1L, 0, 1000 },
    { -1, "", (int64_t)1L, BU_HN_DIVISOR_1000, 1000 },
    { -1, "", (int64_t)0L, 0, 1000*1000 },
    { -1, "", (int64_t)0L, BU_HN_DIVISOR_1000, 1000*1000 },
    { -1, "", (int64_t)0L, 0, INT_MAX },
    { -1, "", (int64_t)0L, BU_HN_DIVISOR_1000, INT_MAX },

    /* Negative scale values are not handled well
       by the existing library routine - should report as error */
    /*  all should return errors with new code, fail assertion with old */

    { -1, "", (int64_t)1L, 0, -1 },
    { -1, "", (int64_t)1L, BU_HN_DIVISOR_1000, -1 },
    { -1, "", (int64_t)1L, 0, -1000 },
    { -1, "", (int64_t)1L, BU_HN_DIVISOR_1000, -1000 },

    /* tests for scale == 0, without autoscale */
    /* tests 100-114 test scale 0 with 1000 divisor - print first N digits */
    { 1, "0", (int64_t)0L, BU_HN_DIVISOR_1000, 0 },
    { 1, "1", (int64_t)1L, BU_HN_DIVISOR_1000, 0 },
    { 2, "10", (int64_t)10L, BU_HN_DIVISOR_1000, 0 },
    { 3, "0 M", (int64_t)150L, BU_HN_DIVISOR_1000, BU_HN_NOSPACE },
    { 3, "0 M", (int64_t)500L, BU_HN_DIVISOR_1000, BU_HN_NOSPACE },
    { 3, "0 M", (int64_t)999L, BU_HN_DIVISOR_1000, BU_HN_NOSPACE },
    { 3, "150", (int64_t)150L, BU_HN_DIVISOR_1000, 0 },
    { 3, "500", (int64_t)500L, BU_HN_DIVISOR_1000, 0 },
    { 3, "999", (int64_t)999L, BU_HN_DIVISOR_1000, 0 },
    { 4, "100", (int64_t)1000L, BU_HN_DIVISOR_1000, 0 },
    { 4, "150", (int64_t)1500L, BU_HN_DIVISOR_1000, 0 },
    { 6, "500", (int64_t)500*1000L, BU_HN_DIVISOR_1000, 0 },
    { 7, "150", (int64_t)1500*1000L, BU_HN_DIVISOR_1000, 0 },
    { 9, "500", (int64_t)500*1000*1000L, BU_HN_DIVISOR_1000, 0 },
    { 10, "150", (int64_t)1500*1000*1000L, BU_HN_DIVISOR_1000, 0 },

    /* tests 115-126 test scale 0 with 1024 divisor - print first N digits */
    { 1, "0", (int64_t)0L, 0, 0 },
    { 1, "1", (int64_t)1L, 0, 0 },
    { 2, "10", (int64_t)10L, 0, 0 },
    { 3, "150", (int64_t)150L, 0, 0 },
    { 3, "500", (int64_t)500L, 0, 0 },
    { 3, "999", (int64_t)999L, 0, 0 },
    { 4, "100", (int64_t)1000L, 0, 0 },
    { 4, "150", (int64_t)1500L, 0, 0 },
    { 6, "500", (int64_t)500*1000L, 0, 0 },
    { 7, "150", (int64_t)1500*1000L, 0, 0 },
    { 9, "500", (int64_t)500*1000*1000L, 0, 0 },
    { 10, "150", (int64_t)1500*1000*1000L, 0, 0 },

    /* Test boundary cases for very large positive/negative number formatting */
    /* Explicit scale, divisor 1024 */

    /* XXX = requires length 5 (buflen 6) for some cases*/
    /* KLUDGE - test loop below will bump length 5 up to 5 */
    { 3, "8 E",   INT64_MAX, 0, 6 },
    { 4, "-8 E", -INT64_MAX, 0, 6 },
    { 3, "0 E", (int64_t)92*1024*1024*1024*1024*1024L, 0, 6 },
    { 3, "0 E", -(int64_t)92*1024*1024*1024*1024*1024L, 0, 6 },
    { 3, "0 E", (int64_t)82*1024*1024*1024*1024*1024L, 0, 6 },
    { 3, "0 E", -(int64_t)82*1024*1024*1024*1024*1024L, 0, 6 },
    { 3, "0 E", (int64_t)81*1024*1024*1024*1024*1024L, 0, 6 },
    { 3, "0 E", -(int64_t)81*1024*1024*1024*1024*1024L, 0, 6 },
    { 4, "92 P", (int64_t)92*1024*1024*1024*1024*1024L, 0, 5 },
    { 5, "-92 P", -(int64_t)92*1024*1024*1024*1024*1024L, 0, 5 },
    { 4, "82 P", (int64_t)82*1024*1024*1024*1024*1024L, 0, 5 },
    { 5, "-82 P", -(int64_t)82*1024*1024*1024*1024*1024L, 0, 5 },
    { 4, "81 P", (int64_t)81*1024*1024*1024*1024*1024L, 0, 5 },
    { 5, "-81 P", -(int64_t)81*1024*1024*1024*1024*1024L, 0, 5 },

    /* Explicit scale, divisor 1000 */
    { 3, "9 E",   INT64_MAX, BU_HN_DIVISOR_1000, 6 },
    { 4, "-9 E", -INT64_MAX, BU_HN_DIVISOR_1000,  6 },
    { 3, "0 E", (int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  6 },
    { 3, "0 E", -(int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  6 },
    { 3, "0 E", (int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  6 },
    { 3, "0 E", -(int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  6 },
    { 4, "92 P", (int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  5 },
    { 5, "-92 P", -(int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  5 },
    { 4, "91 P", (int64_t)81*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  5 },
    { 5, "-91 P", -(int64_t)81*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  5 },

    /* Autoscale, divisor 1024 */
    { 3, "8 E",   INT64_MAX, 0, BU_HN_AUTOSCALE },
    { 4, "-8 E", -INT64_MAX, 0, BU_HN_AUTOSCALE },
    { 4, "92 P", (int64_t)92*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 5, "-92 P", -(int64_t)92*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 4, "82 P", (int64_t)82*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 5, "-82 P", -(int64_t)82*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 4, "81 P", (int64_t)81*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    { 5, "-81 P", -(int64_t)81*1024*1024*1024*1024*1024L, 0, BU_HN_AUTOSCALE },
    /* Autoscale, divisor 1000 */
    { 3, "9 E",   INT64_MAX, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 4, "-9 E", -INT64_MAX, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 4, "92 P", (int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "-92 P", -(int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 4, "91 P", (int64_t)81*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "-91 P", -(int64_t)81*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },

    /* 0 scale, divisor 1024 */
    { 12, "skdj",  INT64_MAX, 0, 0 },
    { 20, "-9223", -INT64_MAX, 0, 0 },
    { 18, "10358", (int64_t)92*1024*1024*1024*1024*1024L, 0, 0 },
    { 19, "-1035", -(int64_t)92*1024*1024*1024*1024*1024L, 0, 0 },
    { 17, "92323", (int64_t)82*1024*1024*1024*1024*1024L, 0, 0 },
    { 18, "-9232", -(int64_t)82*1024*1024*1024*1024*1024L, 0, 0 },
    { 17, "91197", (int64_t)81*1024*1024*1024*1024*1024L, 0, 0 },
    { 18, "-9119", -(int64_t)81*1024*1024*1024*1024*1024L, 0, 0 },

    /* 0 scale, divisor 1000 */
    /* XXX - why does this fail? */
    { -1, "", INT64_MAX, BU_HN_DIVISOR_1000, 0 },
    { 20, "-9223", -INT64_MAX, BU_HN_DIVISOR_1000,  0 },
    { 18, "10358", (int64_t)92*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  0 },
    { 19, "-1035", -(int64_t)92*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  0 },
    { 17, "92323", (int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  0 },
    { 18, "-9232", -(int64_t)82*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  0 },
    /* Expected to pass */
    { 17, "91197", (int64_t)81*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  0 },
    { 18, "-9119", -(int64_t)81*1024*1024*1024*1024*1024L, BU_HN_DIVISOR_1000,  0 },



    /* Need to implement tests for GETSCALE */
    /*	{ ?, "", (int64_t)0L, BU_HN_DIVISOR_1000, BU_HN_GETSCALE },
	...
    */
    /* Tests for BU_HN_DECIMAL */
    /* Positive, Autoscale */
    { 5, "500 k", (int64_t)500*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "994 k", (int64_t)994*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "995 k", (int64_t)995*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "999 k", (int64_t)999*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "1.0 M", (int64_t)1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "1.5 M", (int64_t)1500*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "1.9 M", (int64_t)1949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "2.0 M", (int64_t)1950*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "9.9 M", (int64_t)9949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 4, "10 M", (int64_t)9950*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "500 M", (int64_t)500*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "994 M", (int64_t)994*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "995 M", (int64_t)995*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "999 M", (int64_t)999*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },

    { 5, "500 K", (int64_t)500*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "994 K", (int64_t)994*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "995 K", (int64_t)995*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "999 K", (int64_t)999*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.0 M", (int64_t)1000*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.0 M", (int64_t)1018*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.0 M", (int64_t)1019*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.5 M", (int64_t)1536*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.9 M", (int64_t)1996*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "2.0 M", (int64_t)1997*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "2.0 M", (int64_t)2047*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "2.0 M", (int64_t)2048*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "2.0 M", (int64_t)2099*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "2.1 M", (int64_t)2100*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "9.9 M", (int64_t)10188*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    /* XXX - shouldn't the following two be "10. M"? */
    { 4, "10 M", (int64_t)10189*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 4, "10 M", (int64_t)10240*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "500 M", (int64_t)500*1024*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "994 M", (int64_t)994*1024*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "995 M", (int64_t)995*1024*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "999 M", (int64_t)999*1024*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.0 G", (int64_t)1000*1024*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "1.0 G", (int64_t)1023*1024*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },

    /* Negative, Autoscale - should pass */
    { 6, "-1.5 ", -(int64_t)1500*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 6, "-1.9 ", -(int64_t)1949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 6, "-9.9 ", -(int64_t)9949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },

    { 6, "-1.5 ", -(int64_t)1536*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 6, "-1.9 ", -(int64_t)1949*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 6, "-9.7 ", -(int64_t)9949*1024L, BU_HN_DECIMAL, BU_HN_AUTOSCALE },

    /* Positive/negative, at maximum scale */
    { 5, "500 P", (int64_t)500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "1.9 E", (int64_t)1949*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "8.9 E", (int64_t)8949*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 5, "9.2 E", INT64_MAX, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    /* Negatives work with latest rev only: */
    { 6, "-9.2 ", -INT64_MAX, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },
    { 6, "-8.9 ", -(int64_t)8949*1000*1000*1000*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, BU_HN_AUTOSCALE },

    { 5, "8.0 E",   INT64_MAX, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 5, "7.9 E",   INT64_MAX-(int64_t)100*1024*1024*1024*1024*1024LL, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 6, "-8.0 ", -INT64_MAX, BU_HN_DECIMAL, BU_HN_AUTOSCALE },
    { 6, "-7.9 ",   -INT64_MAX+(int64_t)100*1024*1024*1024*1024*1024LL, BU_HN_DECIMAL, BU_HN_AUTOSCALE },

    /* Positive, Fixed scales */
    { 5, "500 k", (int64_t)500*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 1 },
    { 5, "0.5 M", (int64_t)500*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "949 k", (int64_t)949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 1 },
    { 5, "0.9 M", (int64_t)949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "950 k", (int64_t)950*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 1 },
    { 5, "1.0 M", (int64_t)950*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "999 k", (int64_t)999*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 1 },
    { 5, "1.0 M", (int64_t)999*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "1.5 M", (int64_t)1500*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "1.9 M", (int64_t)1949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "2.0 M", (int64_t)1950*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "9.9 M", (int64_t)9949*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 4, "10 M", (int64_t)9950*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "500 M", (int64_t)500*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "0.5 G", (int64_t)500*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 3 },
    { 5, "999 M", (int64_t)999*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 2 },
    { 5, "1.0 G", (int64_t)999*1000*1000L, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 3 },
    /* Positive/negative, at maximum scale */
    { 5, "500 P", (int64_t)500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 5 },
    { 5, "1.0 E", (int64_t)500*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },
    { 5, "1.9 E", (int64_t)1949*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },
    { 5, "8.9 E", (int64_t)8949*1000*1000*1000*1000*1000L, BU_HN_DIVISOR_1000, 6 },
    { 5, "9.2 E", INT64_MAX, BU_HN_DECIMAL|BU_HN_DIVISOR_1000, 6 },

    /* BU_HN_DECIMAL + binary + fixed scale cases not completed */
    { 5, "512 K", (int64_t)512*1024L, BU_HN_DECIMAL, 1 },
    { 5, "0.5 M", (int64_t)512*1024L, BU_HN_DECIMAL, 2 },

    /* Negative, Fixed scales */
    /* Not yet added, but should work with latest rev */

};


/* Command line options usage */
static void
usage(char * progname) {
    printf("%s: tests libutil humanize_number function\n", progname);
    printf("Usage: %s [-nE] [-l num] [-v]\n\n", progname);
    printf("Options:\n");
    printf("\t-l num\tSet max length for result; buflen = num + 1\n");
    printf("\t\t  (NOTE: does not change expected result strings.)\n");
    printf("\t-n\tInclude negative scale tests, which cause older libutil\n");
    printf("\t\t  version of function to coredump with assertion failure\n");
    printf("\t-E\tInclude numbers > 1/2 Exa[byte] which currently fail\n");
    printf("\t-v\tVerbose - always print summary results\n");
    printf("\t-h, -?\tShow options\n");
}

/* Parse command line options */
static void
read_options(int argc, char * const argv[], size_t *bufLength,
	     int *includeNegativeScale, int *includeExabytes, int *verbose) {
    int ch;
    size_t temp;

    while ((ch = bu_getopt(argc, argv, "nEh?vl:")) != -1) {
	switch (ch) {
	    default:
		usage(argv[0]);
		bu_exit(1, NULL);
		break;	/* UNREACHABLE */
	    case 'h' :
	    case '?' :
		usage(argv[0]);
		bu_exit(0, NULL);
		break;	/* UNREACHABLE */
	    case 'l' :
		bu_sscanf(bu_optarg, "%zu", &temp);
		*bufLength = temp + 1;
		break;
	    case 'n' :
		*includeNegativeScale = 1;
		break;
	    case 'E' :
		*includeExabytes = 1;
		break;
	    case 'v' :
		*verbose = 1;
		break;
	}
    }
}

static struct {
    int value;
    const char *name;
} flags[] = {
    { BU_HN_AUTOSCALE, "BU_HN_AUTOSCALE" },
    { BU_HN_GETSCALE, "BU_HN_GETSCALE" },
    { BU_HN_DIVISOR_1000, "BU_HN_DIVISOR_1000"},
    { BU_HN_B, "BU_HN_B"},
    { BU_HN_DECIMAL, "BU_HN_DECIMAL"},
};

static const char *separator = "|";

/* Format flags parameter for meaningful display */
static char *
str_flags(int hn_flags, char *noFlags) {
    size_t i;
    char * result;

    result = (char *)bu_malloc(MAX_STR_FLAGS_RESULT, "result");
    result[0] = '\0';

    for (i = 0; i < sizeof flags / sizeof *flags; i++) {
	if (hn_flags & flags[i].value) {
	    if (*result != 0)
		bu_strlcat(result, separator,
			   MAX_STR_FLAGS_RESULT);
	    bu_strlcat(result, flags[i].name, MAX_STR_FLAGS_RESULT);
	}
    }

    if (strlen(result) == 0)
	bu_strlcat(result, noFlags, MAX_STR_FLAGS_RESULT);
    return result;
}


/* Format scale parameter for meaningful display */
static char *
str_scale(int scale) {
    char *result;

    if (scale == BU_HN_AUTOSCALE || scale == BU_HN_GETSCALE)
	return str_flags(scale, "");

    result = (char *)bu_malloc(MAX_INT_STR_DIGITS, NULL);
    result[0] = '\0';
    snprintf(result, MAX_INT_STR_DIGITS, "%d", scale);
    return result;
}

static void
testskipped(size_t i)
{

    printf("ok %lu # skip - not turned on\n", (long unsigned int)i);
}

int
main(int argc, char *argv[])
{
    char *buf;
    char *flag_str, *scale_str;
    size_t i;
    size_t errcnt, tested, skipped;
    int r;
    size_t buflen;
    int includeNegScale;
    int includeExabyteTests;
    int verbose;

    bu_setprogname(argv[0]);

    buflen = 4;
    includeNegScale = 0;
    includeExabyteTests = 0;
    verbose = 0;

    read_options(argc, argv, &buflen, &includeNegScale,
		 &includeExabyteTests, &verbose);

    buf = (char *)bu_malloc(buflen, NULL);
    errcnt = 0;
    tested = 0;
    skipped = 0;

    if (buflen != 4)
	bu_log("Warning: buffer size %zu != 4, expect some results to differ.\n", buflen);

    printf("1..%lu\n", (long unsigned int)(sizeof test_args / sizeof *test_args));
    for (i = 0; i < sizeof test_args / sizeof *test_args; i++) {
	/* KLUDGE */
	if (test_args[i].num == INT64_MAX && buflen == 4) {
	    /* Start final tests which require buffer of 6 */
	    bu_free(buf, NULL);
	    buflen = 6;
	    buf = (char *)bu_malloc(buflen, NULL);
	    if (verbose)
		bu_log("Buffer length increased to %zu\n", buflen);
	}

	if (test_args[i].scale < 0 && ! includeNegScale) {
	    skipped++;
	    testskipped(i);
	    continue;
	}
	if (test_args[i].num >= halfExabyte && ! includeExabyteTests) {
	    skipped++;
	    testskipped(i);
	    continue;
	}

	r = bu_humanize_number(buf, buflen, test_args[i].num, "",
			       test_args[i].scale, test_args[i].flags);
	flag_str = str_flags(test_args[i].flags, "[no flags]");
	scale_str = str_scale(test_args[i].scale);

	if (r != test_args[i].retval) {
	    if (verbose)
		bu_log("wrong return value on index %lu, buflen: %zu, got: %d + \"%s\", expected %d + \"%s\"; num = %" PRId64 ", scale = %s, flags= %s.\n",
		       i, buflen, r, buf, test_args[i].retval,
		       test_args[i].res, test_args[i].num,
		       scale_str, flag_str);
	    else
		printf("not ok %lu # return %d != %d\n", (long unsigned int)i, r,
		       test_args[i].retval);
	    errcnt++;
	} else if (bu_strcmp(buf, test_args[i].res) != 0) {
	    if (verbose)
		printf("result mismatch on index %lu, got: \"%s\", expected \"%s\"; num = %" PRId64 ", scale = %s, flags= %s.\n",
		       (long unsigned int)i, buf, test_args[i].res, test_args[i].num,
		       scale_str, flag_str);
	    else
		printf("not ok %lu # buf \"%s\" != \"%s\"\n", (long unsigned int)i,
		       buf, test_args[i].res);
	    errcnt++;
	} else {
	    if (verbose)
		printf("successful result on index %lu, returned %d, got: \"%s\"; num = %" PRId64 ", scale = %s, flags= %s.\n",
		       (long unsigned int)i, r, buf, test_args[i].num, scale_str,
		       flag_str);
	    else
		printf("ok %lu\n", (long unsigned int)i);
	}
	tested++;
    }

    if (verbose)
	printf("total errors: %lu/%lu tests, %lu skipped\n", (long unsigned int)errcnt,
	       (long unsigned int)tested, (long unsigned int)skipped);

    if (errcnt)
	return 1;

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

