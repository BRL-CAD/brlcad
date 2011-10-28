/*                       F N M A T C H . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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
/*
 * Based off of OpenBSD's fnmatch.c v 1.13 2006/03/31
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


#define FNMATCH_IGNORECASE  BU_FNMATCH_CASEFOLD
#define FNMATCH_FILE_NAME   BU_FNMATCH_PATHNAME

#define FNMATCH_EOS '\0'

#define FNMATCH_RANGE_MATCH   1
#define FNMATCH_RANGE_NOMATCH 0
#define FNMATCH_RANGE_ERROR   (-1)

/* isblank appears to be obsolete in newer ctype.h files so use
 * fnblank instead when looking for the "blank" character class.
 */
static inline int
fnblank(int c)
{
#ifdef isblank
    return isblank(c);
#else
    return c == ' ' || c == '\t';
#endif
}


static inline int
fnalnum(int c)
{
    return isalnum(c);
}


static inline int
fnalpha(int c)
{
    return isalpha(c);
}


static inline int
fncntrl(int c)
{
    return iscntrl(c);
}


static inline int
fndigit(int c)
{
    return isdigit(c);
}


static inline int
fngraph(int c)
{
    return isgraph(c);
}


static inline int
fnlower(int c)
{
    return islower(c);
}


static inline int
fnprint(int c)
{
    return isprint(c);
}


static inline int
fnpunct(int c)
{
    return ispunct(c);
}


static inline int
fnspace(int c)
{
    return isspace(c);
}


static inline int
fnupper(int c)
{
    return isupper(c);
}


static inline int
fnxdigit(int c)
{
    return isxdigit(c);
}


typedef struct _charclass {
    char *idstring;		/* identifying string */
    int (*checkfun)(int);	/* testing function */
} CHARCLASS;

static CHARCLASS charclasses[] = {
    { "alnum", fnalnum },
    { "alpha", fnalpha },
    { "blank", fnblank },
    { "cntrl", fncntrl },
    { "digit", fndigit },
    { "graph", fngraph },
    { "lower", fnlower },
    { "print", fnprint },
    { "punct", fnpunct },
    { "space", fnspace },
    { "upper", fnupper },
    { "xdigit", fnxdigit },
};


static int
classcompare(const void *a, const void *b)
{
    return bu_strcmp(((CHARCLASS *)a)->idstring, ((CHARCLASS *)b)->idstring);
}


static CHARCLASS *
findclass(char *charclass)
{
    CHARCLASS tmp;
    tmp.idstring = charclass;
    return (CHARCLASS *)bsearch(&tmp, charclasses, sizeof(charclasses)/sizeof(CHARCLASS), sizeof(CHARCLASS), classcompare);
}


static int
charclassmatch(const char *pattern, char test, int *s)
{
    char c;
    int counter = 0;
    int resultholder = 0;
    struct bu_vls classname;
    CHARCLASS *ctclass;
    bu_vls_init(&classname);
    c = *pattern++;
    while (c && (c != ':') && (resultholder != -1)) {
	if (c == FNMATCH_EOS) resultholder = -1;
	counter++;

	c = *pattern++; /* next */
    }
    c = *pattern++;
    if (c != ']') resultholder = -1;
    bu_vls_strncpy(&classname, pattern-counter-2, counter);

    ctclass = findclass(bu_vls_addr(&classname));
    if (ctclass == NULL) {
	bu_log("Unknown character class type: %s\n", bu_vls_addr(&classname));
	resultholder = -1;
    } else {
	/*bu_log("classname: %s, test char = %c, (class->checkfun)=%d\n", bu_vls_addr(&classname), test, (ctclass->checkfun)(test));*/
	if ((ctclass->checkfun)(test) != 0) {
	    resultholder = counter;
	} else {
	    resultholder = 0;
	}
    }
    *s = resultholder;
    bu_vls_free(&classname);
    return counter;
}


static int
_rangematch(const char *pattern, char test, int flags, char **newp)
{
    int negate, ok, s, incpattern;
    char c, c2;
    /*
     * A bracket expression starting with an unquoted circumflex
     * character produces unspecified results (IEEE 1003.2-1992,
     * 3.13.2).  This implementation treats it like '!', for
     * consistency with the regular expression syntax.  J.T. Conklin
     * (conklin@ngai.kaleida.com)
     */
    negate = (*pattern == '!' || *pattern == '^');
    if (negate)
	++pattern;


    if (flags & BU_FNMATCH_CASEFOLD)
	test = (char)tolower((unsigned char)test);

    ok = 0;

    /*
     * A right bracket shall lose its special meaning and represent
     * itself in a bracket expression if it occurs first in the list.
     * -- POSIX.2 2.8.3.2
     */
    c = *pattern++;
    do {
	if (c == '\\' && !(flags & BU_FNMATCH_NOESCAPE))
	    c = *pattern++;
	if (c == FNMATCH_EOS)
	    return FNMATCH_RANGE_ERROR;
	if (c == '/' && (flags & BU_FNMATCH_PATHNAME))
	    return FNMATCH_RANGE_NOMATCH;
	if ((flags & BU_FNMATCH_CASEFOLD))
	    c = (char)tolower((unsigned char)c);
	if (*pattern == '-'
	    && (c2 = *(pattern+1)) != FNMATCH_EOS && c2 != ']') {
	    pattern += 2;
	    if (c2 == '\\' && !(flags & BU_FNMATCH_NOESCAPE))
		c2 = *pattern++;
	    if (c2 == FNMATCH_EOS)
		return FNMATCH_RANGE_ERROR;
	    if (flags & BU_FNMATCH_CASEFOLD)
		c2 = (char)tolower((unsigned char)c2);
	    if (c <= test && test <= c2)
		ok = 1;
	} else if (c == test) {
	    ok = 1;
	} else if ((c == '[') && (*pattern == ':')) {
	    incpattern = charclassmatch(pattern+1, test, &s);
	    if (s == -1) return FNMATCH_RANGE_ERROR;
	    if (s > 0) ok = 1;
	    pattern = pattern + incpattern + 3;
	}
    } while ((c = *pattern++) != ']');

    *newp = (char *)pattern;
    return ok == negate ? FNMATCH_RANGE_NOMATCH : FNMATCH_RANGE_MATCH;
}


