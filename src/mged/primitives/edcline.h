/*                      E D C L I N E . H
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
/** @file mged/edcline.h
 */

#ifndef EDCLINE_H
#define EDCLINE_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_CLINE_SCALE_H	77	/* scale height vector */
#define ECMD_CLINE_MOVE_H	78	/* move end of height vector */
#define ECMD_CLINE_SCALE_R	79	/* scale radius */
#define ECMD_CLINE_SCALE_T	80	/* scale thickness */

#define MENU_CLINE_SCALE_H	107
#define MENU_CLINE_MOVE_H	108
#define MENU_CLINE_SCALE_R	109
#define MENU_CLINE_SCALE_T	110

extern struct menu_item cline_menu[];

void
mged_cline_e_axes_pos(
	const struct rt_db_internal *ip,
       	const struct bn_tol *tol);

void ecmd_cline_scale_h(struct mged_state *s);
void ecmd_cline_scale_r(struct mged_state *s);
void ecmd_cline_scale_t(struct mged_state *s);
void ecmd_cline_move_h(struct mged_state *s);
void ecmd_cline_move_h_mousevec(struct mged_state *s, const vect_t mousevec);

#endif  /* EDCLINE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
