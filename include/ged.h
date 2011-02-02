/*                           G E D . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

#if defined(_WIN32) && !defined(__CYGWIN__)
#  define NOMINMAX
#  include <windows.h>
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

/** all okay return code, not a maskable result */
#define GED_OK    0x0000

/**
 * possible maskable return codes from ged functions.  callers should
 * not rely on the actual values but should instead test via masking.
 */
#define GED_ERROR 0x0001 /**< something went wrong, the action was not performed */
#define GED_HELP  0x0002 /**< invalid specification, result contains usage */
#define GED_MORE  0x0004 /**< incomplete specification, can specify again interactively */
#define GED_QUIET 0x0008 /**< don't set or modify the result string */

#define GED_VMIN -2048.0
#define GED_VMAX 2047.0
#define GED_VRANGE 4095.0
#define INV_GED_V 0.00048828125
#define INV_4096_V 0.000244140625

#define GED_NULL (struct ged *)0
#define GED_DISPLAY_LIST_NULL (struct ged_display_list *)0
#define GED_DRAWABLE_NULL (struct ged_drawable *)0
#define GED_VIEW_NULL (struct ged_view *)0

#define GED_VIEW_OBJ_NULL ((struct view_obj *)0)
#define GED_RESULT_NULL ((void *)0)

#define GED_FUNC_PTR_NULL (ged_func_ptr)0
#define GED_REFRESH_CALLBACK_PTR_NULL (ged_refresh_callback_ptr)0

#define GED_IDLE_MODE 0
#define GED_ROTATE_MODE 1
#define GED_TRANSLATE_MODE 2
#define GED_SCALE_MODE 3
#define GED_CONSTRAINED_ROTATE_MODE 4
#define GED_CONSTRAINED_TRANSLATE_MODE 5
#define GED_OROTATE_MODE 6
#define GED_OSCALE_MODE 7
#define GED_OTRANSLATE_MODE 8
#define GED_MOVE_ARB_EDGE_MODE 9
#define GED_MOVE_ARB_FACE_MODE 10
#define GED_ROTATE_ARB_FACE_MODE 11
#define GED_PROTATE_MODE 12
#define GED_PSCALE_MODE 13
#define GED_PTRANSLATE_MODE 14
#define GED_RECTANGLE_MODE 15

/**
 * S E M A P H O R E S
 *
 * Definition of global parallel-processing semaphores.
 *
 */
#define GED_SEM_WORKER RT_SEM_LAST
#define GED_SEM_STATS GED_SEM_WORKER+1
#define GED_SEM_LIST GED_SEM_STATS+1
#define GED_SEM_LAST GED_SEM_LIST+1

#define GED_INIT(_gedp, _wdbp) { \
    ged_init((_gedp)); \
    (_gedp)->ged_wdbp = (_wdbp); \
}

#define GED_INITIALIZED(_gedp) ((_gedp)->ged_wdbp != RT_WDB_NULL)
#define GED_LOCAL2BASE(_gedp) ((_gedp)->ged_wdbp->dbip->dbi_local2base)
#define GED_BASE2LOCAL(_gedp) ((_gedp)->ged_wdbp->dbip->dbi_base2local)

/** Check if the object is a combination */
#define	GED_CHECK_COMB(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_COMB) == 0) { \
	int ged_check_comb_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_comb_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "%s is not a combination", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** Check if a database is open */
#define GED_CHECK_DATABASE_OPEN(_gedp, _flags) \
    if ((_gedp) == GED_NULL || (_gedp)->ged_wdbp == RT_WDB_NULL || (_gedp)->ged_wdbp->dbip == DBI_NULL) { \
	int ged_check_database_open_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_database_open_quiet) { \
	    if ((_gedp) != GED_NULL) { \
		bu_vls_trunc(&(_gedp)->ged_result_str, 0); \
		bu_vls_printf(&(_gedp)->ged_result_str, "A database is not open!"); \
	    } else {\
		bu_log("A database is not open!\n"); \
	    } \
	} \
	return (_flags); \
    }

/** Check if a drawable exists */
#define GED_CHECK_DRAWABLE(_gedp, _flags) \
    if (_gedp->ged_gdp == GED_DRAWABLE_NULL) { \
	int ged_check_drawable_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_drawable_quiet) { \
	    bu_vls_trunc(&(_gedp)->ged_result_str, 0); \
	    bu_vls_printf(&(_gedp)->ged_result_str, "A drawable does not exist."); \
	} \
	return (_flags); \
    }

/** Check if a view exists */
#define GED_CHECK_VIEW(_gedp, _flags) \
    if (_gedp->ged_gvp == GED_VIEW_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc(&(_gedp)->ged_result_str, 0); \
	    bu_vls_printf(&(_gedp)->ged_result_str, "A view does not exist."); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_CHECK_EXISTS(_gedp, _name, _noisy, _flags) \
    if (db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy)) != RT_DIR_NULL) { \
	int ged_check_exists_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_exists_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "%s already exists.", (_name)); \
	} \
	return (_flags); \
    }

/** Check if the database is read only */
#define	GED_CHECK_READ_ONLY(_gedp, _flags) \
    if ((_gedp)->ged_wdbp->dbip->dbi_read_only) { \
	int ged_check_read_only_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_read_only_quiet) { \
	    bu_vls_trunc(&(_gedp)->ged_result_str, 0); \
	    bu_vls_printf(&(_gedp)->ged_result_str, "Sorry, this database is READ-ONLY."); \
	} \
	return (_flags); \
    }

/** Check if the object is a region */
#define	GED_CHECK_REGION(_gedp, _dp, _flags) \
    if (((_dp)->d_flags & RT_DIR_REGION) == 0) { \
	int ged_check_region_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_region_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "%s is not a region.", (_dp)->d_namep); \
	} \
	return (_flags); \
    }

