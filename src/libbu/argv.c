/*                          A R G V . C
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

#include "common.h"

#include <string.h>

#include "bu.h"


void
bu_free_argv(int argc, char *argv[])
{
    register int i;

    for (i = 0; i < argc; ++i) {
	if (argv[i]) {
	    bu_free((void *)argv[i], "bu_free_argv");
	    argv[i] = NULL; /* sanity */
	}
    }

    bu_free((void *)argv, "bu_free_argv");
    argv = NULL;
}


void
bu_free_array(int argc, char *argv[], const char *str)
{
    int count = 0;

    if (UNLIKELY(!argv || argc <= 0)) {
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
bu_dup_argv(int argc, const char *argv[])
{
    register int i;
    char **av;

    if (UNLIKELY(argc < 1))
	return (char **)0;

    av = (char **)bu_calloc((unsigned int)argc+1, sizeof(char *), "bu_copy_argv");
    for (i = 0; i < argc; ++i)
	av[i] = bu_strdup(argv[i]);
    av[i] = (char *)0;

    return av;
}


char **
bu_dupinsert_argv(int insert, int insertArgc, const char *insertArgv[], int argc, const char *argv[])
{
    register int i, j;
    int ac = argc + insertArgc + 1;
    char **av;

    /* Nothing to insert */
    if (insertArgc < 1)
	return bu_dup_argv(argc, argv);

    av = (char **)bu_calloc((unsigned int)ac, sizeof(char *), "bu_insert_argv");

    if (insert <= 0) {			    	/* prepend */
	for (i = 0; i < insertArgc; ++i)
	    av[i] = bu_strdup(insertArgv[i]);

	for (j = 0; j < argc; ++i, ++j)
	    av[i] = bu_strdup(argv[j]);
    } else if (argc <= insert) {		/* append */
	for (i = 0; i < argc; ++i)
	    av[i] = bu_strdup(argv[i]);

	for (j = 0; j < insertArgc; ++i, ++j)
	    av[i] = bu_strdup(insertArgv[j]);
    } else {					/* insert */
	for (i = 0; i < insert; ++i)
	    av[i] = bu_strdup(argv[i]);

	for (j = 0; j < insertArgc; ++i, ++j)
	    av[i] = bu_strdup(insertArgv[j]);

	for (j = insert; j < argc; ++i, ++j)
	    av[i] = bu_strdup(argv[j]);
    }

    av[i] = (char *)0;

    return av;
}


char **
bu_argv_from_path(const char *path, int *ac)
{
    char **av;
    char *begin;
    char *end;
    char *newstr;
    char *headpath;
    register int i;

    if (UNLIKELY(path == (char *)0 || path[0] == '\0'))
	return (char **)0;

    newstr = bu_strdup(path);

    /* skip leading /'s */
    i = 0;
    while (newstr[i] == '/')
	++i;

    if (UNLIKELY(newstr[i] == '\0')) {
	bu_free((genptr_t)newstr, "bu_argv_from_path");
	return (char **)0;
    }

    /* If we get here, there is alteast one path element */
    *ac = 1;
    headpath = &newstr[i];

    /* First count the number of '/' */
    begin = headpath;
    while ((end = strchr(begin, '/')) != (char *)0) {
	if (begin != end)
	    ++*ac;

	begin = end + 1;
    }
    av = (char **)bu_calloc((unsigned int)(*ac)+1, sizeof(char *), "bu_argv_from_path");

    begin = headpath;
    i = 0;
    while ((end = strchr(begin, '/')) != (char *)0) {
	if (begin != end) {
	    *end = '\0';
	    av[i++] = bu_strdup(begin);
	}

	begin = end + 1;
    }

    if (begin[0] != '\0') {
	av[i++] = bu_strdup(begin);
	av[i] = (char *)0;
    } else {
	av[i] = (char *)0;
	--*ac;
    }
    bu_free((void *)newstr, "bu_argv_from_path");

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
