/*                           M G E D . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#  define HAVE_X11_TYPES 1
#endif
#include "bu/parallel.h"
#include "bu/list.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "wdb.h"

/* Needed to define struct bv_scene_obj */
#include "bv/defines.h"


#define MGED_DB_NAME "db"
#define MGED_INMEM_NAME ".inmem"


#define MGED_CMD_MAGIC 0x4D474544 /**< MGED */
#define MGED_CK_CMD(_bp) BU_CKMAG(_bp, MGED_CMD_MAGIC , "cmdtab")

#define MGED_STATE_MAGIC 0x4D474553 /**< MGEDS*/
#define MGED_CK_STATE(_bp) BU_CKMAG(_bp, MGED_STATE_MAGIC , "mged_state")


/* Tolerances */
struct mged_tol {
    double abs_tol; /* abs surface tolerance */
    double rel_tol; /* rel surface tolerance */
    double nrm_tol; /* surface normal tolerance */
    struct bn_tol tol;  /* calculation tolerance */
    struct bg_tess_tol ttol; /* XXX needs to replace abs_tol, et al. */
};


/* global application state */
struct mged_state {
    uint32_t magic;
    struct ged *gedp;
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct mged_tol tol;
    Tcl_Interp *interp;

    /* >0 means interactive. Gets set to 0 if there's libdm graphics support,
     * and forced with -c option. */
    int classic_mged;

    /* Prompt and input lines */
    size_t input_str_index;
    struct bu_vls input_str;
    struct bu_vls input_str_prefix;
    struct bu_vls scratchline;
    struct bu_vls mged_prompt;
};
extern struct mged_state *MGED_STATE;

typedef int (*tcl_func_ptr)(ClientData, Tcl_Interp *, int, const char *[]);

struct cmdtab {
    uint32_t magic;
    const char *name;
    tcl_func_ptr tcl_func;
    ged_func_ptr ged_func;
    struct mged_state *s;
};

#include "./mged_dm.h" /* _view_state */
/* Needed to define struct menu_item */
#include "./menu.h"

/* initialization states */
extern char *dpy_string;
extern int mged_db_upgrade;
extern int mged_db_version;
extern int mged_db_warn;

/*
 * All GED files are stored in a fixed base unit (MM).  These factors
 * convert database unit to local (or working) units.
 */
#define base2local (s->dbip->dbi_base2local)
#define local2base (s->dbip->dbi_local2base)
#define cur_title (s->dbip->dbi_title)      /* current model title */


/* Some useful constants, if they haven't been defined elsewhere. */

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif



/* default region codes defined in mover.c */
extern int item_default;
extern int air_default;
extern int mat_default;
extern int los_default;

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

extern mat_t modelchanges;		/* full changes this edit */
extern mat_t incr_change;		/* change(s) from last cycle */
extern point_t recip_vanishing_point;

/* defined in buttons.c */
extern fastf_t acc_sc_sol;	/* accumulate solid scale factor */
extern fastf_t acc_sc_obj;	/* accumulate global object scale factor */
extern fastf_t acc_sc[3];	/* accumulate local object scale factors */
extern mat_t acc_rot_sol;	/* accumulate solid rotations */

/* defined in mged.c */
extern FILE *infile;
extern FILE *outfile;
extern jmp_buf jmp_env;

/*
 * GED functions referenced in more than one source file:
 */

extern void mged_setup(struct mged_state *s);
extern void mged_global_variable_teardown(struct mged_state *s); /* cmd.c */
extern void buildHrot(mat_t, double, double, double);
extern void dozoom(struct mged_state *s, int which_eye);
#ifndef _WIN32
extern void itoa(int n, char *s, int w);
#endif
extern void eraseobj(struct directory **dpp);
extern void eraseobjall(struct directory **dpp);
extern void mged_finish(struct mged_state *s, int exitcode);
extern void slewview(struct mged_state *s, vect_t view_pos);
extern void mmenu_init(void);
extern void moveHinstance(struct mged_state *s, struct directory *cdp, struct directory *dp, matp_t xlate);
extern void moveHobj(struct mged_state *s, struct directory *dp, matp_t xlate);
extern void quit(struct mged_state *s);
extern void refresh(struct mged_state *s);
extern void sedit(struct mged_state *s);
extern void setview(struct mged_state *s, double a1, double a2, double a3);
extern void adcursor(void);
extern void mmenu_display(int y_top);
extern void mmenu_set(struct mged_state *s, int idx, struct menu_item *value);
extern void mmenu_set_all(struct mged_state *s, int idx, struct menu_item *value);
extern void sedit_menu(struct mged_state *s);
extern void get_attached(struct mged_state *s);
extern void (*cur_sigint)(int);	/* Current SIGINT status */
extern void sig2(int);
extern void sig3(int);

