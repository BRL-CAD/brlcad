/*                      E D A R S . H
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
/** @file mged/edars.h
 */

#ifndef EDARS_H
#define EDARS_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define ECMD_ARS_PICK		34	/* select an ARS point */
#define ECMD_ARS_NEXT_PT	35	/* select next ARS point in same curve */
#define ECMD_ARS_PREV_PT	36	/* select previous ARS point in same curve */
#define ECMD_ARS_NEXT_CRV	37	/* select corresponding ARS point in next curve */
#define ECMD_ARS_PREV_CRV	38	/* select corresponding ARS point in previous curve */
#define ECMD_ARS_MOVE_PT	39	/* translate an ARS point */
#define ECMD_ARS_DEL_CRV	40	/* delete an ARS curve */
#define ECMD_ARS_DEL_COL	41	/* delete all corresponding points in each curve (a column) */
#define ECMD_ARS_DUP_CRV	42	/* duplicate an ARS curve */
#define ECMD_ARS_DUP_COL	43	/* duplicate an ARS column */
#define ECMD_ARS_MOVE_CRV	44	/* translate an ARS curve */
#define ECMD_ARS_MOVE_COL	45	/* translate an ARS column */
#define ECMD_ARS_PICK_MENU	46	/* display the ARS pick menu */
#define ECMD_ARS_EDIT_MENU	47	/* display the ARS edit menu */

extern int es_ars_crv;	/* curve and column identifying selected ARS point */
extern int es_ars_col;
extern point_t es_pt;		/* coordinates of selected ARS point */

extern struct menu_item ars_menu[];
extern struct menu_item ars_pick_menu[];

void
ars_label_solid(
    struct mged_state *s,
    struct rt_point_labels pl[],
    int max_pl,
    const mat_t xform,
    struct rt_db_internal *ip);

void ecmd_ars_pick(struct mged_state *s);
void ecmd_ars_next_pt(struct mged_state *s);
void ecmd_ars_prev_pt(struct mged_state *s);
void ecmd_ars_next_crv(struct mged_state *s);
void ecmd_ars_prev_crv(struct mged_state *s);
void ecmd_ars_dup_crv(struct mged_state *s);
void ecmd_ars_dup_col(struct mged_state *s);
void ecmd_ars_del_crv(struct mged_state *s);
void ecmd_ars_del_col(struct mged_state *s);
void ecmd_ars_move_col(struct mged_state *s);
void ecmd_ars_move_crv(struct mged_state *s);
void ecmd_ars_move_pt(struct mged_state *s);


#endif  /* EDARS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
