/*                   G E D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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

#ifndef LIBGED_GED_PRIVATE_H
#define LIBGED_GED_PRIVATE_H

#include "common.h"

#include <time.h>

#include "rt/db4.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "ged.h"

__BEGIN_DECLS

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#define _GED_V4_MAXNAME NAMESIZE
#define _GED_TERMINAL_WIDTH 80
#define _GED_COLUMNS ((_GED_TERMINAL_WIDTH + _GED_V4_MAXNAME - 1) / _GED_V4_MAXNAME)

#define _GED_CPEVAL      0
#define _GED_LISTPATH    1
#define _GED_LISTEVAL    2
#define _GED_EVAL_ONLY   3

#define _GED_SHADED_MODE_UNSET -1
#define _GED_WIREFRAME          0
#define _GED_SHADED_MODE_BOTS   1
#define _GED_SHADED_MODE_ALL    2
#define _GED_BOOL_EVAL          3
#define _GED_HIDDEN_LINE        4
#define _GED_SHADED_MODE_EVAL   5

#define _GED_DRAW_WIREFRAME 1
#define _GED_DRAW_NMG_POLY  3

#define _GED_TREE_AFLAG 0x01
#define _GED_TREE_CFLAG 0x02

/* Container for defining sub-command structures */
#define _GED_FUNTAB_UNLIMITED -1

#define DG_GED_MAX 2047.0
#define DG_GED_MIN -2048.0

struct _ged_funtab {
    char *ft_name;
    char *ft_parms;
    char *ft_comment;
    int (*ft_func)();
    int ft_min;
    int ft_max;
    int tcl_converted;
};


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
    uint32_t magic;  /* add this so a pointer to the struct and a pointer to any of its active elements will differ */
    struct ged *gedp;
    struct display_list *gdlp;
    int wireframe_color_override;
    int wireframe_color[3];
    int draw_nmg_only;
    int nmg_triangulate;
    int draw_wireframes;
    int draw_normals;
    int draw_solid_lines_only;
    int draw_no_surfaces;
    int shade_per_vertex_normals;
    int draw_edge_uses;
    int fastpath_count;			/* statistics */
    int do_not_draw_nmg_solids_during_debugging;
    struct bn_vlblock *draw_edge_uses_vbp;
    int shaded_mode_override;
    fastf_t transparency;
    int dmode;
    int hiddenLine;
    struct solid *freesolid;
    /* bigE related members */
    struct application *ap;
    struct bu_ptbl leaf_list;
    struct rt_i *rtip;
    time_t start_time;
    time_t etime;
    long nvectors;
    int do_polysolids;
    int num_halfs;
    int autoview;
    size_t bot_threshold;
};


void vls_col_item(struct bu_vls *str, const char *cp);
void vls_col_eol(struct bu_vls *str);

/* defined in facedef.c */
extern int edarb_facedef(void *data, int argc, const char *argv[]);

/* defined in ged.c */
extern void _ged_print_node(struct ged *gedp,
			    struct directory *dp,
			    size_t pathpos,
			    int indentSize,
			    char prefix,
			    unsigned flags,
			    int displayDepth,
			    int currdisplayDepth);
extern struct db_i *_ged_open_dbip(const char *filename,
				   int existing_only);

/* defined in comb.c */
extern struct directory *_ged_combadd(struct ged *gedp,
				      struct directory *objp,
				      char *combname,
				      int region_flag,
				      db_op_t relation,
				      int ident,
				      int air);
extern int _ged_combadd2(struct ged *gedp,
			 char *combname,
			 int argc,
			 const char *argv[],
			 int region_flag,
			 db_op_t relation,
			 int ident,
			 int air);

/* defined in display_list.c */
extern void _dl_eraseAllNamesFromDisplay(struct bu_list *hdlp, struct db_i *dbip,
	        void (*callback)(unsigned int, int),
					  const char *name,
					  const int skip_first, struct solid *freesolid);
extern void _dl_eraseAllPathsFromDisplay(struct bu_list *hdlp, struct db_i *dbip,
	        void (*callback)(unsigned int, int),
					  const char *path,
					  const int skip_first, struct solid *freesolid);
