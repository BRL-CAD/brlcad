/*
 *				D G _ O B J . C
 *
 * A drawable geometry object contains methods and attributes
 * for creating/destroying geometry that is ready (i.e. vlists) for
 * display. Much of this code was extracted from MGED and modified
 * to work herein.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Authors -
 *	Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>
#include <math.h>

#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "cmd.h"			/* includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "mater.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "solid.h"
/* XXX this should be done else where? */
int rt_pg_plot(struct bu_list *, struct rt_db_internal *,
	       CONST struct rt_tess_tol *, CONST struct bn_tol *);
int rt_pg_plot_poly(struct bu_list *, struct rt_db_internal *,
		    CONST struct rt_tess_tol *, CONST struct bn_tol *);
int rt_bot_plot(struct bu_list *, struct rt_db_internal *,
		    CONST struct rt_tess_tol *, CONST struct bn_tol *);
int rt_bot_plot_poly(struct bu_list *, struct rt_db_internal *,
		    CONST struct rt_tess_tol *, CONST struct bn_tol *);

#include "./debug.h"

#define DGO_CHECK_WDBP_NULL(_dgop,_interp) \
	if (_dgop->dgo_wdbp == RT_WDB_NULL) \
	{ \
		Tcl_AppendResult(_interp, "Not associated with a database!\n", (char *)NULL); \
		return TCL_ERROR; \
	}	

/*
 *  It is expected that entries on this mater list will be sorted
 *  in strictly ascending order, with no overlaps (ie, monotonicly
 *  increasing).
 */
extern struct mater *rt_material_head;	/* now defined in librt/mater.c */

/* declared in vdraw.c */
extern struct bu_cmdtab vdraw_cmds[];

static int dgo_open_tcl();
static int dgo_close_tcl();
static int dgo_cmd();
static int dgo_headSolid_tcl();
static int dgo_illum_tcl();
static int dgo_label_tcl();
static int dgo_draw_tcl();
static int dgo_erase_tcl();
static int dgo_erase_all_tcl();
static int dgo_who_tcl();
static int dgo_rt_tcl();
static int dgo_vdraw_tcl();
static int dgo_overlay_tcl();
static int dgo_get_autoview_tcl();
static int dgo_zap_tcl();
static int dgo_blast_tcl();
static int dgo_assoc_tcl();
#if 0
static int dgo_tol_tcl();
#endif
static int dgo_rtcheck_tcl();
static int dgo_observer_tcl();
static int dgo_report_tcl();

static union tree *dgo_wireframe_region_end();
static union tree *dgo_wireframe_leaf();
static int dgo_drawtrees();
static void dgo_cvt_vlblock_to_solids();
int dgo_invent_solid();
static void dgo_bound_solid();
static void dgo_drawH_part2();
static void dgo_eraseobjpath();
static void dgo_eraseobjall();
void dgo_eraseobjall_callback();
static void dgo_eraseobj();
static void dgo_color_soltab();
static int dgo_run_rt();
static int dgo_build_tops();
static void dgo_rt_write();
static void dgo_rt_set_eye_model();

void dgo_notify();
static void dgo_print_schain();
static void dgo_print_schain_vlcmds();

struct dg_obj HeadDGObj;		/* head of drawable geometry object list */
static struct solid FreeSolid;		/* head of free solid list */

static struct bu_cmdtab dgo_cmds[] = {
	{"assoc",		dgo_assoc_tcl},
	{"blast",		dgo_blast_tcl},
	{"clear",		dgo_zap_tcl},
	{"close",		dgo_close_tcl},
	{"draw",		dgo_draw_tcl},
	{"erase",		dgo_erase_tcl},
	{"erase_all",		dgo_erase_all_tcl},
	{"ev",			dgo_draw_tcl},
	{"get_autoview",	dgo_get_autoview_tcl},
	{"headSolid",		dgo_headSolid_tcl},
	{"illum",		dgo_illum_tcl},
	{"label",		dgo_label_tcl},
	{"observer",		dgo_observer_tcl},
	{"overlay",		dgo_overlay_tcl},
	{"report",		dgo_report_tcl},
	{"rt",			dgo_rt_tcl},
	{"rtcheck",		dgo_rtcheck_tcl},
#if 0
	{"tol",			dgo_tol_tcl},
#endif
	{"vdraw",		dgo_vdraw_tcl},
	{"who",			dgo_who_tcl},
	{"zap",			dgo_zap_tcl},
	{(char *)0,		(int (*)())0}
};

/*
 *			D G O _ C M D
 *
 * Generic interface for drawable geometry objects.
 * Usage:
 *        procname cmd ?args?
 *
 * Returns: result of dbo command.
 */
static int
dgo_cmd(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	return bu_cmd(clientData, interp, argc, argv, dgo_cmds, 1);
}

int
Dgo_Init(interp)
     Tcl_Interp *interp;
{
	BU_LIST_INIT(&HeadDGObj.l);
	BU_LIST_INIT(&FreeSolid.l);

	(void)Tcl_CreateCommand(interp, "dg_open", dgo_open_tcl,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
static void
dgo_deleteProc(clientData)
     ClientData clientData;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	/* free observers */
	bu_observer_free(&dgop->dgo_observers);

	bu_vls_free(&dgop->dgo_name);

	BU_LIST_DEQUEUE(&dgop->l);
	bu_free((genptr_t)dgop, "dgo_deleteProc: dgop");
}

/*
 * Close a drawable geometry object.
 *
 * USAGE:
 *	  procname close
 */
static int
dgo_close_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call dgo_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&dgop->dgo_name));

	return TCL_OK;
}

/*
 * Open/create a drawable geometry object that's associated with the
 * database object "rt_wdb".
 *
 * USAGE:
 *	  dgo_open [name rt_wdb]
 */
static int
dgo_open_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop;
	struct rt_wdb *wdbp;
	struct bu_vls vls;

	if (argc == 1) {
		/* get list of drawable geometry objects */
		for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l))
			Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_name), " ", (char *)NULL);

		return TCL_OK;
	}

	if (argc != 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_open");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* search for database object */
	for (BU_LIST_FOR(wdbp, rt_wdb, &rt_g.rtg_headwdb.l)) {
		if (strcmp(bu_vls_addr(&wdbp->wdb_name), argv[2]) == 0)
			break;
	}

	if (BU_LIST_IS_HEAD(wdbp, &rt_g.rtg_headwdb.l)) {
#if 0
		Tcl_AppendResult(interp, "dgo_open: bad database object - ", argv[2],
				 "\n", (char *)NULL);
		return TCL_ERROR;
#else
		wdbp = RT_WDB_NULL;
#endif
	}

	/* acquire dg_obj struct */
	BU_GETSTRUCT(dgop,dg_obj);

	/* initialize dg_obj */
	bu_vls_init(&dgop->dgo_name);
	bu_vls_strcpy(&dgop->dgo_name,argv[1]);
	dgop->dgo_wdbp = wdbp;
	BU_LIST_INIT(&dgop->dgo_headSolid);
	BU_LIST_INIT(&dgop->dgo_headVDraw);
	BU_LIST_INIT(&dgop->dgo_observers.l);

#if 0
	/* initilize tolerance structures */
	dgop->dgo_ttol.magic = RT_TESS_TOL_MAGIC;
	dgop->dgo_ttol.abs = 0.0;		/* disabled */
	dgop->dgo_ttol.rel = 0.01;
	dgop->dgo_ttol.norm = 0.0;		/* disabled */

	dgop->dgo_tol.magic = BN_TOL_MAGIC;
	dgop->dgo_tol.dist = 0.005;
	dgop->dgo_tol.dist_sq = dgop->dgo_tol.dist * dgop->dgo_tol.dist;
	dgop->dgo_tol.perp = 1e-6;
	dgop->dgo_tol.para = 1 - dgop->dgo_tol.perp;

	/* initialize tree state */
	dgop->dgo_initial_tree_state = rt_initial_tree_state;  /* struct copy */
	dgop->dgo_initial_tree_state.ts_ttol = &dgop->dgo_ttol;
	dgop->dgo_initial_tree_state.ts_tol = &dgop->dgo_tol;
#endif

	/* append to list of dg_obj's */
	BU_LIST_APPEND(&HeadDGObj.l,&dgop->l);

	(void)Tcl_CreateCommand(interp,
				bu_vls_addr(&dgop->dgo_name),
				dgo_cmd,
				(ClientData)dgop,
				dgo_deleteProc);

	/* Return new function name as result */
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_name), (char *)NULL);
	return TCL_OK;
}

/****************** Drawable Geometry Object Methods ********************/

/*
 *
 * Usage:
 *        procname headSolid
 *
 * Returns: database object's headSolid.
 */
static int
dgo_headSolid_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc != 2) {
		bu_vls_printf(&vls, "helplib dgo_headSolid");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_printf(&vls, "%lu", (unsigned long)&dgop->dgo_headSolid);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
}

/*
 * Illuminate/highlight database object
 *
 * Usage:
 *        procname illum [-n] obj
 *
 */
static int
dgo_illum_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	register struct solid *sp;
	struct bu_vls vls;
	int found = 0;
	int illum = 1;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc == 4) {
		if (argv[2][0] == '-' && argv[2][1] == 'n')
			illum = 0;
		else
			goto bad;

		--argc;
		++argv;
	}

	if (argc != 3)
		goto bad;

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		register int i;

		for (i = 0; i < sp->s_fullpath.fp_len; ++i) {
			if (*argv[2] == *DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_namep &&
			    strcmp(argv[2], DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_namep) == 0) {
				found = 1;
				if (illum)
					sp->s_iflag = UP;
				else
					sp->s_iflag = DOWN;
			}
		}
	}

	if (!found) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "illum: %s not found", argv[2]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	dgo_notify(dgop, interp);
	return TCL_OK;

bad:
	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, "helplib dgo_illum");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 * Label database objects.
 *
 * Usage:
 *        procname label [-n] obj
 *
 */
static int
dgo_label_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	return TCL_OK;
}

