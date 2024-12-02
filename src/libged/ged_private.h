/*                   G E D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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

#include "bu/avs.h"
#include "bu/cmd.h"
#include "bu/opt.h"
#include "bg/spsr.h"
#include "bg/trimesh.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "bv/defines.h"
#include "bv/util.h"
#include "ged.h"

#ifdef __cplusplus

#include <map>
#include <stack>
#include <string>

class Ged_Internal {
    public:
	struct ged *gedp;
	std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> cmd_prerun_clbk;
	std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> cmd_during_clbk;
	std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> cmd_postrun_clbk;
	std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> cmd_linger_clbk;

	std::map<bu_clbk_t, int> clbk_recursion_depth_cnt;
	std::map<std::string, int> cmd_recursion_depth_cnt;

	std::stack<std::string> exec_stack;

	std::map<std::string, void *> dm_map;
};

#else

#define Ged_Internal void

#endif

struct ged_impl {
    uint32_t magic;
    Ged_Internal *i;
};

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
#define _GED_WIREFRAME_EVAL     6

#define _GED_DRAW_WIREFRAME 1
#define _GED_DRAW_NMG_POLY  3

#define _GED_TREE_AFLAG 0x01
#define _GED_TREE_CFLAG 0x02

/* Container for defining sub-command structures */
#define _GED_FUNTAB_UNLIMITED -1

#define DG_GED_MAX 2047.0
#define DG_GED_MIN -2048.0

/* Default libged column width assumption */
#define GED_TERMINAL_WIDTH 80

#define GED_CMD_RECURSION_LIMIT 100

/* Callback management related structures */
#define GED_REFRESH_FUNC_NULL ((ged_refresh_func_t)0)
#define GED_CREATE_VLIST_SOLID_FUNC_NULL ((ged_create_vlist_solid_func_t)0)
#define GED_CREATE_VLIST_DISPLAY_LIST_FUNC_NULL ((ged_create_vlist_display_list_func_t)0)
#define GED_DESTROY_VLIST_FUNC_NULL ((ged_destroy_vlist_func_t)0)

/* Common flags used by multiple GED commands for help printing */
#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

// Private bookkeeping structure used by callback handlers
struct ged_callback_state {
    int ged_refresh_handler_cnt;
    int ged_output_handler_cnt;
    int ged_create_vlist_scene_obj_callback_cnt;
    int ged_create_vlist_display_list_callback_cnt;
    int ged_destroy_vlist_callback_cnt;
    int ged_io_handler_callback_cnt;
};

/* These are wrapper functions, that should be called by libged code instead of
 * the callback pointers supplied by the client (if any).  They will do the boilerplate
 * bookkeeping and validation required by callbacks.
 */
GED_EXPORT extern void ged_refresh_cb(struct ged *);
GED_EXPORT extern void ged_output_handler_cb(struct ged *, char *);
GED_EXPORT extern void ged_create_vlist_solid_cb(struct ged *, struct bv_scene_obj *);
GED_EXPORT extern void ged_create_vlist_display_list_cb(struct ged *, struct display_list *);
GED_EXPORT extern void ged_destroy_vlist_cb(struct ged *, unsigned int, int);
GED_EXPORT extern void ged_io_handler_cb(struct ged *, void *, int);

struct ged_solid_data {
    struct display_list *gdlp;
    int draw_solid_lines_only;
    int wireframe_color_override;
    int wireframe_color[3];
    fastf_t transparency;
    int dmode;
    struct bview *v;
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
    struct rt_wdb *wdbp;
    struct display_list *gdlp;
    int fastpath_count;			/* statistics */
    struct bv_vlblock *draw_edge_uses_vbp;
    struct bview *v;



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
    int nmg_fast_wireframe_draw;

    // Debugging plotting specific options.  These don't actually belong with
    // the drawing routines at all - they are analogous to the brep debugging
    // plotting routines, and belong with nmg/bot/etc. plot subcommands.
    int draw_nmg_only;
    int nmg_triangulate;
    int draw_normals;
    int draw_no_surfaces;
    int shade_per_vertex_normals;
    int draw_edge_uses;
    int do_not_draw_nmg_solids_during_debugging;


    struct bv_obj_settings vs;
};


/* Data for tree walk */
struct draw_data_t {
    struct db_i *dbip;
    struct bv_scene_group *g;
    struct bview *v;
    struct bv_obj_settings *vs;
    const struct bn_tol *tol;
    const struct bg_tess_tol *ttol;
    struct bu_color c;
    int color_inherit;
    int bool_op;
    struct resource *res;
    struct bv_mesh_lod_context *mesh_c;

