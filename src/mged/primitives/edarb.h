/*                      E D A R B . H
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
/** @file mged/edarb.h
 */

#ifndef EDARB_H
#define EDARB_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define EARB		9	/* chgmodel.c, edarb.c */
#define PTARB		10	/* edarb.c */
#define ECMD_ARB_MAIN_MENU	11
#define ECMD_ARB_SPECIFIC_MENU	12
#define ECMD_ARB_MOVE_FACE	13
#define ECMD_ARB_SETUP_ROTFACE	14
#define ECMD_ARB_ROTATE_FACE	15

#define MENU_ARB_MV_EDGE	36
#define MENU_ARB_MV_FACE	37
#define MENU_ARB_ROT_FACE	38

extern struct menu_item cntrl_menu[];
extern struct menu_item *which_menu[];

extern fastf_t es_peqn[7][4];	/* ARBs defining plane equations */

void arb_mv_pnt_to(struct mged_state *s, const vect_t mousevec);
void edarb_mousevec(struct mged_state *s, const vect_t mousevec);
void edarb_move_face_mousevec(struct mged_state *s, const vect_t mousevec);

#endif  /* EDARB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
