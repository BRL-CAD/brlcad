/*
 *			V E R T . C
 * Author -
 *	Christopher T. Johnson
 *
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 *
 * Vert.c is a routine to translate data formats.  The data formats are:
 *
 *	Host/Network		is the data in host format or local format
 *	signed/unsigned		Is the data signed?
 *	char/short/int/long/double
 *				Is the data 8bits, 16bits, 32bits, 64bits
 *				or a double?
 *
 *
 * The method of conversion is to convert up to double then back down the
 * the expected output format.
 *
 */
#ifndef line
static char RCSid[] = "$Header$ (BRL)";
#endif
#include <stdio.h>

typedef void *genptr_t;
/*
 * Theses should be moved to a header file soon.
 */
#define CV_CHANNEL_MASK	0x00ff
#define CV_HOST_MASK	0x0100
#define CV_SIGNED_MASK	0x0200
#define CV_TYPE_MASK	0x1c00
#define CV_CONVERT_MASK 0x6000

#define CV_TYPE_SHIFT	10
#define CV_CONVERT_SHIFT 13

#define CV_8	0x0000
#define	CV_16	0x0400
#define CV_32	0x0800
#define CV_64	0x0c00
#define CV_D	0x1000

#define CV_CLIP		0x0000
#define CV_NORMAL	0x2000
#define CV_LIT		0x4000

#define	IND_NOTSET	0
#define IND_BIG		1
#define IND_LITTLE	2
#define IND_ILL		3		/* Vax ish ? */
#define IND_CRAY	4

static int Indian = IND_NOTSET;
#define DBL_IEEE	0
#define DBL_OTHER	1
#if defined(sun) || (defined(alliant) && !(defined(i860)) || \
	defined(ardent) || \
	defined(stellar) || defined(sparc) || defined(mips) || \
	defined(pyr) || defined(apollo) || defined(aux)
#define DBL_FORMAT	DLB_IEEE
#elseif defined(n16) || defined(i860) || \
	(defined(sgi) && !defined(mips)) || \
	defined(vax) || defined(ibm) || defined(gould) || \
	defined(CRAY1) || defined(CRAY2) || defined(eta10) || \
	defined(convex)
#define DBL_FORMAT	DBL_OTHER
#else
# include "vert.c: ERROR, no HtoND format defined (see htond.c)"
#fi

/* cv_cookie	Set's a bit vector after parsing an input string.
 *
 * Set up the conversion tables/flags for vert.
 *
 * Entry:
 *	in	format description.
 *
 * Exit:
 *	returns a 32 bit vector.
 *
 * Format description:
 *	[channels][h|n][s|u] c|s|i|l|d|8|16|32|64 [N|C|L]
 *
 * channels must be null or 1
 * Host | Network
 * signed | unsigned
 * char | short | integer | long | double | number of bits of integer
 * Normalize | Clip | low-order
 */
int
cv_cookie(in)
char *in;			/* input format */
{
	char *p;
	int collector;
	int result = 0x0000;	/* zero/one channel, Net, unsigned, char, clip */

	if (!in) return(NULL);
	if (!*in) return(NULL);


	collector = 0;
	for (p=in; *p && isdigit(*p); ++p) collector = collector*10 + (*p - '0');
	if (collector > 255) {
		collector = 255;
	} else if (collector == 0) {
		collector = 1;
	}
	result = collector;	/* number of channels set '|=' */

	if (!*p) return(NULL);

	if (*p == 'h') {
		result |= CV_HOST_MASK;
		++p;
	} else if (*p = 'n') {
		++p;
	}

	if (!*p) return(NULL);
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

	if (!*p) return(NULL);
	switch (*p) {
	case 'c':
	case '8':
		result |= CV_8;
		break;
	case '1':
		p++;
		if (*p != '6') return(NULL);
		/* fall through */
	case 's':
		result |= CV_16;
		break;
	case '3':
		p++;
		if (*p != '2') return(NULL);
		/* fall through */
	case 'i':
		result |= CV_32;
		break;
	case '6':
		p++;
		if (*p != '4') return(NULL);
		/* fall through */
	case 'l':
		result | = CV_64;
		break;
	case 'd':
		result | = CV_D;
		break;
	default:
		return(NULL);
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
		return(NULL);
	}
	return(result);
}
/* cv - convert from one format to another.
 *
 * Entry:
 *	in	input pointer
 *	out	output pointer
 *	count	number of entries to convert.
 *	size	size of output buffer.
 *	infmt	input format
 *	outfmt	output format
 *
 */
