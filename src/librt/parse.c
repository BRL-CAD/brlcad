/*                         P A R S E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
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

/** @addtogroup librt */
/*@{*/
/** @file ./librt/parse.c
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
 */
/*@}*/

#ifndef lint
static const char RCSparse[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"



/*
 *			B U _ M A T P R I N T
 *
 *	XXX Should this be here, or could it be with the matrix support?
 *	pretty-print a matrix
 */
HIDDEN void
bu_matprint(const char *name, register const matp_t mat)
{
	int	delta = strlen(name)+2;

	/* indent the body of the matrix */
	bu_log_indent_delta(delta);

	bu_log(" %s=%.-12E %.-12E %.-12E %.-12E\n",
		name, mat[0], mat[1], mat[2], mat[3]);

	bu_log("%.-12E %.-12E %.-12E %.-12E\n",
		mat[4], mat[5], mat[6], mat[7]);

	bu_log("%.-12E %.-12E %.-12E %.-12E\n",
		mat[8], mat[9], mat[10], mat[11]);

	bu_log_indent_delta(-delta);

	bu_log("%.-12E %.-12E %.-12E %.-12E\n",
		mat[12], mat[13], mat[14], mat[15]);
}


/*
 *			B U _ S T R U C T P R I N T
 */
void
bu_structprint(const char *title, const struct bu_structparse *parsetab, const char *base)

                           	          /* structure description */
          			      	  /* base address of users structure */
{
	register const struct bu_structparse	*sdp;
	register char			*loc;
	register int			lastoff = -1;
	struct bu_vls vls;

	bu_vls_init( &vls );
	bu_log( "%s\n", title );
	if (parsetab == (struct bu_structparse *)NULL) {
		bu_log( "Null \"struct bu_structparse\" pointer\n");
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
			bu_structprint( sdp->sp_name,
				(struct bu_structparse *)sdp->sp_count,
				base );
			continue;
		}

		if ( sdp->sp_fmt[0] != '%')  {
			bu_log("bu_structprint:  %s: unknown format '%s'\n",
				sdp->sp_name, sdp->sp_fmt );
			continue;
		}
#if 0
		bu_vls_trunc( &vls, 0 );
		bu_vls_item_print( &vls, sdp, base );
		bu_log( " %s=%s\n", sdp->sp_name, bu_vls_addr(&vls) );
#else
		switch( sdp->sp_fmt[1] )  {
		case 'c':
		case 's':
			if (sdp->sp_count < 1)
				break;
			if (sdp->sp_count == 1)
				bu_log( " %s='%c'\n", sdp->sp_name, *loc);
			else
				bu_log( " %s=\"%s\"\n", sdp->sp_name,
					(char *)loc );
			break;
		case 'S':
			{
				int delta = strlen(sdp->sp_name)+2;
				register struct bu_vls *vls =
					(struct bu_vls *)loc;

				bu_log_indent_delta(delta);
				bu_log(" %s=(vls_magic)%d (vls_offset)%d (vls_len)%d (vls_max)%d\n",
					sdp->sp_name, vls->vls_magic,
					vls->vls_offset,
					vls->vls_len, vls->vls_max);
				bu_log_indent_delta(-delta);
				bu_log("\"%s\"\n", vls->vls_str+vls->vls_offset);
			}
			break;
		case 'i':
			{	register int i = sdp->sp_count;
				register short *sp = (short *)loc;

				bu_log( " %s=%hd", sdp->sp_name, *sp++ );

				while (--i > 0) bu_log( ",%d", *sp++ );

				bu_log("\n");
			}
			break;
		case 'd':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;

				bu_log( " %s=%d", sdp->sp_name, *dp++ );

				while (--i > 0) bu_log( ",%d", *dp++ );

				bu_log("\n");
			}
			break;
		case 'f':
			{	register int i = sdp->sp_count;
				register double *dp = (double *)loc;

				if (sdp->sp_count == ELEMENTS_PER_MAT) {
					bu_matprint(sdp->sp_name, (matp_t)dp);
				} else if (sdp->sp_count <= ELEMENTS_PER_VECT){
					bu_log( " %s=%.25G", sdp->sp_name, *dp++ );

					while (--i > 0)
						bu_log( ",%.25G", *dp++ );

					bu_log("\n");
				}else  {
					int delta = strlen(sdp->sp_name)+2;

					bu_log_indent_delta(delta);

					bu_log( " %s=%.25G\n", sdp->sp_name, *dp++ );

					while (--i > 1)
						bu_log( "%.25G\n", *dp++ );

					bu_log_indent_delta(-delta);
					bu_log( "%.25G\n", *dp );
				}
			}
			break;
		case 'x':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;

				bu_log( " %s=%08x", sdp->sp_name, *dp++ );

				while (--i > 0) bu_log( ",%08x", *dp++ );

				bu_log("\n");
			}
			break;
		default:
			bu_log( " bu_structprint: Unknown format: %s=%s??\n",
				sdp->sp_name, sdp->sp_fmt );
			break;
		}
