/*                         U N I T S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2018 United States Government as represented by
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

/* strict c89 doesn't declare strtoll() */
#if defined(HAVE_STRTOLL) && !defined(HAVE_DECL_STRTOLL) && !defined(__cplusplus)
extern long long int strtoll(const char *nptr, char **endptr, int base);
#endif

#if !defined(LLONG_MAX) && defined(__LONG_LONG_MAX__)
#  define LLONG_MAX __LONG_LONG_MAX__
#endif
#if !defined(LLONG_MIN) && defined(__LONG_LONG_MAX__)
#  define LLONG_MIN (-__LONG_LONG_MAX__-1LL)
#endif

struct cvt_tab {
    double val;
    char name[32];
};

struct conv_table {
    struct cvt_tab *cvttab;
};

static struct cvt_tab bu_units_length_tab[] = {
    {0.0,		"none"},
    {1.0e-21,		"ym"},
    {1.0e-21,		"yoctometer"},
    {1.0e-18,		"zm"},
    {1.0e-18,		"zeptometer"},
    {1.0e-15,		"am"},
    {1.0e-15,		"attometer"},
    {1.0e-12,		"fm"},
    {1.0e-12,		"femtometer"},
    {1.0e-9,		"pm"},
    {1.0e-9,		"picometer"},
    {1.0e-7,		"angstrom"},
    {1.0e-7,		"decinanometer"},
    {1.0e-6,		"nm"},
    {1.0e-6,		"nanometer"},
    {1.0e-3,		"um"},
    {1.0e-3,		"micrometer"},
    {1.0e-3,		"micron"},
    {1.0,		"mm"},
    {1.0,		"millimeter"},
    {10.0,		"cm"},
    {10.0,		"centimeter"},
    {100.0,		"dm"},
    {100.0,		"decimeter"},
    {1000.0,		"m"},
    {1000.0,		"meter"},
    {10000.0,		"Dm"},
    {10000.0,		"decameter"},
    {100000.0,		"hm"},
    {100000.0,		"hectometer"},
    {1000000.0,		"km"},
    {1000000.0,		"kilometer"},
    {1.0e+9,		"Mm"},
    {1.0e+9,		"megameter"},
    {1.0e+12,		"Gm"},
    {1.0e+12,		"gigameter"},
    {1.0e+15,		"Tm"},
    {1.0e+15,		"terameter"},
    {1.0e+18,		"Pm"},
    {1.0e+18,		"petameter"},
    {1.0e+21,		"Em"},
    {1.0e+21,		"exameter"},
    {1.0e+24,		"Zm"},
    {1.0e+24,		"zettameter"},
    {1.0e+27,		"Ym"},
    {1.0e+27,		"yottameter"},
    {25.4,		"in"},
    {25.4,		"inch"},
    {25.4,		"inches"}, /* plural */
    {101.6,		"hand"},
    {304.8,		"ft"},
    {304.8,		"foot"},
    {304.8,		"feet"}, /* plural */
    {456.2,		"cubit"},
    {914.4,		"yd"},
    {914.4,		"yard"},
    {5029.2,		"rd"},
    {5029.2,		"rod"},
    {20116.8,		"chain"},
    {201168.0,		"furlong"},
    {1609344.0,		"mi"},
    {1609344.0,		"mile"},
    {1852000.0,		"nmile"},
    {1852000.0,		"nautical mile"},
    {5556000.0,		"league"},
    {2.99792458e+11,	"light second"},
    {1.79875475e+13,	"light minute"},
    {1.495979e+14,	"AU"},
    {1.495979e+14,	"astronomical unit"},
    {1.07925285e+15,	"light hour"},
    {2.59020684e+16,	"light day"},
    {9.4605284e+18,	"light year"},
    {3.08568025e+19,	"pc"},
    {3.08568025e+19,	"parsec"},
    {0.0,		""}			/* LAST ENTRY */
};
#define BU_UNITS_TABLE_SIZE (sizeof(bu_units_length_tab) / sizeof(struct cvt_tab) - 1)

