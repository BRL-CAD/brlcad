/*
 *			G E O M E T R Y _ O B J . C
 *
 * A geometry object contains the attributes and methods
 * for manipulating BRLCAD geometry.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 */
#include "conf.h"
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "solid.h"
#include "cmd.h"

#include "../librt/debug.h"	/* XXX */

extern Tcl_Interp *interp;   /* This must be defined by the application */

struct geometry_obj {
  struct bu_list	l;
  struct bu_vls		geo_name;	/* geometry object name/cmd */
  struct bu_vls		geo_dbname;	/* geometry objects name in database */
  struct db_i		*geo_dbip;	/* database instance pointer */
  struct solid		geo_solid;	/* head of solid list */
};

HIDDEN union tree *geo_wireframe_region_end();
HIDDEN union tree *geo_wireframe_leaf();
HIDDEN void geo_bound_solid();
HIDDEN void geo_drawH_part2();
HIDDEN int geo_cmd();
HIDDEN int geo_slist_tcl();
HIDDEN int geo_vlist_tcl();
HIDDEN int geo_clist_tcl();
HIDDEN int geo_solid_tcl();

HIDDEN struct db_tree_state	geo_initial_tree_state;
HIDDEN struct rt_tess_tol	geo_ttol;
HIDDEN struct bn_tol		geo_tol;

HIDDEN struct geometry_obj HeadGeoObj;	/* head of geometry object list */
HIDDEN struct db_i *curr_dbip;		/* current database instance pointer */
HIDDEN struct solid *curr_hsp;		/* current head solid pointer */

HIDDEN struct cmdtab geo_cmds[] = 
{
	"slist",		geo_slist_tcl,
	"vlist",		geo_vlist_tcl,
	"clist",		geo_clist_tcl,
	"solid",		geo_solid_tcl,
	(char *)0,		(int (*)())0
};

HIDDEN int
geo_cmd(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
  return do_cmd(clientData, interp, argc, argv, geo_cmds, 1);
}

int
Geo_Init(interp)
Tcl_Interp *interp;
{
  BU_LIST_INIT(&HeadGeoObj.l);

  /* initilize tolerance structures */
  geo_ttol.magic = RT_TESS_TOL_MAGIC;
  geo_ttol.abs = 0.0;		/* disabled */
  geo_ttol.rel = 0.01;
  geo_ttol.norm = 0.0;		/* disabled */

  geo_tol.magic = BN_TOL_MAGIC;
  geo_tol.dist = 0.005;
  geo_tol.dist_sq = geo_tol.dist * geo_tol.dist;
  geo_tol.perp = 1e-6;
  geo_tol.para = 1 - geo_tol.perp;

  /* initialize tree state */
  geo_initial_tree_state = rt_initial_tree_state;  /* struct copy */
  geo_initial_tree_state.ts_ttol = &geo_ttol;
  geo_initial_tree_state.ts_tol = &geo_tol;

  return TCL_OK;
}

void
geo_deleteProc(clientData)
ClientData clientData;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  register struct solid *sp;
  register struct solid *nsp;
  struct directory	*dp;

  bu_vls_free(&gop->geo_name);
  bu_vls_free(&gop->geo_dbname);

  /* free solids */
  sp = BU_LIST_NEXT(solid, &gop->geo_solid.l);
  while (BU_LIST_NOT_HEAD(sp, &gop->geo_solid.l)) {
    dp = sp->s_path[0];
#if 1
    RT_CK_DIR(dp);
#else
    /* XXX not ready yet */
    RT_CK_DIR_TCL(dp);
#endif
    if (dp->d_addr == RT_DIR_PHONY_ADDR) {
      if (db_dirdelete(gop->geo_dbip, dp) < 0)
	Tcl_AppendResult(interp,
			 "geo_deleteProc: db_dirdelete failed\n",
			 (char *)NULL);
    }

    nsp = BU_LIST_PNEXT(solid, sp);
    BU_LIST_DEQUEUE(&sp->l);
    /* free solid */
    bu_free((genptr_t)sp, "geo_deleteProc: sp");
    sp = nsp;
  }

  BU_LIST_DEQUEUE(&gop->l);
  bu_free((genptr_t)gop, "geo_deleteProc: gop");
}

