/*                         Q U O T E . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2012 United States Government as represented by
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

#include <string.h>

#include "bu.h"


static const char SPACE = ' ';
static const char DQUOTE = '"';
static const char ESCAPE = '\\';

/* known failure cases (round trip):

{\}         -> {}         [FAIL]  (should be: {\})
{\\}        -> {\}        [FAIL]  (should be: {\\})
{\hello}    -> {hello}    [FAIL]  (should be: {\hello})
{\hello"}   -> {hello"}   [FAIL]  (should be: {\hello"})
{hello\\}   -> {hello\}   [FAIL]  (should be: {hello\\})

*/

/* current description from bu.h: */
/**
 * given an input string, wrap the string in double quotes if there is
 * a space and append it to the provided bu_vls.  escape any existing
 * double quotes.
 *
 * TODO: consider a specifiable quote character and octal encoding
 * instead of double quote wrapping.  perhaps specifiable encode type:
 *   BU_ENCODE_QUOTE
 *   BU_ENCODE_OCTAL
 *   BU_ENCODE_XML
 *
 * the behavior of this routine is subject to change but should remain
 * a reversible operation when used in conjunction with
 * bu_vls_decode().
 *
 * returns a pointer to the encoded string (i.e., the substring held
 * within the bu_vls)
 */

/*

I think the intent is to take a string and encode it so it can be used
in a printf statement (except that doesn't explain why the space is
escaped).  If that is the case, then we would escape ''' and '"' and '\'
characters but only if they.

*/

const char *
bu_vls_encode(struct bu_vls *vp, const char *str)
{
    static const char *empty = "";
    int skip = 0;

    if (UNLIKELY(!str))
	return empty;

    BU_CK_VLS(vp);

    skip = bu_vls_strlen(vp);

    if (strchr(str, SPACE) == NULL) {
	/* no spaces, just watch for quotes */
	for (; *str != '\0'; str++) {
	    if (*str == DQUOTE) {
		bu_vls_putc(vp, ESCAPE);
	    }
	    bu_vls_putc(vp, *str);
	}
    } else {
	/* argv elements has spaces, quote it */
	bu_vls_putc(vp, DQUOTE);
	for (; *str != '\0'; str++) {
	    if (*str == DQUOTE) {
		bu_vls_putc(vp, ESCAPE);
	    } else if (*str == SPACE) {
		bu_vls_putc(vp, ESCAPE);
	    }
	    bu_vls_putc(vp, *str);
	}
    }

    return bu_vls_addr(vp) + skip;
}


const char *
bu_vls_decode(struct bu_vls *vp, const char *str)
{
    static const char *empty = "";

    int skip = 0;
    int dquote = 0;
    int escape = 0;

    struct bu_vls quotebuf = BU_VLS_INIT_ZERO;

    if (UNLIKELY(!str))
	return empty;

    BU_CK_VLS(vp);

    skip = bu_vls_strlen(vp);

    for (; *str != '\0'; str++) {
	if (escape) {
	    /* previous character was escaped */
	    if (dquote) {
		bu_vls_putc(&quotebuf, *str);
	    } else {
		bu_vls_putc(vp, *str);
	    }
	    escape = 0;
	    continue;
	}

	if (*str == ESCAPE) {
	    /* encountered new escape */
	    escape = 1;
	    continue;
	}

	if (*str == DQUOTE) {
	    if (!dquote) {
		/* entering double quote pairing */
		dquote = 1;
	    } else {
		/* end of double quote */
		dquote = 0;
		bu_vls_vlscatzap(vp, &quotebuf);
	    }
	    continue;
	}

	/* if we're inside a quote, buffer up the string until we find
	 * a matching double quote character.
	 */
	if (dquote) {
	    bu_vls_putc(&quotebuf, *str);
	} else {
	    bu_vls_putc(vp, *str);
	}
    }

    if (dquote) {
	/* we got to the end of the input string while still inside a
	 * double-quote.  have to assume the quote is regular content.
	 */
	bu_vls_putc(vp, DQUOTE);
	bu_vls_vlscatzap(vp, &quotebuf);
    }

    bu_vls_free(&quotebuf);

    return bu_vls_addr(vp) + skip;
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
