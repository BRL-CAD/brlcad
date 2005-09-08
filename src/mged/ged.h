/*                           G E D . H
 * BRL-CAD
 *
 * Copyright (C) 1985-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ged.h
 *
 * This file contains all of the definitions local to
 * the GED graphics editor.
 *
 *	     V E R Y   I M P O R T A N T   N O T I C E ! ! !
 *
 *  Many people in the computer graphics field use post-multiplication,
 *  (thanks to Newman and Sproull) with row vectors, ie:
 *
 *		view_vec = model_vec * T
 *
 *  However, in the GED system, the more traditional representation
 *  of column vectors is used (ref: Gwyn).  Therefore, when transforming
 *  a vector by a matrix, pre-multiplication is used, ie:
 *
 *		view_vec = model2view_mat * model_vec
 *
 *  Furthermore, additional transformations are multiplied on the left, ie:
 *
 *		vec'  =  T1 * vec
 *		vec'' =  T2 * T1 * vec  =  T2 * vec'
 *
 *  The most notable implication of this is the location of the
 *  "delta" (translation) values in the matrix, ie:
 *
 *        x'     ( R0   R1   R2   Dx )      x
 *        y' =  (  R4   R5   R6   Dy  )  *  y
 *        z'    (  R8   R9   R10  Dz  )     z
 *        w'     (  0    0    0   1/s)      w
 *
 *  This of course requires that the rotation portion be computed
 *  using somewhat different formulas (see buildHrot for both kinds).
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

#include "tcl.h"
#include "wdb.h"

/* Needed to define struct menu_item - RFH */
#include "./menu.h"

/* Needed to define struct w_dm - RFH */
#include "./mged_dm.h"

/* Needed to define struct solid - RFH */
#include "solid.h"

#define HIDE_MGEDS_ARB_ROUTINES 1

/* A hack around a compilation issue.  No need in fixing it now as the build
   process is about to be redone - RFH */
#ifndef SEEN_RT_NURB_INTERNAL
#define SEEN_RT_NURB_INTERNAL
struct rt_nurb_internal {
	long		magic;
	int	 	nsrf;		/* number of surfaces */
	struct face_g_snurb **srfs;	/* The surfaces themselves */
};
#endif

/*	Stuff needed from db.h */
#ifndef NAMESIZE

#define NAMESIZE 16
#define NAMEMOVE(from,to)       (void)strncpy(to, from, NAMESIZE)

#define MGED_DB_NAME "db"
#define MGED_INMEM_NAME ".inmem"
#define MGED_DG_NAME "dg"

#define ID_NO_UNIT	0		/* unspecified */
#define ID_MM_UNIT	1		/* millimeters (preferred) */
#define ID_UM_UNIT	2		/* micrometers */
#define ID_CM_UNIT	3		/* centimeters */
#define ID_M_UNIT	4		/* meters */
#define ID_KM_UNIT	5		/* kilometers */
#define ID_IN_UNIT	6		/* inches */
#define ID_FT_UNIT	7		/* feet */
#define ID_YD_UNIT	8		/* yards */
#define ID_MI_UNIT	9		/* miles */

#define ARB4	4	/* tetrahedron */
#define ARB5	5	/* pyramid */
#define ARB6	6	/* extruded triangle */
#define ARB7	7	/* weird 7-vertex shape */
#define ARB8	8	/* hexahedron */

#endif

#if USE_PROTOTYPES
#	define	MGED_EXTERN(type_and_name,args)	extern type_and_name args
#	define	MGED_ARGS(args)			args
#else
#	define	MGED_EXTERN(type_and_name,args)	extern type_and_name()
#	define	MGED_ARGS(args)			()
#endif

extern double	degtorad, radtodeg;	/* Defined in usepen.c */

/*
 * All GED files are stored in a fixed base unit (MM).
 * These factors convert database unit to local (or working) units.
 */
