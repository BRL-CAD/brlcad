 /*
 *			D M - O G L . C
 *
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <termio.h>
#undef VMIN		/* is used in vmath.h, too */
#include <ctype.h>

#include <sys/types.h>
#include <sys/time.h>

#include <X11/X.h>
#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "tcl.h"
#include "tk.h"
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include "./oglinit.h"

#include <GL/glx.h>
#include <GL/gl.h>
/*XXXX*/
#if 1
#include <gl/device.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

extern int dm_pipe[];

extern Tcl_Interp *interp;
extern Tk_Window tkwin;

extern void sl_toggle_scroll();		/* from scroll.c */

static void     establish_perspective();
static void     set_perspective();
static void	establish_lighting();
static void	establish_zbuffer();
static int	Ogl2_setup();
static void     set_knob_offset();
static void	Ogl2_configure_window_shape();
static int	Ogl2_doevent();
static void	Ogl2_gen_color();
static void     Ogl2_colorit();
static int      Ogl2_load_startup();
static void     ogl_var_init();
static XVisualInfo *Ogl2_set_visual();
static void     print_cmap();
static struct dm_list *get_dm_list();
static int irisX2ged();
static int irisY2ged();

/* Display Manager package interface */

#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
int	Ogl2_open();
int	Ogl2_dm();
void	Ogl2_close();
MGED_EXTERN(void	Ogl2_input, (fd_set *input, int noblock) );
void	Ogl2_prolog(), Ogl2_epilog();
void	Ogl2_normal(), Ogl2_newrot();
void	Ogl2_update();
void	Ogl2_puts(), Ogl2_2d_line(), Ogl2_light();
int	Ogl2_object();
unsigned Ogl2_cvtvecs(), Ogl2_load();
void	Ogl2_statechange(), Ogl2_viewchange(), Ogl2_colorchange();
void	Ogl2_window(), Ogl2_debug(), Ogl2_selectargs();

struct dm dm_ogl2 = {
	Ogl2_open, Ogl2_close,
	Ogl2_input,
	Ogl2_prolog, Ogl2_epilog,
	Ogl2_normal, Ogl2_newrot,
	Ogl2_update,
	Ogl2_puts, Ogl2_2d_line,
	Ogl2_light,
	Ogl2_object,	Ogl2_cvtvecs, Ogl2_load,
	Ogl2_statechange,
	Ogl2_viewchange,
	Ogl2_colorchange,
	Ogl2_window, Ogl2_debug,
	0,				/* no displaylist */
	0,				/* multi-window */
	IRBOUND,
	"ogl", "X Windows with OpenGL graphics",
	0,				/* mem map */
	Ogl2_dm
};


/* ogl stuff */
#define NSLOTS		4080	/* The mostest possible - may be fewer */
#define dpy (((struct ogl_vars *)dm_vars)->_dpy)
#define win (((struct ogl_vars *)dm_vars)->_win)
#define xtkwin (((struct ogl_vars *)dm_vars)->_xtkwin)
#define omx (((struct ogl_vars *)dm_vars)->_omx)
#define omy (((struct ogl_vars *)dm_vars)->_omy)
#define perspective_angle (((struct ogl_vars *)dm_vars)->_perspective_angle)
#define devmotionnotify (((struct ogl_vars *)dm_vars)->_devmotionnotify)
#define devbuttonpress (((struct ogl_vars *)dm_vars)->_devbuttonpress)
#define devbuttonrelease (((struct ogl_vars *)dm_vars)->_devbuttonrelease)
#define knobs (((struct ogl_vars *)dm_vars)->_knobs)
#define stereo_is_on (((struct ogl_vars *)dm_vars)->_stereo_is_on)
#define aspect (((struct ogl_vars *)dm_vars)->_aspect)
#define glxc (((struct ogl_vars *)dm_vars)->_glxc)
#define fontstruct (((struct ogl_vars *)dm_vars)->_fontstruct)
#define fontOffset (((struct ogl_vars *)dm_vars)->_fontOffset)
#define ovec (((struct ogl_vars *)dm_vars)->_ovec)
#define ogl_is_direct (((struct ogl_vars *)dm_vars)->_ogl_is_direct)
#define ogl_index_size (((struct ogl_vars *)dm_vars)->_ogl_index_size)
#define ogl_nslots (((struct ogl_vars *)dm_vars)->_ogl_nslots)
#define slotsused (((struct ogl_vars *)dm_vars)->_slotsused)
#define ogl_rgbtab (((struct ogl_vars *)dm_vars)->_ogl_rgbtab)

struct modifiable_ogl_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int perspective_mode;
  int dummy_perspective;
  int zbuf;
  int rgb;
  int doublebuffer;
  int depth;
  int debug;
  int linewidth;
  int fastfog;
  double fogdensity;
  int virtual_trackball;
};

struct ogl_vars {
  struct rt_list l;
  struct dm_list *dm_list;
  Display *_dpy;
  Window _win;
  Tk_Window _xtkwin;
  Colormap cmap;
  GLdouble faceplate_mat[16];
  int face_flag;
  int width;
  int height;
  int _omx, _omy;
  int _perspective_angle;
  int _devmotionnotify;
  int _devbuttonpress;
  int _devbuttonrelease;
  int _knobs[8];
  int _stereo_is_on;
  fastf_t _aspect;
  GLXContext _glxc;
  XFontStruct *_fontstruct;
  int _fontOffset;
  int _ovec;		/* Old color map entry number */
  char    _ogl_is_direct;
  int _ogl_index_size;
/*
 * SGI Color Map table
 */
  int _ogl_nslots;		/* how many we have, <= NSLOTS */
  int _slotsused;		/* how many actually used */
  struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
  }_ogl_rgbtab[NSLOTS];
  struct modifiable_ogl_vars mvars;
};

#ifdef IR_BUTTONS
/*
 * Map SGI Button numbers to MGED button functions.
 * The layout of this table is suggestive of the actual button box layout.
 */
#define SW_HELP_KEY	SW0
#define SW_ZERO_KEY	SW3
#define HELP_KEY	0
#define ZERO_KNOBS	0
static unsigned char bmap[IR_BUTTONS] = {
	HELP_KEY,    BV_ADCURSOR, BV_RESET,    ZERO_KNOBS,
	BE_O_SCALE,  BE_O_XSCALE, BE_O_YSCALE, BE_O_ZSCALE, 0,           BV_VSAVE,
	BE_O_X,      BE_O_Y,      BE_O_XY,     BE_O_ROTATE, 0,           BV_VRESTORE,
	BE_S_TRANS,  BE_S_ROTATE, BE_S_SCALE,  BE_MENU,     BE_O_ILLUMINATE, BE_S_ILLUMINATE,
	BE_REJECT,   BV_BOTTOM,   BV_TOP,      BV_REAR,     BV_45_45,    BE_ACCEPT,
	BV_RIGHT,    BV_FRONT,    BV_LEFT,     BV_35_25
};
#endif

#ifdef IR_KNOBS
static int irlimit();			/* provides knob dead spot */
#define NOISE 32		/* Size of dead spot on knob */
/*
 *  Labels for knobs in help mode.
 */
static char	*kn1_knobs[] = {
	/* 0 */ "adc <1",	/* 1 */ "zoom", 
	/* 2 */ "adc <2",	/* 3 */ "adc dist",
	/* 4 */ "adc y",	/* 5 */ "y slew",
	/* 6 */ "adc x",	/* 7 */	"x slew"
};
static char	*kn2_knobs[] = {
	/* 0 */ "unused",	/* 1 */	"zoom",
	/* 2 */ "z rot",	/* 3 */ "z slew",
	/* 4 */ "y rot",	/* 5 */ "y slew",
	/* 6 */ "x rot",	/* 7 */	"x slew"
};
#endif

static struct ogl_vars head_ogl_vars;
static int perspective_table[] = {
	30, 45, 60, 90 };
static double	xlim_view = 1.0;	/* args for glOrtho*/
static double	ylim_view = 1.0;

extern struct device_values dm_values;	/* values read from devices */

/* lighting parameters */
static float amb_three[] = {0.3, 0.3, 0.3, 1.0};

static float light0_position[] = {100.0, 200.0, 100.0, 0.0};
static float light1_position[] = {100.0, 30.0, 100.0, 0.0};
static float light2_position[] = {-100.0, 20.0, 20.0, 0.0};
static float light3_position[] = {0.0, -100.0, -100.0, 0.0};

