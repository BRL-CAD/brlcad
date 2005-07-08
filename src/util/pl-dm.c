/*                         P L - D M . C
 * BRL-CAD
 *
 * Copyright (C) 1999-2005 United States Government as represented by
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
/** @file pl-dm.c
 *
 *  Example application that shows how to hook into the display manager.
 *
 *  Author -
 *	Bob Parker
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "tcl.h"
#include "tk.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "bu.h"
#include "bn.h"
#include "dm.h"
#include "dm-X.h"

#ifdef DM_OGL
#  include <GL/glx.h>
#  include <GL/gl.h>
#  include "dm-ogl.h"
#endif


static int appInit();
static int islewview();
static int slewview();
static int zoom();
static void refresh();
static void size_reset();
static void vrot();
static void setview();
static void new_mats();
static void buildHrot();

static int cmd_aetview();
static int cmd_clear();
static int cmd_closepl();
static int cmd_dm();
static int cmd_draw();
static int cmd_erase();
static int cmd_exit();
static int cmd_list();
static int cmd_openpl();
static int cmd_reset();
static int cmd_slewview();
static int cmd_vrot();
static int cmd_zoom();
static void cmd_setup();

static int X_dmInit();
static int X_doEvent();
static int X_dm();

#ifdef DM_OGL
static int Ogl_dmInit();
static int Ogl_doEvent();
static int Ogl_dm();
static void Ogl_colorchange();
static void Ogl_establish_zbuffer();
static void Ogl_establish_lighting();
#endif

struct cmdtab {
  char *ct_name;
  int (*ct_func)();
};

static struct cmdtab cmdtab[] = {
"ae", cmd_aetview,
"clear", cmd_clear,
"closepl", cmd_closepl,
"dm", cmd_dm,
"draw", cmd_draw,
"erase", cmd_erase,
"exit", cmd_exit,
"ls", cmd_list,
"openpl", cmd_openpl,
"q", cmd_exit,
"reset", cmd_reset,
"sv", cmd_slewview,
"t", cmd_list,
"vrot", cmd_vrot,
"zoom", cmd_zoom,
(char *)NULL, (int (*)())NULL
};

#define MAXARGS 9000
#define MOUSE_MODE_IDLE 0
#define MOUSE_MODE_ROTATE 1
#define MOUSE_MODE_TRANSLATE 3
#define MOUSE_MODE_ZOOM 4
#define X 0
#define Y 1
#define Z 2

Tcl_Interp *interp;
Tk_Window tkwin;
struct dm *dmp;
mat_t toViewcenter;
mat_t Viewrot;
mat_t model2view;
mat_t view2model;
fastf_t Viewscale;
fastf_t azimuth;
fastf_t elevation;
fastf_t twist;
double app_scale = 180.0 / 512.0;
int mouse_mode = MOUSE_MODE_IDLE;
int omx, omy;
int (*cmd_hook)();
static int windowbounds[6] = { 2047, -2048, 2047, -2048, 2047, -2048 };

double degtorad =  0.01745329251994329573;

struct plot_list{
  struct bu_list l;
  int pl_draw;
  int pl_edit;
  struct bu_vls pl_name;
  struct bn_vlblock *pl_vbp;
};

struct plot_list HeadPlot;

#ifdef DM_OGL
int dm_type = DM_TYPE_OGL;
#else
int dm_type = DM_TYPE_X;
#endif

#ifdef DM_OGL
struct bu_structparse Ogl_vparse[] = {
  {"%d",  1, "depthcue",	Ogl_MV_O(cueing_on),	Ogl_colorchange },
  {"%d",  1, "zclip",		Ogl_MV_O(zclipping_on),	refresh },
  {"%d",  1, "zbuffer",		Ogl_MV_O(zbuffer_on),	Ogl_establish_zbuffer },
  {"%d",  1, "lighting",	Ogl_MV_O(lighting_on),	Ogl_establish_lighting },
  {"%d",  1, "debug",		Ogl_MV_O(debug),	BU_STRUCTPARSE_FUNC_NULL },
  {"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

#else  /* !DM_OGL */

