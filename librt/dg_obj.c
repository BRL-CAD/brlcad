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

#include <fcntl.h>
#include <math.h>
#include <sys/errno.h>
#include "conf.h"
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

#include "./debug.h"

/*
 *  It is expected that entries on this mater list will be sorted
 *  in strictly ascending order, with no overlaps (ie, monotonicly
 *  increasing).
 */
extern struct mater *rt_material_head;	/* now defined in librt/mater.c */

/* found in librt/nmg_plot.c */
extern void (*nmg_plot_anim_upcall)();
extern void (*nmg_vlblock_anim_upcall)();
extern void (*nmg_mged_debug_display_hack)();

/* declared in vdraw.c */
extern struct bu_cmdtab vdraw_cmds[];

HIDDEN int dgo_open_tcl();
HIDDEN int dgo_close_tcl();
HIDDEN int dgo_cmd();
HIDDEN int dgo_headSolid_tcl();
HIDDEN int dgo_draw_tcl();
HIDDEN int dgo_erase_tcl();
HIDDEN int dgo_who_tcl();
HIDDEN int dgo_rt_tcl();
HIDDEN int dgo_vdraw_tcl();
HIDDEN int dgo_overlay_tcl();
HIDDEN int dgo_getview_tcl();

HIDDEN void dgo_plot_anim_upcall_handler();
HIDDEN void dgo_vlblock_anim_upcall_handler();
HIDDEN void dgo_nmg_debug_display_hack();
HIDDEN union tree *dgo_wireframe_region_end();
HIDDEN union tree *dgo_wireframe_leaf();
HIDDEN int dgo_drawtrees();
HIDDEN void dgo_cvt_vlblock_to_solids();
HIDDEN int dgo_invent_solid();
HIDDEN void dgo_bound_solid();
HIDDEN void dgo_drawH_part2();
HIDDEN void dgo_eraseobjall();
HIDDEN void dgo_eraseobj();
HIDDEN void dgo_color_soltab();

HIDDEN int		dgo_draw_nmg_only;
HIDDEN int		dgo_nmg_triangulate;
HIDDEN int		dgo_draw_wireframes;
HIDDEN int		dgo_draw_normals;
HIDDEN int		dgo_draw_solid_lines_only=0;
HIDDEN int		dgo_draw_no_surfaces = 0;
HIDDEN int		dgo_shade_per_vertex_normals=0;
HIDDEN int		dgo_wireframe_color_override;
HIDDEN int		dgo_wireframe_color[3];
HIDDEN struct model	*dgo_nmg_model;
HIDDEN struct db_tree_state	dgo_initial_tree_state;
HIDDEN struct rt_tess_tol	dgo_ttol;
HIDDEN struct bn_tol		dgo_tol;

HIDDEN int dgo_do_not_draw_nmg_solids_during_debugging = 0;
HIDDEN int dgo_draw_edge_uses=0;
HIDDEN int dgo_enable_fastpath = 0;
HIDDEN int dgo_fastpath_count=0;	/* statistics */
HIDDEN struct bn_vlblock	*dgo_draw_edge_uses_vbp;

struct dg_obj HeadDGObj;		/* head of drawable geometry object list */
HIDDEN struct dg_obj *curr_dgop;	/* current drawable geometry object */
HIDDEN Tcl_Interp *curr_interp;		/* current Tcl interpreter */
HIDDEN struct db_i *curr_dbip;		/* current database instance pointer */
HIDDEN struct solid *curr_hsp;		/* current head solid pointer */
HIDDEN struct solid FreeSolid;		/* head of free solid list */

