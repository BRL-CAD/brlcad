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

/*			R T _ P A R S E _ M A T
 *
 *	Parse a string as a mat_t.  The elemets should be separated by
 *	a single, comma character.
 */
HIDDEN void
rt_parse_mat( mat, str)
register matp_t mat;
char *str;
{
	register int i;

	for (i=0 ; i < ELEMENTS_PER_MAT ; ++i)
		mat[i] = 0.0;

	for (i=0 ; i < ELEMENTS_PER_MAT ; ++i) {
		if (!isdigit(*str) && *str != '.') return;
		mat[i] = atof(str);
		while (*str && *str != ',')
			str++;
		if (!*str) return;	/* EOS */
		else str++;		/* Skip element separator */
	}
}

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
 *  Parse a comma separated string
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

	while( *str && *str != ',')
		str++;
	if (!*str) return;
	vp[1] = atof(++str);

	while( *str && *str != ',')
		str++;
	if (!*str) return;
	vp[2] = atof(++str);

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
			case 'M':
				rt_parse_mat( loc, value );
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
			case 's':
				strcpy((char *)loc, value);
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
			spp->sp_hook( spp, name, base, value );
		}
		return(0);		/* OK */
	}
	return(-1);			/* Not found */
}

/*
 *			R T _ S T R U C T P A R S E
 */
