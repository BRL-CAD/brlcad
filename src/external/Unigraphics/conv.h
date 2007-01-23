/*                          C O N V . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file conv.h
 *
 */

/* default indentation for a single level in the DAG */
#define LEVEL_INDENT 6
#define FLAG_SUPPRESS		0
#define FLAG_RESET_SUP		1
#define FLAG_FACETIZE		2
#define MAX_FLAGS 14 /* UG limitation */
extern int flags[MAX_FLAGS];

extern int feature_is_suppressible(tag_t feat,
				   int level,
				   double units_conv,
				   uf_list_p_t sup_l);

typedef struct ug_tol {
    double	dist;
    double	radius;
} ug_tol;

extern ug_tol ugtol;

extern int debug;
#define dprintf if (debug) printf

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
