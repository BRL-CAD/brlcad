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
		if (*p2 && (islower(*p2) || isdigit(*p2)) {
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
getptr_t in;
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
 */
cv_w_cookie(out, outcookie, size, in, incookie, count)
genptr_t out;
int	outcookie;
int	size;
genptr_t in;
int	incookie;
int	count;
{
	int number_converted = 0;
	int host_size_table[5] = {sizeof(char), sizeof(short), sizeof(int),
		sizeof(long int), sizeof(double)};
	int net_size_table[5] = {1,2,4,8,8};
	int bytes_per;
	double *working;
	int	work_count = 500;

	working = (double *) malloc(work_count*sizeof(double));

	bytes_per = (outcookie & CV_HOST_MASK) ?
	    host_size_table[ (outcookie & CV_TYPE_MASK) >> CV_TYPE_SHIFT] :
	    net_size_table[ (outcookie & CV_TYPE_MASK) >> CV_TYPE_SHIFT] ;

/*
 * Currently we assume that all machines are big-indian - XXX
 */
	while (size>= bytes_per && number_converted < count) {
		remaining = size / bytes_per;
		if (remaining > count-number_converted) {
			remaining = count - number_converted;
		}
		if (remaining < work_count) work_count = remaining;
/*
 * net to host
 */

/*
 * to double.
 */

/*
 * convert.  (Normalize, clip, literal)
 */

/*
 * from double.
 */

/*
 * to host.
 */
	}
}
