/*                           M G E D . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/mged.h
 *
 * This file contains all of the definitions local to MGED
 *
 * V E R Y   I M P O R T A N T   N O T I C E ! ! !
 *
 * Many people in the computer graphics field use post-multiplication,
 * (thanks to Newman and Sproull) with row vectors, i.e.:
 *
 *	view_vec = model_vec * T
 *
 * However, in the GED system, the more traditional representation of
 * column vectors is used (ref: Gwyn).  Therefore, when transforming a
 * vector by a matrix, pre-multiplication is used, i.e.:
 *
 *	view_vec = model2view_mat * model_vec
 *
 * Furthermore, additional transformations are multiplied on the left, i.e.:
 *
 *	vec'  =  T1 * vec
 *	vec'' =  T2 * T1 * vec  =  T2 * vec'
 *
 * The most notable implication of this is the location of the
 * "delta" (translation) values in the matrix, i.e.:
 *
 *       x'     (R0   R1   R2   Dx)     (x)
 *       y' =   (R4   R5   R6   Dy)  X  (y)
 *       z'     (R8   R9   R10  Dz)     (z)
 *       w'     (0    0    0   1/s)     (w)
 *
 * This of course requires that the rotation portion be computed
 * using somewhat different formulas (see buildHrot for both kinds).
 *
 */

#ifndef MGED_MGED_H
#define MGED_MGED_H

#include "common.h"


#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

#include "bu/parallel.h"
#include "bu/list.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "wdb.h"

/* Needed to define struct bv_scene_obj */
#include "bv/defines.h"

__BEGIN_DECLS
#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#  define HAVE_X11_TYPES 1
#endif
__END_DECLS

#include "mged_ecmds.h"
#include "mged_impl.h"
#include "edfunctab.h"

__BEGIN_DECLS

#define MGED_DB_NAME "db"
#define MGED_INMEM_NAME ".inmem"


#define MGED_CMD_MAGIC 0x4D474544 /**< MGED */
#define MGED_CK_CMD(_bp) BU_CKMAG(_bp, MGED_CMD_MAGIC , "cmdtab")

#define MGED_STATE_MAGIC 0x4D474553 /**< MGEDS*/
#define MGED_CK_STATE(_bp) BU_CKMAG(_bp, MGED_STATE_MAGIC , "mged_state")

/* Forward declaration */
struct mged_state;

/* Tolerances */
struct mged_tol {
    double abs_tol; /* abs surface tolerance */
    double rel_tol; /* rel surface tolerance */
    double nrm_tol; /* surface normal tolerance */
    struct bn_tol tol;  /* calculation tolerance */
    struct bg_tess_tol ttol; /* XXX needs to replace abs_tol, et al. */
};


typedef int (*tcl_func_ptr)(ClientData, Tcl_Interp *, int, const char *[]);

struct cmdtab {
    uint32_t magic;
    const char *name;
    tcl_func_ptr tcl_func;
    ged_func_ptr ged_func;
    struct mged_state *s;
};

#include "./menu.h"
#include "./mged_dm.h" /* _view_state */

struct mged_edit_state {

    // main global editing state (ugh)
    int global_editing_state;

    // Rotate
    vect_t edit_absolute_model_rotate;
    vect_t edit_absolute_object_rotate;
    vect_t edit_absolute_view_rotate;
    vect_t last_edit_absolute_model_rotate;
    vect_t last_edit_absolute_object_rotate;
    vect_t last_edit_absolute_view_rotate;
    vect_t edit_rate_model_rotate;
    vect_t edit_rate_object_rotate;
    vect_t edit_rate_view_rotate;
    int edit_rateflag_model_rotate;
    int edit_rateflag_object_rotate;
    int edit_rateflag_view_rotate;

    // Translate
    vect_t edit_rate_model_tran;
    vect_t edit_rate_view_tran;
    int edit_rateflag_model_tran;
    int edit_rateflag_view_tran;

    // Scale
    fastf_t edit_rate_scale;
    int edit_rateflag_scale;

    /* The "accumulation" solid rotation matrix and scale factor */
    fastf_t acc_sc_obj;		/* accumulate global object scale factor */
    fastf_t acc_sc[3];		/* accumulate local object scale factors */

