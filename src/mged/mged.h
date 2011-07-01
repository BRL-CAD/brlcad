/*                           M G E D . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
 * (thanks to Newman and Sproull) with row vectors, ie:
 *
 *	view_vec = model_vec * T
 *
 * However, in the GED system, the more traditional representation of
 * column vectors is used (ref: Gwyn).  Therefore, when transforming a
 * vector by a matrix, pre-multiplication is used, ie:
 *
 *	view_vec = model2view_mat * model_vec
 *
 * Furthermore, additional transformations are multiplied on the left, ie:
 *
 *	vec'  =  T1 * vec
 *	vec'' =  T2 * T1 * vec  =  T2 * vec'
 *
 * The most notable implication of this is the location of the
 * "delta" (translation) values in the matrix, ie:
 *
 *       x'     (R0   R1   R2   Dx) x
 *       y' =  (R4   R5   R6   Dy)  *  y
 *       z'    (R8   R9   R10  Dz) z
 *       w'     (0    0    0   1/s) w
 *
 * This of course requires that the rotation portion be computed
 * using somewhat different formulas (see buildHrot for both kinds).
 *
 */

#ifndef __MGED_H__
#define __MGED_H__

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

#include "tcl.h"
#include "wdb.h"
#include "dg.h"

/* Needed to define struct menu_item */
#include "./menu.h"

/* Needed to define struct w_dm */
#include "./mged_dm.h"

/* Needed to define struct solid */
#include "solid.h"


#define MGED_DB_NAME "db"
#define MGED_INMEM_NAME ".inmem"


/*
 * All GED files are stored in a fixed base unit (MM).  These factors
 * convert database unit to local (or working) units.
 */
extern struct ged *gedp;		/* defined in mged.c */
extern struct db_i *dbip;		/* defined in mged.c */
extern int dbih;		/* defined in mged.c */
extern struct rt_wdb *wdbp;		/* defined in mged.c */
#define base2local (dbip->dbi_base2local)
#define local2base (dbip->dbi_local2base)
#define cur_title (dbip->dbi_title)      /* current model title */


/* Some useful constants, if they haven't been defined elsewhere. */

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

/* Tolerances */
extern double mged_abs_tol; /* abs surface tolerance */
extern double mged_rel_tol; /* rel surface tolerance */
extern double mged_nrm_tol; /* surface normal tolerance */

extern struct bn_tol mged_tol;  /* calculation tolerance */
extern struct rt_tess_tol mged_ttol; /* XXX needs to replace mged_abs_tol, et.al. */


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
extern struct solid MGED_FreeSolid;	/* Head of freelist */

/* FIXME: ugh, main global interpreter */
extern Tcl_Interp *ged_interp;
#define INTERP ged_interp


/*
 * GED functions referenced in more than one source file:
 */

extern int tran();
extern int irot();
extern void mged_setup(Tcl_Interp **interpreter);
extern void mged_global_variable_teardown(Tcl_Interp *interpreter); /* cmd.c */
extern void dir_build();
extern void buildHrot(fastf_t *, double, double, double);
extern void dozoom(int which_eye);
#ifndef _WIN32
extern void itoa(int n, char *s, int w);
#endif
extern void eraseobj(struct directory **dpp);
extern void eraseobjall(struct directory **dpp);
extern void mged_finish(int exitcode);
extern void slewview(fastf_t *view_pos);
extern void mmenu_init(void);
extern void moveHinstance(struct directory *cdp, struct directory *dp, matp_t xlate);
extern void moveHobj(struct directory *dp, matp_t xlate);
extern void quit(void);
extern void refresh(void);
extern void rej_sedit();
extern void sedit(void);
extern void setview(double a1, double a2, double a3);
extern void adcursor(void);
extern void mmenu_display(int y_top);
extern void mmenu_set(int idx, struct menu_item *value);
extern void mmenu_set_all(int idx, struct menu_item *value);
extern void col_item();
extern void col_putchar();
extern void col_eol();
extern void col_pr4v();
extern void sedit_menu(void);
extern void get_attached(void);
extern void (*cur_sigint)();	/* Current SIGINT status */
extern void sig2(int);
extern void sig3(int);