struct bu_structparse X_vparse[] = {
  {"%d",  1, "zclip",             X_MV_O(zclip),        refresh},
  {"%d",  1, "debug",             X_MV_O(debug),        BU_STRUCTPARSE_FUNC_NULL },
  {"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

#endif  /* DM_OGL */

static char usage[] = "Usage: pl-dm [-t o|X] plot_file(s)\n";

/*
 *                     O U T P U T _ C A T C H
 *
 * Gets the output from bu_log and appends it to clientdata vls.
 */
static int
output_catch(clientdata, str)
genptr_t clientdata;
genptr_t str;
{
    register struct bu_vls *vp = (struct bu_vls *)clientdata;
    register int len;

    BU_CK_VLS(vp);
    len = bu_vls_strlen(vp);
    bu_vls_strcat(vp, str);
    len = bu_vls_strlen(vp) - len;

    return len;
}

/*
 *                 S T A R T _ C A T C H I N G _ O U T P U T
 *
 * Sets up hooks to bu_log so that all output is caught in the given vls.
 *
 */
void
start_catching_output(vp)
struct bu_vls *vp;
{
    bu_log_add_hook(output_catch, (genptr_t)vp);
}

/*
 *                 S T O P _ C A T C H I N G _ O U T P U T
 *
 * Turns off the output catch hook.
 */
void
stop_catching_output(vp)
struct bu_vls *vp;
{
    bu_log_delete_hook(output_catch, (genptr_t)vp);
}

int
get_args( argc, argv )
register char **argv;
{
  register int c;

  while ((c = bu_getopt( argc, argv, "t:" )) != EOF) {
    switch (c) {
    case 't':
      switch (*bu_optarg) {
      case 'x':
      case 'X':
	dm_type = DM_TYPE_X;
	break;
#ifdef DM_OGL
      case 'o':
      case 'O':
	dm_type = DM_TYPE_OGL;
	break;
#endif
      default:
	dm_type = DM_TYPE_X;
	break;
      }
      break;
    default:		/* '?' */
      return(0);
    }
  }

  if (bu_optind >= argc) {
    if (isatty(fileno(stdin)))
      return(0);
  }

  return(1);		/* OK */
}

int
main(argc, argv)
int argc;
char *argv[];
{
  int n;
  char *file;
  FILE *fp;
  struct plot_list *plp;

  if(!get_args(argc, argv)){
    bu_log("%s", usage);
    exit(1);
  }

  bzero((void *)&HeadPlot, sizeof(struct plot_list));
  BU_LIST_INIT(&HeadPlot.l);

  MAT_IDN(toViewcenter);
  MAT_IDN(Viewrot);
  MAT_IDN(model2view);
  MAT_IDN(view2model);

  if(cmd_openpl((ClientData)NULL, (Tcl_Interp *)NULL,
		   argc-bu_optind+1, argv+bu_optind-1) == TCL_ERROR)
    exit(1);

  argv[1] = (char *)NULL;
  Tk_Main(1, argv, appInit);

  exit(0);
}

static int
appInit(_interp)
Tcl_Interp *_interp;
{
  char *filename;
  struct bu_vls str;
  struct bu_vls str2;

  /* libdm uses interp */
  interp = _interp;

  switch(dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    cmd_hook = Ogl_dm;
    break;
#endif
  case DM_TYPE_X:
  default:
    cmd_hook = X_dm;
    break;
  }

  /* Evaluates init.tcl */
  if(Tcl_Init(interp) == TCL_ERROR){
    bu_log("Tcl_Init error %s\n", interp->result);
    exit(1);
  }

  /*
   * Creates the main window and registers all of Tk's commands
   * into the interpreter.
   */
  if (Tk_Init(interp) == TCL_ERROR){
    bu_log("Tk_Init error %s\n", interp->result);
    exit(1);
  }

  if((tkwin = Tk_MainWindow(interp)) == NULL){
    bu_log("appInit: Failed to get main window.\n");
    exit(1);
  }

  /* Locate the BRL-CAD-specific Tcl scripts */
  filename = bu_brlcad_data( "tclscripts", 0 );

  bu_vls_init(&str);
  bu_vls_init(&str2);
  bu_vls_printf(&str2, "%s/pl-dm", filename);
  bu_vls_printf(&str, "wm withdraw .; set auto_path [linsert $auto_path 0 %s %s]",
		bu_vls_addr(&str2), filename);
  (void)Tcl_Eval(interp, bu_vls_addr(&str));
  bu_vls_free(&str);
  bu_vls_free(&str2);

  /* register application commands */
  cmd_setup(interp, cmdtab);

  /* open display manager */
  switch(dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    return Ogl_dmInit();
#endif
  case DM_TYPE_X:
  default:
    return X_dmInit();
  }
}

static void
refresh(){
  int i;
  struct plot_list *plp;

  DM_DRAW_BEGIN(dmp);
  DM_LOADMATRIX(dmp,model2view,0);

  for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
    if(plp->pl_draw)
      for(i=0; i < plp->pl_vbp->nused; i++){
	if(plp->pl_vbp->rgb[i] != 0){
	  long rgb;

	  rgb = plp->pl_vbp->rgb[i];
	  DM_SET_FGCOLOR(dmp, (rgb>>16) & 0xFF, (rgb>>8) & 0xFF, rgb & 0xFF, 0, (fastf_t)0.0);
	}

	DM_DRAW_VLIST(dmp, (struct bn_vlist *)&plp->pl_vbp->head[i]);
      }
  }

  DM_NORMAL(dmp);
  DM_DRAW_END(dmp);
}

static void
vrot(x, y, z)
double x, y, z;
{
  mat_t newrot;

  MAT_IDN( newrot );
  buildHrot( newrot,
	     x * degtorad,
	     y * degtorad,
	     z * degtorad );
  bn_mat_mul2( newrot, Viewrot );
}

/*
 *                      S E T V I E W
 *
 * Set the view.  Angles are DOUBLES, in degrees.
 *
 * Given that viewvec = scale . rotate . (xlate to view center) . modelvec,
 * we just replace the rotation matrix.
 * (This assumes rotation around the view center).
 */
static void
setview(a1, a2, a3)
double a1, a2, a3;		/* DOUBLE angles, in degrees */
{
  point_t model_pos;
  point_t new_pos;

  buildHrot(Viewrot, a1 * degtorad, a2 * degtorad, a3 * degtorad);
}

static int
islewview(vdiff)
vect_t vdiff;
{
  vect_t old_model_pos;
  vect_t old_view_pos;
  vect_t view_pos;

  MAT_DELTAS_GET_NEG(old_model_pos, toViewcenter);
  MAT4X3PNT(old_view_pos, model2view, old_model_pos);
  VSUB2(view_pos, old_view_pos, vdiff);
  
  return slewview(view_pos);
}

static int
slewview(view_pos)
vect_t view_pos;
{
  vect_t new_model_pos;

  MAT4X3PNT( new_model_pos, view2model, view_pos );
  MAT_DELTAS_VEC_NEG( toViewcenter, new_model_pos );

  return TCL_OK;
}

static int
zoom(interp, val)
Tcl_Interp *interp;
double val;
{
  if( val < SMALL_FASTF || val > INFINITY )  {
    Tcl_AppendResult(interp,
		     "zoom: scale factor out of range\n", (char *)NULL);
    return TCL_ERROR;
  }

  if( Viewscale < SMALL_FASTF || INFINITY < Viewscale )
    return TCL_ERROR;

  Viewscale /= val;

  return TCL_OK;
}

/*
 *                      S I Z E _ R E S E T
 *
 *  Reset view size and view center so that everything in the vlist
 *  is in view.
 *  Caller is responsible for calling new_mats().
 */
static void
size_reset()
{
  int i;
  register struct bn_vlist *tvp;
  vect_t min, max;
  vect_t center;
  vect_t radial;
  struct plot_list *plp;

  VSETALL( min,  INFINITY );
  VSETALL( max, -INFINITY );

  for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
    struct bn_vlblock *vbp;

    vbp = plp->pl_vbp;
    for(i=0; i < vbp->nused; i++){
      register struct bn_vlist *vp = (struct bn_vlist *)&vbp->head[i];

      for(BU_LIST_FOR(tvp, bn_vlist, &vp->l)){
	register int j;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	register point_t *pt = tvp->pt;

	for(j = 0; j < nused; j++,cmd++,pt++ ){
	  switch(*cmd){
	  case BN_VLIST_POLY_START:
	  case BN_VLIST_POLY_VERTNORM:
	    break;
	  case BN_VLIST_POLY_MOVE:
	  case BN_VLIST_LINE_MOVE:
	  case BN_VLIST_POLY_DRAW:
	  case BN_VLIST_POLY_END:
	  case BN_VLIST_LINE_DRAW:
	    VMIN(min, *pt);
	    VMAX(max, *pt);
	    break;
	  }
	}
      }
    }
  }