extern void aexists(struct mged_state *s, const char *name);
extern int release(struct mged_state *s, char *name, int need_close);

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
extern int movedir;  /* RARROW | UARROW | SARROW | ROTARROW */

extern struct display_list *illum_gdlp; /* Pointer to solid in solid table to be illuminated */
extern struct bv_scene_obj *illump; /* == 0 if none, else points to ill. solid */
extern int ipathpos; /* path index of illuminated element */
extern int sedraw; /* apply solid editing changes */
extern int edobj; /* object editing options */


/* Flags for line type decisions */
#define ROOT 0
#define INNER 1

/* FIXME: ugh, main global editing state */
extern int ged_state;	  /* (defined in titles.c) */
#define STATE ged_state

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


/* Cloned mged macros for use in Tcl/Tk */

#define TCL_WRITE_ERR { \
	Tcl_AppendResult(s->interp, "Database write error, aborting.\n", (char *)NULL);\
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_WRITE_ERR_return { \
	TCL_WRITE_ERR; \
	return TCL_ERROR; }

#define TCL_ALLOC_ERR { \
	Tcl_AppendResult(s->interp, "\
An error has occurred while adding a new object to the database.\n", (char *)NULL); \
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_ALLOC_ERR_return { \
	TCL_ALLOC_ERR; \
	return TCL_ERROR;  }

/* For errors from db_delete() or db_dirdelete() */
#define TCL_DELETE_ERR(_name) { \
	Tcl_AppendResult(s->interp, "An error has occurred while deleting '", _name, \
			 "' from the database.\n", (char *)NULL);\
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_DELETE_ERR_return(_name) {  \
	TCL_DELETE_ERR(_name); \
	return TCL_ERROR;  }

/* A verbose message to attempt to soothe and advise the user */
#define TCL_ERROR_RECOVERY_SUGGESTION\
    Tcl_AppendResult(s->interp, "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n", (char *)NULL)


/**
 * Helpful macros to inform the user of trouble encountered in library
 * routines, and bail out.  They are intended to be used mainly in
 * top-level command processing routines, and therefore include a
 * "return" statement and curley brackets.  Thus, they should only be
 * used in void functions.  The word "return" is not in upper case in
 * these macros, to enable editor searches for the word "return" to
 * succeed.
 */
/* For errors from db_get() or db_getmrec() */
#define READ_ERR { \
	printf("Database read error, aborting\n"); }

#define READ_ERR_return { \
	READ_ERR; \
	return;  }

/* For errors from db_put() */
#define WRITE_ERR { \
	printf("Database write error, aborting.\n"); \
	ERROR_RECOVERY_SUGGESTION; }

#define WRITE_ERR_return { \
	WRITE_ERR; \
	return;  }

/* For errors from db_diradd() or db_alloc() */
#define ALLOC_ERR { \
	printf("\
An error has occurred while adding a new object to the database.\n"); \
	ERROR_RECOVERY_SUGGESTION; }

#define ALLOC_ERR_return { \
	ALLOC_ERR; \
	return;  }

/* For errors from db_delete() or db_dirdelete() */
#define DELETE_ERR(_name) { \
	printf("\
An error has occurred while deleting '%s' from the database.\n", _name); \
	ERROR_RECOVERY_SUGGESTION; }

#define DELETE_ERR_return(_name) {  \
	DELETE_ERR(_name); \
	return;  }

/* A verbose message to attempt to soothe and advise the user */
#define ERROR_RECOVERY_SUGGESTION	\
    printf("\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n")

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


#define FUNTAB_UNLIMITED -1

struct funtab {
    char *ft_name;
    char *ft_parms;
    char *ft_comment;
    int (*ft_func)(int, const char **);
    int ft_min;
    int ft_max;
    int tcl_converted;
};


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

/* defined in cmd.c */
extern struct cmd_list head_cmd_list;
extern struct cmd_list *curr_cmd_list;

#if !defined(HAVE_TK) && !defined(TK_WINDOW_TYPEDEF)
typedef void *Tk_Window;
#  define TK_WINDOW_TYPEDEF 1
#endif
extern Tk_Window tkwin; /* in cmd.c */

/* defined in rtif.c */
extern struct run_rt head_run_rt;

#define MGED_PROMPT "\rmged> "

/* Command return codes */
#define CMD_OK 919
#define CMD_BAD 920
#define CMD_MORE 921
#define MORE_ARGS_STR "more arguments needed::"


/* attach.c */
int is_dm_null(void);
int mged_attach(struct mged_state *s, const char *wp_name, int argc, const char *argv[]);
void mged_link_vars(struct mged_dm *p);
void mged_slider_free_vls(struct mged_dm *p);
int gui_setup(struct mged_state *s, const char *dstr);


