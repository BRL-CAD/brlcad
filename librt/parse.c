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
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
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

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "externs.h"
#include "raytrace.h"

/*
 *			R T _ P A R S E _ D O U B L E
 *
 *  Parse an array of one or more doubles.
 */
HIDDEN void
rt_parse_double(str, count, loc)
CONST char	*str;
int		count;
double		*loc;
{
	int	i;
	int	dot_seen;
	CONST char	*numstart;
	double	tmp_double;
	char	buf[128];	
	int	len;

	for (i=0 ; i < count && *str ; ++i){
		numstart = str;

		/* skip sign */
		if (*str == '-' || *str == '+') str++;

		/* skip matissa */
		dot_seen = 0;
		for ( ; *str ; str++ ) {
			if (*str == '.' && !dot_seen) {
				dot_seen = 1;
				continue;
			}
			if (!isdigit(*str))
				break;
			
		}

		/* If no mantissa seen, then there is no float here */
		if (str == (numstart + dot_seen) )
			return;

		/* there was a mantissa, so we may have an exponent */
		if  (*str == 'E' || *str == 'e') {
			str++;

			/* skip exponent sign */
		    	if (*str == '+' || *str == '-') str++;

			while (isdigit(*str)) str++;
		}

		len = str - numstart;
		if( len > sizeof(buf)-1 )  len = sizeof(buf)-1;
		strncpy( buf, numstart, len );
		buf[len] = '\0';

		if( sscanf( buf, "%lf", &tmp_double ) != 1 )
			return;

		*loc++ = tmp_double;

		/* skip the separator */
		if (*str) str++;
	}
}

/*
 *			R T _ S T R U C T _ L O O K U P
 *
 *  Returns -
 *	-1	not found
 *	 0	entry found and processed
 */
HIDDEN int
rt_struct_lookup( sdp, name, base, value )
register CONST struct structparse	*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
	register char *loc;
	int i;

	for( ; sdp->sp_name != (char *)0; sdp++ )  {

		if( strcmp( sdp->sp_name, name ) != 0	/* no name match */
		    && sdp->sp_fmt[0] != 'i' )		/* no include desc */
			continue;

		/* if we get this far, we've got a name match
		 * with a name in the structure description
		 */

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)sdp->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + ((int)sdp->sp_offset));
#endif

		if (sdp->sp_fmt[0] == 'i') {
			/* Indirect to another structure */
			if( rt_struct_lookup(
				(struct structparse *)sdp->sp_count,
				name, base, value )
			    == 0 )
				return(0);	/* found */
			else
				continue;
		}
		if (sdp->sp_fmt[0] != '%') {
			rt_log("rt_struct_lookup(%s): unknown format '%s'\n",
				name, sdp->sp_fmt );
			return(-1);
		}

		switch( sdp->sp_fmt[1] )  {
		case 'c':
		case 's':
			{	register int i, j;

				/* copy the string, converting escaped
				 * double quotes to just double quotes
				 */
				for(i=j=0 ;
				    j < sdp->sp_count && value[i] != '\0' ;
				    loc[j++] = value[i++])
					if (value[i] == '\\' &&
					    value[i+1] == '"')
					    	++i;

				if (sdp->sp_count > 1)
					loc[sdp->sp_count-1] = '\0';
			}
			break;
		case 'S':
			{	struct rt_vls *vls = (struct rt_vls *)loc;
				if (vls->vls_magic != RT_VLS_MAGIC)
					rt_vls_init(vls);

				rt_vls_strcpy(vls, value);
			}
			break;
		case 'i':
			{	register short *ip = (short *)loc;
				register short tmpi;
				register CONST char *cp;
				for (i=0 ; i < sdp->sp_count && *value ; ++i){
					tmpi = atoi( value );

					cp = value;
					if (*cp && (*cp == '+' || *cp == '-'))
						cp++;

					while (*cp && isdigit(*cp) )
						cp++; 

					/* make sure we actually had an
					 * integer out there
					 */
					if (cp == value ||
					    (cp == value+1 &&
					    (*value == '+' || *value == '-')))
						break;
					else {
						*(ip++) = tmpi;
						value = cp;
					}
					/* skip the separator */
					if (*value) value++;
				}
			}
			break;
		case 'd':
			{	register int *ip = (int *)loc;
				register int tmpi;
				register char CONST *cp;
				/* Special case:  '=!' toggles a boolean */
				if( *value == '!' )  {
					*ip = *ip ? 0 : 1;
					value++;
					break;
				}
				/* Normal case: an integer */
				for (i=0 ; i < sdp->sp_count && *value ; ++i){
					tmpi = atoi( value );

					cp = value;
					if (*cp && (*cp == '+' || *cp == '-'))
						cp++;

					while (*cp && isdigit(*cp) )
						cp++; 

					/* make sure we actually had an
					 * integer out there
					 */
					if (cp == value ||
					    (cp == value+1 &&
					    (*value == '+' || *value == '-')))
						break;
					else {
						*(ip++) = tmpi;
						value = cp;
					}
					/* skip the separator */
					if (*value) value++;
				}
			}
			break;
		case 'f':
			rt_parse_double(value, sdp->sp_count, (double *)loc);
			break;
		default:
			rt_log("rt_struct_lookup(%s): unknown format '%s'\n",
				name, sdp->sp_fmt );
			return(-1);
		}
		if( sdp->sp_hook )  {
			sdp->sp_hook( sdp, name, base, value );
		}
		return(0);		/* OK */
	}
	return(-1);			/* Not found */
}