static void
dgo_draw(dgop, interp, argc, argv, kind)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
     int kind;
{

	/* skip past procname and cmd */
	argc -= 2;
	argv += 2;

	/*  First, delete any mention of these objects.
	 *  Silently skip any leading options (which start with minus signs).
	 */
	dgo_eraseobjpath(dgop, interp, argc, argv, LOOKUP_QUIET, 0);
	dgo_drawtrees(dgop, interp, argc, argv, kind);
	dgo_color_soltab(&dgop->dgo_headSolid);
}

/*
 * Prepare database objects for drawing.
 *
 * Usage:
 *        procname draw|ev [args]
 *
 */
static int
dgo_draw_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	int kind;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_draw");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	if (argv[1][0] == 'e' && argv[1][1] == 'v')
		kind = 3;
	else
		kind = 1;

	dgo_draw(dgop, interp, argc, argv, kind);
	dgo_notify(dgop, interp);

	return TCL_OK;
}

/*
 * Erase database objects.
 *
 * Usage:
 *        procname erase object(s)
 *
 */
static int
dgo_erase_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct bu_vls vls;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_erase");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	dgo_eraseobjpath(dgop, interp, argc-2, argv+2, LOOKUP_NOISY, 0);
	dgo_notify(dgop, interp);

	return TCL_OK;
}

/*
 * Usage:
 *        procname erase_all object(s)
 */
static int
dgo_erase_all_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct bu_vls vls;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_erase");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	dgo_eraseobjpath(dgop, interp, argc-2, argv+2, LOOKUP_NOISY, 1);
	dgo_notify(dgop, interp);

	return TCL_OK;
}

/*
 * List the objects currently being drawn.
 *
 * Usage:
 *        procname who [r(eal)|p(hony)|b(oth)]
 */
static int
dgo_who_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	register struct solid *sp;
	int skip_real, skip_phony;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 2 || 3 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help dgo_who");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	skip_real = 0;
	skip_phony = 1;
	if (argc > 2) {
		switch (argv[2][0]) {
		case 'b':
			skip_real = 0;
			skip_phony = 0;
			break;
		case 'p':
			skip_real = 1;
			skip_phony = 0;
			break;
		case 'r':
			skip_real = 0;
			skip_phony = 1;
			break;
		default:
			Tcl_AppendResult(interp,"dgo_who: argument not understood\n", (char *)NULL);
			return TCL_ERROR;
		}
	}
		

	/* Find all unique top-level entries.
	 *  Mark ones already done with s_flag == UP
	 */
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
		sp->s_flag = DOWN;
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		register struct solid *forw;	/* XXX */

		if (sp->s_flag == UP)
			continue;
		if (FIRST_SOLID(sp)->d_addr == RT_DIR_PHONY_ADDR) {
			if (skip_phony) continue;
		} else {
			if (skip_real) continue;
		}
		Tcl_AppendResult(interp, FIRST_SOLID(sp)->d_namep, " ", (char *)NULL);
		sp->s_flag = UP;
		FOR_REST_OF_SOLIDS(forw, sp, &dgop->dgo_headSolid){
			if (FIRST_SOLID(forw) == FIRST_SOLID(sp))
				forw->s_flag = UP;
		}
	}
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
		sp->s_flag = DOWN;

	return TCL_OK;
}

static void
dgo_overlay(dgop, interp, fp, name, char_size)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     FILE *fp;
     char *name;
     double char_size;
{
	int ret;
	struct rt_vlblock *vbp;

	vbp = rt_vlblock_init();
	ret = rt_uplot_to_vlist(vbp, fp, char_size);
	fclose(fp);

	if (ret < 0) {
		rt_vlblock_free(vbp);
		return;
	}

	dgo_cvt_vlblock_to_solids(dgop, interp, vbp, name, 0);
	rt_vlblock_free(vbp);
}

/*
 * Usage:
 *        procname overlay file.plot char_size [name]
 */
static int
dgo_overlay_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	FILE		*fp;
	double char_size;
	char		*name;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 4 || 5 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help dgo_overlay");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	if (sscanf(argv[3], "%lf", &char_size) != 1) {
		Tcl_AppendResult(interp, "dgo_overlay: bad character size - ",
				 argv[3], "\n", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc == 4)
		name = "_PLOT_OVERLAY_";
	else
		name = argv[4];

	if ((fp = fopen(argv[2], "r")) == NULL) {
		Tcl_AppendResult(interp, "dgo_overlay: failed to open file - ",
				 argv[2], "\n", (char *)NULL);

		return TCL_ERROR;
	}

	dgo_overlay(dgop, interp, fp, name, char_size);
	dgo_notify(dgop, interp);

	return TCL_OK;
}

/*
 * Usage:
 *        procname get_autoview
 */
static int
dgo_get_autoview_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct bu_vls vls;
	register struct solid	*sp;
	vect_t		min, max;
	vect_t		minus, plus;
	vect_t		center;
	vect_t		radial;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	VSETALL(min,  INFINITY);
	VSETALL(max, -INFINITY);

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN(min, minus);
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX(max, plus);
	}

	if (BU_LIST_IS_EMPTY(&dgop->dgo_headSolid)) {
		/* Nothing is in view */
		VSETALL(center, 0.0);
		VSETALL(radial, 1000.0);	/* 1 meter */
	} else {
		VADD2SCALE(center, max, min, 0.5);
		VSUB2(radial, max, center);
	}

	if (VNEAR_ZERO(radial , SQRT_SMALL_FASTF))
		VSETALL(radial , 1.0);

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "center {%g %g %g} size %g", V3ARGS(center), radial[X] * 2.0);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 * Usage:
 *        procname rt view_obj arg(s)
 */
static int
dgo_rt_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct view_obj *vop;
	register char **vp;
	register int i;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 3 || MAXARGS < argc) {
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help dgo_rt");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* search for view object */
	for (BU_LIST_FOR(vop, view_obj, &HeadViewObj.l)) {
		if (strcmp(bu_vls_addr(&vop->vo_name), argv[2]) == 0)
			break;
	}

	if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
		Tcl_AppendResult(interp, "dgo_rt: bad view object - ", argv[2],
				 "\n", (char *)NULL);
		return TCL_ERROR;
	}

	vp = &dgop->dgo_rt_cmd[0];
	*vp++ = "rt";
	*vp++ = "-s50";
	*vp++ = "-M";
#if 0
	if (mged_variables->mv_perspective > 0) {
		(void)sprintf(pstring, "-p%g", mged_variables->mv_perspective);
		*vp++ = pstring;
	}
#endif
	for (i=3; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == '-' &&
		    argv[i][2] == '\0') {
			++i;
			break;
		}
		*vp++ = argv[i];
	}
	*vp++ = dgop->dgo_wdbp->dbip->dbi_filename;

	/*
	 * Now that we've grabbed all the options, if no args remain,
	 * append the names of all stuff currently displayed.
	 * Otherwise, simply append the remaining args.
	 */
	if (i == argc) {
		dgop->dgo_rt_cmd_len = vp - dgop->dgo_rt_cmd;
		dgop->dgo_rt_cmd_len += dgo_build_tops(interp,
						       &dgop->dgo_headSolid,
						       vp,
						       &dgop->dgo_rt_cmd[MAXARGS]);
	} else {
		while (i < argc)
			*vp++ = argv[i++];
		*vp = 0;
		vp = &dgop->dgo_rt_cmd[0];
		while (*vp)
			Tcl_AppendResult(interp, *vp++, " ", (char *)NULL);

		Tcl_AppendResult(interp, "\n", (char *)NULL);
	}
	(void)dgo_run_rt(dgop, vop);

	return TCL_OK;
}

/*
 * Usage:
 *        procname vdraw cmd arg(s)
 */
static int
dgo_vdraw_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	return bu_cmd(clientData, interp, argc, argv, vdraw_cmds, 2);
}

static void
dgo_zap(dgop, interp)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
{
	register struct solid *sp;
	register struct solid *nsp;
	struct directory *dp;

	sp = BU_LIST_NEXT(solid, &dgop->dgo_headSolid);
	while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid)) {
		dp = FIRST_SOLID(sp);
		RT_CK_DIR(dp);
		if (dp->d_addr == RT_DIR_PHONY_ADDR) {
			if (db_dirdelete(dgop->dgo_wdbp->dbip, dp) < 0) {
			  Tcl_AppendResult(interp, "dgo_zap: db_dirdelete failed\n", (char *)NULL);
			}
		}

		nsp = BU_LIST_PNEXT(solid, sp);
		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp,&FreeSolid.l);
		sp = nsp;
	}
}

/*
 * Usage:
 *        procname clear|zap
 */
static int
dgo_zap_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc != 2) {
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "helplib dgo_%s", argv[1]);
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	dgo_zap(dgop, interp);
	dgo_notify(dgop, interp);

	return TCL_OK;
}

/*
 * Usage:
 *        procname blast object(s)
 */
static int
dgo_blast_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_blast");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	/* First, clear the screen. */
	dgo_zap(dgop, interp);

	/* Now, draw the new object(s). */
	dgo_draw(dgop, interp, argc, argv, 1);
	dgo_notify(dgop, interp);

	return TCL_OK;
}

#if 0
/*
 * Usage:
 *        procname tol [abs|rel|norm|dist|perp [#]]
 *
 *  abs #	sets absolute tolerance.  # > 0.0
 *  rel #	sets relative tolerance.  0.0 < # < 1.0
 *  norm #	sets normal tolerance, in degrees.
 *  dist #	sets calculational distance tolerance
 *  perp #	sets calculational normal tolerance.
 *
 */
