/*                        V L S _ V P R I N T F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2012 United States Government as represented by
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#ifdef HAVE_STDINT_H
#   include <stdint.h>
#endif

#include "bio.h"

#include "bu.h"

#include "./vls_internals.h"

/* private constants */

/* bit flags for fmt specifier attributes */
/* short (char) length modifiers */
static const int SHORTINT =  0x0001;
static const int SHHRTINT =  0x0002;
/* integer length modifiers */
static const int LONG_INT =  0x0004;
static const int LLONGINT =  0x0008;
/* double length modifiers */
static const int LONGDBLE =  0x0010;
/* other integer length modifiers */
static const int INTMAX_T =  0x0020;
static const int PTRDIFFT =  0x0040;
static const int SIZETINT =  0x0080;
/* misc */
static const int FIELDLEN =  0x0100;
static const int PRECISION = 0x0200;
/* groups */
#define MISCINTMODS    (INTMAX_T | PTRDIFFT | SIZETINT)
#define SHORTINTMODS   (SHORTINT | SHHRTINT)
#define LONGINTMODS    (LONG_INT | LLONGINT)
#define ALL_INTMODS    (SHORTINTMODS | LONGINTMODS | MISCINTMODS)
#define ALL_DOUBLEMODS (LONGDBLE)
#define ALL_LENGTHMODS (ALL_INTMODS | ALL_DOUBLEMODS)

/* private structs */

typedef struct
vprintf_flags
{
    int fieldlen;
    int flags;
    int have_digit;
    int have_dot;
    int left_justify;
    int precision;
} vflags_t;

/* private structs */

static void reset_vflags(vflags_t *f);
static void reset_vflags(vflags_t *f)
{
    f->fieldlen     = -1;
    f->flags        =  0;
    f->have_digit   =  0;
    f->have_dot     =  0;
    f->left_justify =  0;
    f->precision    =  0;
}

#if 0 /* comment out temporarily to prevent unused warning/error */
static int _is_format_flag(const char c);
static
int
_is_format_flag(const char c)
{
    /* from 'man sprintf' */
    switch (c) {
        /* defined in C standard; */
        case '#':
        case '0':
        case '-':
        case ' ':
        case '+':
        case '\'': /* SUSv2 */
        case 'I':  /* glibc 2.2 */
            return 1;
            break;
        default:
            return 0;
            break;
    }
}

static int _is_format_length_modifier(const char c);
static
int
_is_format_length_modifier(const char c)
{
    /* from 'man sprintf' */
    switch (c) {
        /* defined in C standard; */
        case 'h': /* can be doubled: 'hh' */
        case 'l': /* can be doubled: 'll' */
        case 'L':
        case 'j':
        case 'z':
        case 't':

        /* obsolete */
        case 'q': /* 4.4BSD and libc5 only--don't use */
            return 1;
            break;
        default:
            return 0;
            break;
    }
}

static int _is_format_conversion_specifier(const char c);
static
int
_is_format_conversion_specifier(const char c)
{
    /* from 'man sprintf' */
    switch (c) {
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
        case 'c':
        case 's':
        case 'p':
        case 'n':
        case 'm': /* glibc extension */
        case '%':
        case 'V': /* bu_vls extension */

        /* obsolete: */
        case 'C': /* (Not in C99, but in SUSv2.)  Synonym for lc.  Don't use. */
        case 'D': /* libc4--don't use */
        case 'O': /* libc5--don't use */
        case 'S': /* (Not in C99, but in SUSv2.)  Synonym for lc.  Don't use. */
        case 'U': /* libc5--don't use */
            return 1;
            break;
        default:
            return 0;
            break;
    }
}
#endif