/*
 *			R T _ S T R U C T P A R S E
 *
 *	Parse the structure element description in the vls string "vls"
 *	according to the structure description in "parsetab"
 *
 *  Returns -
 *	<0	failure
 *	 0	OK
 */
int
rt_structparse( in_vls, desc, base )
CONST struct rt_vls		*in_vls;	/* string to parse through */
CONST struct structparse	*desc;		/* structure description */
char				*base;		/* base addr of users struct */
{
	struct rt_vls	vls;
	register char *cp;
	char	*name;
	char	*value;

	RT_VLS_CHECK(in_vls);
	if (desc == (struct structparse *)NULL) {
		rt_log( "Null \"struct structparse\" pointer\n");
		return(-1);
	}

	/* Duplicate the input string.  This algorithm is destructive. */
	RT_VLS_INIT( &vls );
	rt_vls_vlscat( &vls, in_vls );
	cp = RT_VLS_ADDR( &vls );

	while( *cp )  {
		/* NAME = VALUE white-space-separator */

		/* skip any leading whitespace */
		while( *cp != '\0' && isascii(*cp) && isspace(*cp) )
			cp++;

		/* Find equal sign */
		name = cp;
		while ( *cp != '\0' && *cp != '=' )
			cp++;

		if( *cp == '\0' )  {
			if( name == cp ) break;

			/* end of string in middle of arg */
			rt_log("rt_structparse: name '%s' without '='\n",
				name );
			rt_vls_free( &vls );
			return(-2);
		}

		*cp++ = '\0';

		/* Find end of value. */
		if (*cp == '"')	{
			/* strings are double-quote (") delimited
			 * skip leading " & find terminating "
			 * while skipping escaped quotes (\")
			 */
			for (value = ++cp ; *cp != '\0' ; ++cp)
				if (*cp == '"' &&
				    (cp == value || *(cp-1) != '\\') )
					break;

			if (*cp != '"') {
				rt_log("rt_structparse: name '%s'=\" without closing \"\n",
					name);
				rt_vls_free( &vls );
				return(-3);
			}
		} else {
			/* non-strings are white-space delimited */
			value = cp;
			while( *cp != '\0' && isascii(*cp) && !isspace(*cp) )
				cp++;
		}

		if( *cp != '\0' )
			*cp++ = '\0';

		/* Lookup name in desc table */
		if( rt_struct_lookup( desc, name, base, value ) < 0 )  {
			rt_log("rt_structparse:  '%s=%s', element name not found in:\n",
				name, value);
			rt_structprint( "troublesome one", desc, base );
		}
	}
	rt_vls_free( &vls );
	return(0);
}

/*
 *			R T _ M A T P R I N T
 *
 *	pretty-print a matrix
 */
HIDDEN void
rt_matprint(name, mat)
CONST char		*name;
register CONST matp_t	mat;
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
 *                     R T _ V L S _ I T E M _ P R I N T
 *
 * Takes the single item pointed to by "sp", and prints its value into a
 * vls.
 */

