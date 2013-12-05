/*                        E S C A P E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2013 United States Government as represented by
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


static int
expand_expression(const char *expression, struct bu_vls *vp)
{
    const char *ep = expression;
	const char *cp;

    if (!expression)
	return 0;

    /* skip circumflex */
    if (ep[0] == '^')
	ep++;

    bu_vls_extend(vp, 255); /* max ascii */
    for (cp = ep; *cp != '\0'; cp++) {
	bu_vls_strncat(vp, cp, 1);
    }

    return (expression[0] == '^');
}


char *
bu_str_escape(const char *input, const char *expression, char *output, size_t size)
{
    const char *c = NULL;
    const char *esc = NULL;
    struct bu_vls v = BU_VLS_INIT_ZERO;
    char *chars = NULL;
    char *incpy = NULL;
    int negative = 0;
    size_t need = 0;
    size_t i = 0;

    if (UNLIKELY(!input))
	return bu_strdup("");

    /* expand the expression to the list of chars it represents */
    negative = expand_expression(expression, &v);
    chars = bu_vls_strgrab(&v);

    /* first pass, calculate space requirement.  this is done so we
     * don't partially fill the output buffer before bombing because
     * there might be a bomb-hook registered (and it's faster).
     */
    need = strlen(input);
    for (c = input; *c != '\0'; c++) {
	if (negative) {
	    for (esc = chars; *esc != '\0'; esc++) {
		if (*c == *esc) {
		    break;
		}
	    }
	    if (*esc == '\0') {
		need++;
	    }
	} else {
	    for (esc = chars; *esc != '\0'; esc++) {
		if (*c == *esc) {
		    need++;
		    break;
		}
	    }
	}
    }

    /* allocate dynamic space if output is NULL */
    if (!output) {
	size = need + 1;
	output = (char *)bu_calloc(size, 1, "bu_str_escape");
    } else {
	/* copy input buffer if same or overlapping output buffer */
	if (input == output
	    || (input < output && input+strlen(input)+1 > output)
	    || (input > output && output+need+1 > input))
	{
	    incpy = bu_strdup(input);
	}
    }

    /* make sure we have enough space to work */
    if (UNLIKELY(size < need + 1))
	bu_bomb("INTERNAL ERROR: bu_str_escape() output buffer size is inadequate\n");

    /* second pass, scan through all input characters looking for
     * characters to escape.  write to the output buffer.
     */
    for (c = incpy ? incpy : input; *c != '\0'; c++) {
	if (negative) {
	    for (esc = chars; *esc != '\0'; esc++) {
		if (*c == *esc) {
		    break;
		}
	    }
	    if (*esc == '\0') {
		output[i++] = '\\';
	    }
	} else {
	    for (esc = chars; *esc != '\0'; esc++) {
		if (*c == *esc) {
		    output[i++] = '\\';
		    break;
		}
	    }
	}
	output[i++] = *c;
    }
    /* always null-terminate */
    output[i] = '\0';

    if (incpy)
	bu_free(incpy, "bu_str_escape strdup");
    bu_free(chars, "free strgrab");

    return output;
}


char *
bu_str_unescape(const char *input, char *output, size_t size)
{
    const char *c = NULL;
    char *incpy = NULL;
    size_t need = 0;
    size_t i = 0;

    if (UNLIKELY(!input))
	return bu_strdup("");

    /* first pass, calculate space requirement.  this is done so we
     * don't partially fill the output buffer before bombing because
     * there might be a bomb-hook registered (and it's faster).
     */
    need = strlen(input);
    for (c = input; *c != '\0'; c++) {
	if (*c == '\\') {
	    need--;

	    /* skip the next char */
	    c++;
	    if (*c == '\0')
		break;
	}
    }

    /* allocate dynamic space if output is NULL */
    if (!output) {
	size = need + 1;
	output = (char *)bu_calloc(size, 1, "bu_str_unescape");
    } else {
	/* copy input buffer output buffer starts within the input
	 * buffer (output is "in front" of the input).  it's okay if
	 * output is behind or equal because output is always going to
	 * be shorter or equal to input buffer.
	 */
	if (input < output && input+strlen(input)+1 > output) {
	    incpy = bu_strdup(input);
	}
    }

    /* make sure we have enough space to work */
    if (UNLIKELY(size < need + 1))
	bu_bomb("INTERNAL ERROR: bu_str_unescape() output buffer size is inadequate\n");

    /* second pass, scan through all input characters looking for
     * characters to unescape.  write to the output buffer.
     */
    for (c = incpy ? incpy : input; *c != '\0'; c++) {
	if (*c == '\\') {
	    c++; /* skip */
	}
	/* make sure last char wasn't a backslash */
	if (*c != '\0')
	    output[i++] = *c;
	else
	    break;
    }
    /* always null-terminate */
    output[i] = '\0';

    if (incpy)
	bu_free(incpy, "bu_str_unescape strdup");

    return output;
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