static struct cvt_tab bu_units_volume_tab[] = {
    {0.0,		"none"},
    {1.0,		"mm^3"},		/* default */
    {1.0, 		"cu mm"},
    {1.0e3, 		"cm^3"},
    {1.0e3, 		"cu cm"},
    {1.0e3, 		"cc"},
    {1.0e6, 		"l"},
    {1.0e6, 		"liter"},
    {1.0e6, 		"litre"},
    {1.0e9, 		"m^3"},
    {1.0e9,		"cu m"},
    {16387.064, 	"in^3"},
    {16387.064, 	"cu in"},
    {28316846.592, 	"ft^3"},
    {28316846.592, 	"cu ft"},
    {764554857.984, 	"yds^3"},
    {764554857.984, 	"yards^3"},
    {764554857.984, 	"cu yards"},
    {0.0,               ""}                     /* LAST ENTRY */
};

static struct cvt_tab bu_units_mass_tab[] = {
    {0.0,		"none"},
    {1.0,		"grams"},		/* default */
    {1.0, 		"g"},
    {1.0e3, 		"kilogram"},
    {1.0e3,		"kg"},
    {0.0648, 		"gr"},
    {0.0648,		"grain"},
    {453.6,		"lb"},
    {28.35,		"oz"},
    {28.35,		"ounce"},
    {0.0,               ""}                     /* LAST ENTRY */
};

static const struct conv_table unit_lists[4] = {
    {bu_units_length_tab}, {bu_units_volume_tab}, {bu_units_mass_tab}, {NULL}
};


/**
 * compares an input units string to a reference units name and
 * returns truthfully if they match.  the comparison ignores any
 * embedded whitespace and is case insensitive.
 */
static int
units_name_matches(const char *input, const char *name)
{
    const char *cp;
    int match;
    struct bu_vls normalized_input = BU_VLS_INIT_ZERO;
    struct bu_vls normalized_name = BU_VLS_INIT_ZERO;

    /* convert NULL */
    if (!input)
	input = "";
    if (!name)
	name = "";

    /* skip spaces */
    while (isspace((unsigned char)*input))
	input++;
    while (isspace((unsigned char)*name))
	name++;

    /* quick exit */
    if (tolower((unsigned char)input[0]) != tolower((unsigned char)name[0]))
	return 0;

    cp = input;
    /* skip spaces, convert to lowercase */
    while (*cp != '\0') {
	if (!isspace((unsigned char)*cp))
	    bu_vls_putc(&normalized_input, tolower((unsigned char)*cp));
	cp++;
    }

    cp = name;
    /* skip spaces, convert to lowercase */
    while (*cp != '\0') {
	if (!isspace((unsigned char)*cp))
	    bu_vls_putc(&normalized_name, tolower((unsigned char)*cp));
	cp++;
    }

    /* trim any trailing 's' for plurality */
    if (bu_vls_addr(&normalized_input)[bu_vls_strlen(&normalized_input)-1] == 's') {
	bu_vls_trunc(&normalized_input, -1);
    }
    if (bu_vls_addr(&normalized_name)[bu_vls_strlen(&normalized_name)-1] == 's') {
	bu_vls_trunc(&normalized_name, -1);
    }

    /* compare */
    match = BU_STR_EQUAL(bu_vls_addr(&normalized_input), bu_vls_addr(&normalized_name));

    bu_vls_free(&normalized_input);
    bu_vls_free(&normalized_name);

    return match;
}


double
bu_units_conversion(const char *str)
{
    register const struct cvt_tab *tp;
    register const struct conv_table *cvtab;
    char ubuf[256];

    /* Copy the given string */
    bu_strlcpy(ubuf, str, sizeof(ubuf));

    /* Search for this string in the table */
    for (cvtab=unit_lists; cvtab->cvttab; cvtab++) {
	for (tp=cvtab->cvttab; tp->name[0]; tp++) {
	    if (!units_name_matches(ubuf, tp->name))
		continue;
	    return tp->val;
	}
    }
    return 0.0;		/* Unable to find it */
}


