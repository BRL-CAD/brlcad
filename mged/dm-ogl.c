/*
 *			D M - O G L . C
 *
 *  Routines specific to MGED's use of LIBDM's OpenGl display manager.
 *
 *  Author -
 *	Robert G. Parker
 *  
 *  Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#ifndef lint
static char RCSid[] = "@(#)$Header";
#endif

#include "conf.h"

#include <math.h>
#include "tk.h"

#if 1
#ifdef USE_MESA_GL
#include <MESA_GL/glx.h>
#include <MESA_GL/gl.h>
#else
#include <GL/glx.h>
#include <GL/gl.h>
#include <gl/device.h>
#endif
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm_xvars.h"
#include "dm-ogl.h"
#include "./ged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern int common_dm();			/* defined in dm-generic.c */
extern void dm_var_init();		/* defined in attach.c */
extern void cs_set_bg();		/* defined in color_scheme.c */

static int	Ogl_dm();
static int	Ogl_doevent();
static void     Ogl_colorchange();
static void     establish_zbuffer();
static void     establish_lighting();
static void     dirty_hook();
static void     do_fogHint();

struct bu_structparse Ogl_vparse[] = {
	{"%d",	1, "depthcue",		Ogl_MV_O(cueing_on),	Ogl_colorchange },
	{"%d",  1, "zclip",		Ogl_MV_O(zclipping_on),	dirty_hook },
	{"%d",  1, "zbuffer",		Ogl_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		Ogl_MV_O(lighting_on),	establish_lighting },
	{"%d",  1, "fastfog",		Ogl_MV_O(fastfog),	do_fogHint },
	{"%f",  1, "density",		Ogl_MV_O(fogdensity),	dirty_hook },
	{"%d",  1, "has_zbuf",		Ogl_MV_O(zbuf),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "has_rgb",		Ogl_MV_O(rgb),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "has_doublebuffer",	Ogl_MV_O(doublebuffer), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "depth",		Ogl_MV_O(depth),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "debug",		Ogl_MV_O(debug),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

int
Ogl_dm_init(o_dm_list, argc, argv)
struct dm_list *o_dm_list;
int argc;
char *argv[];
{
  int i;
  struct bu_vls vls;

  dm_var_init(o_dm_list);

  /* register application provided routines */
  cmd_hook = Ogl_dm;

  Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);

  if((dmp = dm_open(DM_TYPE_OGL, argc-1, argv)) == DM_NULL)
    return TCL_ERROR;

  /*XXXX this eventually needs to move into Ogl's private structure */
  dmp->dm_vp = &view_state->vs_Viewscale;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->perspective_mode = &mged_variables->mv_perspective_mode;
  zclip_ptr = &((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zclipping_on;
  eventHandler = Ogl_doevent;
  Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
  dm_configureWindowShape(dmp);

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&pathName));
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);

#if 0
  /*XXX Experimenting */
  bu_vls_printf(&vls1, "dm_info(%s)", bu_vls_addr(&dmp->dm_pathName));
  bu_vls_printf(&vls2, "%lu %lu %lu %lu %d %d %lu %d %d",
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap,
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip,
		dmp->dm_width, dmp->dm_height,
		(unsigned long)((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc,
		((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer, 0);
  Tcl_SetVar(interp, bu_vls_addr(&vls1), bu_vls_addr(&vls2), TCL_GLOBAL_ONLY);
#endif

  /* initialize the background color */
  cs_set_bg();

  return TCL_OK;
}

void
Ogl_fb_open()
{
  int status;
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "fb_open_existing /dev/ogl %lu %lu %lu %lu %d %d %lu %d %d",
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap,
		(unsigned long)((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip,
		dmp->dm_width, dmp->dm_height,
		(unsigned long)((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc,
		((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer, 0);
  status = Tcl_Eval(interp, bu_vls_addr(&vls));

  if(status == TCL_OK){
    if(sscanf(interp->result, "%lu", (unsigned long *)&fbp) != 1){
      fbp = (FBIO *)0;   /* sanity */
      Tcl_AppendResult(interp, "Ogl_fb_open: failed to get framebuffer pointer\n",
		       (char *)NULL);
    }
  }else
    Tcl_AppendResult(interp, "Ogl_fb_open: failed to get framebuffer\n",
		     (char *)NULL);

  bu_vls_free(&vls);
}

/*
   This routine is being called from doEvent().
   It does not handle mouse button or key events. The key
   events are being processed via the TCL/TK bind command or are being
   piped to ged.c/stdin_input(). Eventually, I'd also like to have the
   dials+buttons bindable. That would leave this routine to handle only
   events like Expose and ConfigureNotify.
*/
static int
Ogl_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  if(!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
     ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
     ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc))
    /* allow further processing of this event */
    return TCL_OK;

  if(eventPtr->type == Expose && eventPtr->xexpose.count == 0){
#if 0
    glClearColor(0.0, 0.0, 0.0, 0.0);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    dirty = 1;

    /* no further processing of this event */
    return TCL_RETURN;
  }

  /* allow further processing of this event */
  return TCL_OK;
}

/*
 *			O G L _ D M
 * 
 *  Implement display-manager specific commands, from MGED "dm" command.
 */
static int
Ogl_dm(argc, argv)
int argc;
char **argv;
{
  int status;
  struct bu_vls	vls;

  if( !strcmp( argv[0], "set" ) )  {
    struct bu_vls tmp_vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_ogl internal variables", Ogl_vparse, (CONST char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Ogl_vparse, argv[1], (CONST char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
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

  return common_dm(argc, argv);
}

static void
Ogl_colorchange()
{
  if(((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
    glEnable(GL_FOG);
  }else{
    glDisable(GL_FOG);
  }

  ++view_state->vs_flag;
}

static void
establish_zbuffer()
{
  dm_zbuffer(dmp);
  ++view_state->vs_flag;
}

static void
establish_lighting()
{
  dm_lighting(dmp);
  ++view_state->vs_flag;
}

static void
do_fogHint()
{
  ogl_fogHint(dmp);
  ++view_state->vs_flag;
}

static void
dirty_hook()
{
  dirty = 1;
}
