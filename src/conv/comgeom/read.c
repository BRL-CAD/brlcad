/*                          R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file comgeom/read.c
 *
 * This module contains all of the routines necessary to read in
 * a COMGEOM input file, and put the information into internal form.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/str.h"
#include "bu/vls.h"

extern FILE *infp;

extern char name_it[16];		/* argv[3] */

void namecvt(int n, char **cp, int c);

int
get_line(char *cp, int buflen, char *title)
{
    int c = 0;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    c = bu_vls_gets(&str, infp);
    if (c > buflen)
	printf("get_line(x%lx, %d) input record overflows buffer for %s\n",
		(unsigned long)cp, buflen, title);
    return bu_strlcpy(cp, bu_vls_addr(&str), buflen);
}


int
getint(char *cp, int start, size_t len)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int result = 0;

    bu_vls_strncpy(&vls, cp+start, len);
    result = atoi(bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return result;
}


double
getdouble(char *cp, int start, size_t len)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    double result = 0.0;

    bu_vls_strncpy(&vls, cp+start, len);
    result = atof(bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return result;
}


void
namecvt(int n, char **cp, int c)
{
    char str[16];

    sprintf(str, "%c%d%.13s", (char)c, n, name_it);
    *cp = bu_strdup(str);
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
