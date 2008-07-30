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

#define GED_USE_RUN_RT 1

#if GED_USE_RUN_RT
/* Seems to be needed on windows if using ged_run_rt */
#include "bio.h"
#endif
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

#define GED_NULL (struct ged *)0
#define GED_DRAWABLE_NULL (struct ged_drawable *)0
#define GED_VIEW_NULL (struct ged_view *)0

/*XXX This macro is temporary */
#define GED_INIT(_gedp, _wdbp) { \
    bu_vls_init(&(_gedp)->ged_log); \
    bu_vls_init(&(_gedp)->ged_result_str); \
    (_gedp)->ged_wdbp = (_wdbp); \
}

#if 0
/*XXX This macro is temporary */
#define GED_INIT_FROM_WDBP(_gedp, _wdbp) { \
    bu_vls_init(&(_gedp)->ged_name); \
    bu_vls_init(&(_gedp)->ged_log); \
    bu_vls_init(&(_gedp)->ged_result_str); \
    bu_vls_init(&(_gedp)->ged_prestr); \
\
    (_gedp)->ged_type = (_wdbp)->type; \
    (_gedp)->ged_dbip = (_wdbp)->dbip; \
    bu_vls_vlscat(&(_gedp)->ged_name, &(_wdbp)->wdb_name); \
    (_gedp)->ged_initial_tree_state = (_wdbp)->wdb_initial_tree_state; \
    (_gedp)->ged_ttol = (_wdbp)->wdb_ttol; \
    (_gedp)->ged_tol = (_wdbp)->wdb_tol; \
    (_gedp)->ged_resp = (_wdbp)->wdb_resp; \
    (_gedp)->ged_result_flags = 0; \
    (_gedp)->ged_ncharadd = 0; \
    (_gedp)->ged_num_dups = 0; \
    (_gedp)->ged_item_default = (_wdbp)->wdb_item_default; \
    (_gedp)->ged_air_default = (_wdbp)->wdb_air_default; \
    (_gedp)->ged_mat_default = (_wdbp)->wdb_mat_default; \
    (_gedp)->ged_los_default = (_wdbp)->wdb_los_default; \
}
#endif

/* Check if the object is a combination */
#define	GED_CHECK_COMB(_gedp,_dp,_ret) \
    if (((_dp)->d_flags & DIR_COMB) == 0) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "%s: not a combination", (_dp)->d_namep); \
	return (_ret); \
    }

/* Check if a database is open */
#define GED_CHECK_DATABASE_OPEN(_gedp,_ret) \
    if ((_gedp) == GED_NULL || (_gedp)->ged_wdbp == RT_WDB_NULL || (_gedp)->ged_wdbp->dbip == DBI_NULL) { \
	if ((_gedp) != GED_NULL) \
	    bu_vls_printf(&(_gedp)->ged_result_str, "A database is not open!"); \
	else \
	    bu_log("A database is not open!"); \
	return (_ret); \
    }

/* Check if a drawable exists */
#define GED_CHECK_DRAWABLE(_gedp,_ret) \
    if (_gedp->ged_gdp == GED_DRAWABLE_NULL) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "A drawable does not exist!"); \
	return (_ret); \
    }

/* Check if a view exists */
#define GED_CHECK_VIEW(_gedp,_ret) \
    if (_gedp->ged_gvp == GED_VIEW_NULL) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "A view does not exist!"); \
	return (_ret); \
    }

/* Lookup database object */
#define GED_CHECK_EXISTS(_gedp,_name,_noisy,_ret) \
    if (db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy)) != DIR_NULL) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "%s already exists", (_name)); \
	return (_ret); \
    }

/* Check if the database is read only */
#define	GED_CHECK_READ_ONLY(_gedp,_ret) \
    if ((_gedp)->ged_wdbp->dbip->dbi_read_only) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "Sorry, this database is READ-ONLY"); \
	return (_ret); \
    }

/* Check if the object is a region */
#define	GED_CHECK_REGION(_gedp,_dp,_ret) \
    if (((_dp)->d_flags & DIR_REGION) == 0) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "%s: not a region", (_dp)->d_namep); \
	return (_ret); \
    }

#define GED_DB_DIRADD(_gedp,_dp,_name,_laddr,_len,_flags,_ptr,_ret) \
    if (((_dp) = db_diradd((_gedp)->ged_wdbp->dbip, (_name), (_laddr), (_len), (_flags), (_ptr))) == DIR_NULL) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "An error has occured while adding a new object to the database."); \
	return (_ret); \
    }

/* Lookup database object */
#define GED_DB_LOOKUP(_gedp,_dp,_name,_noisy,_ret) \
    if (((_dp) = db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy))) == DIR_NULL) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "%s: not found", (_name)); \
	return (_ret); \
    }

