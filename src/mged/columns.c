/*                       C O L U M N S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @file mged/columns.c
 *
 * A set of routines for printing columns of data.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./mged.h"

static int col_count;		/* names listed on current line */
static int col_len;		/* length of previous name */
#define TERMINAL_WIDTH 80 /* XXX */
#define COLUMNS ((TERMINAL_WIDTH + NAMESIZE - 1) / NAMESIZE)

/*
 * V L S _ C O L _ I T E M
 */
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


/*
 */
void
vls_col_eol(struct bu_vls *str)
{
    if (col_count != 0)		/* partial line */
	bu_vls_putc(str, '\n');
    col_count = 0;
    col_len = 0;
}


/*
 * C M P D I R N A M E
 *
 * Given two pointers to pointers to directory entries, do a string compare
 * on the respective names and return that value.
 */
int
cmpdirname(const genptr_t a, const genptr_t b)
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