/** make sure there is a command name given */
#define GED_CHECK_ARGC_GT_0(_gedp, _argc, _flags) \
    if ((_argc) < 1) { \
	int ged_check_argc_gt_0_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_argc_gt_0_quiet) { \
	    bu_vls_trunc(&(_gedp)->ged_result_str, 0); \
	    bu_vls_printf(&(_gedp)->ged_result_str, "Command name not provided on (%s:%d).", __FILE__, __LINE__); \
	} \
	return (_flags); \
    }

/** add a new directory entry to the currently open database */
#define GED_DB_DIRADD(_gedp, _dp, _name, _laddr, _len, _dirflags, _ptr, _flags) \
    if (((_dp) = db_diradd((_gedp)->ged_wdbp->dbip, (_name), (_laddr), (_len), (_dirflags), (_ptr))) == RT_DIR_NULL) { \
	int ged_db_diradd_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_diradd_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "Unable to add %s to the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Lookup database object */
#define GED_DB_LOOKUP(_gedp, _dp, _name, _noisy, _flags) \
    if (((_dp) = db_lookup((_gedp)->ged_wdbp->dbip, (_name), (_noisy))) == RT_DIR_NULL) { \
	int ged_db_lookup_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_lookup_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "Unable to find %s in the database.", (_name)); \
	} \
	return (_flags); \
    }

/** Get internal representation */
#define GED_DB_GET_INTERNAL(_gedp, _intern, _dp, _mat, _resource, _flags) \
    if (rt_db_get_internal((_intern), (_dp), (_gedp)->ged_wdbp->dbip, (_mat), (_resource)) < 0) { \
	int ged_db_get_internal_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_get_internal_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "Database read failure."); \
	} \
	return (_flags); \
    }

/** Put internal representation */
#define GED_DB_PUT_INTERNAL(_gedp, _dp, _intern, _resource, _flags) \
    if (rt_db_put_internal((_dp), (_gedp)->ged_wdbp->dbip, (_intern), (_resource)) < 0) { \
	int ged_db_put_internal_quiet = (_flags) & GED_QUIET; \
	if (!ged_db_put_internal_quiet) { \
	    bu_vls_printf(&(_gedp)->ged_result_str, "Database write failure."); \
	} \
	return (_flags); \
    }

struct ged_adc_state {
    int		gas_draw;
    int		gas_dv_x;
    int		gas_dv_y;
    int		gas_dv_a1;
    int		gas_dv_a2;
    int		gas_dv_dist;
    fastf_t	gas_pos_model[3];
    fastf_t	gas_pos_view[3];
    fastf_t	gas_pos_grid[3];
    fastf_t	gas_a1;
    fastf_t	gas_a2;
    fastf_t	gas_dst;
    int		gas_anchor_pos;
    int		gas_anchor_a1;
    int		gas_anchor_a2;
    int		gas_anchor_dst;
    fastf_t	gas_anchor_pt_a1[3];
    fastf_t	gas_anchor_pt_a2[3];
    fastf_t	gas_anchor_pt_dst[3];
    int		gas_line_color[3];
    int		gas_tick_color[3];
    int		gas_line_width;
};

struct ged_axes_state {
    int       gas_draw;
    point_t   gas_axes_pos;		/* in model coordinates */
    fastf_t   gas_axes_size; 		/* in view coordinates */
    int	      gas_line_width;    	/* in pixels */
    int	      gas_pos_only;
    int	      gas_axes_color[3];
    int	      gas_label_color[3];
    int	      gas_triple_color;
    int	      gas_tick_enabled;
    int	      gas_tick_length;		/* in pixels */
    int	      gas_tick_major_length; 	/* in pixels */
    fastf_t   gas_tick_interval; 	/* in mm */
    int	      gas_ticks_per_major;
    int	      gas_tick_threshold;
    int	      gas_tick_color[3];
    int	      gas_tick_major_color[3];
};

struct ged_data_arrow_state {
    int       gdas_draw;
    int	      gdas_color[3];
    int	      gdas_line_width;    	/* in pixels */
    int       gdas_tip_length;
    int       gdas_tip_width;
    int       gdas_num_points;
    point_t   *gdas_points;		/* in model coordinates */
};

struct ged_data_axes_state {
    int       gdas_draw;
    int	      gdas_color[3];
    int	      gdas_line_width;    	/* in pixels */
    fastf_t   gdas_size; 		/* in view coordinates */
    int       gdas_num_points;
    point_t   *gdas_points;		/* in model coordinates */
};

struct ged_data_label_state {
    int		gdls_draw;
    int		gdls_color[3];
    int		gdls_num_labels;
    int		gdls_size;
    char	**gdls_labels;
    point_t	*gdls_points;
};

struct ged_data_line_state {
    int       gdls_draw;
    int	      gdls_color[3];
    int	      gdls_line_width;    	/* in pixels */
    int       gdls_num_points;
    point_t   *gdls_points;		/* in model coordinates */
};

struct ged_grid_state {
    int		ggs_draw;		/* draw grid */
    int		ggs_snap;		/* snap to grid */
    fastf_t	ggs_anchor[3];
    fastf_t	ggs_res_h;		/* grid resolution in h */
    fastf_t	ggs_res_v;		/* grid resolution in v */
    int		ggs_res_major_h;	/* major grid resolution in h */
    int		ggs_res_major_v;	/* major grid resolution in v */
    int		ggs_color[3];
};

struct ged_other_state {
    int gos_draw;
    int	gos_line_color[3];
    int	gos_text_color[3];
};

