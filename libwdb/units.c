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

#include "conf.h"

#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
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

	if( (d = bu_units_conversion(str)) <= 0.0 )  return(-1);
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
