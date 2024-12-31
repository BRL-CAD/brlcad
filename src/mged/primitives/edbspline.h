/*                     E D B S P L I N E . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file mged/edbspline.h
 */

#ifndef EDBSPLINE_H
#define EDBSPLINE_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

int nurb_closest2d(int *surface, int *uval, int *vval, const struct rt_nurb_internal *spl, const point_t ref_pt  , const mat_t mat);

void bspline_init_sedit(struct mged_state *s);
void sedit_vpick(struct mged_state *s, point_t v_pos);

void
mged_bspline_labels(
	int *num_lines,
	point_t *lines,
	struct rt_point_labels *pl,
	int max_pl,
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *tol);

const char *
mged_bspline_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *tol);

struct menu_item *
mged_bspline_menu_item(const struct bn_tol *tol);

void ecmd_vtrans(struct mged_state *s);

#endif  /* EDBSPLINE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
