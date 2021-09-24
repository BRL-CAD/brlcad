/* bu_humanize_number is derived from FreeBSD's libutil implementation of
 * humanize_number.c, v 1.14 2008/04/28
 *
 * Copyright (c) 1997, 1998, 1999, 2002 The NetBSD Foundation, Inc.
 * Copyright 2013 John-Mark Gurney <jmg@FreeBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Luke Mewburn and by Tomas Svensson.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include "common.h"

#ifdef HAVE_INTTYPES_H
#  if defined(__cplusplus)
#  define __STDC_FORMAT_MACROS
#  endif
#  include <inttypes.h>
#else
#  include "pinttypes.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include "vmath.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/units.h"
#include "bu/vls.h"

#define hn_maxscale 6

int
bu_humanize_number(char *buf, size_t len, int64_t quotient, const char *suffix, size_t scale, int flags)
{
    struct bu_vls tmpbuf = BU_VLS_INIT_ZERO;
    const char *prefixes, *sep;
    size_t i;
    size_t r;
    int leftover, s1, s2, sign;
    int	divisordeccut;
    int64_t	divisor, max;
    size_t	baselen;

    /* Since so many callers don't check -1, NUL terminate the buffer */
    if (len > 0)
	buf[0] = '\0';

    /* validate args */
    if (buf == NULL || suffix == NULL)
	return (-1);
    else if (scale > hn_maxscale &&
	     ((scale & ~(BU_HN_AUTOSCALE|BU_HN_GETSCALE)) != 0))
	return (-1);
    if ((flags & BU_HN_DIVISOR_1000) && (flags & BU_HN_IEC_PREFIXES))
	return (-1);

    /* setup parameters */
    leftover = 0;

    if (flags & BU_HN_IEC_PREFIXES) {
	baselen = 2;
	/*
	 * Use the prefixes for power of two recommended by
	 * the International Electrotechnical Commission
	 * (IEC) in IEC 80000-3 (i.e. Ki, Mi, Gi...).
	 *
	 * BU_HN_IEC_PREFIXES implies a divisor of 1024 here
	 * (use of BU_HN_DIVISOR_1000 would have triggered
	 * an assertion earlier).
	 */
	divisor = 1024;
	divisordeccut = 973;	/* ceil(.95 * 1024) */
	if (flags & BU_HN_B)
	    prefixes = "B\0\0Ki\0Mi\0Gi\0Ti\0Pi\0Ei";
	else
	    prefixes = "\0\0\0Ki\0Mi\0Gi\0Ti\0Pi\0Ei";
    } else {
	baselen = 1;
	if (flags & BU_HN_DIVISOR_1000) {
	    divisor = 1000;
	    divisordeccut = 950;
	    if (flags & BU_HN_B)
		prefixes = "B\0\0k\0\0M\0\0G\0\0T\0\0P\0\0E";
	    else
		prefixes = "\0\0\0k\0\0M\0\0G\0\0T\0\0P\0\0E";
	} else {
	    divisor = 1024;
	    divisordeccut = 973;	/* ceil(.95 * 1024) */
	    if (flags & BU_HN_B)
		prefixes = "B\0\0K\0\0M\0\0G\0\0T\0\0P\0\0E";
	    else
		prefixes = "\0\0\0K\0\0M\0\0G\0\0T\0\0P\0\0E";
	}
    }

#define	SCALE2PREFIX(scale)	(&prefixes[(scale) * 3])

    if (quotient < 0) {
	sign = -1;
	quotient = -quotient;
	baselen += 2;		/* sign, digit */
    } else {
	sign = 1;
	baselen += 1;		/* digit */
    }
    if (flags & BU_HN_NOSPACE)
	sep = "";
    else {
	sep = " ";
	baselen++;
    }
    baselen += strlen(suffix);

    /* Check if enough room for `x y' + suffix + `\0' */
    if (len < baselen + 1)
	return (-1);

    if (scale & (BU_HN_AUTOSCALE | BU_HN_GETSCALE)) {
	/* See if there is additional columns can be used. */
	for (max = 1, i = len - baselen; i-- > 0;)
	    max *= 10;

	/*
	 * Divide the number until it fits the given column.
	 * If there will be an overflow by the rounding below,
	 * divide once more.
	 */
	for (i = 0;
	     (quotient >= max || (quotient == max - 1 &&
				  leftover >= divisordeccut)) && i < hn_maxscale; i++) {
	    leftover = quotient % divisor;
	    quotient /= divisor;
	}

	if (scale & BU_HN_GETSCALE)
	    return (int)i;
    } else {
	for (i = 0; i < scale && i < hn_maxscale; i++) {
	    leftover = quotient % divisor;
	    quotient /= divisor;
	}
    }

    /* If a value <= 9.9 after rounding and ... */
    /*
     * XXX - should we make sure there is enough space for the decimal
     * place and if not, don't do BU_HN_DECIMAL?
     */
    if (((quotient == 9 && leftover < divisordeccut) || quotient < 9) && i > 0 && flags & BU_HN_DECIMAL) {
	size_t rcpy = 0;
	s1 = (int)quotient + ((leftover * 10 + divisor / 2) / divisor / 10);
	s2 = ((leftover * 10 + divisor / 2) / divisor) % 10;
	bu_vls_sprintf(&tmpbuf, "%d%s%d%s%s%s", sign * s1, ".", s2, sep, SCALE2PREFIX(i), suffix);
	bu_vls_trimspace(&tmpbuf);
	r = bu_vls_strlen(&tmpbuf);
	rcpy = FMIN(r + 1, len);
	bu_strlcpy(buf, bu_vls_addr(&tmpbuf), rcpy);
	bu_vls_free(&tmpbuf);
	buf[len-1] = '\0';
    } else {
	size_t rcpy = 0;
	bu_vls_sprintf(&tmpbuf, "%" PRId64 "%s%s%s", sign * (quotient + (leftover + divisor / 2) / divisor), sep, SCALE2PREFIX(i), suffix);
	bu_vls_trimspace(&tmpbuf);
	r = bu_vls_strlen(&tmpbuf);
	rcpy = FMIN(r + 1, len);
	bu_strlcpy(buf, bu_vls_addr(&tmpbuf), rcpy);
	bu_vls_free(&tmpbuf);
	buf[len-1] = '\0';
    }

    return (int)r;
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