void
bu_vls_vprintf(struct bu_vls *vls, const char *fmt, va_list ap)
{
    const char *sp; /* start pointer */
    const char *ep; /* end pointer */
    int len;

    /* flag variables are reset for each fmt specifier */
    vflags_t f;

    char buf[BUFSIZ] = {0};
    char c;

    struct bu_vls fbuf = BU_VLS_INIT_ZERO; /* % format buffer */
    const char *fbufp  = NULL;

    if (UNLIKELY(!vls || !fmt || fmt[0] == '\0')) {
	/* nothing to print to or from */
	return;
    }

    BU_CK_VLS(vls);

    bu_vls_extend(vls, (unsigned int)_VLS_ALLOC_STEP);

    sp = fmt;
    while (*sp) {
	/* Initial state:  just printing chars */
	fmt = sp;
	while (*sp != '%' && *sp)
	    sp++;

	if (sp != fmt)
	    bu_vls_strncat(vls, fmt, (size_t)(sp-fmt));

	if (*sp == '\0')
	    break;

        /* Saw a percent sign, now need to find end of fmt specifier */
        /* All flags get reset for this fmt specifier */
        reset_vflags(&f);

        ep = sp;
	while ((c = *(++ep))) {
            if (c == ' '
                || c == '#'
                || c == '+'
                || c == '.'
                || c == '\''
                || isdigit(c)) {
                /* need to set flags for some of these */
                if (c == '.') {
                    f.have_dot = 1;
                } else if (isdigit(c)) {
                    /* set flag for later error checks */
                    f.have_digit = 1;
                }
		continue;
	    } else if (c == '-') {
                /* the first occurrence before a dot is the
                 left-justify flag, but the occurrence AFTER a dot is
                 taken to be zero precision */
                if (f.have_dot) {
                  f.precision  = 0;
                  f.have_digit = 0;
                } else if (f.have_digit) {
                    /* FIXME: ERROR condition?: invalid format string
                       (e.g., '%7.8-f') */
                    /* seems as if the fprintf man page is indefinite here,
                       looks like the '-' is passed through and
                       appears in output */
                    ;
                } else {
                    f.left_justify = 1;
                }
	    } else if (c == '*') {
                /* the first occurrence is the field width, but the
                   second occurrence is the precision specifier */
                if (!f.have_dot) {
		    f.fieldlen = va_arg(ap, int);
		    f.flags |= FIELDLEN;
                }
                else {
		    f.precision = va_arg(ap, int);
		    f.flags |= PRECISION;
                }
                /* all length modifiers below here */
	    } else if (c == 'j') {
		f.flags |= INTMAX_T;
	    } else if (c == 't') {
		f.flags |= PTRDIFFT;
	    } else if (c == 'z') {
		f.flags |= SIZETINT;
	    } else if (c == 'l') {
                /* 'l' can be doubled */
                /* clear all length modifiers AFTER we check for the
                   first 'l' */
		if (f.flags & LONG_INT) {
                    f.flags ^= ALL_LENGTHMODS;
                    f.flags |= LLONGINT;
		} else {
                    f.flags ^= ALL_LENGTHMODS;
                    f.flags |= LONG_INT;
		}
	    } else if (c == 'h') {
                /* 'h' can be doubled */
                /* clear all length modifiers AFTER we check for the
                   first 'h' */
		if (f.flags & SHORTINT) {
                    f.flags ^= ALL_LENGTHMODS;
		    f.flags |= SHHRTINT;
		} else {
                    f.flags ^= ALL_LENGTHMODS;
		    f.flags |= SHORTINT;
		}
	    } else if (c == 'L') {
                /* a length modifier for doubles */
                /* clear all length modifiers first */
                f.flags ^= ALL_LENGTHMODS;
                /* set the new flag */
                f.flags |= LONGDBLE;
	    } else
		/* Anything else must be the end of the fmt specifier */
		break;
	}

	/* libc left-justifies if there's a '-' char, even if the
	 * value is already negative, so no need to check current value
	 * of left_justify.
	 */
	if (f.fieldlen < 0) {
	    f.fieldlen = -f.fieldlen;
	    f.left_justify = 1;
	}

	/* Copy off this entire format string specifier */
	len = ep-sp+1;

	/* intentionally avoid bu_strlcpy here since the source field
	 * may be legitimately truncated.
	 */
	bu_vls_strncpy(&fbuf, sp, (size_t)len);
	fbufp = bu_vls_addr(&fbuf);

#ifndef HAVE_C99_FORMAT_SPECIFIERS
	/* if the format string uses the %z width specifier, we need
	 * to replace it with something more palatable to this busted
	 * compiler.
	 */

	if (f.flags & SIZETINT) {
	    char *fp = fbufp;
	    while (*fp) {
		if (*fp == '%') {
		    /* found the next format specifier */
		    while (*fp) {
			fp++;
			/* possible characters that can preceed the
			 * field length character (before the type).
			 */
			if (isdigit(*fp)
			    || *fp == '$'
			    || *fp == '#'
			    || *fp == '+'
			    || *fp == '.'
			    || *fp == '-'
			    || *fp == ' '
			    || *fp == '*') {
			    continue;
			}
			if (*fp == 'z') {
			    /* assume MSVC replacing instances of %z
			     * with %I (capital i) until we encounter
			     * anything different.
			     */
			    *fp = 'I';
			}

			break;
		    }
		    if (*fp == '\0') {
			break;
		    }
		}
		fp++;
	    }
	}
#endif

	/* use type specifier to grab parameter appropriately from arg
           list, and print it correctly */
	switch (c) {
	    case 's':
		{
                    /* variables used to determine final effects of
                       field length and precision (different for
                       strings versus numbers) */
                    int minfldwid = -1;
                    int maxstrlen = -1;

                    char *str = va_arg(ap, char *);

                    /* for strings only */
                    /* field length is a minimum size and precision is
                       max length of string to be printed */
		    if (f.flags & FIELDLEN) {
                        minfldwid = f.fieldlen;
                    }
		    if (f.flags & PRECISION) {
                        maxstrlen = f.precision;
                    }
		    if (str) {
                        int stringlen = (int)strlen(str);
                        struct bu_vls tmpstr = BU_VLS_INIT_ZERO;

                        /* use a copy of the string */
                        bu_vls_strcpy(&tmpstr, str);

                        /* handle a non-empty string */
                        /* strings may be truncated */
                        if (maxstrlen >= 0) {
                            if (maxstrlen < stringlen) {
                                /* have to truncate */
                                bu_vls_trunc(&tmpstr, maxstrlen);
                                stringlen = maxstrlen;
                            } else {
                                maxstrlen = stringlen;
                            }
                        }
                        minfldwid = minfldwid < maxstrlen ? maxstrlen : minfldwid;

                        if (stringlen < minfldwid) {
                            /* padding spaces needed */
                            /* start a temp string to deal with padding */
                            struct bu_vls padded = BU_VLS_INIT_ZERO;
                            int i;

			    if (f.left_justify) {
                                /* string goes before padding spaces */
				bu_vls_vlscat(&padded, &tmpstr);
                            }
                            /* now put in padding spaces in all cases */
			    for (i = 0; i < minfldwid - stringlen; ++i) {
                                bu_vls_putc(&padded, ' ');
                            }
			    if (!f.left_justify) {
                                /* string follows the padding spaces */
				bu_vls_vlscat(&padded, &tmpstr);
                            }
                            /* now we can send the padded string to the tmp string */
                            /* have to truncate it to zero length first */
                            bu_vls_trunc(&tmpstr, 0);
			    bu_vls_vlscat(&tmpstr, &padded);

                            bu_vls_free(&padded);
                        }
                        /* now take string as is */
			bu_vls_vlscat(vls, &tmpstr);

                        bu_vls_free(&tmpstr);
		    } else {
                        /* handle an empty string */
                        /* FIXME: should we trunc to precision if > fieldlen? */
                        if (f.flags & FIELDLEN) {
			    bu_vls_strncat(vls, "(null)", (size_t)f.fieldlen);
                        } else {
			    bu_vls_strcat(vls, "(null)");
                        }
		    }
		}
		break;
	    case 'V':
		{
		    struct bu_vls *vp;

		    vp = va_arg(ap, struct bu_vls *);
		    if (vp) {
			BU_CK_VLS(vp);
			if (f.flags & FIELDLEN) {
			    int stringlen = bu_vls_strlen(vp);

			    if (stringlen >= f.fieldlen)
				bu_vls_strncat(vls, bu_vls_addr(vp), (size_t)f.fieldlen);
			    else {
				struct bu_vls padded = BU_VLS_INIT_ZERO;
				int i;

				if (f.left_justify)
				    bu_vls_vlscat(&padded, vp);
				for (i = 0; i < f.fieldlen - stringlen; ++i)
				    bu_vls_putc(&padded, ' ');
				if (!f.left_justify)
				    bu_vls_vlscat(&padded, vp);
				bu_vls_vlscat(vls, &padded);
			    }
			} else {
			    bu_vls_vlscat(vls, vp);
			}
		    } else {
			if (f.flags & FIELDLEN)
			    bu_vls_strncat(vls, "(null)", (size_t)f.fieldlen);
			else
			    bu_vls_strcat(vls, "(null)");
		    }
		}
		break;
	    case 'e':
	    case 'E':
	    case 'f':
	    case 'g':
	    case 'G':
            case 'F':
		/* All floating point ==> "double" */
		{
		    double d = va_arg(ap, double);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, d);
		    else
			snprintf(buf, BUFSIZ, fbufp, d);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
		if (f.flags & LONG_INT) {
		    /* Unsigned long int */
		    unsigned long l = va_arg(ap, unsigned long);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, l);
		    else
			snprintf(buf, BUFSIZ, fbufp, l);
		} else if (f.flags & LLONGINT) {
		    /* Unsigned long long int */
		    unsigned long long ll = va_arg(ap, unsigned long long);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, ll);
		    else
			snprintf(buf, BUFSIZ, fbufp, ll);
		} else if (f.flags & SHORTINT || f.flags & SHHRTINT) {
		    /* unsigned short int */
		    unsigned short int sh = (unsigned short int)va_arg(ap, int);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, sh);
		    else
			snprintf(buf, BUFSIZ, fbufp, sh);
		} else if (f.flags & INTMAX_T) {
		    intmax_t im = va_arg(ap, intmax_t);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, im);
		    else
			snprintf(buf, BUFSIZ, fbufp, im);
		} else if (f.flags & PTRDIFFT) {
		    ptrdiff_t pd = va_arg(ap, ptrdiff_t);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, pd);
		    else
			snprintf(buf, BUFSIZ, fbufp, pd);
		} else if (f.flags & SIZETINT) {
		    size_t st = va_arg(ap, size_t);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, st);
		    else
			snprintf(buf, BUFSIZ, fbufp, st);
		} else {
		    /* Regular unsigned int */
		    unsigned int j = (unsigned int)va_arg(ap, unsigned int);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbufp, j);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case 'd':
	    case 'i':
		if (f.flags & LONG_INT) {
		    /* Long int */
		    long l = va_arg(ap, long);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, l);
		    else
			snprintf(buf, BUFSIZ, fbufp, l);
		} else if (f.flags & LLONGINT) {
		    /* Long long int */
		    long long ll = va_arg(ap, long long);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, ll);
		    else
			snprintf(buf, BUFSIZ, fbufp, ll);
		} else if (f.flags & SHORTINT || f.flags & SHHRTINT) {
		    /* short int */
		    short int sh = (short int)va_arg(ap, int);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, sh);
		    else
			snprintf(buf, BUFSIZ, fbufp, sh);
		} else if (f.flags & INTMAX_T) {
		    intmax_t im = va_arg(ap, intmax_t);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, im);
		    else
			snprintf(buf, BUFSIZ, fbufp, im);
		} else if (f.flags & PTRDIFFT) {
		    ptrdiff_t pd = va_arg(ap, ptrdiff_t);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, pd);
		    else
			snprintf(buf, BUFSIZ, fbufp, pd);
		} else if (f.flags & SIZETINT) {
		    size_t st = va_arg(ap, size_t);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, st);
		    else
			snprintf(buf, BUFSIZ, fbufp, st);
		} else {
		    /* Regular int */
		    int j = va_arg(ap, int);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbufp, j);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case 'n':
	    case 'p':
		/* all pointer == "void *" */
		{
		    void *vp = (void *)va_arg(ap, void *);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, vp);
		    else
			snprintf(buf, BUFSIZ, fbufp, vp);
		}
		bu_vls_strcat(vls, buf);
		break;
	    case '%':
		bu_vls_putc(vls, '%');
		break;
	    default:
		{
                    /* Something weird, maybe %c */
		    /* We hope, whatever it is, it fits in an int and the resulting
		       stringlet is smaller than sizeof(buf) bytes */
		    int j = va_arg(ap, int);
		    if (f.flags & FIELDLEN)
			snprintf(buf, BUFSIZ, fbufp, f.fieldlen, j);
		    else
			snprintf(buf, BUFSIZ, fbufp, j);
		}
		bu_vls_strcat(vls, buf);
		break;
	}
	sp = ep + 1;
    }

    va_end(ap);

    bu_vls_free(&fbuf);
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