extern struct db_i	*dbip;		       /* defined in ged.c */
extern int		 dbih;		       /* defined in ged.c */
extern struct rt_wdb	*wdbp;			/* defined in ged.c */
extern struct dg_obj	*dgop;			/* defined in ged.c */
#define	base2local	(dbip->dbi_base2local)
#define local2base	(dbip->dbi_local2base)
#define	cur_title	(dbip->dbi_title)      /* current model title */

/* Some useful constants, if they haven't been defined elsewhere. */

#ifndef FALSE
# define FALSE 0
#endif

#ifndef TRUE
# define TRUE 1
#endif

#ifndef False
# define False (0)
#endif

#ifndef True
# define True (1)
#endif

/* Tolerances */
extern double		mged_abs_tol;		/* abs surface tolerance */
extern double		mged_rel_tol;		/* rel surface tolerance */
extern double		mged_nrm_tol;		/* surface normal tolerance */

/* default region codes       defined in mover.c */
extern int	item_default;
extern int	air_default;
extern int	mat_default;
extern int	los_default;

/*
 *  Definitions.
 *
 *  Solids are defined in "model space".
 *  The screen is in "view space".
 *  The visible part of view space is -1.0 <= x,y,z <= +1.0
 *
 *  The transformation from the origin of model space to the
 *  origin of view space (the "view center") is contained
 *  in the matrix "toViewcenter".  The viewing rotation is
 *  contained in the "Viewrot" matrix.  The viewscale factor
 *  (for [15] use) is kept in the float "Viewscale".
 *
 *  model2view = Viewscale * Viewrot * toViewcenter;
 *
 *  model2view is the matrix going from model space coordinates
 *  to the view coordinates, and view2model is the inverse.
 *  It is recomputed by new_mats() only.
 *
 * CHANGE matrix.  Defines the change between the un-edited and the
 * present state in the edited solid or combination.
 *
 * model2objview = modelchanges * model2view
 *
 *  For object editing and solid edit, model2objview translates
 *  from model space to view space with all the modelchanges too.
 *
 *  These are allocated storage in dozoom.c
 */

extern mat_t	modelchanges;		/* full changes this edit */
extern mat_t	incr_change;		/* change(s) from last cycle */
extern point_t	recip_vanishing_point;

/*
 * Identity matrix.  Handy to have around. - initialized in e1.c
 */
extern mat_t	identity;

/* defined in buttons.c */
extern fastf_t	acc_sc_sol;	/* accumulate solid scale factor */
extern fastf_t  acc_sc_obj;	/* accumulate global object scale factor */
extern fastf_t	acc_sc[3];	/* accumulate local object scale factors */
extern mat_t	acc_rot_sol;	/* accumulate solid rotations */

/* defined in dodraw.c */
extern int	no_memory;	/* flag indicating memory for drawing is used up */

/* defined in menu.c */
extern int	menuflag;	/* flag indicating if a menu item is selected */

/* defined in ged.c */
extern FILE *infile;
extern FILE *outfile;
extern jmp_buf jmp_env;

/*
 *	GED functions referenced in more than one source file:
 */
extern int              tran(), irot();
extern void             mged_setup(void);
extern void		dir_build(), buildHrot(fastf_t *, double, double, double), dozoom(int which_eye),
			pr_schain(struct solid *startp, int lvl), itoa(int n, char *s, int w);
extern void		eraseobj(register struct directory **dpp), eraseobjall(register struct directory **dpp), mged_finish(int exitcode), slewview(fastf_t *view_pos),
			mmenu_init(void), moveHinstance(struct directory *cdp, struct directory *dp, matp_t xlate), moveHobj(register struct directory *dp, matp_t xlate),
			quit(void), refresh(void), rej_sedit(), sedit(void),
			setview(double a1, double a2, double a3),
			adcursor(void), mmenu_display(int y_top), mmenu_set(int index, struct menu_item *value), mmenu_set_all(int index, struct menu_item *value),
			col_item(), col_putchar(), col_eol(), col_pr4v();