HIDDEN struct bu_cmdtab dgo_cmds[] = {
	"headSolid",		dgo_headSolid_tcl,
	"draw",			dgo_draw_tcl,
	"ev",			dgo_draw_tcl,
	"erase",		dgo_erase_tcl,
	"overlay",		dgo_overlay_tcl,
	"getview",		dgo_getview_tcl,
	"who",			dgo_who_tcl,
	"rt",			dgo_rt_tcl,
	"vdraw",		dgo_vdraw_tcl,
#if 0
	"B",			dgo_blast_tcl,
	"Z",			dgo_zap_tcl,
	"erase_all",		dgo_erase_all_tcl,
	"rtcheck",		dgo_rtcheck_tcl,
	"tol",			dgo_tol_tcl,
#endif
	"close",		dgo_close_tcl,
	(char *)0,		(int (*)())0
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
HIDDEN int
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

	/* initilize tolerance structures */
	dgo_ttol.magic = RT_TESS_TOL_MAGIC;
	dgo_ttol.abs = 0.0;		/* disabled */
	dgo_ttol.rel = 0.01;
	dgo_ttol.norm = 0.0;		/* disabled */

	dgo_tol.magic = BN_TOL_MAGIC;
	dgo_tol.dist = 0.005;
	dgo_tol.dist_sq = dgo_tol.dist * dgo_tol.dist;
	dgo_tol.perp = 1e-6;
	dgo_tol.para = 1 - dgo_tol.perp;

	/* initialize tree state */
	dgo_initial_tree_state = rt_initial_tree_state;  /* struct copy */
	dgo_initial_tree_state.ts_ttol = &dgo_ttol;
	dgo_initial_tree_state.ts_tol = &dgo_tol;

	(void)Tcl_CreateCommand(interp, "dg_open", dgo_open_tcl,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
HIDDEN void
dgo_deleteProc(clientData)
     ClientData clientData;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

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
HIDDEN int
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
 * database object "wdb_obj".
 *
 * USAGE:
 *	  dgo_open [name wdb_obj]
 */
HIDDEN int
dgo_open_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop;
	struct wdb_obj *wdbop;
	struct bu_vls vls;

	if (argc == 1) {
		/* get list of drawable geometry objects */
		for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l))
			Tcl_AppendResult(interp, bu_vls_addr(&dgop->dgo_name), " ", (char *)NULL);

		return TCL_OK;
	}

	if (argc != 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dg_open");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* search for database object */
	for (BU_LIST_FOR(wdbop, wdb_obj, &HeadWDBObj.l)) {
		if (strcmp(bu_vls_addr(&wdbop->wdb_name), argv[2]) == 0)
			break;
	}

	if (BU_LIST_IS_HEAD(wdbop, &HeadWDBObj.l)) {
		Tcl_AppendResult(interp, "dgo_open: bad database object - ", argv[2],
				 "\n", (char *)NULL);
		return TCL_ERROR;
	}

	/* acquire dg_obj struct */
	BU_GETSTRUCT(dgop,dg_obj);

	/* initialize dg_obj */
	bu_vls_init(&dgop->dgo_name);
	bu_vls_strcpy(&dgop->dgo_name,argv[1]);
	dgop->dgo_wdbop = wdbop;
	BU_LIST_INIT(&dgop->dgo_headSolid.l);
	BU_LIST_INIT(&dgop->dgo_headVDraw);

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
 * Returns: database objects headSolid.
 */
HIDDEN int
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
 * Prepare database objects for drawing.
 *
 * Usage:
 *        procname draw|ev [args]
 *
 */
HIDDEN int
dgo_draw_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	struct wdb_obj *wdbop;
	register struct directory *dp;
	register int i;
	struct bu_vls vls;
	int ret;
	int kind;

	if (argc < 3) {
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

	curr_dgop = dgop;
	curr_interp = interp;
	curr_dbip = dgop->dgo_wdbop->wdb_wp->dbip;
	curr_hsp = &dgop->dgo_headSolid;

	argc -= 2;
	argv += 2;

	/*  First, delete any mention of these objects.
	 *  Silently skip any leading options (which start with minus signs).
	 */
	for (i = 0; i < argc; i++) {
		if ((dp = db_lookup(curr_dbip,  argv[i], LOOKUP_QUIET)) != DIR_NULL) {
			dgo_eraseobj(interp, dgop, dp);
		}
	}

	dgo_drawtrees(argc, argv, kind);
	dgo_color_soltab(&dgop->dgo_headSolid);

	return TCL_OK;
}

HIDDEN void
dgo_erase(dgop, interp, argc, argv)
     struct dg_obj *dgop;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	register struct directory *dp;
	register int i;

	for (i = 0; i < argc; i++) {
		if ((dp = db_lookup(dgop->dgo_wdbop->wdb_wp->dbip,  argv[i], LOOKUP_NOISY)) != DIR_NULL)
			dgo_eraseobj(interp, dgop, dp);
	}
}

/*
 * Erase database objects.
 *
 * Usage:
 *        procname erase object(s)
 *
 */
HIDDEN int
dgo_erase_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dgo_erase");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	dgo_erase(dgop, interp, argc-2, argv+2);

	return TCL_OK;
}

/*
 * List the objects currently being drawn.
 *
 * Usage:
 *        procname who [r(eal)|p(hony)|b(oth)]
 */
HIDDEN int
dgo_who_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;
	register struct solid *sp;
	int skip_real, skip_phony;

	if(argc < 2 || 3 < argc){
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
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l)
	  sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l)  {
	  register struct solid *forw;	/* XXX */

	  if( sp->s_iflag == UP )
	    continue;
	  if (sp->s_path[0]->d_addr == RT_DIR_PHONY_ADDR){
	    if (skip_phony) continue;
	  } else {
	    if (skip_real) continue;
	  }
	  Tcl_AppendResult(interp, sp->s_path[0]->d_namep, " ", (char *)NULL);
	  sp->s_iflag = UP;
	  FOR_REST_OF_SOLIDS(forw, sp, &dgop->dgo_headSolid.l){
	    if( forw->s_path[0] == sp->s_path[0] )
	      forw->s_iflag = UP;
	  }
	}
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l)
		sp->s_iflag = DOWN;

	return TCL_OK;
}

HIDDEN void
dgo_overlay(fp, name, char_size)
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

	dgo_cvt_vlblock_to_solids(vbp, name, 0);
	rt_vlblock_free(vbp);
}

/*
 * Usage:
 *        procname overlay file.plot char_size [name]
 */
