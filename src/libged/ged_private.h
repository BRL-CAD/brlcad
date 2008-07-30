/*                   G E D _ P R I V A T E . H
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
/** @file ged_private.h
 *
 * Private header for libged.
 *
 */

#ifndef __GED_PRIVATE_H__
#define __GED_PRIVATE_H__

#include "common.h"

#include <time.h>

#include "ged.h"
#include "db.h"

__BEGIN_DECLS

#define GED_V4_MAXNAME	NAMESIZE
#define GED_TERMINAL_WIDTH 80
#define GED_COLUMNS ((GED_TERMINAL_WIDTH + GED_V4_MAXNAME - 1) / GED_V4_MAXNAME)

#define GED_MAX_LEVELS 12
#define GED_CPEVAL	0
#define GED_LISTPATH	1
#define GED_LISTEVAL	2
#define GED_EVAL_ONLY	3

/*
 * rt_comb_ifree() should NOT be used here because
 * it doesn't know how to free attributes.
 * rt_db_free_internal() should be used instead.
 */
#define USE_RT_COMB_IFREE 0

#define GED_WIREFRAME 0
#define GED_SHADED_MODE_BOTS 1
#define GED_SHADED_MODE_ALL 2
#define GED_BOOL_EVAL 3

struct ged_client_data {
    struct ged	       	*gedp;
    int			wireframe_color_override;
    int			wireframe_color[3];
    int			draw_nmg_only;
    int			nmg_triangulate;
    int			draw_wireframes;
    int			draw_normals;
    int			draw_solid_lines_only;
    int			draw_no_surfaces;
    int			shade_per_vertex_normals;
    int			draw_edge_uses;
    int			fastpath_count;			/* statistics */
    int			do_not_draw_nmg_solids_during_debugging;
    struct bn_vlblock	*draw_edge_uses_vbp;
    int			shaded_mode_override;
    fastf_t		transparency;
    int			dmode;
    /* bigE related members */
    struct application	*ap;
    struct bu_ptbl	leaf_list;
    struct rt_i		*rtip;
    time_t		start_time;
    time_t		etime;
    long		nvectors;
    int			do_polysolids;
    int			num_halfs;
};

struct ged_rt_client_data {
    struct ged_run_rt 	*rrtp;
    struct ged	       	*gedp;
};

struct ged_trace_data {
    struct ged	      *gtd_gedp;
    struct directory  *gtd_path[GED_MAX_LEVELS];
    struct directory  *gtd_obj[GED_MAX_LEVELS];
    mat_t	      gtd_xform;
    int		      gtd_objpos;
    int		      gtd_prflag;
    int		      gtd_flag;
};

/* defined in globals.c */
extern struct solid FreeSolid;

/* defined in ged.c */
BU_EXTERN (void ged_print_node,
	   (struct ged		*gedp,
	    register struct directory *dp,
	    int			pathpos,
	    int			indentSize,
	    char		prefix,
	    int			cflag,
	    int                 displayDepth,
	    int                 currdisplayDepth));
BU_EXTERN (struct db_i *ged_open_dbip,
	   (const char *filename));


/* defined in comb.c */
BU_EXTERN (struct directory *ged_combadd,
	   (struct ged			*gedp,
	    register struct directory	*objp,
	    char			*combname,
	    int				region_flag,
	    int				relation,
	    int				ident,
	    int				air));

/* defined in draw.c */
BU_EXTERN (void ged_color_soltab,
	   (struct solid *hsp));

BU_EXTERN (void ged_cvt_vlblock_to_solids,
	   (struct ged *gedp,
	    struct bn_vlblock *vbp,
	    char *name,
	    int copy));
BU_EXTERN (int ged_invent_solid,
	   (struct ged		*gedp,
	    char		*name,
	    struct bu_list	*vhead,
	    long int		rgb,
	    int			copy,
	    fastf_t		transparency,
	    int			dmode));


/* defined in erase.c */
BU_EXTERN (void ged_eraseobjpath,
	   (struct ged	*gedp,
	    int		argc,
	    const char	*argv[],
	    int		noisy,
	    int		all));
BU_EXTERN (void ged_eraseobjall,
	   (struct ged			*gedp,
	    register struct directory	**dpp));
