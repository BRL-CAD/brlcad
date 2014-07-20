/*                       E N C O D E . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2014 United States Government as represented by
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

#include "bu/vls.h"
#include "bu/log.h"


static const char DQUOTE = '"';
static const char ESCAPE = '\\';


const char *
bu_vls_encode(struct bu_vls *vp, const char *str)
{
    static const char *empty = "";
    int skip = 0;
    const char *escape_chars = "\" {}\\$[]\0";
    const char *ec = NULL;
    int needs_escaping = 0;

    if (UNLIKELY(!str))
	return empty;

    BU_CK_VLS(vp);

    skip = bu_vls_strlen(vp);

    ec = escape_chars;
    while (*ec != '\0') {
	if (strchr(str, *ec) != NULL) needs_escaping = 1;
	ec++;
    }

    if (needs_escaping) {
	bu_vls_putc(vp, DQUOTE);
	for (; *str != '\0'; str++) {
	    int needs_escape = 0;
	    ec = escape_chars;
	    while (*ec != '\0') {
		if (*str == *ec) needs_escape = 1;
		ec++;
	    }
	    if (needs_escape) {
		bu_vls_putc(vp, ESCAPE);
	    }
	    bu_vls_putc(vp, *str);
	}
	bu_vls_putc(vp, DQUOTE);
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
