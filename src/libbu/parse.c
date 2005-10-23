/*                         P A R S E . C
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
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

/** @file ./libbu/parse.c
 *  Routines to assign values to elements of arbitrary structures.
 *  The layout of a structure to be processed is described by
 *  a structure of type "bu_structparse", giving element names, element
 *  formats, an offset from the beginning of the structure, and
 *  a pointer to an optional "hooked" function that is called whenever
 *  that structure element is changed.
 *
 *  There are four basic operations supported:
 *	print	struct elements to ASCII
 *	parse	ASCII to struct elements
 *	export	struct elements to machine-independent binary
 *	import	machine-independent binary to struct elements
 *
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
#include "bu.h"


#define CKMEM( _len )	{  \
	register int	offset; \
	if( (offset = (ep - cp) - (_len)) < 0 )  { \
		do  { \
			offset += ext->ext_nbytes;	/* decr by new growth */ \
			ext->ext_nbytes <<= 1; \
		} while( offset < 0 ); \
		offset = cp - (char *)ext->ext_buf; \
		ext->ext_buf = (genptr_t)bu_realloc( (char *) ext->ext_buf, \
		     ext->ext_nbytes, "bu_struct_export" ); \
		ep = (char *) ext->ext_buf + ext->ext_nbytes; \
		cp = (char *) ext->ext_buf + offset; \
	} }

#define	BU_GETPUT_MAGIC_1	0x15cb
#define BU_GETPUT_MAGIC_2	0xbc51
#define BU_INIT_GETPUT_1(_p)	{ \
	BU_CK_EXTERNAL(_p); \
	((unsigned char *) _p->ext_buf)[1] = (BU_GETPUT_MAGIC_1 & 0xFF); \
	((unsigned char *) _p->ext_buf)[0] = (BU_GETPUT_MAGIC_1 >> 8) & 0xFF; \
	}
#define BU_INIT_GETPUT_2(_p,_l)	{\
	BU_CK_EXTERNAL(_p); \
	((unsigned char *) _p->ext_buf)[_l-1] = (BU_GETPUT_MAGIC_2 & 0xFF); \
	((unsigned char *) _p->ext_buf)[_l-2] = (BU_GETPUT_MAGIC_2 >> 8) & 0xFF; \
	}

#define	BU_CK_GETPUT(_p) {\
	register long _i; \
	register long _len; \
	BU_CK_EXTERNAL(_p); \
	if ( !(_p->ext_buf) ) { \
		bu_log("ERROR: BU_CK_GETPUT null ext_buf, file %s, line %d\n", \
		    __FILE__, __LINE__); \
		bu_bomb("NULL pointer"); \
	} \
	if ( _p->ext_nbytes < 6 ) { \
		bu_log("ERROR: BU_CK_GETPUT buffer only %d bytes, file %s, line %d\n", \
		    _p->ext_nbytes, __FILE__, __LINE__); \
		bu_bomb("getput buffer too small"); \
	} \
	_i = (((unsigned char *)(_p->ext_buf))[0] << 8) | \
	      ((unsigned char *)(_p->ext_buf))[1]; \
	if ( _i != BU_GETPUT_MAGIC_1)  { \
		bu_log("ERROR: BU_CK_GETPUT buffer x%x, magic1 s/b x%x, was %s(x%x), file %s, line %d\n", \
		    _p->ext_buf, BU_GETPUT_MAGIC_1, \
		    bu_identify_magic( _i), _i, __FILE__, __LINE__); \
		bu_bomb("Bad getput buffer"); \
	} \
	_len = (((unsigned char *)(_p->ext_buf))[2] << 24) | \
	       (((unsigned char *)(_p->ext_buf))[3] << 16) | \
	       (((unsigned char *)(_p->ext_buf))[4] <<  8) | \
		((unsigned char *)(_p->ext_buf))[5]; \
	if (_len > _p->ext_nbytes) { \
		bu_log("ERROR: BU_CK_GETPUT buffer x%x, expected len=%d, ext_nbytes=%d, file %s, line %d\n", \
		    _p->ext_buf, _len, _p->ext_nbytes, \
		    __FILE__, __LINE__); \
		bu_bomb("Bad getput buffer"); \
	} \
	_i = (((unsigned char *)(_p->ext_buf))[_len-2] << 8) | \
	      ((unsigned char *)(_p->ext_buf))[_len-1]; \
	if ( _i != BU_GETPUT_MAGIC_2) { \
		bu_log("ERROR: BU_CK_GETPUT buffer x%x, magic2 s/b x%x, was %s(x%x), file %s, line %d\n", \
		    _p->ext_buf, BU_GETPUT_MAGIC_2, \
		    bu_identify_magic( _i), _i, __FILE__, __LINE__); \
		bu_bomb("Bad getput buffer"); \
	} \
}

/*
 *			B U _ S T R U C T _ E X P O R T
 */