static float light0_diffuse[] = {0.70, 0.70, 0.70, 1.0}; /* white */
static float light1_diffuse[] = {0.60, 0.10, 0.10, 1.0}; /* red */
static float light2_diffuse[] = {0.10, 0.30, 0.10, 1.0}; /* green */
static float light3_diffuse[] = {0.10, 0.10, 0.30, 1.0}; /* blue */

/* functions */
static void
refresh_hook()
{
	dmaflag = 1;
}

static void
do_linewidth()
{
	glLineWidth((GLfloat) ((struct ogl_vars *)dm_vars)->mvars.linewidth);
	dmaflag = 1;
}


static void
do_fog()
{
	glHint(GL_FOG_HINT, ((struct ogl_vars *)dm_vars)->mvars.fastfog ? GL_FASTEST : GL_NICEST);
	dmaflag = 1;
}

#define OGL2_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)
struct structparse Ogl2_vparse[] = {
	{"%d",	1, "depthcue",		OGL2_MV_O(cueing_on),	Ogl2_colorchange },
	{"%d",  1, "zclip",		OGL2_MV_O(zclipping_on),	refresh_hook },
	{"%d",  1, "zbuffer",		OGL2_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		OGL2_MV_O(lighting_on),	establish_lighting },
	{"%d",  1, "perspective",       OGL2_MV_O(perspective_mode), establish_perspective },
	{"%d",  1, "set_perspective",   OGL2_MV_O(dummy_perspective),  set_perspective },
	{"%d",  1, "has_zbuf",		OGL2_MV_O(zbuf),	refresh_hook },
	{"%d",  1, "has_rgb",		OGL2_MV_O(rgb),	Ogl2_colorchange },
	{"%d",  1, "has_doublebuffer",	OGL2_MV_O(doublebuffer), refresh_hook },
	{"%d",  1, "depth",		OGL2_MV_O(depth),	FUNC_NULL },
	{"%d",  1, "debug",		OGL2_MV_O(debug),	FUNC_NULL },
	{"%d",  1, "linewidth",		OGL2_MV_O(linewidth),	do_linewidth },
	{"%d",  1, "fastfog",		OGL2_MV_O(fastfog),	do_fog },
	{"%f",  1, "density",		OGL2_MV_O(fogdensity),	refresh_hook },
	{"%d",  1, "virtual_trackball",	OGL2_MV_O(virtual_trackball),FUNC_NULL },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

static int OgldoMotion = 0;

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	(((x)/4096.0+0.5)*((struct ogl_vars *)dm_vars)->width)
#define	GED_TO_Xy(x)	((0.5-(x)/4096.0)*((struct ogl_vars *)dm_vars)->height)

/* get rid of when no longer needed */
#define USE_RAMP	(((struct ogl_vars *)dm_vars)->mvars.cueing_on || ((struct ogl_vars *)dm_vars)->mvars.lighting_on)
#define CMAP_BASE	32
#define CMAP_RAMP_WIDTH	16
#define MAP_ENTRY(x)	( USE_RAMP ? \
			((x) * CMAP_RAMP_WIDTH + CMAP_BASE) : \
			((x) + CMAP_BASE) )

/********************************************************************/

/*
 *  Mouse coordinates are in absolute screen space, not relative to
 *  the window they came from.  Convert to window-relative,
 *  then to MGED-style +/-2048 range.
 */
static int
irisX2ged(x)
register int x;
{
  return ((x/(double)((struct ogl_vars *)dm_vars)->width - 0.5) * 4095);
}

static int
irisY2ged(y)
register int y;
{
  return ((0.5 - y/(double)((struct ogl_vars *)dm_vars)->height) * 4095);
}


/*
 *			O G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
Ogl2_open()
{
  ogl_var_init();

  return Ogl2_setup(dname);
}

/*XXX Just experimenting */
int
Ogl2_load_startup()
{
  FILE    *fp;
  struct rt_vls str;
  char *path;
  char *filename;
  int     found;

/*XXX*/
#define DM_OGL_RCFILE "oglinit.tk"

#if 1
  bzero((void *)&head_ogl_vars, sizeof(struct ogl_vars));
  RT_LIST_INIT( &head_ogl_vars.l );
#endif

  found = 0;
  rt_vls_init( &str );

  if((filename = getenv("DM_OGL_RCFILE")) == (char *)NULL )
    /* Use default file name */
    filename = DM_OGL_RCFILE;

  if((path = getenv("MGED_LIBRARY")) != (char *)NULL ){
    /* Use MGED_LIBRARY path */
    rt_vls_strcpy( &str, path );
    rt_vls_strcat( &str, "/" );
    rt_vls_strcat( &str, filename );

    if ((fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    if( (path = getenv("HOME")) != (char *)NULL )  {
      /* Use HOME path */
      rt_vls_strcpy( &str, path );
      rt_vls_strcat( &str, "/" );
      rt_vls_strcat( &str, filename );

      if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
	found = 1;
    }
  }

  if( !found ) {
    /* Check current directory */
    if( (fp = fopen( filename, "r" )) != NULL )  {
      rt_vls_strcpy( &str, filename );
      found = 1;
    }
  }

  if(!found){
    rt_vls_free(&str);

    /* Using default */
    if(Tcl_Eval( interp, ogl_init_str ) == TCL_ERROR)
      return -1;

    return 0;
  }

  fclose( fp );

  if (Tcl_EvalFile( interp, rt_vls_addr(&str) ) == TCL_ERROR) {
    rt_vls_free(&str);
    return -1;
  }

  rt_vls_free(&str);
  return 0;
}

/*
 *  			O G L _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
Ogl2_close()
{

  if(glxc != NULL){
    glDrawBuffer(GL_FRONT);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    /*	glClearDepth(0.0);*/
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glXDestroyContext(dpy, glxc);
  }

  if(xtkwin != NULL)
    Tk_DestroyWindow(xtkwin);

  if(((struct ogl_vars *)dm_vars)->l.forw != RT_LIST_NULL)
    RT_LIST_DEQUEUE(&((struct ogl_vars *)dm_vars)->l);

  rt_free(dm_vars, "Ogl2_close: dm_vars");

#if 0
	Tk_DeleteGenericHandler(Ogl2_doevent, (ClientData)curr_dm_list);
#else
	if(RT_LIST_IS_EMPTY(&head_ogl_vars.l))
	  Tk_DeleteGenericHandler(Ogl2_doevent, (ClientData)NULL);
#endif
}

/*
 *			O G L _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
Ogl2_prolog()
{
  GLint mm; 
  char i;
  char *str = "a";
  GLfloat fogdepth;

  if (((struct ogl_vars *)dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Ogl2_prolog\n", (char *)NULL);

  if (!glXMakeCurrent(dpy, win, glxc)){
    Tcl_AppendResult(interp, "Ogl2_prolog: Couldn't make context current\n", (char *)NULL);
    return;
  }

#if 0
  Ogl2_configure_window_shape();
#endif

  if (!((struct ogl_vars *)dm_vars)->mvars.doublebuffer){
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /*			return;*/
  }

  if (((struct ogl_vars *)dm_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    ((struct ogl_vars *)dm_vars)->face_flag = 0;
    if (((struct ogl_vars *)dm_vars)->mvars.cueing_on){
      glEnable(GL_FOG);
      fogdepth = 2.2 * Viewscale; /* 2.2 is heuristic */
      glFogf(GL_FOG_END, fogdepth);
      fogdepth = (GLfloat) (0.5*((struct ogl_vars *)dm_vars)->mvars.fogdensity/Viewscale);
      glFogf(GL_FOG_DENSITY, fogdepth);
      glFogi(GL_FOG_MODE, ((struct ogl_vars *)dm_vars)->mvars.perspective_mode ? GL_EXP : GL_LINEAR);
    }
    if (((struct ogl_vars *)dm_vars)->mvars.lighting_on){
      glEnable(GL_LIGHTING);
    }
  }
	
  glLineWidth((GLfloat) ((struct ogl_vars *)dm_vars)->mvars.linewidth);
}

/*
 *			O G L _ E P I L O G
 */