/* buttons.c */
void btn_head_menu(struct mged_state *s, int i, int menu, int item);
void chg_l2menu(struct mged_state *s, int i);

/* chgmodel.c */
int extract_mater_from_line(
    char *line,
    char *name,
    char *shader,
    int *r, int *g, int *b,
    int *override,
    int *inherit);

/* chgview.c */
int mged_erot_xyz(struct mged_state *s, char origin, vect_t rvec);
int mged_svbase(void);
int mged_vrot_xyz(char origin, char coords, vect_t rvec);
void size_reset(struct mged_state *s);
void solid_list_callback(struct mged_state *s);

extern void view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2); /* defined in chgview.c */
extern void view_ring_destroy(struct mged_dm *dlp);

/* cmd.c */
int cmdline(struct mged_state *s, struct bu_vls *vp, int record);
int mged_cmd(struct mged_state *s, int argc, const char *argv[], struct funtab in_functions[]);
void mged_print_result(struct mged_state *s, int status);
int gui_output(void *clientData, void *str);
void mged_pr_output(Tcl_Interp *interp);

/* color_scheme.c */
void cs_set_bg(const struct bu_structparse *, const char *, void *, const char *, void *);
void cs_update(const struct bu_structparse *, const char *, void *, const char *, void *);
void cs_set_dirty_flag(const struct bu_structparse *, const char *, void *, const char *, void *);

/* columns.c */
void vls_col_item(struct bu_vls *str, const char *cp);
void vls_col_eol(struct bu_vls *str);

/* dir.c */
void dir_summary(int flag);
int cmd_killall(
    ClientData clientData,
    Tcl_Interp *interpreter,
    int argc,
    const char *argv[]);
int cmd_killrefs(
    ClientData clientData,
    Tcl_Interp *interpreter,
    int argc,
    const char *argv[]);
int cmd_killtree(
    ClientData clientData,
    Tcl_Interp *interpreter,
    int argc,
    const char *argv[]);

/* dodraw.c */
int drawtrees(int argc, const char *argv[], int kind);
int replot_modified_solid(struct mged_state *s, struct bv_scene_obj *sp, struct rt_db_internal *ip, const mat_t mat);
int replot_original_solid(struct mged_state *s, struct bv_scene_obj *sp);
void add_solid_path_to_result(Tcl_Interp *interpreter, struct bv_scene_obj *sp);
int redraw_visible_objects(struct mged_state *s);

/* dozoom.c */
void createDLists(struct bu_list *hdlp);
void createDListSolid(void *, struct bv_scene_obj *);
void createDListAll(void *, struct display_list *);
void freeDListsAll(unsigned int dlist, int range);

/* edarb.c */
int editarb(struct mged_state *s, vect_t pos_model);
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
};
extern struct mged_opendb_ctx mged_global_db_ctx;

/* mged.c */
void new_edit_mats(struct mged_state *s);
void new_mats(void);
void pr_beep(void);

extern int interactive; /* for pr_prompt */
void pr_prompt(struct mged_state *s, int show_prompt);

/* grid.c */
extern void round_to_grid(struct mged_state *s, fastf_t *view_dx, fastf_t *view_dy);
extern void snap_keypoint_to_grid(struct mged_state *s);
extern void snap_view_center_to_grid(struct mged_state *s);
extern void snap_to_grid(struct mged_state *s, fastf_t *mx, fastf_t *my);
extern void snap_view_to_grid(struct mged_state *s, fastf_t view_dx, fastf_t view_dy);
extern void draw_grid(struct mged_state *s);

/* menu.c */
int mmenu_select(struct mged_state *s, int pen_y, int do_func);

/* predictor.c */
extern void predictor_frame(void);
extern void predictor_init(void);

/* usepen.c */
void buildHrot(mat_t mat, double alpha, double beta, double ggamma);
void wrt_view(mat_t out, const mat_t change, const mat_t in);
void wrt_point(mat_t out, const mat_t change, const mat_t in, const point_t point);

/* tedit.c */
int get_editor_string(struct mged_state *s, struct bu_vls *editstring);

/* titles.c */
void create_text_overlay(struct mged_state *s, struct bu_vls *vp);
void screen_vls(int xbase, int ybase, struct bu_vls *vp);
void dotitles(struct mged_state *s, struct bu_vls *overlay_vls);

/* rect.c */
void zoom_rect_area(struct mged_state *);
void paint_rect_area(void);
void rt_rect_area(struct mged_state *);
void draw_rect(void);
void set_rect(const struct bu_structparse *, const char *, void *, const char *, void *);
void rect_view2image(void);
void rect_image2view(void);
void rb_set_dirty_flag(const struct bu_structparse *, const char *, void *, const char *, void *);