    // Origin
    char edit_rate_model_origin;
    char edit_rate_object_origin;
    char edit_rate_view_origin;


    // DM pointers - used by the editing code to stash current dm pointers for
    // later restoration when editing.  Not 100% sure yet what the purpose is -
    // seems to be allowing for the possibility of a change of mged_curr_dm
    // mid-edit?
    struct mged_dm *edit_rate_mr_dm;
    struct mged_dm *edit_rate_or_dm;
    struct mged_dm *edit_rate_vr_dm;
    struct mged_dm *edit_rate_mt_dm;
    struct mged_dm *edit_rate_vt_dm;

    // TODO - can we eliminate these?
    int e_edclass;		/* type of editing class for this solid */
    int e_type;			/* COMGEOM solid type */

};


// Callback registration mechanism.  sedit() is going to become
// something like rt_solid_edit_process().  We need general methods for:
//
// mged_print_results
// set_e_axes_pos
// replot_editing_solid
// view_update
//
// in addition to allowing ECMD specific callback registrations.  Simplest thing
// to do is probably assign the "generic" operations above some specific numbers
// and supply a 0 obj type so we can just use the same mechanism for everything.
extern int mged_state_clbk_set(struct mged_state *s, int obj_type, int ed_cmd, int menu_cmd, int mode, bu_clbk_t f, void *d);
extern int mged_state_clbk_get(bu_clbk_t *f, void **d, struct mged_state *s, int obj_type, int ed_cmd, int menu_cmd, int mode);
extern int mged_edit_clbk_sync(struct rt_solid_edit *se, struct mged_state *s);


extern int mged_mmenu_set(int ac, const char **av, void *d, void *ms);

extern int arb_setup_rotface_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_bot_mode_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_bot_orient_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_bot_thick_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_bot_flags_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_bot_fmode_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_bot_pickt_multihit_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_nmg_edebug_clbk(int ac, const char **av, void *d, void *d2);
extern int ecmd_extrude_skt_name_clbk(int ac, const char **av, void *d, void *d2);


/* global application state */
struct mged_state_impl;
struct mged_state {
    uint32_t magic;
    struct mged_state_impl *i;
    struct ged *gedp;
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct mged_tol tol;
    Tcl_Interp *interp;

    /* >0 means interactive. Gets set to 0 if there's libdm graphics support,
     * and forced with -c option. */
    int classic_mged;
    int interactive; /* for pr_prompt */

    /* Prompt and input lines */
    size_t input_str_index;
    struct bu_vls input_str;
    struct bu_vls input_str_prefix;
    struct bu_vls scratchline;
    struct bu_vls mged_prompt;

    /* Display related */
    struct mged_dm *mged_curr_dm;
    char *dpy_string;
    struct bu_list *vlfree;

    /* Editing related */
    struct mged_edit_state edit_state;
    struct rt_solid_edit *s_edit;

    /* Checked by numerous functions to indicate truthfully whether the
     * views need to be redrawn. */
    int update_views;
};
extern struct mged_state *MGED_STATE;

extern struct mged_state *
mged_state_create(void);
extern void
mged_state_destroy(struct mged_state *s);

void get_solid_keypoint(struct rt_solid_edit *s, point_t *pt, const char **strp, struct rt_db_internal *ip, fastf_t *mat);

/**
 * Definitions.
 *
 * Solids are defined in "model space".  The screen is in "view
 * space".  The visible part of view space is -1.0 <= x, y, z <= +1.0
 *
 * The transformation from the origin of model space to the origin of
 * view space (the "view center") is contained in the matrix
 * "toViewcenter".  The viewing rotation is contained in the "Viewrot"
 * matrix.  The viewscale factor (for [15] use) is kept in the float
 * "Viewscale".
 *
 * model2view = Viewscale * Viewrot * toViewcenter;
 *
 * model2view is the matrix going from model space coordinates to the
 * view coordinates, and view2model is the inverse.  It is recomputed
 * by new_mats() only.
 *
 * CHANGE matrix.  Defines the change between the un-edited and the
 * present state in the edited solid or combination.
 *
 * model2objview = modelchanges * model2view
 *
 * For object editing and solid edit, model2objview translates from
 * model space to view space with all the modelchanges too.
 *
 * These are allocated storage in dozoom.c
 */