static int
dgo_tol_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct bu_vls vls;
	double	f;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 2 || 4 < argc){
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_tol");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* print all tolerance settings */
	if (argc == 2) {
		Tcl_AppendResult(interp, "Current tolerance settings are:\n", (char *)NULL);
		Tcl_AppendResult(interp, "Tesselation tolerances:\n", (char *)NULL );

		if (dgop->dgo_ttol.abs > 0.0) {
			bu_vls_init(&vls);
			bu_vls_printf(&vls, "\tabs %g mm\n", dgop->dgo_ttol.abs);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		} else {
			Tcl_AppendResult(interp, "\tabs None\n", (char *)NULL);
		}

		if (dgop->dgo_ttol.rel > 0.0) {
			bu_vls_init(&vls);
			bu_vls_printf(&vls, "\trel %g (%g%%)\n",
				      dgop->dgo_ttol.rel, dgop->dgo_ttol.rel * 100.0 );
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		} else {
			Tcl_AppendResult(interp, "\trel None\n", (char *)NULL);
		}

		if (dgop->dgo_ttol.norm > 0.0) {
			int	deg, min;
			double	sec;

			bu_vls_init(&vls);
			sec = dgop->dgo_ttol.norm * bn_radtodeg;
			deg = (int)(sec);
			sec = (sec - (double)deg) * 60;
			min = (int)(sec);
			sec = (sec - (double)min) * 60;

			bu_vls_printf(&vls, "\tnorm %g degrees (%d deg %d min %g sec)\n",
				      dgop->dgo_ttol.norm * bn_radtodeg, deg, min, sec);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		} else {
			Tcl_AppendResult(interp, "\tnorm None\n", (char *)NULL);
		}

		bu_vls_init(&vls);
		bu_vls_printf(&vls,"Calculational tolerances:\n");
		bu_vls_printf(&vls,
			      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)\n",
			      dgop->dgo_tol.dist, dgop->dgo_tol.perp,
			      acos(dgop->dgo_tol.perp)*bn_radtodeg);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* get the specified tolerance */
	if (argc == 3) {
		int status = TCL_OK;

		bu_vls_init(&vls);

		switch (argv[2][0]) {
		case 'a':
			if (dgop->dgo_ttol.abs > 0.0)
				bu_vls_printf(&vls, "%g", dgop->dgo_ttol.abs);
			else
				bu_vls_printf(&vls, "None");
			break;
		case 'r':
			if (dgop->dgo_ttol.rel > 0.0)
				bu_vls_printf(&vls, "%g", dgop->dgo_ttol.rel);
			else
				bu_vls_printf(&vls, "None");
			break;
		case 'n':
			if (dgop->dgo_ttol.norm > 0.0)
				bu_vls_printf(&vls, "%g", dgop->dgo_ttol.norm);
			else
				bu_vls_printf(&vls, "None");
			break;
		case 'd':
			bu_vls_printf(&vls, "%g", dgop->dgo_tol.dist);
			break;
		case 'p':
			bu_vls_printf(&vls, "%g", dgop->dgo_tol.perp);
			break;
		default:
			bu_vls_printf(&vls, "unrecognized tolerance type - %s\n", argv[2]);
			status = TCL_ERROR;
			break;
		}

		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return status;
	}

	/* set the specified tolerance */
	if (sscanf(argv[3], "%lf", &f) != 1) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "bad tolerance - %s\n", argv[3]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	switch (argv[2][0]) {
	case 'a':
		/* Absolute tol */
		if (f <= 0.0)
			dgop->dgo_ttol.abs = 0.0;
		else
			dgop->dgo_ttol.abs = f;
		break;
	case 'r':
		if (f < 0.0 || f >= 1.0) {
			   Tcl_AppendResult(interp,
					    "relative tolerance must be between 0 and 1, not changed\n",
					    (char *)NULL);
			   return TCL_ERROR;
		}
		/* Note that a value of 0.0 will disable relative tolerance */
		dgop->dgo_ttol.rel = f;
		break;
	case 'n':
		/* Normal tolerance, in degrees */
		if (f < 0.0 || f > 90.0) {
			Tcl_AppendResult(interp,
					 "Normal tolerance must be in positive degrees, < 90.0\n",
					 (char *)NULL);
			return TCL_ERROR;
		}
		/* Note that a value of 0.0 or 360.0 will disable this tol */
		dgop->dgo_ttol.norm = f * bn_degtorad;
		break;
	case 'd':
		/* Calculational distance tolerance */
		if (f < 0.0) {
			Tcl_AppendResult(interp,
					 "Calculational distance tolerance must be positive\n",
					 (char *)NULL);
			return TCL_ERROR;
		}
		dgop->dgo_tol.dist = f;
		dgop->dgo_tol.dist_sq = dgop->dgo_tol.dist * dgop->dgo_tol.dist;
		break;
	case 'p':
		/* Calculational perpendicularity tolerance */
		if (f < 0.0 || f > 1.0) {
			Tcl_AppendResult(interp,
					 "Calculational perpendicular tolerance must be from 0 to 1\n",
					 (char *)NULL);
			return TCL_ERROR;
		}
		dgop->dgo_tol.perp = f;
		dgop->dgo_tol.para = 1.0 - f;
		break;
	default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "unrecognized tolerance type - %s\n", argv[2]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	return TCL_OK;
}
#endif

struct rtcheck {
	int			fd;
	FILE			*fp;
	int			pid;
	struct bn_vlblock	*vbp;
	struct bu_list		*vhead;
	double			csize;  
	struct dg_obj		*dgop;
	Tcl_Interp		*interp;
};

/*
 *			D G O _ W A I T _ S T A T U S
 *
 *  Interpret the status return of a wait() system call,
 *  for the edification of the watching luser.
 *  Warning:  This may be somewhat system specific, most especially
 *  on non-UNIX machines.
 */
static void
dgo_wait_status(interp, status)
     Tcl_Interp *interp;
     int	status;
{
	int	sig = status & 0x7f;
	int	core = status & 0x80;
	int	ret = status >> 8;
	struct bu_vls tmp_vls;

	if (status == 0) {
		Tcl_AppendResult(interp, "Normal exit\n", (char *)NULL);
		return;
	}

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "Abnormal exit x%x", status);

	if (core)
		bu_vls_printf(&tmp_vls, ", core dumped");

	if (sig)
		bu_vls_printf(&tmp_vls, ", terminating signal = %d", sig);
	else
		bu_vls_printf(&tmp_vls, ", return (exit) code = %d", ret);

	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), "\n", (char *)NULL);
	bu_vls_free(&tmp_vls);
}

static void
dgo_rtcheck_vector_handler(clientData, mask)
     ClientData clientData;
     int mask;
{
	int value;
	struct solid *sp;
	struct rtcheck *rtcp = (struct rtcheck *)clientData;

	/* Get vector output from rtcheck */
	if ((value = getc(rtcp->fp)) == EOF) {
		int retcode;
		int rpid;

		Tcl_DeleteFileHandler(rtcp->fd);
		fclose(rtcp->fp);

		FOR_ALL_SOLIDS(sp, &rtcp->dgop->dgo_headSolid)
			sp->s_flag = DOWN;

		/* Add overlay */
		dgo_cvt_vlblock_to_solids(rtcp->dgop, rtcp->interp, rtcp->vbp, "OVERLAPS", 0);
		rt_vlblock_free(rtcp->vbp);

		/* wait for the forked process */
		while ((rpid = wait(&retcode)) != rtcp->pid && rpid != -1)
			dgo_wait_status(rtcp->interp, retcode);

		dgo_notify(rtcp->dgop, rtcp->interp);

		/* free rtcp */
		bu_free((genptr_t)rtcp, "dgo_rtcheck_vector_handler: rtcp");

		return;
	}

	(void)rt_process_uplot_value(&rtcp->vhead,
				     rtcp->vbp,
				     rtcp->fp,
				     value,
				     rtcp->csize);
}