BU_EXTERN (void ged_eraseobj,
	   (struct ged			*gedp,
	    register struct directory	**dpp));

/* defined in get_obj_bounds.c */
BU_EXTERN (int ged_get_obj_bounds,
	   (struct ged		*gedp,
	    int			argc,
	    const char		*argv[],
	    int			use_air,
	    point_t		rpp_min,
	    point_t		rpp_max));

BU_EXTERN (int ged_get_obj_bounds2,
	   (struct ged			*gedp,
	    int				argc,
	    const char			*argv[],
	    struct ged_trace_data	*gtdp,
	    point_t			rpp_min,
	    point_t			rpp_max));

/* defined in how.c */
BU_EXTERN (struct directory **ged_build_dpp,
	   (struct ged *gedp,
	    const char *path));

/* defined in list.c */
BU_EXTERN(void ged_do_list,
	  (struct ged			*gedp,
	   register struct directory	*dp,
	   int				verbose));

/* defined in ls.c */
BU_EXTERN(void ged_vls_col_pr4v,
	  (struct bu_vls	*vls,
	   struct directory	**list_of_names,
	   int			num_in_list,
	   int			no_decorate));
BU_EXTERN(void ged_vls_long_dpp,
	  (struct bu_vls	*vls,
	   struct directory	**list_of_names,
	   int			num_in_list,
	   int			aflag,		/* print all objects */
	   int			cflag,		/* print combinations */
	   int			rflag,		/* print regions */
	   int			sflag));	/* print solids */
BU_EXTERN(void ged_vls_line_dpp,
	  (struct bu_vls	*vls,
	   struct directory	**list_of_names,
	   int			num_in_list,
	   int			aflag,	/* print all objects */
	   int			cflag,	/* print combinations */
	   int			rflag,	/* print regions */
	   int			sflag));	/* print solids */
BU_EXTERN(struct directory ** ged_getspace,
	  (struct db_i	*dbip,
	   register int	num_entries));

/* defined in rt.c */
BU_EXTERN (void ged_rt_set_eye_model,
	   (struct ged *gedp,
	    vect_t eye_model));
#if GED_USE_RUN_RT
BU_EXTERN (int ged_run_rt,
	   (struct ged *gdp));
BU_EXTERN (void ged_rt_write,
	   (struct ged *gedp,
	    FILE *fp,
	    vect_t eye_model));
BU_EXTERN (void ged_rt_output_handler,
	   (ClientData clientData,
	    int	 mask));
BU_EXTERN (int ged_build_tops,
	   (struct ged	*gedp,
	    struct solid	*hsp,
	    char		**start,
	    register char	**end));
#endif

/* defined in rtcheck.c */
BU_EXTERN (void ged_wait_status,
	   (struct bu_vls *log,
	    int status));


/* defined in tops.c */
struct directory **
ged_dir_getspace(struct db_i	*dbip,
		 register int	num_entries);

/* defined in trace.c */
BU_EXTERN (void ged_trace,
	   (register struct directory	*dp,
	    int				pathpos,
	    const mat_t			old_xlate,
	    struct ged_trace_data	*gtdp));

/* defined in vutil.c */
BU_EXTERN (void ged_view_update,
	   (struct ged_view *gvp));
BU_EXTERN (void ged_mat_aet,
	   (struct ged_view *gvp));
BU_EXTERN (int ged_do_rot,
	   (struct ged	*gedp,
	    char	coord,
	    mat_t	rmat,
	    int		(*func)()));
BU_EXTERN (int ged_do_slew,
	   (struct ged	*gedp,
	    vect_t	svec));
BU_EXTERN (int ged_do_tra,
	   (struct ged	*gedp,
	    char	coord,
	    vect_t	tvec,
	    int		(*func)()));
BU_EXTERN (int ged_do_zoom,
	   (struct ged	*gedp,
	    fastf_t	sf));
BU_EXTERN (void ged_persp_mat,
	   (fastf_t *m,
	    fastf_t fovy,
	    fastf_t aspect,
	    fastf_t near1,
	    fastf_t far1,
	    fastf_t backoff));
BU_EXTERN (void ged_mike_persp_mat,
	   (fastf_t *pmat,
	    const fastf_t *eye));


__END_DECLS

#endif /* __GED_PRIVATE_H__ */

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
