/*                       C O N V E R T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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

/** \addtogroup conv */
/*@{*/
/** @file convert.c
 * 
 * @brief
 * Routines to translate data formats.  The data formats are:
 *
 * \li Host/Network		is the data in host format or local format
 * \li  signed/unsigned		Is the data signed?
 * \li char/short/int/long/double
 *				Is the data 8bits, 16bits, 32bits, 64bits
 *				or a double?
 *
 * The method of conversion is to convert up to double then back down the
 * the expected output format.
 *
 * @author Christopher T. Johnson
 *
 *  @par Source
 *	The U. S. Army Research Laboratory			@n
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char libbu_convert_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "bu.h"


/* bu_cv_cookie	
 *
 * @brief
 * Set's a bit vector after parsing an input string.
 *
 * Set up the conversion tables/flags for vert.
 * 
 * @param in	format description.
 * 
 * @return a 32 bit vector.
 *
 * Format description:
 *	[channels][h|n][s|u] c|s|i|l|d|8|16|32|64 [N|C|L]
 *
 * @n channels must be null or 1
 * @n Host | Network
 * @n signed | unsigned
 * @n char | short | integer | long | double | number of bits of integer
 * @n Normalize | Clip | low-order
 */
int
bu_cv_cookie(char *in)			/* input format */
{
	char *p;
	int collector;
	int result = 0x0000;	/* zero/one channel, Net, unsigned, char, clip */

	if (!in) return 0;
	if (!*in) return 0;


	collector = 0;
	for (p=in; *p && isdigit(*p); ++p) collector = collector*10 + (*p - '0');
	if (collector > 255) {
		collector = 255;
	} else if (collector == 0) {
		collector = 1;
	}
	result = collector;	/* number of channels set '|=' */

	if (!*p) return 0;

	if (*p == 'h') {
		result |= CV_HOST_MASK;
		++p;
	} else if (*p == 'n') {
		++p;
	}

	if (!*p) return 0;
	if (*p == 'u') {
		++p;
	} else if (*p == 's') {	/* could be 'signed' or 'short' */
		char *p2;
		p2 = p+1;
		if (*p2 && (islower(*p2) || isdigit(*p2))) {
			result |= CV_SIGNED_MASK;
			++p;
		}
	}

	if (!*p) return 0;
	switch (*p) {
	case 'c':
	case '8':
		result |= CV_8;
		break;
	case '1':
		p++;
		if (*p != '6') return 0;
		/* fall through */
	case 's':
		result |= CV_16;
		break;
	case '3':
		p++;
		if (*p != '2') return 0;
		/* fall through */
	case 'i':
		result |= CV_32;
		break;
	case '6':
		p++;
		if (*p != '4') return 0;
		/* fall through */
	case 'l':
		result |= CV_64;
		break;
	case 'd':
		result |= CV_D;
		break;
	default:
		return 0;
	}
	p++;

	if (!*p) return(result);
	if (*p == 'N') {
		result |= CV_NORMAL;
	} else if (*p == 'C') {
		result |= CV_CLIP;
	} else if (*p == 'L') {
		result |= CV_LIT;
	} else {
		return 0;
	}
	return(result);
}

/**
 *
 */
