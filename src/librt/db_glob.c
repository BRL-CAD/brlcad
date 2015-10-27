/*                        D B _ G L O B . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_glob.c
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu/path.h"
#include "bu/vls.h"
#include "raytrace.h"

/**
 * unescapes various special characters
 */
HIDDEN void
_debackslash(struct bu_vls *dest, struct bu_vls *src)
{
    char *ptr;

    ptr = bu_vls_addr(src);
    while (*ptr) {
	if (*ptr == '\\')
	    ++ptr;
	if (*ptr == '\0')
	    break;
	bu_vls_putc(dest, *ptr++);
    }
}

/**
 * escapes various special characters
 */
HIDDEN void
_backslash_specials(struct bu_vls *dest, struct bu_vls *src)
{
    int backslashed;
    char *ptr, buf[2];

    buf[1] = '\0';
    backslashed = 0;
    for (ptr = bu_vls_addr(src); *ptr; ptr++) {
	if (*ptr == '[' && !backslashed)
	    bu_vls_strcat(dest, "\\[");
	else if (*ptr == ']' && !backslashed)
	    bu_vls_strcat(dest, "\\]");
	else if (backslashed) {
	    bu_vls_strcat(dest, "\\");
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	    backslashed = 0;
	} else if (*ptr == '\\')
	    backslashed = 1;
	else {
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	}
    }
}


int
db_expand_str_glob(struct bu_vls *dest, const char *input, struct db_i *dbip, int skip_first)
{
    char *start, *end;          /* Start and ends of words */
    int is_fnmatch;                 /* Set to TRUE when word is a is_fnmatch */
    int backslashed;
    int match_cnt = 0;
    int firstword = 1;
    struct bu_vls word = BU_VLS_INIT_ZERO;         /* Current word being processed */
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    char *src = NULL;

    if (dbip == DBI_NULL) {
	bu_vls_sprintf(dest, "%s", src);
	return 0;
    }

    src = bu_strdup(input);

    start = end = src;
    while (*end != '\0') {
	/* Run through entire string */

	/* First, pass along leading whitespace. */

	start = end;                   /* Begin where last word ended */
	while (*start != '\0') {
	    if (*start == ' '  ||
		    *start == '\t' ||
		    *start == '\n')
		bu_vls_putc(dest, *start++);
	    else
		break;
	}
	if (*start == '\0')
	    break;
	/* Next, advance "end" pointer to the end of the word, while
	 * adding each character to the "word" vls.  Also make a note
	 * of any unbackslashed wildcard characters.
	 */

	end = start;
	bu_vls_trunc(&word, 0);
	is_fnmatch = 0;
	backslashed = 0;
	while (*end != '\0') {
	    if (*end == ' '  ||
		    *end == '\t' ||
		    *end == '\n')
		break;
	    if ((*end == '*' || *end == '?' || *end == '[') && !backslashed)
		is_fnmatch = 1;
	    if (*end == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	    bu_vls_putc(&word, *end++);
	}

	/* If the client told us not to do expansion on the first word
	 * in the stream, disable matching */
	if (firstword && skip_first) {
	    is_fnmatch = 0;
	}

	/* Now, if the word was suspected of being a wildcard, try to
	 * match it to the database.
	 */
	if (is_fnmatch) {
	    register int i, num;
	    register struct directory *dp;
	    bu_vls_trunc(&temp, 0);
	    for (i = num = 0; i < RT_DBNHASH; i++) {
		for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    if (bu_fnmatch(bu_vls_addr(&word), dp->d_namep, 0) != 0)
			continue;
		    if (num == 0)
			bu_vls_strcat(&temp, dp->d_namep);
		    else {
			bu_vls_strcat(&temp, " ");
			bu_vls_strcat(&temp, dp->d_namep);
		    }
		    match_cnt++;
		    ++num;
		}
	    }

	    if (num == 0) {
		_debackslash(&temp, &word);
		_backslash_specials(dest, &temp);
	    } else
		bu_vls_vlscat(dest, &temp);
	} else {
	    _debackslash(dest, &word);
	}

    }

    bu_vls_free(&temp);
    bu_vls_free(&word);
    bu_free(src, "free input cpy");
    return match_cnt;
}



/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