extern void _dl_freeDisplayListItem(struct db_i *dbip,
	        void (*callback)(unsigned int, int),
				     struct display_list *gdlp, struct solid *freesolid);
extern int headsolid_splitGDL(struct bu_list *hdlp, struct db_i *dbip, struct display_list *gdlp, struct db_full_path *path);
extern int dl_bounding_sph(struct bu_list *hdlp, vect_t *min, vect_t *max, int pflag);
/* Returns a bu_ptbl of all solids referenced by the display list */
extern struct bu_ptbl *dl_get_solids(struct display_list *gdlp);

extern void dl_add_path(struct display_list *gdlp, int dashflag, int transparency, int dmode, int hiddenLine, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, unsigned char *wireframe_color_override, void (*callback)(struct solid *), struct solid *freesolid);

extern int dl_redraw(struct display_list *gdlp, struct db_i *dbip, struct db_tree_state *tsp, struct bview *gvp, void (*callback)(struct display_list *), int skip_subtractions);
extern union tree * append_solid_to_display_list(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *client_data);
int dl_set_illum(struct display_list *gdlp, const char *obj, int illum);
void dl_set_flag(struct bu_list *hdlp, int flag);
void dl_set_wflag(struct bu_list *hdlp, int wflag);
void dl_zap(struct bu_list *hdlp, struct db_i *dbip, void (*callback)(unsigned int, int), struct solid *freesolid);
int dl_how(struct bu_list *hdlp, struct bu_vls *vls, struct directory **dpp, int both);
void dl_plot(struct bu_list *hdlp, FILE *fp, mat_t model2view, int floating, mat_t center, fastf_t scale, int Three_D, int Z_clip);
void dl_png(struct bu_list *hdlp, mat_t model2view, fastf_t perspective, vect_t eye_pos, size_t size, size_t half_size, unsigned char **image);

#define PS_COORD(_x) ((int)((_x)+2048))
#define PS_COLOR(_c) ((_c)*(1.0/255.0))
void dl_ps(struct bu_list *hdlp, FILE *fp, int border, char *font, char *title, char *creator, int linewidth, fastf_t scale, int xoffset, int yoffset, mat_t model2view, fastf_t perspective, vect_t eye_pos, float red, float green, float blue);


void dl_print_schain(struct bu_list *hdlp, struct db_i *dbip, int lvl, int vlcmds, struct bu_vls *vls);

void dl_bitwise_and_fullpath(struct bu_list *hdlp, int flag);

void dl_write_animate(struct bu_list *hdlp, FILE *fp);

int dl_select(struct bu_list *hdlp, mat_t model2view, struct bu_vls *vls, double vx, double vy, double vwidth, double vheight, int rflag);
int dl_select_partial(struct bu_list *hdlp, mat_t model2view, struct bu_vls *vls, double vx, double vy, double vwidth, double vheight, int rflag);
void dl_set_transparency(struct bu_list *hdlp, struct directory **dpp, double transparency, void (*callback)(struct display_list *));

enum otype {
    OTYPE_DXF = 1,
    OTYPE_OBJ,
    OTYPE_SAT,
    OTYPE_STL
};
void _ged_bot_dump(struct directory *dp, struct rt_bot_internal *bot, FILE *fp, int fd, const char *file_ext, const char *db_name);
void dl_botdump(struct bu_list *hdlp, struct db_i *dbip, FILE *fp, int fd, char *file_ext, int output_type, int *red, int *green, int *blue, fastf_t *alpha);


/* defined in draw.c */
extern void _ged_cvt_vlblock_to_solids(struct ged *gedp,
				       struct bn_vlblock *vbp,
				       const char *name,
				       int copy);
extern int _ged_drawtrees(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  int kind,
			  struct _ged_client_data *_dgcdp);
extern void _ged_drawH_part2(int dashflag,
			     struct bu_list *vhead,
			     const struct db_full_path *pathp,
			     struct db_tree_state *tsp,
			     struct _ged_client_data *dgcdp);