void
Ogl2_epilog()
{
  if (((struct ogl_vars *)dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Ogl2_epilog\n", (char *)NULL);

  /*
   * A Point, in the Center of the Screen.
   * This is drawn last, to always come out on top.
   */

  glColor3ub( (short)ogl_rgbtab[4].r, (short)ogl_rgbtab[4].g, (short)ogl_rgbtab[4].b );
  glBegin(GL_POINTS);
  glVertex2i(0,0);
  glEnd();
  /* end of faceplate */

  if(((struct ogl_vars *)dm_vars)->mvars.doublebuffer ){
    glXSwapBuffers(dpy, win);
    /* give Graphics pipe time to work */
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  /* Prevent lag between events and updates */
  XSync(dpy, 0);

  if(((struct ogl_vars *)dm_vars)->mvars.debug){
    int error;
    struct rt_vls tmp_vls;

    rt_vls_init(&tmp_vls);
    rt_vls_printf(&tmp_vls, "ANY ERRORS?\n");

    while((error = glGetError())!=0){
      rt_vls_printf(&tmp_vls, "Error: %x\n", error);
    }

    Tcl_AppendResult(interp, rt_vls_addr(&tmp_vls), (char *)NULL);
    rt_vls_free(&tmp_vls);
  }

  return;
}

/*
 *  			O G L _ N E W R O T
 *  load new rotation matrix onto top of stack
 */
void
Ogl2_newrot(mat, which_eye)
mat_t mat;
int which_eye;
{
	register fastf_t *mptr;
	GLfloat gtmat[16], view[16];
	GLfloat *gtmatp;
	mat_t	newm;
	int	i;

	
	if (((struct ogl_vars *)dm_vars)->mvars.debug)
	  Tcl_AppendResult(interp, "Ogl2_newrot()\n", (char *)NULL);

	if(((struct ogl_vars *)dm_vars)->mvars.debug){
	  struct rt_vls tmp_vls;

	  rt_vls_init(&tmp_vls);
	  rt_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
	  rt_vls_printf(&tmp_vls, "newrot matrix = \n");
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8],mat[12]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9],mat[13]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10],mat[14]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11],mat[15]);

	  Tcl_AppendResult(interp, rt_vls_addr(&tmp_vls), (char *)NULL);
	  rt_vls_free(&tmp_vls);
	}

	switch(which_eye)  {
	case 0:
		/* Non-stereo */
		break;
	case 1:
		/* R eye */
		glViewport(0,  0, (XMAXSCREEN)+1, ( YSTEREO)+1); 
		glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
		Ogl2_puts( "R", 2020, 0, 0, DM_RED );
		break;
	case 2:
		/* L eye */
		glViewport(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1, ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1); 
		glScissor(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1, ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
		break;
	}

	mptr = mat;

	gtmat[0] = *(mptr++) * aspect;
	gtmat[4] = *(mptr++) * aspect;
	gtmat[8] = *(mptr++) * aspect;
	gtmat[12] = *(mptr++) * aspect;

	gtmat[1] = *(mptr++) * aspect;
	gtmat[5] = *(mptr++) * aspect;
	gtmat[9] = *(mptr++) * aspect;
	gtmat[13] = *(mptr++) * aspect;

	gtmat[2] = *(mptr++);
	gtmat[6] = *(mptr++);
	gtmat[10] = *(mptr++);
	gtmat[14] = *(mptr++);

	gtmat[3] = *(mptr++);
	gtmat[7] = *(mptr++);
	gtmat[11] = *(mptr++);
	gtmat[15] = *(mptr++);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef( 0.0, 0.0, -1.0 );
	glMultMatrixf( gtmat );

	/* Make sure that new matrix is applied to the lights */
	if (((struct ogl_vars *)dm_vars)->mvars.lighting_on ){
		glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
		glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
		glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
		glLightfv(GL_LIGHT3, GL_POSITION, light3_position);

	}
}



/*
 *  			O G L _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */

/* ARGSUSED */
int
Ogl2_object( sp, mat, ratio, white_flag )
register struct solid *sp;
mat_t mat;
double ratio;
int white_flag;
{
	register struct rt_vlist	*vp;
	register int nvec;
	register float	*gtvec;
	register float material[4];
	char	gtbuf[16+3*sizeof(double)];
	int first;
	int i,j;

	if (((struct ogl_vars *)dm_vars)->mvars.debug)
	  Tcl_AppendResult(interp, "ogl2_Object()\n", (char *)NULL);

	/*
	 *  It is claimed that the "dancing vector disease" of the
	 *  4D GT processors is due to the array being passed to v3f()
	 *  not being quad-word aligned (16-byte boundary).
	 *  This hack ensures that the buffer has this alignment.
	 *  Note that this requires gtbuf to be 16 bytes longer than needed.
	 */
	gtvec = (float *)((((int)gtbuf)+15) & (~0xF));

	if (sp->s_soldash)
		glEnable(GL_LINE_STIPPLE);		/* set dot-dash */

	if (white_flag && ((struct ogl_vars *)dm_vars)->mvars.cueing_on)
		glDisable(GL_FOG);	

	if( ((struct ogl_vars *)dm_vars)->mvars.rgb )  {
		register short	r, g, b;
		if( white_flag )  {
			r = g = b = 230;
		} else {
			r = (short)sp->s_color[0];
			g = (short)sp->s_color[1];
			b = (short)sp->s_color[2];
		}

		if(((struct ogl_vars *)dm_vars)->mvars.lighting_on)
		{

			/* Ambient = .2, Diffuse = .6, Specular = .2 */

			material[0] = 	.2 * ( r / 255.0);
			material[1] = 	.2 * ( g / 255.0);
			material[2] = 	.2 * ( b / 255.0);
			material[3] = 1.0;
			glMaterialfv(GL_FRONT, GL_AMBIENT, material);
			glMaterialfv(GL_FRONT, GL_SPECULAR, material);

			material[0] *= 3.0;
			material[1] *= 3.0;
			material[2] *= 3.0;
			glMaterialfv(GL_FRONT, GL_DIFFUSE, material);

		} else {
			glColor3ub( r,  g,  b );
		}
	} else {
		if (white_flag){
			ovec = MAP_ENTRY(DM_WHITE);
			glIndexi(ovec);
		} else if( (nvec = MAP_ENTRY( sp->s_dmindex )) != ovec) {
			glIndexi(nvec);
			ovec = nvec;
		}

		if (((struct ogl_vars *)dm_vars)->mvars.lighting_on){
			material[0] = ovec - CMAP_RAMP_WIDTH + 2;
			material[1] = ovec - CMAP_RAMP_WIDTH/2;
			material[2] = ovec - 1;
			glMaterialfv(GL_FRONT, GL_COLOR_INDEXES, material);
		}
	}


	/* Viewing region is from -1.0 to +1.0 */
	first = 1;
	for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		register int	i;
		register int	nused = vp->nused;
		register int	*cmd = vp->cmd;
		register point_t *pt = vp->pt;
		for( i = 0; i < nused; i++,cmd++,pt++ )  {
			switch( *cmd )  {
			case RT_VLIST_LINE_MOVE:
				/* Move, start line */
				if( first == 0 )
					glEnd();
				first = 0;
				glBegin(GL_LINE_STRIP);
				glVertex3dv( *pt );
				break;
			case RT_VLIST_LINE_DRAW:
				/* Draw line */
				glVertex3dv( *pt );
				break;
			case RT_VLIST_POLY_START:
				/* Start poly marker & normal */
				if( first == 0 )
					glEnd();
				glBegin(GL_POLYGON);
				/* Set surface normal (vl_pnt points outward) */
				VMOVE( gtvec, *pt );
				glNormal3fv(gtvec);
				break;
			case RT_VLIST_POLY_MOVE:
				/* Polygon Move */
				glVertex3dv( *pt );
				break;
			case RT_VLIST_POLY_DRAW:
				/* Polygon Draw */
				glVertex3dv( *pt );
				break;
			case RT_VLIST_POLY_END:
				/* Draw, End Polygon */
				glVertex3dv( *pt );
				glEnd();
				first = 1;
				break;
			case RT_VLIST_POLY_VERTNORM:
				/* Set per-vertex normal.  Given before vert. */
				VMOVE( gtvec, *pt );
				glNormal3fv(gtvec);
				break;
			}
		}
	}
	if( first == 0 ) glEnd();

	if (sp->s_soldash)
		glDisable(GL_LINE_STIPPLE);	/* restore solid lines */

	if (white_flag && ((struct ogl_vars *)dm_vars)->mvars.cueing_on){
		glEnable(GL_FOG);
	}

	return(1);	/* OK */

}