/* defined in mged.c */
extern jmp_buf jmp_env;

#include "./mged_wdb.h"

/*
 * GED functions referenced in more than one source file:
 */

extern void mged_setup(struct mged_state *s);
extern void mged_global_variable_teardown(struct mged_state *s); /* cmd.c */
extern void dozoom(struct mged_state *s, int which_eye);
extern void mged_finish(struct mged_state *s, int exitcode);
extern void slewview(struct mged_state *s, vect_t view_pos);
extern void moveHinstance(struct mged_state *s, struct directory *cdp, struct directory *dp, matp_t xlate);
extern void moveHobj(struct mged_state *s, struct directory *dp, matp_t xlate);
extern void quit(struct mged_state *s);
extern void refresh(struct mged_state *s);
extern void setview(struct mged_state *s, double a1, double a2, double a3);
extern void adcursor(struct mged_state *s);
extern void get_attached(struct mged_state *s);
extern void (*cur_sigint)(int);	/* Current SIGINT status */
extern void sig2(int);
extern void sig3(int);

/* mged.c */
extern void mged_view_callback(struct bview *gvp, void *clientData);

/* buttons.c */
extern void button(struct mged_state *s, int bnum);
extern void press(char *str);
extern char *label_button(struct mged_state *s, int bnum);
extern int not_state(struct mged_state *s, int desired, char *str);
extern int chg_state(struct mged_state *s, int from, int to, char *str);
extern void state_err(struct mged_state *s, char *str);

extern int invoke_db_wrapper(Tcl_Interp *interpreter, int argc, const char *argv[]);

/* history.c */
void history_record(struct bu_vls *cmdp, int64_t start, int64_t finish, int status); /* Either CMD_OK or CMD_BAD */
void history_setup(void);

/* defined in usepen.c */
#define RARROW   001
#define UARROW   002
#define SARROW   004
#define ROTARROW 010 /* Object rotation enabled */

/* Ew.  Globals. */
extern int movedir;  /* RARROW | UARROW | SARROW | ROTARROW */
extern struct display_list *illum_gdlp; /* Pointer to solid in solid table to be illuminated */
extern struct bv_scene_obj *illump; /* == 0 if none, else points to ill. solid */
extern int ipathpos; /* path index of illuminated element */
extern int sedraw; /* apply solid editing changes */
extern int edobj; /* object editing options */


/* Flags for line type decisions */
#define ROOT 0
#define INNER 1

/**
 * Editor States
 */
extern char *state_str[]; /* identifying strings */
#define ST_VIEW		1 /* Viewing only */
#define ST_S_PICK	2 /* Picking for Solid Edit */
#define ST_S_EDIT	3 /* Solid Editing */
#define ST_O_PICK	4 /* Picking for Object Edit */
#define ST_O_PATH	5 /* Path select for Object Edit */
#define ST_O_EDIT	6 /* Object Editing */
#define ST_S_VPICK	7 /* Vertex Pick */
#define ST_S_NO_EDIT	8 /* Solid edit without Rotate, translate, or scale */

#define ERROR_RECOVERY_SUGGESTION "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n"


/* Check if database pointer is NULL */
#define CHECK_DBI_NULL \
    if (s->dbip == DBI_NULL) { \
	Tcl_AppendResult(s->interp, "A database is not open!\n", (char *)NULL); \
	return TCL_ERROR; \
    }

/* Check if the database is read only, and if so return TCL_ERROR */
#define CHECK_READ_ONLY	\
    if (s->dbip->dbi_read_only) { \
	Tcl_AppendResult(s->interp, "Sorry, this database is READ-ONLY\n", (char *)NULL); \
	return TCL_ERROR; \
    }


struct mged_hist {
    struct bu_list l;
    struct bu_vls mh_command;
    int64_t mh_start;
    int64_t mh_finish;
    int mh_status;
};


