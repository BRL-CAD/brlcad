/*
 *			P A R S E . C
 *
 *  Routines to assign values to elements of arbitrary structures.
 *  The layout of a structure to be processed is described by
 *  a structure of type "structparse", giving element names, element
 *  formats, an offset from the beginning of the structure, and
 *  a pointer to an optional "hooked" function that is called whenever
 *  that structure element is changed.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSparse[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#ifdef BSD
# include <strings.h>
#else
# include <string.h>
#endif

/*
 *			R T _ P A R S E _ R G B
 *
 *  Parse a slash (or other non-numeric, non-whitespace) separated string
 *  as 3 decimal (or octal) bytes.  Useful for entering rgb values in
 *  mlib_parse as 4/5/6.
 *
 *  Element [3] is made non-zero to indicate that a value has been set.
 */
HIDDEN void
rt_parse_rgb( rgb, str )
register unsigned char *rgb;
register char *str;
{
	if( !isdigit(*str) )  return;
	rgb[0] = atoi(str);
	rgb[1] = rgb[2] = 0;
	rgb[3] = 1;
	while( *str )
		if( !isdigit(*str++) )  break;
	if( !*str )  return;
	rgb[1] = atoi(str);
	while( *str )
		if( !isdigit(*str++) )  break;
	if( !*str )  return;
	rgb[2] = atoi(str);
}

/*
 *			R T _ P A R S E _ V E C T
 *
 *  Parse a slash (or other non-numeric, non-whitespace) separated string
 *  as a vect_t (3 fastf_t's).  Useful for entering vector values in
 *  mlib_parse as 1.0/0.5/0.1.
 */
HIDDEN void
rt_parse_vect( vp, str )
register fastf_t	*vp;
register char		*str;
{
	vp[0] = vp[1] = vp[2] = 0.0;
	if( !isdigit(*str) && *str != '.' )  return;
	vp[0] = atof(str);
	while( *str ) {
		if( !isdigit(*str) && *str != '.' ) {
			str++;
			break;
		}
		str++;
	}
	if( !*str )  return;
	vp[1] = atof(str);
	while( *str ) {
		if( !isdigit(*str) && *str != '.' ) {
			str++;
			break;
		}
		str++;
	}
	if( !*str )  return;
	vp[2] = atof(str);
}

/*
 *			R T _ S T R U C T _ L O O K U P
 *
 *  Returns -
 *	-1	not found
 *	 0	entry found and processed
 */
HIDDEN int
rt_struct_lookup( spp, name, base, value )
register struct structparse	*spp;
register char			*name;
char				*base;
char				*value;		/* string containing value */
{
	register char *loc;

	for( ; spp->sp_name != (char *)0; spp++ )  {
		if( strcmp( spp->sp_name, name ) != 0
		    && spp->sp_fmt[0] != 'i' )
			continue;

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)spp->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + ((int)spp->sp_offset));
#endif

		switch( spp->sp_fmt[0] )  {
		case 'i':
			/* Indirect to another structure */
			if( rt_struct_lookup(
				(struct structparse *)spp->sp_offset,
				name, base, value )
			    == 0 )
				return(0);	/* found */
			break;
		case '%':
			switch( spp->sp_fmt[1] )  {

			case 'C':
				rt_parse_rgb( loc, value );
				break;
			case 'V':
				rt_parse_vect( loc, value );
				break;
			case 'f':
				/*  Silicon Graphics sucks wind.
				 *  On the 3-D machines, float==double,
				 *  which breaks the scanf() strings.
				 *  So, here we cause "%f" to store into
				 *  a double.  This is the "generic"
				 *  floating point read.  Humbug.
				 */
				*((double *)loc) = atof( value );
				break;
			default:
				(void)sscanf( value, spp->sp_fmt, loc );
				break;
			}
			break;
		default:
			rt_log("rt_struct_lookup(%s): unknown format '%s'\n",
				name, spp->sp_fmt );
			return(-1);
		}
		if( spp->sp_hook != FUNC_NULL )  {
			/* XXX What should the args be? */
			spp->sp_hook( spp, base, value );
		}
		return(0);		/* OK */
	}
	return(-1);			/* Not found */
}

/*
 *			R T _ S T R U C T P A R S E
 */
void
rt_structparse( cp, parsetab, base )
register char		*cp;
struct structparse	*parsetab;
char			*base;		/* base address of users structure */
{
	char	*name;
	char	*value;

	while( *cp )  {
		/* NAME = VALUE separator (comma, space, tab) */

		/* skip any leading whitespace */
		while( *cp != '\0' && 
		    (*cp == ',' || *cp == ' ' || *cp == '\t' ) )
			cp++;

		/* Find equal sign */
		name = cp;
		while( *cp != '\0' && *cp != '=' )  cp++;
		if( *cp == '\0' )  {
			if( name == cp )  break;
			rt_log("rt_structparse: name '%s' without '='\n", name );
		}
		*cp++ = '\0';

		/* Find end of value */
		value = cp;
		while( *cp != '\0' && *cp != ',' &&
		    *cp != ' ' && *cp != '\t' )
			cp++;
		if( *cp != '\0' )
			*cp++ = '\0';

		/* Lookup name in parsetab table */
		if( rt_struct_lookup( parsetab, name, base, value ) < 0 )  {
			rt_log("rt_structparse:  '%s=%s', element name not found in:\n",
				name, value);
			rt_structprint( "troublesome one", parsetab, base );
		}
	}
}

/*
 *			R T _ S T R U C T P R I N T
 */
void
rt_structprint( title, parsetab, base )
char			*title;
struct structparse	*parsetab;
char			*base;		/* base address of users structure */
{
	register struct structparse	*spp;
	register char			*loc;
	register int			lastoff = -1;

	rt_log( "%s\n", title );
	for( spp = parsetab; spp->sp_name != (char *)0; spp++ )  {

		/* Skip alternate keywords for same value */
		if( lastoff == spp->sp_offset )
			continue;
		lastoff = spp->sp_offset;

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)spp->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + ((int)spp->sp_offset));
#endif

		switch( spp->sp_fmt[0] )  {
		case 'i':
			rt_structprint( spp->sp_name,
				(struct structparse *)spp->sp_offset,
				base );
			break;
		case '%':
			switch( spp->sp_fmt[1] )  {

			case 's':
				rt_log( " %s=%s\n", spp->sp_name,
					(char *)loc );
				break;
			case 'd':
				rt_log( " %s=%d\n", spp->sp_name,
					*((int *)loc) );
				break;
			case 'f':
				rt_log( " %s=%g\n", spp->sp_name,
					*((double *)loc) );
				break;
			case 'C':
				{
					register unsigned char *cp =
						(unsigned char *)loc;
					rt_log(" %s=%d/%d/%d(%d)\n", spp->sp_name,
						cp[0], cp[1], cp[2], cp[3] );
					break;
				}
			case 'V':
				{
					register fastf_t *fp =
						(fastf_t *)loc;
					rt_log(" %s=%f/%f/%f\n", spp->sp_name,
						fp[0], fp[1], fp[2] );
					break;
				}
			default:
				rt_log( " %s=%s??\n", spp->sp_name,
					spp->sp_fmt );
				break;
			}
			break;
		default:
			rt_log("rt_structprint:  %s: unknown format '%s'\n",
				spp->sp_name, spp->sp_fmt );
			break;
		}
	}
}
