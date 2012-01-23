/*                        S S C A N F . C
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright (c) 1990, 1993 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file sscanf.c
 *
 * Custom sscanf and vsscanf.
 *
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bu.h"

#define	BUF 513  /* Maximum length of numeric string. */

/*
 * Flags used during conversion.
 */
#define	LONG		0x0001	/* l: long or double */
#define	LONGDBL		0x0002	/* L: long double */
#define	SHORT		0x0004	/* h: short */
#define	SUPPRESS	0x0008	/* *: suppress assignment */
#define	POINTER		0x0010	/* p: void * (as hex) */
#define	NOSKIP		0x0020	/* [ or c: do not skip blanks */
#define	LONGLONG	0x0400	/* ll: long long (+ deprecated q: quad) */
#define	INTMAXT		0x0800	/* j: intmax_t */
#define	PTRDIFFT	0x1000	/* t: ptrdiff_t */
#define	SIZET		0x2000	/* z: size_t */
#define	SHORTSHORT	0x4000	/* hh: char */
#define	UNSIGNED	0x8000	/* %[oupxX] conversions */

/*
 * The following are used in integral conversions only:
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS
 */
#define	SIGNOK		0x00040	/* +/- is (still) legal */
#define	NDIGITS		0x00080	/* no digits detected */
#define	PFXOK		0x00100	/* 0x prefix is (still) legal */
#define	NZDIGITS	0x00200	/* no zero digits detected */
#define	HAVESIGN	0x10000	/* sign detected */

/*
 * Conversion types.
 */
#define	CT_CHAR		0	/* %c conversion */
#define	CT_CCL		1	/* %[...] conversion */
#define	CT_STRING	2	/* %s conversion */
#define	CT_INT		3	/* %[dioupxX] conversion */
#define	CT_FLOAT	4	/* %[efgEFG] conversion */

/* Copy part of a string, everything from srcStart to srcEnd (exclusive),
 * into a new buffer. Returns the allocated buffer.
 */
static char*
getSubstring(const char *srcStart, const char *srcEnd)
{
    size_t size = (size_t)srcEnd - (size_t)srcStart + 1;
    char *sub = (char*)bu_malloc(size * sizeof(char), "getSubstring");
    bu_strlcpy(sub, srcStart, size);
    return sub;
}

/* append %n to format string to get consumed count */
static void
append_n(char **fmt) {
    char *result;
    int nLen, size, resultSize;

    size = strlen(*fmt) + 1;
    nLen = strlen("%n");

    resultSize = size + nLen;
    result = (char*)bu_malloc(resultSize * sizeof(char), "append_n");

    bu_strlcpy(result, *fmt, resultSize);
    bu_strlcat(result, "%n", resultSize);

    bu_free(*fmt, "append_n");
    *fmt = result;
}

/* The basic strategy of this routine is to break the formatted scan into
 * pieces, one for each word of the format string.
 *
 * New two-word format strings are created by appending "%n" (request to sscanf
 * for consumed char count) to each word of the provided format string. sscanf
 * is called with each two-word format string, followed by an appropriately
 * cast pointer from the vararg list, followed by the local consumed count
 * pointer.
 *
 * Each time sscanf successfully returns, the total assignment count and the
 * total consumed character count are updated. The consumed character count is
 * used to offset the read position of the source string on successive calls to
 * sscanf. The total assignment count is the ultimate return value of the
 * routine.
 */
