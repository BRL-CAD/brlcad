/*
 *				D M _ O B J . C
 *
 * A display manager object contains the attributes and
 * methods for controlling display managers.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
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
#if 1
#define USE_FBSERV
#endif

#include "conf.h"
#include <math.h>
#include "tk.h"
#include <X11/Xutil.h>

#include "machine.h"
#include "externs.h"
#include "cmd.h"                  /* includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#ifdef USE_FBSERV
#include "dm-X.h"
#ifdef DM_OGL
#include <GL/glx.h>
#include <GL/gl.h>
#include "dm-ogl.h"
#endif
#include "dm_xvars.h"
#endif

/* These functions live in libfb. */
extern int _X24_open_existing();
extern int X24_close_existing();
extern int X24_refresh();
extern int _ogl_open_existing();
extern int ogl_close_existing();
extern int ogl_refresh();

static int dmo_open_tcl();
static int dmo_close_tcl();
static int dmo_drawBegin_tcl();
static int dmo_drawEnd_tcl();
static int dmo_clear_tcl();
static int dmo_normal_tcl();
static int dmo_loadmat_tcl();
static int dmo_drawString_tcl();
static int dmo_drawPoint_tcl();
static int dmo_drawLine_tcl();
static int dmo_drawVList_tcl();
static int dmo_drawSList_tcl();
static int dmo_drawGeom_tcl();
static int dmo_fg_tcl();
static int dmo_bg_tcl();
static int dmo_lineWidth_tcl();
static int dmo_lineStyle_tcl();
static int dmo_configure_tcl();
static int dmo_zclip_tcl();
static int dmo_zbuffer_tcl();
static int dmo_light_tcl();
static int dmo_bounds_tcl();
static int dmo_perspective_tcl();
static int dmo_debug_tcl();
#ifdef USE_FBSERV
static int dmo_openFb_tcl();
static int dmo_closeFb();
static int dmo_closeFb_tcl();
static int dmo_listen_tcl();
static int dmo_refreshFb_tcl();
#endif
static int dmo_flush_tcl();
static int dmo_sync_tcl();
static int dmo_size_tcl();
static int dmo_aspect_tcl();

static struct dm_obj HeadDMObj;	/* head of display manager object list */

static struct bu_cmdtab dmo_cmds[] = {
	"drawBegin",		dmo_drawBegin_tcl,
	"drawEnd",		dmo_drawEnd_tcl,
	"clear",		dmo_clear_tcl,
	"normal",		dmo_normal_tcl,
	"loadmat",		dmo_loadmat_tcl,
	"drawString",		dmo_drawString_tcl,
	"drawPoint",		dmo_drawPoint_tcl,
	"drawLine",		dmo_drawLine_tcl,
	"drawVList",		dmo_drawVList_tcl,
	"drawSList",		dmo_drawSList_tcl,
	"drawGeom",		dmo_drawGeom_tcl,
	"fg",			dmo_fg_tcl,
	"bg",			dmo_bg_tcl,
	"linewidth",		dmo_lineWidth_tcl,
	"linestyle",		dmo_lineStyle_tcl,
	"configure",		dmo_configure_tcl,
	"light",		dmo_light_tcl,
	"zbuffer",		dmo_zbuffer_tcl,
	"zclip",		dmo_zclip_tcl,
	"bounds",		dmo_bounds_tcl,
	"perspective",		dmo_perspective_tcl,
	"debug",		dmo_debug_tcl,
#ifdef USE_FBSERV
	"openfb",		dmo_openFb_tcl,
	"closefb",		dmo_closeFb_tcl,
	"listen",		dmo_listen_tcl,
	"refreshfb",		dmo_refreshFb_tcl,
#endif
	"flush",		dmo_flush_tcl,
	"sync",			dmo_sync_tcl,
	"close",		dmo_close_tcl,
	"size",			dmo_size_tcl,
	"aspect",		dmo_aspect_tcl,
	(char *)0,		(int (*)())0
};

