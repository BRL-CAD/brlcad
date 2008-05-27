/*                           G E D . H
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file ged.h
 *
 * Functions provided by the LIBGED geometry editing library.  These
 * routines are a procedural basis for the geometric editing
 * capabilities available in BRL-CAD.  The library is tightly coupled
 * to the LIBRT library for geometric representation and analysis.
 *
 */

#ifndef __GED_H__
#define __GED_H__

#include "raytrace.h"


__BEGIN_DECLS

#ifndef GED_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef GED_EXPORT_DLL
#      define GED_EXPORT __declspec(dllexport)
#    else
#      define GED_EXPORT __declspec(dllimport)
#    endif
#  else
#    define GED_EXPORT
#  endif
#endif

/* Check if the object is a combination */
#define	GED_CHECK_COMB(_wdbp,_dp,_ret) \
    if (((_dp)->d_flags & DIR_COMB) == 0) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "%s: not a combination", (_dp)->d_namep); \
	return (_ret); \
    }

/* Check if a database is open */
#define GED_CHECK_DATABASE_OPEN(_wdbp,_ret) \
    if ((_wdbp) == RT_WDB_NULL || (_wdbp)->dbip == DBI_NULL) { \
	if ((_wdbp) != RT_WDB_NULL) \
	    bu_vls_printf(&(_wdbp)->wdb_result_str, "A database is not open!"); \
	else \
	    bu_log("A database is not open!"); \
	return (_ret); \
    }

/* Lookup database object */
#define GED_CHECK_EXISTS(_wdbp,_name,_noisy,_ret) \
    if (db_lookup((_wdbp)->dbip, (_name), (_noisy)) != DIR_NULL) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "%s already exists", (_name)); \
	return (_ret); \
    }

/* Check if the database is read only */
#define	GED_CHECK_READ_ONLY(_wdbp,_ret) \
    if ((_wdbp)->dbip->dbi_read_only) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "Sorry, this database is READ-ONLY"); \
	return (_ret); \
    }

/* Check if the object is a region */
#define	GED_CHECK_REGION(_wdbp,_dp,_ret) \
    if (((_dp)->d_flags & DIR_REGION) == 0) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "%s: not a region", (_dp)->d_namep); \
	return (_ret); \
    }

#define GED_DB_DIRADD(_wdbp,_dp,_name,_laddr,_len,_flags,_ptr,_ret) \
    if (((_dp) = db_diradd((_wdbp)->dbip, (_name), (_laddr), (_len), (_flags), (_ptr))) == DIR_NULL) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "An error has occured while adding a new object to the database."); \
	return (_ret); \
    }

/* Lookup database object */
#define GED_DB_LOOKUP(_wdbp,_dp,_name,_noisy,_ret) \
    if (((_dp) = db_lookup((_wdbp)->dbip, (_name), (_noisy))) == DIR_NULL) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "%s: not found", (_name)); \
	return (_ret); \
    }

/* Get internal representation */
#define GED_DB_GET_INTERNAL(_wdbp,_intern,_dp,_mat,_resource,_ret) \
    if (rt_db_get_internal((_intern), (_dp), (_wdbp)->dbip, (_mat), (_resource)) < 0) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "Database read error, aborting"); \
	return (_ret); \
    }

/* Put internal representation */
#define GED_DB_PUT_INTERNAL(_wdbp,_dp,_intern,_resource,_ret) \
    if (rt_db_put_internal((_dp), (_wdbp)->dbip, (_intern), (_resource)) < 0) { \
	bu_vls_printf(&(_wdbp)->wdb_result_str, "Database write error, aborting"); \
	return (_ret); \
    }


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
};

#define GED_VIEW_OBJ_NULL ((struct view_obj *)0)
#define GED_RESULT_NULL ((void *)0)
#define GED_RESULT_FLAGS_HELP_BIT 0x1
#define GED_OK 0
#define GED_ERROR 1

/* loadable Tcl interface routines */

/* defined in tcl.c */
GED_EXPORT BU_EXTERN(int Ged_Init,
		    (Tcl_Interp *interp));

/* defined in wdb_comb_std.c */
GED_EXPORT BU_EXTERN(int wdb_comb_std_cmd,
		     (struct rt_wdb	*wdbp,
		      Tcl_Interp	*interp,
		      int		argc,
		      char 		**argv));

/* defined in wdb_obj.c */
GED_EXPORT BU_EXTERN(int Wdb_Init,
		    (Tcl_Interp *interp));

GED_EXPORT BU_EXTERN(int wdb_create_cmd,
		    (Tcl_Interp	*interp,
		     struct rt_wdb *wdbp,
		     const char	*oname));
GED_EXPORT BU_EXTERN(void wdb_deleteProc,
		    (ClientData clientData));
GED_EXPORT BU_EXTERN(int	wdb_get_tcl,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc, char **argv));
GED_EXPORT BU_EXTERN(int	wdb_init_obj,
		    (Tcl_Interp *interp,
		     struct rt_wdb *wdbp,
		     const char *oname));
GED_EXPORT BU_EXTERN(struct db_i	*wdb_prep_dbip,
		    (Tcl_Interp *interp,
		     const char *filename));