static void
dgo_rtcheck_output_handler(clientData, mask)
     ClientData clientData;
     int mask;
{
	int count;
	char line[RT_MAXLINE];
	int fd = (int)((long)clientData & 0xFFFF);	/* fd's will be small */

	/* Get textual output from rtcheck */
#if 0
	if((count = read((int)fd, line, RT_MAXLINE)) == 0){
#else
	if((count = read((int)fd, line, 5120)) == 0){
#endif
		Tcl_DeleteFileHandler(fd);
		close(fd);

		return;
	}

	line[count] = '\0';
	bu_log("%s", line);
}

/*
 * Usage:
 *        procname rtcheck view_obj [args]
 */
static int
dgo_rtcheck_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct view_obj *vop;
	register char **vp;
	register int i;
	int	pid; 	 
	int	i_pipe[2];	/* object reads results for building vectors */
	int	o_pipe[2];	/* object writes view parameters */
	int	e_pipe[2];	/* object reads textual results */
	FILE	*fp;
	struct rtcheck *rtcp;
	vect_t temp;
	vect_t eye_model;

	DGO_CHECK_WDBP_NULL(dgop,interp);

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_rtcheck");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	/* search for view object */
	for (BU_LIST_FOR(vop, view_obj, &HeadViewObj.l)) {
		if (strcmp(bu_vls_addr(&vop->vo_name), argv[2]) == 0)
			break;
	}

	if (BU_LIST_IS_HEAD(vop, &HeadViewObj.l)) {
		Tcl_AppendResult(interp, "dgo_rtcheck: bad view object - ", argv[2],
				 "\n", (char *)NULL);
		return TCL_ERROR;
	}

	vp = &dgop->dgo_rt_cmd[0];
	*vp++ = "rtcheck";
	*vp++ = "-s50";
	*vp++ = "-M";
	for (i=3; i < argc; i++)
		*vp++ = argv[i];
	*vp++ = dgop->dgo_wdbp->dbip->dbi_filename;

	/*
	 * Now that we've grabbed all the options, if no args remain,
	 * append the names of all stuff currently displayed.
	 * Otherwise, simply append the remaining args.
	 */
	if (i == argc) {
		dgop->dgo_rt_cmd_len = vp - dgop->dgo_rt_cmd;
		dgop->dgo_rt_cmd_len += dgo_build_tops(interp,
						       &dgop->dgo_headSolid,
						       vp,
						       &dgop->dgo_rt_cmd[MAXARGS]);
	} else {
		while (i < argc)
			*vp++ = argv[i++];
		*vp = 0;
		vp = &dgop->dgo_rt_cmd[0];
		while (*vp)
			Tcl_AppendResult(interp, *vp++, " ", (char *)NULL);

		Tcl_AppendResult(interp, "\n", (char *)NULL);
	}

	(void)pipe(i_pipe);
	(void)pipe(o_pipe);
	(void)pipe(e_pipe);

	if ((pid = fork()) == 0) {
		/* Redirect stdin, stdout and stderr */
		(void)close(0);
		(void)dup(o_pipe[0]);
		(void)close(1);
		(void)dup(i_pipe[1]);
		(void)close(2);
		(void)dup(e_pipe[1]);

		/* close pipes */
		(void)close(i_pipe[0]);
		(void)close(i_pipe[1]);
		(void)close(o_pipe[0]);
		(void)close(o_pipe[1]);
		(void)close(e_pipe[0]);
		(void)close(e_pipe[1]);

		for (i=3; i < 20; i++)
			(void)close(i);

		(void)execvp(dgop->dgo_rt_cmd[0], dgop->dgo_rt_cmd);
		perror(dgop->dgo_rt_cmd[0]);
		exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close(o_pipe[0]);
	fp = fdopen(o_pipe[1], "w"); 
#if 1
	VSET(temp, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye_model, vop->vo_view2model, temp);
#else
	dgo_rt_set_eye_model(dgop, vop, eye_model);
#endif
	dgo_rt_write(dgop, vop, fp, eye_model);

	(void)fclose(fp);

	/* close write end of pipes */
	(void)close(i_pipe[1]);
	(void)close(e_pipe[1]);

	BU_GETSTRUCT(rtcp, rtcheck);

	/* initialize the rtcheck struct */
	rtcp->fd = i_pipe[0];
	rtcp->fp = fdopen(i_pipe[0], "r");
	rtcp->pid = pid;
	rtcp->vbp = rt_vlblock_init();
	rtcp->vhead = rt_vlblock_find(rtcp->vbp, 0xFF, 0xFF, 0x00);
	rtcp->csize = vop->vo_scale * 0.01;
	rtcp->dgop = dgop;
	rtcp->interp = interp;

	/* register file handlers */
	Tcl_CreateFileHandler(i_pipe[0], TCL_READABLE,
			      dgo_rtcheck_vector_handler, (ClientData)rtcp);
	Tcl_CreateFileHandler(e_pipe[0], TCL_READABLE,
			      dgo_rtcheck_output_handler, (ClientData)e_pipe[0]);

	return TCL_OK;
}

/*
 * Associate this drawable geometry object with a database object.
 *
 * Usage:
 *        procname assoc [wdb_obj]
 */
static int
dgo_assoc_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct rt_wdb *wdbp;
	struct bu_vls vls;

	/* Get associated database object */
	if (argc == 2) {
		if (dgop->dgo_wdbp == RT_WDB_NULL)
			Tcl_AppendResult(interp, (char *)NULL);
		else
			Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_wdbp->wdb_name), (char *)NULL);
		return TCL_OK;
	}

	/* Set associated database object */
	if (argc == 3) {
		/* search for database object */
		for (BU_LIST_FOR(wdbp, rt_wdb, &rt_g.rtg_headwdb.l)) {
			if (strcmp(bu_vls_addr(&wdbp->wdb_name), argv[2]) == 0)
				break;
		}

		if (BU_LIST_IS_HEAD(wdbp, &rt_g.rtg_headwdb.l))
			wdbp = RT_WDB_NULL;

		if (dgop->dgo_wdbp != RT_WDB_NULL)
			dgo_zap(dgop, interp);

		dgop->dgo_wdbp = wdbp;
		dgo_notify(dgop, interp);

		return TCL_OK;
	}

	/* return help message */
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib dgo_assoc");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return TCL_ERROR;
}

/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 *	  procname observer cmd [args]
 *
 */
static int
dgo_observer_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	if (argc < 3) {
		struct bu_vls vls;

		/* return help message */
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_observer");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	return bu_cmd((ClientData)&dgop->dgo_observers,
		      interp, argc - 2, argv + 2, bu_observer_cmds, 0);
}

/*
 *  Report information about solid table, and per-solid VLS
 */
static int
dgo_report_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	int		lvl = 0;
	struct dg_obj	*dgop = (struct dg_obj *)clientData;

	if (argc < 2 || 3 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_report");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argc == 3)
		lvl = atoi(argv[2]);

	if (lvl <= 3)
		dgo_print_schain(dgop, interp, lvl);
	else
		dgo_print_schain_vlcmds(dgop, interp);

	return TCL_OK;
}

/****************** utility routines ********************/

struct dg_client_data {
	struct dg_obj		*dgop;
	Tcl_Interp		*interp;
	int			draw_nmg_only;
	int			nmg_triangulate;
	int			draw_wireframes;
	int			draw_normals;
	int			draw_solid_lines_only;
	int			draw_no_surfaces;
	int			shade_per_vertex_normals;
	int			draw_edge_uses;
	int			wireframe_color_override;
	int			wireframe_color[3];
	int			fastpath_count;			/* statistics */
	int			do_not_draw_nmg_solids_during_debugging;
	struct bn_vlblock	*draw_edge_uses_vbp;
};

static union tree *
dgo_wireframe_region_end(tsp, pathp, curtree, client_data)
     register struct db_tree_state	*tsp;
     struct db_full_path		*pathp;
     union tree				*curtree;
     genptr_t				client_data;
{
	return (curtree);
}

/*
 *			D G O _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
static union tree *
dgo_wireframe_leaf(tsp, pathp, ip, client_data)
     struct db_tree_state	*tsp;
     struct db_full_path	*pathp;
     struct rt_db_internal	*ip;
     genptr_t			client_data;
{
	union tree	*curtree;
	int		dashflag;		/* draw with dashed lines */
	struct bu_list	vhead;
	struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	RT_CK_RESOURCE(tsp->ts_resp);

	BU_LIST_INIT(&vhead);

	if (rt_g.debug&DEBUG_TREEWALK) {
		char	*sofar = db_path_to_string(pathp);

		Tcl_AppendResult(dgcdp->interp, "dgo_wireframe_leaf(",
				 ip->idb_meth->ft_name,
				 ") path='", sofar, "'\n", (char *)NULL);
		bu_free((genptr_t)sofar, "path string");
	}

	if (dgcdp->draw_solid_lines_only)
		dashflag = 0;
	else
		dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));

	RT_CK_DB_INTERNAL(ip);

	if (ip->idb_meth->ft_plot(&vhead, ip,
				   tsp->ts_ttol,
				   tsp->ts_tol) < 0) {
		Tcl_AppendResult(dgcdp->interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
				 ": plot failure\n", (char *)NULL);
		return (TREE_NULL);		/* ERROR */
	}

	/*
	 * XXX HACK CTJ - dgo_drawH_part2 sets the default color of a
	 * solid by looking in tps->ts_mater.ma_color, for pseudo
	 * solids, this needs to be something different and drawH
	 * has no idea or need to know what type of solid this is.
	 */
	if (ip->idb_type == ID_GRIP) {
		int r,g,b;
		r= tsp->ts_mater.ma_color[0];
		g= tsp->ts_mater.ma_color[1];
		b= tsp->ts_mater.ma_color[2];
		tsp->ts_mater.ma_color[0] = 0;
		tsp->ts_mater.ma_color[1] = 128;
		tsp->ts_mater.ma_color[2] = 128;
		dgo_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
		tsp->ts_mater.ma_color[0] = r;
		tsp->ts_mater.ma_color[1] = g;
		tsp->ts_mater.ma_color[2] = b;
	} else {
		dgo_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	}

	/* Indicate success by returning something other than TREE_NULL */
	RT_GET_TREE(curtree, tsp->ts_resp);
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;

	return (curtree);
}

/*
 *			D G O _ N M G _ R E G I O N _ S T A R T
 *
 *  When performing "ev" on a region, consider whether to process
 *  the whole subtree recursively.
 *  Normally, say "yes" to all regions by returning 0.
 *
 *  Check for special case:  a region of one solid, which can be
 *  directly drawn as polygons without going through NMGs.
 *  If we draw it here, then return -1 to signal caller to ignore
 *  further processing of this region.
 *  A hack to view polygonal models (converted from FASTGEN) more rapidly.
 */
