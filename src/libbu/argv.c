/*                           V L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @addtogroup vls */
/** @{ */
/** @file vls.c
 *
 * @brief The variable length string package.
 *
 * The variable length string package.
 *
 * Assumption:  libc-provided sprintf() function is safe to use in parallel,
 * on parallel systems.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "bio.h"

#include "bu.h"


void
bu_free_argv(int argc, char *argv[])
{
    register int i;

    for (i = 0; i < argc; ++i)
	bu_free((void *)argv[i], "bu_free_argv");

    bu_free((void *)argv, "bu_free_argv");
}

char **
bu_copy_argv(int argc, const char *argv[])
{
    register int i;
    char **av;

    if (argc < 1)
	return (char **)0;

    av = (char **)bu_calloc(argc+1, sizeof(char *), "bu_copy_argv");
    for (i = 0; i < argc; ++i)
	av[i] = bu_strdup(argv[i]);
    av[i] = (char *)0;

    return av;
}

char **
bu_copyinsert_argv(int insert, int insertArgc, const char *insertArgv[], int argc, const char *argv[])
{
    register int i, j;
    int ac = argc + insertArgc + 1;
    char **av;

    /* Nothing to insert */
    if (insertArgc < 1)
	return bu_copy_argv(argc, argv);

    av = (char **)bu_calloc(ac, sizeof(char *), "bu_insert_argv");

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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