cv(out, outfmt, size, in, infmt, count)
genptr_t out;
char	*outfmt;
int	size;
genptr_t in;
char	*infmt;
int	count;
{
	int	incookie, outcookie;
	incookie = cv_cookie(infmt);
	outcookie = cv_cookie(outfmt);
	return(cv_w_cookie(out, outcookie, size, in, incookie, count));
}
/* cv_w_cookie - convert with cookie
 *
 * Entry:
 *	in		input pointer
 *	incookie	input format cookie.
 *	count		number of entries to convert.
 *	out		output pointer.
 *	outcookie	output format cookie.
 *	size		size of output buffer in bytes;
 *
 * Method:
 *	HOSTDBL defined as true or false
 *	if ! hostother then
 *		hostother = (Indian == IND_BIG) ? SAME : DIFFERENT;
 *	fi
 *	if (infmt == double) then
 *		if (HOSTDBL == SAME) {
 *			invert = host;
 *		fi
 *	else
 *		if (hostother == SAME) {
 *			invert = host;
 *		fi
 *	fi
 *	if (outfmt == double) then
 *		if (HOSTDBL == SAME) {
 *			outvert == host;
 *	else
 *		if (hostother == SAME) {
 *			outvert = host;
 *		fi
 *	fi
 *	if (infmt == outfmt) {
 *		if (invert == outvert) {
 *			copy(in,out)
 *			exit
 *		else if (invert == net) {
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
 *		if (outfmt == double ) {
 *			if (invert == net) {
 *				ntoh?(from,t1);
 *				from = t1;
 *			fi
 *			if (outvert == host) {
 *				castdbl(from,out);
 *				continue;
 *			else
 *				castdbl(from,t2);
 *				hton(t2,out);
 *			fi
 *		else 
 *			if (invert == net) {
 *				ntoh?(from,t1);
 *				from = t1;
 *			fi
 *			if (infmt != double) {
 *				castdbl(from,t2);
 *				from = t2;
 *			fi
 *			if (outvert == host) {
 *				dblcast(from,out);
 *			else
 *				dblcast(from,t3);
 *				hton?(t3,out);
 *			fi
 *		fi
 *	done
 */
