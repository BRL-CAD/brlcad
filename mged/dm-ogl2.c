#ifdef MULTI_ATTACH
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

#define CJDEBUG 0

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
#include "./oglinit.h"

#include <GL/glx.h>
#include <GL/gl.h>

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

#ifdef SEND_KEY_DOWN_PIPE
extern int dm_pipe[];
#endif

extern Tcl_Interp *interp;
extern Tk_Window tkwin;

static int	Ogl2_setup();
static void	Ogl2_configure_window_shape();
static int	Ogl2_doevent();
static void	Ogl2_gen_color();
static int      Ogl2_load_startup();
static struct dm_list *get_dm_list()

/* Flags indicating whether the gl and sgi display managers have been
 * attached. 
 * These are necessary to decide whether or not to use direct rendering
 * with gl.
 */
char	ogl2_ogl_used = 0;
char	ogl2_sgi_used = 0;
char	ogl2_is_direct = 0;

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

struct dm dm_ogl = {
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
	"gl", "X Windows with OpenGL graphics",
	0,				/* mem map */
	Ogl2_dm
};


/* ogl stuff */
static int ogl2_index_size;
static Colormap cmap;

static XVisualInfo *Ogl2_set_visual();

static GLdouble faceplate_mat[16];
static int ogl2_face_flag;		/* 1: faceplate matrix on stack */

#define dpy (((struct ogl_vars *)dm_vars)->_dpy)
#define win (((struct ogl_vars *)dm_vars)->_win)
#define xtkwin (((struct ogl_vars *)dm_vars)->_xtkwin)
#define width (((struct ogl_vars *)dm_vars)->_width)
#define height (((struct ogl_vars *)dm_vars)->_height)
#ifdef VIRTUAL_TRACKBALL
#define omx (((struct ogl_vars *)dm_vars)->_omx)
#define omy (((struct ogl_vars *)dm_vars)->_omy)
#endif
#define perspective_angle (((struct ogl_vars *)dm_vars)->_perspective_angle)
#define devmotionnotify (((struct ogl_vars *)dm_vars)->_devmotionnotify)
#define devbuttonpress (((struct ogl_vars *)dm_vars)->_devbuttonpress)
#define devbuttonrelease (((struct ogl_vars *)dm_vars)->_devbuttonrelease)
#define knobs (((struct ogl_vars *)dm_vars)->_knobs)
#define stereo_is_on (((struct ogl_vars *)dm_vars)->_stereo_is_on)
#define aspect (((struct ogl_vars *)dm_vars)->_aspect)
#define _fontstruct (((struct ogl_vars *)dm_vars)->_fontstruct)
#define glxc (((struct ogl_vars *)dm_vars)->_glxc)
#define fontOffset (((struct ogl_vars *)dm_vars)->_fontOffset)
#define mvars (((struct ogl_vars *)dm_vars)->_mvars)

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
  int debug;
  int linewidth;
  int fastfog;
  double fogdensity;
#ifdef VIRTUAL_TRACKBALL
  int virtual_trackball;
#endif
};

struct ogl_vars {
  Display *_dpy;
  Window _win;
  Tk_Window _xtkwin;
  int width;
  int height;
#ifdef VIRTUAL_TRACKBALL
  int _omx, _omy;
#endif
  int _perspective_angle;
  int _devmotionnotify;
  int _devbuttonpress;
  int _devbuttonrelease;
  int _knobs[8];
  int _stereo_is_on;
  fastf_t _aspect;
  XFontStruct *_fontstruct;
  GLXContext _glxc;
  int _fontOffset;
  struct modifiable_ogl_vars _mvars;
}

static int	ogl2_fd;			/* GL file descriptor to select() on */
static CONST char ogl2_title[] = "BRL MGED";
static int perspective_table[] = {
	30, 45, 60, 90 };
static int ovec = -1;		/* Old color map entry number */
static int kblights();
static double	xlim_view = 1.0;	/* args for glOrtho*/
static double	ylim_view = 1.0;

void		ogl2_colorit();

/*
 * SGI Color Map table
 */
#define NSLOTS		4080	/* The mostest possible - may be fewer */
static int ogl2_nslots=0;		/* how many we have, <= NSLOTS */
static int slotsused;		/* how many actually used */
static struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
} ogl2_rgbtab[NSLOTS];

extern struct device_values dm_values;	/* values read from devices */

extern void sl_toggle_scroll();		/* from scroll.c */

static void	establish_lighting();
static void	establish_zbuffer();

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
void print_cmap();


static void
refresh_hook()
{
	dmaflag = 1;
}

static void
do_linewidth()
{
	glLineWidth((GLfloat) ogl2_linewidth);
	dmaflag = 1;
}


static void
do_fog()
{
	glHint(GL_FOG_HINT, mvars.fastfog ? GL_FASTEST : GL_NICEST);
	dmaflag = 1;
}

