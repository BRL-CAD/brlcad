/*                    D A T E T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "bu.h"


int
main(int argc, char *argv[])
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    int64_t curr_time;
    int function_num;

    bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_log("Usage: %s {function_num}\n", argv[0]);
	bu_exit(1, "ERROR: wrong number of parameters");
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 0:	{
	    int64_t time0 = 0;
	    int64_t time1 = 0;
	    int64_t time2 = 0;
	    int64_t i = 0;
	    size_t counter = 1;

	    time0 = bu_gettime();
	    bu_snooze(BU_SEC2USEC(1));
	    time1 = bu_gettime();

	    if (time1 - time0 <= 0)
		bu_exit(1, "ERROR: We went back in time!\n");

	    /* iterate for exactly 1 second */
	    while (i < 1.0e6 && counter < 1.0e15) {
		counter++;
		time2 = bu_gettime();
		i = time2 - time1;
	    }

	    if (time2 - time1 <= 0)
		bu_exit(1, "ERROR: We looped back in time!\n");

#if 0
	    bu_log("Called bu_gettime() %zu times\n", counter);
	    bu_log("Time1: %llu\n", (unsigned long)time1);
	    bu_log("Time2: %llu\n", (unsigned long)time2);
	    bu_log("Time delta (Time2-Time1): %d\n", i);
#endif

	    return 0;
	}
	case 1:
	    curr_time = 1087449261LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2004-06-17T05:14:21Z"))
		return 1;
	    break;
	case 2:
	    curr_time = 631152000LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1990-01-01T00:00:00Z"))
		return 1;
	    break;
	case 3:
	    curr_time = 936860949LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1999-09-09T07:09:09Z"))
		return 1;
	    break;
	case 4:
	    curr_time = 1388696601LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2014-01-02T21:03:21Z"))
		return 1;
	    break;
	case 5:
	    curr_time = 0LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:00Z"))
		return 1;
	    break;
	case 6:
	    curr_time = 1LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:01Z"))
		return 1;
	    break;
	case 7:
	    curr_time = 1431482805LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2015-05-13T02:06:45Z"))
		return 1;
	    break;
	case 8:
	    curr_time = 2147483647LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:07Z"))
		return 1;
	    break;
	case 9:
	    curr_time = 2147483649LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:09Z"))
		return 1;
	    break;
	case 10:
	    curr_time = 3147483649LL;
	    bu_utctime(&result, curr_time);
	    if (!BU_STR_EQUAL(result.vls_str, "2069-09-27T05:00:49Z"))
		return 1;
	    break;
#if 0
	case 11:
	    {
		/* Per POSIX and Microsoft's docs, time should return the time
		 * as seconds elapsed since the POSIX Epoch (midnight, January
		 * 1, 1970).  Since bu_utctime is assuming a time offset from
		 * the epoch, check that bu_gettime and time are more or less
		 * on the same page. */
		struct bu_vls result1 = BU_VLS_INIT_ZERO;
		struct bu_vls result2 = BU_VLS_INIT_ZERO;
		time_t t = time(NULL);
		int64_t t_since_epoc_systime = (int64_t)t * 1.0e6;
		int64_t t_since_epoc_gettime = bu_gettime();
		if (llabs((long long)(t_since_epoc_gettime - t_since_epoc_systime)) > 1.0e6) {
		    bu_exit(1, "ERROR: bu_gettime(%lld) and time(%lld) disagree by > 1.0e6", (long long int)t_since_epoc_gettime, (long long int)t_since_epoc_systime);
		}
		/* If we got this far, bu_utctime should give us the same
		 * result - probably redundant to do so given the numerical
		 * comparison above, but go ahead and make sure the strings
		 * check out as equal. */
		bu_utctime(&result1, t_since_epoc_gettime/1.0e6);
		bu_utctime(&result2, t_since_epoc_systime/1.0e6);
		if (!BU_STR_EQUAL(bu_vls_cstr(&result1), bu_vls_cstr(&result2))) {
		    bu_exit(1, "ERROR: bu_gettime(%s) and time(%s) bu_utctime strings differ", bu_vls_cstr(&result1), bu_vls_cstr(&result2));
		}
		bu_vls_free(&result1);
		bu_vls_free(&result2);
		return 0;
	    }
#endif
    }
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