HIDDEN int
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

	curr_dgop = dgop;
	curr_interp = interp;
	curr_dbip = dgop->dgo_wdbop->wdb_wp->dbip;
	curr_hsp = &dgop->dgo_headSolid;
	dgo_overlay(fp, name, char_size);

	return TCL_OK;
}

/*
 * Usage:
 *        procname getview
 */
HIDDEN int
dgo_getview_tcl(clientData, interp, argc, argv)
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

	VSETALL(min,  INFINITY);
	VSETALL(max, -INFINITY);

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l) {
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN(min, minus);
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX(max, plus);
	}

	if (BU_LIST_IS_EMPTY(&dgop->dgo_headSolid.l)) {
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
	bu_vls_printf(&vls, "center {%g %g %g} scale %g", V3ARGS(center), radial[X]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 * Usage:
 *        procname rt view_obj arg(s)
 */
HIDDEN int
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
	int retcode;
	char *dm;
	char	pstring[32];
	struct bu_vls cmd;

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
	*vp++ = dgop->dgo_wdbop->wdb_wp->dbip->dbi_filename;

	/*
	 * Now that we've grabbed all the options, if no args remain,
	 * have setup_rt() append the names of all stuff currently displayed.
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
	retcode = dgo_run_rt(dgop, vop);

	return TCL_OK;
}

/*
 * Usage:
 *        procname vdraw cmd arg(s)
 */
HIDDEN int
dgo_vdraw_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct dg_obj *dgop = (struct dg_obj *)clientData;

	curr_dgop = dgop;
	curr_interp = interp;
	curr_dbip = dgop->dgo_wdbop->wdb_wp->dbip;
	curr_hsp = &dgop->dgo_headSolid;

	return bu_cmd(clientData, interp, argc, argv, vdraw_cmds, 2);
}

/****************** utility routines ********************/

/*
 */
HIDDEN void
dgo_plot_anim_upcall_handler(file, us)
     char *file;
     long us;		/* microseconds of extra delay */
{
	char *av[3];

#if 0
	/* Overlay plot file */
	av[0] = "overlay";
	av[1] = file;
	av[2] = NULL;
	(void)dgo_overlay((ClientData)NULL, interp, 2, av);
#endif
}

/*
 */
HIDDEN void
dgo_vlblock_anim_upcall_handler(vbp, us, copy)
     struct bn_vlblock *vbp;
     long us; /* microseconds of extra delay */
     int copy;
{
	dgo_cvt_vlblock_to_solids(vbp, "_PLOT_OVERLAY_", copy);
}

/*
 */
HIDDEN void
dgo_nmg_debug_display_hack()
{
}

HIDDEN union tree *
dgo_wireframe_region_end(tsp, pathp, curtree)
     register struct db_tree_state	*tsp;
     struct db_full_path	*pathp;
     union tree		*curtree;
{
	return (curtree);
}

/*
 *			D G O _ W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
dgo_wireframe_leaf(tsp, pathp, ep, id)
     struct db_tree_state	*tsp;
     struct db_full_path	*pathp;
     struct bu_external	*ep;
     int			id;
{
	struct rt_db_internal	intern;
	union tree	*curtree;
	int		dashflag;		/* draw with dashed lines */
	struct bu_list	vhead;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);

	BU_LIST_INIT(&vhead);

	if (rt_g.debug&DEBUG_TREEWALK) {
		char	*sofar = db_path_to_string(pathp);

		Tcl_AppendResult(curr_interp, "dgo_wireframe_leaf(", rt_functab[id].ft_name,
				 ") path='", sofar, "'\n", (char *)NULL);
		bu_free((genptr_t)sofar, "path string");
	}

	if (dgo_draw_solid_lines_only)
		dashflag = 0;
	else
		dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));

	RT_INIT_DB_INTERNAL(&intern);
	if (rt_functab[id].ft_import(&intern, ep, tsp->ts_mat, curr_dbip) < 0) {
		Tcl_AppendResult(curr_interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
				 ":  solid import failure\n", (char *)NULL);

		if (intern.idb_ptr)
			rt_functab[id].ft_ifree( &intern );
		return (TREE_NULL);		/* ERROR */
	}
	RT_CK_DB_INTERNAL(&intern);

	if (rt_functab[id].ft_plot(&vhead,
				   &intern,
				   tsp->ts_ttol,
				   tsp->ts_tol) < 0) {
		Tcl_AppendResult(curr_interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
				 ": plot failure\n", (char *)NULL);
		rt_functab[id].ft_ifree(&intern);
		return (TREE_NULL);		/* ERROR */
	}

	/*
	 * XXX HACK CTJ - dgo_drawH_part2 sets the default color of a
	 * solid by looking in tps->ts_mater.ma_color, for pseudo
	 * solids, this needs to be something different and drawH
	 * has no idea or need to know what type of solid this is.
	 */
	if (intern.idb_type == ID_GRIP) {
		int r,g,b;
		r= tsp->ts_mater.ma_color[0];
		g= tsp->ts_mater.ma_color[1];
		b= tsp->ts_mater.ma_color[2];
		tsp->ts_mater.ma_color[0] = 0;
		tsp->ts_mater.ma_color[1] = 128;
		tsp->ts_mater.ma_color[2] = 128;
		dgo_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL);
		tsp->ts_mater.ma_color[0] = r;
		tsp->ts_mater.ma_color[1] = g;
		tsp->ts_mater.ma_color[2] = b;
	} else {
		dgo_drawH_part2(dashflag, &vhead, pathp, tsp, SOLID_NULL);
	}
	rt_functab[id].ft_ifree(&intern);

	/* Indicate success by returning something other than TREE_NULL */
	BU_GETUNION(curtree, tree);
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
dgo_nmg_region_start(tsp, pathp, combp)
struct db_tree_state	*tsp;
struct db_full_path	*pathp;
CONST struct rt_comb_internal *combp;
{
	union tree		*tp;
	struct directory	*dp;
	struct rt_db_internal	intern;
	mat_t			xform;
	matp_t			matp;
	struct bu_list		vhead;

	if (rt_g.debug&DEBUG_TREEWALK) {
		char	*sofar = db_path_to_string(pathp);
		bu_log("dgo_nmg_region_start(%s)\n", sofar);
		bu_free((genptr_t)sofar, "path string");
		rt_pr_tree( combp->tree, 1 );
		db_pr_tree_state(tsp);
	}

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
	if (rt_db_get_internal(&intern, dp, tsp->ts_dbip, matp) < 0)
		return 0;	/* proceed as usual */

	switch (intern.idb_type) {
	case ID_POLY:
		{
			struct rt_pg_internal	*pgp;
			register int	i;
			int		p;

			if (rt_g.debug&DEBUG_TREEWALK) {
				bu_log("fastpath draw ID_POLY\n", dp->d_namep);
			}
			pgp = (struct rt_pg_internal *)intern.idb_ptr;
			RT_PG_CK_MAGIC(pgp);

			if (dgo_draw_wireframes) {
				for (p = 0; p < pgp->npoly; p++) {
					register struct rt_pg_face_internal	*pp;

					pp = &pgp->poly[p];
					RT_ADD_VLIST( &vhead, &pp->verts[3*(pp->npts-1)],
						BN_VLIST_LINE_MOVE );
					for (i=0; i < pp->npts; i++) {
						RT_ADD_VLIST(&vhead, &pp->verts[3*i],
							      BN_VLIST_LINE_DRAW);
					}
				}
			} else {
				for (p = 0; p < pgp->npoly; p++) {
					register struct rt_pg_face_internal	*pp;
					vect_t aa, bb, norm;

					pp = &pgp->poly[p];
					if (pp->npts < 3)
						continue;
					VSUB2( aa, &pp->verts[3*(0)], &pp->verts[3*(1)] );
					VSUB2( bb, &pp->verts[3*(0)], &pp->verts[3*(2)] );
					VCROSS( norm, aa, bb );
					VUNITIZE(norm);
					RT_ADD_VLIST(&vhead, norm,
						     BN_VLIST_POLY_START);

					RT_ADD_VLIST(&vhead, &pp->verts[3*(pp->npts-1)],
						     BN_VLIST_POLY_MOVE);
					for (i=0; i < pp->npts-1; i++) {
						RT_ADD_VLIST(&vhead, &pp->verts[3*i],
							     BN_VLIST_POLY_DRAW);
					}
					RT_ADD_VLIST(&vhead, &pp->verts[3*(pp->npts-1)],
						     BN_VLIST_POLY_END);
				}
			}
		}
		goto out;
	case ID_COMBINATION:
	default:
		break;
	}
	rt_db_free_internal(&intern);
	return 0;

out:
	/* Successful fastpath drawing of this solid */
	db_add_node_to_full_path(pathp, dp);
	dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL);
	DB_FULL_PATH_POP(pathp);
	rt_db_free_internal(&intern);
	dgo_fastpath_count++;
	return -1;	/* SKIP THIS REGION */
}