  VADD2SCALE( center, max, min, 0.5 );
  VSUB2( radial, max, center );

  if( VNEAR_ZERO( radial , SQRT_SMALL_FASTF ) )
    VSETALL( radial , 1.0 );

  MAT_IDN( toViewcenter );
  MAT_DELTAS( toViewcenter, -center[X], -center[Y], -center[Z] );
  Viewscale = radial[X];
  V_MAX( Viewscale, radial[Y] );
  V_MAX( Viewscale, radial[Z] );
}

/*
 *                      N E W _ M A T S
 *  
 *  Derive the inverse and editing matrices, as required.
 *  Centralized here to simplify things.
 */
static void
new_mats()
{
  bn_mat_mul( model2view, Viewrot, toViewcenter );
  model2view[15] = Viewscale;
  bn_mat_inv( view2model, model2view );
}

/*
 *                      B U I L D H R O T
 *
 * This routine builds a Homogeneous rotation matrix, given
 * alpha, beta, and gamma as angles of rotation.
 *
 * NOTE:  Only initialize the rotation 3x3 parts of the 4x4
 * There is important information in dx,dy,dz,s .
 */
static void
buildHrot( mat, alpha, beta, ggamma )
register matp_t mat;
double alpha, beta, ggamma;
{
  static fastf_t calpha, cbeta, cgamma;
  static fastf_t salpha, sbeta, sgamma;

  calpha = cos( alpha );
  cbeta = cos( beta );
  cgamma = cos( ggamma );

  salpha = sin( alpha );
  sbeta = sin( beta );
  sgamma = sin( ggamma );

  /*
   * compute the new rotation to apply to the previous
   * viewing rotation.
   * Alpha is angle of rotation about the X axis, and is done third.
   * Beta is angle of rotation about the Y axis, and is done second.
   * Gamma is angle of rotation about Z axis, and is done first.
   */
#ifdef m_RZ_RY_RX
  /* view = model * RZ * RY * RX (Neuman+Sproul, premultiply) */
  mat[0] = cbeta * cgamma;
  mat[1] = -cbeta * sgamma;
  mat[2] = -sbeta;

  mat[4] = -salpha * sbeta * cgamma + calpha * sgamma;
  mat[5] = salpha * sbeta * sgamma + calpha * cgamma;
  mat[6] = -salpha * cbeta;

  mat[8] = calpha * sbeta * cgamma + salpha * sgamma;
  mat[9] = -calpha * sbeta * sgamma + salpha * cgamma;
  mat[10] = calpha * cbeta;
#endif
  /* This is the correct form for this version of GED */
  /* view = RX * RY * RZ * model (Rodgers, postmultiply) */
  /* Point thumb along axis of rotation.  +Angle as hand closes */
  mat[0] = cbeta * cgamma;
  mat[1] = -cbeta * sgamma;
  mat[2] = sbeta;

  mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
  mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
  mat[6] = -salpha * cbeta;

  mat[8] = -calpha * sbeta * cgamma + salpha * sgamma;
  mat[9] = calpha * sbeta * sgamma + salpha * cgamma;
  mat[10] = calpha * cbeta;
}