/*
 *			D M _ C M D
 *
 * Generic interface for display manager routines.
 * Usage:
 *        procname cmd ?args?
 *
 * Returns: result of DM command.
 */
static int
dmo_cmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  return bu_cmd(clientData, interp, argc, argv, dmo_cmds, 1);
}

int
Dmo_Init(interp)
Tcl_Interp *interp;
{
  BU_LIST_INIT(&HeadDMObj.l);
  (void)Tcl_CreateCommand(interp, "dm_open", dmo_open_tcl,
			  (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

  return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
static void
dmo_deleteProc(clientData)
ClientData clientData;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  dmo_closeFb(dmop);
  bu_vls_free(&dmop->dmo_name);
  DM_CLOSE(dmop->dmo_dmp);
  BU_LIST_DEQUEUE(&dmop->l);
  bu_free((genptr_t)dmop, "dmo_deleteProc: dmop");

}

/*
 * Close a display manager object.
 *
 * Usage:
 *	  procname close
 */
static int
dmo_close_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
	struct bu_vls vls;
	struct dm_obj *dmop = (struct dm_obj *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dmo_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call dmo_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&dmop->dmo_name));

	return TCL_OK;
}

/*
 * Open/create a display manager object.
 *
 * Usage:
 *	  dm_open [name type [args]]
 */
static int
dmo_open_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop;
  struct dm *dmp;
  struct bu_vls vls;
  int name_index = 1;
  int type;

  if (argc == 1) {
    /* get list of display manager objects */
    for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l))
      Tcl_AppendResult(interp, bu_vls_addr(&dmop->dmo_name), " ", (char *)NULL);

    return TCL_OK;
  }

  if (argc < 3) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib dm_open");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* check to see if display manager object exists */
  for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l)) {
    if (strcmp(argv[name_index],bu_vls_addr(&dmop->dmo_name)) == 0) {
      Tcl_AppendResult(interp, "dmo_open: ", argv[name_index],
		       " exists.", (char *)NULL);
      return TCL_ERROR;
    }
  }

  /* find display manager type */
  if (argv[2][0] == 'X' || argv[2][0] == 'x')
    type = DM_TYPE_X;
#ifdef DM_OGL
  else if (!strcmp(argv[2], "ogl"))
    type = DM_TYPE_OGL;
#endif
  else if (!strcmp(argv[2], "ps"))
    type = DM_TYPE_PS;
  else if (!strcmp(argv[2], "plot"))
    type = DM_TYPE_PLOT;
  else if (!strcmp(argv[2], "nu"))
    type = DM_TYPE_NULL;
  else {
    Tcl_AppendResult(interp,
		     "Unsupported display manager type - ",
		     argv[2], "\n",
		     "The supported types are: X, ogl, ps, plot and nu", (char *)NULL);
    return TCL_ERROR;
  }

  {
    int i;
    int arg_start = 3;
    int newargs = 2;
    int ac;
    char **av;

    ac = argc + newargs;
    av = (char **)bu_malloc(sizeof(char *) * (ac+1), "dmo_open_tcl: av");
    av[0] = argv[0];

    /* Insert new args (i.e. arrange to call init_dm_obj from dm_open()) */
    av[1] = "-i";
    av[2] = "init_dm_obj";

    /*
     * Stuff name into argument list.
     */
    av[3] = "-n";
    av[4] = argv[name_index];

    /* copy the rest */
    for (i = arg_start; i < argc; ++i)
      av[i+newargs] = argv[i];
    av[i+newargs] = (char *)NULL;

    if ((dmp = dm_open(type, ac, av)) == DM_NULL) {
      Tcl_AppendResult(interp,
		       "dmo_open_tcl: Failed to open - ",
		       argv[name_index],
		       "\n",
		       (char *)NULL);
      bu_free((genptr_t)av, "dmo_open_tcl: av");
      
      return TCL_ERROR;
    }

    bu_free((genptr_t)av, "dmo_open_tcl: av");
  }

  /* acquire dm_obj struct */
  BU_GETSTRUCT(dmop,dm_obj);

  /* initialize dm_obj */
  bu_vls_init(&dmop->dmo_name);
  bu_vls_strcpy(&dmop->dmo_name,argv[name_index]);
  dmop->dmo_dmp = dmp;
  VSETALL(dmop->dmo_dmp->dm_clipmin, -2048.0);
  VSETALL(dmop->dmo_dmp->dm_clipmax, 2047.0);
  dmop->dmo_fbs.fbs_listener.fbsl_fbsp = &dmop->dmo_fbs;
  dmop->dmo_fbs.fbs_listener.fbsl_fd = -1;
  dmop->dmo_fbs.fbs_listener.fbsl_port = -1;
  dmop->dmo_fbs.fbs_fbp = FBIO_NULL;

  /* append to list of dm_obj's */
  BU_LIST_APPEND(&HeadDMObj.l,&dmop->l);

  (void)Tcl_CreateCommand(interp,
			  bu_vls_addr(&dmop->dmo_name),
			  dmo_cmd,
			  (ClientData)dmop,
			  dmo_deleteProc);

  /* send Configure event */
  bu_vls_init(&vls);
  bu_vls_printf(&vls, "event generate %s <Configure>", bu_vls_addr(&dmop->dmo_name));
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);


  /* Return new function name as result */
  Tcl_ResetResult(interp);
  Tcl_AppendResult(interp, bu_vls_addr(&dmop->dmo_name), (char *)NULL);
  return TCL_OK;
}