/*
 *			O G L _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
Ogl2_normal()
{
	GLint mm; 

	if (((struct ogl_vars *)dm_vars)->mvars.debug)
	  Tcl_AppendResult(interp, "Ogl2_normal\n", (char *)NULL);

	if( ((struct ogl_vars *)dm_vars)->mvars.rgb )  {
		glColor3ub( 0,  0,  0 );
	} else {
		ovec = MAP_ENTRY(DM_BLACK);
		glIndexi( ovec );
	}

	if (!((struct ogl_vars *)dm_vars)->face_flag){
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrixd( ((struct ogl_vars *)dm_vars)->faceplate_mat );
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		((struct ogl_vars *)dm_vars)->face_flag = 1;
		if(((struct ogl_vars *)dm_vars)->mvars.cueing_on)
			glDisable(GL_FOG);
		if (((struct ogl_vars *)dm_vars)->mvars.lighting_on)
			glDisable(GL_LIGHTING);

	}

	return;
}

/*
 *			O G L _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
Ogl2_update()
{
  if (((struct ogl_vars *)dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Ogl2_update()\n", (char *)NULL);

    XFlush(dpy);
}


/*
 *			XOGL _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
void
Ogl2_puts( str, x, y, size, colour )
register char *str;
int x,y,size, colour;
{
	if (((struct ogl_vars *)dm_vars)->mvars.debug)
	  Tcl_AppendResult(interp, "Ogl2_puts()\n", (char *)NULL);

	
/*	glRasterPos2f( GED2IRIS(x),  GED2IRIS(y));*/
	if( ((struct ogl_vars *)dm_vars)->mvars.rgb )  {
		glColor3ub( (short)ogl_rgbtab[colour].r,  (short)ogl_rgbtab[colour].g,  (short)ogl_rgbtab[colour].b );
	} else {
		ovec = MAP_ENTRY(colour);
		glIndexi( ovec );
	}


/*	glRasterPos2i( x,  y);*/
	glRasterPos2f( GED2IRIS(x),  GED2IRIS(y));
	glListBase(fontOffset);
	glCallLists(strlen( str ), GL_UNSIGNED_BYTE,  str );
}


/*
 *			O G L _ 2 D _ L I N E
 *
 */
void
Ogl2_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	register int nvec;

	if (((struct ogl_vars *)dm_vars)->mvars.debug)
	  Tcl_AppendResult(interp, "Ogl2_2d_line()\n", (char *)NULL);

	if( ((struct ogl_vars *)dm_vars)->mvars.rgb )  {
		/* Yellow */

		glColor3ub( (short)255,  (short)255,  (short) 0 );
	} else {
		if((nvec = MAP_ENTRY(DM_YELLOW)) != ovec) {
			glIndexi(nvec);
			ovec = nvec;
		}
	}
	
/*	glColor3ub( (short)255,  (short)255,  (short) 0 );*/

	if(((struct ogl_vars *)dm_vars)->mvars.debug){
	  GLfloat pmat[16];
	  struct rt_vls tmp_vls;

	  rt_vls_init(&tmp_vls);
	  glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	  rt_vls_printf(&tmp_vls, "projection matrix:\n");
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);
	  glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	  rt_vls_printf(&tmp_vls, "modelview matrix:\n");
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
	  rt_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);

	  Tcl_AppendResult(interp, rt_vls_addr(&tmp_vls), (char *)NULL);
	  rt_vls_free(&tmp_vls);
	}

	if( dashed )
		glEnable(GL_LINE_STIPPLE);
		
	glBegin(GL_LINES); 
	 glVertex2f( GED2IRIS(x1),  GED2IRIS(y1));
	 glVertex2f( GED2IRIS(x2),  GED2IRIS(y2));
	glEnd();

	if( dashed )		/* restore solid */
		glDisable(GL_LINE_STIPPLE); 


}

#define Ogl2_NUM_SLID	7
#define Ogl2_XSLEW	0
#define Ogl2_YSLEW	1
#define Ogl2_ZSLEW	2
#define Ogl2_ZOOM	3
#define Ogl2_XROT	4
#define Ogl2_YROT	5
#define Ogl2_ZROT	6

static int
Ogl2_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  static int button0  = 0;   /*  State of button 0 */
  static int knobs_during_help[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  register struct dm_list *save_dm_list;
  register struct dm_list *p;
  struct rt_vls cmd;
  int status = CMD_OK;

  rt_vls_init(&cmd);
  save_dm_list = curr_dm_list;

  curr_dm_list = get_dm_list(eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  if(mged_variables.send_key && eventPtr->type == KeyPress){
    char buffer[1];
    KeySym keysym;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables.hot_key)
      goto end;

    write(dm_pipe[1], buffer, 1);
    rt_vls_free(&cmd);
    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  if ( eventPtr->type == Expose && eventPtr->xexpose.count == 0 ) {
#if 0
    Ogl2_configure_window_shape();
#else
    glClearColor(0.0, 0.0, 0.0, 0.0);
    if (((struct ogl_vars *)dm_vars)->mvars.zbuf)
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
       glClear(GL_COLOR_BUFFER_BIT);
#endif

    dirty = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == ConfigureNotify ) {
    Ogl2_configure_window_shape();

    dirty = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    if( !OgldoMotion &&
	(VIRTUAL_TRACKBALL_NOT_ACTIVE(struct ogl_vars *, mvars.virtual_trackball)) )
      goto end;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(((struct ogl_vars *)dm_vars)->mvars.virtual_trackball){
    case VIRTUAL_TRACKBALL_OFF:
    case VIRTUAL_TRACKBALL_ON:
      /* do the regular thing */
      /* Constant tracking (e.g. illuminate mode) bound to M mouse */
      rt_vls_printf( &cmd, "M 0 %d %d\n",
		     (mx/(double)((struct ogl_vars *)dm_vars)->width - 0.5) * 4095,
		     (0.5 - my/(double)((struct ogl_vars *)dm_vars)->height) * 4095);
      break;
    case VIRTUAL_TRACKBALL_ROTATE:
      rt_vls_printf( &cmd, "irot %f %f 0\n", (my - omy)/2.0,
		     (mx - omx)/2.0);
      break;
    case VIRTUAL_TRACKBALL_TRANSLATE:
      rt_vls_printf( &cmd, "tran %f %f %f\n",
		     (mx/(double)((struct ogl_vars *)dm_vars)->width - 0.5) * 2,
		     (0.5 - my/(double)((struct ogl_vars *)dm_vars)->height) * 2, tran_z);
      break;
    case VIRTUAL_TRACKBALL_ZOOM:
      rt_vls_printf( &cmd, "zoom %lf\n",
		     (omy - my)/(double)((struct ogl_vars *)dm_vars)->height + 1.0);
      break;
    }

    omx = mx;
    omy = my;
  }else if( eventPtr->type == devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      ogl_dbtext(
		(adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto end;
    }

    knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
    setting = irlimit(knobs[M->first_axis]);
    knob_values[M->first_axis] = M->axis_data[0];

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(adcflag) {
	rt_vls_printf( &cmd, "knob ang1 %d\n",
		      setting );
      }
      break;
    case DIAL1:
      rt_vls_printf( &cmd , "knob S %f\n",
		    setting / 2048.0 );
      break;
    case DIAL2:
      if(adcflag)
	rt_vls_printf( &cmd , "knob ang2 %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob z %f\n",
		      setting / 2048.0 );
      break;
    case DIAL3:
      if(adcflag)
	rt_vls_printf( &cmd , "knob distadc %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob Z %f\n",
		      setting / 2048.0 );
      break;
    case DIAL4:
      if(adcflag)
	rt_vls_printf( &cmd , "knob yadc %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob y %f\n",
		      setting / 2048.0 );
      break;
    case DIAL5:
      rt_vls_printf( &cmd , "knob Y %f\n",
		    setting / 2048.0 );
      break;
    case DIAL6:
      if(adcflag)
	rt_vls_printf( &cmd , "knob xadc %d\n",
		      setting );
      else
	rt_vls_printf( &cmd , "knob x %f\n",
		      setting / 2048.0 );
      break;
    case DIAL7:
      rt_vls_printf( &cmd , "knob X %f\n",
		    setting / 2048.0 );
      break;
    default:
      break;
    }

  }else if( eventPtr->type == devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto end;
    }

    if(button0){
      ogl_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      rt_vls_strcat(&cmd, "knob zero\n");
      set_knob_offset();
    }else
      rt_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1)
      button0 = 0;

    goto end;
  }else
    goto end;

  status = cmdline(&cmd, FALSE);