int
dgo_nmg_region_start(tsp, pathp, combp, client_data)
     struct db_tree_state		*tsp;
     struct db_full_path		*pathp;
     CONST struct rt_comb_internal	*combp;
     genptr_t				client_data;
{
	union tree		*tp;
	struct directory	*dp;
	struct rt_db_internal	intern;
	mat_t			xform;
	matp_t			matp;
	struct bu_list		vhead;
	struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

	if (rt_g.debug&DEBUG_TREEWALK) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("dgo_nmg_region_start(%s)\n", sofar);
		bu_free((genptr_t)sofar, "path string");
		rt_pr_tree( combp->tree, 1 );
		db_pr_tree_state(tsp);
	}

	RT_CK_DBI(tsp->ts_dbip);
	RT_CK_RESOURCE(tsp->ts_resp);

	BU_LIST_INIT(&vhead);

	RT_CK_COMB(combp);
	tp = combp->tree;
	if (!tp)
		return( -1 );
	RT_CK_TREE(tp);
	if (tp->tr_l.tl_op != OP_DB_LEAF)
		return 0;	/* proceed as usual */

	/* The subtree is a single node.  It may be a combination, though */

	/* Fetch by name, check to see if it's an easy type */
	dp = db_lookup( tsp->ts_dbip, tp->tr_l.tl_name, LOOKUP_NOISY );
	if (!dp)
		return 0;	/* proceed as usual */
	if (tsp->ts_mat) {
		if (tp->tr_l.tl_mat) {
			matp = xform;
			bn_mat_mul(xform, tsp->ts_mat, tp->tr_l.tl_mat);
		} else {
			matp = tsp->ts_mat;
		}
	} else {
		if (tp->tr_l.tl_mat) {
			matp = tp->tr_l.tl_mat;
		} else {
			matp = (matp_t)NULL;
		}
	}
	if (rt_db_get_internal(&intern, dp, tsp->ts_dbip, matp, &rt_uniresource) < 0)
		return 0;	/* proceed as usual */

	switch (intern.idb_type) {
	case ID_POLY:
		{
			if (rt_g.debug&DEBUG_TREEWALK) {
				bu_log("fastpath draw ID_POLY %s\n", dp->d_namep);
			}
			if (dgcdp->draw_wireframes) {
				(void)rt_pg_plot( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			} else {
				(void)rt_pg_plot_poly( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			}
		}
		goto out;
	case ID_BOT:
		{
			if (rt_g.debug&DEBUG_TREEWALK) {
				bu_log("fastpath draw ID_BOT %s\n", dp->d_namep);
			}
			if (dgcdp->draw_wireframes) {
				(void)rt_bot_plot( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			} else {
				(void)rt_bot_plot_poly( &vhead, &intern, tsp->ts_ttol, tsp->ts_tol );
			}
		}
		goto out;
	case ID_COMBINATION:
	default:
		break;
	}
	rt_db_free_internal(&intern, tsp->ts_resp);
	return 0;

out:
	/* Successful fastpath drawing of this solid */
	db_add_node_to_full_path(pathp, dp);
	dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);
	DB_FULL_PATH_POP(pathp);
	rt_db_free_internal(&intern, tsp->ts_resp);
	dgcdp->fastpath_count++;
	return -1;	/* SKIP THIS REGION */
}

/*
 *			D G O _ N M G _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
static union tree *
dgo_nmg_region_end(tsp, pathp, curtree, client_data)
     register struct db_tree_state	*tsp;
     struct db_full_path		*pathp;
     union tree				*curtree;
     genptr_t				client_data;
{
	struct nmgregion	*r;
	struct bu_list		vhead;
	int			failed;
	struct dg_client_data *dgcdp = (struct dg_client_data *)client_data;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);
	RT_CK_RESOURCE(tsp->ts_resp);

	BU_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(dgcdp->interp, "dgo_nmg_region_end() path='", sofar,
			   "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( curtree->tr_op == OP_NOP )  return  curtree;

	if ( !dgcdp->draw_nmg_only ) {
		if( BU_SETJUMP )
		{
			char  *sofar = db_path_to_string(pathp);

			BU_UNSETJUMP;

			Tcl_AppendResult(dgcdp->interp, "WARNING: Boolean evaluation of ", sofar,
				" failed!!!\n", (char *)NULL );
			bu_free((genptr_t)sofar, "path string");
			if( curtree )
				db_free_tree( curtree, tsp->ts_resp );
			return (union tree *)NULL;
		}
		failed = nmg_boolean( curtree, *tsp->ts_m, tsp->ts_tol, tsp->ts_resp );
		BU_UNSETJUMP;
		if( failed )  {
			db_free_tree( curtree, tsp->ts_resp );
			return (union tree *)NULL;
		}
	}
	else if( curtree->tr_op != OP_NMG_TESS )
	{
	  Tcl_AppendResult(dgcdp->interp, "Cannot use '-d' option when Boolean evaluation is required\n", (char *)NULL);
	  db_free_tree( curtree, tsp->ts_resp );
	  return (union tree *)NULL;
	}
	r = curtree->tr_d.td_r;
	NMG_CK_REGION(r);

	if( dgcdp->do_not_draw_nmg_solids_during_debugging && r )  {
		db_free_tree( curtree, tsp->ts_resp );
		return (union tree *)NULL;
	}

	if (dgcdp->nmg_triangulate) {
		if (BU_SETJUMP) {
			char  *sofar = db_path_to_string(pathp);

			BU_UNSETJUMP;

			Tcl_AppendResult(dgcdp->interp, "WARNING: Triangulation of ", sofar,
				" failed!!!\n", (char *)NULL );
			bu_free((genptr_t)sofar, "path string");
			if( curtree )
				db_free_tree( curtree, tsp->ts_resp );
			return (union tree *)NULL;
		}
		nmg_triangulate_model(*tsp->ts_m, tsp->ts_tol);
		BU_UNSETJUMP;
	}

	if( r != 0 )  {
		int	style;
		/* Convert NMG to vlist */
		NMG_CK_REGION(r);

		if (dgcdp->draw_wireframes) {
			/* Draw in vector form */
			style = NMG_VLIST_STYLE_VECTOR;
		} else {
			/* Default -- draw polygons */
			style = NMG_VLIST_STYLE_POLYGON;
		}
		if (dgcdp->draw_normals) {
			style |= NMG_VLIST_STYLE_VISUALIZE_NORMALS;
		}
		if (dgcdp->shade_per_vertex_normals) {
			style |= NMG_VLIST_STYLE_USE_VU_NORMALS;
		}
		if (dgcdp->draw_no_surfaces) {
			style |= NMG_VLIST_STYLE_NO_SURFACES;
		}
		nmg_r_to_vlist(&vhead, r, style);

		dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL, dgcdp);

		if (dgcdp->draw_edge_uses) {
			nmg_vlblock_r(dgcdp->draw_edge_uses_vbp, r, 1);
		}
		/* NMG region is no longer necessary, only vlist remains */
		db_free_tree( curtree, tsp->ts_resp );
		return (union tree *)NULL;
	}

	/* Return tree -- it needs to be freed (by caller) */
	return curtree;
}

/*
 *			D G O _ D R A W T R E E S
 *
 *  This routine is the drawable geometry object's analog of rt_gettrees().
 *  Add a set of tree hierarchies to the active set.
 *  Note that argv[0] should be ignored, it has the command name in it.
 *
 *  Kind =
 *	1	regular wireframes
 *	2	big-E
 *	3	NMG polygons
 *  
 *  Returns -
 *  	0	Ordinarily
 *	-1	On major error
 */
static int
dgo_drawtrees(dgop, interp, argc, argv, kind)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
     int	kind;
{
	int		ret = 0;
	register int	c;
	int		ncpu = 1;
	int		dgo_nmg_use_tnurbs = 0;
	int		dgo_enable_fastpath = 0;
	struct model	*dgo_nmg_model;
	struct dg_client_data *dgcdp;

	RT_CHECK_DBI(dgop->dgo_wdbp->dbip);

	if (argc <= 0)
		return(-1);	/* FAIL */

	BU_GETSTRUCT(dgcdp, dg_client_data);
	dgcdp->dgop = dgop;
	dgcdp->interp = interp;

	/* Initial values for options, must be reset each time */
	dgcdp->draw_nmg_only = 0;	/* no booleans */
	dgcdp->nmg_triangulate = 1;
	dgcdp->draw_wireframes = 0;
	dgcdp->draw_normals = 0;
	dgcdp->draw_solid_lines_only = 0;
	dgcdp->draw_no_surfaces = 0;
	dgcdp->shade_per_vertex_normals = 0;
	dgcdp->draw_edge_uses = 0;
	dgcdp->wireframe_color_override = 0;
	dgcdp->fastpath_count = 0;

	/* default color - red */
	dgcdp->wireframe_color[0] = 255;
	dgcdp->wireframe_color[1] = 0;
	dgcdp->wireframe_color[2] = 0;

	dgo_enable_fastpath = 0;

	/* Parse options. */
	bu_optind = 0;		/* re-init bu_getopt() */
	while ((c = bu_getopt(argc,argv,"dfnqstuvwSTP:C:")) != EOF) {
		switch (c) {
		case 'u':
			dgcdp->draw_edge_uses = 1;
			break;
		case 's':
			dgcdp->draw_solid_lines_only = 1;
			break;
		case 't':
			dgo_nmg_use_tnurbs = 1;
			break;
		case 'v':
			dgcdp->shade_per_vertex_normals = 1;
			break;
		case 'w':
			dgcdp->draw_wireframes = 1;
			break;
		case 'S':
			dgcdp->draw_no_surfaces = 1;
			break;
		case 'T':
			dgcdp->nmg_triangulate = 0;
			break;
		case 'n':
			dgcdp->draw_normals = 1;
			break;
		case 'P':
			ncpu = atoi(bu_optarg);
			break;
		case 'q':
			dgcdp->do_not_draw_nmg_solids_during_debugging = 1;
			break;
		case 'd':
			dgcdp->draw_nmg_only = 1;
			break;
		case 'f':
			dgo_enable_fastpath = 1;
			break;
		case 'C':
			{
				int		r,g,b;
				register char	*cp = bu_optarg;

				r = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				g = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				b = atoi(cp);

				if( r < 0 || r > 255 )  r = 255;
				if( g < 0 || g > 255 )  g = 255;
				if( b < 0 || b > 255 )  b = 255;

				dgcdp->wireframe_color_override = 1;
				dgcdp->wireframe_color[0] = r;
				dgcdp->wireframe_color[1] = g;
				dgcdp->wireframe_color[2] = b;
			}
			break;
		default:
			{
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls, "help %s", argv[0]);
				Tcl_Eval(interp, bu_vls_addr(&vls));
				bu_vls_free(&vls);
				bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");

				return TCL_ERROR;
			}
		}
	}
	argc -= bu_optind;
	argv += bu_optind;

	switch (kind) {
	default:
	  Tcl_AppendResult(interp, "ERROR, bad kind\n", (char *)NULL);
	  bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");
	  return(-1);
	case 1:		/* Wireframes */
		ret = db_walk_tree(dgop->dgo_wdbp->dbip, argc, (CONST char **)argv,
			ncpu,
			&dgop->dgo_wdbp->wdb_initial_tree_state,
			0,			/* take all regions */
			dgo_wireframe_region_end,
			dgo_wireframe_leaf, (genptr_t)dgcdp);
		break;
	case 2:		/* Big-E */
		Tcl_AppendResult(interp, "drawtrees:  can't do big-E here\n", (char *)NULL);
		bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");
		return (-1);
	case 3:
		{
		/* NMG */
	  	dgo_nmg_model = nmg_mm();
		dgop->dgo_wdbp->wdb_initial_tree_state.ts_m = &dgo_nmg_model;
	  	if (dgcdp->draw_edge_uses) {
			Tcl_AppendResult(interp, "Doing the edgeuse thang (-u)\n", (char *)NULL);
			dgcdp->draw_edge_uses_vbp = rt_vlblock_init();
	  	}

		ret = db_walk_tree(dgop->dgo_wdbp->dbip, argc, (CONST char **)argv,
				   ncpu,
				   &dgop->dgo_wdbp->wdb_initial_tree_state,
				   dgo_enable_fastpath ? dgo_nmg_region_start : 0,
				   dgo_nmg_region_end,
				   dgo_nmg_use_tnurbs ? nmg_booltree_leaf_tnurb : nmg_booltree_leaf_tess,
				   (genptr_t)dgcdp);

	  	if (dgcdp->draw_edge_uses) {
	  		dgo_cvt_vlblock_to_solids(dgop->dgo_wdbp->dbip, interp,
						  dgcdp->draw_edge_uses_vbp, "_EDGEUSES_", 0);
	  		rt_vlblock_free(dgcdp->draw_edge_uses_vbp);
			dgcdp->draw_edge_uses_vbp = (struct bn_vlblock *)NULL;
 	  	}

		/* Destroy NMG */
		nmg_km(dgo_nmg_model);
	  	break;
	  }
	}
	if (dgcdp->fastpath_count) {
		bu_log("%d region%s rendered through polygon fastpath\n",
		       dgcdp->fastpath_count, dgcdp->fastpath_count==1?"":"s");
	}

	bu_free((genptr_t)dgcdp, "dgo_drawtrees: dgcdp");

	if (ret < 0)
		return (-1);

	return (0);	/* OK */
}