int
bu_struct_export(struct bu_external *ext, const genptr_t base, const struct bu_structparse *imp)
{
	register char	*cp;		/* current possition in buffer */
	char		*ep;		/* &ext->ext_buf[ext->ext_nbytes] */
	const struct bu_structparse *ip;	/* current imexport structure */
	char		*loc;		/* where host-format data is */
	int		len;
	register int	i;

	BU_INIT_EXTERNAL(ext);

	ext->ext_nbytes = 480;
	ext->ext_buf = (genptr_t)bu_malloc( ext->ext_nbytes,
	    "bu_struct_export output ext->ext_buf" );
	BU_INIT_GETPUT_1(ext);
	cp = (char *) ext->ext_buf + 6; /* skip magic and length */
	ep = cp + ext->ext_nbytes;

	for( ip = imp; ip->sp_fmt[0] != '\0'; ip++ )  {

#if CRAY && !__STDC__
		loc = ((char *)base) + ((int)ip->sp_offset*sizeof(int));
#else
		loc = ((char *)base) + ip->sp_offset;
#endif

		switch( ip->sp_fmt[0] )  {
		case 'i':
			/* Indirect to another structure */
			/* deferred */
			bu_free( (char *) ext->ext_buf, "output ext_buf" );
			return( 0 );
		case '%':
			/* See below */
			break;
		default:
			/* Unknown */
			bu_free( (char *) ext->ext_buf, "output ext_buf" );
			return( 0 );
		}
		/* [0] == '%', use printf-like format char */
		switch( ip->sp_fmt[1] )  {
		case 'f':
			/* Double-precision floating point */
			len = ip->sp_count * 8;
			CKMEM( len );
			htond( (unsigned char *)cp, (unsigned char *)loc, ip->sp_count );
			cp += len;
			continue;
		case 'd':
			/* 32-bit network integer, from "int" */
			CKMEM( ip->sp_count * 4 );
			{
				register unsigned long	l;
				for( i = ip->sp_count-1; i >= 0; i-- )  {
					l = *((int *)loc);
					cp[3] = l;
					cp[2] = l >> 8;
					cp[1] = l >> 16;
					cp[0] = l >> 24;
					loc += sizeof(int);
					cp += 4;
				}
			}
			continue;
		case 'i':
			/* 16-bit integer, from "int" */
			CKMEM( ip->sp_count * 2 );
			{
				register unsigned short	s;
				for( i = ip->sp_count-1; i >= 0; i-- )  {
					s = *((int *)loc);
					cp[1] = s;
					cp[0] = s >> 8;
					loc += sizeof(int); /* XXX */
					cp += 2;
				}
			}
			continue;
		case 's':
			{
				/* char array is transmitted as a
				 * 4 byte character count, followed by a
				 * null terminated, word padded char array.
				 * The count includes any pad bytes,
				 * but not the count itself.
				 *
				 * ip->sp_count == sizeof(char array)
				 */
				register int lenstr;

				/* include the terminating null */
				lenstr = strlen( loc ) + 1;

				len = lenstr;

				/* output an integer number of words */
				if ((len & 0x03) != 0)
					len += 4 - (len & 0x03);

				CKMEM( len + 4 );

				/* put the length on the front
				 * of the string
				 */
				cp[3] = len;
				cp[2] = len >> 8;
				cp[1] = len >> 16;
				cp[0] = len >> 24;

				cp += 4;

				bcopy( loc, cp, lenstr );
				cp += lenstr;
				while (lenstr++ < len) *cp++ = '\0';
			}
			continue;
		case 'c':
			{
				CKMEM( ip->sp_count + 4 );
				cp[3] = ip->sp_count;
				cp[2] = ip->sp_count >> 8;
				cp[1] = ip->sp_count >> 16;
				cp[0] = ip->sp_count >> 24;
				cp += 4;
				bcopy( loc, cp, ip->sp_count);
				cp += ip->sp_count;
			}
			continue;
		default:
			bu_free( (char *) ext->ext_buf, "output ext_buf" );
			return( 0 );
		}
	}
	CKMEM( 2);	/* get room for the trailing magic number */
	cp += 2;

	i = cp - (char *)ext->ext_buf;
	/* Fill in length in external buffer */
	((char *)ext->ext_buf)[5] = i;
	((char *)ext->ext_buf)[4] = i >> 8;
	((char *)ext->ext_buf)[3] = i >>16;
	((char *)ext->ext_buf)[2] = i >>24;
	BU_INIT_GETPUT_2(ext, i);
	ext->ext_nbytes = i;	/* XXX this changes nbytes if i < 480 ? */
	return( 1 );
}

/*
 *			B U _ S T R U C T _ I M P O R T
 */
int
bu_struct_import(genptr_t base, const struct bu_structparse *imp, const struct bu_external *ext)
{
	register const unsigned char	*cp;	/* current possition in buffer */
	const struct bu_structparse	*ip;	/* current imexport structure */
	char		*loc;		/* where host-format data is */
	int		len;
	int		bytes_used;
	register int	i;

	BU_CK_GETPUT(ext);

	cp = (unsigned char *)ext->ext_buf+6;
	bytes_used = 0;
	for( ip = imp; ip->sp_fmt[0] != '\0'; ip++ )  {

#if CRAY && !__STDC__
		loc = ((char *)base) + ((int)ip->sp_offset*sizeof(int));
#else
		loc = ((char *)base) + ip->sp_offset;
#endif

		switch( ip->sp_fmt[0] )  {
		case 'i':
			/* Indirect to another structure */
			/* deferred */
			return( -1 );
		case '%':
			/* See below */
			break;
		default:
			/* Unknown */
			return( -1 );
		}
		/* [0] == '%', use printf-like format char */
		switch( ip->sp_fmt[1] )  {
		case 'f':
			/* Double-precision floating point */
			len = ip->sp_count * 8;
			ntohd( (unsigned char *)loc, cp, ip->sp_count );
			cp += len;
			bytes_used += len;
			break;
		case 'd':
			/* 32-bit network integer, from "int" */
			{
				register long	l;
				for( i = ip->sp_count-1; i >= 0; i-- )  {
					l =	(cp[0] << 24) |
						(cp[1] << 16) |
						(cp[2] <<  8) |
						 cp[3];
					*(int *)loc = l;
					loc += sizeof(int); /* XXX */
					cp += 4;
				}
				bytes_used += ip->sp_count * 4;
			}
			break;
		case 'i':
			/* 16-bit integer, from "int" */
			for( i = ip->sp_count-1; i >= 0; i-- )  {
				*(int *)loc =	(cp[0] <<  8) |
						 cp[1];
				loc += sizeof(int); /* XXX */
				cp += 2;
			}
			bytes_used += ip->sp_count * 2;
			break;
		case 's':
			{	/* char array transmitted as a
				 * 4 byte character count, followed by a
				 * null terminated, word padded char array
				 *
				 * ip->sp_count == sizeof(char array)
				 */
				register unsigned long lenstr;

				lenstr = (cp[0] << 24) |
					 (cp[1] << 16) |
					 (cp[2] <<  8) |
					  cp[3];

				cp += 4;

				/* don't read more than the buffer can hold */
				if (ip->sp_count < lenstr)
					bcopy( cp, loc, ip->sp_count);
				else
					bcopy( cp, loc, lenstr );

				/* ensure proper null termination */
				loc[ip->sp_count-1] = '\0';

				cp += lenstr;
				bytes_used += lenstr;
			}
			break;
		case 'c':
			{
				register unsigned long lenarray;

				lenarray = (cp[0] << 24) |
					   (cp[1] << 16) |
					   (cp[2] <<  8) |
					    cp[3];
				cp += 4;

				if (ip->sp_count < lenarray) {
					bcopy( cp, loc, ip->sp_count);
				} else {
					bcopy( cp, loc, lenarray );
				}
				cp += lenarray;
				bytes_used += lenarray;
			}
			break;
		default:
			return( -1 );
		}
		if ( ip->sp_hook ) {

			ip->sp_hook (ip, ip->sp_name, base, (char *)NULL);
		}
	}

	/* This number may differ from that stored as "claimed_length" */
	return( bytes_used );
}