    /* To avoid the need for multiple subtree walking
     * functions, we also set up to support a bounding
     * only walk. */
    int bound_only;
    int skip_subtractions;
    int have_bbox;
    point_t min;
    point_t max;
#ifdef __cplusplus
    std::map<struct directory *, fastf_t> *s_size;
#endif
};

GED_EXPORT void draw_gather_paths(struct db_full_path *path, mat_t *curr_mat, void *client_data);

GED_EXPORT void vls_col_item(struct bu_vls *str, const char *cp);
GED_EXPORT void vls_col_eol(struct bu_vls *str);

/* defined in ged.c */
GED_EXPORT extern struct db_i *_ged_open_dbip(const char *filename,
				   int existing_only);

/* defined in comb.c */
GED_EXPORT extern struct directory *_ged_combadd(struct ged *gedp,
				      struct directory *objp,
				      char *combname,
				      int region_flag,
				      db_op_t relation,
				      int ident,
				      int air);
GED_EXPORT extern int _ged_combadd2(struct ged *gedp,
			 char *combname,
			 int argc,
			 const char *argv[],
			 int region_flag,
			 db_op_t relation,
			 int ident,
			 int air,
			 matp_t m,
			 int validate);

/* defined in display_list.c */
GED_EXPORT extern void _dl_eraseAllNamesFromDisplay(struct ged *gedp, const char *name, const int skip_first);
GED_EXPORT extern void _dl_eraseAllPathsFromDisplay(struct ged *gedp, const char *path, const int skip_first);
extern void _dl_freeDisplayListItem(struct ged *gedp, struct display_list *gdlp);
GED_EXPORT extern int dl_bounding_sph(struct bu_list *hdlp, vect_t *min, vect_t *max, int pflag);

GED_EXPORT extern void color_soltab(struct bv_scene_obj *sp);

/* defined in draw.c */
GED_EXPORT extern void _ged_cvt_vlblock_to_solids(struct ged *gedp,
				       struct bv_vlblock *vbp,
				       const char *name,
				       int copy);
GED_EXPORT extern int _ged_drawtrees(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  int kind,
			  struct _ged_client_data *_dgcdp);