/*
 * Open one or more plot files.
 */
static int
cmd_openpl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  static int not_first = 0;
  int i;
  int do_size_reset;
  int read_file;
  char *file;
  FILE *fp;
  struct plot_list *plp;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help openpl");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* check to see if we should resize the view */
  if(BU_LIST_IS_EMPTY(&HeadPlot.l))
    do_size_reset = 1;
  else
    do_size_reset = 0;

  /* read plot files */
  for(read_file=0,i=1; i < argc; ++i){
    char *bnp;

    file = argv[i];
    if((fp = fopen(file,"r")) == NULL){
      bu_log("%s: can't open \"%s\"\n", argv[0], file);
      continue;
    }

    ++read_file;

    /* skip path */
    bnp = strrchr(argv[i], '/');
    if(bnp == (char *)NULL)
      bnp = argv[i];
    else
      ++bnp;

    /* check for existing objects with same name as argv[i] */
    for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
      /* found object with same name */
      if(!strcmp(bu_vls_addr(&plp->pl_name),bnp)){
	rt_vlblock_free(plp->pl_vbp);
	goto up_to_vl;
      }
    }

    BU_GETSTRUCT(plp, plot_list);
    BU_LIST_APPEND(&HeadPlot.l, &plp->l);
    bu_vls_init(&plp->pl_name);
    bu_vls_strcpy(&plp->pl_name, bnp);

  up_to_vl:
    plp->pl_vbp = rt_vlblock_init();
    rt_uplot_to_vlist(plp->pl_vbp, fp, 0.001);
    plp->pl_draw = 1;
    fclose( fp );
  }

  if(!read_file)
    return TCL_ERROR;

  if(do_size_reset){
    size_reset();
    new_mats();
  }

  if(not_first)
    refresh();
  else
    not_first = 1;

  return TCL_OK;
}

