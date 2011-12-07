/*                           O B J . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file obj.h
 *
 * DEPRECATED object structures and functions provided by the LIBGED
 * geometry editing library that are related to viewable geometry
 * objects.  These structures and routines should not be used.
 *
 */

#ifndef __OBJ_H__
#define __OBJ_H__

#include "common.h"

#include "ged.h"
#include "fbserv_obj.h"

__BEGIN_DECLS


/**
 * V I E W _ O B J
 *
 * A view object maintains state for controlling a view.
 */
struct view_obj {
    struct bu_list 	l;
    struct bu_vls	vo_name;		/**< @brief  view object name/cmd */
    fastf_t		vo_scale;
    fastf_t		vo_size;		/**< @brief  2.0 * scale */
    fastf_t		vo_invSize;		/**< @brief  1.0 / size */
    fastf_t		vo_perspective;		/**< @brief  perspective angle */
    fastf_t		vo_local2base;		/**< @brief  scale local units to base units (i.e. mm) */
    fastf_t		vo_base2local;		/**< @brief  scale base units (i.e. mm) to local units */
    vect_t		vo_aet;
    vect_t		vo_eye_pos;		/**< @brief  eye position */
    vect_t		vo_keypoint;
    char		vo_coord;		/**< @brief  coordinate system */
    char		vo_rotate_about;	/**< @brief  indicates what point rotations are about */
    mat_t		vo_rotation;
    mat_t		vo_center;
    mat_t		vo_model2view;
    mat_t		vo_pmodel2view;
    mat_t		vo_view2model;
    mat_t		vo_pmat;		/**< @brief  perspective matrix */
    struct bu_observer	vo_observers;
    void 		(*vo_callback)();	/**< @brief  called in vo_update with vo_clientData and vop */
    genptr_t		vo_clientData;		/**< @brief  passed to vo_callback */
    int			vo_zclip;
    Tcl_Interp		*interp;
};
#define GED_VIEW_OBJ_NULL ((struct view_obj *)0)

struct ged_obj {
    struct ged		*go_gedp;
    struct ged_dm_view	go_head_views;
    struct bu_vls	go_name;
    struct bu_observer	go_observers;
    struct bu_vls	go_more_args_callback;
    struct bu_vls	go_rt_end_callback;
    struct bu_vls	*go_prim_label_list;
    int			go_prim_label_list_size;
    int			go_refresh_on;
    Tcl_Interp		*interp;
};
#define GED_OBJ_NULL ((struct ged_obj *)0)


/* DEPRECATED * DEPRECATED * DEPRECATED * DEPRECATED * DEPRECATED * DEPRECATED *
 * DEPRECATED *                                                   * DEPRECATED *
 * DEPRECATED *    Everything in this file should not be used.    * DEPRECATED *
 * DEPRECATED *                                                   * DEPRECATED *
 * DEPRECATED * DEPRECATED * DEPRECATED * DEPRECATED * DEPRECATED * DEPRECATED *
 */

GED_EXPORT extern int Wdb_Init(Tcl_Interp *interp);
GED_EXPORT extern int Vo_Init(Tcl_Interp *interp);

/* wdb_obj.c (used by g_diff) */
GED_EXPORT extern int wdb_get_tcl(void *clientData, int argc, const char *argv[]);
GED_EXPORT extern int wdb_init_obj(Tcl_Interp *interp, struct rt_wdb *wdbp, const char *oname);
GED_EXPORT extern int wdb_create_cmd(struct rt_wdb *wdbp, const char *oname);

/* wdb_obj.c (used by mged) */
GED_EXPORT extern int wdb_copy_cmd(struct rt_wdb *wdbp, int argc, const char *argv[]);
GED_EXPORT extern int wdb_stub_cmd(struct rt_wdb *wdbp, int argc, const char *argv[]);
GED_EXPORT extern int wdb_rt_gettrees_cmd(struct rt_wdb *wdbp, int argc, const char *argv[]);

/* go_refresh.c */
GED_EXPORT extern void go_refresh(struct ged_obj *gop,
				  struct ged_dm_view *gdvp);
GED_EXPORT extern void go_refresh_draw(struct ged_obj *gop,
				       struct ged_dm_view *gdvp);


__END_DECLS

#endif /* __GED_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