void
rt_structparse( vls, parsetab, base )
struct rt_vls		*vls;		/* string to parse through */
struct structparse	*parsetab;
char			*base;		/* base address of users structure */
{
	register char *cp;
	char	*name;
	char	*value;

	RT_VLS_CHECK(vls);

	cp = RT_VLS_ADDR(vls);

	while( *cp )  {
		/* NAME = VALUE separator (comma, space, tab) */

		/* skip any leading whitespace */
		while( *cp != '\0' && 
		    (*cp == ',' || (isascii(*cp) && isspace(*cp)) ))
			cp++;

		/* Find equal sign */
		name = cp;
		while( *cp != '\0' && *cp != '=' )  cp++;
		if( *cp == '\0' )  {
			if( name == cp )  break;
			rt_log("rt_structparse: name '%s' without '='\n", name );
		}
		*cp++ = '\0';

		/* Find end of value. */
		value = cp;
		if (*cp == '"')	{
			/* strings are double-quote (") delimited */

			value = ++cp; /* skip leading " */
			while (*cp != '\0' && *cp != '"')
				cp++;

		} else	/* non-strings are white-space delimited */
			while( *cp != '\0' && isascii(*cp) && !isspace(*cp) )
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



/*	M A T P R I N T
 *
 *	pretty-print a matrix
 */
HIDDEN void
matprint(name, mat)
char *name;
register matp_t mat;
{
	int i = rt_g.rtg_logindent;

	/* indent the body of the matrix */
	rt_g.rtg_logindent += strlen(name)+2;

	rt_log(" %s=%.-12E %.-12E %.-12E %.-12E\n",
		name, mat[0], mat[1], mat[2], mat[3]);
					
	rt_log("%.-12E %.-12E %.-12E %.-12E\n",
		mat[4], mat[5], mat[6], mat[7]);

	rt_log("%.-12E %.-12E %.-12E %.-12E\n",
		mat[8], mat[9], mat[10], mat[11]);

	rt_g.rtg_logindent = i;

	rt_log("%.-12E %.-12E %.-12E %.-12E\n",
		mat[12], mat[13], mat[14], mat[15]);
}


/*
 *			R T _ S T R U C T P R I N T
 */
void
rt_structprint( title, parsetab, base )
char 			*title;
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

			case 'c':
				rt_log( " %s=%c\n", spp->sp_name, *loc);
				break;
			case 's':
				rt_log( " %s=\"%s\"\n", spp->sp_name,
					(char *)loc );
				break;
			case 'd':
				rt_log( " %s=%d\n", spp->sp_name,
					*((int *)loc) );
				break;
			case 'f':
				rt_log( " %s=%.-25G\n", spp->sp_name,
					*((double *)loc) );
				break;
			case 'C':
				{
					register unsigned char *cp =
						(unsigned char *)loc;
					rt_log(" %s=%d/%d/%d\n", spp->sp_name,
						cp[0], cp[1], cp[2] );
					break;
				}
			case 'M': matprint(spp->sp_name, (matp_t)loc);
				break;
			case 'V':
				{
					register fastf_t *fp = (fastf_t *)loc;
					rt_log(" %s=%.-25G  %.-25G  %.-25G\n",
						spp->sp_name,
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

/*	R T _ V L S _ S T R U C T P R I N T
 *
 *	This differs from rt_structprint in that this output is less readable
 *	by humans, but easier to parse with the computer.
 */
void
rt_vls_structprint( vls, ptab, base)
struct	rt_vls			*vls;	/* vls to print into */
register struct structparse	*ptab;
char				*base;	/* structure ponter */
{
	register char			*loc;
	register int			lastoff = -1;
	register char			*cp;

	RT_VLS_CHECK(vls);

	for ( ; ptab->sp_name != (char*)NULL ; ptab++) {
		/* Skip alternate keywords for same value */

		if( lastoff == ptab->sp_offset )
			continue;
		lastoff = ptab->sp_offset;

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)ptab->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + ((int)ptab->sp_offset));
#endif

		switch( ptab->sp_fmt[0] )  {
		case 'i':
			{
				struct rt_vls sub_str;

				rt_vls_init(&sub_str);
				rt_vls_structprint( &sub_str,
					(struct structparse *)ptab->sp_offset,
					base );

				rt_vls_vlscat(vls, &sub_str);
				rt_vls_free( &sub_str );
				break;
			}
		case '%':
			switch( ptab->sp_fmt[1] )  {

			case 'c':
				rt_vls_extend(vls, strlen(ptab->sp_name)+4);
				cp = &vls->vls_str[vls->vls_len];
				sprintf(cp, " %s=%c",
					ptab->sp_name, (char)(*loc));
				vls->vls_len += strlen(cp);
				break;
			case 's':
				rt_vls_extend(vls, strlen(ptab->sp_name)+
					strlen(loc)+6);
				cp = &vls->vls_str[vls->vls_len];
				sprintf(cp, " %s=\"%s\"",
				 	ptab->sp_name, loc);
				vls->vls_len += strlen(cp);
				break;
			case 'd':
				rt_vls_extend(vls, 64+strlen(ptab->sp_name)+3);
				cp = &vls->vls_str[vls->vls_len];
				sprintf(cp, " %s=%d",
					ptab->sp_name, *((int *)loc) );
				vls->vls_len += strlen(cp);
				break;
			case 'f':
				rt_vls_extend(vls, 32+strlen(ptab->sp_name));
				cp = &vls->vls_str[vls->vls_len];
				sprintf(cp, " %s=%.27E",
					ptab->sp_name, *((double *)loc) );
				vls->vls_len += strlen(cp);
				break;
			case 'C':
				{
					register unsigned char *ucp =
						(unsigned char *)loc;

					rt_vls_extend(vls,
						16+strlen(ptab->sp_name) );
					/* WARNING: peeks inside rt_vls structure */
					cp = &vls->vls_str[vls->vls_len];
					sprintf( cp, " %s=%d/%d/%d",
						ptab->sp_name,
						ucp[0], ucp[1], ucp[2]);
					vls->vls_len += strlen(cp);
					break;
				}
			case 'M':
				{	register fastf_t *fp = (fastf_t *)loc;

					rt_vls_extend(vls,
						365+strlen(ptab->sp_name));

					cp = &vls->vls_str[vls->vls_len];
					sprintf(cp,
" %s=\
%.27E,%.27E,%.27E,%.27E,\
%.27E,%.27E,%.27E,%.27E,\
%.27E,%.27E,%.27E,%.27E,\
%.27E,%.27E,%.27E,%.27E",
						ptab->sp_name,
						fp[0], fp[1], fp[2], fp[3],
						fp[4], fp[5], fp[6], fp[7],
						fp[8], fp[9], fp[10],fp[11],
						fp[12],fp[13],fp[14],fp[15]);
					vls->vls_len += strlen(cp);
					break;
				}
			case 'V':
				{
					register fastf_t *fp = (fastf_t *)loc;

					rt_vls_extend(vls, 96);

					cp = &vls->vls_str[vls->vls_len];
					sprintf(cp, " %s=%.27E,%.27E,%.27E",
						ptab->sp_name,
						fp[0], fp[1], fp[2] );
					vls->vls_len += strlen(cp);
					break;
				}
			default:
				rt_log( " %s=%s??\n", ptab->sp_name,
					ptab->sp_fmt );
				abort();
				break;
			}
			break;
		default:
			rt_log("rt_structprint:  %s: unknown format '%s'\n",
				ptab->sp_name, ptab->sp_fmt );
			break;
		}
	}
}