const char *
bu_units_string(register const double mm)
{
    register const struct cvt_tab *tp;

    if (UNLIKELY(mm <= 0))
	return (const char *)NULL;

    /* Search for this string in the table */
    for (tp=bu_units_length_tab; tp->name[0]; tp++) {
	fastf_t diff, bigger;

	if (ZERO(mm - tp->val))
	    return tp->name;

	/* Check for near-miss */
	if (mm > tp->val) {
	    bigger = mm;
	    diff = mm - tp->val;
	} else {
	    bigger = tp->val;
	    diff = tp->val - mm;
	}

	/* Absolute difference less than 0.1 angstrom */
	if (diff < 1.0e-8)
	    return tp->name;

	/* Relative difference less than 1 part per billion */
	if (diff < 0.000000001 * bigger)
	    return tp->name;
    }
    return (const char *)NULL;
}

struct bu_vls *
bu_units_strings_vls()
{
    register const struct cvt_tab *tp;
    struct bu_vls *vlsp;
    double prev_val = 0.0;

    BU_ALLOC(vlsp, struct bu_vls);
    bu_vls_init(vlsp);
    for (tp=bu_units_length_tab; tp->name[0]; tp++) {
	if (ZERO(prev_val - tp->val))
	    continue;

	bu_vls_printf(vlsp, "%s, ", tp->name);
	prev_val = tp->val;
    }

    /* Remove the last ", " */
    bu_vls_trunc(vlsp, -2);

    return vlsp;
}


const char *
bu_nearest_units_string(register const double mm)
{
    register const struct cvt_tab *tp;

    const char *nearest = NULL;
    double nearer = DBL_MAX;

    if (UNLIKELY(mm <= 0))
	return (const char *)NULL;

    /* Search for this unit in the table */
    for (tp=bu_units_length_tab; tp->name[0]; tp++) {
	double nearness;

	/* skip zero so we don't return 'none' */
	if (ZERO(tp->val))
	    continue;

	/* break early on perfect match */
	if (ZERO(mm - tp->val))
	    return tp->name;

	/* Check for nearness */
	if (mm > tp->val) {
	    nearness = mm - tp->val;
	} else {
	    nearness = tp->val - mm;
	}

	/* :-) */
	if (nearness < nearer) {
	    nearer = nearness;
	    nearest = tp->name;
	}
    }
    return nearest;
}


double
bu_mm_value(const char *s)
{
    double v;
    char *ptr;
    register const struct cvt_tab *tp;

    v = strtod(s, &ptr);

    if (ptr == s) {
	/* No number could be found, unity is implied */
	/* e.g. interpret "ft" as "1ft" */
	v = 1.0;
    }
    if (! *ptr) {
	/* There are no characters following the scanned number */
	return v;
    }

    for (tp=bu_units_length_tab; tp->name[0]; tp++) {
	if (units_name_matches(ptr, tp->name)) {
	    v *= tp->val;
	    return v;
	}
    }

    /* A string was seen, but not found in the table.  Signal error */
    return -1.0;
}


void
bu_mm_cvt(const struct bu_structparse *sdp,
	  const char *name,
	  void *base,
	  const char *value,
	  void *UNUSED(data))
/* structure description */
/* struct member name */
/* beginning of structure */
/* string containing value */
{
    register double *p = (double *)((char *)base + sdp->sp_offset);

    if (UNLIKELY(!name)) {
	bu_log("bu_mm_cvt: NULL name encountered\n");
    }

    /* reconvert with optional units */
    *p = bu_mm_value(value);
}

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

#define hn_maxscale 7