extern void aexists(const char *name);
extern int getname();
extern int use_pen();
extern int dir_print();
extern int mged_cmd_arg_check();
extern int release(char *name, int need_close);
extern struct directory *combadd();
extern struct directory **dir_getspace();
extern void ellipse();

/* mged.c */
extern void mged_view_callback(struct ged_view *gvp, genptr_t clientData);

/* buttons.c */
extern void button(int bnum);
extern void press(char *str);
extern char *label_button(int bnum);
extern int not_state(int desired, char *str);
extern int chg_state(int from, int to, char *str);
extern void state_err(char *str);

extern int invoke_db_wrapper(Tcl_Interp *interpreter, int argc, const char *argv[]);

/* history.c */
void history_record(struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status); /* Either CMD_OK or CMD_BAD */
void history_setup(void);

/* defined in usepen.c */
#define RARROW   001
#define UARROW   002
#define SARROW   004
#define ROTARROW 010 /* Object rotation enabled */
extern int movedir;  /* RARROW | UARROW | SARROW | ROTARROW */

extern struct ged_display_list *illum_gdlp; /* Pointer to solid in solid table to be illuminated */
extern struct solid *illump; /* == 0 if none, else points to ill. solid */
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
#define TCL_READ_ERR {\
	Tcl_AppendResult(INTERP, "Database read error, aborting\n", (char *)NULL);\
    }

#define TCL_READ_ERR_return {\
	TCL_READ_ERR;\
	return TCL_ERROR;\
    }

#define TCL_WRITE_ERR { \
	Tcl_AppendResult(INTERP, "Database write error, aborting.\n", (char *)NULL);\
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_WRITE_ERR_return { \
	TCL_WRITE_ERR; \
	return TCL_ERROR; }

#define TCL_ALLOC_ERR { \
	Tcl_AppendResult(INTERP, "\
An error has occured while adding a new object to the database.\n", (char *)NULL); \
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_ALLOC_ERR_return { \
	TCL_ALLOC_ERR; \
	return TCL_ERROR;  }

/* For errors from db_delete() or db_dirdelete() */
#define TCL_DELETE_ERR(_name) { \
	Tcl_AppendResult(INTERP, "An error has occurred while deleting '", _name, \
			 "' from the database.\n", (char *)NULL);\
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_DELETE_ERR_return(_name) {  \
	TCL_DELETE_ERR(_name); \
	return TCL_ERROR;  }

/* A verbose message to attempt to soothe and advise the user */
#define TCL_ERROR_RECOVERY_SUGGESTION\
    Tcl_AppendResult(INTERP, "\
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
	(void)printf("Database read error, aborting\n"); }

#define READ_ERR_return { \
	READ_ERR; \
	return;  }

/* For errors from db_put() */
#define WRITE_ERR { \
	(void)printf("Database write error, aborting.\n"); \
	ERROR_RECOVERY_SUGGESTION; }

#define WRITE_ERR_return { \
	WRITE_ERR; \
	return;  }

/* For errors from db_diradd() or db_alloc() */
#define ALLOC_ERR { \
	(void)printf("\
An error has occured while adding a new object to the database.\n"); \
	ERROR_RECOVERY_SUGGESTION; }

#define ALLOC_ERR_return { \
	ALLOC_ERR; \
	return;  }

/* For errors from db_delete() or db_dirdelete() */
#define DELETE_ERR(_name) { \
	(void)printf("\
An error has occurred while deleting '%s' from the database.\n", _name); \
	ERROR_RECOVERY_SUGGESTION; }

#define DELETE_ERR_return(_name) {  \
	DELETE_ERR(_name); \
	return;  }

/* A verbose message to attempt to soothe and advise the user */
#define ERROR_RECOVERY_SUGGESTION	\
    (void)printf("\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety, \n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n")