/* Get internal representation */
#define GED_DB_GET_INTERNAL(_gedp,_intern,_dp,_mat,_resource,_ret) \
    if (rt_db_get_internal((_intern), (_dp), (_gedp)->ged_wdbp->dbip, (_mat), (_resource)) < 0) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "Database read error, aborting"); \
	return (_ret); \
    }

/* Put internal representation */
#define GED_DB_PUT_INTERNAL(_gedp,_dp,_intern,_resource,_ret) \
    if (rt_db_put_internal((_dp), (_gedp)->ged_wdbp->dbip, (_intern), (_resource)) < 0) { \
	bu_vls_printf(&(_gedp)->ged_result_str, "Database write error, aborting"); \
	return (_ret); \
    }

#if GED_USE_RUN_RT
struct ged_run_rt {
    struct bu_list l;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE fd;
    HANDLE hProcess;
    DWORD pid;

#  ifdef TCL_OK
    Tcl_Channel chan;
#  else
    genptr_t chan;
#  endif
#else
    int fd;
    int pid;
#endif
    int aborted;
};
#endif

struct ged_qray_color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct ged_qray_fmt {
    char type;
    struct bu_vls fmt;
};

struct ged_drawable {
    struct bu_list		l;
    struct bu_list		gd_headSolid;		/**< @brief  head of solid list */
    struct bu_list		gd_headVDraw;		/**< @brief  head of vdraw list */
    struct vd_curve		*gd_currVHead;		/**< @brief  current vdraw head */
    struct solid		*gd_freeSolids;		/**< @brief  ptr to head of free solid list */

    char			*gd_rt_cmd[RT_MAXARGS];
    int				gd_rt_cmd_len;
#if GED_USE_RUN_RT
    struct ged_run_rt		gd_headRunRt;		/**< @brief  head of forked rt processes */
#endif
    void			(*gd_rtCmdNotify)();	/**< @brief  function called when rt command completes */

    int				gd_uplotOutputMode;	/**< @brief  output mode for unix plots */

    /* qray state */
    struct bu_vls		gd_qray_basename;	/**< @brief  basename of query ray vlist */
    struct bu_vls		gd_qray_script;		/**< @brief  query ray script */
    char			gd_qray_effects;	/**< @brief  t for text, g for graphics or b for both */
    int				gd_qray_cmd_echo;	/**< @brief  0 - don't echo command, 1 - echo command */
    struct ged_qray_fmt		*gd_qray_fmts;
    struct ged_qray_color	gd_qray_odd_color;
    struct ged_qray_color	gd_qray_even_color;
    struct ged_qray_color	gd_qray_void_color;
    struct ged_qray_color	gd_qray_overlap_color;
    int				gd_shaded_mode;		/**< @brief  1 - draw bots shaded by default */
#if 0
    struct bu_observer		gd_observers;
#endif
};


struct ged_view {
    struct bu_list 	l;
    fastf_t		gv_scale;
    fastf_t		gv_size;		/**< @brief  2.0 * scale */
    fastf_t		gv_isize;		/**< @brief  1.0 / size */
    fastf_t		gv_perspective;		/**< @brief  perspective angle */
    vect_t		gv_aet;
    vect_t		gv_eye_pos;		/**< @brief  eye position */
    vect_t		gv_keypoint;
    char		gv_coord;		/**< @brief  coordinate system */
    char		gv_rotate_about;	/**< @brief  indicates what point rotations are about */
    mat_t		gv_rotation;
    mat_t		gv_center;
    mat_t		gv_model2view;
    mat_t		gv_pmodel2view;
    mat_t		gv_view2model;
    mat_t		gv_pmat;		/**< @brief  perspective matrix */
#if 0
    struct bu_observer	gv_observers;
    void 		(*gv_callback)();	/**< @brief  called in vo_update with gv_clientData and gvp */
    genptr_t		gv_clientData;		/**< @brief  passed to gv_callback */
#endif
    int			gv_zclip;
    fastf_t		gv_prevMouseX;
    fastf_t		gv_prevMouseY;
    fastf_t		gv_minMouseDelta;
    fastf_t		gv_maxMouseDelta;
    fastf_t		gv_rscale;
    fastf_t		gv_sscale;
};


struct ged {
    struct bu_list		l;
    struct rt_wdb		*ged_wdbp;

    /* for catching log messages */
    struct bu_vls		ged_log;

    /* for setting results */
    void			*ged_result;
    struct bu_vls		ged_result_str;
    unsigned int		ged_result_flags;

#if 1
    struct ged_drawable		*ged_gdp;
    struct ged_view		*ged_gvp;
#else
    struct ged_drawable		*ged_head_drawables;
    struct ged_view		*ged_head_views;
#endif
    void			(*ged_output_handler)();	/**< @brief  function for handling output */
    char			*ged_output_script;		/**< @brief  script for use by the outputHandler */
#if 0
    struct bu_observer		ged_observers;
#endif
};


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

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
#define GED_FUNC_PTR_NULL (ged_func_ptr)0