/*
 *			B U _ S T R U C T _ P U T
 *
 *  Put a structure in external form to a stdio file.
 *  All formatting must have been accomplished previously.
 */
int
bu_struct_put(FILE *fp, const struct bu_external *ext)
{
	BU_CK_GETPUT(ext);

	return(fwrite(ext->ext_buf, 1, ext->ext_nbytes, fp));
}

/*
 *			B U _ S T R U C T _ G E T
 *
 *  Obtain the next structure in external form from a stdio file.
 */
int
bu_struct_get(struct bu_external *ext, FILE *fp)
{
	register long i, len;

	BU_INIT_EXTERNAL(ext);
	ext->ext_buf = (genptr_t) bu_malloc( 6, "bu_struct_get buffer head");
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */

	i=fread( (char *) ext->ext_buf, 1, 6, fp);	/* res_syscall */
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */

	if (i != 6 ) {
		if (i == 0) return(0);
		bu_log("ERROR: bu_struct_get bad fread (%d), file %s, line %d\n",
		    i, __FILE__, __LINE__);
		bu_bomb("Bad fread");
	}
	i = (((unsigned char *)(ext->ext_buf))[0] << 8) |
	     ((unsigned char *)(ext->ext_buf))[1];
	len = (((unsigned char *)(ext->ext_buf))[2] << 24) |
	      (((unsigned char *)(ext->ext_buf))[3] << 16) |
	      (((unsigned char *)(ext->ext_buf))[4] <<  8) |
	       ((unsigned char *)(ext->ext_buf))[5];
	if ( i != BU_GETPUT_MAGIC_1) {
		bu_log("ERROR: bad getput buffer header x%x, s/b x%x, was %s(x%x), file %s, line %d\n",
		    ext->ext_buf, BU_GETPUT_MAGIC_1,
		    bu_identify_magic( i), i, __FILE__, __LINE__);
		bu_bomb("bad getput buffer");
	}
	ext->ext_nbytes = len;
	ext->ext_buf = (genptr_t) bu_realloc((char *) ext->ext_buf, len,
	    "bu_struct_get full buffer");
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	i=fread((char *) ext->ext_buf + 6, 1 , len-6, fp);	/* res_syscall */
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	if (i != len-6) {
		bu_log("ERROR: bu_struct_get bad fread (%d), file %s, line %d\n",
		    i, __FILE__, __LINE__);
		bu_bomb("Bad fread");
	}
	i = (((unsigned char *)(ext->ext_buf))[len-2] <<8) |
	     ((unsigned char *)(ext->ext_buf))[len-1];
	if ( i != BU_GETPUT_MAGIC_2) {
		bu_log("ERROR: bad getput buffer x%x, s/b x%x, was %s(x%x), file %s, line %d\n",
		    ext->ext_buf, BU_GETPUT_MAGIC_2,
		    bu_identify_magic( i), i, __FILE__, __LINE__);
		bu_bomb("Bad getput buffer");
	}
	return(1);
}

/*
 *			B U _ S T R U C T _ B U F
 *
 *  Given a buffer with an external representation of a structure
 *  (e.g. the ext_buf portion of the output from bu_struct_export),
 *  check it for damage in shipment, and if it's OK,
 *  wrap it up in an bu_external structure, suitable for passing
 *  to bu_struct_import().
 */