cv_w_cookie(out, outcookie, size, in, incookie, count)
genptr_t out;
int	outcookie;
int	size;
genptr_t in;
int	incookie;
int	count;
{
	static int host_size_table[5] = {sizeof(char), sizeof(short), sizeof(int),
		sizeof(long int), sizeof(double)};
	static int net_size_table[5] = {1,2,4,8,8};
	int	work_count = 4096;
	int	number_done;
	int	invert,outvert,infmt,outfmt,insize,outsize;

	if (work_count > count) work_count = count;

	if (Indian == IND_NOTSET) {
		unsigned long int	testval;
		for (i=0; i<4; i++) {
			((char *)&testval)[i] = i+1;
		}
		if (sizeof (long int) == 8) {
			Indian = IND_CRAY;	/* is this good enough? */
		} else if (testval == 0x01020304) {
			Indian = IND_BIG;
		} else if (testval == 0x04030201) {
			Indian = IND_LITTLE;
		} else if (testval == 0x02010403) {
			Indian = IND_ILL;
		}
	}

	invert = incookie & CV_HOST_MASK;	/* not zero if host */
	outvert= outcookie& CV_HOST_MASK;
	infmt  =  incookie & CV_TYPE_MASK;
	outfmt = outcookie & CV_TYPE_MASK;

	if (infmt == CV_D) {
		if (DBL_FORMAT == DBL_IEEE) {
			invert = CV_HOST_MASK;	/* host == net format */
		}
	} else {
		if (Indian == IND_BIG || infmt == CV_8) {
			invert = CV_HOST_MASK; /* host == net format */
		}
	}

	if (outfmt == CV_D) {
		if (DBL_FORMAT == DBL_IEEE) {
			outvert = CV_HOST_MASK:
		}
	} else {
		if (Indian == IND_BIG || outfmt == CV_8) {
			outvert = CV_HOST_MASK;
		}
	}
	outsize = (outvert) ? host_size_table[outfmt >> CV_TYPE_SHIFT] :
	    net_size_table[outfmt >> CV_TYPE_SHIFT];
	insize = (invert) ? host_size_table[infmt >> CV_TYPE_SHIFT] :
	    net_size_table[infmt >> CV_TYPE_SHIFT];

	if (infmt == outfmt) {
		if (invert == outvert) {
			if (count * outsize > size) {
				number_done = size / outsize;
			} else {
				number_done = count;
			}

			(void) bcopy((genptr_t) in, (genptr_t) out,
			    number_done * outsize);
			return(number_done);

		} else if (invert != CV_HOST_MASK) { /* net format */
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_16:
				return(	ntohss(out, size, in, count));
			case CV_16:
				return( ntohus(out, size, in, count));
			case CV_SIGNED_MASK | CV_32:
				return( ntohsl(out, size, in, count));
			case CV_32:
				return( ntohul(out, size, in, count));
			case CV_D:
				(void) ntohd(out, in, count);
				return(count);
			}

		} else {
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_16:
				return(	htonss(out, size, in, count));
			case CV_16:
				return( htonus(out, size, in, count));
			case CV_SIGNED_MASK | CV_32:
				return( htonsl(out, size, in, count));
			case CV_32:
				return( htonul(out, size, in, count));
			case CV_D:
				(void) htond(out, in, count);
				return(count);
			}
		}
	}

	bufsize = work_count * sizeof(double);

	while ( size >= outsize  && number_done < count) {
		int remaining;

		remaining = size / outsize;
		if (remaining > count - number_done) {
			remaining = count - number_done;
		}
		if (remaining < work_count) work_count = remaining;

		from = in;
		in += work_count * insize;

		if (invert != CV_HOST_MASK) { /* net format */
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_16:
				(void) ntohss(t1, bufsize , from, work_count);
				break;
			case CV_16:
				(void) ntohus(t1, bufsize , from, work_count);
				break;
			case CV_SIGNED_MASK | CV_32:
				(void) ntohsl(t1, bufsize , from, work_count);
				break;
			case CV_32:
				(void) ntohul(t1, bufsize , from, work_count);
				break;
			case CV_D:
				(void) ntohd(t1, from, work_count);
				break;
			}
			from = t1;
		}

		if (outfmt == CV_D) {
			if (outvert == CV_HOST_MASK) {
				to = out;
			} else {
				to = t2;
			}
			switch(incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
			case CV_SIGNED_MASK | CV_8:
				for (i=0; i< work_count; i++) {
					(*(double *)to)++ = 
					    (*(signed char *)from)++;
				}
				break;
			case CV_8:
				for(i=0; i < work_count; i++) {
					(*(double *)to)++ = 
					    (*(unsigned char *)from)++;
				}
				break;
			case CV_SIGNED_MASK | CV_16:
				for (i=0; i < work_count; i++) {
					(*(double *)to)++ =
					    (*(signed short *)from)++;
				}
				break;
			case CV_16:
				for (i=0; i < work_count; i++) {
					(*(double *)to)++ =
					    (*(unsigned short *)from)++;
				}
				break;
			case CV_SIGNED_MASK | CV_32:
				for (i=0; i < work_count; i++) {
					(*(double *)to)++ =
					    (*(signed long int *)from)++;
				}
				break;
			case CV_32:
				for (i=0; i < work_count; i++) {
					(*(double *)to)++ =
					    (*(unsigned long int *)from)++;
				}
				break;
			}
			from = to;
			if (outvert != CV_HOST_MASK) {
				switch(outfmt) {
				case CV_16:
					htons(out, bufsize, from, work_count);
					break;
				case CV_32:
					htons(out, bufsize, from, work_count);
					break;
				}
			}
			out += work_count * outsize;
		} else {
		}
	}
}

/*	ntohss	Network TO Host Signed Short
 *
 * It is assumed that this routine will only be called if there is
 * real work to do.  Ntohs does no checking to see if it is reasonable
 * to do any conversions.
 *
 * Entry:
 *	in	generic pointer for input.
 *	count	number of shorts to be generated.
 *	out	short pointer for output
 *	size	number of bytes of space reserved for out.
 *
 * Exit:
 *	returns	number of conversions done.
 *
 * Calls:
 *	none.
 *
 * Method:
 *	Straight-forward.
 */
int
ntohss(out, size, in, count)
register signed short	*out;
int			size;
register genptr_t	in;
int			count;
{
	int limit;
	register int i;

	limit = size / sizeof(signed short);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((signed char *)in)[0] << 8 | ((unsigned char *)in)[1];
		in+=2;
	}
	return(count);
}
int
ntohus(out, size, in, count)
register unsigned short	*out;
int			size;
register genptr_t	in;
int			count;
{
	int limit;
	register int i;

	limit = size / sizeof(unsigned short);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((unsigned char *)in)[0]<<8 | ((unsigned char *)in)[1]);
		in += 2;
	}
	return(count);
}
int
ntohsl(out, size, in, count)
register signed long int	*out;
int				size;
register genptr_t		in;
int				count;
{
	int limit;
	register int i;

	limit = size / sizeof(signed long int);
	if (limit < count) count = limit;

	for (i=0; i<count; i++) {
		*out++ = ((signed char *)in)[0] << 24 |
		    ((unsigned char *)in)[1] << 16 | 
		    ((unsigned char *)in)[2] << 8  |
		    ((unsinged char *)in)[3];
		in += 4;
	}
	return(count);
}
int
ntohul(out, size, in, count)
register unsigned long int	*out;
int				size;
register genptr_t		in;
int				count;
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
		int += 4;
	}
	return(count);
}