static int
dmo_drawBegin_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return DM_DRAW_BEGIN(dmop->dmo_dmp);
}

static int
dmo_drawEnd_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return DM_DRAW_END(dmop->dmo_dmp);
}

static int
dmo_clear_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  int status;

  if ((status = DM_DRAW_BEGIN(dmop->dmo_dmp)) != TCL_OK)
    return status;

  return DM_DRAW_END(dmop->dmo_dmp);
}

static int
dmo_normal_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return DM_NORMAL(dmop->dmo_dmp);
}

static int
dmo_loadmat_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  mat_t mat;
  int which_eye;

  if (argc != 4) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib loadmat");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }


  if (bn_decode_mat(mat, argv[2]) != 16)
    return TCL_ERROR;

  if (sscanf(argv[3], "%d", &which_eye) != 1) {
    Tcl_AppendResult(interp, "bad eye value - ", argv[3], (char *)NULL);
    return TCL_ERROR;
  }

  return DM_LOADMATRIX(dmop->dmo_dmp,mat,which_eye);
}

static int
dmo_drawString_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  fastf_t x, y;
  int size;
  int use_aspect;

  if (argc != 7) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib drawString");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /*XXX use sscanf */
  x = atof(argv[3]);
  y = atof(argv[4]);
  size = atoi(argv[5]);
  use_aspect = atoi(argv[6]);

  return DM_DRAW_STRING_2D(dmop->dmo_dmp,argv[2],x,y,size,use_aspect);
}

static int
dmo_drawPoint_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  fastf_t x, y;

  if (argc != 4) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib drawPoint");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /*XXX use sscanf */
  x = atof(argv[2]);
  y = atof(argv[3]);

  return DM_DRAW_POINT_2D(dmop->dmo_dmp,x,y);
}

static int
dmo_drawLine_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  fastf_t x1, y1, x2, y2;

  if (argc != 6) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib drawLine");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /*XXX use sscanf */
  x1 = atof(argv[2]);
  y1 = atof(argv[3]);
  x2 = atof(argv[4]);
  y2 = atof(argv[5]);

  return DM_DRAW_LINE_2D(dmop->dmo_dmp,x1,y1,x2,y2);
}

/*
 * Usage:
 *	  procname drawVList vid
 */
static int
dmo_drawVList_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct rt_vlist *vp;

  if (argc != 3) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib drawVList");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (sscanf(argv[2], "%lu", (unsigned long *)&vp) != 1) {
    Tcl_AppendResult(interp, "invalid vlist pointer - ", argv[2], (char *)NULL);
    return TCL_ERROR;
  }

  /* XXX this causes a core dump if vp is bogus */
  BN_CK_VLIST_TCL(interp,vp);

  return DM_DRAW_VLIST(dmop->dmo_dmp, vp);
}