end:
  rt_vls_free(&cmd);
  curr_dm_list = save_dm_list;

  if(status == CMD_OK)
    return TCL_OK;

  return TCL_ERROR;
}
	    
/*
 *			O G L _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 *
 * DEPRECATED 
 *
 */
/* ARGSUSED */
void
Ogl2_input( input, noblock )
fd_set		*input;
int		noblock;
{
    return;
}

/* 
 *			O G L _ L I G H T
 */
/* ARGSUSED */
void
Ogl2_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
Ogl2_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
Ogl2_load( addr, count )
unsigned addr, count;
{
  rt_log("Ogl2_load(x%x, %d.)\n", addr, count );
  return( 0 );
}

void
Ogl2_statechange( a, b )
int	a, b;
{
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
	switch( b )  {
	case ST_VIEW:
	    /* constant tracking OFF */
	    OgldoMotion = 0;
	    break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	    /* constant tracking ON */
	    OgldoMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    OgldoMotion = 0;
	    break;
	default:
	  Tcl_AppendResult(interp, "Ogl2_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
	  break;
	}

	/*Ogl2_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
}

void
Ogl2_viewchange()
{
}

void
Ogl2_colorchange()
{
	register int i;
	register int nramp;
	XColor celltest;
	int count = 0;
	Colormap a_cmap;

	if( ((struct ogl_vars *)dm_vars)->mvars.debug )
	  Tcl_AppendResult(interp, "colorchange\n", (char *)NULL);

	/* Program the builtin colors */
	ogl_rgbtab[0].r=0; 
	ogl_rgbtab[0].g=0; 
	ogl_rgbtab[0].b=0;/* Black */
	ogl_rgbtab[1].r=255; 
	ogl_rgbtab[1].g=0; 
	ogl_rgbtab[1].b=0;/* Red */
	ogl_rgbtab[2].r=0; 
	ogl_rgbtab[2].g=0; 
	ogl_rgbtab[2].b=255;/* Blue */
	ogl_rgbtab[3].r=255; 
	ogl_rgbtab[3].g=255;
	ogl_rgbtab[3].b=0;/*Yellow */
	ogl_rgbtab[4].r = ogl_rgbtab[4].g = ogl_rgbtab[4].b = 255; /* White */
	slotsused = 5;

	if( ((struct ogl_vars *)dm_vars)->mvars.rgb )  {
		if(((struct ogl_vars *)dm_vars)->mvars.cueing_on) {
			glEnable(GL_FOG);
		} else {
			glDisable(GL_FOG);
		}

		glColor3ub( (short)255,  (short)255,  (short)255 );

		/* apply region-id based colors to the solid table */
		color_soltab();

		return;
	}

	if(USE_RAMP && (ogl_index_size < 7)) {
	  Tcl_AppendResult(interp, "Too few bitplanes: depthcueing and lighting disabled\n",
			   (char *)NULL);
	  ((struct ogl_vars *)dm_vars)->mvars.cueing_on = 0;
	  ((struct ogl_vars *)dm_vars)->mvars.lighting_on = 0;
	}
	/* number of slots is 2^indexsize */
	ogl_nslots = 1<<ogl_index_size;
	if( ogl_nslots > NSLOTS )  ogl_nslots = NSLOTS;
	if(USE_RAMP) {
		/* peel off reserved ones */
		ogl_nslots = (ogl_nslots - CMAP_BASE) / CMAP_RAMP_WIDTH;
	} else {
		ogl_nslots -= CMAP_BASE;	/* peel off the reserved entries */
	}

	ovec = -1;	/* Invalidate the old colormap entry */

	/* apply region-id based colors to the solid table */
	color_soltab();

	/* best to do this before the colorit */
	if (((struct ogl_vars *)dm_vars)->mvars.cueing_on && ((struct ogl_vars *)dm_vars)->mvars.lighting_on){
		((struct ogl_vars *)dm_vars)->mvars.lighting_on = 0;
		glDisable(GL_LIGHTING);
	}

	/* Map the colors in the solid table to colormap indices */
	Ogl2_colorit();

	for( i=0; i < slotsused; i++ )  {
		Ogl2_gen_color( i, ogl_rgbtab[i].r, ogl_rgbtab[i].g, ogl_rgbtab[i].b);
	}

	/* best to do this after the colorit */
	if (((struct ogl_vars *)dm_vars)->mvars.cueing_on){
		glEnable(GL_FOG);
	} else {
		glDisable(GL_FOG);
	}

	ovec = MAP_ENTRY(DM_WHITE);
	glIndexi( ovec );

}

/* ARGSUSED */
void
Ogl2_debug(lvl)
{
  ((struct ogl_vars *)dm_vars)->mvars.debug = lvl;
  XFlush(dpy);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);
}

void
Ogl2_window(w)
register int w[];
{
}

/* the font used depends on the size of the window opened */
#define FONTBACK	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5	"5x7"
#define FONT6	"6x10"
#define FONT7	"7x13"
#define FONT8	"8x13"
#define FONT9	"9x15"

static int
Ogl2_setup( name )
char	*name;
{
  static count = 0;
  char *cp, symbol;
  XGCValues gcv;
  XColor a_color;
  Visual *a_visual;
  int a_screen, num, i, success;
  int major, minor;
  Colormap  a_cmap;
  XVisualInfo *vip;
  int dsize, use, dbfr, rgba, red, blue, green, alpha, index;
  GLfloat backgnd[4];
  long supplied;
  int j, k;
  int ndevices;
  int nclass = 0;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  struct rt_vls str;
  Display *tmp_dpy;

  rt_vls_init(&str);

  /* Only need to do this once */
  if(tkwin == NULL){
    rt_vls_printf(&str, "loadtk %s\n", name);

    if(cmdline(&str, FALSE) == CMD_BAD){
      rt_vls_free(&str);
      return -1;
    }
  }

  /* Only need to do this once for this display manager */
  if(!count){
    if( Ogl2_load_startup() ){
      rt_vls_free(&str);
      return -1;
    }
  }

  if(RT_LIST_IS_EMPTY(&head_ogl_vars.l))
    Tk_CreateGenericHandler(Ogl2_doevent, (ClientData)NULL);

  RT_LIST_APPEND(&head_ogl_vars.l, &((struct ogl_vars *)curr_dm_list->_dm_vars)->l);

  rt_vls_printf(&pathName, ".dm_ogl%d", count++);

  /* this is important so that Ogl2_configure_notify knows to set
   * the font */
  fontstruct = NULL;

  if((tmp_dpy = XOpenDisplay(name)) == NULL){
    rt_vls_free(&str);
    return -1;
  }

  ((struct ogl_vars *)dm_vars)->width = DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;
  ((struct ogl_vars *)dm_vars)->height = DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;

  /* Make window square */
  if(((struct ogl_vars *)dm_vars)->height < ((struct ogl_vars *)dm_vars)->width)
    ((struct ogl_vars *)dm_vars)->width = ((struct ogl_vars *)dm_vars)->height;
  else
    ((struct ogl_vars *)dm_vars)->height = ((struct ogl_vars *)dm_vars)->width;

  XCloseDisplay(tmp_dpy);

  /* Make xtkwin a toplevel window */
  xtkwin = Tk_CreateWindowFromPath(interp, tkwin, rt_vls_addr(&pathName), name);

  /* Open the display - XXX see what NULL does now */
  if( xtkwin == NULL ) {
    Tcl_AppendResult(interp, "dm-ogl2: Failed to open ", rt_vls_addr(&pathName),
		     "\n", (char *)NULL);
    return -1;
  }

  rt_vls_strcpy(&str, "init_ogl ");
  rt_vls_printf(&str, "%s\n", rt_vls_addr(&pathName));

  if(cmdline(&str, FALSE) == CMD_BAD){
    rt_vls_free(&str);
    return -1;
  }

  dpy = Tk_Display(xtkwin);

  /* must do this before MakeExist */
  if ((vip=Ogl2_set_visual(xtkwin))==NULL){
    Tcl_AppendResult(interp, "Ogl2_open: Can't get an appropriate visual.\n", (char *)NULL);
    return -1;
  }

  Tk_GeometryRequest(xtkwin, ((struct ogl_vars *)dm_vars)->width, ((struct ogl_vars *)dm_vars)->height);
  Tk_MoveToplevelWindow(xtkwin, 1276 - 976, 0);
  Tk_MakeWindowExist(xtkwin);

  win = Tk_WindowId(xtkwin);

  a_screen = Tk_ScreenNumber(xtkwin);

  /* open GLX context */
  /* If the sgi display manager has been used, then we must use
   * an indirect context. Otherwise use direct, since it is usually
   * faster.
   */
  if ((glxc = glXCreateContext(dpy, vip, 0, ogl_sgi_used ? GL_FALSE : GL_TRUE))==NULL) {
    Tcl_AppendResult(interp, "Ogl2_open: couldn't create glXContext.\n", (char *)NULL);
    return -1;
  }
  /* If we used an indirect context, then as far as sgi is concerned,
   * gl hasn't been used.
   */
  ogl_is_direct = (char) glXIsDirect(dpy, glxc);
  Tcl_AppendResult(interp, "Using ", ogl_is_direct ? "a direct" : "an indirect",
		   " OpenGL rendering context.\n", (char *)NULL);
  /* set ogl_ogl_used if the context was ever direct */
  ogl_ogl_used = (ogl_is_direct || ogl_ogl_used);

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  olist = list = (XDeviceInfoPtr) XListInputDevices (dpy, &ndevices);

  /* IRIX 4.0.5 bug workaround */
  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(dpy, list->id)) == (XDevice *)NULL){
	  Tcl_AppendResult(interp, "Glx_open: Couldn't open the dials+buttons\n", (char *)NULL);
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
	  case ButtonClass:
	    DeviceButtonPress(dev, devbuttonpress, e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, devbuttonrelease, e_class[nclass]);
	    ++nclass;
	    break;
	  case ValuatorClass:
	    DeviceMotionNotify(dev, devmotionnotify, e_class[nclass]);
	    ++nclass;
	    break;
	  default:
	    break;
	  }
	}

	XSelectExtensionEvent(dpy, win, e_class, nclass);
	goto Done;
      }
    }
  }