#endif
	}
	bu_vls_free(&vls);
}


/*
 *			B U _ P A R S E _ D O U B L E
 *
 *  Parse an array of one or more doubles.
 *  Return value: 0 when successful
 *               <0 upon failure
 */
HIDDEN int
bu_parse_double(const char *str, long int count, double *loc)
{
	long	i;
	int	dot_seen;
	const char	*numstart;
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
			return -1;

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
			return -1;

		*loc++ = tmp_double;

		/* skip the separator */
		if (*str) str++;
	}
	return 0;
}

/*
 *			B U _ S T R U C T _ L O O K U P
 *
 *  Returns -
 *      -2      parse error
 *	-1	not found
 *	 0	entry found and processed
 */
HIDDEN int
bu_struct_lookup(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
                                    	     	/* structure description */
                   			      	/* struct member name */
    					      	/* begining of structure */
          				       	/* string containing value */
{
	register char *loc;
	int i, retval = 0;

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
			if( bu_struct_lookup(
				(struct bu_structparse *)sdp->sp_count,
				name, base, value )
			    == 0 )
				return(0);	/* found */
			else
				continue;
		}
		if (sdp->sp_fmt[0] != '%') {
			bu_log("bu_struct_lookup(%s): unknown format '%s'\n",
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
			{	struct bu_vls *vls = (struct bu_vls *)loc;
				bu_vls_init_if_uninit( vls );
				bu_vls_strcpy(vls, value);
			}
			break;
		case 'i':
			{	register short *ip = (short *)loc;
				register short tmpi;
				register const char *cp;
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
					    (*value == '+' || *value == '-'))){
					    retval = -2;
					    break;
					} else {
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
				register char const *cp;
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
					    (*value == '+' || *value == '-'))){
					    retval = -2;
					    break;
					} else {
						*(ip++) = tmpi;
						value = cp;
					}
					/* skip the separator */
					if (*value) value++;
				}
			}
			break;
		case 'f':
			retval = bu_parse_double(value, sdp->sp_count,
						 (double *)loc);
			break;
		default:
			bu_log("bu_struct_lookup(%s): unknown format '%s'\n",
				name, sdp->sp_fmt );
			return(-1);
		}
		if( sdp->sp_hook )  {
			sdp->sp_hook( sdp, name, base, value );
		}
		return(retval);		/* OK or parse error */
	}
	return(-1);			/* Not found */
}

/*
 *			B U _ S T R U C T P A R S E
 *
 *	Parse the structure element description in the vls string "vls"
 *	according to the structure description in "parsetab"
 *
 *  Returns -
 *	<0	failure
 *	 0	OK
 */
