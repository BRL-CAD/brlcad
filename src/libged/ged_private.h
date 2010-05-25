/*                   G E D _ P R I V A T E . H
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
/** @file ged_private.h
 *
 * Private header for libged.
 *
 */

#ifndef __GED_PRIVATE_H__
#define __GED_PRIVATE_H__

#include "common.h"

#include <time.h>

#include "db.h"
#include "mater.h"
#include "rtgeom.h"
#include "ged.h"

__BEGIN_DECLS

#define _GED_V4_MAXNAME	NAMESIZE
#define _GED_TERMINAL_WIDTH 80
#define _GED_COLUMNS ((_GED_TERMINAL_WIDTH + _GED_V4_MAXNAME - 1) / _GED_V4_MAXNAME)

#define _GED_MAX_LEVELS 12
#define _GED_CPEVAL	0
#define _GED_LISTPATH	1
#define _GED_LISTEVAL	2
#define _GED_EVAL_ONLY	3
        
#define _GED_WIREFRAME        0
#define _GED_SHADED_MODE_BOTS 1
#define _GED_SHADED_MODE_ALL  2
#define _GED_BOOL_EVAL        3

struct _ged_id_names {
    struct bu_list l;
    struct bu_vls name;		/**< name associated with region id */
};

struct _ged_id_to_names {
    struct bu_list l;
    int id;				/**< starting id (i.e. region id or air code) */
    struct _ged_id_names headName;	/**< head of list of names */
};

struct _ged_client_data {
    struct ged			*gedp;
    struct ged_display_list	*gdlp;
    int				wireframe_color_override;
    int				wireframe_color[3];
    int				draw_nmg_only;
    int				nmg_triangulate;
    int				draw_wireframes;
    int				draw_normals;
    int				draw_solid_lines_only;
    int				draw_no_surfaces;
    int				shade_per_vertex_normals;
    int				draw_edge_uses;
    int				fastpath_count;			/* statistics */
    int				do_not_draw_nmg_solids_during_debugging;
    struct bn_vlblock		*draw_edge_uses_vbp;
    int				shaded_mode_override;
    fastf_t			transparency;
    int				dmode;
    int				hiddenLine;
    /* bigE related members */
    struct application		*ap;
    struct bu_ptbl		leaf_list;
    struct rt_i			*rtip;
    time_t			start_time;
    time_t			etime;
    long			nvectors;
    int				do_polysolids;
    int				num_halfs;
};

struct _ged_trace_data {
    struct ged	      *gtd_gedp;
    struct directory  *gtd_path[_GED_MAX_LEVELS];
    struct directory  *gtd_obj[_GED_MAX_LEVELS];
    mat_t	      gtd_xform;
    int		      gtd_objpos;
    int		      gtd_prflag;
    int		      gtd_flag;
};

/* defined in globals.c */
extern struct solid _FreeSolid;

/* defined in ged.c */
BU_EXTERN (void _ged_print_node,
	   (struct ged		*gedp,
	    struct directory *dp,
	    int			pathpos,
	    int			indentSize,
	    char		prefix,
	    int			cflag,
	    int                 displayDepth,
	    int                 currdisplayDepth));
BU_EXTERN (struct db_i *_ged_open_dbip,
	   (const char *filename,
	    int existing_only));

/* defined in color.c */
BU_EXTERN (void _ged_color_putrec,
	   (struct ged			*gedp,
	    struct mater	*mp));
	    
BU_EXTERN (void _ged_color_zaprec,
	   (struct ged			*gedp,
	    struct mater	*mp));

/* defined in comb.c */
BU_EXTERN (struct directory *_ged_combadd,
	   (struct ged			*gedp,
	    struct directory	*objp,
	    char			*combname,
	    int				region_flag,
	    int				relation,
	    int				ident,
	    int				air));

/* defined in draw.c */
BU_EXTERN (void _ged_cvt_vlblock_to_solids,
	   (struct ged *gedp,
	    struct bn_vlblock *vbp,
	    char *name,
	    int copy));
BU_EXTERN (int _ged_invent_solid,
	   (struct ged		*gedp,
	    char		*name,
	    struct bu_list	*vhead,
	    long int		rgb,
	    int			copy,
	    fastf_t		transparency,
	    int			dmode));
BU_EXTERN (int _ged_drawtrees,
	   (struct ged *gedp,
	    int argc,
	    const char *argv[],
	    int kind,
	    struct _ged_client_data *_dgcdp));
BU_EXTERN (void _ged_drawH_part2,
	   (int dashflag,
	    struct bu_list *vhead,
	    const struct db_full_path *pathp,
	    struct db_tree_state *tsp,
	    struct solid *existing_sp,
	    struct _ged_client_data *dgcdp));