extern void		sedit_menu(void);
extern void		attach(), get_attached(void);
extern void		(*cur_sigint)();	/* Current SIGINT status */
extern void		sig2(int), sig3(int);

extern void		aexists(char *name);
extern int		getname(), use_pen(), dir_print();
extern int              mged_cmd_arg_check(), release(char *name, int need_close);
extern struct directory	*combadd(), **dir_getspace();
extern void		ellipse();

#if 0
/* rt_memalloc.c */
MGED_EXTERN(unsigned long rt_memalloc, (struct mem_map **pp, unsigned size) );
MGED_EXTERN(unsigned long rt_memget, (struct mem_map **pp, unsigned int size,
	unsigned int place) );
MGED_EXTERN(void rt_memfree, (struct mem_map **pp, unsigned size, unsigned long addr) );
MGED_EXTERN(void rt_mempurge, (struct mem_map **pp) );
MGED_EXTERN(void rt_memprint, (struct mem_map **pp) );
#endif

/* buttons.c */
MGED_EXTERN(void button, (int bnum) );
MGED_EXTERN(void press, (char *str) );
MGED_EXTERN(char *label_button, (int bnum) );
MGED_EXTERN(int not_state, (int desired, char *str) );
MGED_EXTERN(int chg_state, (int from, int to, char *str) );
MGED_EXTERN(void state_err, (char *str) );

MGED_EXTERN(void do_list, (struct bu_vls *outstrp, struct directory *dp, int verbose));
MGED_EXTERN(int invoke_db_wrapper, (Tcl_Interp *interpreter, int argc, char **argv));

/* history.c */
void history_record(
	struct bu_vls *cmdp,
	struct timeval *start,
	struct timeval *finish,
	int status);			   /* Either CMD_OK or CMD_BAD */
void history_setup(void);


/* cmd.c */

extern void start_catching_output(struct bu_vls *vp), stop_catching_output(struct bu_vls *vp);

#ifndef	NULL
#define	NULL		0
#endif

/*
 * Pointer to solid in solid table to be illuminated. - defined in usepen.c
 */
extern struct solid	*illump;/* == 0 if none, else points to ill. solid */
extern int	sedraw;		/* apply solid editing changes */

/* defined in chgview.c */
extern int	inpara;		/* parameter input from keyboard flag */
extern int	newedge;	/* new edge for arb editing */

/* defined in usepen.c */
extern int	ipathpos;	/* path index of illuminated element */

#define RARROW		001
#define UARROW		002
#define SARROW		004
#define	ROTARROW	010	/* Object rotation enabled */
extern int	movedir;	/* RARROW | UARROW | SARROW | ROTARROW */

extern int	edobj;		/* object editing options */

/* Flags for line type decisions */
#define ROOT	0
#define INNER	1

/*
 *  Editor States
 */
extern int state;			/* (defined in titles.c) */
extern char *state_str[];		/* identifying strings */
#define ST_VIEW		1		/* Viewing only */
#define ST_S_PICK	2		/* Picking for Solid Edit */
#define ST_S_EDIT	3		/* Solid Editing */
#define ST_O_PICK	4		/* Picking for Object Edit */
#define ST_O_PATH	5		/* Path select for Object Edit */
#define ST_O_EDIT	6		/* Object Editing */
#define ST_S_VPICK	7		/* Vertex Pick */
#define ST_S_NO_EDIT	8		/* Solid edit without Rotate, translate, or scale */