/*
 * Rotate the view.
 */
static int
cmd_vrot(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  mat_t newrot;

  if(argc != 4){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help vrot");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  vrot(atof(argv[1]), atof(argv[2]), atof(argv[3]));
  new_mats();
  refresh();

  return TCL_OK;
}

/*
 * Do display manager specific commands.
 */
static int
cmd_dm(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int status;

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help dm");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(cmd_hook)
    return cmd_hook(argc-1, argv+1);

  return TCL_ERROR;
}

/*
 * Clear the screen.
 */
static int
cmd_clear(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct plot_list *plp;

  if(argc != 1){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help clear");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l))
    plp->pl_draw = 0;

  refresh();
  return TCL_OK;
}

/*
 * Close the specified plots.
 */
static int
cmd_closepl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int i;
  struct plot_list *plp;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help closepl");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for(i=1; i < argc; ++i){
    for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
      if(!strcmp(argv[i], bu_vls_addr(&plp->pl_name))){
	BU_LIST_DEQUEUE(&plp->l);
	bu_vls_free(&plp->pl_name);
	rt_vlblock_free(plp->pl_vbp);
	bu_free((genptr_t)plp, "cmd_closepl");
	break;
      }
    }
  }

  refresh();
  return TCL_OK;
}

/*
 * Draw the specified plots.
 */
static int
cmd_draw(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int i;
  struct plot_list *plp;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help draw");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for(i=1; i < argc; ++i){
    for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
      if(!strcmp(argv[i], bu_vls_addr(&plp->pl_name))){
	plp->pl_draw = 1;
	break;
      }
    }
  }

  refresh();
  return TCL_OK;
}

