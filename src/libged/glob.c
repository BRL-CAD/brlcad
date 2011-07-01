/*                         G L O B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/glob.c
 *
 * The glob command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


/**
 * unescapes various special characters
 */
static void
debackslash(struct bu_vls *dest, struct bu_vls *src)
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
static void
backslash(struct bu_vls *dest, struct bu_vls *src)
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
ged_glob(struct ged *gedp, int argc, const char *argv[])
{
    char *start, *end;          /* Start and ends of words */
    int regexp;                 /* Set to TRUE when word is a regexp */
    int backslashed;
    int firstword;
    struct bu_vls word;         /* Current word being processed */
    struct bu_vls temp;
    struct bu_vls src;
    static const char *usage = "expression";

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_vls_init(&word);
    bu_vls_init(&temp);
    bu_vls_init(&src);

    bu_vls_strcat(&src, argv[1]);

    start = end = bu_vls_addr(&src);
    firstword = 1;
    while (*end != '\0') {
	/* Run through entire string */

	/* First, pass along leading whitespace. */

	start = end;                   /* Begin where last word ended */
	while (*start != '\0') {
	    if (*start == ' '  ||
		*start == '\t' ||
		*start == '\n')
		bu_vls_putc(gedp->ged_result_str, *start++);
	    else
		break;
	}
	if (*start == '\0')
	    break;

	/* Next, advance "end" pointer to the end of the word, while adding
	   each character to the "word" vls.  Also make a note of any
	   unbackslashed wildcard characters. */

	end = start;
	bu_vls_trunc(&word, 0);
	regexp = 0;
	backslashed = 0;
	while (*end != '\0') {
	    if (*end == ' '  ||
		*end == '\t' ||
		*end == '\n')
		break;
	    if ((*end == '*' || *end == '?' || *end == '[') && !backslashed)
		regexp = 1;
	    if (*end == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	    bu_vls_putc(&word, *end++);
	}

	/* Now, if the word was suspected of being a wildcard, try to match
	   it to the database. */

	if (regexp) {
	    GED_CHECK_DATABASE_OPEN(gedp, GED_INITIALIZED(gedp) ? GED_ERROR : GED_OK);
	    bu_vls_trunc(&temp, 0);
	    if (db_regexp_match_all(&temp, gedp->ged_wdbp->dbip,
				    bu_vls_addr(&word)) == 0) {
		debackslash(&temp, &word);
		backslash(gedp->ged_result_str, &temp);
	    } else
		bu_vls_vlscat(gedp->ged_result_str, &temp);
	} else {
	    debackslash(gedp->ged_result_str, &word);
	}

	firstword = 0;
    }

    bu_vls_free(&temp);
    bu_vls_free(&word);
    bu_vls_free(&src);

    return GED_OK;
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