#if 0 /* Using the one provided by bu.h */
#ifndef GETSTRUCT
/* Acquire storage for a given struct, eg, GETSTRUCT(ptr,structname); */
#if __STDC__ && !alliant && !apollo
# define GETSTRUCT(p,str) \
	p = (struct str *)rt_calloc(1,sizeof(struct str), "getstruct " #str)
# define GETUNION(p,unn) \
	p = (union unn *)rt_calloc(1,sizeof(union unn), "getstruct " #unn)
#else
# define GETSTRUCT(p,str) \
	p = (struct str *)rt_calloc(1,sizeof(struct str), "getstruct str")
# define GETUNION(p,unn) \
	p = (union unn *)rt_calloc(1,sizeof(union unn), "getstruct unn")
#endif
#endif
#endif

#define	MAXLINE		RT_MAXLINE	/* Maximum number of chars per line */

/* Cloned mged macros for use in Tcl/Tk */
#define TCL_READ_ERR {\
	  Tcl_AppendResult(interp, "Database read error, aborting\n", (char *)NULL);\
	}

#define TCL_READ_ERR_return {\
          TCL_READ_ERR;\
	  return TCL_ERROR;\
	}

#define TCL_WRITE_ERR { \
	  Tcl_AppendResult(interp, "Database write error, aborting.\n", (char *)NULL);\
	  TCL_ERROR_RECOVERY_SUGGESTION; }	

#define TCL_WRITE_ERR_return { \
	  TCL_WRITE_ERR; \
	  return TCL_ERROR; }

#define TCL_ALLOC_ERR { \
	  Tcl_AppendResult(interp, "\
An error has occured while adding a new object to the database.\n", (char *)NULL); \
	  TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_ALLOC_ERR_return { \
	TCL_ALLOC_ERR; \
	return TCL_ERROR;  }

/* For errors from db_delete() or db_dirdelete() */
#define TCL_DELETE_ERR(_name){ \
	Tcl_AppendResult(interp, "An error has occurred while deleting '", _name,\
	"' from the database.\n", (char *)NULL);\
	TCL_ERROR_RECOVERY_SUGGESTION; }

#define TCL_DELETE_ERR_return(_name){  \
	TCL_DELETE_ERR(_name); \
	return TCL_ERROR;  }

/* A verbose message to attempt to soothe and advise the user */
#define	TCL_ERROR_RECOVERY_SUGGESTION\
        Tcl_AppendResult(interp, "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety,\n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n", (char *)NULL)

/*
 *  Helpful macros to inform the user of trouble encountered in
 *  library routines, and bail out.
 *  They are intended to be used mainly in top-level command processing
 *  routines, and therefore include a "return" statement and curley brackets.
 *  Thus, they should only be used in void functions.
 *  The word "return" is not in upper case in these macros,
 *  to enable editor searches for the word "return" to succeed.
 */
/* For errors from db_get() or db_getmrec() */
#define READ_ERR { \
	(void)printf("Database read error, aborting\n"); }

#define READ_ERR_return		{ \
	READ_ERR; \
	return;  }

/* For errors from db_put() */
#define WRITE_ERR { \
	(void)printf("Database write error, aborting.\n"); \
	ERROR_RECOVERY_SUGGESTION; }	

#define WRITE_ERR_return	{ \
	WRITE_ERR; \
	return;  }

/* For errors from db_diradd() or db_alloc() */
#define ALLOC_ERR { \
	(void)printf("\
An error has occured while adding a new object to the database.\n"); \
	ERROR_RECOVERY_SUGGESTION; }

#define ALLOC_ERR_return	{ \
	ALLOC_ERR; \
	return;  }

/* For errors from db_delete() or db_dirdelete() */
#define DELETE_ERR(_name)	{ \
	(void)printf("\
An error has occurred while deleting '%s' from the database.\n", _name); \
	ERROR_RECOVERY_SUGGESTION; }

#define DELETE_ERR_return(_name)	{  \
	DELETE_ERR(_name); \
	return;  }

/* A verbose message to attempt to soothe and advise the user */
#define	ERROR_RECOVERY_SUGGESTION	\
	(void)printf("\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety,\n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n")

/* Check if database pointer is NULL */
#define CHECK_DBI_NULL \
	if( dbip == DBI_NULL ) \
	{ \
		Tcl_AppendResult(interp, "A database is not open!\n", (char *)NULL); \
		return TCL_ERROR; \
	}	