/*
 * Open/create a geometry object.
 *
 * USAGE: geo_open [name dbip dbname]
 */
int
geo_open_tcl(clientData, interp, argc, argv)
ClientData      clientData;
Tcl_Interp      *interp;
int             argc;
char            **argv;
{
  int ret;
  struct bu_vls vls;
  struct db_i *dbip;
  struct geometry_obj *gop;

  if (argc == 1) {
    /* get list of geometry objects */
    for (BU_LIST_FOR(gop, geometry_obj, &HeadGeoObj.l))
      Tcl_AppendResult(interp, bu_vls_addr(&gop->geo_name), " ", (char *)NULL);

    return TCL_OK;
  }

  if (argc != 4) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib geo_open");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* check to see if geometry object exists */
  for (BU_LIST_FOR(gop, geometry_obj, &HeadGeoObj.l)) {
    if (strcmp(argv[1],bu_vls_addr(&gop->geo_name)) == 0) {
      Tcl_AppendResult(interp, "geo_open: ", argv[1],
		       " exists.\n", (char *)NULL);
      return TCL_ERROR;
    }
  }

  /* get database instance pointer */
  if (sscanf(argv[2], "%lu", (unsigned long *)&dbip) != 1) {
    Tcl_AppendResult(interp, "geo_open: bad dbip - ", argv[2],
		     "\n", (char *)NULL);
    bu_free((genptr_t)gop, "geo_open: gop");
    return TCL_ERROR;
  }

  /* XXX this causes a core dump if dbip is bogus */
  RT_CHECK_DBI_TCL(interp,dbip);

  /* acquire geometry_obj struct */
  BU_GETSTRUCT(gop,geometry_obj);

  /* initialize geometry_obj */
  gop->geo_dbip = dbip;
  bu_vls_init(&gop->geo_name);
  bu_vls_strcpy(&gop->geo_name,argv[1]);
  bu_vls_init(&gop->geo_dbname);
  bu_vls_strcpy(&gop->geo_dbname,argv[3]);

  /* append to list of geometry_obj's */
  BU_LIST_APPEND(&HeadGeoObj.l,&gop->l);

  /* get solids */
  BU_LIST_INIT(&gop->geo_solid.l);
  curr_hsp = &gop->geo_solid;
  curr_dbip = gop->geo_dbip;
#if 1
  argc -= 3;
  argv += 3;
  /* Wireframes */
  ret = db_walk_tree(gop->geo_dbip, argc, (CONST char **)argv,
		     1,			/* # of cpu's */
		     &geo_initial_tree_state,
		     0,			/* take all regions */
		     geo_wireframe_region_end,
		     geo_wireframe_leaf );

  if (ret < 0) {
    /* XXX free resources */

    Tcl_AppendResult(interp, "geo_open: db_walk_tree failed\n", (char *)NULL);
    return TCL_ERROR;
  }
#endif

  (void)Tcl_CreateCommand(interp,
			  bu_vls_addr(&gop->geo_name),
			  geo_cmd,
			  (ClientData)gop,
			  geo_deleteProc);

  /* Return new function name as result */
  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, bu_vls_addr(&gop->geo_name), (char *)NULL);

  return TCL_OK;
}

HIDDEN union tree *
geo_wireframe_region_end(tsp, pathp, curtree)
register struct db_tree_state	*tsp;
struct db_full_path	*pathp;
union tree		*curtree;
{
  return (curtree);
}

