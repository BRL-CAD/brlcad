#include "bu.h"
#include "stdio.h"

int
main(int argc, char **argv)
{
    struct bu_vls result;
    int64_t time;
    int function_num;
    time = 2147483647;
    bu_vls_init(&result);
    bu_utctime(&result, time);
    printf("%s", result.vls_str);
    if (argc != 2)
	bu_exit(1, "ERROR: wrong number of parameters");
    sscanf(argv[1], "%d", &function_num);
    switch (function_num) {
	case 1:
	    time = 1087449261;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2004-06-17T05:14:21Z"))
		return 1;
	    break;
	case 2:
	    time = 631152000;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1990-01-01T00:00:00Z"))
		return 1;
	    break;
	case 3:
	    time = 936860949;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1999-09-09T07:09:09Z"))
		return 1;
	    break;
	case 4:
	    time = 1388696601;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2014-01-02T21:03:21Z"))
		return 1;
	    break;
	case 5:
	    time = 0;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:00Z"))
		return 1;
	    break;
	case 6:
	    time = 1;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "1970-01-01T00:00:01Z"))
		return 1;
	    break;
	case 7:
	    time = 1431482805;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2015-05-13T02:06:45Z"))
		return 1;
	    break;
	case 8:
	    time = 2147483647;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:07Z"))
		return 1;
	    break;
	case 9:
	    time = 2147483649;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2038-01-19T03:14:09Z"))
		return 1;
	    break;
	case 10:
	    time = 3147483649;
	    bu_utctime(&result, time);
	    if(!BU_STR_EQUAL(result.vls_str, "2069-09-27T05:00:49Z"))
		return 1;
	    break;

    }
    bu_utctime(&result, time);
    printf("%s", result.vls_str);
    return 0;
}
