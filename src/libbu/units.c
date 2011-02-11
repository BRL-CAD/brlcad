/*                         U N I T S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <float.h>

#include "bu.h"


/* done specifically to avoid a libbn dependency */
#define NEAR_ZERO(val, epsilon) (((val) > -epsilon) && ((val) < epsilon))
#define ZERO(val) NEAR_ZERO((val), SMALL_FASTF)


struct cvt_tab {
    double val;
    char name[32];
};

struct conv_table {
    struct cvt_tab *cvttab;
};

struct cvt_tab bu_units_length_tab[] = {
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
    {25.4,		"inche"}, /* plural */
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
    {1852000.0,		"nauticalmile"},
    {1852000.0,		"nautical mile"},
    {5556000.0,		"league"},
    {2.99792458e+11,	"lightsecond"},
    {2.99792458e+11,	"light second"},
    {1.79875475e+13,	"lightminute"},
    {1.79875475e+13,	"light minute"},
    {1.495979e+14,	"AU"},
    {1.495979e+14,	"astronomicalunit"},
    {1.495979e+14,	"astronomical unit"},
    {1.07925285e+15,	"lighthour"},
    {1.07925285e+15,	"light hour"},
    {2.59020684e+16,	"lightday"},
    {2.59020684e+16,	"light day"},
    {9.4605284+18,	"lightyear"},
    {9.4605284+18,	"light year"},
    {3.08568025e+19,	"pc"},
    {3.08568025e+19,	"parsec"},
    {0.0,		""}			/* LAST ENTRY */
};
#define BU_UNITS_TABLE_SIZE (sizeof(bu_units_length_tab) / sizeof(struct cvt_tab) - 1)

struct cvt_tab bu_units_volume_tab[] = {
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

struct cvt_tab bu_units_mass_tab[] = {
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


double
bu_units_conversion(const char *str)
{
    register char *ip;
    register int c;
    register const struct cvt_tab *tp;
    register const struct conv_table *cvtab;
    char ubuf[256];
    size_t len;

    bu_strlcpy(ubuf, str, sizeof(ubuf));

    /* Copy the given string, making it lower case */
    ip = ubuf;
    c = *ip;
    while (c) {
	if (isupper(c))
	    *ip++ = tolower(c);
	else
	    ip++;

	c = *ip;
    }

    /* Remove any trailing "s" (plural) */
    len = strlen(ubuf);
    if (ubuf[len-1] == 's')  ubuf[len-1] = '\0';

    /* Search for this string in the table */
    for (cvtab=unit_lists; cvtab->cvttab; cvtab++) {
	for (tp=cvtab->cvttab; tp->name[0]; tp++) {
	    if (ubuf[0] != tp->name[0])  continue;
	    if (!BU_STR_EQUAL(ubuf, tp->name))  continue;
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
	return (char *)NULL;

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
    return (char *)NULL;
}

struct bu_vls *
bu_units_strings_vls()
{
    register const struct cvt_tab *tp;
    struct bu_vls *vlsp;
    double prev_val = 0.0;

    BU_GETSTRUCT(vlsp, bu_vls);
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
	return (char *)NULL;

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
	/* e.g. interprept "ft" as "1ft" */
	v = 1.0;
    }
    if (! *ptr) {
	/* There are no characters following the scaned number */
	return v;
    }

    for (tp=bu_units_length_tab; tp->name[0]; tp++) {
	if (*ptr != tp->name[0])  continue;
	if (BU_STR_EQUAL(ptr, tp->name)) {
	    v *= tp->val;
	    return v;
	}
    }

    /* A string was seen, but not found in the table.  Signal error */
    return -1;
}


void
bu_mm_cvt(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
/* structure description */
/* struct member name */
/* begining of structure */
/* string containing value */
{
    register double *p = (double *)(base+sdp->sp_offset);

    if (UNLIKELY(!name)) {
	bu_log("bu_mm_cvt: NULL name encountered\n");
    }

    /* reconvert with optional units */
    *p = bu_mm_value(value);
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
