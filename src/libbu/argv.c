/*                          A R G V . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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


size_t
bu_argv_from_string(char *argv[], size_t lim, char *lp)
{
    size_t argc = 0; /* number of words seen */
    size_t skip = 0;
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

    /* skip leading whitespace */
    while (*lp != '\0' && isspace((int)(*lp)))
	lp++;

    if (*lp == '\0') {
	/* no words, only return NULL in argv[0] */
	return 0;
    }

    /* some non-space string has been seen, set argv[0] */
    argc = 0;
    argv[argc] = lp;

    for (; *lp != '\0'; lp++) {

	if (*lp == '\\') {
	    char *cp = lp;

	    /* Shift everything to the left (i.e. stomp on the escape character) */
	    while (*cp != '\0') {
		*cp = *(cp+1);
		cp++;
	    }

	    /* mark the next character as escaped */
	    escaped = 1;

	    /* remember the loops lp++ */
	    lp--;

	    continue;
	}

	if (*lp == '"') {
	    if (!quoted) {
		char *cp = lp;

		/* start collecting quoted string */
		quoted = 1;

		if (!escaped) {
		    /* Shift everything to the left (i.e. stomp on the quote character) */
		    while (*cp != '\0') {
			*cp = *(cp+1);
			cp++;
		    }

		    /* remember the loops lp++ */
		    lp--;
		}

		continue;
	    }

	    /* end qoute */
	    quoted = 0;
	    if (escaped)
		lp++;
	    else
		*lp++ = '\0';

	    /* skip leading whitespace */
	    while (*lp != '\0' && isspace((int)(*lp))) {
		/* null out spaces */
		*lp = '\0';
		lp++;
	    }

	    skip = 0;
	    goto nextword;
	}

	escaped = 0;

	/* skip over current word */
	if (quoted || !isspace((int)(*lp)))
	    continue;

	skip = 0;

	/* terminate current word, skip space until we find the start
	 * of the next word nulling out the spaces as we go along.
	 */
	while (*(lp+skip) != '\0' && isspace((int)(*(lp+skip)))) {
	    lp[skip] = '\0';
	    skip++;
	}

	if (*(lp + skip) == '\0')
	    break;

    nextword:
	/* make sure argv[] isn't full, need room for NULL */
	if (argc >= lim-1)
	    break;

	/* start of next word */
	argc++;
	argv[argc] = lp + skip;

	/* jump over the spaces, remember the loop's lp++ */
	lp += skip - 1;
    }

    /* always NULL-terminate the array */
    argc++;
    argv[argc] = (char *)NULL;

    return argc;
}


void
bu_argv_free(size_t argc, char *argv[])
{
    register size_t i;

    if (UNLIKELY(!argv || (ssize_t)argc < 1)) {
	return;
    }

    for (i = 0; i < argc; ++i) {
	if (argv[i]) {
	    bu_free((void *)argv[i], "bu_argv_free");
	    argv[i] = NULL; /* sanity */
	}
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
	if (argv[count]) {
	    bu_free(argv[count], str);
	    argv[count] = NULL;
	}
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