int
bu_humanize_number(char *buf, size_t len, int64_t quotient,
	const char *suffix, int scale, int flags)
{
    struct bu_vls tmpbuf = BU_VLS_INIT_ZERO;
    const char *prefixes, *sep;
    int	i, r, leftover, s1, s2, sign;
    int	divisordeccut;
    int64_t	divisor, max;
    size_t	baselen;

    /* Since so many callers don't check -1, NUL terminate the buffer */
    if (len > 0)
	buf[0] = '\0';

    /* validate args */
    if (buf == NULL || suffix == NULL)
	return (-1);
    if (scale < 0)
	return (-1);
    else if (scale >= hn_maxscale &&
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
	    return (i);
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
	int rcpy = 0;
	s1 = (int)quotient + ((leftover * 10 + divisor / 2) / divisor / 10);
	s2 = ((leftover * 10 + divisor / 2) / divisor) % 10;
	bu_vls_sprintf(&tmpbuf, "%d%s%d%s%s%s", sign * s1, ".", s2, sep, SCALE2PREFIX(i), suffix);
	bu_vls_trimspace(&tmpbuf);
	r = bu_vls_strlen(&tmpbuf);
	rcpy = r + 1 > (int)len ? (int)len : r + 1;
	bu_strlcpy(buf, bu_vls_addr(&tmpbuf), rcpy);
	bu_vls_free(&tmpbuf);
	buf[len-1] = '\0';
    } else {
	int rcpy = 0;
	bu_vls_sprintf(&tmpbuf, "%" PRId64 "%s%s%s", sign * (quotient + (leftover + divisor / 2) / divisor), sep, SCALE2PREFIX(i), suffix);
	bu_vls_trimspace(&tmpbuf);
	r = bu_vls_strlen(&tmpbuf);
	rcpy = r + 1 > (int)len ? (int)len : r + 1;
	bu_strlcpy(buf, bu_vls_addr(&tmpbuf), rcpy);
	bu_vls_free(&tmpbuf);
	buf[len-1] = '\0';
    }

    return (r);
}

/* bu_dehumanize_number is derived from NetBSD's implementation:
 * NetBSD: dehumanize_number.c,v 1.4 2012/03/13
 *
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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

/*
 * Converts the number given in 'str', which may be given in a humanized
 * form (as described in humanize_number(3), but with some limitations),
 * to an int64_t without units.
 * In case of success, 0 is returned and *size holds the value.
 * Otherwise, -1 is returned and *size is untouched.
 *
 * TODO: Internationalization, SI units.
 */
int
bu_dehumanize_number(const char *str, int64_t *size)
{
    char *ep, unit;
    const char *delimit;
    long multiplier;
    long long tmp, tmp2;
    size_t len;

    len = strlen(str);
    if (len == 0) {
	errno = EINVAL;
	return -1;
    }

    multiplier = 1;

    unit = str[len - 1];
    if (isalpha((unsigned char)unit)) {
	switch (tolower((unsigned char)unit)) {
	    case 'b':
		multiplier = 1;
		break;

	    case 'k':
		multiplier = 1024;
		break;

	    case 'm':
		multiplier = 1024 * 1024;
		break;

	    case 'g':
		multiplier = 1024 * 1024 * 1024;
		break;

	    default:
		errno = EINVAL;
		return -1; /* Invalid suffix. */
	}

	delimit = &str[len - 1];
    } else
	delimit = NULL;

    errno = 0;
    tmp = strtoll(str, &ep, 10);
    if (str[0] == '\0' || (ep != delimit && *ep != '\0'))
	return -1; /* Not a number. */
    else if (errno == ERANGE && (tmp == LLONG_MAX || tmp == LLONG_MIN))
	return -1; /* Out of range. */

    tmp2 = tmp * multiplier;
    tmp2 = tmp2 / multiplier;
    if (tmp != tmp2) {
	errno = ERANGE;
	return -1; /* Out of range. */
    }
    tmp *= multiplier;
    *size = (int64_t)tmp;

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