Done:
  XFreeDeviceList(olist);

#if 0
  /* Register the file descriptor with the Tk event handler */
  Tk_CreateGenericHandler(Ogl2_doevent, (ClientData)curr_dm_list);
#endif

#if 0
  Tk_SetWindowBackground(xtkwin, bg);
#endif

  if (!glXMakeCurrent(dpy, win, glxc)){
    Tcl_AppendResult(interp, "Ogl2_open: Couldn't make context current\n", (char *)NULL);
    return -1;
  }

  /* display list (fontOffset + char) will displays a given ASCII char */
  if ((fontOffset = glGenLists(128))==0){
    Tcl_AppendResult(interp, "dm-ogl: Can't make display lists for font.\n", (char *)NULL);
    return -1;
  }

  Tk_MapWindow(xtkwin);

  /* do viewport, ortho commands and initialize font*/
  Ogl2_configure_window_shape();

  /* Lines will be solid when stippling disabled, dashed when enabled*/
  glLineStipple( 1, 0xCF33);
  glDisable(GL_LINE_STIPPLE);

  backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
  glFogi(GL_FOG_MODE, GL_LINEAR);
  glFogf(GL_FOG_START, 0.0);
  glFogf(GL_FOG_END, 2.0);
  if (((struct ogl_vars *)dm_vars)->mvars.rgb)
    glFogfv(GL_FOG_COLOR, backgnd);
  else
    glFogi(GL_FOG_INDEX, CMAP_RAMP_WIDTH - 1);
  glFogf(GL_FOG_DENSITY, VIEWFACTOR);
	

  /* Initialize matrices */
  /* Leave it in model_view mode normally */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 2.0);
  glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dm_vars)->faceplate_mat);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity(); 
  glTranslatef( 0.0, 0.0, -1.0); 
  glPushMatrix();
  glLoadIdentity();
  ((struct ogl_vars *)dm_vars)->face_flag = 1;	/* faceplate matrix is on top of stack */
		
  return 0;
}

/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
XVisualInfo *
Ogl2_set_visual(tkwin)
Tk_Window tkwin;
{
	XVisualInfo *vip, vitemp, *vibase, *maxvip;
	int good[40];
	int num, i, j;
	int use, rgba, dbfr, stereo;
	int m_stereo, m_double, m_rgba;
	int tries, baddepth;

	/* m_stereo - try to get stereo 
	 * m_double - try to get double buffered
	 * m_rgba  - try to get rgb
	 */

	if( mged_variables.eye_sep_dist )  {
		m_stereo = 1;
	} else {
		m_stereo = 0;
	}

	m_double = 1;
	m_rgba = 1;
	
	/* Try to satisfy the above desires with a color visual of the
	 * greatest depth */

	vibase = XGetVisualInfo(dpy, 0,	&vitemp, &num);

	while (1) {
		for (i=0, j=0, vip=vibase; i<num; i++, vip++){
			/* requirements */
			glXGetConfig(dpy,vip, GLX_USE_GL, &use);
			if (!use)
				continue;
			/* desires */
			if (m_rgba){
				glXGetConfig(dpy, vip, GLX_RGBA, &rgba);
				if (!rgba)
					continue;
			} else if (vip->class != PseudoColor) {
				/* if index mode, accept only read/write*/
				continue;
			}
			if ( m_stereo ) {
				glXGetConfig(dpy, vip, GLX_STEREO, &stereo);
				if (!stereo)
					continue;
			}
			if (m_double ) {
				glXGetConfig(dpy, vip, GLX_DOUBLEBUFFER,&dbfr);
				if (!dbfr)
					continue;
			}
			
			/* this visual meets criteria */
			good[j++] = i;
		}

		/* j = number of acceptable visuals under consideration */
		if (j >= 1){
			baddepth = 1000;
			tries = 0;
			while (tries < j) {
				maxvip = vibase + good[0];
				for (i=1; i<j; i++) {
					vip = vibase + good[i];
					if ((vip->depth > maxvip->depth)&&(vip->depth < baddepth)){
						maxvip = vip;
					}
				}

				/* make sure Tk handles it */
				if (maxvip->class == PseudoColor)
					((struct ogl_vars *)dm_vars)->cmap = XCreateColormap(dpy,
						RootWindow(dpy, maxvip->screen),
						maxvip->visual, AllocAll);
				else
					((struct ogl_vars *)dm_vars)->cmap = XCreateColormap(dpy,
						RootWindow(dpy, maxvip->screen),
						maxvip->visual, AllocNone);

				if (Tk_SetWindowVisual(tkwin, maxvip->visual, maxvip->depth, ((struct ogl_vars *)dm_vars)->cmap)){
					((struct ogl_vars *)dm_vars)->mvars.doublebuffer = m_double;
					glXGetConfig(dpy, maxvip, GLX_DEPTH_SIZE, &((struct ogl_vars *)dm_vars)->mvars.depth);
					if (((struct ogl_vars *)dm_vars)->mvars.depth > 0)
						((struct ogl_vars *)dm_vars)->mvars.zbuf = 1;
					((struct ogl_vars *)dm_vars)->mvars.rgb = m_rgba;
					if (!m_rgba){
						glXGetConfig(dpy, maxvip, GLX_BUFFER_SIZE, &ogl_index_size);
					}
					stereo_is_on = m_stereo;
					return (maxvip); /* sucess */
				} else { 
					/* retry with lesser depth */
					baddepth = maxvip->depth;
					tries ++;
					XFreeColormap(dpy,((struct ogl_vars *)dm_vars)->cmap);
				}
			}
					
		}
				

		/* if no success at this point, relax a desire and try again */
		if ( m_stereo ){
		  m_stereo = 0;
		  Tcl_AppendResult(interp, "Stereo not available.\n", (char *)NULL);
		} else if (m_rgba) {
		  m_rgba = 0;
		  Tcl_AppendResult(interp, "RGBA not available.\n", (char *)NULL);
		} else if (m_double) {
		  m_double = 0;
		  Tcl_AppendResult(interp, "Doublebuffering not available. \n", (char *)NULL);
		} else {
			return(NULL); /* failure */
		}
	}
}

/* 
 *			O G L _ C O N F I G U R E _ W I N D O W _ S H A P E
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 *
 * also change font size if necessary
 */