/*
 * Erase the specified plots.
 */
static int
cmd_erase(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int i;
  struct plot_list *plp;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help erase");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for(i=1; i < argc; ++i){
    for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
      if(!strcmp(argv[i], bu_vls_addr(&plp->pl_name))){
	plp->pl_draw = 0;
	break;
      }
    }
  }

  refresh();
  return TCL_OK;
}

/*
 * Print a list of the load plot objects.
 */
static int
cmd_list(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct plot_list *plp;
  int len;
  int linelen=0;

  if(argc != 1){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help ls");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  for(BU_LIST_FOR(plp, plot_list, &HeadPlot.l)){
    len = strlen(bu_vls_addr(&plp->pl_name));

    if(len < 13)
      len = 16;
    else
      len += 4;

    linelen += len;
    if(linelen > 80)
      bu_log("\n%-*S", len, &plp->pl_name);
    else
      bu_log("%-*S", len, &plp->pl_name);
  }

  bu_log("\n");
  return TCL_OK;
}

/*
 *                      F _ Z O O M
 *
 *  A scale factor of 2 will increase the view size by a factor of 2,
 *  (i.e., a zoom out) which is accomplished by reducing Viewscale in half.
 */
static int
cmd_zoom(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int status;
  double	val;

  if(argc != 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help zoom");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  val = atof(argv[1]);
  status = zoom(interp, val);
  new_mats();
  refresh();

  return status;
}

static int
cmd_reset(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  size_reset();
  new_mats();
  refresh();

  return TCL_OK;
}

/*
 *                      S L E W V I E W
 *
 *  Given a position in view space,
 *  make that point the new view center.
 */
static int
cmd_slewview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  int status;
  vect_t view_pos;

  if(argc != 3){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help sv");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  view_pos[X] = atof(argv[1]) / 2047.0;
  view_pos[Y] = atof(argv[2]) / 2047.0;
  view_pos[Z] = 0.0;

  status = slewview(view_pos);
  new_mats();
  refresh();

  return status;
}

/* set view using azimuth, elevation and twist angles */
static int
cmd_aetview(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  int iflag = 0;
  fastf_t o_twist;

  if(argc < 3 || 5 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, "help ae");
    (void)Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  /* Check for -i option */
  if(argv[1][0] == '-' && argv[1][1] == 'i'){
    iflag = 1;  /* treat arguments as incremental values */
    ++argv;
    --argc;
  }

  /* grab old twist angle before it's lost */
  o_twist = twist;

  /* set view using azimuth and elevation angles */
  if(iflag)
    setview(270.0 + atof(argv[2]) + elevation,
	    0.0,
	    270.0 - atof(argv[1]) - azimuth);
  else
    setview(270.0 + atof(argv[2]), 0.0, 270.0 - atof(argv[1]));

  new_mats();

  if(argc == 4){ /* twist angle supplied */
    double x, y, z;

    x = y = 0.0;
    twist = atof(argv[3]);

    if(iflag)
      z = -o_twist - twist;
    else
      z = -twist;

    vrot(x, y, z);
    new_mats();
  }

  refresh();
  return TCL_OK;
}

/*
 * Exit.
 */
static int
cmd_exit(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  if(dmp != DM_NULL)
    DM_CLOSE(dmp);

  exit(0);

  /* not reached */
  return TCL_OK;
}


/*
 * Register application commands with the Tcl interpreter.
 */
static void
cmd_setup(interp, commands)
Tcl_Interp *interp;
struct cmdtab commands[];
{
  register struct cmdtab *ctp;

  for (ctp = commands; ctp->ct_name; ctp++) {
    (void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
			    (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
  }
}


/*
 * X Display Manager Specific Stuff
 */

/*
 * Open an X display manager.
 */
static int
X_dmInit()
{
  char *av[4];

  av[0] = "X_open";
  av[1] = "-i";
  av[2] = "sampler_bind_dm";
  av[3] = (char *)NULL;

  if((dmp = DM_OPEN(DM_TYPE_X, 3, av)) == DM_NULL){
    Tcl_AppendResult(interp, "Failed to open a display manager\n", (char *)NULL);
    return TCL_ERROR;
  }

  ((struct x_vars *)dmp->dm_vars.priv_vars)->mvars.zclip = 0;
  Tk_CreateGenericHandler(X_doEvent, (ClientData)DM_TYPE_X);
  dm_configureWindowShape(dmp);
  DM_SET_WIN_BOUNDS(dmp, windowbounds);

  return TCL_OK;
}

/* 
 * Event handler for the X display manager.
 */
static int
X_doEvent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    refresh();
  }else if(eventPtr->type == ConfigureNotify){
    dm_configureWindowShape(dmp);
    refresh();
  }else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(mouse_mode){
    case MOUSE_MODE_ROTATE:
      vrot((my - omy) * app_scale,
	   (mx - omx) * app_scale,
	   0.0);
      new_mats();
      refresh();

      break;
    case MOUSE_MODE_TRANSLATE:
      {
	vect_t vdiff;

	vdiff[X] = (mx - omx) /
	  (fastf_t)dmp->dm_width * 2.0;
	vdiff[Y] = (omy - my) /
	  (fastf_t)dmp->dm_height * 2.0;
	vdiff[Z] = 0.0;

	(void)islewview(vdiff);
	new_mats();
	refresh();
      }

      break;
    case MOUSE_MODE_ZOOM:
      {
	double val;

	val = 1.0 + (omy - my) /
	  (fastf_t)dmp->dm_height;

	zoom(interp, val);
	new_mats();
	refresh();
      }

      break;
    case MOUSE_MODE_IDLE:
    default:
      break;
    }

    omx = mx;
    omy = my;
  }

  return TCL_OK;
}