#define OGL2_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)
structparse ogl2_vparse[] = {
	{"%d",	1, "depthcue",		OGL2_MV_O(cueing_on),	Ogl2_colorchange },
	{"%d",  1, "zclip",		OGL2_MV_O(zclipping_on),	refresh_hook },
	{"%d",  1, "zbuffer",		OGL2_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		OGL2_MV_O(lighting_on),	establish_lighting },
	{"%d",  1, "perspective",       OGL2_MV_O(perspective_mode), establish_perspective },
	{"%d",  1, "set_perspective",OGL2_MV_O(dummy_perspective),  set_perspective },
	{"%d",  1, "has_zbuf",		OGL2_MV_O(zbuf),	establish_zbuffer },
	{"%d",  1, "has_rgb",		OGL2_MV_O(rgb),	Ogl2_colorchange },
	{"%d",  1, "has_doublebuffer",	OGL2_MV_O(doublebuffer), refresh_hook },
	{"%d",  1, "debug",		OGL2_MV_O(debug),	FUNC_NULL },
	{"%d",  1, "linewidth",		OGL2_MV_O(linewidth),	do_linewidth },
	{"%d",  1, "fastfog",		OGL2_MV_O(fastfog),	do_fog },
	{"%d",  1, "density",		OGL2_MV_O(fogdensity),	refresh_hook },
#ifdef VIRTUAL_TRACKBALL
	{"%d",  1, "virtual_trackball",	OGL2_MV_O(virtual_trackball),FUNC_NULL },
#endif
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
#define	GED_TO_Xx(x)	(((x)/4096.0+0.5)*width)
#define	GED_TO_Xy(x)	((0.5-(x)/4096.0)*height)

/* get rid of when no longer needed */
#define USE_RAMP	(mvars.cueing_on || mvars.lighting_on)
#define CMAP_BASE	32
#define CMAP_RAMP_WIDTH	16
#define MAP_ENTRY(x)	( USE_RAMP ? \
			((x) * CMAP_RAMP_WIDTH + CMAP_BASE) : \
			((x) + CMAP_BASE) )

/********************************************************************/



/*
 *			O G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
Ogl2_open()
{
  char	line[82];
  char	hostname[80];
  char	display[82];
  char	*envp;

  Ogl2_var_init();

  /* get or create the default display */
  if( (envp = getenv("DISPLAY")) == NULL ) {
    /* Env not set, use local host */
    gethostname( hostname, 80 );
    hostname[79] = '\0';
    (void)sprintf( display, "%s:0", hostname );
    envp = display;
  }

  rt_log("X Display [%s]? ", envp );
  (void)fgets( line, sizeof(line), stdin );
  line[strlen(line)-1] = '\0';		/* remove newline */
  if( feof(stdin) )  quit();
  if( line[0] != '\0' ) {
    if( Ogl2_setup(line) ) {
      return(1);		/* BAD */
    }
  } else {
    if( Ogl2_setup(envp) ) {
      return(1);	/* BAD */
    }
  }

  return(0);			/* OK */
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
    if(Tcl_Eval( interp, ogl_init_str ) == TCL_ERROR){
      rt_log("Glx_load_startup: Error interpreting glx_init_str.\n");
      rt_log("%s\n", interp->result);
      return -1;
    }

    return 0;
  }

  fclose( fp );

  if (Tcl_EvalFile( interp, rt_vls_addr(&str) ) == TCL_ERROR) {
    rt_log("Error reading %s: %s\n", filename, interp->result);
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
	glDrawBuffer(GL_FRONT);
	glClearColor(0.0, 0.0, 0.0, 0.0);
/*	glClearDepth(0.0);*/
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glXMakeCurrent(dpy, None, NULL);

	glXDestroyContext(dpy, glxc);

	Tk_DeleteGenericHandler(Ogldoevent, (ClientData)NULL);
	Tk_DestroyWindow(xtkwin);
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

	if (mvars.debug)
		rt_log( "Ogl2_prolog\n");

	if (dmaflag) {
		if (!mvars.doublebuffer){
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
/*			return;*/
		}

		if (ogl2_face_flag){
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			ogl2_face_flag = 0;
			if (mvars.cueing_on){
				glEnable(GL_FOG);
				fogdepth = 2.2 * Viewscale; /* 2.2 is heuristic */
				glFogf(GL_FOG_END, fogdepth);
				fogdepth = (GLfloat) (0.5*mvars.fogdensity/Viewscale);
				glFogf(GL_FOG_DENSITY, fogdepth);
				glFogi(GL_FOG_MODE, mvars.perspective_mode ? GL_EXP : GL_LINEAR);
			}
			if (mvars.lighting_on){
				glEnable(GL_LIGHTING);
			}
		}
	
		glLineWidth((GLfloat) ogl2_linewidth);

	}


}

/*
 *			O G L _ E P I L O G
 */