int
bu_vsscanf(const char *src, const char *fmt, va_list ap)
{
    int c, base, flags;
    int numCharsConsumed, partConsumed;
    int numFieldsAssigned, partAssigned;
    char *partFmt;
    const char *wordStart;
    size_t width;

    BU_ASSERT(src != NULL);
    BU_ASSERT(fmt != NULL);

#define UPDATE_COUNTS \
numCharsConsumed += partConsumed; \
numFieldsAssigned += partAssigned;

    numFieldsAssigned = 0;
    numCharsConsumed = 0;
    base = 0;

#define FREE_FORMAT_PART \
if (partFmt != NULL) { \
    bu_free(partFmt, "bu_sscanf partFmt"); \
}

#define GET_FORMAT_PART \
FREE_FORMAT_PART; \
partFmt = getSubstring(wordStart, fmt); \
append_n(&partFmt);

    partFmt = NULL;
    partConsumed = 0;
    partAssigned = 0;

    while (1) {
	/* Mark word start, then skip to first non-white char. */
	wordStart = fmt;
	do {
	    c = *fmt;
	    if (c == '\0') {
		goto exit;
	    }
	    ++fmt;
	} while (isspace(c));

	if (c != '%') {
	    /* Must have found literal sequence. Find where it ends. */
	    while (1) {
		c = *fmt;
		if (c == '\0') {
		    goto exit;
		}
		if (isspace(c) || c == '%') {
		    /* found start of next word */
		    break;
		}
		++fmt;
	    }
	    /* scan literal sequence */
	    GET_FORMAT_PART;
	    partAssigned = sscanf(&src[numCharsConsumed], partFmt, &partConsumed);
	    if (partAssigned < 1) {
		goto exit;
	    }
	    UPDATE_COUNTS;
	    continue;
	}

	/* Found conversion specification. Parse it. */

	width = 0;
	flags = 0;
again:
	c = *fmt++;
	switch (c) {

	/* Literal '%'. */
	case '%':
	    GET_FORMAT_PART;
	    partAssigned = sscanf(&src[numCharsConsumed], partFmt, &partConsumed);
	    if (partAssigned < 1) {
		goto exit;
	    }
	    UPDATE_COUNTS;
	    continue;
	

	/* MODIFIER */
	case '*':
	    flags |= SUPPRESS;
	    goto again;
	case 'j':
	    flags |= INTMAXT;
	    goto again;
	case 'l':
	    if (!(flags & LONG)) {
		/* First occurance of 'l' in this conversion specifier. */
		flags |= LONG;
	    } else {
		/* Since LONG is set, the previous conversion character must
		 * have been 'l'. With this second 'l', we know we have an "ll"
		 * modifer, not an 'l' modifer. We need to replace the
		 * incorrect flag with the correct one.
		 */
		flags &= ~LONG;
		flags |= LONGLONG;
	    }
	    goto again;
	case 'q':
	    /* Quad conversion specific to 4.4BSD and Linux libc5 only.
	     * Should probably print a warning here to use ll or L instead.
	     */
	    flags |= LONGLONG;
	    goto again;
	case 't':
	    flags |= PTRDIFFT;
	    goto again;
	case 'z':
	    flags |= SIZET;
	    goto again;
	case 'L':
	    flags |= LONGDBL;
	    goto again;
	case 'h':
	    if (!(flags & SHORT)) {
		/* First occurance of 'h' in this conversion specifier. */
		flags |= SHORT;
	    } else {
		/* Since SHORT is set, the previous conversion character must
		 * have been 'h'. With this second 'h', we know we have an "hh"
		 * modifer, not an 'h' modifer. We need to replace the
		 * incorrect flag with the correct one.
		 */
		flags &= ~SHORT;
		flags |= SHORTSHORT;
	    }
	    goto again;


	/* MAXIMUM FIELD WIDTH */
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
#define NUMERIC_CHAR_TO_INT(c) (c - '0')
	    width = (width * 10) + NUMERIC_CHAR_TO_INT(c);
	    goto again;


	/* CONVERSION */
	case 'd':
	    c = CT_INT;
	    base = 10;
	    break;
	case 'i':
	    c = CT_INT;
	    base = 0;
	    break;
	case 'o':
	    c = CT_INT;
	    flags |= UNSIGNED;
	    base = 8;
	    break;
	case 'u':
	    c = CT_INT;
	    flags |= UNSIGNED;
	    base = 10;
	    break;
	case 'p':
	case 'x':
	case 'X':
	    if (c == 'p') {
		flags |= POINTER;
	    }
	    flags |= PFXOK;
	    flags |= UNSIGNED;
	    c = CT_INT; 
	    base = 16;
	    break;
	case 'A': case 'E': case 'F': case 'G':
	case 'a': case 'e': case 'f': case 'g':
	    c = CT_FLOAT;
	    break;
	case 'S':
	    /* XXX This may be a BSD extension. */
	    flags |= LONG;
	    /* FALLTHROUGH */
	case 's':
	    c = CT_STRING;
	    break;
	case '[':
	    flags |= NOSKIP;
	    c = CT_CCL;
	    break;
	case 'C':
	    /* XXX This may be a BSD extension. */
	    flags |= LONG;
	    /* FALLTHROUGH */
	case 'c':
	    flags |= NOSKIP;
	    c = CT_CHAR;
	    break;
	case 'n':
	    if (flags & SUPPRESS) {
		/* This is legal, but doesn't really make sense. Caller
		 * requested assignment of the current count of consumed
		 * characters, but then suppressed the assignment they
		 * requested!
	         */
		continue;
	    }

	    /* Store current count of consumed characters to whatever kind of
	     * int pointer was provided.
	     */
	    if (flags & SHORTSHORT) {
		*va_arg(ap, char *) = numCharsConsumed;
	    } else if (flags & SHORT) {
		*va_arg(ap, short *) = numCharsConsumed;
	    } else if (flags & LONG) {
		*va_arg(ap, long *) = numCharsConsumed;
	    } else if (flags & LONGLONG) {
		*va_arg(ap, long long *) = numCharsConsumed;
	    } else if (flags & INTMAXT) {
		*va_arg(ap, intmax_t *) = numCharsConsumed;
	    } else if (flags & SIZET) {
		*va_arg(ap, size_t *) = numCharsConsumed;
	    } else if (flags & PTRDIFFT) {
		*va_arg(ap, ptrdiff_t *) = numCharsConsumed;
	    } else {
		*va_arg(ap, int *) = numCharsConsumed;
	    }
	    continue;

	case '\0':
	    /* Format string ends with bare '%'. Returning EOF regardless of
	     * successfull assignments is a backwards compatability behavior.
	     */
	    FREE_FORMAT_PART;
	    return EOF;

	default:
	    /* match failure */
	    goto exit;
	}

	/* Done parsing conversion specification.
	 * Now do the actual conversion.
	 */

	GET_FORMAT_PART;

#define SSCANF_TYPE(type) \
partAssigned = sscanf(&src[numCharsConsumed], partFmt, va_arg(ap, type), \
		      &partConsumed);

#define SSCANF_SIGNED_UNSIGNED(type) \
if (flags & UNSIGNED) { \
    SSCANF_TYPE(unsigned type); \
} else { \
    SSCANF_TYPE(type); \
}
	switch (c) {

	/* %c conversion */
	/* %[...] conversion */
	/* %s conversion */
	case CT_CHAR:
	case CT_CCL:
	case CT_STRING:
	    if (flags & LONG) {
		SSCANF_TYPE(wchar_t*);
	    } else {
		SSCANF_TYPE(char*);
	    }
	    break;

	/* %[dioupxX] conversion */
	case CT_INT:
	    if (flags & SHORT) {
		SSCANF_SIGNED_UNSIGNED(short int*);
	    } else if (flags & SHORTSHORT) {
		SSCANF_SIGNED_UNSIGNED(char*);
	    } else if (flags & LONG) {
		SSCANF_SIGNED_UNSIGNED(long int*);
	    } else if (flags & LONGLONG) {
		SSCANF_SIGNED_UNSIGNED(long long int*);
	    } else if (flags & PTRDIFFT) {
		SSCANF_TYPE(ptrdiff_t*);
	    } else if (flags & SIZET) {
		SSCANF_TYPE(size_t*);
	    } else if (flags & INTMAXT) {
		if (flags & UNSIGNED) {
		    SSCANF_TYPE(uintmax_t*);
		} else {
		    SSCANF_TYPE(intmax_t*);
		}
	    } else {
		SSCANF_TYPE(int*);
	    }
	    break;

	/* %[eEfg] conversion */
	case CT_FLOAT:
	    if (flags & LONG) {
		SSCANF_TYPE(double*);
	    } else if (flags & LONGDBL) {
		SSCANF_TYPE(long double*);
	    } else {
		SSCANF_TYPE(float*);
	    }
	}

	/* check for read error or bad source string */
	if (partAssigned == EOF) {
	    FREE_FORMAT_PART;
	    return EOF;
	}

        /* check that assignment was successful */
	if (!(flags & SUPPRESS) && partAssigned < 1) {
	    goto exit;
	}

	/* Conversion successful, on to the next one! */
	UPDATE_COUNTS;

    } /* while (1) */

exit:
    FREE_FORMAT_PART;
    return numFieldsAssigned;
} /* bu_vsscanf */

int
bu_sscanf(const char *src, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = bu_vsscanf(src, fmt, ap);
    va_end(ap);

    return ret;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