int
bu_structparse(const struct bu_vls *in_vls, const struct bu_structparse *desc, char *base)
                   		        	/* string to parse through */
                           	      		/* structure description */
    				      		/* base addr of users struct */
{
	struct bu_vls	vls;
	register char *cp;
	char	*name;
	char	*value;
	int retval;

	BU_CK_VLS(in_vls);
	if (desc == (struct bu_structparse *)NULL) {
		bu_log( "Null \"struct bu_structparse\" pointer\n");
		return(-1);
	}

	/* Duplicate the input string.  This algorithm is destructive. */
	bu_vls_init( &vls );
	bu_vls_vlscat( &vls, in_vls );
	cp = bu_vls_addr( &vls );

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
			bu_log("bu_structparse: name '%s' without '='\n",
				name );
			bu_vls_free( &vls );
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
				bu_log("bu_structparse: name '%s'=\" without closing \"\n",
					name);
				bu_vls_free( &vls );
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

		/* Lookup name in desc table and modify */
		retval = bu_struct_lookup( desc, name, base, value );
		if( retval == -1 ) {
		    bu_log("bu_structparse:  '%s=%s', element name not found in:\n",
			   name, value);
		    bu_structprint( "troublesome one", desc, base );
		} else if( retval == -2 ) {
		    bu_vls_free( &vls );
		    return -2;
		}

	}
	bu_vls_free( &vls );
	return(0);
}


HIDDEN void
bu_vls_item_print_core(struct bu_vls *vp, const struct bu_structparse *sdp, const char *base, char sep_char)

                                     /* item description */
                                  /* base address of users structure */
                                  /* value separator */
{
    register char *loc;

    if (sdp == (struct bu_structparse *)NULL) {
	bu_log( "Null \"struct bu_structparse\" pointer\n");
	return;
    }

#if CRAY && !__STDC__
    loc = (char *)(base + ((int)sdp->sp_offset*sizeof(int)));
#else
    loc = (char *)(base + ((int)sdp->sp_offset));
#endif

    if (sdp->sp_fmt[0] == 'i' )  {
	bu_log( "Cannot print type 'i' yet!\n" );
	return;
    }

    if ( sdp->sp_fmt[0] != '%')  {
	bu_log("bu_vls_item_print:  %s: unknown format '%s'\n",
	       sdp->sp_name, sdp->sp_fmt );
	return;
    }

    switch( sdp->sp_fmt[1] )  {
    case 'c':
    case 's':
	if (sdp->sp_count < 1)
	    break;
	if (sdp->sp_count == 1)
	    bu_vls_printf( vp, "%c", *loc );
	else
	    bu_vls_printf( vp, "%s", (char *)loc );
	break;
    case 'S': {
	register struct bu_vls *vls = (struct bu_vls *)loc;

	bu_vls_vlscat( vp, vls ); }
	break;
    case 'i': {
	register int i = sdp->sp_count;
	register short *sp = (short *)loc;

	bu_vls_printf( vp, "%hd", *sp++ );
	while( --i > 0 ) bu_vls_printf( vp, "%c%hd", sep_char, *sp++ ); }
	break;
    case 'd': {
	register int i = sdp->sp_count;
	register int *dp = (int *)loc;

	bu_vls_printf( vp, "%d", *dp++ );
	while( --i > 0 ) bu_vls_printf( vp, "%c%d", sep_char, *dp++ ); }
	break;
    case 'f': {
	register int i = sdp->sp_count;
	register double *dp = (double *)loc;

	bu_vls_printf( vp, "%.25G", *dp++ );
	while( --i > 0 ) bu_vls_printf( vp, "%c%.25G", sep_char, *dp++ ); }
	break;
    case 'x': {
	register int i = sdp->sp_count;
	register int *dp = (int *)loc;

	bu_vls_printf( vp, "%08x", *dp++ );
	while( --i > 0 ) bu_vls_printf( vp, "%c%08x", sep_char, *dp++ );  }
	break;
    default:
	break;
    }
}



/*
 *                     B U _ V L S _ I T E M _ P R I N T
 *
 * Takes the single item pointed to by "sp", and prints its value into a
 * vls.
 */

void
bu_vls_item_print(struct bu_vls *vp, const struct bu_structparse *sdp, const char *base)

                                      /* item description */
                                  /* base address of users structure */
{
    bu_vls_item_print_core( vp, sdp, base, ',' );
}

/*
 *    B U _ V L S _ I T E M _ P R I N T _ N C
 *
 *    A "no-commas" version of the bu_vls_item_print() routine.
 */

