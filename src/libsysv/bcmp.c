/*                          B C M P . C
 * BRL-CAD
 *
 * This work is in the public domain.
 *
 **/
/** @file bcmp.c
 *
 * Compares two regions of memory.
 *
 */

#include "common.h"

/* quell empty-compilation unit warnings */
static const int unused = 0;

/*
 * defined for folks that don't have a system bcmp()
 */
#ifndef HAVE_BCMP
#include "sysv.h"

int
bcmp(register const void *b1, register const void *b2, size_t len)
{
    int ret = 0;

    while (len-- > 0) {
	if (*(const char *)(b1)++ != *(const char *)(b2)++) {
	    ret = 1;
	    break;
	}
    }

    return ret;
}

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
