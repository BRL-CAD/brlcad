/*                       C O N V E R T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "bu/cv.h"
#include "bu/endian.h"
#include "bu/malloc.h"
#include "bu/str.h"

int
bu_cv_cookie(const char *in)			/* input format */
{
    const char *p;
    int collector;
    int result = 0x0000;	/* zero/one channel, Net, unsigned, char, clip */
    int val;

    if (UNLIKELY(!in)) return 0;
    if (UNLIKELY(!*in)) return 0;
    if (UNLIKELY(strlen(in) > 4 || strlen(in) < 1)) return 0;

    collector = 0;
    for (p=in, val = *p; val>0 && val<CHAR_MAX && isdigit(val); ++p, val = *p)
	collector = collector*10 + (val - '0');

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
    } else if (*p == 's') {
	/* could be 'signed' or 'short' */
	const char *p2;
	p2 = p+1;
	val = *p2;
	if (*p2 && val>0 && val<CHAR_MAX && (islower(val) || isdigit(val))) {
	    result |= CV_SIGNED_MASK;
	    ++p;
	}
    }

    if (!*p)
	return 0;

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

    if (!*p) return result;
    if (*p == 'N') {
	result |= CV_NORMAL;
    } else if (*p == 'C') {
	result |= CV_CLIP;
    } else if (*p == 'L') {
	result |= CV_LIT;
    } else {
	return 0;
    }
    return result;
}


