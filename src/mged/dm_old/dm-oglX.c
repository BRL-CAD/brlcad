/*			D M - G L . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file tokens.h
 /*
  *
  *
  *
  *  Source -
  *	SECAD/VLD Computing Consortium, Bldg 394
  *	The U. S. Army Ballistic Research Laboratory
  *	Aberdeen Proving Ground, Maryland  21005
  */

#define CJDEBUG 0

#include "common.h"



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

#include <GL/glx.h>
#include <GL/gl.h>


#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "bu.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

#if 0
static void	label();
static void	draw();
#endif
static int	Ogl_xsetup();
static void	Ogl_configure_window_shape();
static int	Ogldoevent();
static void	Ogl_gen_color();

/* Flags indicating whether the gl and sgi display managers have been
 * attached.
 * These are necessary to decide whether or not to use direct rendering
 * with gl.
 */
char	ogl_ogl_used = 0;
char	ogl_sgi_used = 0;
char	ogl_is_direct = 0;

/* Display Manager package interface */

#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
int	Ogl_open();
int	Ogl_dm();
void	Ogl_close();
MGED_EXTERN(void	Ogl_input, (fd_set *input, int noblock) );
void	Ogl_prolog(), Ogl_epilog();
void	Ogl_normal(), Ogl_newrot();
void	Ogl_update();
void	Ogl_puts(), Ogl_2d_line(), Ogl_light();
int	Ogl_object();
unsigned Ogl_cvtvecs(), Ogl_load();
void	Ogl_statechange(), Ogl_viewchange(), Ogl_colorchange();
void	Ogl_window(), Ogl_debug(), Ogl_selectargs();

struct dm dm_gl = {
	Ogl_open, Ogl_close,
	Ogl_input,
	Ogl_prolog, Ogl_epilog,
	Ogl_normal, Ogl_newrot,
	Ogl_update,
	Ogl_puts, Ogl_2d_line,
	Ogl_light,
	Ogl_object,	Ogl_cvtvecs, Ogl_load,
	Ogl_statechange,
	Ogl_viewchange,
	Ogl_colorchange,
	Ogl_window, Ogl_debug,
	0,				/* no displaylist */
	0,				/* multi-window */
	IRBOUND,
	"gl", "X Windows with OpenGL graphics",
	0,				/* mem map */
	Ogl_dm
};


/* ogl stuff */
static GLXContext glxc;
/*static Window wind;*/
static int ogl_has_dbfr;
static int ogl_depth_size;
static int ogl_has_rgb;
static int ogl_has_stereo;
static int ogl_index_size;
static Colormap cmap;

static int fontOffset;
static XVisualInfo *ogl_set_visual();

static GLdouble faceplate_mat[16];
static int ogl_face_flag;		/* 1: faceplate matrix on stack */

/*
 * These variables are visible and modifiable via a "dm set" command.
 */
static int	cueing_on = 1;		/* Depth cueing flag - for colormap work */
static int	zclipping_on = 1;	/* Z Clipping flag */
static int	zbuffer_on = 1;		/* Hardware Z buffer is on */
static int	lighting_on = 0;	/* Lighting model on */
static int	ogl_debug;		/* 2 for basic, 3 for full */
static int	no_faceplate = 0;	/* Don't draw faceplate */
static int	ogl_linewidth = 1;	/* Line drawing width */
static int	ogl_fastfog = 1;	/* 1 -> fast, 0 -> quality */
static double	ogl_fogdensity = 1.0;	/* Fog density parameter */
/*
 * These are derived from the hardware inventory -- user can change them,
 * but the results may not be pleasing.  Mostly, this allows them to be seen.
 */
static int	ogl_is_gt;		/* 0 for non-GT machines */
static int	ogl_has_zbuf;		/* 0 if no Z buffer */
static int	ogl_has_rgb;		/* 0 if mapped mode must be used */
static int	ogl_has_doublebuffer;	/* 0 if singlebuffer mode must be used */


static int	min_scr_z;		/* based on (glGetIntegerv(XXX_ZMIN, &gdtmp), gdtmp) */
static int	max_scr_z;		/* based on (glGetIntegerv(XXX_ZMAX, &gdtmp), gdtmp) */
/* End modifiable variables */

static int	ogl_fd;			/* GL file descriptor to select() on */
static const char ogl_title[] = "BRL MGED";
static int perspective_mode = 0;	/* Perspective flag */
static int perspective_angle =3;	/* GLfloat of perspective */
static int perspective_table[] = {
	30, 45, 60, 90 };
static int ovec = -1;		/* Old color map entry number */
static int kblights();
static double	xlim_view = 1.0;	/* args for glOrtho*/
static double	ylim_view = 1.0;
static mat_t	aspect_corr;
static int	stereo_is_on = 0;

void		ogl_colorit();

/*
 * SGI Color Map table
 */
#define NSLOTS		4080	/* The mostest possible - may be fewer */
static int ogl_nslots=0;		/* how many we have, <= NSLOTS */
static int slotsused;		/* how many actually used */
static struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
} ogl_rgbtab[NSLOTS];

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

static void
refresh_hook()
{
	dmaflag = 1;
}

static void
do_linewidth()
{
	glLineWidth((GLfloat) ogl_linewidth);
	dmaflag = 1;
}


static void
do_fog()
{
	glHint(GL_FOG_HINT, ogl_fastfog ? GL_FASTEST : GL_NICEST);
	dmaflag = 1;
}