struct ged_rect_state {
    int		grs_active;	/* 1 - actively drawing a rectangle */
    int		grs_draw;	/* draw rubber band rectangle */
    int		grs_line_width;
    int		grs_line_style;  /* 0 - solid, 1 - dashed */
    int		grs_pos[2];	/* Position in image coordinates */
    int		grs_dim[2];	/* Rectangle dimension in image coordinates */
    fastf_t	grs_x;		/* Corner of rectangle in normalized     */
    fastf_t	grs_y;		/* ------ view coordinates (i.e. +-1.0). */
    fastf_t	grs_width;	/* Width and height of rectangle in      */
    fastf_t	grs_height;	/* ------ normalized view coordinates.   */
    int		grs_bg[3];	/* Background color */
    int		grs_color[3];	/* Rectangle color */
    int		grs_cdim[2];	/* Canvas dimension in pixels */
    fastf_t	grs_aspect;	/* Canvas aspect ratio */
};



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

struct ged_qray_color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct ged_qray_fmt {
    char type;
    struct bu_vls fmt;
};

struct ged_display_list {
    struct bu_list	l;
    struct directory	*gdl_dp;
    struct bu_vls	gdl_path;
    struct bu_list	gdl_headSolid;		/**< @brief  head of solid list for this object */
    int			gdl_wflag;
};

struct ged_drawable {
    struct bu_list		l;
    struct bu_list		gd_headDisplay;		/**< @brief  head of display list */
    struct bu_list		gd_headVDraw;		/**< @brief  head of vdraw list */
    struct vd_curve		*gd_currVHead;		/**< @brief  current vdraw head */
    struct solid		*gd_freeSolids;		/**< @brief  ptr to head of free solid list */

    char			**gd_rt_cmd;
    int				gd_rt_cmd_len;
    struct ged_run_rt		gd_headRunRt;		/**< @brief  head of forked rt processes */

    void			(*gd_rtCmdNotify)(int aborted);	/**< @brief  function called when rt command completes */

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
    struct bu_list		l;
    fastf_t			gv_scale;
    fastf_t			gv_size;		/**< @brief  2.0 * scale */
    fastf_t			gv_isize;		/**< @brief  1.0 / size */
    fastf_t			gv_perspective;		/**< @brief  perspective angle */
    vect_t			gv_aet;
    vect_t			gv_eye_pos;		/**< @brief  eye position */
    vect_t			gv_keypoint;
    char			gv_coord;		/**< @brief  coordinate system */
    char			gv_rotate_about;	/**< @brief  indicates what point rotations are about */
    mat_t			gv_rotation;
    mat_t			gv_center;
    mat_t			gv_model2view;
    mat_t			gv_pmodel2view;
    mat_t			gv_view2model;
    mat_t			gv_pmat;		/**< @brief  perspective matrix */
#if 0
    struct bu_observer		gv_observers;
#endif
    void 			(*gv_callback)();	/**< @brief  called in ged_view_update with gvp and gv_clientData */
    genptr_t			gv_clientData;		/**< @brief  passed to gv_callback */
    fastf_t			gv_prevMouseX;
    fastf_t			gv_prevMouseY;
    fastf_t			gv_minMouseDelta;
    fastf_t			gv_maxMouseDelta;
    fastf_t			gv_rscale;
    fastf_t			gv_sscale;
    int				gv_mode;
    int				gv_zclip;
    struct ged_adc_state 	gv_adc;
    struct ged_axes_state 	gv_model_axes;
    struct ged_axes_state 	gv_view_axes;
    struct ged_data_arrow_state gv_data_arrows;
    struct ged_data_axes_state 	gv_data_axes;
    struct ged_data_label_state gv_data_labels;
    struct ged_data_line_state  gv_data_lines;
    struct ged_data_arrow_state	gv_sdata_arrows;
    struct ged_data_axes_state 	gv_sdata_axes;
    struct ged_data_label_state gv_sdata_labels;
    struct ged_data_line_state 	gv_sdata_lines;
    struct ged_grid_state 	gv_grid;
    struct ged_other_state 	gv_center_dot;
    struct ged_other_state 	gv_prim_labels;
    struct ged_other_state 	gv_view_params;
    struct ged_other_state 	gv_view_scale;
    struct ged_rect_state 	gv_rect;
};


struct ged {
    struct bu_list		l;
    struct rt_wdb		*ged_wdbp;

    /** for catching log messages */
    struct bu_vls		ged_log;

    /** for setting results */
    struct bu_vls		ged_result_str;

    struct ged_drawable		*ged_gdp;
    struct ged_view		*ged_gvp;

    void			*ged_dmp;
    void			*ged_refresh_clientdata;	/**< @brief  client data passed to refresh handler */
    void			(*ged_refresh_handler)();	/**< @brief  function for handling refresh requests */
    void			(*ged_output_handler)();	/**< @brief  function for handling output */
    char			*ged_output_script;		/**< @brief  script for use by the outputHandler */

    /* FIXME -- this ugly hack needs to die.  the result string should be stored before the call. */
    int 			ged_internal_call;
};

typedef int (*ged_func_ptr)(struct ged *, int, const char *[]);
typedef void (*ged_refresh_callback_ptr)(void *);

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


/* defined in adc.c */
GED_EXPORT BU_EXTERN(void ged_calc_adc_pos,
		     (struct ged_view *gvp));
GED_EXPORT BU_EXTERN(void ged_calc_adc_a1,
		     (struct ged_view *gvp));
GED_EXPORT BU_EXTERN(void ged_calc_adc_a2,
		     (struct ged_view *gvp));
GED_EXPORT BU_EXTERN(void ged_calc_adc_dst,
		     (struct ged_view *gvp));

/* defined in clip.c */
GED_EXPORT BU_EXTERN(int ged_clip,
		     (fastf_t *xp1,
		      fastf_t *yp1,
		      fastf_t *xp2,
		      fastf_t *yp2));
GED_EXPORT BU_EXTERN(int ged_vclip,
		     (vect_t a,
		      vect_t b,
		      fastf_t *min,
		      fastf_t *max));

