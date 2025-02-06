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
#include "rt/rt_ecmds.h"

__BEGIN_DECLS

const char *
rt_solid_edit_generic_keypoint(
	point_t *pt,
	const char *keystr,
	const mat_t mat,
	struct rt_solid_edit *s,
	const struct bn_tol *tol
	);

/* scale the solid uniformly about its vertex point */
int
rt_solid_edit_generic_sscale(
	struct rt_solid_edit *s,
	struct rt_db_internal *ip
	);

/* translate solid */
void
rt_solid_edit_generic_strans(
	struct rt_solid_edit *s,
	struct rt_db_internal *ip
	);

/* rot solid about vertex */
void
rt_solid_edit_generic_srot(
	struct rt_solid_edit *s,
	struct rt_db_internal *ip
	);

void
rt_solid_edit_generic_sscale_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	);
void
rt_solid_edit_generic_strans_xy(vect_t *pos_view,
	struct rt_solid_edit *s,
	const vect_t mousevec
	);

int rt_solid_edit_generic_edit(struct rt_solid_edit *s, int edflag);

int
rt_solid_edit_generic_edit_xy(
	struct rt_solid_edit *s,
	const vect_t mousevec
	);

int
rt_solid_edit_generic_menu_str(
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