void
Ogl2_configure_window_shape()
{
	int		npix;
	GLint mm; 
	XWindowAttributes xwa;
	XFontStruct	*newfontstruct;

	XGetWindowAttributes( dpy, win, &xwa );
	((struct ogl_vars *)dm_vars)->height = xwa.height;
	((struct ogl_vars *)dm_vars)->width = xwa.width;
	
	glViewport(0,  0, (((struct ogl_vars *)dm_vars)->width), (((struct ogl_vars *)dm_vars)->height));
	glScissor(0,  0, (((struct ogl_vars *)dm_vars)->width)+1, (((struct ogl_vars *)dm_vars)->height)+1);

	if( ((struct ogl_vars *)dm_vars)->mvars.zbuffer_on )
	  establish_zbuffer();

	establish_lighting();

#if 0
	glDrawBuffer(GL_FRONT_AND_BACK);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	if (((struct ogl_vars *)dm_vars)->mvars.zbuffer_on)
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		glClear( GL_COLOR_BUFFER_BIT);

	if (((struct ogl_vars *)dm_vars)->mvars.doublebuffer)
		glDrawBuffer(GL_BACK);
	else
		glDrawBuffer(GL_FRONT);

	/*CJXX*/
	glFlush();
#endif
	/*CJXX this might cause problems in perspective mode? */
	glGetIntegerv(GL_MATRIX_MODE, &mm);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho( -xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0 );
	glMatrixMode(mm);
	aspect = (fastf_t)((struct ogl_vars *)dm_vars)->height/
	  (fastf_t)((struct ogl_vars *)dm_vars)->width;


	/* First time through, load a font or quit */
	if (fontstruct == NULL) {
	  if ((fontstruct = XLoadQueryFont(dpy, FONT9)) == NULL ) {
	    /* Try hardcoded backup font */
	    if ((fontstruct = XLoadQueryFont(dpy, FONTBACK)) == NULL) {
	      Tcl_AppendResult(interp, "dm-ogl2: Can't open font '", FONT9,
			       "' or '", FONTBACK, "'\n", (char *)NULL);
	      return;
	    }
	  }
	  glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
	}
		

	/* Always try to choose a the font that best fits the window size.
	 */

	if (((struct ogl_vars *)dm_vars)->width < 582) {
		if (fontstruct->per_char->width != 5) {
			if ((newfontstruct = XLoadQueryFont(dpy, FONT5)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else if (((struct ogl_vars *)dm_vars)->width < 679) {
		if (fontstruct->per_char->width != 6){
			if ((newfontstruct = XLoadQueryFont(dpy, FONT6)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else if (((struct ogl_vars *)dm_vars)->width < 776) {
		if (fontstruct->per_char->width != 7){
			if ((newfontstruct = XLoadQueryFont(dpy, FONT7)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else if (((struct ogl_vars *)dm_vars)->width < 873) {
		if (fontstruct->per_char->width != 8){
			if ((newfontstruct = XLoadQueryFont(dpy, FONT8)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else {
		if (fontstruct->per_char->width != 9){
			if ((newfontstruct = XLoadQueryFont(dpy, FONT9)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	}

	
}

/*
 *			O G L _ D M
 * 
 *  Implement display-manager specific commands, from MGED "dm" command.
 */
int
Ogl2_dm(argc, argv)
int	argc;
char	**argv;
{
  struct rt_vls	vls;
  int status;
  char *av[4];
  char xstr[32];
  char ystr[32];
  char zstr[32];

  if( !strcmp( argv[0], "set" ) )  {
    struct rt_vls tmp_vls;

    rt_vls_init(&vls);
    rt_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      rt_structprint("dm_ogl internal variables", Ogl2_vparse, (CONST char *)&((struct ogl_vars *)dm_vars)->mvars );
      rt_log("%s", rt_vls_addr(&vls) );
    } else if( argc == 2 ) {
      rt_vls_name_print( &vls, Ogl2_vparse, argv[1], (CONST char *)&((struct ogl_vars *)dm_vars)->mvars );
      rt_log( "%s\n", rt_vls_addr(&vls) );
    } else {
      rt_vls_printf( &vls, "%s=\"", argv[1] );
      rt_vls_from_argv( &vls, argc-2, argv+2 );
      rt_vls_putc( &vls, '\"' );
      rt_structparse( &vls, Ogl2_vparse, (char *)&((struct ogl_vars *)dm_vars)->mvars );
    }

    rt_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, rt_vls_addr(&tmp_vls), (char *)NULL);
    rt_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "mouse" )){
    if( argc < 4){
      Tcl_AppendResult(interp, "dm: need more parameters\n",
		       "mouse 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

#if 0
    sprintf(xstr, "%d", irisX2ged(atoi(argv[2])));
    sprintf(ystr, "%d", irisY2ged(atoi(argv[3])));

    av[0] = "M";
    av[1] = argv[1];
    av[2] = xstr;
    av[3] = ystr;
    return f_mouse((ClientData)NULL, interp, 4, av);
#else
    rt_vls_init(&vls);
    rt_vls_printf(&vls, "M %s %d %d\n", argv[1],
		  irisX2ged(atoi(argv[2])), irisY2ged(atoi(argv[3])));
    status = cmdline(&vls, FALSE);
    rt_vls_free(&vls);

    if(status == CMD_OK)
      return TCL_OK;

    return TCL_ERROR;
#endif
  }

  status = TCL_OK;
  if(((struct ogl_vars *)dm_vars)->mvars.virtual_trackball){
    if( !strcmp( argv[0], "vtb" )){
      int buttonpress;

      if( argc < 5){
	Tcl_AppendResult(interp, "dm: need more parameters\n",
			 "vtb <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }

      buttonpress = atoi(argv[2]);
      omx = atoi(argv[3]);
      omy = atoi(argv[4]);

      if(buttonpress){
	switch(*argv[1]){
	case 'r':
	  ((struct ogl_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_ROTATE;
	  break;
	case 't':
	  ((struct ogl_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_TRANSLATE;

	  sprintf(xstr, "%f", (omx/(double)((struct ogl_vars *)dm_vars)->width - 0.5) * 2);
	  sprintf(ystr, "%f", (0.5 - omy/(double)((struct ogl_vars *)dm_vars)->height) * 2);
	  sprintf(zstr, "%f", tran_z);
	  av[0] = "tran";
	  av[1] = xstr;
	  av[2] = ystr;
	  av[3] = zstr;
	  status = f_tran((ClientData)NULL, interp, 4, av);
	  
	  break;
	case 'z':
	  ((struct ogl_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_ZOOM;
	  break;
	default:
	  Tcl_AppendResult(interp, "dm: need more parameters\n",
			   "vtb <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
	  return TCL_ERROR;
	}
      }else{
	((struct ogl_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_ON;
    }

    return status;
    }
  }else{
    return status;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

void	
establish_lighting()
{


	if (!((struct ogl_vars *)dm_vars)->mvars.lighting_on) {
		/* Turn it off */
		glDisable(GL_LIGHTING);
		if (!((struct ogl_vars *)dm_vars)->mvars.rgb)
			Ogl2_colorchange();
	} else {
		/* Turn it on */

		if (!((struct ogl_vars *)dm_vars)->mvars.rgb){
			if (((struct ogl_vars *)dm_vars)->mvars.cueing_on){
				((struct ogl_vars *)dm_vars)->mvars.cueing_on = 0;
				glDisable(GL_FOG);
			} 
			Ogl2_colorchange();
		}

		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

		/* light positions specified in Ogl2_newrot */

		glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
		glLightfv(GL_LIGHT1, GL_SPECULAR, light1_diffuse);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
		glLightfv(GL_LIGHT2, GL_SPECULAR, light2_diffuse);
		glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
		glLightfv(GL_LIGHT3, GL_SPECULAR, light3_diffuse);
		glLightfv(GL_LIGHT3, GL_DIFFUSE, light3_diffuse);

		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT3);
		glEnable(GL_LIGHT2);
	}
	dmaflag = 1;
}	


static void	
establish_zbuffer()
{
	if( ((struct ogl_vars *)dm_vars)->mvars.zbuf == 0 ) {
	  Tcl_AppendResult(interp, "dm-ogl2: This machine has no Zbuffer to enable\n",
			   (char *)NULL);
	  ((struct ogl_vars *)dm_vars)->mvars.zbuffer_on = 0;
	}

	if (((struct ogl_vars *)dm_vars)->mvars.zbuffer_on)  {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}
	dmaflag = 1;
	
	return;
}

static void
establish_perspective()
{
  rt_vls_printf( &dm_values.dv_string,
		 "set perspective %d\n",
		 ((struct ogl_vars *)dm_vars)->mvars.perspective_mode ?
		 perspective_table[perspective_angle] :
		 -1 );
  dmaflag = 1;
}

/*
  This routine will toggle the perspective_angle if the
  dummy_perspective value is 0 or less. Otherwise, the
  perspective_angle is set to the value of (dummy_perspective - 1).
*/
static void
set_perspective()
{
  /* set perspective matrix */
  if(((struct ogl_vars *)dm_vars)->mvars.dummy_perspective > 0)
    perspective_angle = ((struct ogl_vars *)dm_vars)->mvars.dummy_perspective <= 4 ? ((struct ogl_vars *)dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--perspective_angle < 0) /* toggle perspective matrix */
    perspective_angle = 3;

  if(((struct ogl_vars *)dm_vars)->mvars.perspective_mode)
    rt_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct ogl_vars *)dm_vars)->mvars.dummy_perspective = 1;

  dmaflag = 1;
}


static void
establish_vtb()
{
  return;
}

static void
Ogl2_colorit()
{
	register struct solid	*sp;
	register struct rgbtab *rgb;
	register int i;
	register int r,g,b;

	if( ((struct ogl_vars *)dm_vars)->mvars.rgb )  return;

	FOR_ALL_SOLIDS( sp )  {
		r = sp->s_color[0];
		g = sp->s_color[1];
		b = sp->s_color[2];
		if( (r == 255 && g == 255 && b == 255) ||
		    (r == 0 && g == 0 && b == 0) )  {
			sp->s_dmindex = DM_WHITE;
			continue;
		}

		/* First, see if this matches an existing color map entry */
		rgb = ogl_rgbtab;
		for( i = 0; i < slotsused; i++, rgb++ )  {
			if( rgb->r == r && rgb->g == g && rgb->b == b )  {
				sp->s_dmindex = i;
				goto next;
			}
		}

		/* If slots left, create a new color map entry, first-come basis */
		if( slotsused < ogl_nslots )  {
			rgb = &ogl_rgbtab[i=(slotsused++)];
			rgb->r = r;
			rgb->g = g;
			rgb->b = b;
			sp->s_dmindex = i;
			continue;
		}
		sp->s_dmindex = DM_YELLOW;	/* Default color */
next:		
		;
	}
}


#if IR_KNOBS
ogl_dbtext(str)
{
  Tcl_AppendResult(interp, "dm-ogl: You pressed Help key and '",
		   str, "'\n", (char *)NULL);
}
/*
 *			I R L I M I T
 *
 * Because the dials are so sensitive, setting them exactly to
 * zero is very difficult.  This function can be used to extend the
 * location of "zero" into "the dead zone".
 */
static 
int irlimit(i)
int i;
{
	if( i > NOISE )
		return( i-NOISE );
	if( i < -NOISE )
		return( i+NOISE );
	return(0);
}
#endif

/*			G E N _ C O L O R
 *
 *	maps a given color into the appropriate colormap entry
 *	for both depthcued and non-depthcued mode.  In depthcued mode,
 *	Ogl2_gen_color also generates the colormap ramps.  Note that in depthcued
 *	mode, DM_BLACK uses map entry 0, and does not generate a ramp for it.
 *	Non depthcued mode skips the first CMAP_BASE colormap entries.
 *
 *	This routine is not called at all if ((struct ogl_vars *)dm_vars)->mvars.rgb is set.
 */
void
Ogl2_gen_color(c)
int c;
{
	if(USE_RAMP) {

		/*  Not much sense in making a ramp for DM_BLACK.  Besides
		 *  which, doing so, would overwrite the bottom color
		 *  map entries, which is a no-no.
		 */
		if( c != DM_BLACK) {
			register int i, j;
			fastf_t r_inc, g_inc, b_inc;
			fastf_t red, green, blue;
			XColor cells[CMAP_RAMP_WIDTH];

			red = r_inc = ogl_rgbtab[c].r * (256/CMAP_RAMP_WIDTH);
			green = g_inc = ogl_rgbtab[c].g * (256/CMAP_RAMP_WIDTH);
			blue = b_inc = ogl_rgbtab[c].b * (256/CMAP_RAMP_WIDTH);

#if 0
			red = ogl_rgbtab[c].r * 256;
			green = ogl_rgbtab[c].g * 256;
			blue = ogl_rgbtab[c].b * 256;
#endif
			

			if (((struct ogl_vars *)dm_vars)->mvars.cueing_on){
				for(i = 0, j = MAP_ENTRY(c) + CMAP_RAMP_WIDTH - 1; 
					i < CMAP_RAMP_WIDTH;
				    i++, j--, red += r_inc, green += g_inc, blue += b_inc){
				    	cells[i].pixel = j;
				    	cells[i].red = (short)red;
				    	cells[i].green = (short)green;
				    	cells[i].blue = (short)blue;
				    	cells[i].flags = DoRed|DoGreen|DoBlue;
				}
			} else { /* ((struct ogl_vars *)dm_vars)->mvars.lighting_on */ 
				for(i = 0, j = MAP_ENTRY(c) - CMAP_RAMP_WIDTH + 1; 
					i < CMAP_RAMP_WIDTH;
				    i++, j++, red += r_inc, green += g_inc, blue += b_inc){
				    	cells[i].pixel = j;
				    	cells[i].red = (short)red;
				    	cells[i].green = (short)green;
				    	cells[i].blue = (short)blue;
				    	cells[i].flags = DoRed|DoGreen|DoBlue;
				    }
			}
			XStoreColors(dpy, ((struct ogl_vars *)dm_vars)->cmap, cells, CMAP_RAMP_WIDTH);
		}
	} else {
		XColor cell, celltest;

		cell.pixel = c + CMAP_BASE;
		cell.red = ogl_rgbtab[c].r * 256;
		cell.green = ogl_rgbtab[c].g * 256;
		cell.blue = ogl_rgbtab[c].b * 256;
		cell.flags = DoRed|DoGreen|DoBlue;
		XStoreColor(dpy, ((struct ogl_vars *)dm_vars)->cmap, &cell);

	}
}

static void
print_cmap()
{
	int i;
	XColor cell;

	for (i=0; i<112; i++){
		cell.pixel = i;
		XQueryColor(dpy, ((struct ogl_vars *)dm_vars)->cmap, &cell);
		printf("%d  = %d %d %d\n",i,cell.red,cell.green,cell.blue);
	}
}

static void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i){
#if 0
    knobs_offset[i] = knobs[i];
#else
    knobs[i] = 0;
#endif
  }
}

static void
ogl_var_init()
{
  dm_vars = (char *)rt_malloc(sizeof(struct ogl_vars),
					    "ogl_var_init: glx_vars");
  bzero((void *)dm_vars, sizeof(struct ogl_vars));
  devmotionnotify = LASTEvent;
  devbuttonpress = LASTEvent;
  devbuttonrelease = LASTEvent;
  ((struct ogl_vars *)dm_vars)->dm_list = curr_dm_list;
  perspective_angle = 3;
  aspect = 1.0;
  ovec = -1;

  /* initialize the modifiable variables */
  ((struct ogl_vars *)dm_vars)->mvars.cueing_on = 1;          /* Depth cueing flag - for colormap work */
  ((struct ogl_vars *)dm_vars)->mvars.zclipping_on = 1;       /* Z Clipping flag */
  ((struct ogl_vars *)dm_vars)->mvars.zbuffer_on = 1;         /* Hardware Z buffer is on */
  ((struct ogl_vars *)dm_vars)->mvars.linewidth = 1;      /* Line drawing width */
  ((struct ogl_vars *)dm_vars)->mvars.dummy_perspective = 1;
  ((struct ogl_vars *)dm_vars)->mvars.virtual_trackball = 1;
  ((struct ogl_vars *)dm_vars)->mvars.fastfog = 1;
  ((struct ogl_vars *)dm_vars)->mvars.fogdensity = 1.0;
}

static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct ogl_vars *p;

  for( RT_LIST_FOR(p, ogl_vars, &head_ogl_vars.l) ){
    if(window == p->_win){
      if (!glXMakeCurrent(p->_dpy, p->_win, p->_glxc)){
	return DM_LIST_NULL;
      }

      return p->dm_list;
    }
  }

  return DM_LIST_NULL;
}