/*
 *			W I R E F R A M E _ L E A F
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN union tree *
geo_wireframe_leaf(tsp, pathp, ep, id)
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

    Tcl_AppendResult(interp, "geo_wireframe_leaf(", rt_functab[id].ft_name,
		     ") path='", sofar, "'\n", (char *)NULL);
    bu_free((genptr_t)sofar, "path string");
  }

  /* solid lines */
  dashflag = 0;
#if 0
  dashflag = (tsp->ts_sofar & (TS_SOFAR_MINUS|TS_SOFAR_INTER));
#endif

  RT_INIT_DB_INTERNAL(&intern);
  if (rt_functab[id].ft_import(&intern, ep, tsp->ts_mat, curr_dbip) < 0) {
    Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
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
    Tcl_AppendResult(interp, DB_FULL_PATH_CUR_DIR(pathp)->d_namep,
		     ": plot failure\n", (char *)NULL);
    rt_functab[id].ft_ifree(&intern);
    return (TREE_NULL);		/* ERROR */
  }

  /*
   * XXX HACK CTJ - geo_drawH_part2 sets the default color of a
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
    geo_drawH_part2(dashflag, &vhead, pathp, tsp);
    tsp->ts_mater.ma_color[0] = r;
    tsp->ts_mater.ma_color[1] = g;
    tsp->ts_mater.ma_color[2] = b;
  } else {
    geo_drawH_part2(dashflag, &vhead, pathp, tsp);
  }
  rt_functab[id].ft_ifree(&intern);

  /* Indicate success by returning something other than TREE_NULL */
  BU_GETUNION(curtree, tree);
  curtree->magic = RT_TREE_MAGIC;
  curtree->tr_op = OP_NOP;

  return (curtree);
}

/*
 *  Compute the min, max, and center points of the solid.
 *  Also finds s_vlen;
 * XXX Should split out a separate bn_vlist_rpp() routine, for librt/vlist.c
 */
HIDDEN void
geo_bound_solid(sp)
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
 *			G E O _ D R A W h _ P A R T 2
 *
 *  Once the vlist has been created, perform the common tasks
 *  in handling the drawn solid.
 *
 *  This routine must be prepared to run in parallel.
 */
HIDDEN void
geo_drawH_part2(dashflag, vhead, pathp, tsp)
int			dashflag;
struct bu_list		*vhead;
struct db_full_path	*pathp;
struct db_tree_state	*tsp;
{
  register struct solid *sp;
  register int	i;
  register struct dm_list *dmlp;
  register struct dm_list *save_dmlp;

  if (pathp->fp_len > MAX_PATH) {
    char *cp = db_path_to_string(pathp);

    Tcl_AppendResult(interp, "geo_drawH_part2: path too long, solid ignored.\n\t",
		     cp, "\n", (char *)NULL);
    bu_free((genptr_t)cp, "Path string");
    return;
  }

  BU_GETSTRUCT(sp,solid);
  BU_LIST_INIT(&sp->s_vlist);

  /*
   * Compute the min, max, and center points.
   */
  BU_LIST_APPEND_LIST(&(sp->s_vlist), vhead);
  geo_bound_solid(sp);

  sp->s_basecolor[0] = tsp->ts_mater.ma_color[0] * 255.;
  sp->s_basecolor[1] = tsp->ts_mater.ma_color[1] * 255.;
  sp->s_basecolor[2] = tsp->ts_mater.ma_color[2] * 255.;
  sp->s_color[0] = sp->s_basecolor[0];
  sp->s_color[1] = sp->s_basecolor[1];
  sp->s_color[2] = sp->s_basecolor[2];

  sp->s_cflag = 0;
  sp->s_iflag = DOWN;
  sp->s_soldash = dashflag;
  sp->s_Eflag = 0; /* This is a solid */
  sp->s_last = pathp->fp_len-1;

  /* Copy path information */
  for( i=0; i<=sp->s_last; i++ ) {
    sp->s_path[i] = pathp->fp_names[i];
  }

  sp->s_regionid = tsp->ts_regionid;