/*
 * Usage:
 *	  procname drawSList hsp
 */
static int
dmo_drawSList(dmop, hsp)
     struct dm_obj *dmop;
     struct solid *hsp;
{
	struct solid *sp;
	int linestyle = -1;

	FOR_ALL_SOLIDS(sp, &hsp->l) {
		if (linestyle != sp->s_soldash) {
			linestyle = sp->s_soldash;
			DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
		}

		DM_SET_FGCOLOR(dmop->dmo_dmp,
			       (short)sp->s_color[0],
			       (short)sp->s_color[1],
			       (short)sp->s_color[2], 0);
		DM_DRAW_VLIST(dmop->dmo_dmp, (struct rt_vlist *)&sp->s_vlist);
	}

	return TCL_OK;
}

/*
 * Usage:
 *	  procname drawSList sid
 */
static int
dmo_drawSList_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct solid *hsp;

	if (argc != 3) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib drawSList");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (sscanf(argv[2], "%lu", (unsigned long *)&hsp) != 1) {
		Tcl_AppendResult(interp, "invalid solid list pointer - ",
				 argv[2], "\n", (char *)NULL);
		return TCL_ERROR;
	}
	dmo_drawSList(dmop, hsp);

	return TCL_OK;
}

/*
 * Draw "drawable geometry" objects.
 *
 * Usage:
 *	  procname drawGeom dg_obj(s)
 */
static int
dmo_drawGeom_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct dg_obj *dgop;
	struct bu_vls vls;
	register int i;

	if (argc < 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dm_drawGeom");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	argc -= 2;
	argv += 2;
	for (i = 0; i < argc; ++i) {
		for (BU_LIST_FOR(dgop, dg_obj, &HeadDGObj.l)) {
			if (strcmp(bu_vls_addr(&dgop->dgo_name), argv[i]) == 0) {
				dmo_drawSList(dmop, &dgop->dgo_headSolid);
				break;
			}
		}
	}

	return TCL_OK;
}

/*
 * Get/set the display manager's foreground color.
 *
 * Usage:
 *	  procname fg [rgb]
 */