/* defined in editit.c */
GED_EXPORT extern int _ged_editit(const char *editstring,
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

GED_EXPORT extern int _ged_get_obj_bounds2(struct ged *gedp,
				int argc,
				const char *argv[],
				struct _ged_trace_data *gtdp,
				point_t rpp_min,
				point_t rpp_max);

/*  defined in get_solid_kp.c */
GED_EXPORT extern int _ged_get_solid_keypoint(struct ged *const gedp,
				   fastf_t *const pt,
				   const struct rt_db_internal *const ip,
				   const fastf_t *const mat);

/* defined in how.c */
GED_EXPORT extern struct directory **_ged_build_dpp(struct ged *gedp,
					 const char *path);

/* defined in list.c */
GED_EXPORT extern void _ged_do_list(struct ged *gedp,
			 struct directory *dp,
			 int verbose);

/* defined in loadview.c
 *
 * TODO - I'm thinking these shouldn't exist... need to figure out how they're
 * being used and make them go away.
 **/
GED_EXPORT extern vect_t _ged_eye_model;
GED_EXPORT extern mat_t _ged_viewrot;
GED_EXPORT extern struct ged *_ged_current_gedp;

/* defined in rt.c */
GED_EXPORT extern void
_ged_rt_output_handler(void *clientData, int mask);

GED_EXPORT extern void _ged_rt_set_eye_model(struct ged *gedp,
				  vect_t eye_model);
GED_EXPORT extern int _ged_run_rt(
	struct ged *gdp,
       	int cmd_len,
       	const char **gd_rt_cmd,
       	int argc,
       	const char **argv,
       	int stdout_is_txt,
       	int *pid,
	bu_clbk_t end_clbk,
	void *end_clbk_data);

GED_EXPORT extern void _ged_rt_write(struct ged *gedp,
			  FILE *fp,
			  vect_t eye_model,
			  int argc,
			  const char **argv);

/* defined in rtcheck.c */
GED_EXPORT extern void _ged_wait_status(struct bu_vls *logstr,
			     int status);

/* defined in ged_util.cpp */
GED_EXPORT struct directory **
_ged_dir_getspace(struct db_i *dbip,
		  size_t num_entries);

/* defined in vutil.c */
GED_EXPORT extern int _ged_do_rot(struct ged *gedp,
		       char coord,
		       mat_t rmat,
		       int (*func)(struct ged *, char, char, mat_t));
GED_EXPORT extern int _ged_do_slew(struct ged *gedp,
			vect_t svec);
GED_EXPORT extern int _ged_do_tra(struct ged *gedp,
		       char coord,
		       vect_t tvec,
		       int (*func)(struct ged *, char, vect_t));

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
GED_EXPORT extern int _ged_results_add(struct ged_results *results, const char *result_string);

/* defined in ged_util.c */

/**
 * Given two pointers to pointers to directory entries, do a string
 * compare on the respective names and return that value.
 */
GED_EXPORT extern int cmpdirname(const void *a, const void *b, void *arg);

/**
 * Given two pointers to pointers to directory entries, compare
 * the dp->d_len sizes.
 */
GED_EXPORT extern int cmpdlen(const void *a, const void *b, void *arg);


/**
 * Given a pointer to a list of pointers to names and the number of
 * names in that list, sort and print that list in column order over
 * four columns.
 */
GED_EXPORT extern void _ged_vls_col_pr4v(struct bu_vls *vls,
			      struct directory **list_of_names,
			      size_t num_in_list,
			      int no_decorate,
			      int ssflag);


GED_EXPORT extern int invent_solid(struct ged *gedp, char *name, struct bu_list *vhead, long int rgb, int copy, fastf_t transparency, int dmode, int csoltab);

#if 0
/**
 * Characterize a path specification (search command style).
 *
 * Return flags characterizing the path specifier, as defined below:
 */
#define GED_PATHSPEC_INVALID   1ULL << 1
#define GED_PATHSPEC_SPECIFIC  1ULL << 2
#define GED_PATHSPEC_LOCAL     1ULL << 3
#define GED_PATHSPEC_FLAT      1ULL << 4

GED_EXPORT extern unsigned long long
_ged_characterize_path_spec(struct bu_vls *normalized,
	struct ged *gedp, const char *pathspec
	);
#endif

/**
 * Routine for generic command help printing.
 */
GED_EXPORT extern void _ged_cmd_help(struct ged *gedp, const char *usage, struct bu_opt_desc *d);

/* Option for verbosity variable setting */
GED_EXPORT extern int _ged_vopt(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/* Function to read in density information, either from a file or from the
 * database itself. Implements the following priority order:
 *
 * 1. filename explicitly supplied to function.
 * 2. Density information stored in the .g database.
 * 3. A .density file in the same directory as the .g file
 * 4. A .density file in the user's HOME directory
 *
 * Note that this function does *not* store the density information in the
 * current .g once read.  To store density information in a .g file, use
 * the "mater -d load" command.
 *
 */
GED_EXPORT extern int _ged_read_densities(struct analyze_densities **dens, char **den_src, struct ged *gedp, const char *filename, int fault_tolerant);

#define GED_DB_DENSITY_OBJECT "_DENSITIES" 

/**
 * Routine for checking argc/argv list for existing objects and sorting anything
 * that isn't a valid object to the end of the list.  Returns the number of
 * argv entries where db_lookup failed.  Optionally takes a pointer to a directory
 * pointer array and will stash found directory pointers there - caller must make
 * sure the allocated array is large enough to hold up to argc pointers.
 */
GED_EXPORT extern int
_ged_sort_existing_objs(struct db_i *dbip, int argc, const char *argv[], struct directory **dpa);


GED_EXPORT extern int ged_view_data_lines(struct ged *gedp, int argc, const char *argv[]);


GED_EXPORT extern void ged_push_scene_obj(struct ged *gedp, struct bv_scene_obj *sp);
GED_EXPORT extern struct bv_scene_obj *ged_pop_scene_obj(struct ged *gedp);

GED_EXPORT extern int
_ged_subcmd_help(struct ged *gedp, struct bu_opt_desc *gopts, const struct bu_cmdtab *cmds,
	const char *cmdname, const char *cmdargs, void *gd, int argc, const char **argv);

GED_EXPORT extern int
_ged_subcmd_exec(struct ged *gedp, struct bu_opt_desc *gopts, const struct bu_cmdtab *cmds,
	const char *cmdname, const char *cmdargs, void *gd, int argc, const char **argv,
       	int help, int cmd_pos);


// TODO:  alternative approach to the command structure supported by
// _ged_subcmd_help - if we successfully migrate all the uses of the above
// functions, rename this from subcmd2 to subcmd...
//
// Prototyping with edit/edit2.cpp
#ifdef __cplusplus

#include <string>
#include <map>

class ged_subcmd {
    public:
	virtual std::string usage()   { return std::string(""); }
	virtual std::string purpose() { return std::string(""); }
	virtual int exec(struct ged *, void *, int, const char **) { return BRLCAD_ERROR; }
};

GED_EXPORT extern int
_ged_subcmd2_help(struct ged *gedp, struct bu_opt_desc *gopts, std::map<std::string, ged_subcmd *> &subcmds, const char *cmdname, const char *cmdargs, int argc, const char **argv);

#endif

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