void
bu_vls_item_print_nc(struct bu_vls *vp, const struct bu_structparse *sdp, const char *base)

                                      /* item description */
                                  /* base address of users structure */
{
    bu_vls_item_print_core( vp, sdp, base, ' ' );
}

/*                  B U _ V L S _ N A M E _ P R I N T
 *
 * A version of bu_vls_item_print that allows you to select by name.
 */

int
bu_vls_name_print(struct bu_vls *vp, const struct bu_structparse *parsetab, const char *name, const char *base)
{
    register const struct bu_structparse *sdp;

    for( sdp = parsetab; sdp->sp_name != NULL; sdp++ )
	if( strcmp(sdp->sp_name, name) == 0 ) {
	    bu_vls_item_print( vp, sdp, base );
	    return 0;
	}

    return -1;
}

/*                  B U _ V L S _ N A M E _ P R I N T _ N C
 *
 * A "no-commas" version of bu_vls_name_print
 */

int
bu_vls_name_print_nc(struct bu_vls *vp, const struct bu_structparse *parsetab, const char *name, const char *base)
{
    register const struct bu_structparse *sdp;

    for( sdp = parsetab; sdp->sp_name != NULL; sdp++ )
	if( strcmp(sdp->sp_name, name) == 0 ) {
	    bu_vls_item_print_nc( vp, sdp, base );
	    return 0;
	}

    return -1;
}


/*
 *			B U _ V L S _ P R I N T _ D O U B L E
 */
HIDDEN void
bu_vls_print_double(struct bu_vls *vls, const char *name, register long int count, register const double *dp)
{
	register int tmpi;
	register char *cp;

	bu_vls_extend(vls, strlen(name) + 3 + 32 * count);

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
 *			B U _ V L S _ S T R U C T P R I N T
 *
 *	This differs from bu_structprint in that this output is less readable
 *	by humans, but easier to parse with the computer.
 */
void
bu_vls_structprint(struct bu_vls *vls, register const struct bu_structparse *sdp, const char *base)
      	      				     	/* vls to print into */
                                    	     	/* structure description */
          				      	/* structure ponter */
{
	register char			*loc;
	register int			lastoff = -1;
	register char			*cp;

	BU_CK_VLS(vls);

	if (sdp == (struct bu_structparse *)NULL) {
		bu_log( "Null \"struct bu_structparse\" pointer\n");
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
			struct bu_vls sub_str;

			bu_vls_init(&sub_str);
			bu_vls_structprint( &sub_str,
				(struct bu_structparse *)sdp->sp_count,
				base );

			bu_vls_vlscat(vls, &sub_str);
			bu_vls_free( &sub_str );
			continue;
		}

		if ( sdp->sp_fmt[0] != '%' )  {
			bu_log("bu_structprint:  %s: unknown format '%s'\n",
				sdp->sp_name, sdp->sp_fmt );
			break;
		}

		switch( sdp->sp_fmt[1] )  {
		case 'c':
		case 's':
			if (sdp->sp_count < 1)
				break;
			if (sdp->sp_count == 1) {
				bu_vls_extend(vls, strlen(sdp->sp_name)+6);
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
				bu_vls_extend(vls, strlen(sdp->sp_name)+
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
			{	register struct bu_vls *vls_p =
					(struct bu_vls *)loc;

				bu_vls_extend(vls, bu_vls_strlen(vls_p) + 5 +
					strlen(sdp->sp_name) );

				cp = vls->vls_str + vls->vls_offset + vls->vls_len;
				sprintf(cp, "%s%s=\"%s\"",
					(vls->vls_len?" ":""),
					sdp->sp_name,
					bu_vls_addr(vls_p) );
				vls->vls_len += strlen(cp);
			}
			break;
		case 'i':
			{	register int i = sdp->sp_count;
				register short *sp = (short *)loc;
				register int tmpi;

				bu_vls_extend(vls,
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

				bu_vls_extend(vls,
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
			bu_vls_print_double(vls, sdp->sp_name, sdp->sp_count,
				(double *)loc);
			break;
		default:
			bu_log( " %s=%s??\n", sdp->sp_name, sdp->sp_fmt );
			abort();
			break;
		}
	}
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
