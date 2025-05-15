/*                  E D I T _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file primitives/edit_private.h
  */

#ifndef EDIT_PRIVATE_H
#define EDIT_PRIVATE_H

#include "common.h"

#include "vmath.h"
#include "bn.h"
#include "rt/functab.h"
#include "rt/edit.h"

__BEGIN_DECLS

void
edit_abs_tra(
	struct rt_edit *s,
	vect_t view_pos
	);

const char *
edit_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_edit *s,
	const struct bn_tol *tol
	);

/* scale the solid uniformly about its vertex point */
int
edit_sscale(
	struct rt_edit *s,
	struct rt_db_internal *ip
	);

/* translate solid */
void
edit_stra(
	struct rt_edit *s,
	struct rt_db_internal *ip
	);

/* rot solid about vertex */
void
edit_srot(
	struct rt_edit *s,
	struct rt_db_internal *ip
	);

void
edit_sscale_xy(
	struct rt_edit *s,
	const vect_t mousevec
	);
void
edit_stra_xy(vect_t *pos_view,
	struct rt_edit *s,
	const vect_t mousevec
	);

void
edit_mscale_xy(
	struct rt_edit *s,
	const vect_t mousevec
	);
void
edit_tra_xy(vect_t *pos_view,
	struct rt_edit *s,
	const vect_t mousevec
	);

void
edit_mscale(
	struct rt_edit *s,
	const vect_t pos_model,
	const mat_t incr_mat
	);

void
edit_mtra(
	struct rt_edit *s,
	const vect_t pos_model
	);

int
edit_generic(
	struct rt_edit *s
	);

int
edit_generic_xy(
	struct rt_edit *s,
	const vect_t mousevec
	);

int
edit_menu_str(
	struct bu_vls *mstr,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *tol
	);


__END_DECLS

#endif  /* EDIT_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