/* internal variables related to the command window(s) */
struct cmd_list {
    struct bu_list l;
    struct mged_dm *cl_tie;        /* the drawing window that we're tied to */
    struct mged_hist *cl_cur_hist;
    struct bu_vls cl_more_default;
    struct bu_vls cl_name;
    int cl_quote_string;
};
#define CMD_LIST_NULL ((struct cmd_list *)NULL)

/* Ew.  Globals. */
/* defined in cmd.c */
extern struct cmd_list head_cmd_list;
extern struct cmd_list *curr_cmd_list;

#if !defined(HAVE_TK) && !defined(TK_WINDOW_TYPEDEF)
typedef void *Tk_Window;
#  define TK_WINDOW_TYPEDEF 1
#endif
extern Tk_Window tkwin; /* in cmd.c */

/* Ew.  Globals. */
/* defined in rtif.c */
extern struct run_rt head_run_rt;

#define MGED_PROMPT "\rmged> "

/* Command return codes */
#define CMD_OK 919
#define CMD_BAD 920
#define CMD_MORE 921
#define MORE_ARGS_STR "more arguments needed::"


/* attach.c */
int mged_attach(struct mged_state *s, const char *wp_name, int argc, const char *argv[]);
void mged_link_vars(struct mged_dm *p);
void mged_slider_free_vls(struct mged_dm *p);
int gui_setup(struct mged_state *s, const char *dstr);


/* chgview.c */
int mged_svbase(struct mged_state *s);
void size_reset(struct mged_state *s);
void solid_list_callback(struct mged_state *s);

extern void view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2); /* defined in chgview.c */
extern void view_ring_destroy(struct mged_dm *dlp);

/* cmd.c */
int cmdline(struct mged_state *s, struct bu_vls *vp, int record);
int mged_print_result(int, const char **, void *, void*);
int mged_print_str(int, const char **, void *, void*);
int mged_view_update(int, const char **, void *, void*);
int mged_view_set_flag(int, const char **, void *, void*);
int gui_output(void *clientData, void *str);
void mged_pr_output(Tcl_Interp *interp);

/* columns.c */
void vls_col_item(struct bu_vls *str, const char *cp);
void vls_col_eol(struct bu_vls *str);

/* dodraw.c */
int replot_modified_solid(struct mged_state *s, struct bv_scene_obj *sp, struct rt_db_internal *ip, const mat_t mat);
int replot_original_solid(struct mged_state *s, struct bv_scene_obj *sp);
void add_solid_path_to_result(Tcl_Interp *interpreter, struct bv_scene_obj *sp);
int redraw_visible_objects(struct mged_state *s);

/* dozoom.c */
void createDLists(void *, struct bu_list *hdlp);
void createDListSolid(void *, struct bv_scene_obj *);
void createDListAll(void *, struct display_list *);
void freeDListsAll(void *, unsigned int dlist, int range);

/* edarb.c */
int editarb(struct rt_solid_edit *s, vect_t pos_model);
/* Ew.  Global. */
extern int newedge;	/* new edge for arb editing */

/* edars.c */
void find_ars_nearest_pnt(int *crv, int *col, struct rt_ars_internal *ars, point_t pick_pt, vect_t dir);

/* f_db.c */
struct mged_opendb_ctx {
    int argc;
    const char **argv;
    int force_create;
    int no_create;
    int created_new_db;
    int ret;
    int ged_ret;
    Tcl_Interp *interpreter;
    struct db_i *old_dbip;
    int post_open_cnt;
    struct mged_state *s;
    int init_flag; /* >0 means in initialization stage */
    int db_upgrade; /* 0 (default) no upgrade, 1 upgrade */
    int db_version; /* force creation of specific database version (default = BRLCAD_DB_FORMAT_LATEST) */
    int db_warn; /* 0 (default) no warn, 1 warn */
};
extern struct mged_opendb_ctx mged_global_db_ctx;

/* mged.c */
void new_edit_mats(struct mged_state *s);
void new_mats(struct mged_state *s);
void pr_beep(void);

void pr_prompt(struct mged_state *s);

