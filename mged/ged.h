/*
 *			G E D . H
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#include <sys/time.h>
#include <time.h>
#include "tcl.h"

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
#define	base2local	(dbip->dbi_base2local)
#define local2base	(dbip->dbi_local2base)
#define localunit	(dbip->dbi_localunit)  /* current local unit (index) */
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
extern void             mged_setup();
extern void		dir_build(), buildHrot(), dozoom(),
			pr_schain(), itoa();
extern void		eraseobj(), mged_finish(), slewview(),
			mmenu_init(), moveHinstance(), moveHobj(),
			quit(), refresh(), rej_sedit(), sedit(),
			setview(),
			adcursor(), mmenu_display(), mmenu_set(),
			col_item(), col_putchar(), col_eol(), col_pr4v();
extern void		sedit_menu();
#if 0
extern void		attach(), get_attached();
#endif
extern void		(*cur_sigint)();	/* Current SIGINT status */
extern void		sig2(), sig3();

extern void		aexists();
extern int		clip(), getname(), use_pen(), dir_print();
extern int              mged_cmd_arg_check(), release();
extern struct directory	*combadd(), **dir_getspace();
extern void		ellipse();

/* rt_memalloc.c */
MGED_EXTERN(unsigned long rt_memalloc, (struct mem_map **pp, unsigned size) );
MGED_EXTERN(unsigned long rt_memget, (struct mem_map **pp, unsigned int size,
	unsigned int place) );
MGED_EXTERN(void rt_memfree, (struct mem_map **pp, unsigned size, unsigned long addr) );
MGED_EXTERN(void rt_mempurge, (struct mem_map **pp) );
MGED_EXTERN(void rt_memprint, (struct mem_map **pp) );

/* buttons.c */
MGED_EXTERN(void button, (int bnum) );
MGED_EXTERN(void press, (char *str) );
MGED_EXTERN(char *label_button, (int bnum) );
MGED_EXTERN(int not_state, (int desired, char *str) );
MGED_EXTERN(int chg_state, (int from, int to, char *str) );
MGED_EXTERN(void state_err, (char *str) );

MGED_EXTERN(void do_list, (struct bu_vls *outstrp, struct directory *dp, int verbose));

/* history.c */

extern int cmd_prev(), cmd_next();
extern void history_record();


/* cmd.c */

extern void start_catching_output(), stop_catching_output();

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

#define	MAXLINE		10240	/* Maximum number of chars per line */

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
  struct bu_vls command;
  struct timeval start, finish;
  int status;
};

/* internal variables related to the command window(s) */
struct cmd_list {
  struct bu_list l;
  struct dm_list *aim;        /* the drawing window being aimed at */
  struct mged_hist *cur_hist;
  struct bu_vls more_default;
  struct bu_vls name;
};
#define CMD_LIST_NULL ((struct cmd_list *)NULL)

/* defined in cmd.c */
extern Tcl_Interp *interp;
extern struct cmd_list head_cmd_list;
extern struct cmd_list *curr_cmd_list;

/* mged command variables for affecting the user environment */
struct _mged_variables {
	int	autosize;
	int	rateknobs;
        int     adcflag;
        int     scroll_enabled;
    	int	sgi_win_size;
	int	sgi_win_origin[2];
	int	faceplate;
        int     show_menu;
	int     w_axes;  /* world view axes */
	int     v_axes;  /* view axes */
	int     e_axes;  /* edit axes */
        int     send_key;
        int     hot_key;
	int     view;
	int	edit;
        int     context;
	int	predictor;
	double	predictor_advance;
	double	predictor_length;
	double	perspective;	/* >0 implies perspective viewing is on. */
	double	nmg_eu_dist;
	double	eye_sep_dist;	/* >0 implies stereo.  units = "room" mm */
	char	union_lexeme[1024];
	char	intersection_lexeme[1024];
	char	difference_lexeme[1024];
};

#define	MAXARGS		9000	/* Maximum number of args per line */

#define MGED_PROMPT "\rmged> "

/* Flags indicating whether the ogl and sgi display managers have been
 * attached. Defined in dm-ogl.c. 
 * These are necessary to decide whether or not to use direct rendering
 * with ogl.
 */
extern	char	ogl_ogl_used;
extern	char	ogl_sgi_used;

/* Command return codes */

#define CMD_OK		919
#define CMD_BAD		920
#define CMD_MORE	921
#define MORE_ARGS_STR    "more arguments needed::"