#define GED_VIEW_OBJ_NULL ((struct view_obj *)0)
#define GED_RESULT_NULL ((void *)0)
#define GED_RESULT_FLAGS_HELP_BIT 0x1

/* defined in ged.c */
GED_EXPORT BU_EXTERN(struct ged *ged_open,
		     (const char *dbtype,
		      const char *filename));
GED_EXPORT BU_EXTERN(void ged_close,
		     (struct ged *gedp));
GED_EXPORT BU_EXTERN(void ged_view_init,
		     (struct ged_view *gvp));

/* defined in wdb_comb_std.c */
GED_EXPORT BU_EXTERN(int wdb_comb_std_cmd,
		     (struct rt_wdb	*gedp,
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
		     int argc, char *argv[]));
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
		     int argc, char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_bot_decimate_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_close_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_reopen_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_match_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_get_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_put_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_adjust_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_form_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_tops_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_rt_gettrees_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_dump_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_dbip_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_ls_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_list_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_lt_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_pathlist_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_pathsum_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_expand_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_kill_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_killall_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_killtree_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_copy_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_move_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_move_all_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_concat_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_dup_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_group_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_remove_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_region_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_comb_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_find_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_which_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_title_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_color_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_prcolor_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_tol_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_push_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_whatid_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_keep_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_cat_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_instance_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_observer_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_make_bb_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_units_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_hide_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_unhide_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_attr_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_summary_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_comb_std_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_nmg_collapse_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_nmg_simplify_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_shells_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_xpush_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_showmats_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_copyeval_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_version_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_binary_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_track_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int wdb_bot_smooth_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_importFg4Section_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));



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
		     char *argv[]));
GED_EXPORT BU_EXTERN(void vo_size,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t size));
GED_EXPORT BU_EXTERN(int	vo_size_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_invSize_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(void vo_mat_aet,
		    (struct view_obj *vop));
GED_EXPORT BU_EXTERN(int	vo_zoom,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t sf));
GED_EXPORT BU_EXTERN(int	vo_zoom_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_orientation_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_lookat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(void vo_setview,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     vect_t rvec));
GED_EXPORT BU_EXTERN(int	vo_setview_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_eye_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_eye_pos_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_pmat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_perspective_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(void vo_update,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int oflag));
GED_EXPORT BU_EXTERN(int	vo_aet_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_rmat_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_model2view_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_pmodel2view_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_view2model_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_pov_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_units_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_base2local_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_local2base_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
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
		     int argc, char *argv[],
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_arot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[],
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_mrot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[],
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
		     int argc, char *argv[],
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_slew,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     vect_t svec));
GED_EXPORT BU_EXTERN(int	vo_slew_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_observer_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_coord_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_rotate_about_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_keypoint_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_vrot_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_sca,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     fastf_t sf,
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_sca_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[],
		     int (*func)()));
GED_EXPORT BU_EXTERN(int	vo_viewDir_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_ae2dir_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));
GED_EXPORT BU_EXTERN(int	vo_dir2ae_cmd,
		    (struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char *argv[]));


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
 * Convert az/el to a direction vector.
 *
 * Usage:
 *     ae2dir [-i] az el
 */
