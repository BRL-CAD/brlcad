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
 */
#include "conf.h"
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "dm.h"
#include "solid.h"
#include "cmd.h"

struct dm_obj {
  struct bu_list	l;
  struct bu_vls		dmo_name;		/* display manager name/cmd */
  struct dm		*dmo_dmp;		/* display manager pointer */
  int	 		dmo_perspective;	/* !0 means using perspective */
#ifdef USE_FBSERV
  struct fbserv_obj	*dmo_fbsp;		/* fbserv pointer */
  int			dmo_fbactive;		/* !0 means framebuffer is active */
#endif
};

HIDDEN int dmo_drawBegin_tcl();
HIDDEN int dmo_drawEnd_tcl();
HIDDEN int dmo_normal_tcl();
HIDDEN int dmo_loadmat_tcl();
HIDDEN int dmo_drawString_tcl();
HIDDEN int dmo_drawPoint_tcl();
HIDDEN int dmo_drawLine_tcl();
HIDDEN int dmo_drawVList_tcl();
HIDDEN int dmo_drawSList_tcl();
HIDDEN int dmo_fg_tcl();
HIDDEN int dmo_bg_tcl();
HIDDEN int dmo_lineWidth_tcl();
HIDDEN int dmo_lineStyle_tcl();
HIDDEN int dmo_configure_tcl();
HIDDEN int dmo_zclip_tcl();
HIDDEN int dmo_bounds_tcl();
HIDDEN int dmo_perspective_tcl();
HIDDEN int dmo_debug_tcl();
#ifdef USE_FBSERV
HIDDEN int dmo_openFb_tcl();
HIDDEN int dmo_closeFb_tcl();
#endif

HIDDEN struct dm_obj HeadDMObj;	/* head of display manager object list */

HIDDEN struct cmdtab dmo_cmds[] = {
	"drawBegin",		dmo_drawBegin_tcl,
	"drawEnd",		dmo_drawEnd_tcl,
	"normal",		dmo_normal_tcl,
	"loadmat",		dmo_loadmat_tcl,
	"drawString",		dmo_drawString_tcl,
	"drawPoint",		dmo_drawPoint_tcl,
	"drawLine",		dmo_drawLine_tcl,
	"drawVList",		dmo_drawVList_tcl,
	"drawSList",		dmo_drawSList_tcl,
	"fg",			dmo_fg_tcl,
	"bg",			dmo_bg_tcl,
	"linewidth",		dmo_lineWidth_tcl,
	"linestyle",		dmo_lineStyle_tcl,
	"configure",		dmo_configure_tcl,
	"zclip",		dmo_zclip_tcl,
	"bounds",		dmo_bounds_tcl,
	"perspective",		dmo_perspective_tcl,
	"debug",		dmo_debug_tcl,
#ifdef USE_FBSERV
	"openfb",		dmo_openFb_tcl,
	"closefb",		dmo_closeFb_tcl,
#endif
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
HIDDEN int
dmo_cmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  return do_cmd(clientData, interp, argc, argv, dmo_cmds, 1);
}

int
Dmo_Init(interp)
Tcl_Interp *interp;
{
  BU_LIST_INIT(&HeadDMObj.l);

  return TCL_OK;
}

/*
 * Called by Tcl when the object is destroyed.
 */
HIDDEN void
dmo_deleteProc(clientData)
ClientData clientData;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  bu_vls_free(&dmop->dmo_name);
  DM_CLOSE(dmop->dmo_dmp);
  BU_LIST_DEQUEUE(&dmop->l);
  bu_free((genptr_t)dmop, "dmo_deleteProc: dmop");
}

/*
 * Open/create a display manager object.
 *
 * Usage:
 *	  dm_open [type name [args]]
 */
HIDDEN int
dmo_open_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop;
  struct dm *dmp;
  struct bu_vls vls;
  int type;
  char *save_argv1;

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

  /* find display manager type */
  if (argv[1][0] == 'X' || argv[1][0] == 'x')
    type = DM_TYPE_X;
#ifdef DM_OGL
  else if (!strcmp(argv[1], "ogl"))
    type = DM_TYPE_OGL;
