/*                   G E D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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

struct vd_curve {
    struct bu_list      l;
    char                vdc_name[RT_VDRW_MAXNAME+1];    /**< @brief name array */
    long                vdc_rgb;        /**< @brief color */
    struct bu_list      vdc_vhd;        /**< @brief head of list of vertices */
};
#define VD_CURVE_NULL   ((struct vd_curve *)NULL)

/* Need to figure out a good general scheme for object vlist data management.
 * In MGED, one of the biggest advantages of oed vs. sed is that oed can do
 * editing while reusing the wireframes from an already drawn model - i.e., it
 * doesn't have to use any additional memory to store alternate copies.  oed
 * operations alter a matrix, which is then use to alter how the EXISTING solid
 * vlists are drawn for the edited geometry.  However, a drawback of using this
 * ability is you lose improved wireframe generation if that is an option for
 * the primitive in question.  If LoD is not in effect BoTs won't need any new
 * wireframes, but (for example) if a TOR is scaled the existing wireframe may
 * be coarse in the view when representing the new shape without regenerating
 * the wireframe.
 *
 * A wholesale replacement of the existing wireframe isn't an option, since
 * other instances in the view may be using the original and not being edited,
 * so what we really need is an ability to optionally generate a new wireframe
 * only in the editing cases where that makes sense, and a way for the
 * primitive edits (sed or oed) to notify the app when it should be using
 * either the edit local vlist or altering an existing solid with the matrix.
 *
 * MGED handles this reuse by having a set of bv_scene_obj containers stored in
 * the ged_drawable struct's gd_headDisplay list.  It then calls the
 * dm_draw_head_dl function twice, once for non editing objs and once for
 * edited objs, with a flag set to filter on whether to draw bv_scene_objs that
 * are or are not being edited.  The solids in turn have a flag that is set to
 * indicate whether or not they are part of the current edit.  (Incidently,
 * that part-of-edit-op flag setting is the reason that multiply drawn objects
 * are rejected for editing - doing so would require the drawing code to be
 * able to put up both the edit and non-edited versions of a solid
 * simultaneously, and this mechanism isn't flexible enough for that.)
 *
 * For the new draw code, I think BViewState::redraw will need to check if
 * bv_scene_objs in the view are overridden by an rt_edit generated bv_scene_obj.
 * If we allow an rt_edit callback to return a bv_scene_obj from the app (if it
 * supplies one) or an internally generated one (if the edit op indicates it is
 * necessary or the parent simply doesn't have one) we might be able to come up
 * with a way to allow for this sort of reuse...
 */

struct ged_drawable {
    struct bu_list              *gd_headDisplay;        /**< @brief  head of display list */
    struct bu_list              *gd_headVDraw;          /**< @brief  head of vdraw list */
    struct vd_curve             *gd_currVHead;          /**< @brief  current vdraw head */

    ged_drawable_notify_func_t  gd_rtCmdNotify; /**< @brief  function called when rt command completes */

    int                         gd_uplotOutputMode;     /**< @brief  output mode for unix plots */

    /* qray state */
    struct bu_vls               gd_qray_basename;       /**< @brief  basename of query ray vlist */
    struct bu_vls               gd_qray_script;         /**< @brief  query ray script */
    char                        gd_qray_effects;        /**< @brief  t for text, g for graphics or b for both */
    int                         gd_qray_cmd_echo;       /**< @brief  0 - don't echo command, 1 - echo command */
    struct ged_qray_fmt         *gd_qray_fmts;
    struct ged_qray_color       gd_qray_odd_color;
    struct ged_qray_color       gd_qray_even_color;
    struct ged_qray_color       gd_qray_void_color;
    struct ged_qray_color       gd_qray_overlap_color;
    int                         gd_shaded_mode;         /**< @brief  1 - draw bots shaded by default */
};


#ifdef __cplusplus

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

	// Persisting state between loadview and preview
	// commands and subcommands.
	vect_t ged_eye_model = VINIT_ZERO;
	mat_t ged_viewrot = MAT_INIT_ZERO;
};

#else

#define Ged_Internal void

#endif

struct ged_impl {
    uint32_t magic;
    Ged_Internal *i;

    struct ged_drawable *ged_gdp;
};

__BEGIN_DECLS

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#define _GED_V4_MAXNAME NAMESIZE
/* Default libged column width assumption */
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

/* defined in editit.c */
GED_EXPORT extern int _ged_editit(struct ged *gedp,
	               const char *editstring,
		       const char *file);

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
