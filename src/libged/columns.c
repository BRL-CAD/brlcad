/*                       C O L U M N S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2016 United States Government as represented by
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
 */
/** @file libged/columns.c
 *
 * A set of routines for printing columns of data.
 *
 */

/* TODO: It should be lowered to libbu. */

#include "common.h"

#include <stdlib.h>
#include <string.h>


#include "vmath.h"
#include "raytrace.h"
#include "rt/db4.h"


static int col_count;		/* names listed on current line */
static int col_len;		/* length of previous name */
#define TERMINAL_WIDTH 80 /* XXX */
#define COLUMNS ((TERMINAL_WIDTH + NAMESIZE - 1) / NAMESIZE)

void
vls_col_item(
    struct bu_vls *str,
    const char *cp)
{
    /* Output newline if last column printed. */
    if (col_count >= COLUMNS || (col_len+NAMESIZE-1) >= TERMINAL_WIDTH) {
	/* line now full */
	bu_vls_putc(str, '\n');
	col_count = 0;
    } else if (col_count != 0) {
	/* Space over before starting new column */
	do {
	    bu_vls_putc(str, ' ');
	    col_len++;
	}  while ((col_len % NAMESIZE) != 0);
    }
    /* Output string and save length for next tab. */
    col_len = 0;
    while (*cp != '\0') {
	bu_vls_putc(str, *cp);
	++cp;
	++col_len;
    }
    col_count++;
}


void
vls_col_eol(struct bu_vls *str)
{
    if (col_count != 0)		/* partial line */
	bu_vls_putc(str, '\n');
    col_count = 0;
    col_len = 0;
}


/*
 * Given two pointers to pointers to directory entries, do a string compare
 * on the respective names and return that value.
 */
int
cmpdirname(const void *a, const void *b)
{
    struct directory **dp1, **dp2;

    dp1 = (struct directory **)a;
    dp2 = (struct directory **)b;
    return bu_strcmp((*dp1)->d_namep, (*dp2)->d_namep);
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