struct bu_structparse Ogl_vparse[] = {
	{
		"%d",  1, "depthcue",		(int)&cueing_on,	Ogl_colorchange 	},
	{
		"%d",  1, "zclip",		(int)&zclipping_on,	refresh_hook 	}, /* doesn't seem to have any use*/
	{
		"%d",  1, "zbuffer",		(int)&zbuffer_on,	establish_zbuffer 	},
	{
		"%d",  1, "lighting",		(int)&lighting_on,	establish_lighting 	},
	{
		"%d",  1, "no_faceplate",	(int)&no_faceplate,	refresh_hook 	},
	{
		"%d",  1, "has_zbuf",		(int)&ogl_has_zbuf,	refresh_hook 	},
	{
		"%d",  1, "has_rgb",		(int)&ogl_has_rgb,	Ogl_colorchange 	},
	{
		"%d",  1, "has_doublebuffer",	(int)&ogl_has_doublebuffer, refresh_hook 	},
	{
		"%d",  1, "min_scr_z",		(int)&min_scr_z,	refresh_hook 	},
	{
		"%d",  1, "max_scr_z",		(int)&max_scr_z,	refresh_hook 	},
	{
		"%d",  1, "debug",		(int)&ogl_debug,		FUNC_NULL 	},
	{
		"%d",  1, "linewidth",		(int)&ogl_linewidth,	do_linewidth 	},
	{
		"%f",   1, "density",		(int)&ogl_fogdensity,  refresh	},
	{
		"%d",   1, "fastfog",		(int)&ogl_fastfog,  do_fog	},
	{
		"",	0,  (char *)0,		0,			FUNC_NULL 	}
};

/*int ogl_winx_size, ogl_winy_size;*/

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

static int height, width;
static Tcl_Interp *xinterp;
static Tk_Window xtkwin;
static int OgldoMotion = 0;

static Display	*dpy;			/* X display pointer */
static Window	win;			/* X window */
static unsigned long black,gray,white,yellow,red,blue;
static unsigned long bd, bg, fg;   /*color of border, background, foreground */

static GC	gc;			/* X Graphics Context */
static int	is_monochrome = 0;
static XFontStruct *fontstruct;		/* X Font */

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	(((x)/4096.0+0.5)*width)
#define	GED_TO_Xy(x)	((0.5-(x)/4096.0)*height)

/* get rid of when no longer needed */
#define CMAP_BASE	32
#define CMAP_RAMP_WIDTH	16
#define MAP_ENTRY(x)	((cueing_on) ? \
			((x) * CMAP_RAMP_WIDTH + CMAP_BASE) : \
			((x) + CMAP_BASE) )

/********************************************************************/



/*
 *			O G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
Ogl_open()
{
        char	line[82];
        char	hostname[80];
	char	display[82];
	char	*envp;

	ogl_debug = CJDEBUG;

	/* get or create the default display */
	if( (envp = getenv("DISPLAY")) == NULL ) {
		/* Env not set, use local host */
		gethostname( hostname, 80 );
		hostname[79] = '\0';
		(void)sprintf( display, "%s:0", hostname );
		envp = display;
	}

	bu_log("X Display [%s]? ", envp );
	(void)fgets( line, sizeof(line), stdin );
	line[strlen(line)-1] = '\0';		/* remove newline */
	if( feof(stdin) )  quit();
	if( line[0] != '\0' ) {
		if( Ogl_xsetup(line) ) {
			return(1);		/* BAD */
		}
	} else {
		if( Ogl_xsetup(envp) ) {
			return(1);	/* BAD */
		}
	}

	return(0);			/* OK */
}

/*
 *  			O G L _ C L O S E
 *
 *  Gracefully release the display.
 */
void
Ogl_close()
{
	glDrawBuffer(GL_FRONT);
	glClearColor(0.0, 0.0, 0.0, 0.0);
/*	glClearDepth(0.0);*/
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glXMakeCurrent(dpy, None, NULL);

	glXDestroyContext(dpy, glxc);

	Tk_DeleteGenericHandler(Ogldoevent, (ClientData)NULL);
	Tk_DestroyWindow(xtkwin);
	Tcl_DeleteInterp(xinterp);
}

/*
 *			O G L _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
Ogl_prolog()
{
	GLint mm;
	char i;
	char *str = "a";
	GLfloat fogdepth;

	if (ogl_debug)
		bu_log( "Ogl_prolog\n");

	if (dmaflag) {
		if (!ogl_has_doublebuffer){
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
/*			return;*/
		}

		if (ogl_face_flag){
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			ogl_face_flag = 0;
			if (cueing_on){
				glEnable(GL_FOG);
				fogdepth = 2.2 * Viewscale; /* 2.2 is heuristic */
				glFogf(GL_FOG_END, fogdepth);
				fogdepth = (GLfloat) (0.5*ogl_fogdensity/Viewscale);
				glFogf(GL_FOG_DENSITY, fogdepth);
				glFogi(GL_FOG_MODE, perspective_mode ? GL_EXP : GL_LINEAR);
			}
			if (lighting_on){
				glEnable(GL_LIGHTING);
			}
		}

		glLineWidth((GLfloat) ogl_linewidth);

	}


}

/*
 *			O G L _ E P I L O G
 */
void
Ogl_epilog()
{
	if (ogl_debug)
		bu_log( "ogl_epilog\n");
	/*
	 * A Point, in the Center of the Screen.
	 * This is drawn last, to always come out on top.
	 */

	glColor3ub( (short)ogl_rgbtab[4].r, (short)ogl_rgbtab[4].g, (short)ogl_rgbtab[4].b );
	glBegin(GL_POINTS);
	 glVertex2i(0,0);
	glEnd();
	/* end of faceplate */

	if(ogl_has_doublebuffer )
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

		bu_log("ANY ERRORS?\n");
		while(	(error = glGetError())!=0){
			bu_log("Error: %x\n", error);
		}
	}


	return;
}

/*
 *  			O G L _ N E W R O T
 *  load new rotation matrix onto top of stack
 */