/*
 *			D G O _ N M G _ R E G I O N _ E N D
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
dgo_nmg_region_end( tsp, pathp, curtree )
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
	struct nmgregion	*r;
	struct bu_list		vhead;
	int			failed;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	NMG_CK_MODEL(*tsp->ts_m);

	BU_LIST_INIT( &vhead );

	if(rt_g.debug&DEBUG_TREEWALK)  {
	  char	*sofar = db_path_to_string(pathp);

	  Tcl_AppendResult(curr_interp, "dgo_nmg_region_end() path='", sofar,
			   "'\n", (char *)NULL);
	  bu_free((genptr_t)sofar, "path string");
	}

	if( curtree->tr_op == OP_NOP )  return  curtree;

	if ( !dgo_draw_nmg_only ) {
		if( BU_SETJUMP )
		{
			char  *sofar = db_path_to_string(pathp);

			BU_UNSETJUMP;

			Tcl_AppendResult(curr_interp, "WARNING: Boolean evaluation of ", sofar,
				" failed!!!\n", (char *)NULL );
			bu_free((genptr_t)sofar, "path string");
			if( curtree )
				db_free_tree( curtree );
			return (union tree *)NULL;
		}
		failed = nmg_boolean( curtree, *tsp->ts_m, tsp->ts_tol );
		BU_UNSETJUMP;
		if( failed )  {
			db_free_tree( curtree );
			return (union tree *)NULL;
		}
	}
	else if( curtree->tr_op != OP_NMG_TESS )
	{
	  Tcl_AppendResult(curr_interp, "Cannot use '-d' option when Boolean evaluation is required\n", (char *)NULL);
	  db_free_tree( curtree );
	  return (union tree *)NULL;
	}
	r = curtree->tr_d.td_r;
	NMG_CK_REGION(r);

	if( dgo_do_not_draw_nmg_solids_during_debugging && r )  {
		db_free_tree( curtree );
		return (union tree *)NULL;
	}

	if (dgo_nmg_triangulate) {
		if( BU_SETJUMP )
		{
			char  *sofar = db_path_to_string(pathp);

			BU_UNSETJUMP;

			Tcl_AppendResult(curr_interp, "WARNING: Triangulation of ", sofar,
				" failed!!!\n", (char *)NULL );
			bu_free((genptr_t)sofar, "path string");
			if( curtree )
				db_free_tree( curtree );
			return (union tree *)NULL;
		}
		nmg_triangulate_model(*tsp->ts_m, tsp->ts_tol);
		BU_UNSETJUMP;
	}

	if( r != 0 )  {
		int	style;
		/* Convert NMG to vlist */
		NMG_CK_REGION(r);

		if( dgo_draw_wireframes )  {
			/* Draw in vector form */
			style = NMG_VLIST_STYLE_VECTOR;
		} else {
			/* Default -- draw polygons */
			style = NMG_VLIST_STYLE_POLYGON;
		}
		if( dgo_draw_normals )  {
			style |= NMG_VLIST_STYLE_VISUALIZE_NORMALS;
		}
		if( dgo_shade_per_vertex_normals )  {
			style |= NMG_VLIST_STYLE_USE_VU_NORMALS;
		}
		if( dgo_draw_no_surfaces )  {
			style |= NMG_VLIST_STYLE_NO_SURFACES;
		}
		nmg_r_to_vlist( &vhead, r, style );

		dgo_drawH_part2(0, &vhead, pathp, tsp, SOLID_NULL );

		if( dgo_draw_edge_uses )  {
			nmg_vlblock_r(dgo_draw_edge_uses_vbp, r, 1);
		}
		/* NMG region is no longer necessary, only vlist remains */
		db_free_tree( curtree );
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
HIDDEN int
dgo_drawtrees(argc, argv, kind)
     int	argc;
     char	**argv;
     int	kind;
{
	int		ret = 0;
	register int	c;
	int		ncpu;
	int		dgo_nmg_use_tnurbs = 0;

	if (curr_dbip == DBI_NULL)
		return 0;

	RT_CHECK_DBI(curr_dbip);

	if (argc <= 0)
		return(-1);	/* FAIL */

	/* Initial values for options, must be reset each time */
	ncpu = 1;
	dgo_draw_nmg_only = 0;	/* no booleans */
	dgo_nmg_triangulate = 1;
	dgo_draw_wireframes = 0;
	dgo_draw_normals = 0;
	dgo_draw_edge_uses = 0;
	dgo_draw_solid_lines_only = 0;
	dgo_shade_per_vertex_normals = 0;
	dgo_draw_no_surfaces = 0;
	dgo_wireframe_color_override = 0;
	dgo_fastpath_count = 0;
	dgo_enable_fastpath = 0;

	/* Parse options. */
	bu_optind = 0;		/* re-init bu_getopt() */
	while ((c = bu_getopt(argc,argv,"dfnqstuvwSTP:C:")) != EOF) {
		switch (c) {
		case 'u':
			dgo_draw_edge_uses = 1;
			break;
		case 's':
			dgo_draw_solid_lines_only = 1;
			break;
		case 't':
			dgo_nmg_use_tnurbs = 1;
			break;
		case 'v':
			dgo_shade_per_vertex_normals = 1;
			break;
		case 'w':
			dgo_draw_wireframes = 1;
			break;
		case 'S':
			dgo_draw_no_surfaces = 1;
			break;
		case 'T':
			dgo_nmg_triangulate = 0;
			break;
		case 'n':
			dgo_draw_normals = 1;
			break;
		case 'P':
			ncpu = atoi(bu_optarg);
			break;
		case 'q':
			dgo_do_not_draw_nmg_solids_during_debugging = 1;
			break;
		case 'd':
			dgo_draw_nmg_only = 1;
			break;
		case 'f':
			dgo_enable_fastpath = 1;
			break;
		case 'C':
			{
				char		buf[128];
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

				dgo_wireframe_color_override = 1;
				dgo_wireframe_color[0] = r;
				dgo_wireframe_color[1] = g;
				dgo_wireframe_color[2] = b;
			}
			break;
		default:
			{
				struct bu_vls vls;

				bu_vls_init(&vls);
				bu_vls_printf(&vls, "help %s", argv[0]);
				Tcl_Eval(curr_interp, bu_vls_addr(&vls));
				bu_vls_free(&vls);

				return TCL_ERROR;
			}
		}
	}
	argc -= bu_optind;
	argv += bu_optind;

	/* Establish upcall interfaces for use by bottom of NMG library */
	nmg_plot_anim_upcall = dgo_plot_anim_upcall_handler;
	nmg_vlblock_anim_upcall = dgo_vlblock_anim_upcall_handler;
	nmg_mged_debug_display_hack = dgo_nmg_debug_display_hack;

	switch (kind) {
	default:
	  Tcl_AppendResult(curr_interp, "ERROR, bad kind\n", (char *)NULL);
	  return(-1);
	case 1:		/* Wireframes */
		ret = db_walk_tree(curr_dbip, argc, (CONST char **)argv,
			ncpu,
			&dgo_initial_tree_state,
			0,			/* take all regions */
			dgo_wireframe_region_end,
			dgo_wireframe_leaf);
		break;
	case 2:		/* Big-E */
		Tcl_AppendResult(curr_interp, "drawtrees:  can't do big-E here\n", (char *)NULL);
		return (-1);
	case 3:
		{
		/* NMG */
	  	dgo_nmg_model = nmg_mm();
		dgo_initial_tree_state.ts_m = &dgo_nmg_model;
	  	if (dgo_draw_edge_uses) {
		  Tcl_AppendResult(curr_interp, "Doing the edgeuse thang (-u)\n", (char *)NULL);
		  dgo_draw_edge_uses_vbp = rt_vlblock_init();
	  	}

		ret = db_walk_tree(curr_dbip, argc, (CONST char **)argv,
			ncpu,
			&dgo_initial_tree_state,
			dgo_enable_fastpath ? dgo_nmg_region_start : 0,
			dgo_nmg_region_end,
	  		dgo_nmg_use_tnurbs ?
	  			nmg_booltree_leaf_tnurb :
				nmg_booltree_leaf_tess
			);

	  	if (dgo_draw_edge_uses) {
	  		dgo_cvt_vlblock_to_solids(dgo_draw_edge_uses_vbp, "_EDGEUSES_", 0);
	  		rt_vlblock_free(dgo_draw_edge_uses_vbp);
			dgo_draw_edge_uses_vbp = (struct bn_vlblock *)NULL;
 	  	}

		/* Destroy NMG */
		nmg_km(dgo_nmg_model);
	  	break;
	  }
	}
	if (dgo_fastpath_count) {
		bu_log("%d region%s rendered through polygon fastpath\n",
		       dgo_fastpath_count, dgo_fastpath_count==1?"":"s");
	}

	if (ret < 0)
		return (-1);

	return (0);	/* OK */
}