GED_EXPORT BU_EXTERN(int ged_ae2dir, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get or set the azimuth, elevation and twist.
 *
 * Usage:
 *     ae [[-i] az el [tw]]
 */
GED_EXPORT BU_EXTERN(int ged_aet, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Adjust object's attribute(s)
 *
 * Usage:
 *     adjust object attr value ?attr value?
 */
GED_EXPORT BU_EXTERN(int ged_adjust, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Allow editing of the matrix, etc., along an arc.
 *
 * Usage:
 *     arced a/b anim_cmd ...
 */
GED_EXPORT BU_EXTERN(int ged_arced, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set, get, show, remove or append to attribute values for the specified object.
 * The arguments for "set" and "append" subcommands are attribute name/value pairs.
 * The arguments for "get", "rm", and "show" subcommands are attribute names.
 * The "set" subcommand sets the specified attributes for the object.
 * The "append" subcommand appends the provided value to an existing attribute,
 * or creates a new attribute if it does not already exist.
 * The "get" subcommand retrieves and displays the specified attributes.
 * The "rm" subcommand deletes the specified attributes.
 * The "show" subcommand does a "get" and displays the results in a user readable format.
 *
 * Usage:
 *     attr set|get|show|rm|append} object [args]
 */
GED_EXPORT BU_EXTERN(int ged_attr, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate angle degrees about the specified axis
 *
 * Usage:
 *     arot x y z angle
 */
GED_EXPORT BU_EXTERN(int ged_arot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Auto-adjust the view so that all displayed geometry is in view
 *
 * Usage:
 *     autoview
 */
GED_EXPORT BU_EXTERN(int ged_autoview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Manipulate opaque objects.
 * Must specify one of -i (for creating or adjusting objects (input))
 * or -o for extracting objects (output).
 * If the major type is "u" the minor type must be one of:
 *   "f" -> float
 *   "d" -> double
 *   "c" -> char (8 bit)
 *   "s" -> short (16 bit)
 *   "i" -> int (32 bit)
 *   "l" -> long (64 bit)
 *   "C" -> unsigned char (8 bit)
 *   "S" -> unsigned short (16 bit)
 *   "I" -> unsigned int (32 bit)
 *   "L" -> unsigned long (64 bit)
 * For input, source is a file name and dest is an object name.
 * For output source is an object name and dest is a file name.
 * Only uniform array binary objects (major_type=u) are currently supported}}
 * 
 * Usage:
 *     binary {-i major_type minor_type | -o} dest source
 */
GED_EXPORT BU_EXTERN(int ged_binary, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase all currently displayed geometry and draw the specified object(s)
 *
 * Usage:
 *     blast object(s)
 */
GED_EXPORT BU_EXTERN(int ged_blast, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Uses edge decimation to reduce the number of triangles in the
 * specified BOT while keeping within the specified constraints.
 *
 * Usage:
 *     bot_decimate -c maximum_chord_error -n maximum_normal_error -e minimum_edge_length new_bot_name current_bot_name
 */
GED_EXPORT BU_EXTERN(int ged_bot_decimate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Sort the facelist of BOT solids to optimize ray trace performance
 * for a particular number of triangles per raytrace piece.
 *
 * Usage:
 *     bot_face_sort triangles_per_piece bot_solid1 [bot_solid2 bot_solid3 ...]
 */
GED_EXPORT BU_EXTERN(int ged_bot_face_sort, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Calculate vertex normals for the BOT primitive
 *
 * Usage:
 *     bot_smoooth [-t ntol] new_bot old_bot
 */
GED_EXPORT BU_EXTERN(int ged_bot_smooth, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List attributes (brief).
 *
 * Usage:
 *     cat <objects>
 */
GED_EXPORT BU_EXTERN(int ged_cat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get or set the view center.
 *
 * Usage:
 *     center ["x y z"]
 */
GED_EXPORT BU_EXTERN(int ged_center, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make color entry.
 *
 * Usage:
 *     color low high r g b
 */
GED_EXPORT BU_EXTERN(int ged_color, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set combination color.
 *
 * Usage:
 *     comb_color combination R G B
 */
GED_EXPORT BU_EXTERN(int ged_comb_color, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or extend combination w/booleans.
 *
 * Usage:
 *     comb comb_name <operation primitive>
 */
GED_EXPORT BU_EXTERN(int ged_comb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or extend a combination using standard notation.
 *
 * Usage:
 *     c [-cr] comb_name <boolean_expr>
 */
GED_EXPORT BU_EXTERN(int ged_comb_std, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Import a database into the current database using an auto-incrementing or custom affix
 *
 * Usage:
 *     concat [-s|-p] file.g [suffix|prefix]
 */
GED_EXPORT BU_EXTERN(int ged_concat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Copy a database object
 *
 * Usage:
 *     copy from to
 */
GED_EXPORT BU_EXTERN(int ged_copy, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Copy an 'evaluated' path solid
 *
 * Usage:
 *     copyeval new_prim path_to_old_prim
 */
GED_EXPORT BU_EXTERN(int ged_copyeval, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get dbip
 *
 * Usage:
 *     dbip
 */
GED_EXPORT BU_EXTERN(int ged_dbip, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert a direction vector to az/el.
 *
 * Usage:
 *     dir2ae [-i] x y z
 */
GED_EXPORT BU_EXTERN(int ged_dir2ae, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Prepare object(s) for display
 *
 * Usage:
 *     draw [-A -o -C#/#/# -s] <objects | attribute name/value pairs>
 */
GED_EXPORT BU_EXTERN(int ged_draw, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Dump a full copy of the database into file.g
 *
 * Usage:
 *     dump file.g
 */
GED_EXPORT BU_EXTERN(int ged_dump, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Check for duplicate names in file
 *
 * Usage:
 *     dup file.g prefix
 */
GED_EXPORT BU_EXTERN(int ged_dup, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Prepare object(s) for display
 *
 * Usage:
 *     E [-C#/#/# -s] objects(s)
 */
GED_EXPORT BU_EXTERN(int ged_E, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit combination.
 *
 * Usage:
 *     edcomb combname Regionflag regionid air los GIFTmater
 */
GED_EXPORT BU_EXTERN(int ged_edcomb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit combination materials.
 *
 * Command relies on rmater, editit, and wmater commands.
 *
 * Usage:
 *     edmater combination1 [combination2 ...]
 */
GED_EXPORT BU_EXTERN(int ged_edmater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase objects from the display.
 *
 * Usage:
 *     erase objects(s)
 */
GED_EXPORT BU_EXTERN(int ged_erase, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase all occurrences of objects from the display.
 *
 * Usage:
 *     erase_all objects(s)
 */
GED_EXPORT BU_EXTERN(int ged_erase_all, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Evaluate objects via NMG tessellation
 *
 * Usage:
 *     ev [-dfnstuvwT] [-P #] <objects>
 */
GED_EXPORT BU_EXTERN(int ged_ev, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the eye point
 *
 * Usage:
 *     eye [x y z]
 */
GED_EXPORT BU_EXTERN(int ged_eye, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the eye position
 *
 * Usage:
 *     eye_pos [x y z]
 */
GED_EXPORT BU_EXTERN(int ged_eye_pos, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Globs expression against database objects
 *
 * Usage:
 *     expand expression
 */
GED_EXPORT BU_EXTERN(int ged_expand, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Facetize the specified objects
 *
 * Usage:
 *     facetize new_obj old_obj [old_obj2 old_obj3 ...]
 */
GED_EXPORT BU_EXTERN(int ged_facetize, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Find combinations that reference object
 *
 * Usage:
 *     find <objects>
 */
GED_EXPORT BU_EXTERN(int ged_find, (struct ged *gedp, int argc, const char *argv[]));

/**
 * returns form for objects of type "type"
 *
 * Usage:
 *     form type
 */
GED_EXPORT BU_EXTERN(int ged_form, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get object attributes
 *
 * Usage:
 *     get object ?attr? 
 */
GED_EXPORT BU_EXTERN(int ged_get, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get view size and center such that all displayed solids would be in view
 *
 * Usage:
 *     get_autoview
 */
GED_EXPORT BU_EXTERN(int ged_get_autoview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the viewsize, orientation and eye point.
 *
 * Usage:
 *     get_eyemodel
 */
GED_EXPORT BU_EXTERN(int ged_get_eyemodel, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the object's type
 *
 * Usage:
 *     get_type object
 */
GED_EXPORT BU_EXTERN(int ged_get_type, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or append objects to a group
 *
 * Usage:
 *     group object(s)
 */
GED_EXPORT BU_EXTERN(int ged_group, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the "hidden" flag for the specified objects so they do not appear in an "ls" command output
 *
 * Usage:
 *     hide <objects>
 */
GED_EXPORT BU_EXTERN(int ged_hide, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns how an object is being displayed
 *
 * Usage:
 *     how [-b] obj
 */
GED_EXPORT BU_EXTERN(int ged_how, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Illuminate/highlight database object.
 *
 * Usage:
 *     illum [-n] obj
 */
GED_EXPORT BU_EXTERN(int ged_illum, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Add instance of obj to comb
 *
 * Usage:
 *     instance obj comb [op]
 */
GED_EXPORT BU_EXTERN(int ged_instance, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Makes a bot object out of the specified section.
 *
 * Usage:
 *     importFg4Section obj section
 */
GED_EXPORT BU_EXTERN(int ged_importFg4Section, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns the inverse view size.
 *
 * Usage:
 *     isize
 */
GED_EXPORT BU_EXTERN(int ged_isize, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set region ident codes.
 *
 * Usage:
 *     item region ident [air [material [los]]]
 */
GED_EXPORT BU_EXTERN(int ged_item, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Save/keep the specified objects in the specified file
 *
 * Usage:
 *     keep file.g object(s)
 */
GED_EXPORT BU_EXTERN(int ged_keep, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the keypoint
 *
 * Usage:
 *     keypoint [x y z]
 */
GED_EXPORT BU_EXTERN(int ged_keypoint, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill/delete the specified objects from the database
 *
 * Usage:
 *     kill object(s)
 */
GED_EXPORT BU_EXTERN(int ged_kill, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill/delete the specified objects from the database, removing all references
 *
 * Usage:
 *     killall object(s)
 */
GED_EXPORT BU_EXTERN(int ged_killall, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill all references to the specified object(s).
 *
 * Usage:
 *     killrefs object(s)
 */
GED_EXPORT BU_EXTERN(int ged_killrefs, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill all paths belonging to objects
 *
 * Usage:
 *     killtree object(s)
 */
GED_EXPORT BU_EXTERN(int ged_killtree, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List object information, verbose.
 *
 * Usage:
 *     l [-r] <objects>
 */
GED_EXPORT BU_EXTERN(int ged_list, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Used to control logging.
 *
 * Usage:
 *     log {get|start|stop}
 */
GED_EXPORT BU_EXTERN(int ged_log, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the look-at point
 *
 * Usage:
 *     lookat x y z
 */
GED_EXPORT BU_EXTERN(int ged_lookat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List the objects in this database
 *
 * Usage:
 *     ls [-A name/value pairs] OR [-acrslop] object(s)
 */
GED_EXPORT BU_EXTERN(int ged_ls, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List object's tree as a tcl list of {operator object} pairs
 *
 * Usage:
 *     lt object
 */
GED_EXPORT BU_EXTERN(int ged_lt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert the specified model point to a view point.
 *
 * Usage:
 *     m2v_point x y z
 */
GED_EXPORT BU_EXTERN(int ged_m2v_point, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make a new primitive.
 *
 * Usage:
 *     make obj type
 */
GED_EXPORT BU_EXTERN(int ged_make, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make a bounding box (rpp) around the specified objects
 *
 * Usage:
 *     make_bb bbname object(s)
 */
GED_EXPORT BU_EXTERN(int ged_make_bb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make a unique object name.
 *
 * Usage:
 *     make_name template | -s [num]
 */
GED_EXPORT BU_EXTERN(int ged_make_name, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Globs expression against database objects, does not return tokens that match nothing
 *
 * Usage:
 *     match expression
 */
GED_EXPORT BU_EXTERN(int ged_match, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Modify material information.
 *
 * Usage:
 *     mater region_name shader r g b inherit
 */
GED_EXPORT BU_EXTERN(int ged_mater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Mirror the primitive or combination along the specified axis.
 *
 * Usage:
 *     mirror [-d dir] [-o origin] [-p scalar_pt] [-x] [-y] [-z] old new
 *
 */
GED_EXPORT BU_EXTERN(int ged_mirror, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the model to view matrix
 *
 * Usage:
 *     model2view
 */
GED_EXPORT BU_EXTERN(int ged_model2view, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move an arb's edge through point
 *
 * Usage:
 *     move_arb_edge arb edge pt
 */
GED_EXPORT BU_EXTERN(int ged_move_arb_edge, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move/rename a database object
 *
 * Usage:
 *     mv from to
 */
GED_EXPORT BU_EXTERN(int ged_move, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move/rename all occurrences object
 *
 * Usage:
 *     mvall from to
 */
GED_EXPORT BU_EXTERN(int ged_move_all, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move an arb's face through point
 *
 * Usage:
 *     move_arb_face arb face pt
 */
GED_EXPORT BU_EXTERN(int ged_move_arb_face, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the view. Note - x, y and z are rotations in model coordinates.
 *
 * Usage:
 *     mrot x y z
 */
GED_EXPORT BU_EXTERN(int ged_mrot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Trace a single ray from the current view.
 *
 * Usage:
 *     nirt options [x y z]
 */
GED_EXPORT BU_EXTERN(int ged_nirt, (struct ged *gedp, int argc, const char *argv[]));
GED_EXPORT BU_EXTERN(int ged_vnirt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Decimate NMG primitive via edge collapse
 *
 * Usage:
 *     nmg_collapse nmg_prim new_prim max_err_dist [min_angle]
 */
GED_EXPORT BU_EXTERN(int ged_nmg_collapse, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Simplify the NMG primitive, if possible
 *
 * Usage:
 *     nmg_simplify [arb|tgc|ell|poly] new_prim nmg_prim
 */
GED_EXPORT BU_EXTERN(int ged_nmg_simplify, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get object center.
 *
 * Usage:
 *     ocenter obj [x y z]
 */
GED_EXPORT BU_EXTERN(int ged_ocenter, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Open a database
 *
 * Usage:
 *     open [filename]
 */
GED_EXPORT BU_EXTERN(int ged_reopen, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the view orientation using a quaternion.
 *
 * Usage:
 *     orient quat
 */
GED_EXPORT BU_EXTERN(int ged_orient, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate obj about the keypoint by 
 *
 * Usage:
 *     orotate obj rX rY rZ [kX kY kZ]
 */
GED_EXPORT BU_EXTERN(int ged_orotate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Scale obj about the keypoint by sf.
 *
 * Usage:
 *     oscale obj sf [kX kY kZ]
 */
GED_EXPORT BU_EXTERN(int ged_oscale, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Translate obj by dx dy dz.
 *
 * Usage:
 *     otranslate obj dx dy dz
 */
GED_EXPORT BU_EXTERN(int ged_otranslate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Overlay the specified 2D/3D UNIX plot file
 *
 * Usage:
 *     overlay file.pl [name]
 */
GED_EXPORT BU_EXTERN(int ged_overlay, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List all paths from name(s) to leaves
 *
 * Usage:
 *     pathlist name(s)
 */
GED_EXPORT BU_EXTERN(int ged_pathlist, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Lists all paths matching the input path
 *
 * Usage:
 *     paths pattern
 */
GED_EXPORT BU_EXTERN(int ged_pathsum, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the perspective angle.
 *
 * Usage:
 *     perspective [angle]
 */
GED_EXPORT BU_EXTERN(int ged_perspective, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the perspective matrix.
 *
 * Usage:
 *     pmat [mat]
 */
GED_EXPORT BU_EXTERN(int ged_pmat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the pmodel2view matrix.
 *
 * Usage:
 *     pmodel2view
 */
GED_EXPORT BU_EXTERN(int ged_pmodel2view, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set point of view
 *
 * Usage:
 *     pov center quat scale eye_pos perspective
 */
GED_EXPORT BU_EXTERN(int ged_pov, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Print color table
 *
 * Usage:
 *     prcolor
 */
GED_EXPORT BU_EXTERN(int ged_prcolor, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Push objects' path transformations to  primitives
 *
 * Usage:
 *     push object(s)
 */
GED_EXPORT BU_EXTERN(int ged_push, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a database object
 *
 * Usage:
 *     put object type attrs
 */
GED_EXPORT BU_EXTERN(int ged_put, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set query_ray attributes
 *
 * Usage:
 *     qray subcommand args
 */
GED_EXPORT BU_EXTERN(int ged_qray, (struct ged *gedp, int argc, const char *argv[]));

GED_EXPORT BU_EXTERN(void ged_init_qray,
		    (struct ged_drawable *gdp));
GED_EXPORT BU_EXTERN(void ged_free_qray,
		    (struct ged_drawable *gdp));

/**
 * Create or append objects to a region
 *
 * Usage:
 *     region object(s)
 */
GED_EXPORT BU_EXTERN(int ged_region, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns a list of id to region name mappings for the entire database.
 *
 * Usage:
 *     rmap
 */
GED_EXPORT BU_EXTERN(int ged_rmap, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Remove members from a combination
 *
 * Usage:
 *     remove object(s)
 */
GED_EXPORT BU_EXTERN(int ged_remove, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns the solid table & vector list as a string
 *
 * Usage:
 *     report [lvl]
 */
GED_EXPORT BU_EXTERN(int ged_report, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the rotation matrix.
 *
 * Usage:
 *     rmat [mat]
 */
GED_EXPORT BU_EXTERN(int ged_rmat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Read material properties from a file.
 *
 * Usage:
 *     rmater file
 */
GED_EXPORT BU_EXTERN(int ged_rmater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the view.
 *
 * Usage:
 *     rot [-m|-v] x y z
 */
GED_EXPORT BU_EXTERN(int ged_rot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the rotate_about point.
 *
 * Usage:
 *     rotate_about [e|k|m|v]
 */
GED_EXPORT BU_EXTERN(int ged_rotate_about, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate an arb's face through point
 *
 * Usage:
 *     rotate_arb_face arb face pt
 */
GED_EXPORT BU_EXTERN(int ged_rotate_arb_face, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the point.
 *
 * Usage:
 *     rot_point x y z
 */
GED_EXPORT BU_EXTERN(int ged_rot_point, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Run the raytracing application.
 *
 * Usage:
 *     rt [args]
 */
GED_EXPORT BU_EXTERN(int ged_rt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Abort the current raytracing processes.
 *
 * Usage:
 *     rtabort
 */
GED_EXPORT BU_EXTERN(int ged_rtabort, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Check for overlaps in the current view.
 *
 * Usage:
 *     rtcheck [args]
 */
GED_EXPORT BU_EXTERN(int ged_rtcheck, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Scale the view.
 *
 * Usage:
 *     sca sf
 */
GED_EXPORT BU_EXTERN(int ged_scale, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set the output handler script
 *
 * Usage:
 *     set_output_script [script]
 */
GED_EXPORT BU_EXTERN(int ged_set_output_script, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set the unix plot output mode
 *
 * Usage:
 *     set_uplotOutputMode [binary|text]
 */
GED_EXPORT BU_EXTERN(int ged_set_uplotOutputMode, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the transparency of the specified object
 *
 * Usage:
 *     set_transparency obj tr
 */
GED_EXPORT BU_EXTERN(int ged_set_transparency, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the view orientation given the angles x, y and z in degrees.
 *
 * Usage:
 *     setview x y z
 */
GED_EXPORT BU_EXTERN(int ged_setview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the shaded mode.
 *
 * Usage:
 *     shaded_mode [0|1|2]
 */
GED_EXPORT BU_EXTERN(int ged_shaded_mode, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Simpler, command-line version of 'mater' command.
 *
 * Usage:
 *     shader combination shader_material [shader_argument(s)]
 */
GED_EXPORT BU_EXTERN(int ged_shader, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Breaks the NMG model into seperate shells
 *
 * Usage:
 *     shells nmg_model
 */
GED_EXPORT BU_EXTERN(int ged_shells, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Show the matrix transformations along path
 *
 * Usage:
 *     showmats path
 */
GED_EXPORT BU_EXTERN(int ged_showmats, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get or set the view size.
 *
 * Usage:
 *     size [s]
 */
GED_EXPORT BU_EXTERN(int ged_size, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Slew the view
 *
 * Usage:
 *     slew x y [z]
 */
GED_EXPORT BU_EXTERN(int ged_slew, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Count/list primitives/regions/groups
 *
 * Usage:
 *     summary [p r g]
 */
GED_EXPORT BU_EXTERN(int ged_summary, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the database title
 *
 * Usage:
 *     title description
 */
GED_EXPORT BU_EXTERN(int ged_title, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get tessellation and calculation tolerances
 *
 * Usage:
 *     tol ([abs|rel|norm|dist|perp] [tolerance]) ...
 */
GED_EXPORT BU_EXTERN(int ged_tol, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Find all top level objects
 *
 * Usage:
 *     tops [-a] [-h] [-n] [-p], if NEW_TOPS_BEHAVIOR
 *     tops [-g] [-n] [-u]
 */
GED_EXPORT BU_EXTERN(int ged_tops, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Translate the view.
 *
 * Usage:
 *     tra x y z
 */
GED_EXPORT BU_EXTERN(int ged_tra, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a track
 *
 * Usage:
 *     track args
 */
GED_EXPORT BU_EXTERN(int ged_track, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 *
 * Usage:
 *     tree [-c] [-o outfile] [-i indentSize] [-d displayDepth] [object(s)]
 */
GED_EXPORT BU_EXTERN(int ged_tree, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output
 *
 * Usage:
 *     unhide object(s)
 */
GED_EXPORT BU_EXTERN(int ged_unhide, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the database units
 *
 * Usage:
 *     units [mm|cm|m|in|ft|...]
 */
GED_EXPORT BU_EXTERN(int ged_units, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert the specified view point to a model point.
 *
 * Usage:
 *     v2m_point x y z
 */
GED_EXPORT BU_EXTERN(int ged_v2m_point, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Vector drawing utility.
 *
 * Usage:
 *     vdraw write|insert|delete|read|send|params|open|vlist [args]
 */
GED_EXPORT BU_EXTERN(int ged_vdraw, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns the database version.
 *
 * Usage:
 *     version
 */
GED_EXPORT BU_EXTERN(int ged_version, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the view2model matrix.
 *
 * Usage:
 *     view2model
 */
GED_EXPORT BU_EXTERN(int ged_view2model, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the view. Note - x, y and z are rotations in view coordinates.
 *
 * Usage:
 *     vrot x y z
 */
GED_EXPORT BU_EXTERN(int ged_vrot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Return the view direction.
 *
 * Usage:
 *     viewdir [-i]
 */
GED_EXPORT BU_EXTERN(int ged_viewdir, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Return the specified region's id
 *
 * Usage:
 *     whatid region
 */
GED_EXPORT BU_EXTERN(int ged_whatid, (struct ged *gedp, int argc, const char *argv[]));

/**
 * The ged_which() function serves both whichair and whichid.
 *
 * Find the regions with the specified air codes
 *
 * Usage:
 *     whichair codes(s)
 *
 *
 * Find the regions with the specified region ids
 *
 * Usage:
 *     whichid [-s] id(s)
 *
 */
GED_EXPORT BU_EXTERN(int ged_which, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List the objects currently prepped for drawing
 *
 * Usage:
 *     who [r(eal)|p(hony)|b(oth)]
 */
GED_EXPORT BU_EXTERN(int ged_who, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Write material properties to a file for specified combination(s).
 *
 * Usage:
 *     wmater file combination1 [combination2 ...]
 */
GED_EXPORT BU_EXTERN(int ged_wmater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Push object path transformations to solids, creating primitives if necessary
 *
 * Usage:
 *     xpush object
 */
GED_EXPORT BU_EXTERN(int ged_xpush, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase all currently displayed geometry
 *
 * Usage:
 *     zap
 */
GED_EXPORT BU_EXTERN(int ged_zap, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Zoom the view in or out.
 *
 * Usage:
 *     zoom sf
 */
GED_EXPORT BU_EXTERN(int ged_zoom, (struct ged *gedp, int argc, const char *argv[]));


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
