/*                          A R G V . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
#include <ctype.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/file.h"
#include "bu/exit.h"

static void
process_quote(char **lpp, int *quoted)
{
    char *cp = *lpp;

    // Starting a quote
    if (!(*quoted)) {
	/* Shift everything to the left (i.e. stomp on the quote character) */
	while (*cp != '\0') {
	    *cp = *(cp+1);
	    cp++;
	}
    }

    // Ending a quote
    if (*quoted) {
	*cp = '\0';
	(*lpp)++;
    }

    /* toggle quoted state */
    *quoted = (*quoted) ? 0 : 1;
}

static void
process_escape(char **lp)
{
    char *cp = *lp;
    /* Shift everything to the left (i.e. stomp on the escape character) */
    while (*cp != '\0') {
	*cp = *(cp+1);
	cp++;
    }
}


/* skip leading whitespace */
static void
skip_whitespace(char **lpp)
{
    while (*(*lpp) != '\0' && isspace((int)(*(*lpp)))) {
	/* null out spaces */
	*(*lpp) = '\0';
	(*lpp)++;
    }
}

size_t
bu_argv_from_string(char *argv[], size_t lim, char *lp)
{
    size_t argc = 0; /* number of words seen */
    int quoted = 0;
    int escaped = 0;

    if (UNLIKELY(!argv)) {
	/* do this instead of crashing */
	bu_bomb("bu_argv_from_string received a null argv\n");
    }

    /* argv is expected to be at least lim+1 */
    argv[0] = (char *)NULL;

    if (UNLIKELY(lim == 0 || !lp)) {
	/* nothing to do, only return NULL in argv[0] */
	return 0;
    }
    char *echar = &(lp[strlen(lp)-1]);

    /* skip leading whitespace */
    skip_whitespace(&lp);

    if (*lp == '\0') {
	/* no words, only return NULL in argv[0] */
	return 0;
    }


    // If we're leading off with a quoted string we need to handle the leading
    // quote before we assign the argv pointer, to avoid capturing the arg's
    // quote along with the string contents
    if (*lp == '"')
	process_quote(&lp, &quoted);

    // Similarly, if we're leading with an escaped character handle the
    // string adjustment before argv assignment
    if (*lp == '\\') {
	// Handle the implications of the escaped char
	process_escape(&lp);
	escaped = 1;
    }

    /* some non-space string has been seen, set argv[0] */
    argv[argc] = lp;
    argc = 1;

    for (; *lp != '\0'; lp++) {

	if (*lp == '\\') {
	    // If this escape character was previously escaped,
	    // it's just a normal char
	    if (escaped) {
		escaped = 0;
		continue;
	    }

	    // Handle the implications of the escaped char
	    process_escape(&lp);
	    escaped = 1;
	    continue;
	}

	if (*lp == '"') {
	    if (escaped) {
		// An escaped quote is a normal char
		escaped = 0;
		continue;
	    }

	    // Adjust the string based on the implications of the quote
	    process_quote(&lp, &quoted);

	    // If we're starting a quote, repeat the previous processing step
	    // with the string left shifted to remove the starting quote char.
	    if (quoted)
		continue;

	    goto nextword;
	}

	// We've handled all the characters that might have been escaped -
	// clear the flag for the next round
	escaped = 0;

	// skip over current word
	if (quoted || !isspace((int)(*lp))) {
	    continue;
	}

nextword:
	// We either have a space or we're at the end of the string - either
	// way, the arg is complete.  Terminate it with a NULL - if this is
	// not the last arg, this replaces the space in the string with a
	// NULL terminator to allow C to interpret the portion of the string
	// pointed to by argv[argc] as its own string.
	if (lp == echar)
	    goto nullterm;
	*lp = '\0';
	lp++;

	// If we have additional whitespace, we need to clear it
	skip_whitespace(&lp);

	// If we're staring up a quoted string we need to handle the leading
	// quote before we assign the argv pointer, to avoid capturing the
	// arg's quote along with the string contents
	if (*lp == '"')
	    process_quote(&lp, &quoted);

	// Similarly, if we're leading with an escaped character handle the
	// string adjustment before argv assignment
	if (*lp == '\\') {
	    // Handle the implications of the escaped char
	    process_escape(&lp);
	    escaped = 1;
	}

	// If we have nothing left, we're done
	if (*lp == '\0')
	    goto nullterm;

	// make sure argv[] isn't full, need room for NULL.  Because (per the
	// header comments) the user is supposed to have passed in the maximum
	// number of elements *not including a terminating null*, we check if
	// we are at or beyond that user provided limit.
	if (argc >= lim)
	    break;

	/* start of next word */
	argv[argc] = lp;
	argc++;
    }

nullterm:
    /* always NULL-terminate the array */
    argv[argc] = (char *)NULL;

    return argc;
}


void
bu_argv_free(size_t argc, char *argv[])
{
    register size_t i;

    if (UNLIKELY(!argv))
	return;

    if ((ssize_t)argc < 1) {
	bu_free((void *)argv, "bu_argv_free");
	return;
    }

    for (i = 0; i < argc; ++i) {
	bu_free((void *)argv[i], "bu_argv_free");
	argv[i] = NULL; /* sanity */
    }

    bu_free((void *)argv, "bu_argv_free");
    argv = NULL;
}


void
bu_free_args(size_t argc, char *argv[], const char *str)
{
    size_t count = 0;

    if (UNLIKELY(!argv || (ssize_t)argc < 1)) {
	return;
    }

    while (count < argc) {
	bu_free(argv[count], str);
	argv[count] = NULL;
	count++;
    }

    return;
}


char **
bu_argv_dup(size_t argc, const char *argv[])
{
    register size_t i;
    char **av;

    if (UNLIKELY((ssize_t)argc < 1))
	return (char **)0;

    av = (char **)bu_calloc(argc+1, sizeof(char *), "bu_copy_argv");
    for (i = 0; i < argc; ++i)
	av[i] = bu_strdup(argv[i]);
    av[i] = (char *)0;

    return av;
}


char **
bu_argv_dupinsert(int insert, size_t insertArgc, const char *insertArgv[], size_t argc, const char *argv[])
{
    register size_t i, j;
    size_t ac = argc + insertArgc + 1;
    char **av;

    /* Nothing to insert */
    if ((ssize_t)insertArgc < 1)
	return bu_argv_dup(argc, argv);

    av = (char **)bu_calloc(ac, sizeof(char *), "bu_insert_argv");

    if (insert <= 0) {			    	/* prepend */
	for (i = 0; i < insertArgc; ++i)
	    av[i] = bu_strdup(insertArgv[i]);

	for (j = 0; j < argc; ++i, ++j)
	    av[i] = bu_strdup(argv[j]);
    } else if (argc <= (size_t)insert) {	/* append */
	for (i = 0; i < argc; ++i)
	    av[i] = bu_strdup(argv[i]);

	for (j = 0; j < insertArgc; ++i, ++j)
	    av[i] = bu_strdup(insertArgv[j]);
    } else {					/* insert */
	for (i = 0; i < (size_t)insert; ++i)
	    av[i] = bu_strdup(argv[i]);

	for (j = 0; j < insertArgc; ++i, ++j)
	    av[i] = bu_strdup(insertArgv[j]);

	for (j = insert; j < argc; ++i, ++j)
	    av[i] = bu_strdup(argv[j]);
    }

    av[i] = (char *)0;

    return av;
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
