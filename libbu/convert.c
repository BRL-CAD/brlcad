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
/*
 * Theses should be moved to a header file soon.
 */
struct cv_cookie_type {
	int	channels:8;	/* number of input channels */
	int	host:1;		/* host or net */
	int	is_signed:1;	/* is this a signed integer */
	int	type:3;		/* type/size */
	int	conversion:2;	/* conversion style */
};
#define	CV_8	0
#define	CV_16	1
#define	CV_32	2
#define	CV_64	3
#define	CV_D	4
#define CV_NORMAL	0;
#define CV_CLIP		1;
#define CV_LIT		2;

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
struct cv_cookie
cv_cookie(in)
char *in;			/* input format */
{
	char *p;
	int collector;
	struct cv_cookie result = {
		1,0,0,CV_8,CV_CLIP};

	if (!in) return(NULL);
	if (!*in) return(NULL);

	collector = 0;
	for (p=in; *p && isdigit(*p); ++p) collector = collector*10 + (*p - '0');
	if (collector > 255) collector = 255;
	if (!collector) result.channels = collector;

	if (!*p) return(NULL);

	if (*p == 'h') {
		result.host = 1;
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
		if (*p2 && (islower(*p2) || isdigit(*p2)) {
			result.signed = 1;
			++p;
		}
	}

	if (!*p) return(NULL);
	switch (*p) {
	case 'c':
	case '8':
		collector = CV_8;
		break;
	case '1':
		p++;
		if (*p != '6') return(NULL);
		/* fall through */
	case 's':
		collector = CV_16;
		break;
	case '3':
		p++;
		if (*p != '2') return(NULL);
		/* fall through */
	case 'i':
		collector = CV_32;
		break;
	case '6':
		p++;
		if (*p != '4') return(NULL);
		/* fall through */
	case 'l':
		collector = CV_64;
		break;
	case 'd':
		collector = CV_D;
		break;
	default:
		return(NULL);
	}
	p++;
	result.type = collector;

	if (!*p) return(result);
	if (*p == 'N') {
		result.conversion = CV_NORMAL;
	} else if (*p == 'C') {
		result.conversion = CV_CLIP;
	} else if (*p == 'L') {
		result.conversion = CV_LIT;
	} else {
		return(NULL);
	}
	return(result);
}
/* convert - convert from one format to another.
 *
 * Entry:
 *	in	input pointer
 *	out	output pointer
 *	count	number of entries to convert.
 *	size	size of output buffer.
 *
 */
convert(out, size, in, count)
genptr_t out;
int	size;
getptr_t in;
int	count;
{
}