/*
 *			C V T _ V L B L O C K _ T O _ S O L I D S
 */
static void
dgo_cvt_vlblock_to_solids(dgop, interp, vbp, name, copy)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     struct bn_vlblock *vbp;
     char *name;
     int copy;
{
	int		i;
	char		shortname[32];
	char		namebuf[64];

	strncpy(shortname, name, 16-6);
	shortname[16-6] = '\0';

	for( i=0; i < vbp->nused; i++ )  {
		if (BU_LIST_IS_EMPTY(&(vbp->head[i])))
			continue;

		sprintf(namebuf, "%s%lx",
			shortname, vbp->rgb[i]);
		dgo_invent_solid(dgop, interp, namebuf, &vbp->head[i], vbp->rgb[i], copy);
	}
}

/*
 *			I N V E N T _ S O L I D
 *
 *  Invent a solid by adding a fake entry in the database table,
 *  adding an entry to the solid table, and populating it with
 *  the given vector list.
 *
 *  This parallels much of the code in dodraw.c
 */
int
dgo_invent_solid(dgop, interp, name, vhead, rgb, copy)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     char		*name;
     struct bu_list	*vhead;
     long		rgb;
     int		copy;
{
	register struct directory	*dp;
	struct directory		*dpp[2] = {DIR_NULL, DIR_NULL};
	register struct solid		*sp;

	if (dgop->dgo_wdbp->dbip == DBI_NULL)
		return 0;

	if ((dp = db_lookup(dgop->dgo_wdbp->dbip, name, LOOKUP_QUIET)) != DIR_NULL) {
		if (dp->d_addr != RT_DIR_PHONY_ADDR) {
			Tcl_AppendResult(interp, "dgo_invent_solid(", name,
					 ") would clobber existing database entry, ignored\n", (char *)NULL);
			return (-1);
		}

		/*
		 * Name exists from some other overlay,
		 * zap any associated solids
		 */
		dpp[0] = dp;
		dgo_eraseobjall(dgop, interp, dpp);
	}
	/* Need to enter phony name in directory structure */
	dp = db_diradd(dgop->dgo_wdbp->dbip,  name, RT_DIR_PHONY_ADDR, 0, DIR_SOLID, NULL);

	/* Obtain a fresh solid structure, and fill it in */
	GET_SOLID(sp,&FreeSolid.l);

	if (copy) {
		BU_LIST_INIT( &(sp->s_vlist) );
		rt_vlist_copy( &(sp->s_vlist), vhead );
	} else {
		/* For efficiency, just swipe the vlist */
		BU_LIST_APPEND_LIST( &(sp->s_vlist), vhead );
		BU_LIST_INIT(vhead);
	}
	dgo_bound_solid(interp, sp);

	/* set path information -- this is a top level node */
	db_add_node_to_full_path( &sp->s_fullpath, dp );

	sp->s_iflag = DOWN;
	sp->s_soldash = 0;
	sp->s_Eflag = 1;		/* Can't be solid edited! */
	sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
	sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
	sp->s_color[2] = sp->s_basecolor[2] = (rgb    ) & 0xFF;
	sp->s_regionid = 0;
	sp->s_dlist = BU_LIST_LAST(solid, &dgop->dgo_headSolid)->s_dlist + 1;

	/* Solid successfully drawn, add to linked list of solid structs */
	BU_LIST_APPEND(dgop->dgo_headSolid.back, &sp->l);

	return (0);		/* OK */
}

/*
 *  Compute the min, max, and center points of the solid.
 *  Also finds s_vlen;
 * XXX Should split out a separate bn_vlist_rpp() routine, for librt/vlist.c
 */
static void
dgo_bound_solid(interp, sp)
     Tcl_Interp *interp;
     register struct solid *sp;
{
	register struct bn_vlist	*vp;
	register double			xmax, ymax, zmax;
	register double			xmin, ymin, zmin;

	xmax = ymax = zmax = -INFINITY;
	xmin = ymin = zmin =  INFINITY;
	sp->s_vlen = 0;
	for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		register int	j;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for (j = 0; j < nused; j++,cmd++,pt++) {
			switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
				/* Has normal vector, not location */
				break;
			case BN_VLIST_LINE_MOVE:
			case BN_VLIST_LINE_DRAW:
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
				V_MIN(xmin, (*pt)[X]);
				V_MAX(xmax, (*pt)[X]);
				V_MIN(ymin, (*pt)[Y]);
				V_MAX(ymax, (*pt)[Y]);
				V_MIN(zmin, (*pt)[Z]);
				V_MAX(zmax, (*pt)[Z]);
				break;
			default:
				{
					struct bu_vls tmp_vls;

					bu_vls_init(&tmp_vls);
					bu_vls_printf(&tmp_vls, "unknown vlist op %d\n", *cmd);
					Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
					bu_vls_free(&tmp_vls);
				}
			}
		}
		sp->s_vlen += nused;
	}

	sp->s_center[X] = (xmin + xmax) * 0.5;
	sp->s_center[Y] = (ymin + ymax) * 0.5;
	sp->s_center[Z] = (zmin + zmax) * 0.5;

	sp->s_size = xmax - xmin;
	V_MAX( sp->s_size, ymax - ymin );
	V_MAX( sp->s_size, zmax - zmin );
}

/*
 *			D M O _ D R A W h _ P A R T 2
 *
 *  Once the vlist has been created, perform the common tasks
 *  in handling the drawn solid.
 *
 *  This routine must be prepared to run in parallel.
 */
static void
dgo_drawH_part2(dashflag, vhead, pathp, tsp, existing_sp, dgcdp)
     int			 dashflag;
     struct bu_list		 *vhead;
     struct db_full_path	 *pathp;
     struct db_tree_state	 *tsp;
     struct solid		 *existing_sp;
     struct dg_client_data	 *dgcdp; 
{
	register struct solid *sp;

	if (!existing_sp) {
		/* Handling a new solid */
		GET_SOLID(sp, &FreeSolid.l);
		/* NOTICE:  The structure is dirty & not initialized for you! */

		sp->s_dlist = BU_LIST_LAST(solid, &dgcdp->dgop->dgo_headSolid)->s_dlist + 1;
	} else {
		/* Just updating an existing solid.
		 *  'tsp' and 'pathpos' will not be used
		 */
		sp = existing_sp;
	}


	/*
	 * Compute the min, max, and center points.
	 */
	BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
	dgo_bound_solid(dgcdp->interp, sp);

	/*
	 *  If this solid is new, fill in it's information.
	 *  Otherwise, don't touch what is already there.
	 */
	if (!existing_sp) {
		/* Take note of the base color */
		if (dgcdp->wireframe_color_override) {
		        /* a user specified the color, so arrange to use it */
			sp->s_uflag = 1;
			sp->s_dflag = 0;
			sp->s_basecolor[0] = dgcdp->wireframe_color[0];
			sp->s_basecolor[1] = dgcdp->wireframe_color[1];
			sp->s_basecolor[2] = dgcdp->wireframe_color[2];
		} else {
			sp->s_uflag = 0;
			if (tsp) {
				if (tsp->ts_mater.ma_color_valid) {
					sp->s_dflag = 0;	/* color specified in db */
					sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
					sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
					sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
				} else {
					sp->s_dflag = 1;	/* default color */
					sp->s_basecolor[0] = 255;
					sp->s_basecolor[1] = 0;
					sp->s_basecolor[2] = 0;
				}
			}
		}
		sp->s_cflag = 0;
		sp->s_flag = DOWN;
		sp->s_iflag = DOWN;
		sp->s_soldash = dashflag;
		sp->s_Eflag = 0;	/* This is a solid */
		db_dup_full_path( &sp->s_fullpath, pathp );
		sp->s_regionid = tsp->ts_regionid;

		/* Add to linked list of solid structs */
		bu_semaphore_acquire(RT_SEM_MODEL);
		BU_LIST_APPEND(dgcdp->dgop->dgo_headSolid.back, &sp->l);
		bu_semaphore_release(RT_SEM_MODEL);
	}

#if 0
	/* Solid is successfully drawn */
	if (!existing_sp) {
		/* Add to linked list of solid structs */
		bu_semaphore_acquire(RT_SEM_MODEL);
		BU_LIST_APPEND(dgcdp->dgop->dgo_headSolid.back, &sp->l);
		bu_semaphore_release(RT_SEM_MODEL);
	} else {
		/* replacing existing solid -- struct already linked in */
		sp->s_flag = UP;
	}
#endif
}

