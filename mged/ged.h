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

extern double	degtorad, radtodeg;	/* Defined in usepen.c */

/*
 * All GED files are stored in a fixed base unit (MM).
 * These factors convert database unit to local (or working) units.
 */
extern double	base2local, local2base;	/* Defined in dir.c */
extern int 	localunit;		/* the current local unit (index) */
extern char	cur_title[];		/* current model title */

extern int	dmaflag;		/* Set !0 to force a new screen DMA */

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
extern fastf_t	Viewscale;
extern mat_t	Viewrot;
extern mat_t	toViewcenter;
extern mat_t	model2view, view2model;
extern mat_t	model2objview, objview2model;
extern mat_t	modelchanges;		/* full changes this edit */
extern mat_t	incr_change;		/* change(s) from last cycle */

#define VIEWSIZE	(2*Viewscale)
#define VIEWFACTOR	(1/Viewscale)	/* 2.0 / VIEWSIZE */

extern fastf_t	maxview;

/*
 * Identity matrix.  Handy to have around. - initialized in e1.c
 */
extern mat_t	identity;

/* defined in chgview.c */
extern int	drawreg;	/* if > 0, process and draw regions */

/* defined in buttons.c */
extern fastf_t	acc_sc_sol;	/* accumulate solid scale factor */
extern fastf_t	acc_sc[3];	/* accumulate local object scale factors */
extern mat_t	acc_rot_sol;	/* accumulate solid rotations */

/* defined in path.c */
extern int	regmemb;	/* # of members left to process in a region */
extern char	memb_oper;	/* op for present member of proc region */
extern int	reg_pathpos;	/* pathpos of a processed region */

/* defined in dodraw.c */
extern int	reg_error;	/* error encountered in region processing */
extern int	no_memory;	/* flag indicating memory for drawing is used up */

/* defined in menu.c */
extern int	menuflag;	/* flag indicating if a menu item is selected */

/*
 * These variables are global for the benefit of
 * the display portion of dozoom. - defined in adc.c
 */
extern fastf_t	curs_x;		/* cursor X position */
extern fastf_t	curs_y;		/* cursor Y position */
extern fastf_t	c_tdist;	/* Cursor tick distance */
extern fastf_t	angle1;		/* Angle to solid wiper */
extern fastf_t	angle2;		/* Angle to dashed wiper */

/* defined in ged.c */
extern FILE *infile;
extern FILE *outfile;
/*
 *	GED functions referenced in more than one source file:
 */
extern void		dir_build(), buildHrot(), button(), dozoom(),
			pr_schain();
extern void		db_getrec(), db_putrec(), db_delete(), db_alloc(),
			drawHobj(), eraseobj(), finish(), slewview(),
			htov_move(), mat_copy(), mat_idn(),
			mat_inv(), mat_mul(), mat_zero(), matXvec(),
			mmenu_init(), moveHinstance(), moveHobj(), pr_solid(),
			quit(), refresh(), rej_sedit(), sedit(),
			sig2(), dir_print(),
			usepen(), vtoh_move(), setview(),
			adcursor(), mmenu_display(),
			col_item(), col_putchar(), col_eol(), col_pr4v();
extern void		sedit_menu();
extern void		attach(), release(), get_attached();
extern void		(*cur_sigint)();	/* Current SIGINT status */
extern void		aexists(), f_quit();
extern char		*addname(), *strdup();
extern int		clip(), getname(), use_pen(), drawHsolid();
extern struct directory	*combadd(), *dir_add(), *lookup(), **dir_getspace();
extern struct solid *redraw();
extern void		ellipse(), memfree(), mempurge();
extern unsigned long	memalloc(), memget();
extern union record	*db_getmrec();

#ifndef	NULL
#define	NULL		0
#endif

/*
 * "Standard" flag settings
 */
#define UP	0
#define DOWN	1

/*
 * Pointer to solid in solid table to be illuminated. - defined in usepen.c
 */
extern struct solid	*illump;/* == 0 if none, else points to ill. solid */
extern int	sedraw;		/* apply solid editing changes */

/* defined in buttons.c */
extern int	adcflag;	/* angle/distance cursor in use */

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
#define ROOT	UP
#define INNER	DOWN

/*
 * Screen locations
 */
#define XMIN		(-2048)
#define XMAX		(2047)
#define YMIN		(-2048)
#define YMAX		(2047)
#define	MENUXLIM	(-1250)		/* Value to set X lim to for menu */
#define	MENUX		(-2048+115)	/* pixel position for menu, X */
#define	MENUY		1800		/* pixel position for menu, Y */
#define	MENU_DY		(-120)		/* Distance between menu items */

#define TITLE_XBASE	(-2048)		/* pixel X of title line start pos */
#define TITLE_YBASE	(-1920)		/* pixel pos of last title line */
#define SOLID_XBASE	MENUXLIM	/* X to start display text */
#define SOLID_YBASE	( 1920)		/* pixel pos of first solid line */
#define TEXT0_DY	(  -60)		/* #pixels per line, Size 0 */
#define TEXT1_DY	(  -90)		/* #pixels per line, Size 1 */

/*
 *  Editor States
 */
extern int state;			/* (defined in dozoom.c) */
extern char *state_str[];		/* identifying strings */
#define ST_VIEW		1		/* Viewing only */
#define ST_S_PICK	2		/* Picking for Solid Edit */
#define ST_S_EDIT	3		/* Solid Editing */
#define ST_O_PICK	4		/* Picking for Object Edit */
#define ST_O_PATH	5		/* Path select for Object Edit */
#define ST_O_EDIT	6		/* Object Editing */

#define MIN(a,b)	if( (b) < (a) )  a = b
#define MAX(a,b)	if( (b) > (a) )  a = b

/* Acquire storage for a given struct, eg, GETSTRUCT(ptr,structname); */
#define GETSTRUCT(p,str) \
	p = (struct str *)malloc((unsigned)sizeof(struct str)); \
	if( p == (struct str *)0 ) \
		(void)printf("getstruct( p, str ): malloc failed\n");/* cpp magic */