static int
dmo_fg_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int r, g, b;

  bu_vls_init(&vls);

  /* get foreground color */
  if (argc == 2) {
    bu_vls_printf(&vls, "%d %d %d",
		  dmop->dmo_dmp->dm_fg[0],
		  dmop->dmo_dmp->dm_fg[1],
		  dmop->dmo_dmp->dm_fg[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set foreground color */
  if (argc == 3) {
    if (sscanf(argv[2], "%d %d %d", &r, &g, &b) != 3)
      goto bad_color;

    /* validate color */
    if (r < 0 || 255 < r ||
	g < 0 || 255 < g ||
	b < 0 || 255 < b)
      goto bad_color;

    bu_vls_free(&vls);
    return DM_SET_FGCOLOR(dmop->dmo_dmp,r,g,b,1);
  }

  /* wrong number of arguments */
  bu_vls_printf(&vls, "helplib dm_fg");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;

bad_color:
  bu_vls_printf(&vls, "bad rgb color - %s\n", argv[2]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Get/set the display manager's background color.
 *
 * Usage:
 *	  procname bg [rgb]
 */
static int
dmo_bg_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int r, g, b;

  bu_vls_init(&vls);

  /* get background color */
  if (argc == 2) {
    bu_vls_printf(&vls, "%d %d %d",
		  dmop->dmo_dmp->dm_bg[0],
		  dmop->dmo_dmp->dm_bg[1],
		  dmop->dmo_dmp->dm_bg[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set background color */
  if (argc == 3) {
    if (sscanf(argv[2], "%d %d %d", &r, &g, &b) != 3)
      goto bad_color;

    /* validate color */
    if (r < 0 || 255 < r ||
	g < 0 || 255 < g ||
	b < 0 || 255 < b)
      goto bad_color;

    bu_vls_free(&vls);
    return DM_SET_BGCOLOR(dmop->dmo_dmp,r,g,b);
  }

  /* wrong number of arguments */
  bu_vls_printf(&vls, "helplib dm_bg");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;

bad_color:
  bu_vls_printf(&vls, "bad rgb color - %s\n", argv[2]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Get/set the display manager's linewidth.
 *
 * Usage:
 *	  procname linewidth [n]
 */
static int
dmo_lineWidth_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int linewidth;

  bu_vls_init(&vls);

  /* get linewidth */
  if (argc == 2) {
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_lineWidth);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set linewidth */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &linewidth) != 1)
      goto bad_linewidth;

    /* validate linewidth */
    if (linewidth < 0 || 20 < linewidth)
      goto bad_linewidth;

    bu_vls_free(&vls);
    return DM_SET_LINE_ATTR(dmop->dmo_dmp, linewidth, dmop->dmo_dmp->dm_lineStyle);
  }

  /* wrong number of arguments */
  bu_vls_printf(&vls, "helplib dm_linewidth");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;

bad_linewidth:
  bu_vls_printf(&vls, "bad linewidth - %s\n", argv[2]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Get/set the display manager's linestyle.
 *
 * Usage:
 *	  procname linestyle [0|1]
 */
static int
dmo_lineStyle_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int linestyle;

  bu_vls_init(&vls);

  /* get linestyle */
  if (argc == 2) {
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_lineStyle);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set linestyle */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &linestyle) != 1)
      goto bad_linestyle;

    /* validate linestyle */
    if (linestyle < 0 || 1 < linestyle)
      goto bad_linestyle;

    bu_vls_free(&vls);
    return DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
  }

  /* wrong number of arguments */
  bu_vls_printf(&vls, "helplib dm_linestyle");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;

bad_linestyle:
  bu_vls_printf(&vls, "bad linestyle - %s\n", argv[2]);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Configure the display manager window. This is typically
 * called as a result of a ConfigureNotify event.
 *
 * Usage:
 *	  procname configure
 */
static int
dmo_configure_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  if (argc != 2) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib dm_configure");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  return DM_CONFIGURE_WIN(dmop->dmo_dmp);
}

/*
 * Get/set the display manager's zclip flag.
 *
 * Usage:
 *	  procname zclip [0|1]
 */
static int
dmo_zclip_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int zclip;

  /* get zclip flag */
  if (argc == 2) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_zclip);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set zclip flag */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &zclip) != 1) {
      Tcl_AppendResult(interp, "dmo_zclip: invalid zclip value - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    dmop->dmo_dmp->dm_zclip = zclip;
    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "helplib dm_zclip");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

/*
 * Get/set the display manager's zbuffer flag.
 *
 * Usage:
 *	  procname zbuffer [0|1]
 */
static int
dmo_zbuffer_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int zbuffer;

  /* get zbuffer flag */
  if (argc == 2) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_zbuffer);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set zbuffer flag */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &zbuffer) != 1) {
      Tcl_AppendResult(interp, "dmo_zbuffer: invalid zbuffer value - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    DM_SET_ZBUFFER(dmop->dmo_dmp, zbuffer);
    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "helplib dm_zbuffer");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

/*
 * Get/set the display manager's light flag.
 *
 * Usage:
 *	  procname light [0|1]
 */
static int
dmo_light_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int light;

  /* get light flag */
  if (argc == 2) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_light);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set light flag */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &light) != 1) {
      Tcl_AppendResult(interp, "dmo_light: invalid light value - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    DM_SET_LIGHT(dmop->dmo_dmp, light);
    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "helplib dm_light");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

/*
 * Get/set the display manager's window bounds.
 *
 * Usage:
 *	  procname bounds ["xmin xmax ymin ymax zmin zmax"]
 */
static int
dmo_bounds_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  vect_t clipmin, clipmax;

  /* get window bounds */
  if (argc == 2) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%g %g %g %g %g %g",
		  dmop->dmo_dmp->dm_clipmin[X],
		  dmop->dmo_dmp->dm_clipmax[X],
		  dmop->dmo_dmp->dm_clipmin[Y],
		  dmop->dmo_dmp->dm_clipmax[Y],
		  dmop->dmo_dmp->dm_clipmin[Z],
		  dmop->dmo_dmp->dm_clipmax[Z]);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set window bounds */
  if (argc == 3) {
    if (sscanf(argv[2], "%lf %lf %lf %lf %lf %lf",
	       &clipmin[X], &clipmax[X],
	       &clipmin[Y], &clipmax[Y],
	       &clipmin[Z], &clipmax[Z]) != 6) {
      Tcl_AppendResult(interp, "dmo_bounds: invalid bounds - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    VMOVE(dmop->dmo_dmp->dm_clipmin, clipmin);
    VMOVE(dmop->dmo_dmp->dm_clipmax, clipmax);
    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "helplib dm_bounds");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

/*
 * Get/set the display manager's perspective mode.
 *
 * Usage:
 *	  procname perspective [n]
 */
static int
dmo_perspective_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int perspective;

  /* get perspective mode */
  if (argc == 2) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_perspective);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set perspective mode */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &perspective) != 1) {
      Tcl_AppendResult(interp, "dmo_perspective: invalid perspective mode - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    dmop->dmo_dmp->dm_perspective = perspective;
    return TCL_OK;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "helplib dm_perspective");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

/*
 * Get/set the display manager's debug level.
 *
 * Usage:
 *	  procname debug [n]
 */
static int
dmo_debug_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;
  int level;

  /* get debug level */
  if (argc == 2) {
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d", dmop->dmo_dmp->dm_debugLevel);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set debug level */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &level) != 1) {
      Tcl_AppendResult(interp, "dmo_debug: invalid debug level - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    return DM_DEBUG(dmop->dmo_dmp,level);
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "helplib dm_debug");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

#ifdef USE_FBSERV
/*
 * Open/activate the display managers framebuffer.
 *
 * Usage:
 *	  procname openfb
 *
 */
static int
dmo_openFb_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  char *X_name = "/dev/X";
#ifdef DM_OGL
  char *ogl_name = "/dev/ogl";
#endif

  /* already open */
  if (dmop->dmo_fbs.fbs_fbp != FBIO_NULL)
	  return TCL_OK;

  if((dmop->dmo_fbs.fbs_fbp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL){
    Tcl_AppendResult(interp, "openfb: failed to allocate framebuffer memory\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  switch (dmop->dmo_dmp->dm_type) {
  case DM_TYPE_X:
    *dmop->dmo_fbs.fbs_fbp = X24_interface; /* struct copy */

    dmop->dmo_fbs.fbs_fbp->if_name = malloc((unsigned)strlen(X_name) + 1);
    (void)strcpy(dmop->dmo_fbs.fbs_fbp->if_name, X_name);

    /* Mark OK by filling in magic number */
    dmop->dmo_fbs.fbs_fbp->if_magic = FB_MAGIC;

    _X24_open_existing(dmop->dmo_fbs.fbs_fbp,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy,
		       ((struct x_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->pix,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->cmap,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->vip,
		       dmop->dmo_dmp->dm_width,
		       dmop->dmo_dmp->dm_height,
		       ((struct x_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->gc);
    break;
#ifdef DM_OGL
  case DM_TYPE_OGL:
    *dmop->dmo_fbs.fbs_fbp = ogl_interface; /* struct copy */

    dmop->dmo_fbs.fbs_fbp->if_name = malloc((unsigned)strlen(ogl_name) + 1);
    (void)strcpy(dmop->dmo_fbs.fbs_fbp->if_name, ogl_name);

    /* Mark OK by filling in magic number */
    dmop->dmo_fbs.fbs_fbp->if_magic = FB_MAGIC;

    _ogl_open_existing(dmop->dmo_fbs.fbs_fbp,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->win,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->cmap,
		       ((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->vip,
		       dmop->dmo_dmp->dm_width,
		       dmop->dmo_dmp->dm_height,
		       ((struct ogl_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->glxc,
		       ((struct ogl_vars *)dmop->dmo_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
		       0);
    break;
#endif
  }

  return TCL_OK;
}

static int
dmo_closeFb(dmop)
     struct dm_obj *dmop;
{
	if (dmop->dmo_fbs.fbs_fbp == FBIO_NULL)
		return TCL_OK;

	_fb_pgflush(dmop->dmo_fbs.fbs_fbp);

	switch (dmop->dmo_dmp->dm_type) {
	case DM_TYPE_X:
		X24_close_existing(dmop->dmo_fbs.fbs_fbp);
		break;
#ifdef DM_OGL
	case DM_TYPE_OGL:
		ogl_close_existing(dmop->dmo_fbs.fbs_fbp);
		break;
#endif
	}

	/* free framebuffer memory */
	if (dmop->dmo_fbs.fbs_fbp->if_pbase != PIXEL_NULL)
		free((void *)dmop->dmo_fbs.fbs_fbp->if_pbase);
	free((void *)dmop->dmo_fbs.fbs_fbp->if_name);
	free((void *)dmop->dmo_fbs.fbs_fbp);
	dmop->dmo_fbs.fbs_fbp = FBIO_NULL;

	return TCL_OK;
}

/*
 * Close/de-activate the display managers framebuffer.
 *
 * Usage:
 *	  procname closefb
 *
 */
static int
dmo_closeFb_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return dmo_closeFb(dmop);
}

/*
 * Listen for framebuffer clients.
 *
 * Usage:
 *	  procname listen port
 *
 * Returns the port number actually used.
 *
 */
static int
dmo_listen_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (dmop->dmo_fbs.fbs_fbp == FBIO_NULL) {
		bu_vls_printf(&vls, "%s listen: framebuffer not open!\n", argv[0]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	/* return the port number */
	if (argc == 2) {
		bu_vls_printf(&vls, "%d", dmop->dmo_fbs.fbs_listener.fbsl_port);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

  if (argc == 3) {
    int port;

    if (sscanf(argv[2], "%d", &port) != 1) {
      Tcl_AppendResult(interp, "listen: bad value - ", argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    if (port >= 0)
      fbs_open(interp, &dmop->dmo_fbs, port);
    else {
      fbs_close(interp, &dmop->dmo_fbs);
    }
    bu_vls_printf(&vls, "%d", dmop->dmo_fbs.fbs_listener.fbsl_port);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
  }

  bu_vls_printf(&vls, "helplib listen");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

  return TCL_ERROR;
}

/*
 * Refresh the display managers framebuffer.
 *
 * Usage:
 *	  procname refresh
 *
 */
static int
dmo_refreshFb_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct bu_vls vls;

	if (dmop->dmo_fbs.fbs_fbp == FBIO_NULL) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "%s refresh: framebuffer not open!\n", argv[0]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	fb_refresh(dmop->dmo_fbs.fbs_fbp, 0, 0, dmop->dmo_dmp->dm_width, dmop->dmo_dmp->dm_height);
  
	return TCL_OK;
}
#endif

/*
 * Flush the output buffer.
 *
 * Usage:
 *	  procname flush
 *
 */
static int
dmo_flush_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  XFlush(((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy);
  
  return TCL_OK;
}

/*
 * Flush the output buffer and process all events.
 *
 * Usage:
 *	  procname sync
 *
 */
static int
dmo_sync_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  XSync(((struct dm_xvars *)dmop->dmo_dmp->dm_vars.pub_vars)->dpy, 0);
  
  return TCL_OK;
}

/*
 * Get window size (i.e. width and height)
 *
 * Usage:
 *	  procname size
 *
 */
static int
dmo_size_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dm_size");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%d %d", dmop->dmo_dmp->dm_width, dmop->dmo_dmp->dm_height);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

/*
 * Get window aspect ratio (i.e. width / height)
 *
 * Usage:
 *	  procname aspect
 *
 */
static int
dmo_aspect_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct dm_obj *dmop = (struct dm_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dm_aspect");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%g", dmop->dmo_dmp->dm_aspect);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}