/*
 * This looks for a drawable geometry object that has a matching "dbip"
 * and deletes the solids corresponding to "dp" from the solid list.
 * At the moment this is being called from wdb_obj.c/wdb_kill_tcl() if the
 * object is not phony.
 */
void
dgo_eraseobjall_callback(dbip, interp, dp)
     struct db_i *dbip;
     Tcl_Interp *interp;
     struct directory *dp;
{
	struct dg_obj		*dgop;
	struct directory	*dpp[2] = {DIR_NULL, DIR_NULL};

	dpp[0] = dp;
	for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l))
		/* drawable geometry objects associated database matches */
		if (dgop->dgo_wdbp->dbip == dbip) {
			dgo_eraseobjall(dgop, interp, dpp);
			dgo_notify(dgop, interp);
		}
}

/*
 * Builds an array of directory pointers from argv and calls
 * either dgo_eraseobj or dgo_eraseobjall.
 */
static void
dgo_eraseobjpath(dgop, interp, argc, argv, noisy, all)
     struct dg_obj *dgop;
     Tcl_Interp	*interp;
     int	argc;
     char	**argv;
     int	noisy;	
     int	all;
{
	register struct directory *dp;
	register int i;
	struct bu_vls vls;
	Tcl_Obj *save_result;

	save_result = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(save_result);

	bu_vls_init(&vls);
	for (i = 0; i < argc; i++) {
		int j;
		char *list;
		int ac;
		char **av, **av_orig;
		struct directory **dpp;

#if 0
		bu_vls_trunc(&vls, 0);
		bu_vls_printf(&vls, "split %s /", argv[i]);
		if (Tcl_Eval(interp, bu_vls_addr(&vls)) != TCL_OK) {
			continue;
		}
		list = Tcl_GetStringResult(interp);
#else
		{
			char *begin;
			char *end;
			char *newstr = strdup(argv[i]);

			begin = newstr;
			bu_vls_trunc(&vls, 0);

			while ((end = strchr(begin, '/')) != NULL) {
				*end = '\0';
				bu_vls_printf(&vls, "%s ", begin);
				begin = end + 1;
			}
			bu_vls_printf(&vls, "%s ", begin);
			free((void *)newstr);
		}
		list = bu_vls_addr(&vls);
#endif
		if (Tcl_SplitList(interp, list, &ac, &av_orig) != TCL_OK)
			continue;

		/* skip first element if empty */
		av = av_orig;
		if (*av[0] == '\0') {
			--ac;
			++av;
		}

		/* ignore last element if empty */
		if (*av[ac-1] == '\0')
			--ac;

		dpp = bu_calloc(ac+1, sizeof(struct directory *), "eraseobjpath: directory pointers");
		for (j = 0; j < ac; ++j)
			if ((dp = db_lookup(dgop->dgo_wdbp->dbip, av[j], noisy)) != DIR_NULL)
				dpp[j] = dp;
			else
				goto end;

		dpp[j] = DIR_NULL;

		if (all)
			dgo_eraseobjall(dgop, interp, dpp);
		else
			dgo_eraseobj(dgop, interp, dpp);

	end:
		bu_free((genptr_t)dpp, "eraseobjpath: directory pointers");
		Tcl_Free((char *)av_orig);
	}
	bu_vls_free(&vls);
	Tcl_SetObjResult(interp, save_result);
	Tcl_DecrRefCount(save_result);
}

/*
 *			E R A S E O B J A L L
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object anywhere in their 'path'
 */
static void
dgo_eraseobjall(dgop, interp, dpp)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     register struct directory **dpp;
{
	register struct directory **tmp_dpp;
	register struct solid *sp;
	register struct solid *nsp;
	struct db_full_path	subpath;

	if(dgop->dgo_wdbp->dbip == DBI_NULL)
		return;

	db_full_path_init(&subpath);
	for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)  {
		RT_CK_DIR(*tmp_dpp);
		db_add_node_to_full_path(&subpath, *tmp_dpp);
	}

	sp = BU_LIST_NEXT(solid, &dgop->dgo_headSolid);
	while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid)) {
		nsp = BU_LIST_PNEXT(solid, sp);
		if( db_full_path_subset( &sp->s_fullpath, &subpath ) )  {
			BU_LIST_DEQUEUE(&sp->l);
			FREE_SOLID(sp, &FreeSolid.l);
		}
		sp = nsp;
	}

	if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR) {
		if (db_dirdelete(dgop->dgo_wdbp->dbip, *dpp) < 0) {
			Tcl_AppendResult(interp, "dgo_eraseobjall: db_dirdelete failed\n", (char *)NULL);
		}
	}
	db_free_full_path(&subpath);
}

/*
 *			E R A S E O B J
 *
 * This routine goes through the solid table and deletes all solids
 * from the solid list which contain the specified object at the
 * beginning of their 'path'
 */
static void
dgo_eraseobj(dgop, interp, dpp)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     register struct directory **dpp;
{
	register struct directory **tmp_dpp;
	register struct solid *sp;
	register struct solid *nsp;
	struct db_full_path	subpath;

	if(dgop->dgo_wdbp->dbip == DBI_NULL)
		return;

	if (*dpp == DIR_NULL)
		return;

	db_full_path_init(&subpath);
	for (tmp_dpp = dpp; *tmp_dpp != DIR_NULL; ++tmp_dpp)  {
		RT_CK_DIR(*tmp_dpp);
		db_add_node_to_full_path(&subpath, *tmp_dpp);
	}

	sp = BU_LIST_FIRST(solid, &dgop->dgo_headSolid);
	while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid)) {
		nsp = BU_LIST_PNEXT(solid, sp);
		if( db_full_path_subset( &sp->s_fullpath, &subpath ) )  {
			BU_LIST_DEQUEUE(&sp->l);
			FREE_SOLID(sp, &FreeSolid.l);
		}
		sp = nsp;
	}

	if ((*dpp)->d_addr == RT_DIR_PHONY_ADDR ) {
		if (db_dirdelete(dgop->dgo_wdbp->dbip, *dpp) < 0) {
			Tcl_AppendResult(interp, "dgo_eraseobj: db_dirdelete failed\n", (char *)NULL);
		}
	}
	db_free_full_path(&subpath);
}

/*
 *  			C O L O R _ S O L T A B
 *
 *  Pass through the solid table and set pointer to appropriate
 *  mater structure.
 */
static void
dgo_color_soltab(hsp)
     struct solid *hsp;
{
	register struct solid *sp;
	register struct mater *mp;

	FOR_ALL_SOLIDS(sp, &hsp->l) {
		sp->s_cflag = 0;

	        /* the user specified the color, so use it */
		if (sp->s_uflag) {
			sp->s_color[0] = sp->s_basecolor[0];
			sp->s_color[1] = sp->s_basecolor[1];
			sp->s_color[2] = sp->s_basecolor[2];
			continue;
		}

		for (mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw) {
			if (sp->s_regionid <= mp->mt_high &&
			    sp->s_regionid >= mp->mt_low) {
			    	sp->s_color[0] = mp->mt_r;
			    	sp->s_color[1] = mp->mt_g;
			    	sp->s_color[2] = mp->mt_b;
				goto done;
			}
		}

		/*
		 *  There is no region-id-based coloring entry in the
		 *  table, so use the combination-record ("mater"
		 *  command) based color if one was provided. Otherwise,
		 *  use the default wireframe color.
		 *  This is the "new way" of coloring things.
		 */

		/* use wireframe_default_color */
		if (sp->s_dflag)
		  sp->s_cflag = 1;
		/* Be conservative and copy color anyway, to avoid black */
		sp->s_color[0] = sp->s_basecolor[0];
		sp->s_color[1] = sp->s_basecolor[1];
		sp->s_color[2] = sp->s_basecolor[2];
done: ;
	}
}

/*
 *                    D G O _ B U I L D _ T O P S
 *
 *  Build a command line vector of the tops of all objects in view.
 */
static int
dgo_build_tops(interp, hsp, start, end)
     Tcl_Interp *interp;
     struct solid *hsp;
     char **start;
     register char **end;
{
	register char **vp = start;
	register struct solid *sp;

	/*
	 * Find all unique top-level entries.
	 *  Mark ones already done with s_flag == UP
	 */
	FOR_ALL_SOLIDS(sp, &hsp->l)
		sp->s_flag = DOWN;
	FOR_ALL_SOLIDS(sp, &hsp->l)  {
		register struct solid *forw;
		struct directory *dp = FIRST_SOLID(sp);

		if (sp->s_flag == UP)
			continue;
		if (dp->d_addr == RT_DIR_PHONY_ADDR)
			continue;	/* Ignore overlays, predictor, etc */
		if (vp < end)
			*vp++ = dp->d_namep;
		else  {
		  Tcl_AppendResult(interp, "mged: ran out of comand vector space at ",
				   dp->d_namep, "\n", (char *)NULL);
		  break;
		}
		sp->s_flag = UP;
		for (BU_LIST_PFOR(forw, sp, solid, &hsp->l)) {
			if (FIRST_SOLID(forw) == dp)
				forw->s_flag = UP;
		}
	}
	*vp = (char *) 0;
	return vp-start;
}


/*
 *  			D G O _ R T _ W R I T E
 *  
 *  Write out the information that RT's -M option needs to show current view.
 *  Note that the model-space location of the eye is a parameter,
 *  as it can be computed in different ways.
 */