/* defined in edbot.c */
extern int _ged_select_botpts(struct ged *gedp,
			      struct rt_bot_internal *botip,
			      double vx,
			      double vy,
			      double vwidth,
			      double vheight,
			      double vminz,
			      int rflag);


/* defined in editit.c */
extern int _ged_editit(const char *editstring,
		       const char *file);

/* defined in erase.c */
extern void _ged_eraseobjpath(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      const int noisy,
			      const int all,
			      const int skip_first);
extern void _ged_eraseobjall(struct ged *gedp,
			     struct directory **dpp,
			     int skip_first);
extern void _ged_eraseobj(struct ged *gedp,
			  struct directory **dpp,
			  int skip_first);

/* defined in get_comb.c */
extern void _ged_vls_print_matrix(struct bu_vls *vls,
				  matp_t matrix);

extern int _ged_get_obj_bounds2(struct ged *gedp,
				int argc,
				const char *argv[],
				struct _ged_trace_data *gtdp,
				point_t rpp_min,
				point_t rpp_max);

/*  defined in get_solid_kp.c */
extern int _ged_get_solid_keypoint(struct ged *const gedp,
				   fastf_t *const pt,
				   const struct rt_db_internal *const ip,
				   const fastf_t *const mat);

/* defined in how.c */
extern struct directory **_ged_build_dpp(struct ged *gedp,
					 const char *path);

/* defined in list.c */
extern void _ged_do_list(struct ged *gedp,
			 struct directory *dp,
			 int verbose);

/* defined in loadview.c */
extern vect_t _ged_eye_model;
extern mat_t _ged_viewrot;
extern struct ged *_ged_current_gedp;
extern int _ged_cm_vsize(const int argc,
			 const char **argv);
extern int _ged_cm_eyept(const int argc,
			 const char **argv);
extern int _ged_cm_lookat_pt(const int argc,
			     const char **argv);
extern int _ged_cm_vrot(const int argc,
			const char **argv);
extern int _ged_cm_orientation(const int argc,
			       const char **argv);
extern int _ged_cm_set(const int argc,
		       const char **argv);
extern int _ged_cm_null(const int argc,
			const char **argv);


/* defined in ls.c */
extern void _ged_vls_col_pr4v(struct bu_vls *vls,
			      struct directory **list_of_names,
			      size_t num_in_list,
			      int no_decorate);
extern struct directory ** _ged_getspace(struct db_i *dbip,
					 size_t num_entries);

/* defined in preview.c */
extern void _ged_setup_rt(struct ged *gedp,
			  char **vp,
			  int printcmd);

/* defined in red.c */

extern char _ged_tmpfil[MAXPATHLEN];


/* defined in rt.c */
extern void _ged_rt_set_eye_model(struct ged *gedp,
				  vect_t eye_model);
extern int _ged_run_rt(struct ged *gdp);
extern void _ged_rt_write(struct ged *gedp,
			  FILE *fp,
			  vect_t eye_model);
extern void _ged_rt_output_handler(ClientData clientData,
				   int mask);

/* defined in rtcheck.c */
extern void _ged_wait_status(struct bu_vls *logstr,
			     int status);

/* defined in rotate_eto.c */
extern int _ged_rotate_eto(struct ged *gedp,
			   struct rt_eto_internal *eto,
			   const char *attribute,
			   matp_t rmat);

/* defined in rotate_extrude.c */
extern int _ged_rotate_extrude(struct ged *gedp,
			       struct rt_extrude_internal *extrude,
			       const char *attribute,
			       matp_t rmat);

/* defined in rotate_hyp.c */
extern int _ged_rotate_hyp(struct ged *gedp,
			   struct rt_hyp_internal *hyp,
			   const char *attribute,
			   matp_t rmat);

/* defined in rotate_tgc.c */
extern int _ged_rotate_tgc(struct ged *gedp,
			   struct rt_tgc_internal *tgc,
			   const char *attribute,
			   matp_t rmat);

