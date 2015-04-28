/*                        T C L C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file tclcad.h
 *
 * @brief
 *  Header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 *  This library contains convenience routines for preparing and
 *  initializing Tcl.
 *
 */

#ifndef TCLCAD_H
#define TCLCAD_H

#include "common.h"
#include "bu/cmd.h"
#include "tcl.h"
#include "dm.h"
#include "ged.h"

#include "fbserv_obj.h"

__BEGIN_DECLS

#ifndef TCLCAD_EXPORT
#  if defined(TCLCAD_DLL_EXPORTS) && defined(TCLCAD_DLL_IMPORTS)
#    error "Only TCLCAD_DLL_EXPORTS or TCLCAD_DLL_IMPORTS can be defined, not both."
#  elif defined(TCLCAD_DLL_EXPORTS)
#    define TCLCAD_EXPORT __declspec(dllexport)
#  elif defined(TCLCAD_DLL_IMPORTS)
#    define TCLCAD_EXPORT __declspec(dllimport)
#  else
#    define TCLCAD_EXPORT
#  endif
#endif

#define TCLCAD_IDLE_MODE 0
#define TCLCAD_ROTATE_MODE 1
#define TCLCAD_TRANSLATE_MODE 2
#define TCLCAD_SCALE_MODE 3
#define TCLCAD_CONSTRAINED_ROTATE_MODE 4
#define TCLCAD_CONSTRAINED_TRANSLATE_MODE 5
#define TCLCAD_OROTATE_MODE 6
#define TCLCAD_OSCALE_MODE 7
#define TCLCAD_OTRANSLATE_MODE 8
#define TCLCAD_MOVE_ARB_EDGE_MODE 9
#define TCLCAD_MOVE_ARB_FACE_MODE 10
#define TCLCAD_ROTATE_ARB_FACE_MODE 11
#define TCLCAD_PROTATE_MODE 12
#define TCLCAD_PSCALE_MODE 13
#define TCLCAD_PTRANSLATE_MODE 14
#define TCLCAD_POLY_CIRCLE_MODE 15
#define TCLCAD_POLY_CONTOUR_MODE 16
#define TCLCAD_POLY_ELLIPSE_MODE 17
#define TCLCAD_POLY_RECTANGLE_MODE 18
#define TCLCAD_POLY_SQUARE_MODE 19
#define TCLCAD_RECTANGLE_MODE 20
#define TCLCAD_MOVE_METABALL_POINT_MODE 21
#define TCLCAD_MOVE_PIPE_POINT_MODE 22
#define TCLCAD_MOVE_BOT_POINT_MODE 23
#define TCLCAD_MOVE_BOT_POINTS_MODE 24
#define TCLCAD_DATA_MOVE_OBJECT_MODE 25
#define TCLCAD_DATA_MOVE_POINT_MODE 26
#define TCLCAD_DATA_SCALE_MODE 27

#define TCLCAD_OBJ_FB_MODE_OFF 0
#define TCLCAD_OBJ_FB_MODE_UNDERLAY 1
#define TCLCAD_OBJ_FB_MODE_INTERLAY 2
#define TCLCAD_OBJ_FB_MODE_OVERLAY  3

struct ged_dm_view {
    struct bu_list		l;
    struct bu_vls		gdv_callback;
    struct bu_vls		gdv_edit_motion_delta_callback;
    struct bu_vls		gdv_name;
    struct bview		*gdv_view;
    dm				*gdv_dmp;
    struct fbserv_obj		gdv_fbs;
    struct ged_obj		*gdv_gop;
    int	   			gdv_hide_view;
};

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
    int			go_dlist_on;
    Tcl_Interp		*interp;
    struct bu_hash_tbl	*go_edited_paths;
};
#define GED_OBJ_NULL ((struct ged_obj *)0)


struct tclcad_obj {
    struct bu_list	l;
    struct ged_obj	*to_gop;
    Tcl_Interp		*to_interp;
};

#define TCLCAD_OBJ_NULL (struct tclcad_obj *)0

TCLCAD_EXPORT extern int tclcad_tk_setup(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_auto_path(Tcl_Interp *interp);
TCLCAD_EXPORT extern void tclcad_tcl_library(Tcl_Interp *interp);
TCLCAD_EXPORT extern int Tclcad_Init(Tcl_Interp *interp);

/* defined in tclcad_obj.c */
TCLCAD_EXPORT extern int Go_Init(Tcl_Interp *interp);
TCLCAD_EXPORT extern int to_open_tcl(ClientData UNUSED(clientData),
				     Tcl_Interp *interp,
				     int argc,
				     const char **argv);
TCLCAD_EXPORT extern struct application *to_rt_gettrees_application(struct ged *gedp,
								    int argc,
								    const char *argv[]);
TCLCAD_EXPORT extern void go_refresh(struct ged_obj *gop,
				  struct ged_dm_view *gdvp);
TCLCAD_EXPORT extern void go_refresh_draw(struct ged_obj *gop,
					  struct ged_dm_view *gdvp,
					  int restore_zbuffer);

/* defined in cmdhist_obj.c */
TCLCAD_EXPORT extern int Cho_Init(Tcl_Interp *interp);

/**
 * Open a command history object.
 *
 * USAGE:
 * ch_open name
 */
TCLCAD_EXPORT extern int cho_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv);


/**
 * This is a convenience routine for registering an array of commands
 * with a Tcl interpreter. Note - this is not intended for use by
 * commands with associated state (i.e. ClientData).  The interp is
 * passed to the bu_cmdtab function as clientdata instead of the
 * bu_cmdtab entry.
 *
 * @param interp - Tcl interpreter wherein to register the commands
 * @param cmds	 - commands and related function pointers
 */
TCLCAD_EXPORT extern void tclcad_register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds);

__END_DECLS

#endif /* TCLCAD_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