void
bu_struct_wrap_buf(struct bu_external *ext, genptr_t buf)
{
	register long i, len;

	BU_INIT_EXTERNAL(ext);
	ext->ext_buf = buf;
	i = (((unsigned char *)(ext->ext_buf))[0] << 8) |
	     ((unsigned char *)(ext->ext_buf))[1];
	len = (((unsigned char *)(ext->ext_buf))[2] << 24) |
	      (((unsigned char *)(ext->ext_buf))[3] << 16) |
	      (((unsigned char *)(ext->ext_buf))[4] <<  8) |
	       ((unsigned char *)(ext->ext_buf))[5];
	if ( i != BU_GETPUT_MAGIC_1) {
		bu_log("ERROR: bad getput buffer header x%x, s/b x%x, was %s(x%x), file %s, line %d\n",
		    ext->ext_buf, BU_GETPUT_MAGIC_1,
		    bu_identify_magic( i), i, __FILE__, __LINE__);
		bu_bomb("bad getput buffer");
	}
	ext->ext_nbytes = len;
	i = (((unsigned char *)(ext->ext_buf))[len-2] <<8) |
	     ((unsigned char *)(ext->ext_buf))[len-1];
	if ( i != BU_GETPUT_MAGIC_2) {
		bu_log("ERROR: bad getput buffer x%x, s/b x%x, was %s(x%x), file %s, line %s\n",
		    ext->ext_buf, BU_GETPUT_MAGIC_2,
		    bu_identify_magic( i), i, __FILE__, __LINE__);
		bu_bomb("Bad getput buffer");
	}
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
bu_struct_lookup(register const struct bu_structparse *sdp, register const char *name, const char *base, const char *const value)
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
		loc = (char *)(base + sdp->sp_offset);
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

				/* Don't null terminate chars, only strings */
				if (sdp->sp_count > 1)  {
					/* OK, it's a string */
					if( j < sdp->sp_count-1 )
						loc[j] = '\0';
					else
						loc[sdp->sp_count-1] = '\0';
				}
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
				register const char *pv = value;

				for (i=0 ; i < sdp->sp_count && *pv ; ++i){
					tmpi = atoi( pv );

					cp = pv;
					if (*cp && (*cp == '+' || *cp == '-'))
						cp++;

					while (*cp && isdigit(*cp) )
						cp++;

					/* make sure we actually had an
					 * integer out there
					 */
					if (cp == pv ||
					    (cp == pv+1 &&
					    (*pv == '+' || *pv == '-'))){
					    retval = -2;
					    break;
					} else {
						*(ip++) = tmpi;
						pv = cp;
					}
					/* skip the separator */
					if (*pv) pv++;
				}
			}
			break;
		case 'd':
			{	register int *ip = (int *)loc;
				register int tmpi;
				register char const *cp;
				register const char *pv = value;

				/* Special case:  '=!' toggles a boolean */
				if( *pv == '!' )  {
					*ip = *ip ? 0 : 1;
					pv++;
					break;
				}
				/* Normal case: an integer */
				for (i=0 ; i < sdp->sp_count && *pv ; ++i){
					tmpi = atoi( pv );

					cp = pv;
					if (*cp && (*cp == '+' || *cp == '-'))
						cp++;

					while (*cp && isdigit(*cp) )
						cp++;

					/* make sure we actually had an
					 * integer out there
					 */
					if (cp == pv ||
					    (cp == pv+1 &&
					    (*pv == '+' || *pv == '-'))){
					    retval = -2;
					    break;
					} else {
						*(ip++) = tmpi;
						pv = cp;
					}
					/* skip the separator */
					if (*pv) pv++;
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
bu_struct_parse(const struct bu_vls *in_vls, const struct bu_structparse *desc, const char *base)
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
			bu_log("bu_structparse: input keyword '%s' is not followed by '=' in '%s'\nInput must be in keyword=value format.\n",
				name, bu_vls_addr(in_vls) );
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
				bu_log("bu_structparse: keyword '%s'=\" without closing \"\n",
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
		    bu_log("bu_structparse:  '%s=%s', keyword not found in:\n",
			   name, value);
		    bu_struct_print( "troublesome one", desc, base );
		} else if( retval == -2 ) {
		    bu_vls_free( &vls );
		    return -2;
		}

	}
	bu_vls_free( &vls );
	return(0);
}


/*
 *			B U _ M A T P R I N T
 *
 *	XXX Should this be here, or could it be with the matrix support?
 *	pretty-print a matrix
 */
HIDDEN void
bu_matprint(const char *name, register const double *mat)
{
	int	delta = strlen(name)+2;

	/* indent the body of the matrix */
	bu_log_indent_delta(delta);

	bu_log(" %s=%12E %12E %12E %12E\n",
		name, mat[0], mat[1], mat[2], mat[3]);

	bu_log("%12E %12E %12E %12E\n",
		mat[4], mat[5], mat[6], mat[7]);

	bu_log("%12E %12E %12E %12E\n",
		mat[8], mat[9], mat[10], mat[11]);

	bu_log_indent_delta(-delta);

	bu_log("%12E %12E %12E %12E\n",
		mat[12], mat[13], mat[14], mat[15]);
}

HIDDEN void
bu_vls_matprint(struct bu_vls		*vls,
		const char		*name,
		register const double	*mat)
{
	int	delta = strlen(name)+2;

	/* indent the body of the matrix */
	bu_log_indent_delta(delta);

	bu_vls_printf(vls, " %s=%12E %12E %12E %12E\n",
		      name, mat[0], mat[1], mat[2], mat[3]);
	bu_log_indent_vls(vls);

	bu_vls_printf(vls, "%12E %12E %12E %12E\n",
		      mat[4], mat[5], mat[6], mat[7]);
	bu_log_indent_vls(vls);

	bu_vls_printf(vls, "%12E %12E %12E %12E\n",
		      mat[8], mat[9], mat[10], mat[11]);
	bu_log_indent_vls(vls);

	bu_log_indent_delta(-delta);

	bu_vls_printf(vls, "%12E %12E %12E %12E\n",
		      mat[12], mat[13], mat[14], mat[15]);
}

/*
 *
 *	Convert a structure element (indicated by sdp) to its ASCII
 *	representation in a VLS
 */
void
bu_vls_struct_item(struct bu_vls *vp, const struct bu_structparse *sdp, const char *base, int sep_char)

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
    loc = (char *)(base + sdp->sp_offset);