#endif
  else if (!strcmp(argv[1], "ps"))
    type = DM_TYPE_PS;
  else if (!strcmp(argv[1], "plot"))
    type = DM_TYPE_PLOT;
  else if (!strcmp(argv[1], "nu"))
    type = DM_TYPE_NULL;
  else {
    Tcl_AppendResult(interp,
		     "Unsupported display manager type - ",
		     argv[1], "\n",
		     "The supported types are: X, ogl, ps, plot and nu\n", (char *)NULL);
    return TCL_ERROR;
  }

  /* check to see if display manager object exists */
  for (BU_LIST_FOR(dmop, dm_obj, &HeadDMObj.l)) {
    if (strcmp(argv[2],bu_vls_addr(&dmop->dmo_name)) == 0) {
      Tcl_AppendResult(interp, "dmo_open: ", argv[2],
		       " exists.\n", (char *)NULL);
      return TCL_ERROR;
    }
  }

  {
    int i;
    int newargs = 2;
    int ac;
    char **av;

    ac = argc + newargs;
    av = (char **)bu_malloc(sizeof(char *) * (ac+1), "dmo_open_tcl: av");
    av[0] = argv[0];

    /* arrange to call init_dm_obj from dm_open() */
    av[1] = "-i";
    av[2] = "init_dm_obj";

    /*
     * Already have type, so reuse pointer to indicate that the next argument
     * points to the name.
     */
    av[3] = "-n";

    /* copy the rest */
    for (i = 2; i < argc; ++i)
      av[i+newargs] = argv[i];
    av[i+newargs] = (char *)NULL;

    if ((dmp = dm_open(type, ac, av)) == DM_NULL) {
      Tcl_AppendResult(interp,
		       "dmo_open_tcl: Failed to open - ",
		       argv[2],
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
  bu_vls_strcpy(&dmop->dmo_name,argv[2]);
  dmop->dmo_dmp = dmp;
  dmop->dmo_perspective = 0;
  VSETALL(dmop->dmo_dmp->dm_clipmin, -2048.0);
  VSETALL(dmop->dmo_dmp->dm_clipmax, 2047.0);

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

HIDDEN int
dmo_drawBegin_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return DM_DRAW_BEGIN(dmop->dmo_dmp);
}

HIDDEN int
dmo_drawEnd_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return DM_DRAW_END(dmop->dmo_dmp);
}

HIDDEN int
dmo_normal_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;

  return DM_NORMAL(dmop->dmo_dmp);
}

HIDDEN int
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

HIDDEN int
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

HIDDEN int
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

HIDDEN int
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
HIDDEN int
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

  return DM_DRAW_VLIST(dmop->dmo_dmp,vp,dmop->dmo_perspective);
}

/*
 * Usage:
 *	  procname drawSList sid
 */
HIDDEN int
dmo_drawSList_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct solid *hsp;
  struct solid *sp;
  struct rt_vlist *vp;
  int linestyle;
#if 0
  fastf_t ratio;
  fastf_t inv_viewsize;
#endif

  if (argc != 3) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib drawSList");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (sscanf(argv[2], "%lu", (unsigned long *)&hsp) != 1) {
    Tcl_AppendResult(interp, "invalid vlist pointer - ", argv[2], (char *)NULL);
    return TCL_ERROR;
  }

#if 0
  inv_viewsize = 1 / VIEWSIZE;
#endif

  FOR_ALL_SOLIDS(sp, &hsp->l) {
#if 0
    ratio = sp->s_size * inv_viewsize;

    /*
     * Check for this object being bigger than 
     * dmp->dm_bound * the window size, or smaller than a speck.
     */
    if (ratio >= dmop->dmo_dmp->dm_bound || ratio < 0.001)
      continue;
#endif

    if (linestyle != sp->s_soldash) {
      linestyle = sp->s_soldash;
      DM_SET_LINE_ATTR(dmop->dmo_dmp, dmop->dmo_dmp->dm_lineWidth, linestyle);
    }

    DM_SET_FGCOLOR(dmop->dmo_dmp,
		   (short)sp->s_color[0],
		   (short)sp->s_color[1],
		   (short)sp->s_color[2], 0);
    DM_DRAW_VLIST(dmop->dmo_dmp,
		  (struct rt_vlist *)&sp->s_vlist,
		  dmop->dmo_perspective);
  }

  return TCL_OK;
}

/*
 * Get/set the display manager's foreground color.
 *
 * Usage:
 *	  procname fg [rgb]
 */
HIDDEN int
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
HIDDEN int
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
HIDDEN int
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
HIDDEN int
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
HIDDEN int
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

  dm_configureWindowShape(dmop->dmo_dmp);
  return TCL_OK;
}

/*
 * Get/set the display manager's zclip flag.
 *
 * Usage:
 *	  procname zclip [0|1]
 */
HIDDEN int
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
 * Get/set the display manager's window bounds.
 *
 * Usage:
 *	  procname bounds ["xmin xmax ymin ymax zmin zmax"]
 */
HIDDEN int
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
HIDDEN int
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
    bu_vls_printf(&vls, "%d", dmop->dmo_perspective);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  /* set perspective mode */
  if (argc == 3) {
    if (sscanf(argv[2], "%d", &perspective) != 1) {
      Tcl_AppendResult(interp, "dmo_perspective: invalid debug level - ",
		       argv[2], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    dmop->dmo_perspective = perspective;
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
HIDDEN int
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

/*
 * Open/activate the display managers framebuffer.
 *
 * Usage:
 *	  procname fb
 *
 * Returns the port number in interp->result.
 */
HIDDEN int
dmo_fb_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct dm_obj *dmop = (struct dm_obj *)clientData;
  struct bu_vls vls;

  return TCL_OK;
}