/* track.c */
int wrobj(struct mged_state *s, char name[], int flags);

/* edsol.c */
extern int inpara;	/* parameter input from keyboard flag */
void vls_solid(struct mged_state *s, struct bu_vls *vp, struct rt_db_internal *ip, const mat_t mat);
void transform_editing_solid(
    struct mged_state *s,
    struct rt_db_internal *os,		/* output solid */
    const mat_t mat,
    struct rt_db_internal *is,		/* input solid */
    int freedbi);
void replot_editing_solid(struct mged_state *s);
void sedit_abs_scale(struct mged_state *s);
void sedit_accept(struct mged_state *s);
void sedit_mouse(struct mged_state *s, const vect_t mousevec);
void sedit_reject(struct mged_state *s);
void sedit_vpick(struct mged_state *s, point_t v_pos);
void oedit_abs_scale(struct mged_state *s);
void oedit_accept(struct mged_state *s);
void oedit_reject(void);
void objedit_mouse(struct mged_state *s, const vect_t mousevec);
extern int nurb_closest2d(int *surface, int *uval, int *vval, const struct rt_nurb_internal *spl, const point_t ref_pt, const mat_t mat);
void label_edited_solid(int *num_lines, point_t *lines, struct rt_point_labels pl[], int max_pl, const mat_t xform, struct rt_db_internal *ip);
void init_oedit(struct mged_state *s);
void init_sedit(struct mged_state *s);

/* share.c */
void usurp_all_resources(struct mged_dm *dlp1, struct mged_dm *dlp2);

/* inside.c */
int torin(struct rt_db_internal *ip, fastf_t thick[6]);
int tgcin(struct rt_db_internal *ip, fastf_t thick[6]);
int rhcin(struct rt_db_internal *ip, fastf_t thick[4]);
int rpcin(struct rt_db_internal *ip, fastf_t thick[4]);
int partin(struct rt_db_internal *ip, fastf_t *thick);
int nmgin(struct rt_db_internal *ip, fastf_t thick);

/* cgtype is # of points, 4..8 */
int arbin(struct rt_db_internal *ip, fastf_t thick[6], int face, int cgtype, plane_t planes[6]);
int ehyin(struct rt_db_internal *ip, fastf_t thick[2]);
int ellgin(struct rt_db_internal *ip, fastf_t thick[6]);
int epain(struct rt_db_internal *ip, fastf_t thick[2]);
int etoin(struct rt_db_internal *ip, fastf_t thick[1]);

/* set.c */
extern void fbserv_set_port(const struct bu_structparse *, const char *, void *, const char *, void *);
extern void set_scroll_private(const struct bu_structparse *, const char *, void *, const char *, void *);
extern void mged_variable_setup(struct mged_state *s);

/* scroll.c */
void set_scroll(void);
int scroll_select(struct mged_state *s, int pen_x, int pen_y, int do_func);
int scroll_display(struct mged_state *s, int y_top);

/* edpipe.c */
void pipe_scale_od(struct mged_state *s, struct rt_db_internal *, fastf_t);
void pipe_scale_id(struct mged_state *s, struct rt_db_internal *, fastf_t);
void pipe_seg_scale_od(struct mged_state *s, struct wdb_pipe_pnt *, fastf_t);
void pipe_seg_scale_id(struct mged_state *s, struct wdb_pipe_pnt *, fastf_t);
void pipe_seg_scale_radius(struct mged_state *s, struct wdb_pipe_pnt *, fastf_t);
void pipe_scale_radius(struct mged_state *s, struct rt_db_internal *, fastf_t);
struct wdb_pipe_pnt *find_pipe_pnt_nearest_pnt(const struct bu_list *, const point_t);
struct wdb_pipe_pnt *pipe_add_pnt(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);
void pipe_ins_pnt(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);
struct wdb_pipe_pnt *pipe_del_pnt(struct mged_state *s, struct wdb_pipe_pnt *);
void pipe_move_pnt(struct mged_state *s, struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t);

/* vparse.c */
extern void mged_vls_struct_parse(struct mged_state *s, struct bu_vls *vls, const char *title, struct bu_structparse *how_to_parse, const char *structp, int argc, const char *argv[]); /* defined in vparse.c */
extern void mged_vls_struct_parse_old(struct mged_state *s, struct bu_vls *vls, const char *title, struct bu_structparse *how_to_parse, char *structp, int argc, const char *argv[]);

/* rtif.c */
int build_tops(char **start, char **end);

/* mater.c */
void mged_color_soltab(struct mged_state *s);

/* utility1.c */
int editit(struct mged_state *s, const char *command, const char *tempfile);

int Wdb_Init(Tcl_Interp *interp);
int wdb_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]);
void wdb_deleteProc(ClientData clientData);

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
