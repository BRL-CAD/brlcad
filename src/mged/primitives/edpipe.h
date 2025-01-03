/*                         E D P I P E . H
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
/** @file mged/edpipe.h
 */

#ifndef EDPIPE_H
#define EDPIPE_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

extern struct wdb_pipe_pnt *es_pipe_pnt;

#define ECMD_PIPE_PICK		28	/* Pick pipe point */
#define ECMD_PIPE_SPLIT		29	/* Split a pipe segment into two */
#define ECMD_PIPE_PT_ADD	30	/* Add a pipe point to end of pipe */
#define ECMD_PIPE_PT_INS	31	/* Add a pipe point to start of pipe */
#define ECMD_PIPE_PT_DEL	32	/* Delete a pipe point */
#define ECMD_PIPE_PT_MOVE	33	/* Move a pipe point */

#define MENU_PIPE_SELECT	61
#define MENU_PIPE_NEXT_PT	62
#define MENU_PIPE_PREV_PT	63
#define MENU_PIPE_SPLIT		64
#define MENU_PIPE_PT_OD		65
#define MENU_PIPE_PT_ID		66
#define MENU_PIPE_SCALE_OD	67
#define MENU_PIPE_SCALE_ID	68
#define MENU_PIPE_ADD_PT	69
#define MENU_PIPE_INS_PT	70
#define MENU_PIPE_DEL_PT	71
#define MENU_PIPE_MOV_PT	72
#define MENU_PIPE_PT_RADIUS	73
#define MENU_PIPE_SCALE_RADIUS	74

void pipe_scale_od(struct mged_state *s, struct rt_db_internal *, fastf_t);
void pipe_scale_id(struct mged_state *s, struct rt_db_internal *, fastf_t);
void pipe_seg_scale_od(struct mged_state *s, struct wdb_pipe_pnt *, fastf_t);
void pipe_seg_scale_id(struct mged_state *s, struct wdb_pipe_pnt *, fastf_t);
void pipe_seg_scale_radius(struct mged_state *s, struct wdb_pipe_pnt *, fastf_t);
void pipe_scale_radius(struct mged_state *s, struct rt_db_internal *, fastf_t);
struct wdb_pipe_pnt *find_pipe_pnt_nearest_pnt(struct mged_state *s, const struct bu_list *, const point_t);
struct wdb_pipe_pnt *pipe_add_pnt(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);
void pipe_ins_pnt(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);
struct wdb_pipe_pnt *pipe_del_pnt(struct mged_state *s, struct wdb_pipe_pnt *);
void pipe_move_pnt(struct mged_state *s, struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);

void pipe_split_pnt(struct bu_list *, struct wdb_pipe_pnt *, point_t);
struct wdb_pipe_pnt *pipe_add_pnt(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);

void menu_pipe_pt_od(struct mged_state *s);
void menu_pipe_pt_id(struct mged_state *s);
void menu_pipe_pt_radius(struct mged_state *s);
void menu_pipe_scale_od(struct mged_state *s);
void menu_pipe_scale_id(struct mged_state *s);
void menu_pipe_scale_radius(struct mged_state *s);

#endif  /* EDPIPE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