void
rt_vls_item_print( vp, sdp, base )
struct rt_vls *vp;
CONST struct structparse *sdp;     /* item description */
CONST char *base;                 /* base address of users structure */
{
    register char *loc;

    if (sdp == (struct structparse *)NULL) {
	rt_log( "Null \"struct structparse\" pointer\n");
	return;
    }

#if CRAY && !__STDC__
    loc = (char *)(base + ((int)sdp->sp_offset*sizeof(int)));
#else
    loc = (char *)(base + ((int)sdp->sp_offset));
#endif

    if (sdp->sp_fmt[0] == 'i' )  {
	rt_log( "Cannot print type 'i' yet!\n" );
	return;
    }

    if ( sdp->sp_fmt[0] != '%')  {
	rt_log("rt_structprint:  %s: unknown format '%s'\n",
	       sdp->sp_name, sdp->sp_fmt );
	return;
    }

    switch( sdp->sp_fmt[1] )  {
    case 'c':
    case 's':
	if (sdp->sp_count < 1)
	    break;
	if (sdp->sp_count == 1)
	    rt_vls_printf( vp, "%c", *loc );
	else
	    rt_vls_printf( vp, "%s", (char *)loc );
	break;
    case 'S': {
	register struct rt_vls *vls = (struct rt_vls *)loc;

	rt_vls_vlscat( vp, vls ); }
	break;
    case 'i': {
	register int i = sdp->sp_count;
	register short *sp = (short *)loc;

	rt_vls_printf( vp, "%hd", *sp++ );
	while( --i > 0 ) rt_vls_printf( vp, ",%hd", *sp++ ); }
	break;
    case 'd': {
	register int i = sdp->sp_count;
	register int *dp = (int *)loc;

	rt_vls_printf( vp, "%d", *dp++ );
	while( --i > 0 ) rt_vls_printf( vp, ",%d", *dp++ ); }
	break;
    case 'f': {
	register int i = sdp->sp_count;
	register double *dp = (double *)loc;

	rt_vls_printf( vp, "%.25G", *dp++ );
	while( --i > 0 ) rt_vls_printf( vp, ",%.25G", *dp++ ); }
	break;
    case 'x': {
	register int i = sdp->sp_count;
	register int *dp = (int *)loc;

	rt_vls_printf( vp, "%08x", *dp++ );
	while( --i > 0 ) rt_vls_printf( vp, ",%08x", *dp++ );  }
	break;
    default:
	break;
    }
}    


/*
 *			R T _ S T R U C T P R I N T
 */
void
rt_structprint( title, parsetab, base )
CONST char			*title;
CONST struct structparse	*parsetab;/* structure description */
CONST char			*base;	  /* base address of users structure */
{
	register CONST struct structparse	*sdp;
	register char			*loc;
	register int			lastoff = -1;
	struct rt_vls vls;

	rt_vls_init( &vls );
	rt_log( "%s\n", title );
	if (parsetab == (struct structparse *)NULL) {
		rt_log( "Null \"struct structparse\" pointer\n");
		return;
	}
	for( sdp = parsetab; sdp->sp_name != (char *)0; sdp++ )  {

		/* Skip alternate keywords for same value */
		if( lastoff == sdp->sp_offset )
			continue;
		lastoff = sdp->sp_offset;

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)sdp->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + ((int)sdp->sp_offset));
#endif

		if (sdp->sp_fmt[0] == 'i' )  {
			rt_structprint( sdp->sp_name,
				(struct structparse *)sdp->sp_count,
				base );
			continue;
		}

		if ( sdp->sp_fmt[0] != '%')  {
			rt_log("rt_structprint:  %s: unknown format '%s'\n",
				sdp->sp_name, sdp->sp_fmt );
			continue;
		}
#if 1
		rt_vls_trunc( &vls, 0 );
		rt_vls_item_print( &vls, sdp, base );
		rt_log( " %s=%s\n", sdp->sp_name, rt_vls_addr(&vls) );