static void
dgo_rt_write(dgop, vop, fp, eye_model)
     struct dg_obj *dgop;
     struct view_obj *vop;
     FILE *fp;
     vect_t eye_model;
{
	register int	i;
	quat_t		quat;
	register struct solid *sp;

	(void)fprintf(fp, "viewsize %.15e;\n", vop->vo_size);
	quat_mat2quat(quat, vop->vo_rotation );
	(void)fprintf(fp, "orientation %.15e %.15e %.15e %.15e;\n", V4ARGS(quat));
	(void)fprintf(fp, "eye_pt %.15e %.15e %.15e;\n",
		      eye_model[X], eye_model[Y], eye_model[Z] );

	(void)fprintf(fp, "start 0; clean;\n");
	FOR_ALL_SOLIDS (sp, &dgop->dgo_headSolid) {
		for (i=0;i<sp->s_fullpath.fp_len;i++) {
			DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_flags &= ~DIR_USED;
		}
	}
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		for (i=0; i<sp->s_fullpath.fp_len; i++ ) {
			if (!(DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_flags & DIR_USED)) {
				register struct animate *anp;
				for (anp = DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_animate; anp;
				    anp=anp->an_forw) {
					db_write_anim(fp, anp);
				}
				DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_flags |= DIR_USED;
			}
		}
	}

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		for (i=0;i< sp->s_fullpath.fp_len;i++) {
			DB_FULL_PATH_GET(&sp->s_fullpath,i)->d_flags &= ~DIR_USED;
		}
	}
	(void)fprintf(fp, "end;\n");
}

static void
dgo_rt_output_handler(clientData, mask)
     ClientData clientData;
     int mask;
{
	int fd = (int)((long)clientData & 0xFFFF);	/* fd's will be small */
	int count;
#if 0
	char line[10240];
#else
	char line[5120];
#endif

	/* Get data from rt */
	/* if ((count = read((int)fd, line, 10240)) == 0) { */
	if ((count = read((int)fd, line, 5120)) == 0) {
		Tcl_DeleteFileHandler(fd);
		close(fd);

		return;
	}

	line[count] = '\0';

	/*XXX For now just blather to stderr */
	bu_log("%s", line);
}

static void
dgo_rt_set_eye_model(dgop, vop, eye_model)
     struct dg_obj *dgop;
     struct view_obj *vop;
     vect_t eye_model;
{
#if 0
	if (dmp->dm_zclip || mged_variables->mv_perspective_mode) {
		vect_t temp;

		VSET( temp, 0.0, 0.0, 1.0 );
		MAT4X3PNT( eye_model, view_state->vs_view2model, temp );
	}
#endif
	/* not doing zclipping, so back out of geometry */
	register struct solid *sp;
	register int i;
	double  t;
	double  t_in;
	vect_t  direction;
	vect_t  extremum[2];
	vect_t  minus, plus;    /* vers of this solid's bounding box */

	VSET(eye_model, -vop->vo_center[MDX],
	     -vop->vo_center[MDY], -vop->vo_center[MDZ]);

	for (i = 0; i < 3; ++i) {
		extremum[0][i] = INFINITY;
		extremum[1][i] = -INFINITY;
	}

	FOR_ALL_SOLIDS (sp, &dgop->dgo_headSolid) {
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN( extremum[0], minus );
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX( extremum[1], plus );
	}
	VMOVEN(direction, vop->vo_rotation + 8, 3);
	VSCALE(direction, direction, -1.0);
	for (i = 0; i < 3; ++i)
		if (NEAR_ZERO(direction[i], 1e-10))
			direction[i] = 0.0;
	if ((eye_model[X] >= extremum[0][X]) &&
	    (eye_model[X] <= extremum[1][X]) &&
	    (eye_model[Y] >= extremum[0][Y]) &&
	    (eye_model[Y] <= extremum[1][Y]) &&
	    (eye_model[Z] >= extremum[0][Z]) &&
	    (eye_model[Z] <= extremum[1][Z])) {
		t_in = -INFINITY;
		for (i = 0; i < 6; ++i) {
			if (direction[i%3] == 0)
				continue;
			t = (extremum[i/3][i%3] - eye_model[i%3]) /
				direction[i%3];
			if ((t < 0) && (t > t_in))
				t_in = t;
		}
		VJOIN1(eye_model, eye_model, t_in, direction);
	}
}

/*
 *                  D G O _ R U N _ R T
 */
static int
dgo_run_rt(dgop, vop)
     struct dg_obj *dgop;
     struct view_obj *vop; 
{
	register int i;
	FILE *fp_in;
	int pipe_in[2];
	int pipe_err[2];
	vect_t eye_model;

	(void)pipe(pipe_in);
	(void)pipe(pipe_err);

	if ((fork()) == 0) {
		/* make this a process group leader */
		setpgid(0, 0);

		/* Redirect stdin and stderr */
		(void)close(0);
		(void)dup(pipe_in[0]);
		(void)close(2);
		(void)dup(pipe_err[1]);

		/* close pipes */
		(void)close(pipe_in[0]);
		(void)close(pipe_in[1]);
		(void)close(pipe_err[0]);
		(void)close(pipe_err[1]);

		for (i=3; i < 20; i++)
			(void)close(i);

		(void)execvp(dgop->dgo_rt_cmd[0], dgop->dgo_rt_cmd);
		perror(dgop->dgo_rt_cmd[0]);
		exit(16);
	}

	/* As parent, send view information down pipe */
	(void)close(pipe_in[0]);
	fp_in = fdopen(pipe_in[1], "w");

	(void)close(pipe_err[1]);

	dgo_rt_set_eye_model(dgop, vop, eye_model);
	dgo_rt_write(dgop, vop, fp_in, eye_model);
	(void)fclose(fp_in);

	Tcl_CreateFileHandler(pipe_err[0], TCL_READABLE,
			      dgo_rt_output_handler, (ClientData)pipe_err[0]);

	return 0;
}

void
dgo_notify(dgop, interp)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
{
	bu_observer_notify(interp, &dgop->dgo_observers, bu_vls_addr(&dgop->dgo_name));
}

void
dgo_impending_wdb_close(wdbp, interp)
     struct rt_wdb *wdbp;
     Tcl_Interp *interp;
{
	struct dg_obj *dgop;

	for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l))
		if (dgop->dgo_wdbp == wdbp) {
			dgo_zap(dgop, interp);
			dgop->dgo_wdbp = RT_WDB_NULL;
			dgo_notify(dgop, interp);
		}
}

void
dgo_zapall(wdbp, interp)
     struct rt_wdb *wdbp;
     Tcl_Interp *interp;
{
	struct dg_obj *dgop;

	for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l))
		if (dgop->dgo_wdbp == wdbp) {
			dgo_zap(dgop, interp);
			dgo_notify(dgop, interp);
		}
}

/*
 *			D G O _ P R _ S C H A I N
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the information
 *  about each solid structure.
 */
static void
dgo_print_schain(dgop, interp, lvl)
     struct dg_obj	*dgop;
     Tcl_Interp		*interp;
     int		lvl;			/* debug level */
{
	register struct solid		*sp;
	register struct bn_vlist	*vp;
	int				nvlist;
	int				npts;
	struct bu_vls 		vls;

	if (dgop->dgo_wdbp->dbip == DBI_NULL)
		return;

	bu_vls_init(&vls);

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		if (lvl <= -2) {
			/* print only leaves */
			bu_vls_printf(&vls, "%s ", LAST_SOLID(sp)->d_namep);
			continue;
		}

		db_path_to_vls(&vls, &sp->s_fullpath);

		if ((lvl != -1) && (sp->s_iflag == UP))
			bu_vls_printf(&vls, " ILLUM");

		bu_vls_printf(&vls, "\n");

		if (lvl <= 0)
			continue;

		/* convert to the local unit for printing */
		bu_vls_printf(&vls, "  cent=(%.3f,%.3f,%.3f) sz=%g ",
			      sp->s_center[X]*dgop->dgo_wdbp->dbip->dbi_base2local,
			      sp->s_center[Y]*dgop->dgo_wdbp->dbip->dbi_base2local,
			      sp->s_center[Z]*dgop->dgo_wdbp->dbip->dbi_base2local,
			      sp->s_size*dgop->dgo_wdbp->dbip->dbi_base2local);
		bu_vls_printf(&vls, "reg=%d\n",sp->s_regionid);
		bu_vls_printf(&vls, "  basecolor=(%d,%d,%d) color=(%d,%d,%d)%s%s%s\n",
			      sp->s_basecolor[0],
			      sp->s_basecolor[1],
			      sp->s_basecolor[2],
			      sp->s_color[0],
			      sp->s_color[1],
			      sp->s_color[2],
			      sp->s_uflag?" U":"",
			      sp->s_dflag?" D":"",
			      sp->s_cflag?" C":"");

		if (lvl <= 1)
			continue;

		/* Print the actual vector list */
		nvlist = 0;
		npts = 0;
		for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
			register int	i;
			register int	nused = vp->nused;
			register int	*cmd = vp->cmd;
			register point_t *pt = vp->pt;

			BN_CK_VLIST(vp);
			nvlist++;
			npts += nused;

			if (lvl <= 2)
				continue;

			for (i = 0; i < nused; i++,cmd++,pt++) {
				bu_vls_printf(&vls, "  %s (%g, %g, %g)\n",
					      rt_vlist_cmd_descriptions[*cmd],
					      V3ARGS(*pt));
			}
		}

		bu_vls_printf(&vls, "  %d vlist structures, %d pts\n", nvlist, npts);
		bu_vls_printf(&vls, "  %d pts (via rt_ck_vlist)\n", rt_ck_vlist(&(sp->s_vlist)));
	}

	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
}

/*
 *			D G O _ P R _ S C H A I N _ V L C M D S
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the vlist cmds
 *  for each structure.
 */
static void
dgo_print_schain_vlcmds(dgop, interp)
     struct dg_obj	*dgop;
     Tcl_Interp		*interp;
{
	register struct solid		*sp;
	register struct bn_vlist	*vp;
	struct bu_vls 		vls;

	if (dgop->dgo_wdbp->dbip == DBI_NULL)
		return;

	bu_vls_init(&vls);

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid) {
		bu_vls_printf(&vls, "-1 %d %d %d\n",
			      sp->s_color[0],
			      sp->s_color[1],
			      sp->s_color[2]);

		/* Print the actual vector list */
		for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
			register int	i;
			register int	nused = vp->nused;
			register int	*cmd = vp->cmd;
			register point_t *pt = vp->pt;

			BN_CK_VLIST(vp);

			for (i = 0; i < nused; i++, cmd++, pt++)
				bu_vls_printf(&vls, "%d %g %g %g\n", *cmd, V3ARGS(*pt));
		}
	}

	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
}