  /* Solid is successfully drawn */
  /* Add to linked list of solid structs */
  bu_semaphore_acquire( RT_SEM_MODEL );
  BU_LIST_APPEND(curr_hsp->l.back, &sp->l);
  bu_semaphore_release( RT_SEM_MODEL );
}

/*
 * Returns the list of solids for the geometry object.
 *
 * Usage:
 *        procname slist
 */
HIDDEN int
geo_slist_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  /* return list of solid pointers */
  if (argc == 2) {
    for (BU_LIST_FOR(sp, solid, &gop->geo_solid.l))
      bu_vls_printf(&vls, "%lu ", (unsigned long)sp);

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* compose error message */
  bu_vls_printf(&vls, "helplib geo_slist");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Returns the list of vlists for the geometry object.
 *
 * Usage:
 *        procname vlist
 */
HIDDEN int
geo_vlist_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  /* return list of vlist pointers */
  if (argc == 2) {
    for (BU_LIST_FOR(sp, solid, &gop->geo_solid.l))
      bu_vls_printf(&vls, "%lu ", (unsigned long)&sp->s_vlist);

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* compose error message */
  bu_vls_printf(&vls, "helplib geo_vlist");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Returns the color for each solid in the geometry object.
 *
 * Usage:
 *        procname clist
 */
HIDDEN int
geo_clist_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  /* return list of colors */
  if (argc == 2) {
    for (BU_LIST_FOR(sp, solid, &gop->geo_solid.l))
      bu_vls_printf(&vls, "{%d %d %d} ",
		    sp->s_color[0],
		    sp->s_color[1],
		    sp->s_color[2]);

    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* compose error message */
  bu_vls_printf(&vls, "helplib geo_clist");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/********************* Solid Commands *********************/
HIDDEN int solid_size_tcl();
HIDDEN int solid_center_tcl();
HIDDEN int solid_vlist_tcl();
HIDDEN int solid_vlen_tcl();
HIDDEN int solid_linestyle_tcl();
HIDDEN int solid_path_tcl();
HIDDEN int solid_basecolor_tcl();
HIDDEN int solid_color_tcl();
HIDDEN int solid_rid_tcl();

HIDDEN struct cmdtab solid_cmds[] = 
{
	"size",			solid_size_tcl,
	"center",		solid_center_tcl,
	"vlist",		solid_vlist_tcl,
	"vlen",			solid_vlen_tcl,
	"linestyle",		solid_linestyle_tcl,
	"path",			solid_path_tcl,
	"basecolor",		solid_basecolor_tcl,
	"color",		solid_color_tcl,
	"rid",			solid_rid_tcl,
	(char *)0,		(int (*)())0
};

/*
 * Usage:
 *	  procname solid cmd [args]
 */
HIDDEN int
geo_solid_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  return do_cmd(clientData, interp, argc, argv, solid_cmds, 2);
}

HIDDEN int
solid_valid(gop, sp, sid)
struct geometry_obj	*gop;
struct solid		**sp;
char			*sid;
{
  struct bu_vls vls;
  struct solid *tmp_sp;

  /* get solid pointer from sid */
  if (sscanf(sid, "%lu", sp) != 1) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "bad solid id - %s\n", sid);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return 0;	/* not valid */
  }

  /* validate the solid id for this geometry object */
  for (BU_LIST_FOR(tmp_sp, solid, &gop->geo_solid.l)) {
    if (tmp_sp == *sp)
      return 1;	/* valid */
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "solid id not valid - %s\n", sid);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return 0;	/* not valid */
}

/*
 * Returns the distance across the solid, in model space.
 *
 * Usage:
 *	  procname solid size solid_id
 */
HIDDEN int
solid_size_tcl(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_size");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%g", sp->s_size);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  
  return TCL_OK;
}

/*
 * Returns the center point of the solid, in model space.
 *
 * Usage:
 *	  procname solid center solid_id
 */