#endif

    if (sdp->sp_fmt[0] == 'i' )  {
	bu_log( "Cannot print type 'i' yet!\n" );
	return;
    }

    if ( sdp->sp_fmt[0] != '%')  {
	bu_log("bu_vls_struct_item:  %s: unknown format '%s'\n",
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

	bu_vls_printf( vp, "%d", *sp++ );
	while( --i > 0 ) bu_vls_printf( vp, "%c%d", sep_char, *sp++ ); }
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
 *	B U _ V L S _ S T R U C T _ I T E M _ N A M E D
 *
 *	Convert a structure element called "name" to an ASCII representation
 *	in a VLS.
 */
int
bu_vls_struct_item_named(struct bu_vls *vp, const struct bu_structparse *parsetab, const char *name, const char *base, int sep_char)
{
    register const struct bu_structparse *sdp;

    for( sdp = parsetab; sdp->sp_name != NULL; sdp++ )
	if( strcmp(sdp->sp_name, name) == 0 ) {
	    bu_vls_struct_item( vp, sdp, base, sep_char );
	    return 0;
	}

    return -1;
}


/*
 *			B U _ S T R U C T P R I N T
 */
void
bu_struct_print(const char *title, const struct bu_structparse *parsetab, const char *base)

                           	          /* structure description */
          			      	  /* base address of users structure */
{
	register const struct bu_structparse	*sdp;
	register char			*loc;
	register int			lastoff = -1;

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
		loc = (char *)(base + sdp->sp_offset);
#endif

		if (sdp->sp_fmt[0] == 'i' )  {
			bu_struct_print( sdp->sp_name,
				(struct bu_structparse *)sdp->sp_count,
				base );
			continue;
		}

		if ( sdp->sp_fmt[0] != '%')  {
			bu_log("bu_struct_print:  %s: unknown format '%s'\n",
				sdp->sp_name, sdp->sp_fmt );
			continue;
		}
#if 0
		bu_vls_trunc( &vls, 0 );
		bu_vls_struct_item( &vls, sdp, base, ',' );
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

				bu_log( " %s=%d", sdp->sp_name, *sp++ );

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

				if (sdp->sp_count == 16) {
					bu_matprint(sdp->sp_name, dp);
				} else if (sdp->sp_count <= 3){
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
			bu_log( " bu_struct_print: Unknown format: %s=%s??\n",
				sdp->sp_name, sdp->sp_fmt );
			break;
		}
#endif
	}
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
 *	This differs from bu_struct_print in that this output is less readable
 *	by humans, but easier to parse with the computer.
 */
void
bu_vls_struct_print(struct bu_vls *vls, register const struct bu_structparse *sdp, const char *base)
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
		loc = (char *)(base + sdp->sp_offset);
#endif

		if (sdp->sp_fmt[0] == 'i')  {
			struct bu_vls sub_str;

			bu_vls_init(&sub_str);
			bu_vls_struct_print( &sub_str,
				(struct bu_structparse *)sdp->sp_count,
				base );

			bu_vls_vlscat(vls, &sub_str);
			bu_vls_free( &sub_str );
			continue;
		}

		if ( sdp->sp_fmt[0] != '%' )  {
			bu_log("bu_struct_print:  %s: unknown format '%s'\n",
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
 *			B U _ V L S _ S T R U C T P R I N T 2
 *
 *	This differs from bu_struct_print in that it prints to a vls.
 */
void
bu_vls_struct_print2(struct bu_vls			*vls_out,
		     const char				*title,
		     const struct bu_structparse	*parsetab,	/* structure description */
		     const char				*base)	  	/* base address of users structure */
{
	register const struct bu_structparse	*sdp;
	register char			*loc;
	register int			lastoff = -1;

	bu_vls_printf(vls_out, "%s\n", title);
	if (parsetab == (struct bu_structparse *)NULL) {
		bu_vls_printf(vls_out, "Null \"struct bu_structparse\" pointer\n");
		return;
	}

	for (sdp = parsetab; sdp->sp_name != (char *)0; sdp++) {

		/* Skip alternate keywords for same value */
		if (lastoff == sdp->sp_offset)
			continue;
		lastoff = sdp->sp_offset;

#if CRAY && !__STDC__
		loc = (char *)(base + ((int)sdp->sp_offset*sizeof(int)));
#else
		loc = (char *)(base + sdp->sp_offset);
#endif

		if (sdp->sp_fmt[0] == 'i' )  {
			bu_vls_struct_print2(vls_out, sdp->sp_name,
					     (struct bu_structparse *)sdp->sp_count,
					     base);
			continue;
		}

		if (sdp->sp_fmt[0] != '%') {
			bu_vls_printf(vls_out, "bu_vls_struct_print:  %s: unknown format '%s'\n",
			       sdp->sp_name, sdp->sp_fmt );
			continue;
		}

		switch( sdp->sp_fmt[1] )  {
		case 'c':
		case 's':
			if (sdp->sp_count < 1)
				break;
			if (sdp->sp_count == 1)
				bu_vls_printf(vls_out, " %s='%c'\n", sdp->sp_name, *loc);
			else
				bu_vls_printf(vls_out, " %s=\"%s\"\n", sdp->sp_name,
					(char *)loc );
			break;
		case 'S':
			{
				int delta = strlen(sdp->sp_name)+2;
				register struct bu_vls *vls =
					(struct bu_vls *)loc;

				bu_log_indent_delta(delta);
				bu_vls_printf(vls_out, " %s=(vls_magic)%d (vls_offset)%d (vls_len)%d (vls_max)%d\n",
					sdp->sp_name, vls->vls_magic,
					vls->vls_offset,
					vls->vls_len, vls->vls_max);
				bu_log_indent_vls(vls_out);
				bu_log_indent_delta(-delta);
				bu_vls_printf(vls_out, "\"%s\"\n", vls->vls_str+vls->vls_offset);
			}
			break;
		case 'i':
			{	register int i = sdp->sp_count;
				register short *sp = (short *)loc;

				bu_vls_printf(vls_out, " %s=%d", sdp->sp_name, *sp++ );

				while (--i > 0)
					bu_vls_printf(vls_out, ",%d", *sp++ );

				bu_vls_printf(vls_out, "\n");
			}
			break;
		case 'd':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;

				bu_vls_printf(vls_out, " %s=%d", sdp->sp_name, *dp++ );

				while (--i > 0)
					bu_vls_printf(vls_out, ",%d", *dp++ );

				bu_vls_printf(vls_out, "\n");
			}
			break;
		case 'f':
			{	register int i = sdp->sp_count;
				register double *dp = (double *)loc;

				if (sdp->sp_count == 16) {
					bu_vls_matprint(vls_out, sdp->sp_name, dp);
				} else if (sdp->sp_count <= 3){
					bu_vls_printf(vls_out, " %s=%.25G", sdp->sp_name, *dp++ );

					while (--i > 0)
						bu_vls_printf(vls_out, ",%.25G", *dp++ );

					bu_vls_printf(vls_out, "\n");
				}else  {
					int delta = strlen(sdp->sp_name)+2;

					bu_log_indent_delta(delta);
					bu_vls_printf(vls_out, " %s=%.25G\n", sdp->sp_name, *dp++ );
					bu_log_indent_vls(vls_out);

					while (--i > 1) {
						bu_vls_printf(vls_out, "%.25G\n", *dp++ );
						bu_log_indent_vls(vls_out);
					}

					bu_log_indent_delta(-delta);
					bu_vls_printf(vls_out, "%.25G\n", *dp );
				}
			}
			break;
		case 'x':
			{	register int i = sdp->sp_count;
				register int *dp = (int *)loc;

				bu_vls_printf(vls_out, " %s=%08x", sdp->sp_name, *dp++ );

				while (--i > 0)
					bu_vls_printf(vls_out, ",%08x", *dp++ );

				bu_vls_printf(vls_out, "\n");
			}
			break;
		default:
			bu_vls_printf(vls_out, " bu_vls_struct_print2: Unknown format: %s=%s??\n",
				sdp->sp_name, sdp->sp_fmt );
			break;
		}
	}
}


/* This allows us to specify the "size" parameter as values like ".5m"
 * or "27in" rather than using mm all the time.
 */
void
bu_parse_mm(register const struct bu_structparse *sdp, register const char *name, char *base, const char *value)
                                    	     	/* structure description */
                   			      	/* struct member name */
    					      	/* begining of structure */
          				       	/* string containing value */
{
	double *p = (double *)(base+sdp->sp_offset);

	/* reconvert with optional units */
	*p = bu_mm_value(value);
}

#define STATE_UNKNOWN		0
#define STATE_IN_KEYWORD	1
#define STATE_IN_VALUE		2
#define STATE_IN_QUOTED_VALUE	3

int
bu_key_eq_to_key_val(char *in, char **next, struct bu_vls *vls)
{
	char *iptr=in;
	char *start;
	int state=STATE_IN_KEYWORD;

	BU_CK_VLS( vls );

	*next = NULL;

	while ( *iptr )
	{
		char *prev='\0';

		switch( state )
		{
			case STATE_IN_KEYWORD:
				/* skip leading white space */
				while( isspace( *iptr ) )
					iptr++;

				if( !(*iptr) )
					break;

				if( *iptr == ';' )
				{
					/* found end of a stack element */
					*next = iptr+1;
					return( 0 );
				}

				/* copy keyword up to '=' */
				start = iptr;
				while( *iptr && *iptr != '=' )
					iptr++;

				bu_vls_strncat( vls, start, iptr - start );

				/* add a single space after keyword */
				bu_vls_putc( vls, ' ' );

				if( !*iptr )
					break;

				/* skip over '=' in input */
				iptr++;

				/* switch to value state */
				state = STATE_IN_VALUE;

				break;
			case STATE_IN_VALUE:
				/* skip excess white space */
				while( isspace( *iptr ) )
					iptr++;

				/* check for quoted value */
				if( *iptr == '"' )
				{
					/* switch to quoted value state */
					state = STATE_IN_QUOTED_VALUE;

					/* skip over '"' */
					iptr++;

					break;
				}

				/* copy value up to next white space or end of string */
				start = iptr;
				while( *iptr && *iptr != ';' && !isspace( *iptr ) )
					iptr++;

				bu_vls_strncat( vls, start, iptr - start );

				if( *iptr ) /* more to come */
				{
					bu_vls_putc( vls, ' ' );

					/* switch back to keyword state */
					state = STATE_IN_KEYWORD;
				}

				break;
			case STATE_IN_QUOTED_VALUE:
				/* copy byte-for-byte to end quote (watch out for escaped quote)
				 * replace quotes with '{' '}' */

				bu_vls_strcat( vls, " {" );
				while( 1 )
				{
					if( *iptr == '"' && *prev != '\\' )
					{
						bu_vls_putc( vls, '}' );
						iptr++;
						break;
					}
					bu_vls_putc( vls, *iptr );
					prev = iptr++;
				}

				if( *iptr && *iptr != ';' ) /* more to come */
					bu_vls_putc( vls, ' ' );

				/* switch back to keyword state */
				state = STATE_IN_KEYWORD;

				break;
		}
	}
	return( 0 );
}

/*
 *			B U _ S H A D E R _ T O _ T C L _ L I S T
 *
 *  Take an old v4 shader specification of the form
 *
 *	shadername arg1=value1 arg2=value2 color=1/2/3
 *
 *  and convert it into the v5 Tcl-list form
 *
 *	shadername {arg1 value1 arg2 value2 color 1/2/3}
 *
 *  Note -- the input string is smashed with nulls.
 *
 *  Note -- the v5 version is used everywhere internally, and in v5
 *  databases.
 *
 *  Returns -
 *	1	error
 *	0	OK
 */
int
bu_shader_to_tcl_list(char *in, struct bu_vls *vls)
{
	char *iptr;
	char *next=in;
	char *shader;
	int shader_name_len=0;
	int is_stack=0;
	int len;


	BU_CK_VLS( vls );

	while( next )
	{
		iptr = next;

		/* skip over white space */
		while( isspace( *iptr ) )
			iptr++;

		/* this is start of shader name */
		shader = iptr;

		/* find end of shader name */
		while( *iptr && !isspace( *iptr ) && *iptr != ';' )
			iptr++;
		shader_name_len = iptr - shader;

		if( !strncmp( shader, "stack", 5 ) )
		{
			/* stack shader, loop through all shaders in stack */
			int done=0;

			bu_vls_strcat( vls, "stack {" );

			while( !done )
			{
				char *shade1;

				while( isspace( *iptr ) )
					iptr++;
				if( *iptr == '\0' )
					break;
				shade1 = iptr;
				while( *iptr && *iptr != ';' )
					iptr++;
				if( *iptr == '\0' )
					done = 1;
				*iptr = '\0';

				bu_vls_putc( vls, '{' );

				if( bu_shader_to_tcl_list( shade1, vls ) )
					return( 1 );

				bu_vls_strcat( vls, "} " );

				if( !done )
					iptr++;
			}
			bu_vls_putc( vls, '}' );
			return( 0 );
		}
		else if( !strncmp( shader, "envmap", 6 ) )
		{
			bu_vls_strcat( vls, "envmap {" );
			if( bu_shader_to_tcl_list( iptr, vls ) )
				return( 1 );
			bu_vls_putc( vls, '}' );
			return( 0 );
		}

		if( is_stack )
			bu_vls_strcat( vls, " {" );

		bu_vls_strncat( vls, shader, shader_name_len );

		/* skip more white space */
		while( *iptr && isspace( *iptr ) )
			iptr++;

		/* iptr now points at start of parameters, if any */
		if( *iptr && *iptr != ';' )
		{
			bu_vls_strcat( vls, " {" );
			len = bu_vls_strlen( vls );
			if( bu_key_eq_to_key_val( iptr, &next, vls ) )
				return( 1 );
			if( bu_vls_strlen( vls ) > len )
				bu_vls_putc( vls, '}' );
			else
				bu_vls_trunc( vls, len-2 );
		}
		else if( *iptr && *iptr == ';' )
			next = ++iptr;
		else
			next = (char *)NULL;

		if( is_stack )
			bu_vls_putc( vls, '}' );
	}

	if( is_stack )
		bu_vls_putc( vls, '}' );

	return( 0 );
}

/*
 *			B U _ L I S T _ E L E M
 *
 *  Given a Tcl list, return a copy of the 'index'th entry,
 *  which may itself be a list.
 *
 *
 *  Note -- the caller is responsible for freeing the returned string.
 */
char *
bu_list_elem( const char *in, int index )
{
	int depth=0;
	int count=0;
	int len=0;
	const char *ptr=in;
	const char *prev=NULL;
	const char *start=NULL;
	const char *end=NULL;
	char *out=NULL;

	while( *ptr )
	{
		/* skip leading white space */
		while( *ptr && isspace( *ptr ) )
		{
			prev = ptr;
			ptr++;
		}

		if( !*ptr )
			break;

		if( depth == 0 && count == index )
			start = ptr;

		if( *ptr == '{' )
		{
			depth++;
			prev = ptr;
			ptr++;
		}
		else if( *ptr == '}' )
		{
			depth--;
			if( depth == 0 )
				count++;
			if( start && depth == 0 )
			{
				end = ptr;
				break;
			}
			prev = ptr;
			ptr++;
		}
		else
		{
			while( *ptr &&
				(!isspace( *ptr ) || *prev == '\\') &&
				(*ptr != '}' || *prev == '\\') &&
				(*ptr != '{' || *prev == '\\') )
			{
				prev = ptr;
				ptr++;
			}
			if( depth == 0 )
				count++;

			if( start && depth == 0 )
			{
				end = ptr-1;
				break;
			}
		}
	}

	if( !start )
		return( (char *)NULL );

	if( *start == '{' )
	{
		if( !end || *end != '}' )
		{
			bu_log( "Error in list (uneven braces?): %s\n", in );
			return( (char *)NULL );
		}

		/* remove enclosing braces */
		start++;
		while( start < end && isspace( *start ) )
			start++;

		end--;
		while( end > start && isspace( *end ) && *(end-1) != '\\' )
			end--;

		if( start == end )
			return( (char *)NULL );
	}

	len = end - start + 1;
	out = bu_malloc( len+1, "bu_list_elem:out" );
	strncpy( out, start, len );
	*(out + len) = '\0';

	return( out );
}

/*
 *			B U _ T C L _ L I S T _ L E N G T H
 *
 *  Return number of items in a string, interpreted as a Tcl list.
 */
int
bu_tcl_list_length( const char *in )
{
	int count=0;
	int depth=0;
	const char *ptr=in;
	const char *prev=NULL;

	while( *ptr )
	{
		/* skip leading white space */
		while( *ptr && isspace( *ptr ) )
		{
			prev = ptr;
			ptr++;
		}

		if( !*ptr )
			break;

		if( *ptr == '{' )
		{
			if( depth == 0 )
				count++;
			depth++;
			prev = ptr;
			ptr++;
		}
		else if( *ptr == '}' )
		{
			depth--;
			prev = ptr;
			ptr++;
		}
		else
		{
			if( depth == 0 )
				count++;

			while( *ptr &&
				(!isspace( *ptr ) || *prev == '\\') &&
				(*ptr != '}' || *prev == '\\') &&
				(*ptr != '{' || *prev == '\\') )
			{
				prev = ptr;
				ptr++;
			}
		}
	}

	return( count );
}

int
bu_key_val_to_vls(struct bu_vls *vls, char *params)
{
	int len;
	int j;

	len = bu_tcl_list_length( params );

	if( len == 1 )
	{
		bu_vls_putc( vls, ' ' );
		bu_vls_strcat( vls, params );
		return( 0 );
	}

	if( len%2 )
	{
		bu_log( "bu_key_val_to_vls: Error: shader parameters must be even numbered!!\n\t%s\n", params );
		return( 1 );
	}

	for( j=0 ; j<len ; j += 2 )
	{
		char *keyword;
		char *value;

		keyword = bu_list_elem( params, j );
		value = bu_list_elem( params, j+1 );

		bu_vls_putc( vls, ' ' );
		bu_vls_strcat( vls, keyword );
		bu_vls_putc( vls, '=' );
		if( bu_tcl_list_length( value ) > 1  )
		{
			bu_vls_putc( vls, '"' );
			bu_vls_strcat( vls, value );
			bu_vls_putc( vls, '"' );
		}
		else
			bu_vls_strcat( vls, value );

		bu_free( keyword, "bu_key_val_to_vls() keyword");
		bu_free( value, "bu_key_val_to_vls() value");

	}
	return( 0 );
}

/*
 *			B U _ S H A D E R _ T O _ K E Y _ E Q
 */
int
bu_shader_to_key_eq(char *in, struct bu_vls *vls)
{
	int len;
	int ret=0;
	char *shader;
	char *params;

	BU_CK_VLS( vls );

	len = bu_tcl_list_length( in );

	if( len == 0 )
		return( 0 );

	if( len == 1 )
	{
		/* shader with no parameters */
		if( bu_vls_strlen( vls ) )
			bu_vls_putc( vls, ' ' );
		bu_vls_strcat( vls, in );
		return( 0 );
	}

	if( len != 2 )
	{
		bu_log( "bu_shader_to_key_eq: Error: shader must have two elements (not %d)!!\n\t%s\n", len, in );
		return 1;
	}

	shader = bu_list_elem( in, 0 );
	params = bu_list_elem( in, 1 );

	if( !strcmp( shader, "envmap" ) )
	{
		/* environment map */

		if( bu_vls_strlen( vls ) )
			bu_vls_putc( vls, ' ' );
		bu_vls_strcat( vls, "envmap" );

		bu_shader_to_key_eq( params, vls );
	}
	else if( !strcmp( shader, "stack" ) )
	{
		/* stacked shaders */

		int i;

		if( bu_vls_strlen( vls ) )
			bu_vls_putc( vls, ' ' );
		bu_vls_strcat( vls, "stack" );

		/* get number of shaders in the stack */
		len = bu_tcl_list_length( params );

		/* process each shader in the stack */
		for( i=0 ; i<len ; i++ )
		{
			char *shader1;

			/* each parameter must be a shader specification in itself */
			shader1 = bu_list_elem( params, i );

			if( i > 0 )
				bu_vls_putc( vls, ';' );
			bu_shader_to_key_eq( shader1, vls );
			bu_free( shader1, "shader1" );
		}
	}
	else
	{
		if( bu_vls_strlen( vls ) )
			bu_vls_putc( vls, ' ' );
		bu_vls_strcat( vls, shader );
		ret = bu_key_val_to_vls( vls, params );
	}

	bu_free( shader, "shader" );
	bu_free( params, "params" );

	return( ret );
}

/*
 *
 *			B U _ F W R I T E _ E X T E R N A L
 *
 *  Take a block of memory, and write it into a file.
 *
 *  Caller is responsible for freeing memory of external representation,
 *  using bu_free_external().
 *
 *  Returns -
 *	<0	error
 *	0	OK
 */
int
bu_fwrite_external( FILE *fp, const struct bu_external *ep )
{
	size_t	got;

	BU_CK_EXTERNAL(ep);

	if( (got = fwrite( ep->ext_buf, 1, ep->ext_nbytes, fp )) != ep->ext_nbytes )  {
		perror("fwrite");
		bu_log("bu_fwrite_external() attempted to write %ld, got %ld\n", (long)ep->ext_nbytes, (long)got );
		return -1;
	}
	return 0;
}

/*
 *			B U _ H E X D U M P _ E X T E R N A L
 */
void
bu_hexdump_external( FILE *fp, const struct bu_external *ep, const char *str)
{
	const unsigned char	*cp;
	const unsigned char	*endp;
	int i, j, k;

	BU_CK_EXTERNAL(ep);

	fprintf(fp, "%s:\n", str);
	if( ep->ext_nbytes <= 0 )  fprintf(fp, "\tWarning: 0 length external buffer\n");

	cp = ep->ext_buf;
	endp = cp + ep->ext_nbytes;
	for( i=0; i < ep->ext_nbytes; i += 16 )  {
		const unsigned char	*sp = cp;

		for( j=0; j < 4; j++ )  {
			for( k=0; k < 4; k++ )  {
				if( cp >= endp )
					fprintf(fp, "   ");
				else
					fprintf(fp, "%2.2x ", *cp++ );
			}
			fprintf(fp, " ");
		}
		fprintf(fp, " |");

		for( j=0; j < 16; j++,sp++ )  {
			if( sp >= endp )  break;
			if( isprint(*sp) )
				putc(*sp, fp);
			else
				putc('.', fp);
		}

		fprintf(fp, "|\n");
	}
}

/*
 *			B U _ F R E E _ E X T E R N A L
 */
void
bu_free_external( register struct bu_external *ep)
{
	BU_CK_EXTERNAL(ep);
	if( ep->ext_buf )  {
		bu_free( ep->ext_buf, "bu_external ext_buf" );
		ep->ext_buf = GENPTR_NULL;
	}
}

/*
 *			B U _ C O P Y _ E X T E R N A L
 */
void
bu_copy_external(struct bu_external *op, const struct bu_external *ip)
{
	BU_CK_EXTERNAL(ip);
	BU_INIT_EXTERNAL(op);

	if( op == ip )  return;

	op->ext_nbytes = ip->ext_nbytes;
	op->ext_buf = bu_malloc( ip->ext_nbytes, "bu_copy_external" );
	bcopy( ip->ext_buf, op->ext_buf, ip->ext_nbytes );
}

/*
 *			B U _ N E X T _ T O K E N
 *
 *  Advance pointer through string over current token,
 *  across white space, to beginning of next token.
 */
char *
bu_next_token( char *str )
{
  char *ret;

  ret = str;
  while( !isspace( *ret ) && *ret !='\0' )
    ret++;
  while( isspace( *ret ) )
    ret++;

  return( ret );
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