/* defined in editit.c */
BU_EXTERN (int _ged_editit, 
	   (const char *editstring, 
	    const char *file));

/* defined in erase.c */
BU_EXTERN (void _ged_eraseobjpath,
	   (struct ged	*gedp,
	    int		argc,
	    const char	*argv[],
	    const int	noisy,
	    const int	all,
	    const int	skip_first));
BU_EXTERN (void _ged_eraseobjall,
	   (struct ged			*gedp,
	    struct directory	**dpp,
	    int				skip_first));
BU_EXTERN (void _ged_eraseobj,
	   (struct ged			*gedp,
	    struct directory	**dpp,
	    int				skip_first));
BU_EXTERN (void _ged_eraseAllNamesFromDisplay,
	   (struct ged			*gedp,
	    const char			*name,
	    const int			skip_first));
BU_EXTERN (void _ged_eraseAllPathsFromDisplay,
	   (struct ged			*gedp,
	    const char			*path,
	    const int			skip_first));
BU_EXTERN (void _ged_freeDisplayListItem,
	   (struct ged			*gedp,
	    struct ged_display_list *gdlp));


/* defined in get_comb.c */
BU_EXTERN(void _ged_vls_print_matrix,
	  (struct bu_vls *vls,
	   matp_t matrix));

/* defined in get_obj_bounds.c */
BU_EXTERN (int _ged_get_obj_bounds,
	   (struct ged		*gedp,
	    int			argc,
	    const char		*argv[],
	    int			use_air,
	    point_t		rpp_min,
	    point_t		rpp_max));

BU_EXTERN (int _ged_get_obj_bounds2,
	   (struct ged			*gedp,
	    int				argc,
	    const char			*argv[],
	    struct _ged_trace_data	*gtdp,
	    point_t			rpp_min,
	    point_t			rpp_max));

/* defined in how.c */
BU_EXTERN (struct directory **_ged_build_dpp,
	   (struct ged *gedp,
	    const char *path));

/* defined in list.c */
BU_EXTERN(void _ged_do_list,
	  (struct ged			*gedp,
	   struct directory	*dp,
	   int				verbose));

/* defined in loadview.c */
extern vect_t _ged_eye_model;
extern mat_t _ged_viewrot;
extern struct ged *_ged_current_gedp;
BU_EXTERN (int _ged_cm_vsize,
	   (int argc,
	    char **argv));
BU_EXTERN (int _ged_cm_eyept,
	   (int argc,
	    char **argv));
BU_EXTERN (int _ged_cm_lookat_pt,
	   (int argc,
	    char **argv));
BU_EXTERN (int _ged_cm_vrot,
	   (int argc,
	    char **argv));
BU_EXTERN (int _ged_cm_orientation,
	   (int argc,
	    char **argv));
BU_EXTERN (int _ged_cm_set,
	   (int argc,
	    char **argv));
BU_EXTERN (int _ged_cm_null,
	   (int argc,
	    char **argv));


/* defined in ls.c */
BU_EXTERN(void _ged_vls_col_pr4v,
	  (struct bu_vls	*vls,
	   struct directory	**list_of_names,
	   int			num_in_list,
	   int			no_decorate));
BU_EXTERN(struct directory ** _ged_getspace,
	  (struct db_i	*dbip,
	   int	num_entries));

/* defined in preview.c */
BU_EXTERN (void _ged_setup_rt,
	   (struct ged *gedp,
	    char **vp,
	    int printcmd));

/* defined in red.c */

extern char _ged_tmpfil[MAXPATHLEN];
extern const char _ged_tmpcomb[16];
extern char _delims[];

BU_EXTERN(int _ged_make_tree,
	  (struct ged *gedp,
	   struct rt_comb_internal *comb,
	   struct directory *dp,
	   size_t node_count,
	   const char *old_name,
	   const char *new_name,
	   struct rt_tree_array *rt_tree_array,
	   int tree_index));
BU_EXTERN(const char *_ged_save_comb,
	  (struct ged *gedp,
	   struct directory *dpold));
BU_EXTERN(void _ged_restore_comb,
	  (struct ged *gedp,
	   struct directory *dp,
	   const char *name));
BU_EXTERN(void _ged_print_matrix,
	  (FILE *fp, matp_t matrix));

/* defined in rt.c */
BU_EXTERN (void _ged_rt_set_eye_model,
	   (struct ged *gedp,
	    vect_t eye_model));
BU_EXTERN (int _ged_run_rt,
	   (struct ged *gdp));
BU_EXTERN (void _ged_rt_write,
	   (struct ged *gedp,
	    FILE *fp,
	    vect_t eye_model));
BU_EXTERN (void _ged_rt_output_handler,
	   (ClientData clientData,
	    int	 mask));

/* defined in rtcheck.c */
BU_EXTERN (void _ged_wait_status,
	   (struct bu_vls *logstr,
	    int status));