/*
 *			C V T _ V L B L O C K _ T O _ S O L I D S
 */
HIDDEN void
dgo_cvt_vlblock_to_solids(vbp, name, copy)
struct bn_vlblock	*vbp;
char			*name;
int			copy;
{
	int		i;
	char		shortname[32];
	char		namebuf[64];
	char		*av[2];

	strncpy(shortname, name, 16-6);
	shortname[16-6] = '\0';
	/* Remove any residue colors from a previous overlay w/same name */
	av[0] = shortname;
	av[1] = NULL;
	dgo_erase(curr_dgop, curr_interp, 1, av);

	for( i=0; i < vbp->nused; i++ )  {
		if (BU_LIST_IS_EMPTY(&(vbp->head[i])))
			continue;

		sprintf(namebuf, "%s%lx",
			shortname, vbp->rgb[i]);
		dgo_invent_solid(namebuf, &vbp->head[i], vbp->rgb[i], copy);
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
HIDDEN int
dgo_invent_solid(name, vhead, rgb, copy)
     char		*name;
     struct bu_list	*vhead;
     long		rgb;
     int		copy;
{
	register struct directory	*dp;
	register struct solid		*sp;
	register struct dm_list *dmlp;
	register struct dm_list *save_dmlp;

	if (curr_dbip == DBI_NULL)
		return 0;

	if ((dp = db_lookup(curr_dbip, name, LOOKUP_QUIET)) != DIR_NULL) {
		if (dp->d_addr != RT_DIR_PHONY_ADDR) {
			Tcl_AppendResult(curr_interp, "dgo_invent_solid(", name,
					 ") would clobber existing database entry, ignored\n", (char *)NULL);
			return (-1);
		}

		/*
		 * Name exists from some other overlay,
		 * zap any associated solids
		 */
		dgo_eraseobjall(curr_interp, curr_dgop, dp);
	}
	/* Need to enter phony name in directory structure */
	dp = db_diradd(curr_dbip,  name, RT_DIR_PHONY_ADDR, 0, DIR_SOLID);

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
	dgo_bound_solid(sp);

	/* set path information -- this is a top level node */
	sp->s_last = 0;
	sp->s_path[0] = dp;

	sp->s_iflag = DOWN;
	sp->s_soldash = 0;
	sp->s_Eflag = 1;		/* Can't be solid edited! */
	sp->s_color[0] = sp->s_basecolor[0] = (rgb>>16) & 0xFF;
	sp->s_color[1] = sp->s_basecolor[1] = (rgb>> 8) & 0xFF;
	sp->s_color[2] = sp->s_basecolor[2] = (rgb    ) & 0xFF;
	sp->s_regionid = 0;
	sp->s_dlist = BU_LIST_LAST(solid, &curr_hsp->l)->s_dlist + 1;

	/* Solid successfully drawn, add to linked list of solid structs */
	BU_LIST_APPEND(curr_hsp->l.back, &sp->l);

	return (0);		/* OK */
}

/*
 *  Compute the min, max, and center points of the solid.
 *  Also finds s_vlen;
 * XXX Should split out a separate bn_vlist_rpp() routine, for librt/vlist.c
 */
HIDDEN void
dgo_bound_solid(sp)
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
					Tcl_AppendResult(curr_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
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
HIDDEN void
dgo_drawH_part2(dashflag, vhead, pathp, tsp, existing_sp)
int			dashflag;
struct bu_list		*vhead;
struct db_full_path	*pathp;
struct db_tree_state	*tsp;
struct solid		*existing_sp;
{
	register struct solid *sp;
	register int	i;
	register struct dm_list *dmlp;
	register struct dm_list *save_dmlp;

	if (!existing_sp) {
		if (pathp->fp_len > MAX_PATH) {
		  char *cp = db_path_to_string(pathp);

		  Tcl_AppendResult(curr_interp, "drawH_part2: path too long, solid ignored.\n\t",
				   cp, "\n", (char *)NULL);
		  bu_free((genptr_t)cp, "Path string");
		  return;
		}
		/* Handling a new solid */
		GET_SOLID(sp, &FreeSolid.l);
		/* NOTICE:  The structure is dirty & not initialized for you! */

		sp->s_dlist = BU_LIST_LAST(solid, &curr_hsp->l)->s_dlist + 1;
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
	dgo_bound_solid(sp);

	/*
	 *  If this solid is new, fill in it's information.
	 *  Otherwise, don't touch what is already there.
	 */
	if (!existing_sp) {
		/* Take note of the base color */
		if (dgo_wireframe_color_override) {
		        /* a user specified the color, so arrange to use it */
			sp->s_uflag = 1;
			sp->s_dflag = 0;
			sp->s_basecolor[0] = dgo_wireframe_color[0];
			sp->s_basecolor[1] = dgo_wireframe_color[1];
			sp->s_basecolor[2] = dgo_wireframe_color[2];
		} else {
			sp->s_uflag = 0;
			if (tsp) {
			  if (tsp->ts_mater.ma_color_valid) {
			    sp->s_dflag = 0;	/* color specified in db */
			  } else {
			    sp->s_dflag = 1;	/* default color */
			  }
			  /* Copy into basecolor anyway, to prevent black */
			  sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
			  sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
			  sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
			}
		}
		sp->s_cflag = 0;
		sp->s_iflag = DOWN;
		sp->s_soldash = dashflag;
		sp->s_Eflag = 0;	/* This is a solid */
		sp->s_last = pathp->fp_len-1;

		/* Copy path information */
		for (i=0; i<=sp->s_last; i++) {
			sp->s_path[i] = pathp->fp_names[i];
		}
		sp->s_regionid = tsp->ts_regionid;
	}

	/* Solid is successfully drawn */
	if (!existing_sp) {
		/* Add to linked list of solid structs */
		bu_semaphore_acquire(RT_SEM_MODEL);
		BU_LIST_APPEND(curr_hsp->l.back, &sp->l);
		bu_semaphore_release(RT_SEM_MODEL);
	} else {
		/* replacing existing solid -- struct already linked in */
		sp->s_iflag = UP;
	}
}

/*
 *			E R A S E O B J A L L
 *
 * This routine goes through the solid table and deletes all displays
 * which contain the specified object in their 'path'
 */
void
dgo_eraseobjall(interp, dgop, dp)
     Tcl_Interp *interp;
     struct dg_obj *dgop;
     register struct directory *dp;
{
  register struct solid *sp;
  static struct solid *nsp;
  register int i;

  if(dgop->dgo_wdbop->wdb_wp->dbip == DBI_NULL)
    return;

  RT_CK_DIR(dp);
  sp = BU_LIST_NEXT(solid, &dgop->dgo_headSolid.l);
  while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid.l)) {
	  nsp = BU_LIST_PNEXT(solid, sp);
	  for (i=0; i<=sp->s_last; i++) {
		  if (sp->s_path[i] != dp)
			  continue;

		  BU_LIST_DEQUEUE(&sp->l);
		  FREE_SOLID(sp, &FreeSolid.l);

		  break;
	  }
	  sp = nsp;
  }

  if (dp->d_addr == RT_DIR_PHONY_ADDR) {
    if (db_dirdelete(dgop->dgo_wdbop->wdb_wp->dbip, dp) < 0) {
	    Tcl_AppendResult(interp, "dgo_eraseobjall: db_dirdelete failed\n", (char *)NULL);
    }
  }
}

HIDDEN void
dgo_eraseobj(interp, dgop, dp)
     Tcl_Interp *interp;
     struct dg_obj *dgop;
     register struct directory *dp;
{
	register struct solid *sp;
	register struct solid *nsp;

	if(dgop->dgo_wdbop->wdb_wp->dbip == DBI_NULL)
		return;

	RT_CK_DIR(dp);

	sp = BU_LIST_FIRST(solid, &dgop->dgo_headSolid.l);
	while (BU_LIST_NOT_HEAD(sp, &dgop->dgo_headSolid.l)) {
		nsp = BU_LIST_PNEXT(solid, sp);
		if (*sp->s_path != dp) {
			sp = nsp;
			continue;
		}

		BU_LIST_DEQUEUE(&sp->l);
		FREE_SOLID(sp, &FreeSolid.l);
		sp = nsp;
	}

	if (dp->d_addr == RT_DIR_PHONY_ADDR ) {
		if( db_dirdelete(dgop->dgo_wdbop->wdb_wp->dbip, dp) < 0 ){
			Tcl_AppendResult(interp, "dgo_eraseobj: db_dirdelete failed\n", (char *)NULL);
		}
	}
}

/*
 *  			C O L O R _ S O L T A B
 *
 *  Pass through the solid table and set pointer to appropriate
 *  mater structure.
 */
HIDDEN void
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
int
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
	 *  Mark ones already done with s_iflag == UP
	 */
	FOR_ALL_SOLIDS(sp, &hsp->l)
		sp->s_iflag = DOWN;
	FOR_ALL_SOLIDS(sp, &hsp->l)  {
		register struct solid *forw;

		if (sp->s_iflag == UP)
			continue;
		if (sp->s_path[0]->d_addr == RT_DIR_PHONY_ADDR)
			continue;	/* Ignore overlays, predictor, etc */
		if (vp < end)
			*vp++ = sp->s_path[0]->d_namep;
		else  {
		  Tcl_AppendResult(interp, "mged: ran out of comand vector space at ",
				   sp->s_path[0]->d_namep, "\n", (char *)NULL);
		  break;
		}
		sp->s_iflag = UP;
		for (BU_LIST_PFOR(forw, sp, solid, &hsp->l)) {
			if (forw->s_path[0] == sp->s_path[0])
				forw->s_iflag = UP;
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
HIDDEN void
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

#define DIR_USED	0x80	/* XXX move to raytrace.h */
	(void)fprintf(fp, "start 0; clean;\n");
	FOR_ALL_SOLIDS (sp, &dgop->dgo_headSolid.l) {
		for (i=0;i<=sp->s_last;i++) {
			sp->s_path[i]->d_flags &= ~DIR_USED;
		}
	}
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l) {
		for (i=0; i<=sp->s_last; i++ ) {
			if (!(sp->s_path[i]->d_flags & DIR_USED)) {
				register struct animate *anp;
				for (anp = sp->s_path[i]->d_animate; anp;
				    anp=anp->an_forw) {
					db_write_anim(fp, anp);
				}
				sp->s_path[i]->d_flags |= DIR_USED;
			}
		}
	}

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l) {
		for (i=0;i<=sp->s_last;i++) {
			sp->s_path[i]->d_flags &= ~DIR_USED;
		}
	}
#undef DIR_USED
	(void)fprintf(fp, "end;\n");
}

HIDDEN void
dgo_rt_output_handler(clientData, mask)
     ClientData clientData;
     int mask;
{
	int fd = (int)clientData;
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

HIDDEN void
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
	vect_t  unit_H, unit_V;

	VSET(eye_model, -vop->vo_center[MDX],
	     -vop->vo_center[MDY], -vop->vo_center[MDZ]);

	for (i = 0; i < 3; ++i) {
		extremum[0][i] = INFINITY;
		extremum[1][i] = -INFINITY;
	}

	FOR_ALL_SOLIDS (sp, &dgop->dgo_headSolid.l) {
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
HIDDEN int
dgo_run_rt(dgop, vop)
     struct dg_obj *dgop;
     struct view_obj *vop; 
{
	register struct solid *sp;
	register int i;
	int pid, rpid;
	int retcode;
	FILE *fp_in;
	int pipe_in[2];
	int pipe_err[2];
	char line[RT_MAXLINE];
	struct bu_vls vls;
	vect_t eye_model;

	(void)pipe(pipe_in);
	(void)pipe(pipe_err);

	if ((pid = fork()) == 0) {
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

#if 0
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid.l)
		sp->s_iflag = DOWN;
#endif

	Tcl_CreateFileHandler(pipe_err[0], TCL_READABLE,
			      dgo_rt_output_handler, (ClientData)pipe_err[0]);

	return 0;
}
