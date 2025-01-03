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
