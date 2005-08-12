/*                         U N I T S . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/

/** @file units.c
 *  Module of libbu to handle units conversion between strings and mm.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
/*@}*/

#ifndef lint
static const char libbu_units_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"

static const struct cvt_tab {
	double	val;
	char	name[32];
} bu_units_tab[] = {
	{0.0,		"none"},
	{1.0e-7,	"angstrom"},
	{1.0e-7,	"decinanometer"},
	{1.0e-6,	"nm"},
	{1.0e-6,	"nanometer"},
	{1.0e-3,	"um"},
	{1.0e-3,	"micrometer"},
	{1.0e-3,	"micron"},
	{1.0,		"mm"},
	{1.0,		"millimeter"},
	{10.0,		"cm"},
	{10.0,		"centimeter"},
	{1000.0,	"m"},
	{1000.0,	"meter"},
	{1000000.0,	"km"},
	{1000000.0,	"kilometer"},
	{25.4,		"in"},
	{25.4,		"inch"},
	{25.4,		"inche"},		/* for plural */
	{304.8,		"ft"},
	{304.8,		"foot"},
	{304.8,		"feet"},
	{456.2,		"cubit"},
	{914.4,		"yd"},
	{914.4,		"yard"},
	{5029.2,	"rd"},
	{5029.2,	"rod"},
	{1609344.0,	"mi"},
	{1609344.0,	"mile"},
	{1852000.0,	"nmile"},
	{1852000.0,	"nautical mile"},
	{1.495979e+14,	"AU"},
	{1.495979e+14,	"astronomical unit"},
	{9.460730e+18,	"lightyear"},
	{3.085678e+19,	"pc"},
	{3.085678e+19,	"parsec"},
	{0.0,		""}			/* LAST ENTRY */
};
#define BU_UNITS_TABLE_SIZE (sizeof(bu_units_tab) / sizeof(struct cvt_tab) - 1)

/*
 *			B U _ U N I T S _ C O N V E R S I O N
 *
 *  Given a string representation of a unit of distance (eg, "feet"),
 *  return the multiplier which will convert that unit into millimeters.
 *
 *  Returns -
 *	0.0	error
 *	>0.0	success
 */
double
bu_units_conversion(const char *str)
{
	register char	*ip;
	register int	c;
	register const struct cvt_tab	*tp;
	char		ubuf[256];
	int		len;

	strncpy( ubuf, str, sizeof(ubuf)-1 );
	ubuf[sizeof(ubuf)-1] = '\0';

	/* Copy the given string, making it lower case */
	ip = ubuf;
	while( (c = *ip) )  {
		if( !isascii(c) )
			*ip++ = '_';
		else if( isupper(c) )
			*ip++ = tolower(c);
		else
			ip++;
	}

	/* Remove any trailing "s" (plural) */
	len = strlen(ubuf);
	if( ubuf[len-1] == 's' )  ubuf[len-1] = '\0';

	/* Search for this string in the table */
	for( tp=bu_units_tab; tp->name[0]; tp++ )  {
		if( ubuf[0] != tp->name[0] )  continue;
		if( strcmp( ubuf, tp->name ) != 0 )  continue;
		return( tp->val );
	}
	return(0.0);		/* Unable to find it */
}

/*
 *			B U _ U N I T S _ S T R I N G
 *
 *  Given a conversion factor to mm, search the table to find
 *  what unit this represents.
 *  To accomodate floating point fuzz, a "near miss" is allowed.
 *  The algorithm depends on the table being sorted small-to-large.
 *
 *  Returns -
 *	char*	units string
 *	NULL	No known unit matches this conversion factor.
 */
const char *
bu_units_string(register const double mm)
{
	register const struct cvt_tab	*tp;

	if( mm <= 0 )  return (char *)NULL;

	/* Search for this string in the table */
	for( tp=bu_units_tab; tp->name[0]; tp++ )  {
		fastf_t	diff, bigger;
		if( mm == tp->val )  return tp->name;

		/* Check for near-miss */
		if( mm > tp->val )  {
			bigger = mm;
			diff = mm - tp->val;
		}  else  {
			bigger = tp->val;
			diff = tp->val - mm;
		}

		/* Absolute difference less than 0.1 angstrom */
		if( diff < 1.0e-8 )  return tp->name;

		/* Relative difference less than 1 part per billion */
		if( diff < 0.000000001 * bigger )  return tp->name;
	}
	return (char *)NULL;
}

/*
 *			B U _ M M _ V A L U E
 *
 * Given a string of the form "25cm" or "5.2ft" returns the 
 * corresponding distance in mm.
 *
 *  Returns -
 *	-1	on error
 *	>0	on success
 */
double
bu_mm_value(const char *s)
{
	double v;
	char *ptr;	
	register const struct cvt_tab	*tp;

	v = strtod(s, &ptr);

	if (ptr == s)  {
		/* No number could be found, unity is implied */
		/* e.g. interprept "ft" as "1ft" */
		v = 1.0;
	}
	if ( ! *ptr)  {
		/* There are no characters following the scaned number */
		return v;
	}

	for (tp=bu_units_tab; tp->name[0]; tp++ )  {
		if( *ptr != tp->name[0] )  continue;
		if( strcmp( ptr, tp->name ) == 0 ) {
			v *= tp->val;
			return v;
		}
	}

	/* A string was seen, but not found in the table.  Signal error */
	return -1;
}

/*	B U _ M M _ C V T
 *
 *  Used primarily as a hooked function for bu_structparse tables
 *  to allow input of floating point values in other units.
 */
void
bu_mm_cvt(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
                                    	     	/* structure description */
                   			      	/* struct member name */
    					      	/* begining of structure */
          				       	/* string containing value */
{
	register double *p = (double *)(base+sdp->sp_offset);

	/* reconvert with optional units */
	*p = bu_mm_value(value);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