/* defined in copy.c */
GED_EXPORT BU_EXTERN(int ged_dbcopy,
		     (struct ged *from_gedp,
		      struct ged *to_gedp,
		      const char *from,
		      const char *to,
		      int fflag));

/* defined in draw.c */
GED_EXPORT BU_EXTERN (void ged_color_soltab,
		      (struct bu_list *hdlp));
GED_EXPORT BU_EXTERN (struct ged_display_list *ged_addToDisplay,
		      (struct ged *gedp,
		       const char *name));

/* defined in erase.c */
GED_EXPORT BU_EXTERN (void ged_erasePathFromDisplay,
		      (struct ged *gedp,
		       const char *path,
		       int allow_split));

/* defined in ged.c */
GED_EXPORT BU_EXTERN(void ged_close,
		     (struct ged *gedp));
GED_EXPORT BU_EXTERN(void ged_drawable_close,
		     (struct ged_drawable *gdp));
GED_EXPORT BU_EXTERN(void ged_free,
		     (struct ged *gedp));
GED_EXPORT BU_EXTERN(void ged_init,
		     (struct ged *gedp));
GED_EXPORT BU_EXTERN(struct ged *ged_open,
		     (const char *dbtype,
		      const char *filename,
		      int existing_only));
GED_EXPORT BU_EXTERN(void ged_view_init,
		     (struct ged_view *gvp));

/* defined in grid.c */
GED_EXPORT BU_EXTERN(void ged_snap_to_grid,
		     (struct ged *gedp, fastf_t *vx, fastf_t *vy));

/* defined in inside.c */
GED_EXPORT BU_EXTERN(int ged_inside_internal,
		     (struct ged *gedp,
		      struct rt_db_internal *ip,
		      int argc,
		      const char *argv[],
		      int arg,
		      char *o_name));

/* defined in rt.c */
GED_EXPORT BU_EXTERN(int ged_build_tops,
		     (struct ged	*gedp,
		      char		**start,
		      char		**end));
GED_EXPORT BU_EXTERN(size_t ged_count_tops, (struct ged *gedp));


/* FIXME: wdb routines do not belong in libged.  need to be
 * refactored, renamed, or removed.
 */
/* defined in wdb_comb_std.c */
GED_EXPORT BU_EXTERN(int wdb_comb_std_cmd,
		     (struct rt_wdb	*gedp,
		      Tcl_Interp	*interp,
		      int		argc,
		      char 		**argv));

/* FIXME: wdb routines do not belong in libged.  need to be
 * refactored, renamed, or removed.
 */
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
		     const char *argv[]));