/* defined in rotate_eto.c */
BU_EXTERN (int _ged_rotate_eto,
	   (struct ged *gedp,
	    struct rt_eto_internal *eto,
	    const char *attribute,
	    matp_t rmat));

/* defined in rotate_extrude.c */
BU_EXTERN (int _ged_rotate_extrude,
	   (struct ged *gedp,
	    struct rt_extrude_internal *extrude,
	    const char *attribute,
	    matp_t rmat));

/* defined in rotate_hyp.c */
BU_EXTERN (int _ged_rotate_hyp,
	   (struct ged *gedp,
	    struct rt_hyp_internal *hyp,
	    const char *attribute,
	    matp_t rmat));

/* defined in rotate_tgc.c */
BU_EXTERN (int _ged_rotate_tgc,
	   (struct ged *gedp,
	    struct rt_tgc_internal *tgc,
	    const char *attribute,
	    matp_t rmat));

/* defined in scale_ehy.c */
BU_EXTERN (int _ged_scale_ehy,
	   (struct ged *gedp,
	    struct rt_ehy_internal *ehy,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_ell.c */
BU_EXTERN (int _ged_scale_ell,
	   (struct ged *gedp,
	    struct rt_ell_internal *ell,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_epa.c */
BU_EXTERN (int _ged_scale_epa,
	   (struct ged *gedp,
	    struct rt_epa_internal *epa,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_eto.c */
BU_EXTERN (int _ged_scale_eto,
	   (struct ged *gedp,
	    struct rt_eto_internal *eto,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_extrude.c */
BU_EXTERN (int _ged_scale_extrude,
	   (struct ged *gedp,
	    struct rt_extrude_internal *extrude,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_hyp.c */
BU_EXTERN (int _ged_scale_hyp,
	   (struct ged *gedp,
	    struct rt_hyp_internal *hyp,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_part.c */
BU_EXTERN (int _ged_scale_part,
	   (struct ged *gedp,
	    struct rt_part_internal *part,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_rhc.c */
BU_EXTERN (int _ged_scale_rhc,
	   (struct ged *gedp,
	    struct rt_rhc_internal *rhc,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_rpc.c */
BU_EXTERN (int _ged_scale_rpc,
	   (struct ged *gedp,
	    struct rt_rpc_internal *rpc,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_superell.c */
BU_EXTERN (int _ged_scale_superell,
	   (struct ged *gedp,
	    struct rt_superell_internal *superell,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_tgc.c */
BU_EXTERN (int _ged_scale_tgc,
	   (struct ged *gedp,
	    struct rt_tgc_internal *tgc,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in scale_tor.c */
BU_EXTERN (int _ged_scale_tor,
	   (struct ged *gedp,
	    struct rt_tor_internal *tor,
	    const char *attribute,
	    fastf_t sf,
	    int rflag));

/* defined in tops.c */
struct directory **
_ged_dir_getspace(struct db_i	*dbip,
		 int	num_entries);

/* defined in trace.c */
BU_EXTERN (void _ged_trace,
	   (struct directory	*dp,
	    int				pathpos,
	    const mat_t			old_xlate,
	    struct _ged_trace_data	*gtdp));

/* defined in translate_extrude.c */
BU_EXTERN (int _ged_translate_extrude,
	   (struct ged *gedp,
	    struct rt_extrude_internal *extrude,
	    const char *attribute,
	    vect_t tvec,
	    int rflag));

/* defined in translate_tgc.c */
BU_EXTERN (int _ged_translate_tgc,
	   (struct ged *gedp,
	    struct rt_tgc_internal *tgc,
	    const char *attribute,
	    vect_t tvec,
	    int rflag));

/* defined in vutil.c */
BU_EXTERN (void _ged_mat_aet,
	   (struct ged_view *gvp));
BU_EXTERN (int _ged_do_rot,
	   (struct ged	*gedp,
	    char	coord,
	    mat_t	rmat,
	    int		(*func)()));
BU_EXTERN (int _ged_do_slew,
	   (struct ged	*gedp,
	    vect_t	svec));
BU_EXTERN (int _ged_do_tra,
	   (struct ged	*gedp,
	    char	coord,
	    vect_t	tvec,
	    int		(*func)()));
BU_EXTERN (int _ged_do_zoom,
	   (struct ged	*gedp,
	    fastf_t	sf));

/* defined in ged_util.c */
BU_EXTERN (int _ged_results_append_str,
	   (struct ged *gedp,
	    char *result_string));
	    
BU_EXTERN (int _ged_results_append_vls,
	   (struct ged *gedp,
	    struct bu_vls *result_vls));

BU_EXTERN (int _ged_results_clear,
	   (struct ged *gedp));
	
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
