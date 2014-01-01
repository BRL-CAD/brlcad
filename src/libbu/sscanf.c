/*                        S S C A N F . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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

#include "common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"


/*
 * Flags used during conversion.
 */
#define LONG		0x00001	/* l: long or double */
#define LONGDBL		0x00002	/* L: long double */
#define SHORT		0x00004	/* h: short */
#define SUPPRESS	0x00008	/* *: suppress assignment */
#define POINTER		0x00010	/* p: void * (as hex) */
#define NOSKIP		0x00020	/* [ or c: do not skip blanks */
#define LONGLONG	0x00400	/* ll: long long (+ deprecated q: quad) */
#define INTMAXT		0x00800	/* j: intmax_t */
#define PTRDIFFT	0x01000	/* t: ptrdiff_t */
#define SIZET		0x02000	/* z: size_t */
#define SHORTSHORT	0x04000	/* hh: char */
#define UNSIGNED	0x08000	/* %[oupxX] conversions */
#define ALTERNATE	0x40000	/* # flag for alternate behavior */

/*
 * The following are used in integral conversions only:
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS
 */
#define SIGNOK		0x00040	/* +/- is (still) legal */
#define NDIGITS		0x00080	/* no digits detected */
#define PFXOK		0x00100	/* 0x prefix is (still) legal */
#define NZDIGITS	0x00200	/* no zero digits detected */
#define HAVESIGN	0x10000	/* sign detected */
#define HAVEWIDTH	0x20000

/*
 * Conversion types.
 */
#define CT_CHAR		0	/* %c conversion */
#define CT_CCL		1	/* %[...] conversion */
#define CT_STRING	2	/* %s conversion */
#define CT_INT		3	/* %[dioupxX] conversion */
#define CT_FLOAT	4	/* %[efgEFG] conversion */
#define CT_VLS		5	/* %V and %#V conversion */

/* The basic strategy of this routine is to break the formatted scan into
 * pieces, one for each conversion in the format string.
 *
 * New two-conversion format strings are created by appending "%n" (request to
 * sscanf for consumed char count) to each conversion of the provided format
 * string. sscanf is called with each two-conversion format string, followed by
 * an appropriately cast pointer from the vararg list, followed by the local
 * consumed count pointer.
 *
 * Each time sscanf successfully returns, the total assignment count and the
 * total consumed character count are updated. The consumed character count is
 * used to offset the read position of the source string on successive calls to
 * sscanf. The total assignment count is the ultimate return value of the
 * routine.
 */