GED_EXPORT BU_EXTERN(int	wdb_bot_face_sort_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc, char **argv));
GED_EXPORT BU_EXTERN(int	wdb_bot_decimate_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_close_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_reopen_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_match_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_get_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_put_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_adjust_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_form_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_tops_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_rt_gettrees_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_dump_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_dbip_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_ls_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_list_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_lt_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_pathlist_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_pathsum_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_expand_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_kill_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_killall_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_killtree_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_copy_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_move_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_move_all_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_concat_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_dup_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_group_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_remove_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_region_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_comb_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_find_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_which_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_title_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_color_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_prcolor_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_tol_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_push_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_whatid_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_keep_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_cat_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_instance_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_observer_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_make_bb_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_units_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_hide_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_unhide_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_attr_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_summary_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_comb_std_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_nmg_collapse_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_nmg_simplify_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_shells_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_xpush_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_showmats_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_copyeval_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_version_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_binary_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_track_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int wdb_bot_smooth_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	wdb_importFg4Section_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));



/* defined in view_obj.c */
GED_EXPORT extern struct view_obj HeadViewObj;		/**< @brief  head of view object list */
GED_EXPORT BU_EXTERN(int Vo_Init,
		    (Tcl_Interp *interp));
GED_EXPORT BU_EXTERN(struct view_obj *vo_open_cmd,
		    (const char *oname));
GED_EXPORT BU_EXTERN(void vo_center,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     point_t center));
GED_EXPORT BU_EXTERN(int	vo_center_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(void vo_size,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t size));
GED_EXPORT BU_EXTERN(int	vo_size_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_invSize_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(void vo_mat_aet,
		    (struct view_obj *vop));
GED_EXPORT BU_EXTERN(int	vo_zoom,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t sf));
GED_EXPORT BU_EXTERN(int	vo_zoom_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
GED_EXPORT BU_EXTERN(int	vo_orientation_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_lookat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(void vo_setview,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     vect_t rvec));
GED_EXPORT BU_EXTERN(int	vo_setview_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_eye_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_eye_pos_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_pmat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_perspective_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(void vo_update,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int oflag));
GED_EXPORT BU_EXTERN(int	vo_aet_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_rmat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_model2view_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_pmodel2view_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_view2model_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_pov_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_units_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_base2local_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_local2base_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_rot,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     char coord,
		     char origin,
		     mat_t rmat,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_rot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_arot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_mrot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_tra,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     char coord,
		     vect_t tvec,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_tra_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_slew,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     vect_t svec));
GED_EXPORT BU_EXTERN(int	vo_slew_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
GED_EXPORT BU_EXTERN(int	vo_observer_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_coord_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_rotate_about_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_keypoint_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_vrot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_sca,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t sf,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_sca_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_viewDir_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_ae2dir_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	vo_dir2ae_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));


/**
 * Launch an editor on file.
 *
 * Runs $EDITOR on temp file, defaulting to various system-specific
 * editors otherwise if unset.
 *
 * Usage:
 *     editit file
 */
GED_EXPORT BU_EXTERN(int ged_editit, (const char *file));

/**
 * Allow editing of the matrix, etc., along an arc.
 *
 * Usage:
 *     arced a/b anim_cmd ...
 */
GED_EXPORT BU_EXTERN(int ged_arced, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Set combination color.
 *
 * Usage:
 *     comb_color combination R G B
 */
GED_EXPORT BU_EXTERN(int ged_comb_color, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Edit combination.
 *
 * Usage:
 *     edcomb combname Regionflag regionid air los GIFTmater
 */
GED_EXPORT BU_EXTERN(int ged_edcomb, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Edit combination materials.
 *
 * Command relies on rmater, editit, and wmater commands.
 *
 * Usage:
 *     edmater combination1 [combination2 ...]
 */
GED_EXPORT BU_EXTERN(int ged_edmater, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Set region ident codes.
 *
 * Usage:
 *     item region ident [air [material [los]]]
 */
GED_EXPORT BU_EXTERN(int ged_item, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Used to control logging.
 *
 * Usage:
 *     log {get|start|stop}
 */
GED_EXPORT BU_EXTERN(int ged_log, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Make a new primitive.
 *
 * Usage:
 *     make obj type
 */
GED_EXPORT BU_EXTERN(int ged_make, (struct rt_wdb *wdbp, int argc, char *argv[]));

/**
 * Make a unique object name.
 *
 * Usage:
 *     make_name template | -s [num]
 */
GED_EXPORT BU_EXTERN(int ged_make_name, (struct rt_wdb *wdbp, int argc, char *argv[]));

/**
 * Modify material information.
 *
 * Usage:
 *     mater region_name shader r g b inherit
 */
GED_EXPORT BU_EXTERN(int ged_mater, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Mirror the primitive or combination along the specified axis.
 *
 * Usage:
 *     mirror [-d dir] [-o origin] [-p scalar_pt] [-x] [-y] [-z] old new
 *
 */
GED_EXPORT BU_EXTERN(int ged_mirror, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Read material properties from a file.
 *
 * Usage:
 *     rmater file
 */
GED_EXPORT BU_EXTERN(int ged_rmater, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Simpler, command-line version of 'mater' command.
 *
 * Usage:
 *     shader combination shader_material [shader_argument(s)]
 */
GED_EXPORT BU_EXTERN(int ged_shader, (struct rt_wdb *wdbp, int argc, const char *argv[]));

/**
 * Write material properties to a file for specified combination(s).
 *
 * Usage:
 *     wmater file combination1 [combination2 ...]
 */
GED_EXPORT BU_EXTERN(int ged_wmater, (struct rt_wdb *wdbp, int argc, const char *argv[]));


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
