/*
 *			U N I T S . C
 * 
 *  Module of libwdb to handle units conversion.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "wdb.h"

double	mk_conv2mm = 1.0;		/* Conversion factor to mm */

/*
 *			M K _ C O N V E R S I O N
 *
 *  Given a string conversion value, find the appropriate factor,
 *  and establish it.
 *
 *  Returns -
 *	-1	error
 *	 0	OK
 */
int
mk_conversion(str)
char	*str;
{
	double	d;

	if( (d = mk_cvt_factor(str)) <= 0.0 )  return(-1);
	return( mk_set_conversion(d) );
}

/*
 *			M K _ S E T _ C O N V E R S I O N
 *
 *  Establish a new conversion factor for LIBWDB routines.
 *
 *  Returns -
 *	-1	error
 *	 0	OK
 */
int
mk_set_conversion(val)
double	val;
{
	if( val <= 0.0 )  return(-1);
	mk_conv2mm = val;
	return(0);
}

static struct cvt_tab {
	double	val;
	char	name[32];
} mk_cvt_tab[] = {
	1.0e-7,		"angstrom",
	1.0e-7,		"angstroms",
	1.0e-7,		"decinanometer",
	1.0e-7,		"decinanometers",
	1.0e-6,		"nm",
	1.0e-6,		"nanometer",
	1.0e-6,		"nanometers",
	1.0e-3,		"micron",
	1.0e-3,		"microns",
	1.0e-3,		"micrometer",
	1.0e-3,		"micrometers",
	1.0,		"mm",
	1.0,		"millimeter",
	1.0,		"millimeters",
	10.0,		"cm",
	10.0,		"centimeter",
	10.0,		"centimeters",
	1000.0,		"m",
	1000.0,		"meter",
	1000.0,		"meters",
	1000000.0,	"km",
	1000000.0,	"kilometer",
	1000000.0,	"kilometers",
	25.4,		"in",
	25.4,		"inche",
	25.4,		"inches",
	304.8,		"ft",
	304.8,		"foot",
	304.8,		"feet",
	456.2,		"cubit",
	456.2,		"cubits",
	914.4,		"yd",
	914.4,		"yard",
	914.4,		"yards",
	5029.2,		"rd",
	5029.2,		"rod",
	5029.2,		"rods",
	1609344.0,	"mi",
	1609344.0,	"mile",
	1609344.0,	"miles",
	1852000.0,	"nmile",
	1852000.0,	"nautical mile",
	0.0,		"",			/* LAST ENTRY */
};

/*
 *			M K _ C V T _ F A C T O R
 *
 *  Given a string representation of a unit of distance (eg, "feet"),
 *  return the number which will convert that unit into millimeters.
 *
 *  Returns -
 *	0.0	error
 *	>0.0	success
 */
double
mk_cvt_factor(str)
char	*str;
{
	register char	*ip, *op;
	register int	c;
	register struct cvt_tab	*tp;
	char		ubuf[64];

	if( strlen(str) >= sizeof(ubuf)-1 )  str[sizeof(ubuf)-1] = '\0';

	/* Copy the given string, making it lower case */
	ip = str;
	op = ubuf;
	while( (c = *ip++) )  {
		if( !isascii(c) )
			*op++ = '_';
		else if( isupper(c) )
			*op++ = tolower(c);
		else
			*op++ = c;
	}
	*op = '\0';

	/* Search for this string in the table */
	for( tp=mk_cvt_tab; tp->name[0]; tp++ )  {
		if( ubuf[0] != tp->name[0] )  continue;
		if( strcmp( ubuf, tp->name ) != 0 )  continue;
		return( tp->val );
	}
	return(0.0);		/* Unable to find it */
}