int
bu_vsscanf(const char *src, const char *fmt0, va_list ap)
{
    int c;
    long flags;
    size_t i, width;
    int numCharsConsumed, partConsumed;
    int numFieldsAssigned, partAssigned;
    struct bu_vls partFmt = BU_VLS_INIT_ZERO;
    const char *fmt;

    BU_ASSERT(src != NULL);
    BU_ASSERT(fmt0 != NULL);

    fmt = fmt0;

    numFieldsAssigned = 0;
    numCharsConsumed = 0;
    partConsumed = 0;
    partAssigned = 0;

#define UPDATE_COUNTS \
    numCharsConsumed += partConsumed; \
    numFieldsAssigned += partAssigned;

#define FREE_FORMAT_PART \
    bu_vls_free(&partFmt);

#define GET_FORMAT_PART \
    bu_vls_strcat(&partFmt, "%n");

#define EXIT_DUE_TO_INPUT_FAILURE \
    FREE_FORMAT_PART; \
    if (numFieldsAssigned == 0) { \
	return EOF; \
    } \
    return numFieldsAssigned;

#define EXIT_DUE_TO_MATCH_FAILURE \
    FREE_FORMAT_PART; \
    return numFieldsAssigned;

#define EXIT_DUE_TO_MISC_ERROR \
    FREE_FORMAT_PART; \
    return EOF;

    while (1) {
	/* skip to first non-white char */
	bu_vls_trunc(&partFmt, 0);
	do {
	    c = *fmt;
	    if (c == '\0') {
		/* Found EOI before next word; implies fmt contains trailing
		 * whitespace or is empty. No worries; exit normally.
		 */
		FREE_FORMAT_PART;
		return numFieldsAssigned;
	    }
	    bu_vls_putc(&partFmt, c);
	    ++fmt;
	} while (isspace(c));

	if (c != '%') {
	    /* Must have found literal sequence. Find where it ends. */
	    while (1) {
		c = *fmt;
		if (c == '\0' || isspace(c) || c == '%') {
		    break;
		}
		bu_vls_putc(&partFmt, c);
		++fmt;
	    }

	    /* scan literal sequence */
	    GET_FORMAT_PART;
	    partAssigned = sscanf(&src[numCharsConsumed],
				  bu_vls_addr(&partFmt), &partConsumed);

	    if (partAssigned < 0) {
		EXIT_DUE_TO_INPUT_FAILURE;
	    }
	    UPDATE_COUNTS;
	    continue;
	}

	/* Found conversion specification. Parse it. */

	width = 0;
	flags = 0;
    again:
	c = *fmt++;
	bu_vls_putc(&partFmt, c);
	switch (c) {

	    /* Literal '%'. */
	    case '%':
		GET_FORMAT_PART;
		partAssigned = sscanf(&src[numCharsConsumed],
				      bu_vls_addr(&partFmt), &partConsumed);

		if (partAssigned < 0) {
		    EXIT_DUE_TO_INPUT_FAILURE;
		}
		UPDATE_COUNTS;
		continue;


		/* MODIFIER */
	    case '*':
		flags |= SUPPRESS;
		goto again;
	    case '#':
		flags |= ALTERNATE;
		goto again;
	    case 'j':
		flags |= INTMAXT;
		goto again;
	    case 'l':
		if (!(flags & LONG)) {
		    /* First occurrence of 'l' in this conversion specifier. */
		    flags |= LONG;
		} else {
		    /* Since LONG is set, the previous conversion character must
		     * have been 'l'. With this second 'l', we know we have an "ll"
		     * modifier, not an 'l' modifier. We need to replace the
		     * incorrect flag with the correct one.
		     */
		    flags &= ~LONG;
		    flags |= LONGLONG;
		}
		goto again;
	    case 't':
#ifndef HAVE_C99_FORMAT_SPECIFIERS
		/* remove C99 't' */
		bu_vls_trunc(&partFmt, bu_vls_strlen(&partFmt) - 1);

		/* Assume MSVC.
		 *
		 * For 32-bit, ptrdiff_t is __int32, and equivalent of %t[dioxX] is
		 * %[dioxX].
		 *
		 * For 64-bit, ptrdiff_t is __int64, and equivalent of %t[dioxX] is
		 * %I64[dioxX].
		 */
#if defined(SIZEOF_SIZE_T) && SIZEOF_SIZE_T == 8
		bu_vls_strcat(&partFmt, "I64");
#endif
#endif
		flags |= PTRDIFFT;
		goto again;
	    case 'z':
#ifndef HAVE_C99_FORMAT_SPECIFIERS
		/* remove C99 'z' */
		bu_vls_trunc(&partFmt, bu_vls_strlen(&partFmt) - 1);

		/* Assume MSVC.
		 *
		 * For 32-bit, size_t is unsigned __int32, and equivalent of
		 * %z[dioxX] is %[dioxX].
		 *
		 * For 64-bit, size_t is unsigned __int64, and equivalent of
		 * %z[dioxX] is %I64[dioxX].
		 */
#if defined(SIZEOF_SIZE_T) && SIZEOF_SIZE_T == 8
		bu_vls_strcat(&partFmt, "I64");
#endif
#endif
		flags |= SIZET;
		goto again;
	    case 'L':
		flags |= LONGDBL;
		goto again;
	    case 'h':
		if (!(flags & SHORT)) {
		    /* First occurrence of 'h' in this conversion specifier. */
		    flags |= SHORT;
		} else {
#ifndef HAVE_C99_FORMAT_SPECIFIERS
		    /* Assume MSVC, where there is no equivalent of %hh[diouxX].
		     * Will use %h[diouxX] with short instead, then cast into
		     * char argument.
		     */
		    bu_vls_trunc(&partFmt, bu_vls_strlen(&partFmt) - 1);
#endif
		    /* Since SHORT is set, the previous conversion character must
		     * have been 'h'. With this second 'h', we know we have an "hh"
		     * modifier, not an 'h' modifier. We need to replace the
		     * incorrect flag with the correct one.
		     */
		    flags &= ~SHORT;
		    flags |= SHORTSHORT;
		}
		goto again;


		/* MAXIMUM FIELD WIDTH */
#define NUMERIC_CHAR_TO_INT(c) (c - '0')
	    case '0':
		/* distinguish default width from width set to 0 */
		flags |= HAVEWIDTH;
		/* FALLTHROUGH */
	    case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		width = (width * 10) + NUMERIC_CHAR_TO_INT(c);
		goto again;


		/* CONVERSION */
	    case 'd':
		c = CT_INT;
		break;
	    case 'i':
		c = CT_INT;
		break;
	    case 'o':
		c = CT_INT;
		flags |= UNSIGNED;
		break;
	    case 'u':
		c = CT_INT;
		flags |= UNSIGNED;
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
		break;
	    case 'A': case 'E': case 'F': case 'G':
	    case 'a': case 'e': case 'f': case 'g':
		/* e/f/g, E/F/G, and a/A (C99) are all synonyms for float
		 * conversion. Support for a/A is limited to C99-compliant
		 * implementations, and there is varying support for E/F/G.
		 * Replace all with the most portable 'f' variant.
		 */
		bu_vls_trunc(&partFmt, bu_vls_strlen(&partFmt) - 1);
		bu_vls_putc(&partFmt, 'f');
		c = CT_FLOAT;
		break;
	    case 's':
		c = CT_STRING;
		break;
	    case '[':
		/* note that at this point c == '[' == fmt[-1] and so fmt[0] is
		 * either '^' or the first character of the class
		 */

		/* there should be at least one character in between brackets */
		if (fmt[0] == '\0' || fmt[1] == '\0') {
		    EXIT_DUE_TO_MISC_ERROR;
		}

		/* skip literal ']' ("[]" or "[^]") */
		if (fmt[0] == ']') {
		    fmt = &fmt[1];
		} else if (fmt[0] == '^' && fmt[1] == ']') {
		    fmt = &fmt[2];
		}

		/* point fmt after character class */
		while (1) {
		    c = *fmt++;
		    bu_vls_putc(&partFmt, c);

		    if (c == '\0') {
			EXIT_DUE_TO_MISC_ERROR;
		    }
		    if (c == ']') {
			/* found end of character class */
			break;
		    }
		}

		flags |= NOSKIP;
		c = CT_CCL;
		break;
	    case 'c':
		flags |= NOSKIP;
		c = CT_CHAR;
		break;
	    case 'V':
		c = CT_VLS;
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
		 * successful assignments is a backwards compatibility behavior.
		 */
		EXIT_DUE_TO_MISC_ERROR;

	    default:
		EXIT_DUE_TO_MATCH_FAILURE;
	}

	/* Done parsing conversion specification.
	 * Now do the actual conversion.
	 */

	GET_FORMAT_PART;

#define SSCANF_TYPE(type) \
	if (flags & SUPPRESS) { \
	    partAssigned = sscanf(&src[numCharsConsumed], bu_vls_addr(&partFmt), \
				  &partConsumed); \
	} else { \
	    partAssigned = sscanf(&src[numCharsConsumed], bu_vls_addr(&partFmt), \
				  va_arg(ap, type), &partConsumed); \
	}

#define SSCANF_SIGNED_UNSIGNED(type) \
	if (flags & UNSIGNED) { \
	    SSCANF_TYPE(unsigned type); \
	} else { \
	    SSCANF_TYPE(type); \
	}


	partAssigned = partConsumed = 0;

	switch (c) {

	    case CT_VLS:
		/* %V %#V conversion */
		{
		    struct bu_vls *vls = NULL;

		    if (src[numCharsConsumed] == '\0') {
			EXIT_DUE_TO_INPUT_FAILURE;
		    }

		    /* Leading input whitespace is skipped for %#V, and for %V if the
		     * conversion specification is preceded by at least one whitespace
		     * character.
		     */
		    if (isspace((int)(*bu_vls_addr(&partFmt))) || flags & ALTERNATE) {
			while (1) {
			    c = src[numCharsConsumed];
			    if (c == '\0' || !isspace(c)) {
				break;
			    }
			    ++numCharsConsumed;
			}
		    }

		    /* if no width provided, width is infinity */
		    if (width == 0 && !(flags & HAVEWIDTH)) {
			width = ~(width & 0);
		    }

		    /* grab vls pointer if we're assigning */
		    if (!(flags & SUPPRESS)) {
			vls = va_arg(ap, struct bu_vls*);
		    }

		    /* Copy characters from src to vls. Stop at width, whitespace
		     * character, or EOI.
		     */
		    if (flags & SUPPRESS) {
			for (i = 0; i < width; ++i) {
			    c = src[numCharsConsumed + i];

			    /* stop at non-matching or EOI */
			    if (c == '\0') {
				break;
			    }
			    if ((flags & ALTERNATE) && isspace(c)) {
				break;
			    }
			    ++partConsumed;
			}
		    } else {
			for (i = 0; i < width; ++i) {
			    c = src[numCharsConsumed + i];

			    /* stop at non-matching or EOI */
			    /* stop at non-matching or EOI */
			    if (c == '\0') {
				break;
			    }
			    if ((flags & ALTERNATE) && isspace(c)) {
				break;
			    }

			    /* copy valid char to vls */
			    bu_vls_putc(vls, c);
			    ++partConsumed;
			}

			if (partConsumed > 0) {
			    /* successful assignment */
			    ++partAssigned;
			}
		    }
		    break;
		} /* CT_VLS */

	    case CT_CHAR:
	    case CT_CCL:
	    case CT_STRING:

		/* %lc %l[...] %ls are unsupported */
		if (flags & LONG) {
		    EXIT_DUE_TO_MISC_ERROR;
		}

		/* unsuppressed %s or %[...] conversion */
		if (!(flags & SUPPRESS)) {
		    if (width == 0) {
			if (flags & HAVEWIDTH) {
			    /* Caller specified zero width in the format string.
			     * (%0c %0s or %0[...])
			     *
			     * The behavior of sscanf for a zero width is
			     * undefined, so we provide our own consistent
			     * behavior here.
			     *
			     * The assignment wasn't suppressed, so we'll assume
			     * the caller provided a pointer and wants us to write
			     * to it. Just write '\0' and call it a successful
			     * assignment.
			     */
			    *va_arg(ap, char*) = '\0';
			    ++partAssigned;
			    break;
			} else if (c != CT_CHAR) {
			    struct bu_vls err = BU_VLS_INIT_ZERO;

			    /* No width was provided by caller.
			     *
			     * If the caller is using %s or %[...] without a
			     * maximum field width, then there is a bug in the
			     * caller code.
			     *
			     * sscanf could easily overrun the provided buffer and
			     * cause a program crash, so just bomb here and make
			     * the source of the problem clear.
			     */
			    bu_vls_sprintf(&err, "ERROR.\n"
					   "  bu_sscanf was called with bad format string: \"%s\"\n"
					   "  %%s and %%[...] conversions must be bounded using "
					   "a maximum field width.", fmt0);
			    bu_bomb(bu_vls_addr(&err));
			}
		    }
		}

		/* ordinary %c or %[...] or %s conversion */
		SSCANF_TYPE(char*);
		break;

		/* %[dioupxX] conversion */
	    case CT_INT:
		if (flags & SHORT) {
		    SSCANF_SIGNED_UNSIGNED(short int*);
		} else if (flags & SHORTSHORT) {
#ifndef HAVE_C99_FORMAT_SPECIFIERS
		    /* Assume MSVC, where there is no equivalent of %hh[diouxX].
		     * Will use %h[diouxX] with short instead, then cast into
		     * char argument.
		     */
		    if (flags & SUPPRESS) {
			partAssigned = sscanf(&src[numCharsConsumed],
					      bu_vls_addr(&partFmt), &partConsumed);
		    } else {
			if (flags & UNSIGNED) {
			    unsigned short charConvVal = 0;
			    partAssigned = sscanf(&src[numCharsConsumed],
						  bu_vls_addr(&partFmt), &charConvVal,
						  &partConsumed);
			    *va_arg(ap, unsigned char*) = (unsigned char)charConvVal;
			} else {
			    short charConvVal = 0;
			    partAssigned = sscanf(&src[numCharsConsumed],
						  bu_vls_addr(&partFmt), &charConvVal,
						  &partConsumed);
			    *va_arg(ap, signed char*) = (signed char)charConvVal;
			}
		    }

#else
		    SSCANF_SIGNED_UNSIGNED(char*);
#endif
		} else if (flags & LONG) {
		    SSCANF_SIGNED_UNSIGNED(long int*);
		} else if (flags & LONGLONG) {
		    SSCANF_SIGNED_UNSIGNED(long long int*);
		} else if (flags & POINTER) {
		    SSCANF_TYPE(void*);
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

		/* %[aefgAEFG] conversion */
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
	    EXIT_DUE_TO_INPUT_FAILURE;
	}

	/* check that assignment was successful */
	if (!(flags & SUPPRESS) && partAssigned < 1) {
	    EXIT_DUE_TO_MATCH_FAILURE;
	}

	/* Conversion successful, on to the next one! */
	UPDATE_COUNTS;

    } /* while (1) */

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
