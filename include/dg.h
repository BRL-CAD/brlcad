/*                            D G . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file dg.h
 *
 * Functions provided by the LIBGED geometry editing library that are
 * related to drawable geometry.
 *
 * A drawable geometry object is associated with a database object and
 * is used to maintain lists of geometry that are ready for display.
 * This geometry can come from a BRL-CAD database or from vdraw
 * commands.  The drawable geometry object is also capabable of
 * indirectly raytracing geometry that comes from a BRL-CAD database.
 */

#ifndef __DG_H__
#define __DG_H__

#include "ged.h"

__BEGIN_DECLS

/*
 * Carl's vdraw stuff.
 */
#define RT_VDRW_PREFIX		"_VDRW"
#define RT_VDRW_PREFIX_LEN	6
#define RT_VDRW_MAXNAME		31
#define RT_VDRW_DEF_COLOR	0xffff00
struct vd_curve {
    struct bu_list	l;
    char		vdc_name[RT_VDRW_MAXNAME+1]; 	/**< @brief  name array */
    long		vdc_rgb;	/**< @brief  color */
    struct bu_list	vdc_vhd;	/**< @brief  head of list of vertices */
};
#define VD_CURVE_NULL	((struct vd_curve *)NULL)

/**
 * Used to keep track of forked rt's for possible future aborts.
 * Currently used in mged/rtif.c and librt/dg_obj.c
 */
struct run_rt {
    struct bu_list l;
#ifdef _WIN32
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

struct dg_qray_color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct dg_qray_fmt {
    char type;
    struct bu_vls fmt;
};

struct dg_obj {
    struct bu_list		l;
    struct bu_vls		dgo_name;		/**< @brief  drawable geometry object name */
    struct rt_wdb		*dgo_wdbp;		/**< @brief  associated database */
    struct bu_list		dgo_headSolid;		/**< @brief  head of solid list */
    struct bu_list		dgo_headVDraw;		/**< @brief  head of vdraw list */
    struct vd_curve		*dgo_currVHead;		/**< @brief  current vdraw head */
    struct solid		*dgo_freeSolids;	/**< @brief  ptr to head of free solid list */
    char			*dgo_rt_cmd[RT_MAXARGS];
    int				dgo_rt_cmd_len;
    struct bu_observer		dgo_observers;
    struct run_rt		dgo_headRunRt;		/**< @brief  head of forked rt processes */
    struct bu_vls		dgo_qray_basename;	/**< @brief  basename of query ray vlist */
    struct bu_vls		dgo_qray_script;	/**< @brief  query ray script */
    char			dgo_qray_effects;	/**< @brief  t for text, g for graphics or b for both */
    int				dgo_qray_cmd_echo;	/**< @brief  0 - don't echo command, 1 - echo command */
    struct dg_qray_fmt		*dgo_qray_fmts;
    struct dg_qray_color	dgo_qray_odd_color;
    struct dg_qray_color	dgo_qray_even_color;
    struct dg_qray_color	dgo_qray_void_color;
    struct dg_qray_color	dgo_qray_overlap_color;
    int				dgo_shaded_mode;	/**< @brief  1 - draw bots shaded by default */
    char			*dgo_outputHandler;	/**< @brief  tcl script for handling output */
    int				dgo_uplotOutputMode;	/**< @brief  output mode for unix plots */
    void			(*dgo_rtCmdNotify)();	/**< @brief  function called when rt command completes */
};
GED_EXPORT extern struct dg_obj HeadDGObj;		/**< @brief  head of drawable geometry object list */
#define GED_DGO_NULL		((struct dg_obj *)NULL)

/* defined in dg_obj.c */
GED_EXPORT BU_EXTERN(int Dgo_Init,
		    (Tcl_Interp *interp));
GED_EXPORT BU_EXTERN(int dgo_cmd,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int dgo_set_outputHandler_cmd,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp,
		     int		argc,
		     char 		**argv));
GED_EXPORT BU_EXTERN(int dgo_set_transparency_cmd,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp,
		     int		argc,
		     char 		**argv));
GED_EXPORT BU_EXTERN(int	dgo_observer_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(void dgo_deleteProc,
		    (ClientData clientData));
GED_EXPORT BU_EXTERN(void dgo_autoview,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp));
GED_EXPORT BU_EXTERN(int	dgo_autoview_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
GED_EXPORT BU_EXTERN(int	dgo_blast_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_draw_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc, char **argv,
		     int kind));
GED_EXPORT BU_EXTERN(int	dgo_E_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_erase_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_erase_all_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_get_autoview_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_how_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_illum_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_label_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(struct dg_obj *dgo_open_cmd,
		    (char *oname,
		     struct rt_wdb *wdbp));
GED_EXPORT BU_EXTERN(int	dgo_overlay_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_report_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_rt_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_rtabort_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_rtcheck_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc, char **argv));
GED_EXPORT BU_EXTERN(int	dgo_who_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(void dgo_zap_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp));
GED_EXPORT BU_EXTERN(int	dgo_shaded_mode_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int dgo_tree_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));

GED_EXPORT BU_EXTERN(void dgo_color_soltab,
		    ());
GED_EXPORT BU_EXTERN(void dgo_eraseobjall_callback,
		    (struct db_i	*dbip,
		     Tcl_Interp		*interp,
		     struct directory	*dp,
		     int		notify));
GED_EXPORT BU_EXTERN(void dgo_eraseobjpath,
		    ());
GED_EXPORT BU_EXTERN(void dgo_impending_wdb_close,
		    ());
GED_EXPORT BU_EXTERN(int dgo_invent_solid,
		    ());
GED_EXPORT BU_EXTERN(void dgo_notify,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp));
GED_EXPORT BU_EXTERN(void dgo_notifyWdb,
		    (struct rt_wdb *wdbp,
		     Tcl_Interp    *interp));
GED_EXPORT BU_EXTERN(void dgo_zapall,
		    ());

/* defined in nirt.c */
GED_EXPORT BU_EXTERN(int	dgo_nirt_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));
GED_EXPORT BU_EXTERN(int	dgo_vnirt_cmd,
		    (struct dg_obj *dgop,
		     struct view_obj *vop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));

/* defined in qray.c */
GED_EXPORT BU_EXTERN(int	dgo_qray_cmd,
		    (struct dg_obj	*dgop,
		     Tcl_Interp		*interp,
		     int		argc,
		     char 		**argv));
GED_EXPORT BU_EXTERN(void dgo_init_qray,
		    (struct dg_obj *dgop));


/* defined in bigE.c */
GED_EXPORT BU_EXTERN(int	dg_E_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));


/* functions defined in vdraw.c */
GED_EXPORT BU_EXTERN(int	vdraw_cmd,
		    (struct dg_obj *dgop,
		     Tcl_Interp *interp,
		     int argc,
		     char **argv));

__END_DECLS

#endif /* __DG_H__ */

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