/*
 * Handle X display manger specific commands.
 */
static int
X_dm(argc, argv)
int argc;
char *argv[];
{
  int status;

  if( !strcmp( argv[0], "set" )){
    struct bu_vls tmp_vls;
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("X internal variables", X_vparse, (const char *)&((struct x_vars *)dmp->dm_vars.priv_vars)->mvars);
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, X_vparse, argv[1], (const char *)&((struct x_vars *)dmp->dm_vars.priv_vars)->mvars, ',');
      bu_log( "%S\n", &vls );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, X_vparse, (char *)&((struct x_vars *)dmp->dm_vars.priv_vars)->mvars);
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m")){
    vect_t view_pos;

    if( argc < 4){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

#if 0
    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      return TCL_ERROR;
    }
#endif

    view_pos[X] = dm_Xx2Normal(dmp, atoi(argv[3]));
    view_pos[Y] = dm_Xy2Normal(dmp, atoi(argv[4]), 0);
    view_pos[Z] = 0.0;
    status = slewview(view_pos);
    new_mats();
    refresh();

    return status;
  }

  if( !strcmp( argv[0], "am" )){
    int buttonpress;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    omx = atoi(argv[3]);
    omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	mouse_mode = MOUSE_MODE_ROTATE;
	break;
      case 't':
	mouse_mode = MOUSE_MODE_TRANSLATE;
	break;
      case 'z':
	mouse_mode = MOUSE_MODE_ZOOM;
	break;
      default:
	mouse_mode = MOUSE_MODE_IDLE;
	break;
      }
    }else
      mouse_mode = MOUSE_MODE_IDLE;
  }

  return TCL_OK;
}

#ifdef DM_OGL
/*
 * ogl Display Manager Specific Stuff
 */

/*
 * Open an ogl display manager.
 */