#else
		switch( sdp->sp_fmt[1] )  {
		case 'c':
		case 's':
			if (sdp->sp_count < 1)
				break;
			if (sdp->sp_count == 1)
				rt_log( " %s='%c'\n", sdp->sp_name, *loc);
			else
				rt_log( " %s=\"%s\"\n", sdp->sp_name,
					(char *)loc );
			break;
		case 'S':
			{	register int indent = rt_g.rtg_logindent;
				register struct rt_vls *vls =
					(struct rt_vls *)loc;

				rt_g.rtg_logindent = strlen(sdp->sp_name)+2;
				
				rt_log(" %s=(vls_magic)%d (vls_offset)%d (vls_len)%d (vls_max)%d\n",
					sdp->sp_name, vls->vls_magic,
					vls->vls_offset,
					vls->vls_len, vls->vls_max);
				rt_g.rtg_logindent = indent;
				rt_log("\"%s\"\n", vls->vls_str+vls->vls_offset);
			}
			break;
		case 'i':
			{	register int i = sdp->sp_count;
				register short *sp = (short *)loc;

				rt_log( " %s=%hd", sdp->sp_name, *sp++ );

				while (--i > 0) rt_log( ",%d", *sp++ );

				rt_log("\n");
			}
			break;
		case 'd':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;

				rt_log( " %s=%d", sdp->sp_name, *dp++ );

				while (--i > 0) rt_log( ",%d", *dp++ );

				rt_log("\n");
			}
			break;
		case 'f':
			{	register int i = sdp->sp_count;
				register double *dp = (double *)loc;

				if (sdp->sp_count == ELEMENTS_PER_MAT) {
					rt_matprint(sdp->sp_name, (matp_t)dp);
				} else if (sdp->sp_count <= ELEMENTS_PER_VECT){
					rt_log( " %s=%.25G", sdp->sp_name, *dp++ );

					while (--i > 0)
						rt_log( ",%.25G", *dp++ );

					rt_log("\n");
				}else  {
					register int j = rt_g.rtg_logindent;

					rt_g.rtg_logindent += strlen(sdp->sp_name)+2;
					
					rt_log( " %s=%.25G\n", sdp->sp_name, *dp++ );

					while (--i > 1)
						rt_log( "%.25G\n", *dp++ );

					rt_g.rtg_logindent = j;
					rt_log( "%.25G\n", *dp );

				}
			}
			break;
		case 'x':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;

				rt_log( " %s=%08x", sdp->sp_name, *dp++ );

				while (--i > 0) rt_log( ",%08x", *dp++ );

				rt_log("\n");
			}
			break;
		default:
			rt_log( " rt_structprint: Unknown format: %s=%s??\n",
				sdp->sp_name, sdp->sp_fmt );
			break;
		}
#endif
	}
	rt_vls_free(&vls);
}

/*
 *			R T _ V L S _ P R I N T _ D O U B L E
 */
HIDDEN void
rt_vls_print_double(vls, name, count, dp)
struct rt_vls		*vls;
CONST char		*name;
register int		count;
register CONST double	*dp;
{
	register int tmpi;
	register char *cp;

	rt_vls_extend(vls, strlen(name) + 3 + 32 * count);

	cp = vls->vls_str + vls->vls_offset + vls->vls_len;
	sprintf(cp, "%s%s=%.27G", (vls->vls_len?" ":""), name, *dp++);
	tmpi = strlen(cp);
	vls->vls_len += tmpi;

	while (--count > 0) {
		cp += tmpi;
		sprintf(cp, ",%.27G", *dp++);
		tmpi = strlen(cp);
		vls->vls_len += tmpi;
	}
}

/*
 *			R T _ V L S _ S T R U C T P R I N T
 *
 *	This differs from rt_structprint in that this output is less readable
 *	by humans, but easier to parse with the computer.
 */