/* Check if database pointer is NULL */
#define CHECK_DBI_NULL \
    if (dbip == DBI_NULL) { \
	Tcl_AppendResult(INTERP, "A database is not open!\n", (char *)NULL); \
	return TCL_ERROR; \
    }

/* Check if the database is read only, and if so return TCL_ERROR */
#define CHECK_READ_ONLY	\
    if (dbip->dbi_read_only) { \
	Tcl_AppendResult(INTERP, "Sorry, this database is READ-ONLY\n", (char *)NULL); \
	return TCL_ERROR; \
    }


#define FUNTAB_UNLIMITED -1

struct funtab {
    char *ft_name;
    char *ft_parms;
    char *ft_comment;
    int (*ft_func)();
    int ft_min;
    int ft_max;
    int tcl_converted;
};


struct mged_hist {
    struct bu_list l;
    struct bu_vls mh_command;
    struct timeval mh_start;
    struct timeval mh_finish;
    int mh_status;
};


/* internal variables related to the command window(s) */
struct cmd_list {
    struct bu_list l;
    struct dm_list *cl_tie;        /* the drawing window that we're tied to */
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
int mged_attach(struct w_dm *wp, int argc, const char *argv[]);
void mged_link_vars(struct dm_list *p);
void mged_slider_free_vls(struct dm_list *p);

/* buttons.c */
void btn_head_menu(int i, int menu, int item);
void chg_l2menu(int i);

/* chgmodel.c */
int extract_mater_from_line(
    char *line,
    char *name,
    char *shader,
    int *r, int *g, int *b,
    int *override,
    int *inherit);

/* chgview.c */
int mged_erot_xyz(char origin, vect_t rvec);
int mged_svbase(void);
int mged_vrot_xyz(char origin, char coords, vect_t rvec);
void size_reset(void);
void solid_list_callback(void);

extern void view_ring_init(struct _view_state *vsp1, struct _view_state *vsp2); /* defined in chgview.c */
extern void view_ring_destroy(struct dm_list *dlp);

/* cmd.c */
int cmdline(struct bu_vls *vp, int record);
int mged_cmd(int argc, const char *argv[], struct funtab in_functions[]);
void mged_print_result(int status);

/* color_scheme.c */
void cs_set_bg(void);
void cs_update(void);
void cs_set_dirty_flag(void);

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
void cvt_vlblock_to_solids(struct bn_vlblock *vbp, const char *name, int copy);
int drawtrees(int argc, const char *argv[], int kind);
int invent_solid(const char *name, struct bu_list *vhead, long rgb, int copy);
void pathHmat(struct solid *sp, matp_t matp, int depth);
int replot_modified_solid(struct solid *sp, struct rt_db_internal *ip, const mat_t mat);
int replot_original_solid(struct solid *sp);
void add_solid_path_to_result(Tcl_Interp *interpreter, struct solid *sp);

/* dozoom.c */
void createDList(struct solid *sp);
void createDLists(struct bu_list *hdlp);
void createDListALL(struct solid *sp);
void freeDListsAll(unsigned int dlist, int range);

/* edarb.c */
int editarb(vect_t pos_model);
int mv_edge(vect_t thru, int bp1, int bp2, int end1, int end2, const vect_t dir);
extern int newedge;	/* new edge for arb editing */

/* edars.c */
#if defined(SEEN_RTGEOM_H)
void find_nearest_ars_pt(int *crv, int *col, struct rt_ars_internal *ars, point_t pick_pt, vect_t dir);
#else
void find_nearest_ars_pt();
#endif

/* mged.c */
int event_check(int non_blocking);
void new_edit_mats(void);
void new_mats(void);
void pr_beep(void);

extern int interactive; /* for pr_prompt */
void pr_prompt(int show_prompt);

/* grid.c */
extern void round_to_grid(fastf_t *view_dx, fastf_t *view_dy);
extern void snap_keypoint_to_grid(void);
extern void snap_view_center_to_grid(void);
extern void snap_to_grid(fastf_t *mx, fastf_t *my);
extern void snap_view_to_grid(fastf_t view_dx, fastf_t view_dy);
extern void draw_grid(void);

/* menu.c */
int mmenu_select(int pen_y, int do_func);

/* predictor.c */
extern void predictor_frame(void);
extern void predictor_init(void);

/* usepen.c */
void buildHrot(mat_t mat, double alpha, double beta, double ggamma);
void wrt_view(mat_t out, const mat_t change, const mat_t in);
void wrt_point(mat_t out, const mat_t change, const mat_t in, const point_t point);

/* tedit.c */
int get_editor_string(struct bu_vls *editstring);

/* titles.c */
void create_text_overlay(struct bu_vls *vp);
void screen_vls(int xbase, int ybase, struct bu_vls *vp);
void dotitles(struct bu_vls *overlay_vls);

/* rect.c */
void zoom_rect_area(void);
void paint_rect_area(void);
void rt_rect_area(void);
void draw_rect(void);
void set_rect(void);
void rect_view2image(void);
void rect_image2view(void);
void rb_set_dirty_flag(void);


/* track.c */
int wrobj(char name[], int flags);

/* edsol.c */
extern int inpara;	/* parameter input from keyboard flag */
void vls_solid(struct bu_vls *vp, struct rt_db_internal *ip, const mat_t mat);
void transform_editing_solid(
    struct rt_db_internal *os,		/* output solid */
    const mat_t mat,
    struct rt_db_internal *is,		/* input solid */
    int freedbi);
void replot_editing_solid(void);
void sedit_abs_scale(void);
void sedit_accept(void);
void sedit_mouse(const vect_t mousevec);
void sedit_reject(void);
void sedit_vpick(point_t v_pos);
void oedit_abs_scale(void);
void oedit_accept(void);
void oedit_reject(void);
void objedit_mouse(const vect_t mousevec);
extern int nurb_closest2d(int *surface, int *uval, int *vval, const struct rt_nurb_internal *spl, const fastf_t *ref_pt, const fastf_t *mat);
void label_edited_solid(int *num_lines, point_t *lines, struct rt_point_labels pl[], int max_pl, const mat_t xform, struct rt_db_internal *ip);
void init_oedit(void);
void init_sedit(void);

/* share.c */
void usurp_all_resources(struct dm_list *dlp1, struct dm_list *dlp2);

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
extern void set_scroll_private(void);
extern void mged_variable_setup(Tcl_Interp *interpreter);

/* scroll.c */
void set_scroll(void);
int scroll_select(int pen_x, int pen_y, int do_func);
int scroll_display(int y_top);

/* edpipe.c */
void pipe_scale_od(struct rt_db_internal *, fastf_t);
void pipe_scale_id(struct rt_db_internal *, fastf_t);
void pipe_seg_scale_od(struct wdb_pipept *, fastf_t);
void pipe_seg_scale_id(struct wdb_pipept *, fastf_t);
void pipe_seg_scale_radius(struct wdb_pipept *, fastf_t);
void pipe_scale_radius(struct rt_db_internal *, fastf_t);
struct wdb_pipept *find_pipept_nearest_pt(const struct bu_list *, const point_t);
struct wdb_pipept *add_pipept(struct rt_pipe_internal *, struct wdb_pipept *, const point_t);
void ins_pipept(struct rt_pipe_internal *, struct wdb_pipept *, const point_t);
struct wdb_pipept *del_pipept(struct wdb_pipept *);
void move_pipept(struct rt_pipe_internal *, struct wdb_pipept *, const point_t);

/* vparse.c */
extern void mged_vls_struct_parse(struct bu_vls *vls, const char *title, struct bu_structparse *how_to_parse, const char *structp, int argc, const char *argv[]); /* defined in vparse.c */
extern void mged_vls_struct_parse_old(struct bu_vls *vls, const char *title, struct bu_structparse *how_to_parse, char *structp, int argc, const char *argv[]);

/* rtif.c */
int build_tops(char **start, char **end);

/* mater.c */
void color_soltab(void);

/* utility1.c */
int editit(const char *command, const char *tempfile);

#endif  /* __GED_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
