/*                       B U _ D A T E - T I M E . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
main(int argc, char **argv)
{
    struct bu_vls result = BU_VLS_INIT_ZERO;
    int64_t time;
    int function_num;
    if (argc != 2)
	bu_exit(1, "ERROR: wrong number of parameters");
    sscanf(argv[1], "%d", &function_num);
    switch (function_num) {
	case 1:
	    time = 1087449261LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2004-06-17T05:14:21Z"))
		return 1;
	    break;
	case 2:
	    time = 631152000LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1990-01-01T00:00:00Z"))
		return 1;
	    break;
	case 3:
	    time = 936860949LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1999-09-09T07:09:09Z"))
		return 1;
	    break;
	case 4:
	    time = 1388696601LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2014-01-02T21:03:21Z"))
		return 1;
	    break;
	case 5:
	    time = 0LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:00Z"))
		return 1;
	    break;
	case 6:
	    time = 1LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:01Z"))
		return 1;
	    break;
	case 7:
	    time = 1431482805LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2015-05-13T02:06:45Z"))
		return 1;
	    break;
	case 8:
	    time = 2147483647LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:07Z"))
		return 1;
	    break;
	case 9:
	    time = 2147483649LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:09Z"))
		return 1;
	    break;
	case 10:
	    time = 3147483649LL;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2069-09-27T05:00:49Z"))
		return 1;
	    break;

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