/* Check if the database is read only, and if so return TCL_ERROR */
#define	CHECK_READ_ONLY	\
	if( dbip->dbi_read_only) \
	{ \
		Tcl_AppendResult(interp, "Sorry, this database is READ-ONLY\n", (char *)NULL ); \
		return TCL_ERROR; \
	}

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
  struct bu_list	l;
  struct bu_vls		mh_command;
  struct timeval	mh_start;
  struct timeval	mh_finish;
  int			mh_status;
};

/* internal variables related to the command window(s) */
struct cmd_list {
  struct bu_list	l;
  struct dm_list	*cl_tie;        /* the drawing window that we're tied to */
  struct mged_hist	*cl_cur_hist;
  struct bu_vls		cl_more_default;
  struct bu_vls		cl_name;
  int			cl_quote_string;
};
#define CMD_LIST_NULL ((struct cmd_list *)NULL)

/* defined in cmd.c */
extern Tcl_Interp *interp;
extern struct cmd_list head_cmd_list;
extern struct cmd_list *curr_cmd_list;

/* defined in rtif.c */
extern struct run_rt head_run_rt;

#define	MAXARGS		9000	/* Maximum number of args per line */

#define MGED_PROMPT "\rmged> "

/* Command return codes */
#define CMD_OK		919
#define CMD_BAD		920
#define CMD_MORE	921
#define MORE_ARGS_STR    "more arguments needed::"

/* adc.c */
int f_adc (
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);

/* attach.c */
#if defined(SEEN_MGED_DM_H)
int mged_attach(
	struct w_dm *wp,
	int argc,
	char *argv[]);
#else
int mged_attach(struct w_dm *wp, int argc, char **argv);
#endif