void
rt_vls_structprint( vls, sdp, base)
struct	rt_vls				*vls;	/* vls to print into */
register CONST struct structparse	*sdp;	/* structure description */
CONST char				*base;	/* structure ponter */
{
	register char			*loc;
	register int			lastoff = -1;
	register char			*cp;

	RT_VLS_CHECK(vls);

	if (sdp == (struct structparse *)NULL) {
		rt_log( "Null \"struct structparse\" pointer\n");
		return;
	}

	for ( ; sdp->sp_name != (char*)NULL ; sdp++) {
		/* Skip alternate keywords for same value */

		if( lastoff == sdp->sp_offset )
			continue;
		lastoff = sdp->sp_offset;

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)sdp->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + ((int)sdp->sp_offset));
#endif

		if (sdp->sp_fmt[0] == 'i')  {
			struct rt_vls sub_str;

			rt_vls_init(&sub_str);
			rt_vls_structprint( &sub_str,
				(struct structparse *)sdp->sp_count,
				base );

			rt_vls_vlscat(vls, &sub_str);
			rt_vls_free( &sub_str );
			continue;
		}

		if ( sdp->sp_fmt[0] != '%' )  {
			rt_log("rt_structprint:  %s: unknown format '%s'\n",
				sdp->sp_name, sdp->sp_fmt );
			break;
		}

		switch( sdp->sp_fmt[1] )  {
		case 'c':
		case 's':
			if (sdp->sp_count < 1)
				break;
			if (sdp->sp_count == 1) {
				rt_vls_extend(vls, strlen(sdp->sp_name)+6);
				cp = vls->vls_str + vls->vls_offset + vls->vls_len;
				if (*loc == '"')
					sprintf(cp, "%s%s=\"%s\"",
						(vls->vls_len?" ":""),
						sdp->sp_name, "\\\"");
				else
					sprintf(cp, "%s%s=\"%c\"",
						(vls->vls_len?" ":""),
						sdp->sp_name, 
						*loc);
			} else {
				register char *p; 
				register int count=0;

				/* count the quote characters */
				p = loc;
				while ((p=strchr(p, '"')) != (char *)NULL) {
					++p;
					++count;
				}
				rt_vls_extend(vls, strlen(sdp->sp_name)+
					strlen(loc)+5+count);

				cp = vls->vls_str + vls->vls_offset + vls->vls_len;
				if (vls->vls_len) (void)strcat(cp, " ");
				(void)strcat(cp, sdp->sp_name);
				(void)strcat(cp, "=\"");

				/* copy the string, escaping all the internal
				 * double quote (") characters
				 */
				p = &cp[strlen(cp)];
				while (*loc) {
					if (*loc == '"') {
						*p++ = '\\';
					}
					*p++ = *loc++;
				}
				*p++ = '"';
				*p = '\0';
			}
			vls->vls_len += strlen(cp);
			break;
		case 'S':
			{	register struct rt_vls *vls_p =
					(struct rt_vls *)loc;

				rt_vls_extend(vls, rt_vls_strlen(vls_p) + 5 +
					strlen(sdp->sp_name) );

				cp = vls->vls_str + vls->vls_offset + vls->vls_len;
				sprintf(cp, "%s%s=\"%s\"",
					(vls->vls_len?" ":""),
					sdp->sp_name,
					rt_vls_addr(vls_p) );
				vls->vls_len += strlen(cp);
			}
			break;
		case 'i':
			{	register int i = sdp->sp_count;
				register short *sp = (short *)loc;
				register int tmpi;

				rt_vls_extend(vls, 
					64 * i + strlen(sdp->sp_name) + 3 );

				cp = vls->vls_str + vls->vls_offset + vls->vls_len;
				sprintf(cp, "%s%s=%d",
						(vls->vls_len?" ":""),
						 sdp->sp_name, *sp++);
				tmpi = strlen(cp);
				vls->vls_len += tmpi;

				while (--i > 0) {
					cp += tmpi;
					sprintf(cp, ",%d", *sp++);
					tmpi = strlen(cp);
					vls->vls_len += tmpi;
				}
			}
			break;
		case 'd':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;
				register int tmpi;

				rt_vls_extend(vls, 
					64 * i + strlen(sdp->sp_name) + 3 );

				cp = vls->vls_str + vls->vls_offset + vls->vls_len;
				sprintf(cp, "%s%s=%d", 
					(vls->vls_len?" ":""),
					sdp->sp_name, *dp++);
				tmpi = strlen(cp);
				vls->vls_len += tmpi;

				while (--i > 0) {
					cp += tmpi;
					sprintf(cp, ",%d", *dp++);
					tmpi = strlen(cp);
					vls->vls_len += tmpi;
				}
			}
			break;
		case 'f':
			rt_vls_print_double(vls, sdp->sp_name, sdp->sp_count,
				(double *)loc);
			break;
		default:
			rt_log( " %s=%s??\n", sdp->sp_name, sdp->sp_fmt );
			abort();
			break;
		}
	}
}