static int
Ogl_dmInit()
{
  char *av[4];
  GLdouble lw_range[2];       /* line width range */
  GLdouble lw_gran;    /* line width granularity */

  av[0] = "ogl_open";
  av[1] = "-i";
  av[2] = "sampler_bind_dm";
  av[3] = (char *)NULL;

  if((dmp = DM_OPEN(DM_TYPE_OGL, 3, av)) == DM_NULL){
    Tcl_AppendResult(interp, "Failed to open a display manager\n", (char *)NULL);
    return TCL_ERROR;
  }

  dmp->dm_vp = &Viewscale;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zclipping_on = 0;
  Tk_CreateGenericHandler(Ogl_doEvent, (ClientData)DM_TYPE_OGL);
  dm_configureWindowShape(dmp);
  DM_SET_WIN_BOUNDS(dmp, windowbounds);

  return TCL_OK;
}

/*
 * Event handler for the ogl display manager.
 */
static int
Ogl_doEvent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    refresh();
  }else if(eventPtr->type == ConfigureNotify){
    dm_configureWindowShape(dmp);
    refresh();
  }else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(mouse_mode){
    case MOUSE_MODE_ROTATE:
      vrot((my - omy) * app_scale,
	   (mx - omx) * app_scale,
	   0.0);
      new_mats();
      refresh();

      break;
    case MOUSE_MODE_TRANSLATE:
      {
	vect_t vdiff;

	vdiff[X] = (mx - omx) /
	  (fastf_t)dmp->dm_width * 2.0;
	vdiff[Y] = (omy - my) /
	  (fastf_t)dmp->dm_height * 2.0;
	vdiff[Z] = 0.0;

	(void)islewview(vdiff);
	new_mats();
	refresh();
      }

      break;
    case MOUSE_MODE_ZOOM:
      {
	double val;

	val = 1.0 + (omy - my) /
	  (fastf_t)dmp->dm_height;

	zoom(interp, val);
	new_mats();
	refresh();
      }

      break;
    case MOUSE_MODE_IDLE:
    default:
      break;
    }

    omx = mx;
    omy = my;
  }

  return TCL_OK;
}

/*
 * Handle ogl display manager specific commands.
 */
static int
Ogl_dm(argc, argv)
int argc;
char *argv[];
{
  int status;

  if( !strcmp( argv[0], "set" ) )  {
    struct bu_vls tmp_vls;
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_ogl internal variables", Ogl_vparse, (const char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Ogl_vparse, argv[1], (const char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars, ',');
      bu_log( "%S\n", &vls );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, Ogl_vparse, (char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars );
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m")){
    vect_t view_pos;

    if( argc < 4){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

#if 0
    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct ogl_vars *)dmp->dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct ogl_vars *)dmp->dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct ogl_vars *)dmp->dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      return TCL_ERROR;
    }
#endif

    view_pos[X] = dm_Xx2Normal(dmp, atoi(argv[3]));
    view_pos[Y] = dm_Xy2Normal(dmp, atoi(argv[4]), 0);
    view_pos[Z] = 0.0;
    status = slewview(view_pos);
    new_mats();
    refresh();

    return status;
  }

  if( !strcmp( argv[0], "am" )){
    int buttonpress;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    omx = atoi(argv[3]);
    omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	mouse_mode = MOUSE_MODE_ROTATE;
	break;
      case 't':
	mouse_mode = MOUSE_MODE_TRANSLATE;
	break;
      case 'z':
	mouse_mode = MOUSE_MODE_ZOOM;
	break;
      default:
	mouse_mode = MOUSE_MODE_IDLE;
	break;
      }
    }else
      mouse_mode = MOUSE_MODE_IDLE;
  }

  return TCL_OK;
}

static void
Ogl_colorchange()
{
  if(((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
    glEnable(GL_FOG);
  }else{
    glDisable(GL_FOG);
  }

  refresh();
}

static void
Ogl_establish_zbuffer()
{
  dm_zbuffer(dmp,
	     ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on);
  refresh();
}

static void
Ogl_establish_lighting()
{
  dm_lighting(dmp,
	      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on);
  refresh();
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