void
Ogl2_epilog()
{
	if (mvars.debug)
		rt_log( "Ogl2_epilog\n");
	/*
	 * A Point, in the Center of the Screen.
	 * This is drawn last, to always come out on top.
	 */

	glColor3ub( (short)ogl2_rgbtab[4].r, (short)ogl2_rgbtab[4].g, (short)ogl2_rgbtab[4].b );
	glBegin(GL_POINTS);
	 glVertex2i(0,0);
	glEnd();
	/* end of faceplate */

	if(mvars.doublebuffer )
	{
		glXSwapBuffers(dpy, win);
		/* give Graphics pipe time to work */
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	}


	/* Prevent lag between events and updates */
	XSync(dpy, 0);

	if(CJDEBUG){
		int error;

		rt_log("ANY ERRORS?\n");
		while(	(error = glGetError())!=0){
			rt_log("Error: %x\n", error);
		}
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


	if(CJDEBUG){
		printf("which eye = %d\t", which_eye);
		printf("newrot matrix = \n");
		printf("%g %g %g %g\n", mat[0], mat[4], mat[8],mat[12]);
		printf("%g %g %g %g\n", mat[1], mat[5], mat[9],mat[13]);
		printf("%g %g %g %g\n", mat[2], mat[6], mat[10],mat[14]);
		printf("%g %g %g %g\n", mat[3], mat[7], mat[11],mat[15]);
	}


	if (mvars.debug)
		rt_log( "Ogl2_newrot()\n");
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


	/* If all the display managers end up doing this maybe it's 
	 * dozoom that has a bug */
	gtmat[2]  = -gtmat[2];
	gtmat[6]  = -gtmat[6];
	gtmat[10]  = -gtmat[10];
	gtmat[14]  = -gtmat[14];

	/* we know that mat = P*T*M
	 *	where 	P = the perspective matrix based on the
	 *			eye at the origin
	 *		T = a translation of one in the -Z direction
	 *		M = model2view matrix
	 *
	 * Therefore P = mat*M'*T'
	 * 
	 * In order for depthcueing and lighting to work correctly, 
	 * P must be stored in the GL_PROJECTION matrix and T*M must
	 * be stored the the GL_MODELVIEW matrix.
	 */
	if ( mvars.perspective_mode){
		float inv_view[16];

		/* disassemble supplied matrix */

		/* convert from row-major to column-major and from double
		 * to float
		 */
		inv_view[0] = view2model[0];
		inv_view[1] = view2model[4];
		inv_view[2] = view2model[8];
		inv_view[3] = view2model[12];
		inv_view[4] = view2model[1];
		inv_view[5] = view2model[5];
		inv_view[6] = view2model[9];
		inv_view[7] = view2model[13];
		inv_view[8] = view2model[2];
		inv_view[9] = view2model[6];
		inv_view[10] = view2model[10];
		inv_view[11] = view2model[14];
		inv_view[12] = view2model[3];
		inv_view[13] = view2model[7];
		inv_view[14] = view2model[11];
		inv_view[15] = view2model[15];

		/* do P = P*T*M*M'*T' = mat*M'*T' (see explanation above) */
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf( gtmat );
		glMultMatrixf( inv_view );
		glTranslatef( 0.0, 0.0, 1.0);

	} else {
		/* Orthographic projection */
		glMatrixMode(	GL_PROJECTION);
		glLoadMatrixd( faceplate_mat);

	}


	/* convert from row-major to column-major and from double
	 * to float
	 */
	view[0] = model2view[0];
	view[1] = model2view[4];
	view[2] = model2view[8];
	view[3] = model2view[12];
	view[4] = model2view[1];
	view[5] = model2view[5];
	view[6] = model2view[9];
	view[7] = model2view[13];
	view[8] = model2view[2];
	view[9] = model2view[6];
	view[10] = model2view[10];
	view[11] = model2view[14];
	view[12] = model2view[3];
	view[13] = model2view[7];
	view[14] = model2view[11];
	view[15] = model2view[15];

	/* do T*M (see above explanation) */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef( 0.0, 0.0, -1.0 );
	glMultMatrixf( view );


	/* Make sure that new matrix is applied to the lights */
	if (mvars.lighting_on ){
		glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
		glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
		glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
		glLightfv(GL_LIGHT3, GL_POSITION, light3_position);

	}


	if(CJDEBUG){
		GLfloat pmat[16];
		int mode;

		glGetIntegerv(GL_MATRIX_MODE, &mode);
		printf("matrix mode %s\n", (mode==GL_MODELVIEW) ? "modelview" : "projection");
		glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
		printf("%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
		printf("%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
		printf("%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
		printf("%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);
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

	if (mvars.debug)
		rt_log( "ogl2_Object()\n");


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

	if (white_flag && mvars.cueing_on)
		glDisable(GL_FOG);	

	if( mvars.rgb )  {
		register short	r, g, b;
		if( white_flag )  {
			r = g = b = 230;
		} else {
			r = (short)sp->s_color[0];
			g = (short)sp->s_color[1];
			b = (short)sp->s_color[2];
		}

		if(mvars.lighting_on)
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

		if (mvars.lighting_on){
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

	if (white_flag && mvars.cueing_on){
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

	if (mvars.debug)
		rt_log( "Ogl2_normal\n");

	if( mvars.rgb )  {
		glColor3ub( 0,  0,  0 );
	} else {
		ovec = MAP_ENTRY(DM_BLACK);
		glIndexi( ovec );
	}

	if (!ogl2_face_flag){
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrixd( faceplate_mat );
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		ogl2_face_flag = 1;
		if(mvars.cueing_on)
			glDisable(GL_FOG);
		if (mvars.lighting_on)
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
	if (mvars.debug)
		rt_log( "Ogl2_update()\n");

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
	if (mvars.debug)
		rt_log( "Ogl2_puts()\n");

	
/*	glRasterPos2f( GED2IRIS(x),  GED2IRIS(y));*/
	if( mvars.rgb )  {
		glColor3ub( (short)ogl2_rgbtab[colour].r,  (short)ogl2_rgbtab[colour].g,  (short)ogl2_rgbtab[colour].b );
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

	if (mvars.debug)
		rt_log( "Ogl2_2d_line()\n");

	if( mvars.rgb )  {
		/* Yellow */

		glColor3ub( (short)255,  (short)255,  (short) 0 );
	} else {
		if((nvec = MAP_ENTRY(DM_YELLOW)) != ovec) {
			glIndexi(nvec);
			ovec = nvec;
		}
	}
	
/*	glColor3ub( (short)255,  (short)255,  (short) 0 );*/

	if(CJDEBUG){
		GLfloat pmat[16];
		glGetFloatv(GL_PROJECTION_MATRIX, pmat);
		printf("projection matrix:");
		printf("%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
		printf("%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
		printf("%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
		printf("%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);
		glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
		printf("modelview matrix:");
		printf("%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
		printf("%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
		printf("%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
		printf("%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);

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

int
Ogldoevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
    KeySym key;
    char keybuf[4];
    int cnt;
	float inc;
    XComposeStatus compose_stat;
	static int ogl2_which_slid = Ogl2_XSLEW;

    if (eventPtr->xany.window != win)
	return 0;

    if (eventPtr->type == Expose ) {
	if (eventPtr->xexpose.count == 0) {
		glClearColor(0.0, 0.0, 0.0, 0.0);
		if (mvars.zbuffer_on)
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear(GL_COLOR_BUFFER_BIT);
	    rt_vls_printf( &dm_values.dv_string, "refresh\n" );
		dmaflag = 1;
	}
    } else if( eventPtr->type == ConfigureNotify) {
    	Ogl2_configure_window_shape();
    } else if( eventPtr->type == MotionNotify ) {
	int	x, y;
	if ( !OgldoMotion )
	    return 1;
	x = (eventPtr->xmotion.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xmotion.y/(double)height) * 4095;
	rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y );
    } else if( eventPtr->type == ButtonPress ) {
	/* There may also be ButtonRelease events */
	int	x, y;
	/* In MGED this is a "penpress" */
	x = (eventPtr->xbutton.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xbutton.y/(double)height) * 4095;
	switch( eventPtr->xbutton.button ) {
	case Button1:
	    /* Left mouse: Zoom out */
	    rt_vls_printf( &dm_values.dv_string, "L 1 %d %d\n", x, y);
	    break;
	case Button2:
	    /* Middle mouse, up to down transition */
	    rt_vls_printf( &dm_values.dv_string, "M 1 %d %d\n", x, y);
	    break;
	case Button3:
	    /* Right mouse: Zoom in */
	    rt_vls_printf( &dm_values.dv_string, "R 1 %d %d\n", x, y);
	    break;
	}
    } else if( eventPtr->type == ButtonRelease ) {
	int	x, y;
	x = (eventPtr->xbutton.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xbutton.y/(double)height) * 4095;
	switch( eventPtr->xbutton.button ) {
	case Button1:
	    rt_vls_printf( &dm_values.dv_string, "L 0 %d %d\n", x, y);
	    break;
	case Button2:
	    /* Middle mouse, down to up transition */
	    rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y);
	    break;
	case Button3:
	    rt_vls_printf( &dm_values.dv_string, "R 0 %d %d\n", x, y);
	    break;
	}
    } else if( eventPtr->type == KeyPress ) {
	register int i;
	/* Turn these into MGED "buttonpress" or knob functions */
	
	cnt = XLookupString(&eventPtr->xkey, keybuf, sizeof(keybuf),
			    &key, &compose_stat);

    	/* CJXX I think the following line is bad in X.c*/
/*	for(i=0 ; i < cnt ; i++){*/

	inc = 0.1;		
	switch( key ) {
	case '?':
		rt_log( "\n\t\tKey Help Menu:\n\
\n\tView Control Functions\n\
0, <F12>	Zero sliders (knobs)\n\
s		Toggle sliders\n\
x		Enable xrot slider\n\
y		Enable yrot slider\n\
z		Enable zrot slider\n\
X		Enable Xslew slider\n\
Y		Enable Yslew slider\n\
Z		Enable Zslew slider\n\
S		Enable Zoom (Scale) slider\n\
<Up Arrow>	Enable slider above the current slider\n\
<Down Arrow>	Enable slider below the current slider\n\
<Left Arrow>	Move enabled slider to the left (decrement)\n\
<Right Arrow>	Move enabled slider to the right (increment)\n\
f		Front view\n\
t		Top view\n\
b		Bottom view\n\
l		Left view\n\
r		Right view\n\
R		Rear view\n\
3		35,25 view\n\
4		45,45 view\n\
\n\tToggle Functions\n\
<F1>		Toggle depthcueing\n\
<F2>		Toggle zclipping\n\
<F3>		Toggle perspective\n\
<F4>		Toggle zbuffer status\n\
<F5>		Toggle smooth-shading\n\
<F6>		Change perspective angle\n\
<F7>,F		Toggle faceplate\n\
" );
		break;
	case 'w':
		print_cmap();
		break;
	case '0':
		rt_vls_printf( &dm_values.dv_string, "knob zero\n" );
		break;
	case 's':
		sl_toggle_scroll(); /* calls rt_vls_printf() */
		break;
	case 'S':
		ogl2_which_slid = Ogl2_ZOOM;
		break;
	case 'x':
		/* 6 degrees per unit */
		ogl2_which_slid = Ogl2_XROT;
		break;
	case 'y':
		ogl2_which_slid = Ogl2_YROT;
		break;
	case 'z':
		ogl2_which_slid = Ogl2_ZROT;
		break;
	case 'X':
		ogl2_which_slid = Ogl2_XSLEW;
		break;
	case 'Y':
		ogl2_which_slid = Ogl2_YSLEW;
		break;
	case 'Z':
		ogl2_which_slid = Ogl2_ZSLEW;
		break;
	case 'f':
		rt_vls_strcat( &dm_values.dv_string, "press front\n");
		break;
	case 't':
		rt_vls_strcat( &dm_values.dv_string, "press top\n");
		break;
	case 'b':
		rt_vls_strcat( &dm_values.dv_string, "press bottom\n");
		break;
	case 'l':
		rt_vls_strcat( &dm_values.dv_string, "press left\n");
		break;
	case 'r':
		rt_vls_strcat( &dm_values.dv_string, "press right\n");
		break;
	case 'R':
		rt_vls_strcat( &dm_values.dv_string, "press rear\n");
		break;
	case '3':
		rt_vls_strcat( &dm_values.dv_string, "press 35,25\n");
		break;
	case '4':
		rt_vls_strcat( &dm_values.dv_string, "press 45,45\n");
		break;
	case 'F':
		no_faceplate = !no_faceplate;
		rt_vls_strcat( &dm_values.dv_string,
			      no_faceplate ?
			      "set faceplate 0\n" :
			      "set faceplate 1\n" );
		break;
	case XK_F1:			/* F1 key */
	    	rt_log("F1 botton!\n");
		rt_vls_printf( &dm_values.dv_string,
				"dm set depthcue !\n");
		break;
	case XK_F2:			/* F2 key */
		rt_vls_printf(&dm_values.dv_string,
				"dm set zclip !\n");
		break;
	case XK_F3:			/* F3 key */
		mvars.perspective_mode = 1-mvars.perspective_mode;
		rt_vls_printf( &dm_values.dv_string,
			    "set perspective %d\n",
			    mvars.perspective_mode ?
			    perspective_table[perspective_angle] :
			    -1 );
		dmaflag = 1;
		break;
	case XK_F4:			/* F4 key */
		/* toggle zbuffer status */
		rt_vls_printf(&dm_values.dv_string,
				"dm set zbuffer !\n");
		break;
	case XK_F5:			/* F5 key */
		/* toggle status */
		rt_vls_printf(&dm_values.dv_string,
		    "dm set lighting !\n");
	    	break;
	case XK_F6:			/* F6 key */
		/* toggle perspective matrix */
		if (--perspective_angle < 0) perspective_angle = 3;
		if(mvars.perspective_mode) rt_vls_printf( &dm_values.dv_string,
			    "set perspective %d\n",
			    perspective_table[perspective_angle] );
		dmaflag = 1;
		break;
	case XK_F7:			/* F7 key */
		/* Toggle faceplate on/off */
		no_faceplate = !no_faceplate;
		rt_vls_strcat( &dm_values.dv_string,
				    no_faceplate ?
				    "set faceplate 0\n" :
				    "set faceplate 1\n" );
		Ogl2_configure_window_shape();
		dmaflag = 1;
		break;
	case XK_F12:			/* F12 key */
		rt_vls_printf( &dm_values.dv_string, "knob zero\n" );
		break;
	case XK_Up:
	    	if (ogl2_which_slid-- == 0)
	    		ogl2_which_slid = ogl2_NUM_SLID - 1;
		break;
	case XK_Down:
	    	if (ogl2_which_slid++ == ogl2_NUM_SLID - 1)
	    		ogl2_which_slid = 0;
		break;
	case XK_Left:
	    	/* set inc and fall through */
	    	inc = -0.1;
	case XK_Right:
	    	/* keep value of inc set at top of switch */
	    	switch(ogl2_which_slid){
	    	case Ogl2_XSLEW:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob X %f\n", rate_slew[X] + inc);
	    		break;
	    	case Ogl2_YSLEW:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob Y %f\n", rate_slew[Y] + inc);
	    		break;
	    	case Ogl2_ZSLEW:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob Z %f\n", rate_slew[Z] + inc);
	    		break;
	    	case Ogl2_ZOOM:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob S %f\n", rate_zoom + inc);
	    		break;
	    	case Ogl2_XROT:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob x %f\n", rate_rotate[X] + inc);
	    		break;
	    	case Ogl2_YROT:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob y %f\n", rate_rotate[Y] + inc);
	    		break;
	    	case Ogl2_ZROT:
	    		rt_vls_printf( &dm_values.dv_string, 
				"knob z %f\n", rate_rotate[Z] + inc);
	    		break;
	    	default:
	    		break;
	    	}
		break;
	case XK_Shift_L:
	case XK_Shift_R:
		break;
	default:
		rt_log("dm-ogl: The key '%c' is not defined. Press '?' for help.\n", key);
		break;
	    }

/*	}  for loop */
    } else {
	return 1;
    }

    return 1;
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
	    /* constant tracking ON */
	    OgldoMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    OgldoMotion = 0;
	    break;
	default:
	    rt_log("Ogl2_statechange: unknown state %s\n", state_str[b]);
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

	if( mvars.debug )  rt_log("colorchange\n");

	/* Program the builtin colors */
	ogl2_rgbtab[0].r=0; 
	ogl2_rgbtab[0].g=0; 
	ogl2_rgbtab[0].b=0;/* Black */
	ogl2_rgbtab[1].r=255; 
	ogl2_rgbtab[1].g=0; 
	ogl2_rgbtab[1].b=0;/* Red */
	ogl2_rgbtab[2].r=0; 
	ogl2_rgbtab[2].g=0; 
	ogl2_rgbtab[2].b=255;/* Blue */
	ogl2_rgbtab[3].r=255; 
	ogl2_rgbtab[3].g=255;
	ogl2_rgbtab[3].b=0;/*Yellow */
	ogl2_rgbtab[4].r = ogl2_rgbtab[4].g = ogl2_rgbtab[4].b = 255; /* White */
	slotsused = 5;

	if( mvars.rgb )  {
		if(mvars.cueing_on) {
			glEnable(GL_FOG);
		} else {
			glDisable(GL_FOG);
		}

		glColor3ub( (short)255,  (short)255,  (short)255 );

		/* apply region-id based colors to the solid table */
		color_soltab();

		return;
	}

	if(USE_RAMP && (ogl2_index_size < 7)) {
		rt_log("Too few bitplanes: depthcueing and lighting disabled\n");
		mvars.cueing_on = 0;
		mvars.lighting_on = 0;
	}
	/* number of slots is 2^indexsize */
	ogl2_nslots = 1<<ogl2_index_size;
	if( ogl2_nslots > NSLOTS )  ogl2_nslots = NSLOTS;
	if(USE_RAMP) {
		/* peel off reserved ones */
		ogl2_nslots = (ogl2_nslots - CMAP_BASE) / CMAP_RAMP_WIDTH;
	} else {
		ogl2_nslots -= CMAP_BASE;	/* peel off the reserved entries */
	}

	ovec = -1;	/* Invalidate the old colormap entry */

	/* apply region-id based colors to the solid table */
	color_soltab();

	/* best to do this before the colorit */
	if (mvars.cueing_on && mvars.lighting_on){
		mvars.lighting_on = 0;
		glDisable(GL_LIGHTING);
	}

	/* Map the colors in the solid table to colormap indices */
	Ogl2_colorit();

	for( i=0; i < slotsused; i++ )  {
		Ogl2_gen_color( i, ogl2_rgbtab[i].r, ogl2_rgbtab[i].g, ogl2_rgbtab[i].b);
	}

	/* best to do this after the colorit */
	if (mvars.cueing_on){
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
	mvars.debug = lvl;
	XFlush(dpy);
	rt_log("flushed\n");
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
  int ndevices;
  int nclass = 0;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  struct rt_vls str;
  Display *tmp_dpy;

  rt_vls_init(&str);
  rt_vls_init(&pathName);

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

  rt_vls_printf(&pathName, ".dm_ogl%d", count++);

  /* this is important so that Ogl2_configure_notify knows to set
   * the font */
  fontstruct = NULL;

  if((tmp_dpy = XOpenDisplay(name)) == NULL){
    rt_vls_free(&str);
    return -1;
  }

  ((struct ogl_vars *)dm_vars)->width =
      DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;
  ((struct ogl_vars *)dm_vars)->height =
      DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;

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
    rt_log( "dm-X: Can't open X display\n" );
    return -1;
  }

  rt_vls_strcpy(&str, "init_ogl ");
  rt_vls_printf(&str, "%s\n", rt_vls_addr(&pathName));
  rt_log("pathname = %s\n", rt_vls_addr(&pathName));

  if(cmdline(&str, FALSE) == CMD_BAD){
    rt_vls_free(&str);
    return -1;
  }

  dpy = Tk_Display(xtkwin);

  /* must do this before MakeExist */
  if ((vip=Ogl2_set_visual(xtkwin))==NULL){
    rt_log("Ogl2_open: Can't get an appropriate visual.\n");
    return -1;
  }

  Tk_GeometryRequest(xtkwin, width, height);
  Tk_MoveToplevelWindow(xtkwin, 1276 - 976, 0);
  Tk_MakeWindowExist(xtkwin);

  win = Tk_WindowId(xtkwin);

  a_screen = Tk_ScreenNumber(xtkwin);

  /* open GLX context */
  /* If the sgi display manager has been used, then we must use
   * an indirect context. Otherwise use direct, since it is usually
   * faster.
   */
  if ((glxc = glXCreateContext(dpy, vip, 0, ogl2_sgi_used ? GL_FALSE : GL_TRUE))==NULL) {
    rt_log("Ogl2_open: couldn't create glXContext.\n");
		return -1;
  }
  /* If we used an indirect context, then as far as sgi is concerned,
   * gl hasn't been used.
   */
  ogl2_is_direct = (char) glXIsDirect(dpy, glxc);
  rt_log("Using %s OpenGL rendering context.\n", ogl2_is_direct ? "a direct" : "an indirect");
  /* set ogl2_Ogl2_used if the context was ever direct */
  ogl2_Ogl2_used = (ogl2_is_direct || ogl2_Ogl2_used);

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
	  rt_log("Glx_open: Couldn't open the dials+buttons\n");
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
  XFreeDeviceList(olist);

  /* Register the file descriptor with the Tk event handler */
  Tk_CreateGenericHandler(Ogldoevent, (ClientData)NULL);

#if 0
  Tk_SetWindowBackground(xtkwin, bg);
#endif

  if (!glXMakeCurrent(dpy, win, glxc)){
    rt_log("Ogl2_open: Couldn't make context current\n");
    return -1;
  }

  /* display list (fontOffset + char) will displays a given ASCII char */
  if ((fontOffset = glGenLists(128))==0){
    rt_log("dm-ogl: Can't make display lists for font.\n");
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
  if (mvars.rgb)
    glFogfv(GL_FOG_COLOR, backgnd);
  else
    glFogi(GL_FOG_INDEX, CMAP_RAMP_WIDTH - 1);
  glFogf(GL_FOG_DENSITY, VIEWFACTOR);
	

  /* Initialize matrices */
  /* Leave it in model_view mode normally */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 2.0);
  glGetDoublev(GL_PROJECTION_MATRIX, faceplate_mat);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity(); 
  glTranslatef( 0.0, 0.0, -1.0); 
  glPushMatrix();
  glLoadIdentity();
  ogl2_face_flag = 1;	/* faceplate matrix is on top of stack */
		
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
					cmap = XCreateColormap(dpy,
						RootWindow(dpy, maxvip->screen),
						maxvip->visual, AllocAll);
				else
					cmap = XCreateColormap(dpy,
						RootWindow(dpy, maxvip->screen),
						maxvip->visual, AllocNone);

				if (Tk_SetWindowVisual(tkwin, maxvip->visual, maxvip->depth, cmap)){
					mvars.doublebuffer = m_double;
					glXGetConfig(dpy, maxvip, GLX_DEPTH_SIZE, &ogl2_depth_size);
					if (ogl2_depth_size > 0)
						ogl2_has_zbuf = 1;
					mvars.rgb = m_rgba;
					if (!m_rgba){
						glXGetConfig(dpy, maxvip, GLX_BUFFER_SIZE, &ogl2_index_size);
					}
					stereo_is_on = m_stereo;
					return (maxvip); /* sucess */
				} else { 
					/* retry with lesser depth */
					baddepth = maxvip->depth;
					tries ++;
					XFreeColormap(dpy,cmap);
				}
			}
					
		}
				

		/* if no success at this point, relax a desire and try again */
		if ( m_stereo ){
			m_stereo = 0;
			rt_log("Stereo not available.\n");
		} else if (m_rgba) {
			m_rgba = 0;
			rt_log("RGBA not available.\n");
		} else if (m_double) {
			m_double = 0;
			rt_log("Doublebuffering not available. \n");
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
	height = xwa.height;
	width = xwa.width;
	
	glViewport(0,  0, (width), (height));
	glScissor(0,  0, (width)+1, (height)+1);

	if( mvars.zbuffer_on )
	  establish_zbuffer();

	establish_lighting();

#if 0
	glDrawBuffer(GL_FRONT_AND_BACK);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	if (mvars.zbuffer_on)
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		glClear( GL_COLOR_BUFFER_BIT);

	if (mvars.doublebuffer)
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


	/* First time through, load a font or quit */
	if (fontstruct == NULL) {
		if ((fontstruct = XLoadQueryFont(dpy, FONT9)) == NULL ) {
			/* Try hardcoded backup font */
			if ((fontstruct = XLoadQueryFont(dpy, FONTBACK)) == NULL) {
				rt_log( "dm-ogl: Can't open font '%s' or '%s'\n", FONT9, FONTBACK );
				return;
			}
		}
		glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
	}
		

	/* Always try to choose a the font that best fits the window size.
	 */

	if (width < 582) {
		if (fontstruct->per_char->width != 5) {
			if ((newfontstruct = XLoadQueryFont(dpy, FONT5)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else if (width < 679) {
		if (fontstruct->per_char->width != 6){
			if ((newfontstruct = XLoadQueryFont(dpy, FONT6)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else if (width < 776) {
		if (fontstruct->per_char->width != 7){
			if ((newfontstruct = XLoadQueryFont(dpy, FONT7)) != NULL ) {
				XFreeFont(dpy,fontstruct);
				fontstruct = newfontstruct;
				glXUseXFont( fontstruct->fid, 0, 127, fontOffset);
			}
		}
	} else if (width < 873) {
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

	if( argc < 1 )  return -1;

	/* For now, only "set" command is implemented */
	if( strcmp( argv[0], "set" ) != 0 )  {
		rt_log("dm: command is not 'set'\n");
		return CMD_BAD;
	}

	rt_vls_init(&vls);
	if( argc < 2 )  {
		/* Bare set command, print out current settings */
		rt_structprint("dm_ogl internal variables", ogl2_vparse, (char *)0 );
		rt_log("%s", rt_vls_addr(&vls) );
	} else if( argc == 2 ) {
	        rt_vls_name_print( &vls, ogl2_vparse, argv[1], (char *)0 );
		rt_log( "%s\n", rt_vls_addr(&vls) );
  	} else {
	        rt_vls_printf( &vls, "%s=\"", argv[1] );
	        rt_vls_from_argv( &vls, argc-2, argv+2 );
		rt_vls_putc( &vls, '\"' );
		rt_structparse( &vls, ogl2_vparse, (char *)0 );
	}
	rt_vls_free(&vls);
	return CMD_OK;
}

void	
establish_lighting()
{


	if (!mvars.lighting_on) {
		/* Turn it off */
		glDisable(GL_LIGHTING);
		if (!mvars.rgb)
			Ogl2_colorchange();
	} else {
		/* Turn it on */

		if (!mvars.rgb){
			if (mvars.cueing_on){
				mvars.cueing_on = 0;
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


void	
establish_zbuffer()
{
	if( ogl2_has_zbuf == 0 ) {
		rt_log("dm-ogl: This machine has no Zbuffer to enable\n");
		mvars.zbuffer_on = 0;
	}

	if (mvars.zbuffer_on)  {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}
	dmaflag = 1;
	
	return;
}


void
Ogl2_colorit()
{
	register struct solid	*sp;
	register struct rgbtab *rgb;
	register int i;
	register int r,g,b;

	if( mvars.rgb )  return;

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
		rgb = ogl2_rgbtab;
		for( i = 0; i < slotsused; i++, rgb++ )  {
			if( rgb->r == r && rgb->g == g && rgb->b == b )  {
				sp->s_dmindex = i;
				goto next;
			}
		}

		/* If slots left, create a new color map entry, first-come basis */
		if( slotsused < ogl2_nslots )  {
			rgb = &ogl2_rgbtab[i=(slotsused++)];
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


/*			G E N _ C O L O R
 *
 *	maps a given color into the appropriate colormap entry
 *	for both depthcued and non-depthcued mode.  In depthcued mode,
 *	Ogl2_gen_color also generates the colormap ramps.  Note that in depthcued
 *	mode, DM_BLACK uses map entry 0, and does not generate a ramp for it.
 *	Non depthcued mode skips the first CMAP_BASE colormap entries.
 *
 *	This routine is not called at all if mvars.rgb is set.
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

			red = r_inc = ogl2_rgbtab[c].r * (256/CMAP_RAMP_WIDTH);
			green = g_inc = ogl2_rgbtab[c].g * (256/CMAP_RAMP_WIDTH);
			blue = b_inc = ogl2_rgbtab[c].b * (256/CMAP_RAMP_WIDTH);

#if 0
			red = ogl2_rgbtab[c].r * 256;
			green = ogl2_rgbtab[c].g * 256;
			blue = ogl2_rgbtab[c].b * 256;
#endif
			

			if (mvars.cueing_on){
				for(i = 0, j = MAP_ENTRY(c) + CMAP_RAMP_WIDTH - 1; 
					i < CMAP_RAMP_WIDTH;
				    i++, j--, red += r_inc, green += g_inc, blue += b_inc){
				    	cells[i].pixel = j;
				    	cells[i].red = (short)red;
				    	cells[i].green = (short)green;
				    	cells[i].blue = (short)blue;
				    	cells[i].flags = DoRed|DoGreen|DoBlue;
				}
			} else { /* mvars.lighting_on */ 
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
			XStoreColors(dpy, cmap, cells, CMAP_RAMP_WIDTH);
		}
	} else {
		XColor cell, celltest;

		cell.pixel = c + CMAP_BASE;
		cell.red = ogl2_rgbtab[c].r * 256;
		cell.green = ogl2_rgbtab[c].g * 256;
		cell.blue = ogl2_rgbtab[c].b * 256;
		cell.flags = DoRed|DoGreen|DoBlue;
		XStoreColor(dpy, cmap, &cell);

	}
}

void
print_cmap()
{
	int i;
	XColor cell;

	for (i=0; i<112; i++){
		cell.pixel = i;
		XQueryColor(dpy, cmap, &cell);
		printf("%d  = %d %d %d\n",i,cell.red,cell.green,cell.blue);
	}
}

static void
Ogl_var_init()
{
  dm_vars = (char *)rt_malloc(sizeof(struct ogl_vars), Ogl_var_init: "ogl_vars");
  bzero((void *)dm_vars, sizeof(struct ogl_vars));
  devmotionnotify = LASTEvent;
  devbuttonpress = LASTEvent;
  devbuttonrelease = LASTEvent;

  /* initialize the modifiable variables */
  mvars.cueing_on = 1;          /* Depth cueing flag - for colormap work */
  mvars.zclipping_on = 1;       /* Z Clipping flag */
  mvars.zbuffer_on = 1;         /* Hardware Z buffer is on */
  mvars.linewidth = 1;      /* Line drawing width */
  mvars.dummy_perspective = 1;
}


static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct dm_list *p;
  struct rt_vls str;

  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    if(window == ((struct ogl_vars *)p->_dm_vars)->_win)
      return p;
  }

  return DM_LIST_NULL;
}
#endif