HIDDEN int
solid_center_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_center");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bn_encode_vect(&vls, sp->s_center);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  
  return TCL_OK;
}

/*
 * Returns the solid's vlist pointer.
 *
 * Usage:
 *	  procname solid vlist solid_id
 */
HIDDEN int
solid_vlist_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_vlist");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%lu", (unsigned long)&sp->s_vlist);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  
  return TCL_OK;
}

/*
 * Returns the solid's # of actual cmd[] entries in vlist.
 *
 * Usage:
 *	  procname solid vlist solid_id
 */
HIDDEN int
solid_vlen_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_vlen");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%d", sp->s_vlen);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  
  return TCL_OK;
}

/*
 * Get/set the solids linestyle.
 *
 * Usage:
 *	  procname solid linestyle solid_id [0|1]
 */
HIDDEN int
solid_linestyle_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;
  int linestyle;

  bu_vls_init(&vls);

  if (argc < 4 || 5 < argc) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_vlen");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* get linestyle */
  if (argc == 4) {
    bu_vls_printf(&vls, "%d", (int)sp->s_soldash);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* Set the solids linestyle */
  /* first, get linestyle from argv */
  if (sscanf(argv[4], "%d", &linestyle) != 1)
    goto bad_linestyle;

  /* validate linestyle */
  if (linestyle < 0 || 1 < linestyle)
    goto bad_linestyle;

  /* set linestyle */
  sp->s_soldash = linestyle;
  return TCL_OK;

bad_linestyle:
  bu_vls_printf(&vls, "bad linestyle - %s\n", argv[4]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Returns the solid's full pathname.
 *
 * Usage:
 *	  procname solid path solid_id
 */
HIDDEN int
solid_path_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;
  int i;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_path");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* build pathname string */
  for(i=0; i <= sp->s_last; i++) {
    bu_vls_strcat(&vls, "/");
    bu_vls_strcat(&vls, sp->s_path[i]->d_namep);
  }

  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);
  return TCL_OK;
}

/*
 * Returns the solid's color from its containing region.
 *
 * Usage:
 *	  procname solid basecolor solid_id
 */
HIDDEN int
solid_basecolor_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_basecolor");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%d %d %d",
		(int)sp->s_basecolor[0],
		(int)sp->s_basecolor[1],
		(int)sp->s_basecolor[2]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

/*
 * Get/set the solid's drawing color.
 *
 * Usage:
 *	  procname solid color solid_id [rgb]
 */
HIDDEN int
solid_color_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;
  int r, g, b;

  bu_vls_init(&vls);

  if (argc < 4 || 5 < argc) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_color");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* get color */
  if (argc == 4) {
    bu_vls_printf(&vls, "%d %d %d",
		  (int)sp->s_color[0],
		  (int)sp->s_color[1],
		  (int)sp->s_color[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* Set the solids color */
  /* first, get color from argv */
  if (sscanf(argv[4], "%d %d %d", &r, &g, &b) != 3)
    goto bad_color;

  /* validate color */
  if (r < 0 || 255 < r ||
      g < 0 || 255 < g ||
      b < 0 || 255 < b)
    goto bad_color;

  /* set color */
  sp->s_color[0] = r;
  sp->s_color[1] = g;
  sp->s_color[2] = b;
  return TCL_OK;

bad_color:
  bu_vls_printf(&vls, "bad rgb color - %s\n", argv[4]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Returns the solid's region id.
 *
 * Usage:
 *	  procname solid rid solid_id
 */
HIDDEN int
solid_rid_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct geometry_obj *gop = (struct geometry_obj *)clientData;
  struct bu_vls vls;
  struct solid *sp;

  bu_vls_init(&vls);

  if (argc != 4) {
    /* compose error message */
    bu_vls_printf(&vls, "helplib solid_rid");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if (!solid_valid(gop, &sp, argv[3])) {
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* put region id into the result */
  bu_vls_printf(&vls, "%d", sp->s_regionid);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}