/* defined in scale_ehy.c */
extern int _ged_scale_ehy(struct ged *gedp,
			  struct rt_ehy_internal *ehy,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_ell.c */
extern int _ged_scale_ell(struct ged *gedp,
			  struct rt_ell_internal *ell,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_epa.c */
extern int _ged_scale_epa(struct ged *gedp,
			  struct rt_epa_internal *epa,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_eto.c */
extern int _ged_scale_eto(struct ged *gedp,
			  struct rt_eto_internal *eto,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_extrude.c */
extern int _ged_scale_extrude(struct ged *gedp,
			      struct rt_extrude_internal *extrude,
			      const char *attribute,
			      fastf_t sf,
			      int rflag);

/* defined in scale_hyp.c */
extern int _ged_scale_hyp(struct ged *gedp,
			  struct rt_hyp_internal *hyp,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in edit_metaball.c */
extern int _ged_scale_metaball(struct ged *gedp,
			       struct rt_metaball_internal *mbip,
			       const char *attribute,
			       fastf_t sf,
			       int rflag);
extern int _ged_set_metaball(struct ged *gedp,
			     struct rt_metaball_internal *mbip,
			     const char *attribute,
			     fastf_t sf);

/* defined in scale_part.c */
extern int _ged_scale_part(struct ged *gedp,
			   struct rt_part_internal *part,
			   const char *attribute,
			   fastf_t sf,
			   int rflag);

/* defined in edpipe.c */
extern int _ged_scale_pipe(struct ged *gedp,
			   struct rt_pipe_internal *pipe_internal,
			   const char *attribute,
			   fastf_t sf,
			   int rflag);

/* defined in scale_rhc.c */
extern int _ged_scale_rhc(struct ged *gedp,
			  struct rt_rhc_internal *rhc,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_rpc.c */
extern int _ged_scale_rpc(struct ged *gedp,
			  struct rt_rpc_internal *rpc,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_superell.c */
extern int _ged_scale_superell(struct ged *gedp,
			       struct rt_superell_internal *superell,
			       const char *attribute,
			       fastf_t sf,
			       int rflag);

/* defined in scale_tgc.c */
extern int _ged_scale_tgc(struct ged *gedp,
			  struct rt_tgc_internal *tgc,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_tor.c */
extern int _ged_scale_tor(struct ged *gedp,
			  struct rt_tor_internal *tor,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in tops.c */
struct directory **
_ged_dir_getspace(struct db_i *dbip,
		  int num_entries);

/* defined in translate_extrude.c */
extern int _ged_translate_extrude(struct ged *gedp,
				  struct rt_extrude_internal *extrude,
				  const char *attribute,
				  vect_t tvec,
				  int rflag);

/* defined in translate_tgc.c */
extern int _ged_translate_tgc(struct ged *gedp,
			      struct rt_tgc_internal *tgc,
			      const char *attribute,
			      vect_t tvec,
			      int rflag);

/* defined in vutil.c */
extern void _ged_mat_aet(struct bview *gvp);
extern int _ged_do_rot(struct ged *gedp,
		       char coord,
		       mat_t rmat,
		       int (*func)());
extern int _ged_do_slew(struct ged *gedp,
			vect_t svec);
extern int _ged_do_tra(struct ged *gedp,
		       char coord,
		       vect_t tvec,
		       int (*func)());

/* Internal implementation of ged_results - since the
 * details of the struct are not for public access,
 * the real definition of the struct goes here.  The public
 * header has only the notion of a ged_results structure.*/
struct ged_results {
	struct bu_ptbl *results_tbl;
};

/* defined in ged_util.c */

/* Called by ged_init */
extern int _ged_results_init(struct ged_results *results);

/* This function adds a copy of result_string into the results container.
 * To duplicate a VLS string, use bu_vls_addr to wrap the vls before
 * passing it to _ged_results_add, e.g.:
 *
 * _ged_results_add(gedp->ged_results, bu_vls_addr(my_vls_ptr));
 *
 */
extern int _ged_results_add(struct ged_results *results, const char *result_string);

extern int _ged_brep_to_csg(struct ged *gedp, const char *obj_name, int verify);
extern int _ged_brep_tikz(struct ged *gedp, const char *obj_name, const char *outfile);

__END_DECLS

#endif /* LIBGED_GED_PRIVATE_H */

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