int
bu_fnmatch(const char *pattern, const char *string, int flags)
{
    const char *stringstart;
    char *newp;
    char c, test;
    int limit = 10000;

    for (stringstart = string; limit > 0; limit--) {
	switch (c = *pattern++) {
	    case FNMATCH_EOS:
		if ((flags & BU_FNMATCH_LEADING_DIR) && *string == '/')
		    return 0;
		return *string == FNMATCH_EOS ? 0 : BU_FNMATCH_NOMATCH;
	    case '?':
		if (*string == FNMATCH_EOS)
		    return BU_FNMATCH_NOMATCH;
		if (*string == '/' && (flags & BU_FNMATCH_PATHNAME))
		    return BU_FNMATCH_NOMATCH;
		if (*string == '.' && (flags & BU_FNMATCH_PERIOD) &&
		    (string == stringstart ||
		     ((flags & BU_FNMATCH_PATHNAME) && *(string - 1) == '/')))
		    return BU_FNMATCH_NOMATCH;
		++string;
		break;
	    case '*':
		c = *pattern;
		/* Collapse multiple stars. */
		while (c == '*')
		    c = *++pattern;

		if (*string == '.' && (flags & BU_FNMATCH_PERIOD) &&
		    (string == stringstart ||
		     ((flags & BU_FNMATCH_PATHNAME) && *(string - 1) == '/')))
		    return BU_FNMATCH_NOMATCH;

		/* Optimize for pattern with * at end or before /. */
		if (c == FNMATCH_EOS) {
		    if (flags & BU_FNMATCH_PATHNAME)
			return ((flags & BU_FNMATCH_LEADING_DIR) ||
				strchr(string, '/') == NULL ?
				0 : BU_FNMATCH_NOMATCH);
		    else
			return 0;
		} else if (c == '/' && (flags & BU_FNMATCH_PATHNAME)) {
		    if ((string = strchr(string, '/')) == NULL)
			return BU_FNMATCH_NOMATCH;
		    break;
		}

		/* General case, use recursion. */
		while ((test = *string) != FNMATCH_EOS) {
		    if (!bu_fnmatch(pattern, string, flags & ~BU_FNMATCH_PERIOD))
			return 0;
		    if (test == '/' && (flags & BU_FNMATCH_PATHNAME))
			break;
		    ++string;
		}
		return BU_FNMATCH_NOMATCH;
	    case '[':
		if (*string == FNMATCH_EOS)
		    return BU_FNMATCH_NOMATCH;
		if (*string == '/' && (flags & BU_FNMATCH_PATHNAME))
		    return BU_FNMATCH_NOMATCH;
		if (*string == '.' && (flags & BU_FNMATCH_PERIOD) &&
		    (string == stringstart ||
		     ((flags & BU_FNMATCH_PATHNAME) && *(string - 1) == '/')))
		    return BU_FNMATCH_NOMATCH;

		switch (_rangematch(pattern, *string, flags, &newp)) {
		    case FNMATCH_RANGE_ERROR:
			/* not a good range, treat as normal text */
			goto normal;
		    case FNMATCH_RANGE_MATCH:
			pattern = newp;
			break;
		    case FNMATCH_RANGE_NOMATCH:
			return BU_FNMATCH_NOMATCH;
		}
		++string;
		break;
	    case '\\':
		if (!(flags & BU_FNMATCH_NOESCAPE)) {
		    if ((c = *pattern++) == FNMATCH_EOS) {
			c = '\\';
			--pattern;
		    }
		}
		/* FALLTHROUGH */
	    default:
	    normal:
		if (c != *string && !((flags & BU_FNMATCH_CASEFOLD) &&
				      (tolower((unsigned char)c) ==
				       tolower((unsigned char)*string))))
		    return BU_FNMATCH_NOMATCH;
		++string;
		break;
	}
    }

    /* NOTREACHED (unless inf looping) */
    return 0;
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