/* grid.c */
extern void round_to_grid(struct mged_state *s, fastf_t *view_dx, fastf_t *view_dy);
extern void snap_keypoint_to_grid(struct mged_state *s);
extern void snap_view_center_to_grid(struct mged_state *s);
extern void snap_to_grid(struct mged_state *s, fastf_t *mx, fastf_t *my);
extern void snap_view_to_grid(struct mged_state *s, fastf_t view_dx, fastf_t view_dy);
extern void draw_grid(struct mged_state *s);

/* predictor.c */
extern void predictor_frame(struct mged_state *s);
extern void predictor_init(struct mged_state *s);

/* usepen.c */
void wrt_view(struct mged_state *s, mat_t out, const mat_t change, const mat_t in);
void wrt_point(mat_t out, const mat_t change, const mat_t in, const point_t point);

/* tedit.c */
int get_editor_string(struct mged_state *s, struct bu_vls *editstring);

/* titles.c */
void create_text_overlay(struct mged_state *s, struct bu_vls *vp);
void dotitles(struct mged_state *s, struct bu_vls *overlay_vls);

/* rect.c */
void zoom_rect_area(struct mged_state *);
void paint_rect_area(struct mged_state *);
void rt_rect_area(struct mged_state *);
void draw_rect(struct mged_state *);
void set_rect(const struct bu_structparse *, const char *, void *, const char *, void *);
void rect_view2image(struct mged_state *);
void rect_image2view(struct mged_state *);
void rb_set_dirty_flag(const struct bu_structparse *, const char *, void *, const char *, void *);

/* edsol.c */
void vls_solid(struct mged_state *s, struct bu_vls *vp, struct rt_db_internal *ip, const mat_t mat);
void transform_editing_solid(
    struct mged_state *s,
    struct rt_db_internal *os,		/* output solid */
    const mat_t mat,
    struct rt_db_internal *is,		/* input solid */
    int freedbi);
int replot_editing_solid(int, const char **, void *, void *);
void sedit_abs_scale(struct mged_state *s);
void sedit_accept(struct mged_state *s);
void sedit_mouse(struct mged_state *s, const vect_t mousevec);
void sedit_reject(struct mged_state *s);
void oedit_abs_scale(struct mged_state *s);
void oedit_accept(struct mged_state *s);
void oedit_reject(struct mged_state *s);
void objedit_mouse(struct mged_state *s, const vect_t mousevec);
void label_edited_solid(struct mged_state *s, int *num_lines, point_t *lines, struct rt_point_labels pl[], int max_pl, const mat_t xform, struct rt_db_internal *ip);
void init_oedit(struct mged_state *s);
void init_sedit(struct mged_state *s);

int set_e_axes_pos(int , const char **, void *, void *);

/* share.c */
void usurp_all_resources(struct mged_dm *dlp1, struct mged_dm *dlp2);

/* set.c */
extern void set_absolute_tran(struct mged_state *);
extern void set_absolute_view_tran(struct mged_state *);
extern void set_absolute_model_tran(struct mged_state *);
extern void fbserv_set_port(const struct bu_structparse *, const char *, void *, const char *, void *);
extern void set_scroll_private(const struct bu_structparse *, const char *, void *, const char *, void *);
extern void mged_variable_setup(struct mged_state *s);

/* scroll.c */
void set_scroll(struct mged_state *);
int scroll_select(struct mged_state *s, int pen_x, int pen_y, int do_func);
int scroll_display(struct mged_state *s, int y_top);

/* vparse.c */
extern void mged_vls_struct_parse(struct mged_state *s, struct bu_vls *vls, const char *title, struct bu_structparse *how_to_parse, const char *structp, int argc, const char *argv[]); /* defined in vparse.c */
extern void mged_vls_struct_parse_old(struct mged_state *s, struct bu_vls *vls, const char *title, struct bu_structparse *how_to_parse, char *structp, int argc, const char *argv[]);

/* mater.c */
void mged_color_soltab(struct mged_state *s);

int Wdb_Init(Tcl_Interp *interp);
int wdb_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]);
void wdb_deleteProc(ClientData clientData);

__END_DECLS

#endif  /* MGED_MGED_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
