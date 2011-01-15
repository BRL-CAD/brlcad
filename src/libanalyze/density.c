/*                    D E N S I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
#include <stdlib.h>
#include <string.h>

#include "analyze.h"
#include "bu.h"

int
parse_densities_buffer(char *buf, size_t len, struct density_entry *densities, struct bu_vls *result_str, int *num_densities)
{
    char *p, *q, *last;
    long idx;
    double density;

    buf[len] = '\0';
    last = &buf[len];

    p = buf;

    /* Skip initial whitespace */
    while (*p && (*p == '\t' || *p == ' ' || *p == '\n')) p++;

    /* Skip initial comments */
    while (*p == '#') {
	/* Skip comment */
	while (*p && *p != '\n') p++;
    }

    /* Skip whitespace */
    while (*p && (*p == '\t' || *p == ' ' || *p == '\n')) p++;

    while (*p) {
	/* Skip comments */
	if (*p == '#') {
	    /* Skip comment */
	    while (*p && *p != '\n') p++;

	    /* Skip whitespace */
	    while (*p && (*p == '\t' || *p == ' ' || *p == '\n')) p++;

	    continue;
	}

	idx = strtol(p, &q, 10);
	if (q == (char *)NULL) {
	    bu_vls_printf(result_str, "could not convert idx\n");
	    return ANALYZE_ERROR;
	}

	if (idx < 0) {
	    bu_vls_printf(result_str, "bad density index (%ld < 0)\n", idx);
	    return ANALYZE_ERROR;
	}

	density = strtod(q, &p);
	if (q == p) {
	    bu_vls_printf(result_str, "could not convert density\n");
	    return ANALYZE_ERROR;
	}

	if (density < 0.0) {
	    bu_vls_printf(result_str, "bad density (%lf < 0)\n", density);
	    return ANALYZE_ERROR;
	}

	/* Skip tabs and spaces */
	while (*p && (*p == '\t' || *p == ' ')) p++;
	if (!*p)
	    break;

        q = strchr(p, '\n');
	if (q)
	    *q++ = '\0';
	else
	    q = last;

	while (idx >= *num_densities) {
	    densities = bu_realloc(densities, sizeof(struct density_entry)*(*num_densities)*2,
				   "density entries");
	    *num_densities *= 2;
	}

	densities[idx].magic = DENSITY_MAGIC;
	/* since BRL-CAD does computation in mm, but the table is in
	 * grams / (cm^3) we convert the table on input
	 */
	densities[idx].grams_per_cu_mm = density / 1000.0;
	densities[idx].name = bu_strdup(p);

	p = q;

	/* Skip whitespace */
	while (*p && (*p == '\t' || *p == ' ' || *p == '\n')) p++;
    }

#ifdef PRINT_DENSITIES
    for (idx=0; idx < &num_densities; idx++)
	if (densities[idx].magic == DENSITY_MAGIC)
	    bu_vls_printf(&_ged_current_gedp->ged_result_str, "%4d %6g %s\n",
			  idx,
			  densities[idx].density,
			  densities[idx].name);
#endif

    return ANALYZE_OK;
}