GED_EXPORT BU_EXTERN(int	wdb_adjust_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     const char *argv[]));
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
		     const char *argv[]));
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
GED_EXPORT BU_EXTERN(int	wdb_stub_cmd,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp *interp,
		     int argc,
		     const char *argv[]));
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
GED_EXPORT BU_EXTERN(int	wdb_bo_cmd,
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



/* FIXME: vo routines do not belong in libged.  need to be refactored,
 * renamed, or removed.
 */
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


/* Defined in vutil.c */
GED_EXPORT BU_EXTERN(void ged_persp_mat,
		     (fastf_t *m,
		      fastf_t fovy,
		      fastf_t aspect,
		      fastf_t near1,
		      fastf_t far1,
		      fastf_t backoff));
GED_EXPORT BU_EXTERN(void ged_mike_persp_mat,
		     (fastf_t *pmat,
		      const fastf_t *eye));
GED_EXPORT BU_EXTERN(void ged_deering_persp_mat,
		     (fastf_t *m,
		      const fastf_t *l,
		      const fastf_t *h,
		      const fastf_t *eye));
GED_EXPORT BU_EXTERN(void ged_view_update,
		     (struct ged_view *gvp));


/**
 * Creates an arb8 given the following:
 *   1)   3 points of one face
 *   2)   coord x, y or z and 2 coordinates of the 4th point in that face
 *   3)   thickness
 */
GED_EXPORT BU_EXTERN(int ged_3ptarb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Angle distance cursor.
 */
GED_EXPORT BU_EXTERN(int ged_adc, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Adjust object's attribute(s)
 */
GED_EXPORT BU_EXTERN(int ged_adjust, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Creates an arb8 given rotation and fallback angles
 */
GED_EXPORT BU_EXTERN(int ged_arb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert az/el to a direction vector.
 */
GED_EXPORT BU_EXTERN(int ged_ae2dir, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get or set the azimuth, elevation and twist.
 */
GED_EXPORT BU_EXTERN(int ged_aet, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns lots of information about the specified object(s)
 */
GED_EXPORT BU_EXTERN(int ged_analyze, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Creates an annotation.
 */
GED_EXPORT BU_EXTERN(int ged_annotate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Allow editing of the matrix, etc., along an arc.
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
 */
GED_EXPORT BU_EXTERN(int ged_attr, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate angle degrees about the specified axis
 */
GED_EXPORT BU_EXTERN(int ged_arot_args, (struct ged *gedp, int argc, const char *argv[], mat_t rmat));
GED_EXPORT BU_EXTERN(int ged_arot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Auto-adjust the view so that all displayed geometry is in view
 */
GED_EXPORT BU_EXTERN(int ged_autoview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Report the size of the bounding box (rpp) around the specified object
 */
GED_EXPORT BU_EXTERN(int ged_bb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Tesselates each operand object, then performs the
 * boolean evaluation, storing result in new_obj
 */
GED_EXPORT BU_EXTERN(int ged_bev, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Manipulate opaque binary objects.
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
 */
GED_EXPORT BU_EXTERN(int ged_bo, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase all currently displayed geometry and draw the specified object(s)
 */
GED_EXPORT BU_EXTERN(int ged_blast, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Query or manipulate properties of bot
 */
GED_EXPORT BU_EXTERN(int ged_bot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create new_bot by condensing old_bot
 */
GED_EXPORT BU_EXTERN(int ged_bot_condense, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Uses edge decimation to reduce the number of triangles in the
 * specified BOT while keeping within the specified constraints.
 */
GED_EXPORT BU_EXTERN(int ged_bot_decimate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Dump bots to the specified format.
 */
GED_EXPORT BU_EXTERN(int ged_bot_dump, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Dump displayed bots to the specified format.
 */
GED_EXPORT BU_EXTERN(int ged_dbot_dump, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create new_bot by fusing faces in old_bot
 */
GED_EXPORT BU_EXTERN(int ged_bot_face_fuse, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Sort the facelist of BOT solids to optimize ray trace performance
 * for a particular number of triangles per raytrace piece.
 */
GED_EXPORT BU_EXTERN(int ged_bot_face_sort, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Flip/reverse the specified bot's orientation.
 */
GED_EXPORT BU_EXTERN(int ged_bot_flip, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create bot_dest by merging the bot sources.
 */
GED_EXPORT BU_EXTERN(int ged_bot_merge, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Calculate vertex normals for the BOT primitive
 */
GED_EXPORT BU_EXTERN(int ged_bot_smooth, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Split the specified bot
 */
GED_EXPORT BU_EXTERN(int ged_bot_split, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Sync the specified bot's orientation (i.e. make sure all neighboring triangles have same orientation).
 */
GED_EXPORT BU_EXTERN(int ged_bot_sync, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Fuse bot vertices
 */
GED_EXPORT BU_EXTERN(int ged_bot_vertex_fuse, (struct ged *gedp, int argc, const char *argv[]));

/**
 * BREP utility command
 */
GED_EXPORT BU_EXTERN(int ged_brep, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List attributes (brief).
 */
GED_EXPORT BU_EXTERN(int ged_cat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create constraint object
 */
GED_EXPORT BU_EXTERN(int ged_cc, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get or set the view center.
 */
GED_EXPORT BU_EXTERN(int ged_center, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Performs a deep copy of object.
 */
GED_EXPORT BU_EXTERN(int ged_clone, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make color entry.
 */
GED_EXPORT BU_EXTERN(int ged_color, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set combination color.
 */
GED_EXPORT BU_EXTERN(int ged_comb_color, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or extend combination w/booleans.
 */
GED_EXPORT BU_EXTERN(int ged_comb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or extend a combination using standard notation.
 */
GED_EXPORT BU_EXTERN(int ged_comb_std, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get comb's members.
 */
GED_EXPORT BU_EXTERN(int ged_combmem, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Import a database into the current database using an auto-incrementing or custom affix
 */
GED_EXPORT BU_EXTERN(int ged_concat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Copy a database object
 */
GED_EXPORT BU_EXTERN(int ged_copy, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Copy an 'evaluated' path solid
 */
GED_EXPORT BU_EXTERN(int ged_copyeval, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Copy the matrix from one combination's arc to another.
 */
GED_EXPORT BU_EXTERN(int ged_copymat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Copy cylinder and position at end of original cylinder
 */
GED_EXPORT BU_EXTERN(int ged_cpi, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get dbip
 */
GED_EXPORT BU_EXTERN(int ged_dbip, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get libbu's debug bit vector
 */
GED_EXPORT BU_EXTERN(int ged_debugbu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Dump of the database's directory
 */
GED_EXPORT BU_EXTERN(int ged_debugdir, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get librt's debug bit vector
 */
GED_EXPORT BU_EXTERN(int ged_debuglib, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Provides user-level access to LIBBU's bu_prmem()
 */
GED_EXPORT BU_EXTERN(int ged_debugmem, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get librt's NMG debug bit vector
 */
GED_EXPORT BU_EXTERN(int ged_debugnmg, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Decompose nmg_solid into maximally connected shells
 */
GED_EXPORT BU_EXTERN(int ged_decompose, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Delay the specified amount of time
 */
GED_EXPORT BU_EXTERN(int ged_delay, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert a direction vector to az/el.
 */
GED_EXPORT BU_EXTERN(int ged_dir2ae, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Prepare object(s) for display
 */
GED_EXPORT BU_EXTERN(int ged_draw, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Dump a full copy of the database into file.g
 */
GED_EXPORT BU_EXTERN(int ged_dump, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Check for duplicate names in file
 */
GED_EXPORT BU_EXTERN(int ged_dup, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Prepare object(s) for display
 */
GED_EXPORT BU_EXTERN(int ged_E, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Prepare all regions with the given air code(s) for display
 */
GED_EXPORT BU_EXTERN(int ged_eac, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Echo the specified arguments.
 */
GED_EXPORT BU_EXTERN(int ged_echo, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Text edit the color table
 */
GED_EXPORT BU_EXTERN(int ged_edcolor, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit region ident codes.
 */
GED_EXPORT BU_EXTERN(int ged_edcodes, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit combination.
 */
GED_EXPORT BU_EXTERN(int ged_edcomb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit file.
 */
GED_EXPORT BU_EXTERN(int ged_editit, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit combination materials.
 *
 * Command relies on rmater, editit, and wmater commands.
 */
GED_EXPORT BU_EXTERN(int ged_edmater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase objects from the display.
 */
GED_EXPORT BU_EXTERN(int ged_erase, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase all occurrences of objects from the display.
 */
GED_EXPORT BU_EXTERN(int ged_erase_all, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Evaluate objects via NMG tessellation
 */
GED_EXPORT BU_EXTERN(int ged_ev, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the eye point
 */
GED_EXPORT BU_EXTERN(int ged_eye, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the eye position
 */
GED_EXPORT BU_EXTERN(int ged_eye_pos, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Globs expression against database objects
 */
GED_EXPORT BU_EXTERN(int ged_expand, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Facetize the specified objects
 */
GED_EXPORT BU_EXTERN(int ged_facetize, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Find combinations that reference object
 */
GED_EXPORT BU_EXTERN(int ged_find, (struct ged *gedp, int argc, const char *argv[]));

/**
 * returns form for objects of type "type"
 */
GED_EXPORT BU_EXTERN(int ged_form, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Given an NMG solid, break it up into several NMG solids, each
 * containing a single shell with a single sub-element.
 */
GED_EXPORT BU_EXTERN(int ged_fracture, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get object attributes
 */
GED_EXPORT BU_EXTERN(int ged_get, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get view size and center such that all displayed solids would be in view
 */
GED_EXPORT BU_EXTERN(int ged_get_autoview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get combination information
 */
GED_EXPORT BU_EXTERN(int ged_get_comb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the viewsize, orientation and eye point.
 */
GED_EXPORT BU_EXTERN(int ged_get_eyemodel, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the object's type
 */
GED_EXPORT BU_EXTERN(int ged_get_type, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Globs expression against the database
 */
GED_EXPORT BU_EXTERN(int ged_glob, (struct ged *gedp, int argc, const char *argv[]));

/**
 *
 */
GED_EXPORT BU_EXTERN(int ged_gqa, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Grid utility command.
 */
GED_EXPORT BU_EXTERN(int ged_grid, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert grid coordinates to model coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_grid2model_lu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert grid coordinates to view coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_grid2view_lu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or append objects to a group
 */
GED_EXPORT BU_EXTERN(int ged_group, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the "hidden" flag for the specified objects so they do not
 * appear in an "ls" command output
 */
GED_EXPORT BU_EXTERN(int ged_hide, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns how an object is being displayed
 */
GED_EXPORT BU_EXTERN(int ged_how, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a human
 */
GED_EXPORT BU_EXTERN(int ged_human, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Illuminate/highlight database object.
 */
GED_EXPORT BU_EXTERN(int ged_illum, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a primitive via keyboard.
 */
GED_EXPORT BU_EXTERN(int ged_in, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Finds the inside primitive per the specified thickness.
 */
GED_EXPORT BU_EXTERN(int ged_inside, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Add instance of obj to comb
 */
GED_EXPORT BU_EXTERN(int ged_instance, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Makes a bot object out of the specified section.
 */
GED_EXPORT BU_EXTERN(int ged_importFg4Section, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns the inverse view size.
 */
GED_EXPORT BU_EXTERN(int ged_isize, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set region ident codes.
 */
GED_EXPORT BU_EXTERN(int ged_item, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Save/keep the specified objects in the specified file
 */
GED_EXPORT BU_EXTERN(int ged_keep, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the keypoint
 */
GED_EXPORT BU_EXTERN(int ged_keypoint, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill/delete the specified objects from the database
 */
GED_EXPORT BU_EXTERN(int ged_kill, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill/delete the specified objects from the database, removing all references
 */
GED_EXPORT BU_EXTERN(int ged_killall, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill all references to the specified object(s).
 */
GED_EXPORT BU_EXTERN(int ged_killrefs, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Kill all paths belonging to objects
 */
GED_EXPORT BU_EXTERN(int ged_killtree, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List object information, verbose.
 */
GED_EXPORT BU_EXTERN(int ged_list, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Load the view
 */
GED_EXPORT BU_EXTERN(int ged_loadview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Used to control logging.
 */
GED_EXPORT BU_EXTERN(int ged_log, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the look-at point
 */
GED_EXPORT BU_EXTERN(int ged_lookat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List the objects in this database
 */
GED_EXPORT BU_EXTERN(int ged_ls, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List object's tree as a tcl list of {operator object} pairs
 */
GED_EXPORT BU_EXTERN(int ged_lt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert the specified model point to a view point.
 */
GED_EXPORT BU_EXTERN(int ged_m2v_point, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make a new primitive.
 */
GED_EXPORT BU_EXTERN(int ged_make, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Creates a point-cloud (pnts) given the following:
 *   1)   object name
 *   2)   path and filename to point data file
 *   3)   point data file format (xyzrgbsijk?)
 *   4)   point data file units or conversion factor to mm
 *   5)   default diameter of each point
 */
GED_EXPORT BU_EXTERN(int ged_make_pnts, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make a bounding box (rpp) around the specified objects
 */
GED_EXPORT BU_EXTERN(int ged_make_bb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Make a unique object name.
 */
GED_EXPORT BU_EXTERN(int ged_make_name, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Globs expression against database objects, does not return tokens that match nothing
 */
GED_EXPORT BU_EXTERN(int ged_match, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Modify material information.
 */
GED_EXPORT BU_EXTERN(int ged_mater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Mirror the primitive or combination along the specified axis.
 */
GED_EXPORT BU_EXTERN(int ged_mirror, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert model coordinates to grid coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_model2grid_lu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the model to view matrix
 */
GED_EXPORT BU_EXTERN(int ged_model2view, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert model coordinates to view coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_model2view_lu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move an arb's edge through point
 */
GED_EXPORT BU_EXTERN(int ged_move_arb_edge, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move/rename a database object
 */
GED_EXPORT BU_EXTERN(int ged_move, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move/rename all occurrences object
 */
GED_EXPORT BU_EXTERN(int ged_move_all, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Move an arb's face through point
 */
GED_EXPORT BU_EXTERN(int ged_move_arb_face, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the view. Note - x, y and z are rotations in model coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_mrot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Trace a single ray from the current view.
 */
GED_EXPORT BU_EXTERN(int ged_nirt, (struct ged *gedp, int argc, const char *argv[]));
GED_EXPORT BU_EXTERN(int ged_vnirt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Decimate NMG primitive via edge collapse
 */
GED_EXPORT BU_EXTERN(int ged_nmg_collapse, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Attempt to fix an NMG primitive's normals.
 */
GED_EXPORT BU_EXTERN(int ged_nmg_fix_normals, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Simplify the NMG primitive, if possible
 */
GED_EXPORT BU_EXTERN(int ged_nmg_simplify, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get object center.
 */
GED_EXPORT BU_EXTERN(int ged_ocenter, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Open a database
 */
GED_EXPORT BU_EXTERN(int ged_reopen, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the view orientation using a quaternion.
 */
GED_EXPORT BU_EXTERN(int ged_orient, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate obj about the keypoint by
 */
GED_EXPORT BU_EXTERN(int ged_orotate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Scale obj about the keypoint by sf.
 */
GED_EXPORT BU_EXTERN(int ged_oscale, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Translate obj by dx dy dz.
 */
GED_EXPORT BU_EXTERN(int ged_otranslate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Overlay the specified 2D/3D UNIX plot file
 */
GED_EXPORT BU_EXTERN(int ged_overlay, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List all paths from name(s) to leaves
 */
GED_EXPORT BU_EXTERN(int ged_pathlist, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Lists all paths matching the input path
 */
GED_EXPORT BU_EXTERN(int ged_pathsum, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the perspective angle.
 */
GED_EXPORT BU_EXTERN(int ged_perspective, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a unix plot file of the currently displayed objects.
 */
GED_EXPORT BU_EXTERN(int ged_plot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the perspective matrix.
 */
GED_EXPORT BU_EXTERN(int ged_pmat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the pmodel2view matrix.
 */
GED_EXPORT BU_EXTERN(int ged_pmodel2view, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a png file of the view.
 */
GED_EXPORT BU_EXTERN(int ged_png, (struct ged *gedp, int argc, const char *argv[]));
GED_EXPORT BU_EXTERN(int ged_screen_grab, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set point of view
 */
GED_EXPORT BU_EXTERN(int ged_pov, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Print color table
 */
GED_EXPORT BU_EXTERN(int ged_prcolor, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Prefix the specified objects with the specified prefix
 */
GED_EXPORT BU_EXTERN(int ged_prefix, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Preview a new style RT animation script.
 */
GED_EXPORT BU_EXTERN(int ged_preview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a postscript file of the view.
 */
GED_EXPORT BU_EXTERN(int ged_ps, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate obj's attributes by rvec.
 */
GED_EXPORT BU_EXTERN(int ged_protate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Scale obj's attributes by sf.
 */
GED_EXPORT BU_EXTERN(int ged_pscale, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Translate obj's attributes by tvec.
 */
GED_EXPORT BU_EXTERN(int ged_ptranslate, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Push objects' path transformations to  primitives
 */
GED_EXPORT BU_EXTERN(int ged_push, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a database object
 */
GED_EXPORT BU_EXTERN(int ged_put, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set combination attributes
 */
GED_EXPORT BU_EXTERN(int ged_put_comb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Replace the matrix on an arc
 */
GED_EXPORT BU_EXTERN(int ged_putmat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set query_ray attributes
 */
GED_EXPORT BU_EXTERN(int ged_qray, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set the view orientation using a quaternion
 */
GED_EXPORT BU_EXTERN(int ged_quat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the view from a direction vector and twist.
 */
GED_EXPORT BU_EXTERN(int ged_qvrot, (struct ged *gedp, int argc, const char *argv[]));

GED_EXPORT BU_EXTERN(void ged_init_qray,
		    (struct ged_drawable *gdp));
GED_EXPORT BU_EXTERN(void ged_free_qray,
		    (struct ged_drawable *gdp));

/**
 * Read region ident codes from filename.
 */
GED_EXPORT BU_EXTERN(int ged_rcodes, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rubber band rectangle utility.
 */
GED_EXPORT BU_EXTERN(int ged_rect, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Edit region/comb
 */
GED_EXPORT BU_EXTERN(int ged_red, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Change the default region ident codes: item air los mat
 */
GED_EXPORT BU_EXTERN(int ged_regdef, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or append objects to a region
 */
GED_EXPORT BU_EXTERN(int ged_region, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Remove members from a combination
 */
GED_EXPORT BU_EXTERN(int ged_remove, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns the solid table & vector list as a string
 */
GED_EXPORT BU_EXTERN(int ged_report, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Makes and arb given a point, 2 coord of 3 pts, rot, fb and thickness.
 */
GED_EXPORT BU_EXTERN(int ged_rfarb, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns a list of id to region name mappings for the entire database.
 */
GED_EXPORT BU_EXTERN(int ged_rmap, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the rotation matrix.
 */
GED_EXPORT BU_EXTERN(int ged_rmat, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Read material properties from a file.
 */
GED_EXPORT BU_EXTERN(int ged_rmater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the view.
 */
GED_EXPORT BU_EXTERN(int ged_rot_args, (struct ged *gedp, int argc, const char *argv[], char *coord, mat_t rmat));
GED_EXPORT BU_EXTERN(int ged_rot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the rotate_about point.
 */
GED_EXPORT BU_EXTERN(int ged_rotate_about, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate an arb's face through point
 */
GED_EXPORT BU_EXTERN(int ged_rotate_arb_face, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the point.
 */
GED_EXPORT BU_EXTERN(int ged_rot_point, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Invoke any raytracing application with the current view.
 */
GED_EXPORT BU_EXTERN(int ged_rrt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns a list of items within the previously defined rectangle.
 */
GED_EXPORT BU_EXTERN(int ged_rselect, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Run the raytracing application.
 */
GED_EXPORT BU_EXTERN(int ged_rt, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Abort the current raytracing processes.
 */
GED_EXPORT BU_EXTERN(int ged_rtabort, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Check for overlaps in the current view.
 */
GED_EXPORT BU_EXTERN(int ged_rtcheck, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Save keyframe in file (experimental)
 */
GED_EXPORT BU_EXTERN(int ged_savekey, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Save the view
 */
GED_EXPORT BU_EXTERN(int ged_saveview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Scale the view.
 */
GED_EXPORT BU_EXTERN(int ged_scale_args, (struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3));
GED_EXPORT BU_EXTERN(int ged_scale, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Interface to search functionality (i.e. Unix find for geometry)
 */
GED_EXPORT BU_EXTERN(int ged_search, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns a list of items within the specified rectangle or circle.
 */
GED_EXPORT BU_EXTERN(int ged_select, (struct ged *gedp, int argc, const char *argv[]));


/**
 * Get/set the output handler script
 */
GED_EXPORT BU_EXTERN(int ged_set_output_script, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set the unix plot output mode
 */
GED_EXPORT BU_EXTERN(int ged_set_uplotOutputMode, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the transparency of the specified object
 */
GED_EXPORT BU_EXTERN(int ged_set_transparency, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set the view orientation given the angles x, y and z in degrees.
 */
GED_EXPORT BU_EXTERN(int ged_setview, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the shaded mode.
 */
GED_EXPORT BU_EXTERN(int ged_shaded_mode, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Simpler, command-line version of 'mater' command.
 */
GED_EXPORT BU_EXTERN(int ged_shader, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Breaks the NMG model into seperate shells
 */
GED_EXPORT BU_EXTERN(int ged_shells, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Show the matrix transformations along path
 */
GED_EXPORT BU_EXTERN(int ged_showmats, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get or set the view size.
 */
GED_EXPORT BU_EXTERN(int ged_size, (struct ged *gedp, int argc, const char *argv[]));

/**
 *
 */
GED_EXPORT BU_EXTERN(int ged_solids_on_ray, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Slew the view
 */
GED_EXPORT BU_EXTERN(int ged_slew, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create or append objects to a group using a sphere
 */
GED_EXPORT BU_EXTERN(int ged_sphgroup, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Count/list primitives/regions/groups
 */
GED_EXPORT BU_EXTERN(int ged_summary, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Sync up the in-memory database with the on-disk database.
 */
GED_EXPORT BU_EXTERN(int ged_sync, (struct ged *gedp, int argc, const char *argv[]));

/**
 * The ged_tables() function serves idents, regions and solids.
 *
 * Make ascii summary of region idents.
 *
 */
GED_EXPORT BU_EXTERN(int ged_tables, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a tire
 */
GED_EXPORT BU_EXTERN(int ged_tire, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the database title
 */
GED_EXPORT BU_EXTERN(int ged_title, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get tessellation and calculation tolerances
 */
GED_EXPORT BU_EXTERN(int ged_tol, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Find all top level objects
 */
GED_EXPORT BU_EXTERN(int ged_tops, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Translate the view.
 */
GED_EXPORT BU_EXTERN(int ged_tra_args, (struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec));
GED_EXPORT BU_EXTERN(int ged_tra, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Create a track
 */
GED_EXPORT BU_EXTERN(int ged_track, (struct ged *gedp, int argc, const char *argv[]));

#if 0
/**
 *
 *
 * Usage:
 *     tracker [-fh] [# links] [increment] [spline.iges] [link...]
 */
GED_EXPORT BU_EXTERN(int ged_tracker, (struct ged *gedp, int argc, const char *argv[]));
#endif

/**
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 */
GED_EXPORT BU_EXTERN(int ged_tree, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output
 */
GED_EXPORT BU_EXTERN(int ged_unhide, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Set/get the database units
 */
GED_EXPORT BU_EXTERN(int ged_units, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert the specified view point to a model point.
 */
GED_EXPORT BU_EXTERN(int ged_v2m_point, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Vector drawing utility.
 */
GED_EXPORT BU_EXTERN(int ged_vdraw, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Returns the database version.
 */
GED_EXPORT BU_EXTERN(int ged_version, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set view attributes
 */
GED_EXPORT BU_EXTERN(int ged_view, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert view coordinates to grid coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_view2grid_lu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get the view2model matrix.
 */
GED_EXPORT BU_EXTERN(int ged_view2model, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert view coordinates to model coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_view2model_lu, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Convert a view vector to a model vector.
 */
GED_EXPORT BU_EXTERN(int ged_view2model_vec, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Rotate the view. Note - x, y and z are rotations in view coordinates.
 */
GED_EXPORT BU_EXTERN(int ged_vrot, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Return the view direction.
 */
GED_EXPORT BU_EXTERN(int ged_viewdir, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Write region ident codes to filename.
 */
GED_EXPORT BU_EXTERN(int ged_wcodes, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Return the specified region's id.
 */
GED_EXPORT BU_EXTERN(int ged_whatid, (struct ged *gedp, int argc, const char *argv[]));

/**
 * The ged_which() function serves both whichair and whichid.
 *
 * Find the regions with the specified air codes.  Find the regions
 * with the specified region ids.
 */
GED_EXPORT BU_EXTERN(int ged_which, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Return all combinations with the specified shaders.
 */
GED_EXPORT BU_EXTERN(int ged_which_shader, (struct ged *gedp, int argc, const char *argv[]));

/**
 * List the objects currently prepped for drawing
 */
GED_EXPORT BU_EXTERN(int ged_who, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Write material properties to a file for specified combination(s).
 */
GED_EXPORT BU_EXTERN(int ged_wmater, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Push object path transformations to solids, creating primitives if necessary
 */
GED_EXPORT BU_EXTERN(int ged_xpush, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Get/set the view orientation using yaw, pitch and roll
 */
GED_EXPORT BU_EXTERN(int ged_ypr, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Erase all currently displayed geometry
 */
GED_EXPORT BU_EXTERN(int ged_zap, (struct ged *gedp, int argc, const char *argv[]));

/**
 * Zoom the view in or out.
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