HIDDEN void
bu_cv_fmt_cookie(char *buf, size_t buflen, int cookie)
{
    register char *cp = buf;
    size_t len;

    if (UNLIKELY(buflen == 0)) {
	fprintf(stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
	return;
    }
    buflen--;
    if (UNLIKELY(cookie == 0)) {
	bu_strlcpy(cp, "bogus!", buflen);
	return;
    }

    snprintf(cp, buflen, "%d", cookie & CV_CHANNEL_MASK);
    len = strlen(cp);
    cp += len;
    if (UNLIKELY(buflen < len))
    {
	fprintf(stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
	return;
    }
    buflen -= len;

    if (UNLIKELY(buflen == 0)) {
	fprintf(stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
	return;
    }
    if (cookie & CV_HOST_MASK) {
	*cp++ = 'h';
	buflen--;
    } else {
	*cp++ = 'n';
	buflen--;
    }

    if (UNLIKELY(buflen == 0)) {
	fprintf(stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
	return;
    }
    if (cookie & CV_SIGNED_MASK) {
	*cp++ = 's';
	buflen--;
    } else {
	*cp++ = 'u';
	buflen--;
    }

    if (UNLIKELY(buflen == 0)) {
	fprintf(stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
	return;
    }
    switch (cookie & CV_TYPE_MASK) {
	case CV_8:
	    *cp++ = '8';
	    buflen--;
	    break;
	case CV_16:
	    bu_strlcpy(cp, "16", buflen);
	    cp += 2;
	    buflen -= 2;
	    break;
	case CV_32:
	    bu_strlcpy(cp, "32", buflen);
	    cp += 2;
	    buflen -= 2;
	    break;
	case CV_64:
	    bu_strlcpy(cp, "64", buflen);
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

    if (UNLIKELY(buflen == 0)) {
	fprintf(stderr, "bu_cv_pr_cookie:  call me with a bigger buffer\n");
	return;
    }
    switch (cookie & CV_CONVERT_MASK) {
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


void
bu_cv_pr_cookie(char *title, int cookie)
{
    char buf[128];

    bu_cv_fmt_cookie(buf, sizeof(buf), cookie);
    fprintf(stderr, "%s cookie '%s' (x%x)\n", title, buf, cookie);
}


size_t
bu_cv(void *out, char *outfmt, size_t size, void *in, char *infmt, size_t count)
{
    int incookie, outcookie;
    incookie = bu_cv_cookie(infmt);
    outcookie = bu_cv_cookie(outfmt);
    return bu_cv_w_cookie(out, outcookie, size, in, incookie, count);
}


int
bu_cv_optimize(register int cookie)
{
    int fmt;

    if (cookie & CV_HOST_MASK)
	return cookie;		/* already in most efficient form */

    /* This is a network format request */
    fmt  =  cookie & CV_TYPE_MASK;

    switch (fmt) {
	case CV_D:
	    cookie |= CV_HOST_MASK;	/* host uses network fmt */
	    return cookie;
	case CV_8:
	    return cookie | CV_HOST_MASK;	/* bytes already host format */
	case CV_16:
	case CV_32:
	case CV_64:
	    /* host is big-endian, so is network */
	    if (bu_byteorder() == BU_BIG_ENDIAN)
		cookie |= CV_HOST_MASK;
	    return cookie;
    }
    return 0;			/* ERROR */
}


size_t
bu_cv_itemlen(register int cookie)
{
    register int fmt = (cookie & CV_TYPE_MASK) >> CV_TYPE_SHIFT;
    static size_t host_size_table[8] = {0, sizeof(char),
				     sizeof(short), sizeof(int),
				     sizeof(long int), sizeof(double)};
    static size_t net_size_table[8] = {0, 1, 2, 4, 8, 8};

    if (cookie & CV_HOST_MASK)
	return host_size_table[fmt];
    return net_size_table[fmt];
}


size_t
bu_cv_ntohss(register short int *out, size_t size, register void *in, size_t count)
{
    size_t limit;
    register size_t i;

    limit = size / sizeof(signed short);
    if (limit < count)
	count = limit;

    for (i=0; i<count; i++) {
	*out++ = ((signed char *)in)[0] << 8 | ((unsigned char *)in)[1];
	/* XXX This needs sign extension here for the case of
	 * XXX a negative 2-byte input on a 4 or 8 byte machine.
	 * XXX The "signed char" trick isn't enough.
	 * XXX Use your Cyber trick w/magic numbers or something.
	 */
	in = ((char *)in) + 2;
    }
    return count;
}


size_t
bu_cv_ntohus(register short unsigned int *out, size_t size, register void *in, size_t count)
{
    size_t limit;
    register size_t i;

    limit = size / sizeof(unsigned short);
    if (limit < count)
	count = limit;

    for (i=0; i<count; i++) {
	*out++ = ((unsigned char *)in)[0]<<8 |
	    ((unsigned char *)in)[1];
	in = ((char *)in) + 2;
    }
    return count;
}


size_t
bu_cv_ntohsl(register long int *out, size_t size, register void *in, size_t count)
{
    size_t limit;
    register size_t i;

    limit = size / sizeof(signed long int);
    if (limit < count)
	count = limit;

    for (i=0; i<count; i++) {
	*out++ = ((signed char *)in)[0] << 24 |
	    ((unsigned char *)in)[1] << 16 |
	    ((unsigned char *)in)[2] << 8  |
	    ((unsigned char *)in)[3];
	/* XXX Sign extension here */
	in = ((char *)in) + 4;
    }

    return count;
}


size_t
bu_cv_ntohul(register long unsigned int *out, size_t size, register void *in, size_t count)
{
    size_t limit;
    register size_t i;

    limit = size / sizeof(unsigned long int);
    if (limit < count)
	count = limit;

    for (i=0; i<count; i++) {
	*out++ =
	    (unsigned long)((unsigned char *)in)[0] << 24 |
	    (unsigned long)((unsigned char *)in)[1] << 16 |
	    (unsigned long)((unsigned char *)in)[2] <<  8 |
	    (unsigned long)((unsigned char *)in)[3];
	in = ((char *)in) + 4;
    }
    return count;
}


size_t
bu_cv_htonss(void *out, size_t size, register short int *in, size_t count)
{
    size_t limit;
    register size_t i;
    register unsigned char *cp = (unsigned char *)out;
    register int val;

    limit = size / 2;
    if (count > limit)  count = limit;

    for (i=0; i<count; i++) {
	*cp++ = (val = *in++)>>8;
	*cp++ = val;
    }
    return count;
}


size_t
bu_cv_htonus(void *out, size_t size, register short unsigned int *in, size_t count)
{
    size_t limit;
    register size_t i;
    register unsigned char *cp = (unsigned char *)out;
    register int val;

    limit = size / 2;
    if (count > limit)
	count = limit;

    for (i=0; i<count; i++) {
	*cp++ = (val = *in++)>>8;
	*cp++ = val;
    }
    return count;
}


size_t
bu_cv_htonsl(void *out, size_t size, register long int *in, size_t count)
{
    size_t limit;
    register size_t i;
    register unsigned char *cp = (unsigned char *)out;
    register long val;

    limit = size / 4;
    if (count > limit)
	count = limit;

    for (i=0; i<count; i++) {
	*cp++ = (val = *in++)>>24;
	*cp++ = val>>16;
	*cp++ = val>> 8;
	*cp++ = val;
    }
    return count;
}


size_t
bu_cv_htonul(void *out, size_t size, register long unsigned int *in, size_t count)
{
    size_t limit;
    register size_t i;
    register unsigned char *cp = (unsigned char *)out;
    register long val;

    limit = size / 4;
    if (count > limit)
	count = limit;

    for (i=0; i<count; i++) {
	*cp++ = (val = *in++)>>24;
	*cp++ = val>>16;
	*cp++ = val>> 8;
	*cp++ = val;
    }
    return count;
}


size_t
bu_cv_w_cookie(void *out, int outcookie, size_t size, void *in, int incookie, size_t count)
{
    size_t work_count = 4096;
    size_t number_done = 0;
    int inIsHost, outIsHost, infmt, outfmt;
    size_t insize, outsize;
    size_t bufsize;
    void *t1;
    void *t2;
    void *t3;
    void *from;
    void *to;
    void *hold;
    register size_t i;

    /*
     * Work_count is the size of the working buffer.  If count is
     * smaller than the default work_count (4096) use the smaller
     * number.
     */

    if (work_count > count)
	work_count = count;

    incookie = bu_cv_optimize(incookie);
    outcookie = bu_cv_optimize(outcookie);

    /*
     * break out the conversion code and the format code.  Conversion
     * is net<-->host.  Format is 8/16/32/64/D casting.
     */
    inIsHost = incookie & CV_HOST_MASK;	/* not zero if host */
    outIsHost= outcookie& CV_HOST_MASK;
    infmt  =  incookie & CV_TYPE_MASK;
    outfmt = outcookie & CV_TYPE_MASK;
    /*
     * Find the number of bytes required for one item of each kind.
     */
    outsize = bu_cv_itemlen(outcookie);
    insize = bu_cv_itemlen(incookie);

    /*
     * If the input format is the same as the output format then the
     * most that has to be done is a host to net or net to host
     * conversion.
     */
    if (infmt == outfmt) {

	/*
	 * Input format is the same as output format, do we need to do
	 * a host/net conversion?
	 */
	if (inIsHost == outIsHost) {

	    /*
	     * No conversion required.  Check the amount of space
	     * remaining before doing the memmove.
	     */
	    if ((unsigned int)count * outsize > size) {
		number_done = (int)(size / outsize);
	    } else {
		number_done = count;
	    }

	    /*
	     * This is the simplest case, binary copy and out.
	     */
	    memmove((void *)out, (void *)in, (size_t)number_done * outsize);
	    return number_done;

	    /*
	     * Well it's still the same format but the conversions are
	     * different.  Only one of the *vert variables can be HOST
	     * therefore if inIsHost != HOST then outIsHost must be
	     * host format.
	     */

	} else if (inIsHost != CV_HOST_MASK) {
	    /* net format */
	    switch (incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
		case CV_SIGNED_MASK | CV_16:
		    return bu_cv_ntohss((signed short *)out, size, in, count);
		case CV_16:
		    return bu_cv_ntohus((unsigned short *)out, size, in, count);
		case CV_SIGNED_MASK | CV_32:
		    return bu_cv_ntohsl((signed long *)out, size, in, count);
		case CV_32:
		    return bu_cv_ntohul((unsigned long *)out, size, in, count);
		case CV_D:
		    (void) bu_cv_ntohd((unsigned char *)out, (unsigned char *)in, count);
		    return count;
	    }

	    /*
	     * Since inIsHost != outIsHost and inIsHost == HOST then
	     * outIsHost must be in net format.  call the correct
	     * subroutine to do the conversion.
	     */
	} else {
	    switch (incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
		case CV_SIGNED_MASK | CV_16:
		    return bu_cv_htonss(out, size, (short *)in, count);
		case CV_16:
		    return bu_cv_htonus(out, size, (unsigned short *)in, count);
		case CV_SIGNED_MASK | CV_32:
		    return bu_cv_htonsl(out, size, (long *)in, count);
		case CV_32:
		    return bu_cv_htonul(out, size, (unsigned long *)in, count);
		case CV_D:
		    (void) bu_cv_htond((unsigned char *)out, (unsigned char *)in, count);
		    return count;
	    }
	}
    }
    /*
     * If we get to this point then the input format is known to be of
     * a different type than the output format.  This will require a
     * cast to, from or to and from double.
     *
     * Because the number of steps is not known initially, we get
     * three working buffers.  The size of a double is the largest of
     * any of the sizes we may be dealing with.
     */

    bufsize = work_count * sizeof(double);
    t1 = (void *) bu_malloc(bufsize, "convert.c: t1");
    t2 = (void *) bu_malloc(bufsize, "convert.c: t2");
    t3 = (void *) bu_malloc(bufsize, "convert.c: t3");

    /*
     * From here on we will be working on a chunk of process at a time.
     */
    while (size >= (unsigned int)outsize  && number_done < count) {
	size_t remaining;

	/*
	 * Size is the number of bytes that the caller said was
	 * available.  We need the check to make sure that we will not
	 * convert too many entries, overflowing the output buffer.
	 */

	/*
	 * Get number of full entries that can be converted
	 */
	remaining = (int)(size / outsize);

	/*
	 * If number of entries that would fit in the output buffer is
	 * larger than the number of entries left to convert(based on
	 * count and number done), set remaining to request count
	 * minus the number of conversions already completed.
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
	 * All input at any stage will come from the "from" pointer.
	 * We start with the from pointer pointing to the input
	 * buffer.
	 */
	from = in;

	/*
	 * We will be processing work_count entries of insize bytes
	 * each, so we set the in pointer to be ready for the next
	 * time through the loop.
	 */
	in = ((char *) in) + work_count * insize;

	/*
	 * If the input is in net format convert it to host format.
	 * Because we know that the input format is not equal to the
	 * output this means that there will be at least two
	 * conversions taking place if the input is in net format.
	 * (from net to host then at least one cast)
	 */
	if (inIsHost != CV_HOST_MASK) {
	    /* net format */
	    switch (incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
		case CV_SIGNED_MASK | CV_16:
		    (void) bu_cv_ntohss((short *)t1, bufsize, from, work_count);
		    break;
		case CV_16:
		    (void) bu_cv_ntohus((unsigned short *)t1, bufsize, from, work_count);
		    break;
		case CV_SIGNED_MASK | CV_32:
		    (void) bu_cv_ntohsl((long *)t1, bufsize, from, work_count);
		    break;
		case CV_32:
		    (void) bu_cv_ntohul((unsigned long *)t1, bufsize, from, work_count);
		    break;
		case CV_D:
		    (void) bu_cv_ntohd((unsigned char *)t1, (unsigned char *)from, work_count);
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
	 * If the input format is not double then there must be a cast
	 * to double.
	 */
	if (infmt != CV_D) {

	    /*
	     * if the output conversion is HOST and output format is
	     * DOUBLE then this will be the last step.
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
	    switch (incookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
		case CV_SIGNED_MASK | CV_8:
		    for (i=0; i< work_count; i++) {
			*((double *)to) = *((signed char *)from);
			to = (void *)(((double *)to) + 1);
			from = ((char *)from) + 1;
		    }
		    break;
		case CV_8:
		    for (i=0; i < work_count; i++) {
			*((double *)to) = *((unsigned char *)from);
			to = (void *)(((double *)to) + 1);
			from = (void *)(((unsigned char *)from) + 1);
		    }
		    break;
		case CV_SIGNED_MASK | CV_16:
		    for (i=0; i < work_count; i++) {
			*((double *)to) = *((signed short *)from);
			to = (void *)(((double *)to) + 1);
			from = (void *)(((signed short *)from) + 1);
		    }
		    break;
		case CV_16:
		    for (i=0; i < work_count; i++) {
			*((double *)to) = *((unsigned short *)from);
			to = (void *)(((double *)to) + 1);
			from = (void *)(((unsigned short *)from) + 1);
		    }
		    break;
		case CV_SIGNED_MASK | CV_32:
		    for (i=0; i < work_count; i++) {
			*((double *)to) = *((signed long int *)from);
			to = (void *)(((double *)to) + 1);
			from =  (void *)(((signed long int *)from) + 1);
		    }
		    break;
		case CV_32:
		    for (i=0; i < work_count; i++) {
			*((double *)to) = *((unsigned long int *) from);
			to = (void *)(((double *)to) + 1);
			from = (void *)(((unsigned long int *)from) + 1);
		    }
		    break;
		default:
		    fprintf(stderr, "Unimplemented input format\n");
		    break;
	    }
	    from = hold;
	}

	if (outfmt != CV_D) {
	    /*
	     * The input point is now pointing to a double in host
	     * format.  If the output is also in host format then the
	     * next conversion will be the last conversion, set the
	     * destination to reflect this.
	     */

	    if (outIsHost == CV_HOST_MASK) {
		to = out;
	    } else {
		to = t3;
	    }

	    /*
	     * The output format is something other than DOUBLE (tested
	     * for earlier), do a cast from double to requested
	     * format.
	     */
	    hold = to;

	    switch (outcookie & (CV_SIGNED_MASK | CV_TYPE_MASK)) {
		case CV_SIGNED_MASK | CV_8:
		    for (i=0; i<work_count; i++) {
			*((signed char *)to) = *((double *)from);
			to = (void *)(((signed char *)to) + 1);
			from = (void *)(((double *)from) + 1);
		    }
		    break;
		case CV_8:
		    for (i=0; i<work_count; i++) {
			*((unsigned char *)to) =
			    (unsigned char)(*((double *)from));
			to = (void *)(((unsigned char *)to) + 1);
			from = (void *)(((double *)from) + 1);
		    }
		    break;
		case CV_SIGNED_MASK | CV_16:
		    for (i=0; i<work_count; i++) {
			*((signed short int *)to) =
			    *((double *)from);
			to = (void *)(((signed short int *)to) + 1);
			from = (void *)(((double *)from) + 1);
		    }
		    break;
		case CV_16:
		    for (i=0; i<work_count; i++) {
			*((unsigned short int *)to) =
			    *((double *)from);
			to = (void *)(((unsigned short int *)to) + 1);
			from = (void *)(((double *)from) + 1);
		    }
		    break;
		case CV_SIGNED_MASK | CV_32:
		    for (i=0; i<work_count; i++) {
			*((signed long int *)to) =
			    *((double *)from);
			to = (void *)(((signed long int *)to) + 1);
			from = (void *)(((double *)from) + 1);
		    }
		    break;
		case CV_32:
		    for (i=0; i<work_count; i++) {
			*((unsigned long int *)to) =
			    *((double *)from);
			to = (void *)(((unsigned long int *)to) + 1);
			from = (void *)(((double *)from) + 1);
		    }
		    break;
		default:
		    fprintf(stderr, "Unimplemented output format\n");
		    break;

	    }
	    from = hold;
	    /*
	     * The input is now pointing to a host formatted buffer of
	     * the requested output format.
	     */

	    /*
	     * If the output conversion is network then do a host to
	     * net call for either 16 or 32 bit values using Host TO
	     * Network All Short | Long
	     */
	    if (outIsHost != CV_HOST_MASK) {
		switch (outfmt) {
		    case CV_16 | CV_SIGNED_MASK:
			(void) bu_cv_htonss(out, bufsize, (short int *)from, work_count);
			break;
		    case CV_16:
			(void) bu_cv_htonus(out, bufsize, (unsigned short int *)from, work_count);
			break;
		    case CV_32 | CV_SIGNED_MASK:
			(void) bu_cv_htonsl(out, bufsize, (long int *)from, work_count);
			break;
		    case CV_32:
			(void) bu_cv_htonul(out, bufsize, (unsigned long int *)from, work_count);
			break;
		    case CV_D:
		    default:
			/* do nothing */
			break;
		}
	    }

	}
	/*
	 * move the output pointer.  reduce the amount of space
	 * remaining in the output buffer.  Increment the count of
	 * values converted.
	 */
	out = ((char *)out) + work_count * outsize;
	size -= work_count * outsize;
	number_done += work_count;
    }
    /*
     * All Done!  Clean up and leave.
     */
    bu_free(t1, "convert.c: t1");
    bu_free(t2, "convert.c: t2");
    bu_free(t3, "convert.c: t3");
    return number_done;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