void
Ogl_newrot(mat, which_eye)
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


	if (ogl_debug)
		bu_log( "Ogl_newrot()\n");
	switch(which_eye)  {
	case 0:
		/* Non-stereo */
		break;
	case 1:
		/* R eye */
		glViewport(0,  0, (XMAXSCREEN)+1, ( YSTEREO)+1);
		glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
		Ogl_puts( "R", 2020, 0, 0, DM_RED );
		break;
	case 2:
		/* L eye */
		glViewport(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1, ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
		glScissor(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1, ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
		break;
	}

	mptr = mat;

	gtmat[0] = *(mptr++) * aspect_corr[0];
	gtmat[4] = *(mptr++) * aspect_corr[0];
	gtmat[8] = *(mptr++) * aspect_corr[0];
	gtmat[12] = *(mptr++) * aspect_corr[0];

	gtmat[1] = *(mptr++) * aspect_corr[5];
	gtmat[5] = *(mptr++) * aspect_corr[5];
	gtmat[9] = *(mptr++) * aspect_corr[5];
	gtmat[13] = *(mptr++) * aspect_corr[5];

	gtmat[2] = *(mptr++);
	gtmat[6] = *(mptr++);
	gtmat[10] = *(mptr++);
	gtmat[14] = *(mptr++);

	gtmat[3] = *(mptr++);
	gtmat[7] = *(mptr++);
	gtmat[11] = *(mptr++);
	gtmat[15] = *(mptr++);


#if 0
	/* CJXX still necessary? I don't think so */
	/*
	 *  Convert between MGED's right handed coordinate system
	 *  where +Z comes out of the screen to the Silicon Graphics's
	 *  left handed coordinate system, where +Z goes INTO the screen.
	 */
	gtmat[0] = -gtmat[0];
	gtmat[1] = -gtmat[1];
	gtmat[2] = -gtmat[2];
	gtmat[3] = -gtmat[3];
#endif

	/*CJXX experimental */
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
	if ( perspective_mode){
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
	if (lighting_on ){
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
Ogl_object( sp, mat, ratio, white_flag )
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

	if (ogl_debug)
		bu_log( "Ogl_Object()\n");


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


	if( ogl_has_rgb )  {
		register short	r, g, b;
		if( white_flag )  {
			r = g = b = 230;
		} else {
			r = (short)sp->s_color[0];
			g = (short)sp->s_color[1];
			b = (short)sp->s_color[2];
		}

		if(lighting_on) /* && ogl_is_gt)*/
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
#if 0
			if (cueing_on){
				glEnable(GL_FOG);
			}
#endif
		}
	} else {

		if( white_flag ) {
			ovec = nvec = MAP_ENTRY(DM_WHITE);
			/* Use the *next* to the brightest white entry */
			if(cueing_on)  {
				glFogi(GL_FOG_INDEX, nvec + 1);
				glIndexi( nvec+1);
/*				lshaderange(nvec+1, nvec+1,
				    min_scr_z, max_scr_z );*/
			} else {
				glIndexi(nvec);
			}
		} else {
			if( (nvec = MAP_ENTRY( sp->s_dmindex )) != ovec) {
				/* Use only the middle 14 to allow for roundoff...
				 * Pity the poor fool who has defined a black object.
				 * The code will use the "reserved" color map entries
				 * to display it when in depthcued mode.
				 */
				if(cueing_on)  {
					glFogi(GL_FOG_INDEX, nvec+14);
					glIndexi(nvec+1);
/*					lshaderange(nvec+1, nvec+14,
					    min_scr_z, max_scr_z );*/
				} else {
					glIndexi(nvec);
				}
				ovec = nvec;
			}
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
Ogl_normal()
{
	GLint mm;

	if (ogl_debug)
		bu_log( "Ogl_normal\n");

	if( ogl_has_rgb )  {
		glColor3ub( 0,  0,  0 );
	} else {
		glIndexi((int) MAP_ENTRY(DM_BLACK));
	}

	if (!ogl_face_flag){
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrixd( faceplate_mat );
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		ogl_face_flag = 1;
		if(cueing_on)
			glDisable(GL_FOG);
		if (lighting_on)
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
Ogl_update()
{
	if (ogl_debug)
		bu_log( "Ogl_update()\n");

    XFlush(dpy);
}


/*
 *			XOGL _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
void
Ogl_puts( str, x, y, size, colour )
register char *str;
int x,y,size, colour;
{
	if (ogl_debug)
		bu_log( "Ogl_puts()\n");


/*	glRasterPos2f( GED2IRIS(x),  GED2IRIS(y));*/
	if( ogl_has_rgb )  {
		glColor3ub( (short)ogl_rgbtab[colour].r,  (short)ogl_rgbtab[colour].g,  (short)ogl_rgbtab[colour].b );
	} else {
		glIndexi( MAP_ENTRY(colour) );
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
Ogl_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	register int nvec;

	if (ogl_debug)
		bu_log( "Ogl_2d_line()\n");

	if( ogl_has_rgb )  {
		/* Yellow */

		glColor3ub( (short)255,  (short)255,  (short) 0 );
	} else {
#if 1
		if((nvec = MAP_ENTRY(DM_YELLOW)) != ovec) {
/*			if(cueing_on) lshaderange(nvec, nvec,
			    min_scr_z, max_scr_z );*/
			if(cueing_on) {
				glFogi(GL_FOG_INDEX, nvec + 1);
				glIndexi(nvec+1);
			} else {
				glIndexi( nvec );
			}
			ovec = nvec;
		}
#endif
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

#define OGL_NUM_SLID	7
#define OGL_XSLEW	0
#define OGL_YSLEW	1
#define OGL_ZSLEW	2
#define OGL_ZOOM	3
#define OGL_XROT	4
#define OGL_YROT	5
#define OGL_ZROT	6

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
	static int ogl_which_slid = OGL_XSLEW;
	struct rt_vlist head, tail;
	point_t point;


    if (eventPtr->xany.window != win)
	return 0;

    if (eventPtr->type == Expose ) {
	if (eventPtr->xexpose.count == 0) {
		glClearColor(0.0, 0.0, 0.0, 0.0);
		if (ogl_has_zbuf)
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear(GL_COLOR_BUFFER_BIT);
	    bu_vls_printf( &dm_values.dv_string, "refresh\n" );
		dmaflag = 1;
	}
    } else if( eventPtr->type == ConfigureNotify) {
    	Ogl_configure_window_shape();
    } else if( eventPtr->type == MotionNotify ) {
	int	x, y;
	if ( !OgldoMotion )
	    return 1;
	x = (eventPtr->xmotion.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xmotion.y/(double)height) * 4095;
	bu_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y );
    } else if( eventPtr->type == ButtonPress ) {
	/* There may also be ButtonRelease events */
	int	x, y;
	/* In MGED this is a "penpress" */
	x = (eventPtr->xbutton.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xbutton.y/(double)height) * 4095;
	switch( eventPtr->xbutton.button ) {
	case Button1:
	    /* Left mouse: Zoom out */
	    bu_vls_printf( &dm_values.dv_string, "L 1 %d %d\n", x, y);
	    break;
	case Button2:
	    /* Middle mouse, up to down transition */
	    bu_vls_printf( &dm_values.dv_string, "M 1 %d %d\n", x, y);
	    break;
	case Button3:
	    /* Right mouse: Zoom in */
	    bu_vls_printf( &dm_values.dv_string, "R 1 %d %d\n", x, y);
	    break;
	}
    } else if( eventPtr->type == ButtonRelease ) {
	int	x, y;
	x = (eventPtr->xbutton.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xbutton.y/(double)height) * 4095;
	switch( eventPtr->xbutton.button ) {
	case Button1:
	    bu_vls_printf( &dm_values.dv_string, "L 0 %d %d\n", x, y);
	    break;
	case Button2:
	    /* Middle mouse, down to up transition */
	    bu_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y);
	    break;
	case Button3:
	    bu_vls_printf( &dm_values.dv_string, "R 0 %d %d\n", x, y);
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
		bu_log( "\n\t\tKey Help Menu:\n\
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
#if 0
	case 'w':
	/* for temporary experimentation only */
		bu_log("Attempting invent_solid\n");
		RT_LIST_INIT( &(head.l) );
		tail.cmd[0] = RT_VLIST_LINE_MOVE;
		(tail.pt)[0][0] = 0.0;
		(tail.pt)[0][1] = 0.0;
		(tail.pt)[0][2] = 0.0;
		tail.cmd[1] = RT_VLIST_LINE_DRAW;
		(tail.pt)[1][0] = 300.0;
		(tail.pt)[1][1] = 30.0;
		(tail.pt)[1][2] = 3.0;
		tail.nused = 2;
		RT_LIST_APPEND( &(head.l), &(tail.l) );
		invent_solid("Carl_solid", &(head.l), 0xfff, 1);
		break;
#endif
	case '0':
		bu_vls_printf( &dm_values.dv_string, "knob zero\n" );
		break;
	case 's':
		sl_toggle_scroll(); /* calls bu_vls_printf() */
		break;
	case 'S':
		ogl_which_slid = OGL_ZOOM;
		break;
	case 'x':
		/* 6 degrees per unit */
		ogl_which_slid = OGL_XROT;
		break;
	case 'y':
		ogl_which_slid = OGL_YROT;
		break;
	case 'z':
		ogl_which_slid = OGL_ZROT;
		break;
	case 'X':
		ogl_which_slid = OGL_XSLEW;
		break;
	case 'Y':
		ogl_which_slid = OGL_YSLEW;
		break;
	case 'Z':
		ogl_which_slid = OGL_ZSLEW;
		break;
	case 'f':
		bu_vls_strcat( &dm_values.dv_string, "press front\n");
		break;
	case 't':
		bu_vls_strcat( &dm_values.dv_string, "press top\n");
		break;
	case 'b':
		bu_vls_strcat( &dm_values.dv_string, "press bottom\n");
		break;
	case 'l':
		bu_vls_strcat( &dm_values.dv_string, "press left\n");
		break;
	case 'r':
		bu_vls_strcat( &dm_values.dv_string, "press right\n");
		break;
	case 'R':
		bu_vls_strcat( &dm_values.dv_string, "press rear\n");
		break;
	case '3':
		bu_vls_strcat( &dm_values.dv_string, "press 35,25\n");
		break;
	case '4':
		bu_vls_strcat( &dm_values.dv_string, "press 45,45\n");
		break;
	case 'F':
		no_faceplate = !no_faceplate;
		bu_vls_strcat( &dm_values.dv_string,
			      no_faceplate ?
			      "set faceplate 0\n" :
			      "set faceplate 1\n" );
		break;
	case XK_F1:			/* F1 key */
	    	bu_log("F1 botton!\n");
		bu_vls_printf( &dm_values.dv_string,
				"dm set depthcue !\n");
		break;
	case XK_F2:			/* F2 key */
		bu_vls_printf(&dm_values.dv_string,
				"dm set zclip !\n");
		break;
	case XK_F3:			/* F3 key */
		perspective_mode = 1-perspective_mode;
		bu_vls_printf( &dm_values.dv_string,
			    "set perspective %d\n",
			    perspective_mode ?
			    perspective_table[perspective_angle] :
			    -1 );
		dmaflag = 1;
		break;
	case XK_F4:			/* F4 key */
		/* toggle zbuffer status */
		bu_vls_printf(&dm_values.dv_string,
				"dm set zbuffer !\n");
		break;
	case XK_F5:			/* F5 key */
		/* toggle status */
		bu_vls_printf(&dm_values.dv_string,
		    "dm set lighting !\n");
	    	break;
	case XK_F6:			/* F6 key */
		/* toggle perspective matrix */
		if (--perspective_angle < 0) perspective_angle = 3;
		if(perspective_mode) bu_vls_printf( &dm_values.dv_string,
			    "set perspective %d\n",
			    perspective_table[perspective_angle] );
		dmaflag = 1;
		break;
	case XK_F7:			/* F7 key */
		/* Toggle faceplate on/off */
		no_faceplate = !no_faceplate;
		bu_vls_strcat( &dm_values.dv_string,
				    no_faceplate ?
				    "set faceplate 0\n" :
				    "set faceplate 1\n" );
		Ogl_configure_window_shape();
		dmaflag = 1;
		break;
	case XK_F12:			/* F12 key */
		bu_vls_printf( &dm_values.dv_string, "knob zero\n" );
		break;
	case XK_Up:
	    	if (ogl_which_slid-- == 0)
	    		ogl_which_slid = OGL_NUM_SLID - 1;
		break;
	case XK_Down:
	    	if (ogl_which_slid++ == OGL_NUM_SLID - 1)
	    		ogl_which_slid = 0;
		break;
	case XK_Left:
	    	/* set inc and fall through */
	    	inc = -0.1;
	case XK_Right:
	    	/* keep value of inc set at top of switch */
	    	switch(ogl_which_slid){
	    	case OGL_XSLEW:
	    		bu_vls_printf( &dm_values.dv_string,
				"knob X %f\n", rate_slew[X] + inc);
	    		break;
	    	case OGL_YSLEW:
	    		bu_vls_printf( &dm_values.dv_string,
				"knob Y %f\n", rate_slew[Y] + inc);
	    		break;
	    	case OGL_ZSLEW:
	    		bu_vls_printf( &dm_values.dv_string,
				"knob Z %f\n", rate_slew[Z] + inc);
	    		break;
	    	case OGL_ZOOM:
	    		bu_vls_printf( &dm_values.dv_string,
				"knob S %f\n", rate_zoom + inc);
	    		break;
	    	case OGL_XROT:
	    		bu_vls_printf( &dm_values.dv_string,
				"knob x %f\n", rate_rotate[X] + inc);
	    		break;
	    	case OGL_YROT:
	    		bu_vls_printf( &dm_values.dv_string,
				"knob y %f\n", rate_rotate[Y] + inc);
	    		break;
	    	case OGL_ZROT:
	    		bu_vls_printf( &dm_values.dv_string,
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
		bu_log("dm-gl: The key '%c' is not defined. Press '?' for help.\n", key);
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
Ogl_input( input, noblock )
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
Ogl_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
Ogl_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
Ogl_load( addr, count )
unsigned addr, count;
{
	bu_log("Ogl_load(x%x, %d.)\n", addr, count );
	return( 0 );
}

void
Ogl_statechange( a, b )
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
	    bu_log("Ogl_statechange: unknown state %s\n", state_str[b]);
	    break;
	}

	/*Ogl_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
}

void
Ogl_viewchange()
{
}

void
Ogl_colorchange()
{
	register int i;
	register int nramp;
	XColor celltest;
	int count = 0;
	Colormap a_cmap;

	if( ogl_debug )  bu_log("colorchange\n");

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

	if( ogl_has_rgb )  {
		if(cueing_on) {
			glEnable(GL_FOG);
		} else {
			glDisable(GL_FOG);
		}

		glColor3ub( (short)255,  (short)255,  (short)255 );

		/* apply region-id based colors to the solid table */
		color_soltab();

		return;
	}
#if 1
	if(cueing_on && (ogl_index_size < 7)) {
		bu_log("Too few bitplanes: depthcueing disabled\n");
		cueing_on = 0;
	}
	/* number of slots is 2^indexsize */
	ogl_nslots = 1<<ogl_index_size;
	if( ogl_nslots > NSLOTS )  ogl_nslots = NSLOTS;
	if(cueing_on) {
		/* peel off reserved ones */
		ogl_nslots = (ogl_nslots - CMAP_BASE) / CMAP_RAMP_WIDTH;
		glEnable(GL_FOG);
	} else {
		ogl_nslots -= CMAP_BASE;	/* peel off the reserved entries */
		glDisable(GL_FOG);
	}

	ovec = -1;	/* Invalidate the old colormap entry */

	/* apply region-id based colors to the solid table */
	color_soltab();

	/* Map the colors in the solid table to colormap indices */
	ogl_colorit();

	for( i=0; i < slotsused; i++ )  {
		Ogl_gen_color( i, ogl_rgbtab[i].r, ogl_rgbtab[i].g, ogl_rgbtab[i].b);
	}

	glIndexi( MAP_ENTRY(DM_WHITE) );
#endif
}

/* ARGSUSED */
void
Ogl_debug(lvl)
{
	ogl_debug = lvl;
	XFlush(dpy);
	bu_log("flushed\n");
}

void
Ogl_window(w)
register int w[];
{
#if 0
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
#endif
}

/* the font used depends on the size of the window opened */
#define FONTBACK	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5	"5x7"
#define FONT6	"6x10"
#define FONT7	"7x13"
#define FONT8	"8x13"
#define FONT9	"9x15"

static int
Ogl_xsetup( name )
char	*name;
{
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
	XSizeHints *hints, gethints;
	long supplied;

	hints = XAllocSizeHints();

	/* this is important so that Ogl_configure_notify knows to set
	 * the font */
	fontstruct = NULL;

	width = height = 900;

	xinterp = Tcl_CreateInterp(); /* Dummy interpreter */
	xtkwin = Tk_CreateMainWindow(xinterp, name, "MGED", "MGED");

	/* Open the display - XXX see what NULL does now */
	if( xtkwin == NULL ) {
		bu_log( "dm-X: Can't open X display\n" );
		return -1;
	}
	dpy = Tk_Display(xtkwin);

#if 0
	/*CJXX temporary */
	if (glXQueryExtension(dpy, NULL, NULL)){
		printf("glX extension exists\n");
		glXQueryVersion(dpy, &major, &minor);
		printf("version %d.%d\n", major, minor);
	} else {
		printf("glX extension doesn't exist\n");
	}
#endif

	/* must do this before Make Exist */

	if ((vip=ogl_set_visual(xtkwin))==NULL){
		bu_log("Ogl_open: Can't get an appropriate visual.\n");
		return -1;
	}

#if 0
	if (vip->class == PseudoColor){
		int npixels = 4096;
		unsigned long pixels[4096];

		while(!XAllocColorCells(dpy, cmap, 0, NULL, 0, pixels, npixels)){
			npixels /= 2;
		}
		printf("Allocated %d colorcells.\n",npixels);
	}
#endif

	Tk_GeometryRequest(xtkwin, width+10, height+10);
	Tk_MoveToplevelWindow(xtkwin, 376, 0);
	Tk_MakeWindowExist(xtkwin);

	win = Tk_WindowId(xtkwin);

	a_screen = Tk_ScreenNumber(xtkwin);

	/* open GLX context */
	/* If the sgi display manager has been used, then we must use
	 * an indirect context. Otherwise use direct, since it is usually
	 * faster.
	 */
	if ((glxc = glXCreateContext(dpy, vip, 0, ogl_sgi_used ? GL_FALSE : GL_TRUE))==NULL) {
		bu_log("Ogl_open: couldn't create glXContext.\n");
		return -1;
	}
	/* If we used an indirect context, then as far as sgi is concerned,
	 * gl hasn't been used.
	 */
	ogl_is_direct = (char) glXIsDirect(dpy, glxc);
	bu_log("Using %s OpenGL rendering context.\n", ogl_is_direct ? "a direct" : "an indirect");
	/* set ogl_ogl_used if the context was ever direct */
	ogl_ogl_used = (ogl_is_direct || ogl_ogl_used);

	/* CJXX We may not want color map indices **/
	/* In fact, lets rewrite code assuming rgba, and add index support
	 * later if we feel it's worthwhile
	 */
#if 0
	if (!ogl_has_rgb){
		/* Get color map inddices for the colors we use. */
		black = BlackPixel( dpy, a_screen );
		white = WhitePixel( dpy, a_screen );

		a_cmap = Tk_Colormap(xtkwin);
		a_color.red = 255<<8;
		a_color.green=0;
		a_color.blue=0;
		a_color.flags = DoRed | DoGreen| DoBlue;
		if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
			bu_log( "dm-X: Can't Allocate red\n");
			return -1;
		}
		red = a_color.pixel;
		if ( red == white ) red = black;

		a_color.red = 200<<8;
		a_color.green=200<<8;
		a_color.blue=0<<8;
		a_color.flags = DoRed | DoGreen| DoBlue;
		if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
			bu_log( "dm-gl: Can't Allocate yellow\n");
			return -1;
		}
		yellow = a_color.pixel;
		if (yellow == white) yellow = black;

		a_color.red = 0;
		a_color.green=0;
		a_color.blue=255<<8;
		a_color.flags = DoRed | DoGreen| DoBlue;
		if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
			bu_log( "dm-gl: Can't Allocate blue\n");
			return -1;
		}
		blue = a_color.pixel;
		if (blue == white) blue = black;

		a_color.red = 128<<8;
		a_color.green=128<<8;
		a_color.blue= 128<<8;
		a_color.flags = DoRed | DoGreen| DoBlue;
		if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
			bu_log( "dm-gl: Can't Allocate gray\n");
			return -1;
		}
		gray = a_color.pixel;
		if (gray == white) gray = black;

		/* Select border, background, foreground colors,
		 * and border width.
		 */
		/* if( a_visual->class == GrayScale || a_visual->class == StaticGray )  */
		if( vip->class == GrayScale || vip->class == StaticGray )  {
			is_monochrome = 1;
			bd = BlackPixel( dpy, a_screen );
			bg = WhitePixel( dpy, a_screen );
			fg = BlackPixel( dpy, a_screen );
		} else {
			/* Hey, it's a color server.  Ought to use 'em! */
			is_monochrome = 0;
			bd = WhitePixel( dpy, a_screen );
			bg = BlackPixel( dpy, a_screen );
			fg = WhitePixel( dpy, a_screen );
		}

		if( !is_monochrome && fg != red && red != black )  fg = red;

		gcv.foreground = fg;
		gcv.background = bg;
	}
#endif

	/* Register the file descriptor with the Tk event handler */
#if 0
	Tk_CreateEventHandler(xtkwin, ExposureMask|ButtonPressMask|KeyPressMask|
			  PointerMotionMask
			  |StructureNotifyMask|FocusChangeMask,
			  (void (*)())Ogldoevent, (ClientData)NULL);

#else
	Tk_CreateGenericHandler(Ogldoevent, (ClientData)NULL);
#endif

	Tk_SetWindowBackground(xtkwin, bg);


	if (!glXMakeCurrent(dpy, win, glxc)){
		bu_log("Ogl_open: Couldn't make context current\n");
		return -1;
	}

	/* display list (fontOffset + char) will displays a given ASCII char */
	if ((fontOffset = glGenLists(128))==0){
		bu_log("dm-gl: Can't make display lists for font.\n");
		return -1;
	}

	Tk_MapWindow(xtkwin);


	/* Keep the window square */
	hints->min_aspect.x = 1;
	hints->min_aspect.y = 1;
	hints->max_aspect.x = 1;
	hints->max_aspect.y = 1;
	hints->flags = PAspect;
	XSetWMNormalHints(dpy, win, hints);
	XFree(hints);

	/* CJXX? */
	/* Don't draw polygon edges */

	/* CJXX? */
	/* Take off a smidgeon for wraparound, as suggested by SGI manual */
	min_scr_z = 0;
	max_scr_z = 1;

	/* do viewport, ortho commands and initialize font*/
	Ogl_configure_window_shape();

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple( 1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	if (ogl_has_rgb)
		glFogfv(GL_FOG_COLOR, backgnd);
	glFogf(GL_FOG_DENSITY, VIEWFACTOR);


	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); /* object transformation matrix */
	glTranslatef( 0.0, 0.0, -1.0); /* new */
	glPushMatrix();
	glLoadIdentity();
	ogl_face_flag = 1;	/* faceplate matrix is on top of stack */

	return 0;
}

/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
XVisualInfo *
ogl_set_visual(tkwin)
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
					ogl_has_dbfr = m_double;
					ogl_has_doublebuffer = m_double;
					glXGetConfig(dpy, maxvip, GLX_DEPTH_SIZE, &ogl_depth_size);
					if (ogl_depth_size > 0)
						ogl_has_zbuf = 1;
					ogl_has_rgb = m_rgba;
					if (!m_rgba){
						glXGetConfig(dpy, maxvip, GLX_BUFFER_SIZE, &ogl_index_size);
					}
					ogl_has_stereo = m_stereo;
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
			bu_log("Stereo not available.\n");
		} else if (m_rgba) {
			m_rgba = 0;
			bu_log("RGBA not available.\n");
		} else if (m_double) {
			m_double = 0;
			bu_log("Doublebuffering not available. \n");
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
Ogl_configure_window_shape()
{
	int		npix;
	GLint mm;
	XWindowAttributes xwa;
	XFontStruct	*newfontstruct;

	xlim_view = 1.0;
	ylim_view = 1.0;
	MAT_IDN(aspect_corr);

#if 1
	XGetWindowAttributes( dpy, win, &xwa );
	height = xwa.height;
	width = xwa.width;
#else
	width = Tk_Width(xtkwin);
	height = Tk_Height(xtkwin);
#endif

	glViewport(0,  0, (width), (height));
	glScissor(0,  0, ( width)+1, ( height)+1);

/* CJXX needed? */
#if 1
	if( ogl_has_zbuf ) establish_zbuffer();
	establish_lighting();
#endif

#if 0
	glDrawBuffer(GL_FRONT_AND_BACK);

	glClearColor(0.0, 0.0, 0.0, 0.0);
/*	glClearDepth(0.0);*/
	if (ogl_has_zbuf)
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		glClear( GL_COLOR_BUFFER_BIT);

	if (ogl_has_doublebuffer)
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


#if 0
	/* CJXX not used */
	/* The glOrtho call really just makes this matrix */
	aspect_corr[0] = 1/xlim_view;
	aspect_corr[5] = 1/ylim_view;
#endif


	/* First time through, load a font or quit */
	if (fontstruct == NULL) {
		if ((fontstruct = XLoadQueryFont(dpy, FONT9)) == NULL ) {
			/* Try hardcoded backup font */
			if ((fontstruct = XLoadQueryFont(dpy, FONTBACK)) == NULL) {
				bu_log( "dm-gl: Can't open font '%s' or '%s'\n", FONT9, FONTBACK );
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
Ogl_dm(argc, argv)
int	argc;
char	**argv;
{
	struct bu_vls	vls;

	if( argc < 1 )  return -1;

	/* For now, only "set" command is implemented */
	if( strcmp( argv[0], "set" ) != 0 )  {
		bu_log("dm: command is not 'set'\n");
		return CMD_BAD;
	}

	bu_vls_init(&vls);
	if( argc < 2 )  {
		/* Bare set command, print out current settings */
		bu_structprint("dm_gl internal variables", Ogl_vparse, (char *)0 );
		bu_log("%s", bu_vls_addr(&vls) );
	} else if( argc == 2 ) {
	        bu_vls_struct_item_named( &vls, Ogl_vparse, argv[1], (char *)0, ',');
		bu_log( "%s\n", bu_vls_addr(&vls) );
  	} else {
	        bu_vls_printf( &vls, "%s=\"", argv[1] );
	        bu_vls_from_argv( &vls, argc-2, argv+2 );
		bu_vls_putc( &vls, '\"' );
		bu_structparse( &vls, Ogl_vparse, (char *)0 );
	}
	bu_vls_free(&vls);
	return CMD_OK;
}

void
establish_lighting()
{
	if (!lighting_on) {
		/* Turn it off */
		glDisable(GL_LIGHTING);
	} else {
		/* Turn it on */

		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

/* CJXX positions specified in Ogl_newrot */
#if 0
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
		glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
		glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
		glLightfv(GL_LIGHT3, GL_POSITION, light3_position);
		glPopMatrix();
#endif

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

#if 0
/*
 *  Some initial lighting model stuff
 *  Really, MGED needs to derive it's lighting from the database,
 *  but for now, this hack will suffice.
 *
 *  For materials, the definitions are:
 *	ALPHA		opacity.  1.0=opaque
 *	AMBIENT		ambient reflectance of the material  0..1
 *	DIFFUSE		diffuse reflectance of the material  0..1
 *	SPECULAR	specular reflectance of the material  0..1
 *	EMISSION	emission color ???
 *	SHININESS	specular scattering exponent, integer 0..128
 */
static float material_default[] = {
	ALPHA,		1.0,
	AMBIENT,	0.2, 0.2, 0.2,
	DIFFUSE,	0.8, 0.8, 0.8,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	0.0,
	SPECULAR,	0.0, 0.0, 0.0,
	LMNULL   };

/* Something like the RT default phong material */
static float material_rtdefault[] = {
	ALPHA,		1.0,
	AMBIENT,	0.2, 0.2, 0.2,	/* 0.4 in rt */
	DIFFUSE,	0.6, 0.6, 0.6,
	SPECULAR,	0.2, 0.2, 0.2,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	10.0,
	LMNULL   };

/* This was the "default" material in the demo */
static float material_xdefault[] = {
	AMBIENT, 0.35, 0.25,  0.1,
	DIFFUSE, 0.1, 0.5, 0.1,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 5.0,
	LMNULL   };

static float mat_brass[] = {
	AMBIENT, 0.35, 0.25,  0.1,
	DIFFUSE, 0.65, 0.5, 0.35,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 5.0,
	LMNULL   };

static float mat_shinybrass[] = {
	AMBIENT, 0.25, 0.15, 0.0,
	DIFFUSE, 0.65, 0.5, 0.35,
	SPECULAR, 0.9, 0.6, 0.0,
	SHININESS, 10.0,
	LMNULL   };

static float mat_pewter[] = {
	AMBIENT, 0.0, 0.0,  0.0,
	DIFFUSE, 0.6, 0.55 , 0.65,
	SPECULAR, 0.9, 0.9, 0.95,
	SHININESS, 10.0,
	LMNULL   };

static float mat_silver[] = {
	AMBIENT, 0.4, 0.4,  0.4,
	DIFFUSE, 0.3, 0.3, 0.3,
	SPECULAR, 0.9, 0.9, 0.95,
	SHININESS, 30.0,
	LMNULL   };

static float mat_gold[] = {
	AMBIENT, 0.4, 0.2, 0.0,
	DIFFUSE, 0.9, 0.5, 0.0,
	SPECULAR, 0.7, 0.7, 0.0,
	SHININESS, 10.0,
	LMNULL   };

static float mat_shinygold[] = {
	AMBIENT, 0.4, 0.2,  0.0,
	DIFFUSE, 0.9, 0.5, 0.0,
	SPECULAR, 0.9, 0.9, 0.0,
	SHININESS, 20.0,
	LMNULL   };

static float mat_plaster[] = {
	AMBIENT, 0.2, 0.2,  0.2,
	DIFFUSE, 0.95, 0.95, 0.95,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 1.0,
	LMNULL   };

static float mat_redplastic[] = {
	AMBIENT, 0.3, 0.1, 0.1,
	DIFFUSE, 0.5, 0.1, 0.1,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_greenplastic[] = {
	AMBIENT, 0.1, 0.3, 0.1,
	DIFFUSE, 0.1, 0.5, 0.1,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_blueplastic[] = {
	AMBIENT, 0.1, 0.1, 0.3,
	DIFFUSE, 0.1, 0.1, 0.5,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_greenflat[] = {
	EMISSION,   0.0, 0.4, 0.0,
	AMBIENT,    0.0, 0.0, 0.0,
	DIFFUSE,    0.0, 0.0, 0.0,
	SPECULAR,   0.0, 0.6, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_greenshiny[]= {
	EMISSION, 0.0, 0.4, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.25, 0.9, 0.25,
	SHININESS, 10.0,
	LMNULL
};

static float mat_blueflat[] = {
	EMISSION, 0.0, 0.0, 0.4,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.0, 0.5, 0.5,
	SPECULAR,  0.0, 0.0, 0.9,
	SHININESS, 10.0,
	LMNULL
};

static float mat_blueshiny[] = {
	EMISSION, 0.0, 0.0, 0.6,
	AMBIENT,  0.1, 0.25, 0.5,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_redflat[] = {
	EMISSION, 0.60, 0.0, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 1.0,
	LMNULL
};

static float mat_redshiny[] = {
	EMISSION, 0.60, 0.0, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_beigeshiny[] = {
	EMISSION, 0.5, 0.5, 0.6,
	AMBIENT,  0.35, 0.35, 0.0,
	DIFFUSE,  0.5, 0.5, 0.0,
	SPECULAR,  0.5, 0.5, 0.0,
	SHININESS, 10.0,
	LMNULL
};

/*
 *  Meanings of the parameters:
 *	AMBIENT		ambient light associated with this source ???, 0..1
 *	LCOLOR		light color, 0..1
 *	POSITION	position of light.  w=0 for infinite lights
 */
static float default_light[] = {
	AMBIENT,	0.0, 0.0, 0.0,
	LCOLOR,		1.0, 1.0, 1.0,
	POSITION,	0.0, 0.0, 1.0, 0.0,
	LMNULL};


static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.70, 0.70, 0.70,
	POSITION, 100.0, 200.0, 100.0, 0.0,
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.6, 0.1, 0.1,
	POSITION, 100.0, 30.0, 100.0, 0.0,
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.3, 0.1,
	POSITION, -100.0, 20.0, 20.0, 0.0,
	LMNULL};


static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.1, 0.3,
	POSITION, 0.0, -100.0, -100.0, 0.0,
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 1.0, 0.0,
	LCOLOR,   0.75, 0.75, 0.75,
	POSITION, 0.0, 10.0, 10.0, 5.0,
	LMNULL};





/*
 *  Lighting model parameters
 *	AMBIENT		amount of ambient light present in the scene, 0..1
 *	ATTENUATION	fixed and variable attenuation factor, 0..1
 *	LOCALVIEWER	1=eye at (0,0,0), 0=eye at (0,0,+inf)
 */
static float	default_lmodel[] = {
	AMBIENT,	0.2,  0.2,  0.2,
	ATTENUATION,	1.0, 0.0,
	LOCALVIEWER,	0.0,
	LMNULL};

static float infinite[] = {
	AMBIENT, 0.3,  0.3, 0.3,
	LOCALVIEWER, 0.0,
	LMNULL};

static float local[] = {
	AMBIENT, 0.3,  0.3, 0.3,
	LOCALVIEWER, 1.0,
	ATTENUATION, 1.0, 0.0,
	LMNULL};



#endif

void
establish_zbuffer()
{
	if( ogl_has_zbuf == 0 ) {
		bu_log("dm-gl: This machine has no Zbuffer to enable\n");
		zbuffer_on = 0;
	}

	if (zbuffer_on)  {
		glEnable(GL_DEPTH_TEST);
	} else {
		glDisable(GL_DEPTH_TEST);
	}
	dmaflag = 1;

	return;
}


void
ogl_colorit()
{
	register struct solid	*sp;
	register struct rgbtab *rgb;
	register int i;
	register int r,g,b;

	if( ogl_has_rgb )  return;

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


/*			G E N _ C O L O R
 *
 *	maps a given color into the appropriate colormap entry
 *	for both depthcued and non-depthcued mode.  In depthcued mode,
 *	Ogl_gen_color also generates the colormap ramps.  Note that in depthcued
 *	mode, DM_BLACK uses map entry 0, and does not generate a ramp for it.
 *	Non depthcued mode skips the first CMAP_BASE colormap entries.
 *
 *	This routine is not called at all if ogl_has_rgb is set.
 */
void
Ogl_gen_color(c)
int c;
{
	if(cueing_on) {

		/*  Not much sense in making a ramp for DM_BLACK.  Besides
		 *  which, doing so, would overwrite the bottom color
		 *  map entries, which is a no-no.
		 */
		if( c != DM_BLACK) {
			register int i, j;
			fastf_t r_inc, g_inc, b_inc;
			fastf_t red, green, blue;
			XColor cells[16];

			r_inc = ogl_rgbtab[c].r/16;
			g_inc = ogl_rgbtab[c].g/16;
			b_inc = ogl_rgbtab[c].b/16;

			red = ogl_rgbtab[c].r;
			green = ogl_rgbtab[c].g;
			blue = ogl_rgbtab[c].b;

			for(i = 15, j = MAP_ENTRY(c) + 15; i >= 0;
			    i--, j--, red -= r_inc, green -= g_inc, blue -= b_inc){
			    	cells[i].pixel = j;
			    	cells[i].red = (short)red;
			    	cells[i].green = (short)green;
			    	cells[i].blue = (short)blue;
			    	cells[i].flags = DoRed|DoGreen|DoBlue;
			}
			XStoreColors(dpy, cmap, cells, 16);
		}
	} else {
		XColor cell, celltest;

		cell.pixel = c + CMAP_BASE;
		cell.red = ogl_rgbtab[c].r;
		cell.green = ogl_rgbtab[c].g;
		cell.blue = ogl_rgbtab[c].b;
		cell.flags = DoRed|DoGreen|DoBlue;
		XStoreColor(dpy, cmap, &cell);

#if 0
		celltest.pixel = c + CMAP_BASE;
		XQueryColor(dpy, cmap, &celltest);
		printf("cell %d: %d %d %d\n", c + CMAP_BASE, celltest.red, celltest.green, celltest.blue);
#endif
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