void
bu_cv_fmt_cookie( char * buf, size_t buflen, int cookie )
{
	register char *cp = buf;
	unsigned int	len;

	if( buflen == 0 )	{
		fprintf( stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
		return;
	}
	buflen--;
	if( cookie == 0 )  {
		strncpy( cp, "bogus!", buflen );
		return;
	}

	sprintf( cp, "%d", cookie & CV_CHANNEL_MASK );
	len = strlen(cp);
	cp += len;
	if( buflen < len )
	{
		fprintf( stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
		return;
	}
	buflen -= len;

	if( buflen == 0 )	{
		fprintf( stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
		return;
	}
	if( cookie & CV_HOST_MASK )  {
		*cp++ = 'h';
		buflen--;
	} else {
		*cp++ = 'n';
		buflen--;
	}

	if( buflen == 0 )	{
		fprintf( stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
		return;
	}
	if( cookie & CV_SIGNED_MASK )  {
		*cp++ = 's';
		buflen--;
	} else {
		*cp++ = 'u';
		buflen--;
	}

	if( buflen == 0 )	{
		fprintf( stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
		return;
	}
	switch( cookie & CV_TYPE_MASK )  {
	case CV_8:
		*cp++ = '8';
		buflen--;
		break;
	case CV_16:
		strncpy( cp, "16", buflen );
		cp += 2;
		buflen -= 2;
		break;
	case CV_32:
		strncpy( cp, "32", buflen );
		cp += 2;
		buflen -= 2;
		break;
	case CV_64:
		strncpy( cp, "64", buflen );
		cp += 2;
		buflen -= 2;
		break;
	case CV_D:
		*cp++ = 'd';
		buflen -= 1;
		break;
	default:
		*cp++ = '?';
		buflen -= 1;
		break;
	}

	if( buflen == 0 )	{
		fprintf( stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
		return;
	}
	switch( cookie & CV_CONVERT_MASK )  {
	case CV_CLIP:
		*cp++ = 'C';
		buflen -= 1;
		break;
	case CV_NORMAL:
		*cp++ = 'N';
		buflen -= 1;
		break;
	case CV_LIT:
		*cp++ = 'L';
		buflen -= 1;
		break;
	default:
		*cp++ = 'X';
		buflen -= 1;
		break;
	}
	*cp = '\0';
}

/**
 *
 *
 */
void
bu_cv_pr_cookie( char *title, int cookie )
{
	char	buf[128];

	bu_cv_fmt_cookie( buf, sizeof(buf), cookie );
	fprintf( stderr, "%s cookie '%s' (x%x)\n", title, buf, cookie );
}

/**		c v
 * @brief
 * convert from one format to another.
 *
 * 
 * @param in	input pointer
 * @param out	output pointer
 * @param count	number of entries to convert.
 * @param size	size of output buffer.
 * @param infmt	input format
 * @param outfmt	output format
 *
 */
int
cv(genptr_t out, char *outfmt, size_t size, genptr_t in, char *infmt, int count)
{
	int	incookie, outcookie;
	incookie = bu_cv_cookie(infmt);
	outcookie = bu_cv_cookie(outfmt);
	return(bu_cv_w_cookie(out, outcookie, size, in, incookie, count));
}

/**
 *			C V _ O P T I M I Z E
 *
 *  It is always more efficient to handle host data, rather than network.
 *  If host and network formats are the same, and the request was for
 *  network format, modify the cookie to request host format.
 */
int
bu_cv_optimize(register int cookie)
{
	static int Indian = IND_NOTSET;
	int	fmt;

	if( cookie & CV_HOST_MASK )
		return cookie;		/* already in most efficient form */

	/* This is a network format request */
	fmt  =  cookie & CV_TYPE_MASK;

	/* Run time check:  which kind of integers does this machine have? */
	if (Indian == IND_NOTSET) {
		size_t soli = sizeof(long int);
		unsigned long int	testval = 0;
		register int		i;
		for (i=0; i<4; i++) {
			((char *)&testval)[i] = i+1;
		}

		if (soli == 8) {
			Indian = IND_CRAY;	/* is this good enough? */
			if ( ( (testval >> 31) >> 1 ) == 0x01020304) {
				Indian = IND_BIG; /* XXX 64bit */
			} else if (testval == 0x04030201) {
				Indian = IND_LITTLE;	/* 64 bit */
			} else {
				bu_bomb("bu_cv_optimize: can not tell indian of host.\n");
			}
		} else if (testval == 0x01020304) {
			Indian = IND_BIG;
		} else if (testval == 0x04030201) {
			Indian = IND_LITTLE;
		} else if (testval == 0x02010403) {
			Indian = IND_ILL;
		}
	}

	switch(fmt)  {
	case CV_D:
#		if IEEE_FLOAT
			cookie |= CV_HOST_MASK;	/* host uses network fmt */
#		endif
		return cookie;
	case CV_8:
		return cookie | CV_HOST_MASK;	/* bytes already host format */
	case CV_16:
	case CV_32:
	case CV_64:
		/* host is big-endian, so is network */
		if( Indian == IND_BIG )
			cookie |= CV_HOST_MASK;
		return cookie;
	}
	return 0;			/* ERROR */
}

/**
 *			C V _ I T E M L E N
 *
 *  Returns the number of bytes each "item" of type "cookie" occupies.
 */
int
bu_cv_itemlen(register int cookie)
{
	register int	fmt = (cookie & CV_TYPE_MASK) >> CV_TYPE_SHIFT;
	static int host_size_table[8] = {0, sizeof(char),
																	 sizeof(short), sizeof(int),
																	 sizeof(long int), sizeof(double)};
	static int net_size_table[8] = {0,1,2,4,8,8};

	if( cookie & CV_HOST_MASK )
		return host_size_table[fmt];
	return net_size_table[fmt];
}


/**	bu_cv_ntohss
 *
 * @brief
 * Network TO Host Signed Short
 *
 * It is assumed that this routine will only be called if there is
 * real work to do.  Ntohs does no checking to see if it is reasonable
 * to do any conversions.
 *
 *
 * @param in	generic pointer for input.
 * @param count	number of shorts to be generated.
 * @param out	short pointer for output
 * @param size	number of bytes of space reserved for out.
 *
 *
 * @return	number of conversions done.
 */
int
bu_cv_ntohss(register short int *out, size_t size, register genptr_t in, int count)
{
	int limit;
	register int i;

	limit = size / sizeof(signed short);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((signed char *)in)[0] << 8 | ((unsigned char *)in)[1];
		/* XXX This needs sign extension here for the case of
		 * XXX a negative 2-byte input on a 4 or 8 byte machine.
		 * XXX The "signed char" trick isn't enough.
		 * XXX Use your Cyber trick w/magic numbers or something.
		 */
		in = ((char *)in) + 2;
	}
	return(count);
}

/**
 *
 *
 */
int
bu_cv_ntohus(register short unsigned int *out, size_t size, register genptr_t in, int count)
{
	int limit;
	register int i;

	limit = size / sizeof(unsigned short);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((unsigned char *)in)[0]<<8 |
			((unsigned char *)in)[1];
		in = ((char *)in) + 2;
	}
	return(count);
}
/**
 *
 *
 */
int
bu_cv_ntohsl(register long int *out, size_t size, register genptr_t in, int count)
{
	int limit;
	register int i;

	limit = size / sizeof(signed long int);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((signed char *)in)[0] << 24 |
		    ((unsigned char *)in)[1] << 16 |
		    ((unsigned char *)in)[2] << 8  |
		    ((unsigned char *)in)[3];
		/* XXX Sign extension here */
		in = ((char *)in) + 4;
	}

	return(count);
}
/**
 *
 *
 */
int
bu_cv_ntohul(register long unsigned int *out, size_t size, register genptr_t in, int count)
{
	int limit;
	register int i;

	limit = size / sizeof(unsigned long int);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((unsigned char *)in)[0] << 24 |
			((unsigned char *)in)[1] << 16 |
		    ((unsigned char *)in)[2] <<  8 |
			((unsigned char *)in)[3];
		in = ((char *)in) + 4;
	}
	return(count);
}

/*****/
/**
 *
 *
 */
int
bu_cv_htonss(genptr_t out, size_t size, register short int *in, int count)
{
	int		limit;
	register int	i;
	register unsigned char *cp = (unsigned char *)out;
	register int	val;

	limit = size / 2;
	if( count > limit )  count = limit;

	for (i=0; i<count; i++) {
		*cp++ = (val = *in++)>>8;
		*cp++ = val;
	}
	return(count);
}
/**
 *
 *
 */
int
bu_cv_htonus(genptr_t out, size_t size, register short unsigned int *in, int count)
{
	int		limit;
	register int	i;
	register unsigned char *cp = (unsigned char *)out;
	register int	val;

	limit = size / 2;
	if( count > limit )  count = limit;

	for (i=0; i<count; i++) {
		*cp++ = (val = *in++)>>8;
		*cp++ = val;
	}
	return(count);
}
/**
 *
 *
 */
int
bu_cv_htonsl(genptr_t out, size_t size, register long int *in, int count)
{
	int		limit;
	register int	i;
	register unsigned char *cp = (unsigned char *)out;
	register long	val;

	limit = size / 4;
	if( count > limit )  count = limit;

	for (i=0; i<count; i++) {
		*cp++ = (val = *in++)>>24;
		*cp++ = val>>16;
		*cp++ = val>> 8;
		*cp++ = val;
	}
	return(count);
}
/**
 *
 *
 */
int
bu_cv_htonul(genptr_t out, size_t size, register long unsigned int *in, int count)
{
	int		limit;
	register int	i;
	register unsigned char *cp = (unsigned char *)out;
	register long	val;

	limit = size / 4;
	if( count > limit ) {
		count = limit;
	}

	for (i=0; i<count; i++) {
		*cp++ = (val = *in++)>>24;
		*cp++ = val>>16;
		*cp++ = val>> 8;
		*cp++ = val;
	}
	return(count);
}


/** bu_cv_w_cookie
 *
 * @brief
 * convert with cookie
 *
 * @param in		input pointer
 * @param incookie	input format cookie.
 * @param count		number of entries to convert.
 * @param out		output pointer.
 * @param outcookie	output format cookie.
 * @param size		size of output buffer in bytes;
 *
 *
 * A worst case would be:	ns16 on vax to ns32
@code
 *	ns16 	-> hs16
 *		-> hd
 *		-> hs32
 *		-> ns32
@endcode 
 * The worst case is probably the easiest to deal with because all steps are
 * done.  The more difficult cases are when only a subset of steps need to
 * be done.
 *
 * @par Method:
@code
 *	HOSTDBL defined as true or false
 *	if ! hostother then
 *		hostother = (Indian == IND_BIG) ? SAME : DIFFERENT;
 *	fi
 *	if (infmt == double) then
 *		if (HOSTDBL == SAME) {
 *			inIsHost = host;
 *		fi
 *	else
 *		if (hostother == SAME) {
 *			inIsHost = host;
 *		fi
 *	fi
 *	if (outfmt == double) then
 *		if (HOSTDBL == SAME) {
 *			outIsHost == host;
 *	else
 *		if (hostother == SAME) {
 *			outIsHost = host;
 *		fi
 *	fi
 *	if (infmt == outfmt) {
 *		if (inIsHost == outIsHost) {
 *			copy(in,out)
 *			exit
 *		else if (inIsHost == net) {
 *			ntoh?(in,out);
 *			exit
 *		else
 *			hton?(in,out);
 *			exit
 *		fi
 *	fi
 *
 *	while not done {
 *		from = in;
 *
 *		if (inIsHost == net) {
 *			ntoh?(from,t1);
 *			from = t1;
 *		fi
 *		if (infmt != double ) {
 *			if (outIsHost == host) {
 *				to = out;
 *			else
 *				to = t2;
 *			fi
 *			castdbl(from,to);
 *			from = to;
 *		fi
 *
 *		if (outfmt == double ) {
 *			if (outIsHost == net) {
 *				hton?(from,out);
 *			fi
 *		else
 *			if (outIsHost == host) {
 *				dblcast(from,out);
 *			else
 *				dblcast(from,t3);
 *				hton?(t3,out);
 *			fi
 *		fi
 *	done
@endcode
 */
int
bu_cv_w_cookie(genptr_t out, int outcookie, size_t size, genptr_t in,  int incookie,  int	count)
{
	int	work_count = 4096;
	int	number_done = 0;
	int	inIsHost,outIsHost,infmt,outfmt,insize,outsize;
	size_t	bufsize;
	genptr_t	t1,t2,t3;
	genptr_t	from;
	genptr_t	to;
	genptr_t	hold;
	register int i;

	/*
	 * Work_count is the size of the working buffer.  If count is smaller
	 * than the default work_count (4096) use the smaller number.
	 */

	if (work_count > count)
		work_count = count;

	incookie = bu_cv_optimize( incookie );
	outcookie = bu_cv_optimize( outcookie );

	/*
	 * break out the conversion code and the format code.
	 * Conversion is net<-->host.
	 * Format is 8/16/32/64/D casting.
	 */
	inIsHost = incookie & CV_HOST_MASK;	/* not zero if host */
	outIsHost= outcookie& CV_HOST_MASK;
	infmt  =  incookie & CV_TYPE_MASK;
	outfmt = outcookie & CV_TYPE_MASK;
	/*
	 * Find the number of bytes required for one item of each kind.
	 */
	outsize = bu_cv_itemlen( outcookie );
	insize = bu_cv_itemlen( incookie );

	/*
	 * If the input format is the same as the output format then the
	 * most that has to be done is a host to net or net to host conversion.
	 */
	if (infmt == outfmt) {

		/*
		 * Input format is the same as output format, do we need to do a
		 * host/net conversion?
		 */
		if (inIsHost == outIsHost) {

			/*
			 * No conversion required.
			 * Check the amount of space remaining before doing the bcopy.
			 */
			if ((unsigned int)count * outsize > size) {
		    number_done = size / outsize;
			} else {
		    number_done = count;
			}

			/*
			 * This is the simplest case, binary copy and out.
			 */
			(void) bcopy((genptr_t) in, (genptr_t) out, (size_t)number_done * outsize);
			return(number_done);

			/*
			 * Well it's still the same format but the conversion are different.
			 * Only one of the *vert variables can be HOST therefore if
			 * inIsHost != HOST then outIsHost must be host format.
			 */

		} else if (inIsHost != CV_HOST_MASK) { /* net format */
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_16:
		    return(	bu_cv_ntohss((signed short *)out, size, in, count));
			case CV_16:
		    return( bu_cv_ntohus((unsigned short *)out, size, in, count));
			case CV_SIGNED_MASK | CV_32:
		    return( bu_cv_ntohsl((signed long *)out, size, in, count));
			case CV_32:
		    return( bu_cv_ntohul((unsigned long *)out, size, in, count));
			case CV_D:
		    (void) ntohd((unsigned char *)out, (unsigned char *)in, count);
		    return(count);
			}

			/*
			 * Since inIsHost != outIsHost and inIsHost == HOST then outIsHost must
			 * be in net format.  call the correct subroutine to do the conversion.
			 */
		} else {
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_16:
		    return(	bu_cv_htonss(out, size, (short *)in, count));
			case CV_16:
		    return( bu_cv_htonus(out, size, (unsigned short *)in, count));
			case CV_SIGNED_MASK | CV_32:
		    return( bu_cv_htonsl(out, size, (long *)in, count));
			case CV_32:
		    return( bu_cv_htonul(out, size, (unsigned long *)in, count));
			case CV_D:
		    (void) htond((unsigned char *)out, (unsigned char *)in, count);
		    return(count);
			}
		}
	}
	/*
	 * If we get to this point then the input format is known to be
	 * of a diffrent type than the output format.  This will require
	 * a cast to, from or to and from double.
	 *
	 * Because the number of steps is not known initially, we get
	 * three working buffers.  The size of a double is the largest of
	 * any of the sizes we may be dealing with.
	 */

	bufsize = work_count * sizeof(double);
	t1 = (genptr_t) bu_malloc(bufsize, "vert.c: t1");
	t2 = (genptr_t) bu_malloc(bufsize, "vert.c: t2");
	t3 = (genptr_t) bu_malloc(bufsize, "vert.c: t3");

	/*
	 * From here on we will be working on a chunk of process at a time.
	 */
	while ( size >= (unsigned int)outsize  && number_done < count) {
		int remaining;

		/*
		 * Size is the number of bytes that the caller said was available.
		 * We need the check to make sure that we will not convert too many
		 * entries, overflowing the output buffer.
		 */

		/*
		 * Get number of full entries that can be converted
		 */
		remaining = size / outsize;

		/*
		 * If number of entries that would fit in the output buffer is
		 * larger than the number of entries left to convert(based on
		 * count and number done), set remaining to request count minus
		 * the number of conversions already completed.
		 */
		if (remaining > count - number_done) {
			remaining = count - number_done;
		}
		/*
		 * If we are in the last chunk, set the work count to take up
		 * the slack.
		 */
		if (remaining < work_count) work_count = remaining;

		/*
		 * All input at any stage will come from the "from" pointer.  We
		 * start with the from pointer pointing to the input buffer.
		 */
		from = in;

		/*
		 * We will be processing work_count entries of insize bytes each, so
		 * we set the in pointer to be ready for the next time through the loop.
		 */
		in = ((char *) in) + work_count * insize;

		/*
		 * If the input is in net format convert it host format.
		 * Because we know that the input format is not equal to the output
		 * this means that there will be at least two conversions taking place
		 * if the input is in net format.  (from net to host then at least one cast)
		 */
		if (inIsHost != CV_HOST_MASK) { /* net format */
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_16:
				(void) bu_cv_ntohss((short *)t1, bufsize , from, work_count);
				break;
			case CV_16:
				(void) bu_cv_ntohus((unsigned short *)t1, bufsize , from, work_count);
				break;
			case CV_SIGNED_MASK | CV_32:
				(void) bu_cv_ntohsl((long *)t1, bufsize , from, work_count);
				break;
			case CV_32:
				(void) bu_cv_ntohul((unsigned long *)t1, bufsize , from, work_count);
				break;
			case CV_D:
				(void) ntohd((unsigned char *)t1, (unsigned char *)from, work_count);
				break;
			}
			/*
			 * Point the "from" pointer to the host format.
			 */
			from = t1;
		}


		/*
		 * "From" is a pointer to a HOST format buffer.
		 */

		/*
		 * If the input format is not double then there must be a cast to
		 * double.
		 */
		if (infmt != CV_D) {

			/*
			 * if the output conversion is HOST and output format is DOUBLE
			 * then this will be the last step.
			 */
			if (outIsHost == CV_HOST_MASK && outfmt == CV_D) {
		    to = out;
			} else {
		    to = t2;
			}

			hold = to;
			/*
			 * Cast the input format to double.
			 */
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_8:
		    for (i=0; i< work_count; i++) {
					*((double *)to) = *((signed char *)from);
					to = (genptr_t)(((double *)to) + 1);
					from = ((char *)from) + 1;
		    }
		    break;
			case CV_8:
		    for(i=0; i < work_count; i++) {
					*((double *)to) = *((unsigned char *)from);
					to = (genptr_t)(((double *)to) + 1);
					from = (genptr_t)(((unsigned char *)from) + 1);
		    }
		    break;
			case CV_SIGNED_MASK | CV_16:
		    for (i=0; i < work_count; i++) {
					*((double *)to) = *((signed short *)from);
					to = (genptr_t)(((double *)to) + 1);
					from = (genptr_t)(((signed short *)from) + 1);
		    }
		    break;
			case CV_16:
		    for (i=0; i < work_count; i++) {
					*((double *)to) = *((unsigned short *)from);
					to = (genptr_t)(((double *)to) + 1);
					from = (genptr_t)(((unsigned short *)from) + 1);
		    }
		    break;
			case CV_SIGNED_MASK | CV_32:
		    for (i=0; i < work_count; i++) {
					*((double *)to) = *((signed long int *)from);
					to = (genptr_t)(((double *)to) + 1);
					from =  (genptr_t)(((signed long int *)from) + 1);
		    }
		    break;
			case CV_32:
		    for (i=0; i < work_count; i++) {
					*((double *)to) = *((unsigned long int *) from);
					to = (genptr_t)(((double *)to) + 1);
					from = (genptr_t)(((unsigned long int *)from) + 1);
		    }
		    break;
			default:
				fprintf( stderr, "Unimplemented input format\n");
				break;
			}
			from = hold;
		}

		if (outfmt != CV_D) {
			/*
			 * The input point is now pointing to a double in host format.  If the
			 * output is also in host format then the next conversion will be
			 * the last conversion, set the destination to reflect this.
			 */

			if (outIsHost == CV_HOST_MASK) {
		    to = out;
			} else {
		    to = t3;
			}

			/*
			 * The ouput format is something other than DOUBLE (tested for earlier),
			 * do a cast from double to requested format.
			 */
			hold = to;

			switch (outcookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_8:
		    for (i=0; i<work_count; i++) {
					*((signed char *)to) = *((double *)from);
					to = (genptr_t)(((signed char *)to) + 1);
					from = (genptr_t)(((double *)from) + 1);
		    }
		    break;
			case CV_8:
		    for (i=0; i<work_count; i++) {
					*((unsigned char *)to) =
						(unsigned char)(*((double *)from));
					to = (genptr_t)(((unsigned char *)to) + 1);
					from = (genptr_t)(((double *)from) + 1);
		    }
		    break;
			case CV_SIGNED_MASK | CV_16:
		    for (i=0; i<work_count; i++) {
					*((signed short int *)to) =
						*((double *)from);
					to = (genptr_t)(((signed short int *)to) + 1);
					from = (genptr_t)(((double *)from) + 1);
		    }
		    break;
			case CV_16:
		    for (i=0; i<work_count; i++) {
					*((unsigned short int *)to) =
						*((double *)from);
					to = (genptr_t)(((unsigned short int *)to) + 1);
					from = (genptr_t)(((double *)from) + 1);
		    }
		    break;
			case CV_SIGNED_MASK | CV_32:
		    for (i=0; i<work_count; i++) {
					*((signed long int *)to) =
						*((double *)from);
					to = (genptr_t)(((signed long int *)to) + 1);
					from = (genptr_t)(((double *)from) + 1);
		    }
		    break;
			case CV_32:
		    for (i=0; i<work_count; i++) {
					*((unsigned long int *)to) =
						*((double *)from);
					to = (genptr_t)(((unsigned long int *)to) + 1);
					from = (genptr_t)(((double *)from) + 1);
		    }
		    break;
			default:
				fprintf( stderr, "Unimplemented output format\n");
				break;

			}
			from = hold;
			/*
			 * The input is now pointing to a host formated buffer of the requested
			 * output format.
			 */

			/*
			 * If the output conversion is network then do a host to net call
			 * for either 16 or 32 bit values using Host TO Network All Short | Long
			 */
			if (outIsHost != CV_HOST_MASK) {
				switch (outfmt) {
				case CV_D:
					(void) htond((unsigned char *)out,
											 (unsigned char *)from,
											 work_count);
					break;
				case CV_16 | CV_SIGNED_MASK:
					(void) bu_cv_htonss(out, bufsize, from,
															work_count);
					break;
				case CV_16:
					(void) bu_cv_htonus(out, bufsize, from,
															work_count);
					break;
				case CV_32 | CV_SIGNED_MASK:
					(void) bu_cv_htonsl(out, bufsize, from,
															work_count);
					break;
				case CV_32:
					(void) bu_cv_htonul(out, bufsize, from,
															work_count);
					break;
				}
			}

		}
		/*
		 * move the output pointer.
		 * reduce the amount of space remaining in the output buffer.
		 * Increment the count of values converted.
		 */
		out = ((char *)out) + work_count * outsize;
		size -= work_count * outsize;
		number_done += work_count;
	}
	/*
	 * All Done!  Clean up and leave.
	 */
	bu_free(t1, "vert.c: t1");
	bu_free(t2, "vert.c: t2");
	bu_free(t3, "vert.c: t3");
	return(number_done);
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