/* buttons.c */
int bv_zoomin(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_zoomout(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_rate_toggle(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_top(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_bottom(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_right(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_left(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_front(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_rear(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_vrestore(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_vsave(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_adcursor(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_reset(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_45_45(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int bv_35_25(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_illuminate(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_s_illuminate(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_scale(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_x(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_y(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_xy(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_rotate(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_accept(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_reject(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_s_edit(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_s_rotate(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_s_trans(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_s_scale(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_xscale(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_yscale(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
int be_o_zscale(ClientData clientData, Tcl_Interp *interpreter, int argc, char **argv);
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
int f_rmater(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int     argc,
	char    *argv[]);
int
f_wmater(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int     argc,
	char    *argv[]);


/* chgtree.c */
int cmd_kill(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int cmd_name(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);

/* chgview.c */
int edit_com(
     int	argc,
     char	**argv,
     int	kind,
     int	catch_sigint);
void eraseobjpath(
     Tcl_Interp	*interpreter,
     int	argc,
     char	**argv,
     int	noisy,
     int	all);
int f_edit(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int f_erase(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int     argc,
	char    **argv);
int f_erase_all(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int     argc,
	char    **argv);
int f_sed(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int f_zap(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int mged_erot_xyz(
	char origin,
	vect_t rvec);
int mged_svbase(void);
int mged_vrot_xyz(
	char origin,
	char coords,
	vect_t rvec);
void size_reset(void);
void solid_list_callback(void);
void view_ring_destroy(struct dm_list *dlp);

/* cmd.c */
int cmdline( struct bu_vls *vp, int record);
int f_quit(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int mged_cmd(
	int argc,
	char **argv,
	struct funtab in_functions[]);
void mged_print_result(int status);

/* color_scheme.c */
void cs_set_bg(void);
void cs_update(void);
void cs_set_dirty_flag(void);

/* columns.c */
void
vls_col_item(
	struct bu_vls	*str,
	const char	*cp);
void vls_col_eol( struct bu_vls *str );
void vls_col_pr4v(struct bu_vls *vls, struct directory **list_of_names, int num_in_list);
void vls_line_dpp(
	struct bu_vls *vls,
	struct directory **list_of_names,
	int num_in_list,
	int aflag,	/* print all objects */
	int cflag,	/* print combinations */
	int rflag,	/* print regions */
	int sflag);	/* print solids */
void vls_long_dpp(
	struct bu_vls *vls,
	struct directory **list_of_names,
	int num_in_list,
	int aflag,	/* print all objects */
	int cflag,	/* print combinations */
	int rflag,	/* print regions */
	int sflag);	/* print solids */

/* dir.c */
void dir_summary(int flag);
int cmd_killall(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int cmd_killtree(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);

/* dodraw.c */
void cvt_vlblock_to_solids(
	struct bn_vlblock	*vbp,
	const char		*name,
	int			copy);
void drawH_part2(
	int			dashflag,
	struct bu_list		*vhead,
	struct db_full_path	*pathp,
	struct db_tree_state	*tsp,
	struct solid		*existing_sp);
int drawtrees(
	int	argc,
	char	**argv,
	int	kind);
int invent_solid(
	const char	*name,
	struct bu_list	*vhead,
	long		rgb,
	int		copy);
void pathHmat(
	register struct solid *sp,
	matp_t matp,
	int depth);
int replot_modified_solid(
	struct solid			*sp,
	struct rt_db_internal		*ip,
	const mat_t			mat);
int replot_original_solid( struct solid *sp );
void add_solid_path_to_result(
	Tcl_Interp *interpreter,
	struct solid *sp);

/* dozoom.c */
void createDList(struct solid *sp);
void createDLists(struct bu_list *hsp);
void createDListALL(struct solid *sp);
void createDListsAll(struct bu_list *hsp);
void freeDListsAll(unsigned int dlist, int range);

/* edarb.c */
int editarb( vect_t pos_model );
int mv_edge(
	vect_t thru,
	int bp1, int bp2, int end1, int end2,
	const vect_t	dir);

/* edars.c */
#if defined(SEEN_RTGEOM_H)
void find_nearest_ars_pt(
	int *crv,
	int *col,
	struct rt_ars_internal *ars,
	point_t pick_pt,
	vect_t dir);
#else
void find_nearest_ars_pt();
#endif

/* ged.c */
int event_check( int non_blocking );
int f_opendb(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int f_closedb(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
void new_edit_mats(void);
void new_mats(void);
void pr_prompt(void);
void pr_beep(void);

/* grid.c */
void round_to_grid(fastf_t *view_dx, fastf_t *view_dy);
void snap_keypoint_to_grid(void);
void snap_view_center_to_grid(void);
void snap_to_grid(
	fastf_t *mx,		/* input and return values */
	fastf_t *my);		/* input and return values */
void snap_view_to_grid(fastf_t view_dx, fastf_t view_dy);

/* menu.c */
int mmenu_select( int pen_y, int do_func );

/* overlay.c */
int f_overlay(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);

/* predictor.c */
void predictor_frame(void);

/* usepen.c */
int f_mouse(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);
int f_aip(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int argc,
	char **argv);
void buildHrot( mat_t mat, double alpha, double beta, double ggamma );
void wrt_view( mat_t out, const mat_t change, const mat_t in );
void wrt_point( mat_t out, const mat_t change, const mat_t in, const point_t point );
void wrt_point_direc( mat_t out, const mat_t change, const mat_t in, const point_t point, const vect_t direc );
int f_matpick(
	ClientData clientData,
	Tcl_Interp *interpreter,
	int	argc,
	char	**argv);

/* tedit.c */
int editit( const char *file );

/* titles.c */
void create_text_overlay( struct bu_vls *vp );
void screen_vls(
	int	xbase,
	int	ybase,
	struct bu_vls	*vp);
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
int wrobj( char name[], int flags );

/* red.c */
int writecomb( const struct rt_comb_internal *comb, const char *name );
int checkcomb(void);

/* edsol.c */
void vls_solid( struct bu_vls *vp, const struct rt_db_internal *ip, const mat_t mat );
void transform_editing_solid(
	struct rt_db_internal	*os,		/* output solid */
	const mat_t		mat,
	struct rt_db_internal	*is,		/* input solid */
	int			free);
void replot_editing_solid(void);
void sedit_abs_scale(void);
void sedit_accept(void);
void sedit_mouse( const vect_t mousevec );
void sedit_reject(void);
void sedit_vpick( point_t v_pos );
void oedit_abs_scale(void);
void oedit_accept(void);
void oedit_reject(void);
void objedit_mouse( const vect_t mousevec );
extern int nurb_closest2d(int *surface, int *uval, int *vval, const struct rt_nurb_internal *spl, const fastf_t *ref_pt, const fastf_t *mat);
void label_edited_solid(
	int *num_lines,
	point_t *lines,
	struct rt_point_labels	pl[],
	int			max_pl,
	const mat_t		xform,
	struct rt_db_internal	*ip);
void init_oedit(void);
void init_sedit(void);

#if 0
#ifdef HIDE_MGEDS_ARB_ROUTINES
#  define rt_arb_calc_planes(planes,arb,type,tol) \
rt_arb_calc_planes(interp,arb,type,planes,tol)
#else
int rt_arb_calc_planes(
	plane_t			planes[6],
	struct rt_arb_internal	*arb,
	int			type,
	const struct bn_tol	*tol);
#endif
#endif


/* share.c */
void usurp_all_resources(struct dm_list *dlp1, struct dm_list *dlp2);

/* inside.c */
int torin(struct rt_db_internal *ip, fastf_t thick[6] );
int tgcin(struct rt_db_internal *ip, fastf_t thick[6]);
int rhcin(struct rt_db_internal *ip, fastf_t thick[4]);
int rpcin(struct rt_db_internal *ip, fastf_t thick[4]);
int partin(struct rt_db_internal *ip, fastf_t *thick );
int nmgin( struct rt_db_internal *ip, fastf_t thick );
int arbin(
	struct rt_db_internal	*ip,
	fastf_t	thick[6],
	int	nface,
	int	cgtype,		/* # of points, 4..8 */
	plane_t	planes[6]);
int ehyin(struct rt_db_internal *ip, fastf_t thick[2]);
int ellgin(struct rt_db_internal *ip, fastf_t thick[6]);
int epain(struct rt_db_internal *ip, fastf_t thick[2]);
int etoin(struct rt_db_internal *ip, fastf_t thick[1]);

/* set.c */
void set_scroll_private(void);
void mged_variable_setup(Tcl_Interp *interpreter);

/* scroll.c */
void set_scroll(void);
int scroll_select( int pen_x, int pen_y, int do_func );
int scroll_display( int y_top );

/* edpipe.c */
void pipe_scale_od( struct rt_db_internal *db_int, fastf_t scale);
void pipe_scale_id( struct rt_db_internal *db_int, fastf_t scale );
void pipe_seg_scale_od( struct wdb_pipept *ps, fastf_t scale );
void pipe_seg_scale_id( struct wdb_pipept *ps, fastf_t scale );
void pipe_seg_scale_radius( struct wdb_pipept *ps, fastf_t scale );
void pipe_scale_radius( struct rt_db_internal *db_int, fastf_t scale );
struct wdb_pipept *find_pipept_nearest_pt( const struct bu_list *pipe_hd, const point_t pt );
struct wdb_pipept *add_pipept( struct rt_pipe_internal *pipe, struct wdb_pipept *pp, const point_t new_pt );
void ins_pipept( struct rt_pipe_internal *pipe, struct wdb_pipept *pp, const point_t new_pt );
struct wdb_pipept *del_pipept( struct wdb_pipept *ps );
void move_pipept( struct rt_pipe_internal *pipe, struct wdb_pipept *ps, const point_t new_pt );

/* vparse.c */
void mged_vls_struct_parse_old(
	struct bu_vls *vls,
	const char *title,
	struct bu_structparse *how_to_parse,
	char *structp,
	int argc,
	char *argv[]);

/* rtif.c */
int build_tops(char **start, char **end);

/* mater.c */
void color_soltab(void);


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
