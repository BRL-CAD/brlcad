/*
 *			D M - X G L . C
 *
 *  An XGL 3.X interface for MGED for Sun Solaris 2.X systems.
 *
 *  Authors -
 *	Original XGL 2.0/OpenWindows 2.0:
 *		Saul Wold
 *		John Cummings  
 *	XGL 3.X/OpenWindows 3.X support and significant other enhancements:
 *		James D. Fiori
 *
 *  Source -
 *	Sun Microsystems, Inc.  
 *	Southern Area Special Projects Group
 *	6716 Alexander Bell Drive, Suite 200
 *	Columbia, MD 21046
 *  
 *  Copyright Notice -
 *	Copyright (c) 1994 Sun Microsystems, Inc. - All Rights Reserved.
 *
 *	Permission is hereby granted, without written agreement and without
 *	license or royalty fees, to use, copy, modify, and distribute this
 *	software and its documentation for any purpose, provided that the
 *	above copyright notice and the following two paragraphs appear in
 *	all copies of this software.
 *
 *	IN NO EVENT SHALL SUN MICROSYSTEMS INC. BE LIABLE TO ANY PARTY FOR
 *	DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING
 *	OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF SUN
 *	MICROSYSTEMS INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	SUN MICROSYSTEMS INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 *	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 *	AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER
 *	IS ON AN "AS IS" BASIS, AND SUN MICROSYSTEMS INC. HAS NO OBLIGATION TO
 *	PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *  
 * History -
 *	Modified 12/94 to support XGL 3.X by Sun Microsystems Computer Company
 *		XView code stripped out for the following reasons:
 *			XView is a 'dead' product
 *			Provide similar interface to X and SGI versions of mged
 *		Uses dm-X event processing code. Knob keys (x,y,z,X,Y,Z) were 
 *		 	fixed to perform "knob key .1"). Also, using the
 *			Control key with the knob keys performs "knob key -.1".
 *		Polygon surface color-fill support
 *		SunDials support
 *			The dials can be re-set (ie. "knob zero") by pressing 
 *			the '0' key.
 *		Z-Buffering (done in h/w for SX, ZX, s/w for others)
 *		24-bit color (for SX, ZX, S24)
 *		Lighting, Depth Cueing (for SX, ZX, S24)
 *		Anti-alias support for vectors (done in h/w for SX, ZX, s/w 
 *						for others)
 *		Double-Buffering Support:
 *			Performed in Hardware on TGX, SX, ZX
 *			Performed in Software: when using PEX
 *			Performed via colormap double-buffering on GX
 *			No double buffering on S24 (unless PEX is being used as
 *				is the case when running remotely)
 *		Function Key Support:
 *			F1 - Depth Cue Toggle
 *			F2 - Z-clip Toggle
 *			F3 - Perspective Toggle (currently not functional)
 *			F4 - Z-Buffer Toggle
 *			F5 - Lighting Toggle
 *			F6 - Perspective Angle Change (30, 45, 60, 90) 
 *						(currently not functional)
 *			F7 - Faceplate Toggle
 *			F8 - Line Anti-alias Toggle
 *
 * 	Known Problems:
 *		monochrome is not supported
 *		The GX amd TGX frame buffers are currently not being used in 
 *		DGA (Direct Graphics Access) mode. PEX is used instead.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

/*
 *	Header files
 */
#include "common.h"



#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"

#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

#include <xgl/xgl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Sunkeysym.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>

/*
 *	Degug macro
 */
				/* currently, only two debug "levels"	*/
				/* are recognized, ON and OFF;  may be	*/
				/* toggled by mged "redebug" command.	*/
static int		XGL_debug_level = 0;
#define	DPRINTF(S)	if(XGL_debug_level) bu_log(S)

/*
 *	NON-dm functions
 */
						/* Initialization	*/
static void		init_globals();			/* init globals */
static int		init_check_buffering();		/* parent setup */
static void		XGL_object_init();	
static void		init_sundials(), reset_sundials();
static void		handle_sundials_event(), reposition_light();
static void		setcolortables();		/* init colortables */

/*
 *	dm (display manager) package interface
 */
extern struct device_values	dm_values;	/* values read from devices */

extern void color_soltab();

#define PLOTBOUND       4000.0  /* Max magnification in Rot matrix */

int		XGL_open();
void		XGL_close();
MGED_EXTERN(void	XGL_input, (fd_set *input, int noblock) );
void		XGL_prolog(), XGL_epilog();
void		XGL_normal(), XGL_newrot();
void		XGL_update();
void		XGL_puts(), XGL_2d_line(), XGL_light();
int		XGL_object(), XGL_dm();
unsigned	XGL_cvtvecs(), XGL_load();
void		XGL_statechange(), XGL_viewchange(), XGL_colorchange();
void		XGL_window(), XGL_debug();

struct dm dm_XGL = {
	XGL_open,
	XGL_close,
	XGL_input,
	XGL_prolog, XGL_epilog,
	XGL_normal, XGL_newrot,
	XGL_update,
	XGL_puts, XGL_2d_line,
	XGL_light,
	XGL_object,
	XGL_cvtvecs, XGL_load,
	XGL_statechange,
	XGL_viewchange,
	XGL_colorchange,
	XGL_window, XGL_debug,
	0,				/* no displaylist */
	0,				/* multi-window */
	PLOTBOUND,
	"xgl", "XGL (X11)",
	0,
	XGL_dm,
};


/* X11 variables */
static Display  *display;               /* X display pointer */
static Window   xwin;                   /* X window we draw in */
static int	screen_num;		/* screen number */
static Atom	wm_protocols_atom;
static Atom	wm_delete_win_atom;
static unsigned char sundials_ev_type;


/*
 *	Global variables
 */
					/* miscellaneous */
static int      	height, width;		/* Maintain window dimensions 
						 * here */
static int		no_2d_flag;		/* ON - no display of 2d ctx */

					/* XGL		*/
static Xgl_sys_state	sys_state;		/* system state */
static Xgl_X_window	xgl_x_win;		/* x window */
static Xgl_obj_desc	win_desc;
static Xgl_win_ras	ras;			/* window raster */
static Xgl_3d_ctx	ctx_3d;			/* 3d context (for objects) */
static Xgl_2d_ctx	ctx_2d;			/* 2d context (text & lines) */
static Xgl_trans	trans;			/* transformation */

static Xgl_trans        itrans;

/*
 * Macro for flushing 3D polylines and polygons.
 */
#define Flush3D 	XGL_object_flush 

/*
 * Color Map info
 */
					/* XGL - color management */
static Xgl_cmap		cmap_A, cmap_B;		/* colormaps */
static Xgl_color_list	cmap_info_A, cmap_info_B; /* color lists */
static Xgl_color	color_table_A[256];	/* color tables */
static Xgl_color	color_table_B[256];
static Xgl_color_type	color_type;		/* INDEX or RGB */
static Xgl_color_index	last_color_index = 0xffffffff;

static int		dbuffering;		/* double buffering? */
static int		maxcolors;		/* max number of colors */
static int		slotsused;		/* used cmap index slots */
static int		colortablesize;		/* size of cmap index */
static int		current_buffer_is_A;	/* flag for cmap dbuffering */

static int      	no_faceplate = 0;

#define cmap		cmap_A			/* for clarity */
#define cmap_info	cmap_info_A
#define color_table	color_table_A

#define XGL_BUF_A	0	/* Used for double buffering */
#define XGL_BUF_B	1

#define	NO_DBUFF	0x0			/* OR'ed for value found */
#define	CMAP8_DBUFF	0x1				/* in dbuffering */
#define CMAP16_DBUFF	0x2
#define	AB_DBUFF	0x4

#define OPTION_ZERO	'0'			/* monochrome option */
#define OPTION_ONE	'1'			/* no dbuffering option */
#define OPTION_TWO	'2'			/* 8 color dbuffering */
#define OPTION_THREE	'3'			/* 16 color dbuffering */

#define DEFAULT_DB	OPTION_TWO		/* default type of dbuffering */
#define MONOCHROME	(colortablesize == 2)	/* monochrome frame buffer */

#define COLORSHIFT	(dbuffering + 2)	/* shift by COLORSHIFT */
#define CMAPDBUFFERING	(dbuffering & 0x3)	/* colormap dbuffering? */
#define COLORINDEX(ci)	(CMAPDBUFFERING ? (ci | ci << COLORSHIFT) : ci)
#define CMAP_MASK_A	(~(-1 << COLORSHIFT))	/* A BUFFER mask */
#define CMAP_MASK_B	(CMAP_MASK_A << COLORSHIFT) /* B BUFFER mask */

static Xgl_color_rgb BLACK	= {0.0, 0.0, 0.0 }; /* the 5 DM_ colors */
static Xgl_color_rgb RED	= {1.0, 0.0, 0.0 };
static Xgl_color_rgb BLUE	= {0.0, 0.0, 1.0 };
static Xgl_color_rgb YELLOW	= {1.0, 1.0, 0.0 };
static Xgl_color_rgb WHITE	= {1.0, 1.0, 1.0 };

#define MINSLOTS	5			/* number of DM_ colors */
#define	DEFAULT_COLOR	DM_YELLOW		/* default color */
#define SetColor(c, i) if (color_type == XGL_COLOR_INDEX) \
                                c.index = COLORINDEX(i); \
                       else \
                                c.rgb = color_table[i].rgb;

/* Light Info */
#define NUM_LIGHTS 2
static Xgl_light        lights[NUM_LIGHTS];
static Xgl_boolean      light_switches[NUM_LIGHTS];
static int		lighting_on = 0;	/* 1 = lighting is on */
static int		dcue_on = 0;		/* 1 = depth-cueing is on */
static int		zbuff_on = 0;		/* 1 = Z-buffer is on */
static int		zclip_on = 0;		/* 1 = Z is clipped */
static int		anti_alias_on = 0;	/* 1 = line A-A is on */
static void		init_lights();

/*
 * Flag that indicates what type of frame buffer we are running.
 */

typedef enum {
	FB_GX,
	FB_TGX,
	FB_SX,
	FB_ZX,
	FB_OTHER
} fb_type;

static fb_type frame_buffer_type = FB_OTHER;

static int X_setup( Display **, int *, Window *, int *, int *);
static int XGL_setup();
static void XGL_object_flush();
static void process_func_key(KeySym);
static void toggle_faceplate();
static void checkevents();
static void display_buff ();

#if 0	/* debug only */
static void start_ts();
static int stop_ts();
static int getms(struct timeval , struct timeval );
#endif

/*
 * Function Key Function Structures
 */
/* 'Borrowed' from dm-4d.c */
static int perspective_mode = 0;        /* Perspective flag */
static int perspective_table[] = {
        30, 45, 60, 90 };
#define NUM_PERSPECTIVE_ANGLES (sizeof (perspective_table)/sizeof(int))
static int perspective_angle = NUM_PERSPECTIVE_ANGLES - 1;
		        /* Angle of perspective */
	


/************************************************************************
 *									*
 * 	DM PACKAGE INTERFACE FUNCTIONS					*
 *									*
 ************************************************************************/

/*
 *	XGL_open
 */
XGL_open()
{

	DPRINTF("XGL_open\n");
	/*
	 * First thing we have to do is create an X11 window to pass to XGL
	 */
	init_globals();			/* initialize global variables */

	/* Init the X11 environment */
	if (X_setup(&display, &screen_num, &xwin, &width, &height))
		return (1);		/* Error */

	/* Init the XGL environment */
	if (XGL_setup())
		return (1);		/* Error */

	/* Init some data that XGL_object() needs */
	XGL_object_init();

	/* Set up for SunDials using the X Input Extensions */
	init_sundials();

	return (0);
}

/*
 * 	XGL_close
 */
void
XGL_close()
{
	DPRINTF("XGL_close\n");

	xgl_object_destroy (ctx_2d);
	xgl_object_destroy (ctx_3d);
	xgl_close(sys_state);
        XCloseDisplay( display );
	return;
}

/*
 *	XGL_prolog
 */
void
XGL_prolog()
{
	DPRINTF("XGL_prolog\n");

	if(!dmaflag || dbuffering)
		return;

    /* If we are NOT doing any double buffering, erase the window	*/
    /* BEFORE we begin to do any drawing.				*/
	xgl_context_new_frame(ctx_3d);
	xgl_context_new_frame(ctx_2d);

	return;
}

/*
 *	XGL_epilog
 */
void
XGL_epilog()
{

	DPRINTF("XGL_epilog\n");

	Flush3D();
	/*
	 * Place a "centering dot" in the center of the screen 
	 */
	XGL_2d_line( 0, 0, 0, 0, 0 );

	display_buff();	/* Display the screen and clear the hidden buffer */
	return;
}


static void
apply_matrix(mat_t mat)
{
	int i;
        Xgl_pt               	pt;
        Xgl_pt_d3d           	pt_d3d;
	double			w;
	static Xgl_matrix_d3d	xgl_mat;

	for (i = 0; i < 16; i++)
		xgl_mat[i/4][i%4] = (fabs(mat[i]) < 1.0e-15) ? 0.0 : mat[i];

	/*
	 * Write the matrix into the transform, then transpose it 
	 */
	xgl_transform_write_specific (trans, xgl_mat, NULL);
        xgl_transform_transpose(trans, trans);

	reposition_light(); /* reposition light source */
}

/*
 *	XGL_newrot
 */
void
XGL_newrot(mat)
mat_t mat;
{
	DPRINTF("XGL_newrot\n");

	apply_matrix(mat);
	return;
}

/*
 * 	XGL_object
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  If sp is null, it is being called from within this .c file
 *  for purposes of flushing ONLY!
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 *
 *  We use four Xgl_pt_list structures, one for solid polylines, one
 *  for dashed polylines, one for polygons without vertex normals, and one
 *  for polygons with vertex normals. The reason for much of this
 *  complexity is we buffer up polylines to enhance performance. We could have
 *  just drawn i polyline/solid, but with some Sun frame buffers, that's much
 *  slower than buffering up many.
 */
/*  First, define the array of point structures for regular lines, dashed lines,
 *  and polygons. Note that we use the 'flag' point type for polylines, since
 *  we need that to indicate whether or not to draw the line. The flag is only
 *  set for LINE_EDGE commands.
 */
#define MAX_PL_PTS	100000
static Xgl_pt_flag_f3d 	pl_solid_pts[MAX_PL_PTS];
static Xgl_pt_flag_f3d 	pl_dash_pts[MAX_PL_PTS];

#define MAX_PG_PTS	12
static Xgl_pt_f3d      	pg_pts[MAX_PG_PTS];		/* no vertext normals */
static Xgl_pt_normal_f3d pg_vnorm_pts[MAX_PG_PTS];	/* w/ vertext normals */

/* Next define the point list structures, one for each type. */
static Xgl_pt_list pl_solid_list = 	{XGL_PT_FLAG_F3D, 0, 0, 0, 0};
static Xgl_pt_list pl_dash_list = 	{XGL_PT_FLAG_F3D, 0, 0, 0, 0};
static Xgl_pt_list pg_list = 	  	{XGL_PT_F3D, 0, 0, 0, 0};
static Xgl_pt_list pg_vnorm_list = 	{XGL_PT_NORMAL_F3D, 0, 0, 0, 0};

/* Polygon data */


static Xgl_pt_f3d      	 *pg_ptr = pg_pts;	/* ptr into polygon point arr*/
static Xgl_pt_normal_f3d *pg_vnorm_ptr = pg_vnorm_pts;	/* ptr into polygon 
						           point arr w/ vertex
							   normals */
Xgl_facet_list		facet_list;		/* facet (contains color and
						   normal) */
Xgl_color_normal_facet	cn_facet;


/*
 * Macros for flushing the polyline and polygon lists
 */
 
#define FlushPlList() \
	if (pl_solid_list.num_pts) {\
		xgl_object_set(ctx_3d,XGL_CTX_LINE_STYLE,XGL_LINE_SOLID,0);\
		xgl_multipolyline(ctx_3d, NULL, 1, &pl_solid_list);\
		pl_solid_list.num_pts = 0;\
	}\
	if (pl_dash_list.num_pts) {\
		xgl_object_set(ctx_3d,XGL_CTX_LINE_STYLE,XGL_LINE_PATTERNED,0);\
		xgl_multipolyline(ctx_3d, NULL, 1, &pl_dash_list);\
		pl_dash_list.num_pts = 0; \
	}

#define FlushPgList() \
	if (pg_list.num_pts) {\
		xgl_multi_simple_polygon(ctx_3d,\
                        XGL_FACET_FLAG_SIDES_UNSPECIFIED |\
                        XGL_FACET_FLAG_SHAPE_NONCONVEX | \
			XGL_FACET_FLAG_FN_CONSISTENT, &facet_list,\
                        NULL, 1, &pg_list);\
		pg_list.num_pts = 0; pg_ptr = pg_pts;\
	}\
	if (pg_vnorm_list.num_pts) {\
		xgl_multi_simple_polygon(ctx_3d,\
                        XGL_FACET_FLAG_SIDES_UNSPECIFIED |\
                        XGL_FACET_FLAG_SHAPE_NONCONVEX | \
			XGL_FACET_FLAG_FN_CONSISTENT, &facet_list,\
                        NULL, 1, &pg_vnorm_list);\
		pg_vnorm_list.num_pts = 0; pg_vnorm_ptr = pg_vnorm_pts;\
	}

/* Macro for pulling out double points and shoving into local array for XGL */
#define GrabPts(xgl_ptr, pt) 	xgl_ptr->x = pt[0][0]; \
                                xgl_ptr->y = pt[0][1]; \
                                xgl_ptr->z = pt[0][2]; \
				xgl_ptr++;
	
/* Macro for bumping the number of polyline points, based on a dashed line */
#define BumpNumPts() \
		if (sp->s_soldash)\
			pl_dash_list.num_pts++;\
		else\
			pl_solid_list.num_pts++;

int
XGL_object(sp, mat, ratio, white)
struct solid *sp;
mat_t mat;
double ratio;
{
	static Xgl_color		solid_color;
	Xgl_color_index			color_index;
	struct rt_vlist         	*vp;
	Xgl_pt_flag_f3d			*flag_f3d_ptr;
	int				num_pts;
	int				vnorm_flag;

/*	DPRINTF("XGL_object\n");*/

	/* Set the line color (s_dmindex was set in XGL_colorchange) */
	if(MONOCHROME) 
		color_index = 1;
	else if(white) 
		color_index = DM_WHITE;
	else 
		color_index = sp->s_dmindex;

	if (color_index != last_color_index) {
		/* Changing colors, need to flush polylines and polygons */
		FlushPlList();
		FlushPgList();
		SetColor(solid_color, color_index);
		xgl_object_set(ctx_3d,
               		XGL_CTX_LINE_COLOR,    &solid_color,
               	 	0);
		last_color_index = color_index;
	}
	/* 
	 * Next, check to see if we filled up the polyline arrays. If so,
	 * flush. 
	 */
	if (sp->s_soldash) {
		num_pts = pl_dash_list.num_pts;
		if (num_pts + sp->s_vlen > MAX_PL_PTS)  {
			FlushPlList();
		}
		flag_f3d_ptr = &(pl_dash_list.pts.flag_f3d[num_pts]);
	} else {
		num_pts = pl_solid_list.num_pts;
		if (num_pts + sp->s_vlen > MAX_PL_PTS)  {
			FlushPlList();
		}
		flag_f3d_ptr = &(pl_solid_list.pts.flag_f3d[num_pts]);
	}
		
	for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		int    i;
		int    nused = vp->nused;
                int    *cmd = vp->cmd;
                point_t *pt = vp->pt;

                for( i = 0; i < nused; i++,cmd++,pt++ )  {
                        switch( *cmd )  {
                        case RT_VLIST_POLY_START:
				FlushPgList();
				/* Grab normal vector */
				cn_facet.normal.x = pt[0][0];
				cn_facet.normal.y = pt[0][1];
				cn_facet.normal.z = pt[0][2];
				cn_facet.color = solid_color;
				/* Assume no vertex normals coming */
				vnorm_flag = 0;
                                continue;
                        case RT_VLIST_POLY_MOVE:
                        case RT_VLIST_POLY_DRAW:
				if (vnorm_flag) {
					GrabPts(pg_vnorm_ptr, pt);
					pg_vnorm_list.num_pts++;
				} else {
					GrabPts(pg_ptr, pt);
					pg_list.num_pts++;
				}
				continue;
                        case RT_VLIST_POLY_END:
				FlushPgList();
				continue;
			case RT_VLIST_POLY_VERTNORM:
				/* Set flag indicating we have vert. normals */
				vnorm_flag = 1;
				pg_vnorm_ptr->normal.x = pt[0][0];
				pg_vnorm_ptr->normal.y = pt[0][1];
				pg_vnorm_ptr->normal.z = pt[0][2];
				/* The vertex comes next (POLY_MOVE/DRAW) */
				continue;
                        case RT_VLIST_LINE_MOVE:
					/* Don't draw edge */
				flag_f3d_ptr->flag = 0x00;
				GrabPts(flag_f3d_ptr, pt);
				BumpNumPts();
				continue;
                        case RT_VLIST_LINE_DRAW:
					/* Draw Edge */
                               	flag_f3d_ptr->flag = 0x01;
				GrabPts(flag_f3d_ptr, pt);
				BumpNumPts();
                                continue;
                        }
                }
	}
	return (1);
}
static void
XGL_object_flush()
{
	/* Flush any outstanding polylines or polygons */
	FlushPlList();
	FlushPgList();
	return;
}


/*
 * Init the 'pts' element of the 3 point lists
 */
static void 
XGL_object_init() {
	pl_solid_list.pts.flag_f3d = pl_solid_pts;
	pl_dash_list.pts.flag_f3d = pl_dash_pts;
	pg_list.pts.f3d = pg_pts;
	pg_vnorm_list.pts.normal_f3d = pg_vnorm_pts;

	facet_list.facet_type = XGL_FACET_COLOR_NORMAL;
	facet_list.num_facets = 1;
	facet_list.facets.color_normal_facets = &cn_facet;
}
	
/*
 *	XGL_normal
 *
 */
void
XGL_normal()
{
	DPRINTF("XGL_normal\n");

	return;
}

/*
 *	XGL_update
 */
void
XGL_update()
{
	DPRINTF("XGL_update\n");

	xgl_context_post(ctx_3d, FALSE);
	xgl_context_post(ctx_2d, FALSE);

	return;
}

/*
 *	XGL_puts
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
void
XGL_puts( str, x, y, size, color )
char 	*str;
int	x, y, size, color;
{
	Xgl_color	ln_color;
	Xgl_pt_f2d	pos;

	DPRINTF("XGL_puts\n");

	if(no_2d_flag) return;
	Flush3D();

	pos.x = x;
	pos.y = y;

	if (MONOCHROME)
		ln_color.index = 1;
	else
		SetColor(ln_color, color);

	xgl_object_set(ctx_2d,
		XGL_CTX_STEXT_COLOR,	&ln_color,
		XGL_CTX_STEXT_CHAR_HEIGHT,	45.,
		0);

	xgl_stroke_text_2d(ctx_2d, str, &pos);

	return;
}


/*
 *	XGL_2d_line
 */
void
XGL_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
	Xgl_pt_list	pl;
	Xgl_pt_f2d	pts[2];

	DPRINTF("XGL_2d_line\n");

	if(no_2d_flag) return;
	Flush3D();

	if(dashed) {
		xgl_object_set(ctx_2d,
			XGL_CTX_LINE_STYLE,	XGL_LINE_PATTERNED,
			XGL_CTX_LINE_PATTERN, 	xgl_lpat_dashed,
			0);
	} else {
		xgl_object_set(ctx_2d,
			XGL_CTX_LINE_STYLE,	XGL_LINE_SOLID,
			0);
	}
	pl.pt_type = XGL_PT_F2D;
	pl.bbox    = NULL;
	pl.num_pts = 2;
	pl.pts.f2d = pts;

	pts[0].x = x1;
	pts[0].y = y1;
	pts[1].x = x2;
	pts[1].y = y2;

	xgl_multipolyline(ctx_2d, NULL, 1, &pl);
	return;
}

/*
 *	XGL_input
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
void
XGL_input( input, noblock )
fd_set		*input;
int		noblock;
{

	struct timeval	tv;
	fd_set		files;
	int		width;
	int		cnt;
	register int	i;

#if defined(_SC_OPEN_MAX)
	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 )
#endif
		width = 32;

	files = *input;		/* save, for restore on each loop */
	FD_SET( display->fd, &files );	/* check X fd too */

	/*
	 * Check for input on the keyboard, mouse, or window system.
	 *
	 * Suspend execution until either
	 *  1)  User types a full line
	 *  2)  Mouse or Window input arrives
	 *  3)  The timelimit on SELECT has expired
	 *
	 * If a RATE operation is in progress (zoom, rotate, slew)
	 * in which we still have to update the display,
	 * do not suspend execution.
	 */
	do {
		*input = files;
		i = XPending( display );
		if( i > 0 ) {
			/* Don't select if we have some input! */
			FD_ZERO( input );
			FD_SET( display->fd, input );
			break;
			/* Any other input will be found on next call */
		}

		tv.tv_sec = 0;
		if( noblock )  {
			tv.tv_usec = 0;
		}  else  {
			/* 1/20th second */
			tv.tv_usec = 50000;
		}
		cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
		if( cnt < 0 )  {
			perror("dm-xgl.c/select");
			break;
		}
		if( noblock )  break;
		for( i=0; i<width; i++ )  {
			if( FD_ISSET(i, input) ) {
				noblock = 1;
				break;
			}
		}
	} while( noblock == 0 );

	if( FD_ISSET( display->fd, input ) )
		checkevents();

	return;
}

/* 
 *	XGL_light
 */
void
XGL_light( cmd, func )
int cmd;
int func;
{
	DPRINTF("XGL_light\n");

	return;
}

/*
 *	XGL_cvtvecs
 */
unsigned
XGL_cvtvecs(sp)
struct solid *sp;
{
	DPRINTF("XGL_ctvecs\n");

	return(0);
}


/*
 *	XGL_load
 */
unsigned
XGL_load( addr, count )
unsigned addr, count;
{
	DPRINTF("XGL_load\n");

	return 0;
}

/*
 *	XGL_statechange
 */
void
XGL_statechange(oldstate, newstate)
int	oldstate, newstate;
{
	DPRINTF("XGL_statechange\n");  /* .... found below	*/
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
	switch( newstate )  {
	case ST_VIEW:
		/* constant tracking OFF */
		XSelectInput( display, xwin, 
			ExposureMask|ButtonPressMask|
			KeyPressMask|StructureNotifyMask);
		break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
		/* constant tracking ON */
		XSelectInput( display, xwin, 
		  	PointerMotionMask|ExposureMask|ButtonPressMask|
			KeyPressMask|StructureNotifyMask );
		break;
	case ST_O_EDIT:
	case ST_S_EDIT:
		/* constant tracking OFF */
		XSelectInput( display, xwin, 
			ExposureMask|ButtonPressMask|
			KeyPressMask|StructureNotifyMask );
		break;
	default:
		bu_log("X_statechange: unknown state %s\n", 
			state_str[newstate]);
		break;
	}

	return;
}


/*
 *	XGL_viewchange
 */
void
XGL_viewchange(cmd, sp)
register int cmd;
register struct solid *sp;
{

	return;
}

/*
 *	XGL_colorchange
 */
void
XGL_colorchange()
{
	register struct solid	*sp;
	register int		i;
	register float		r, g, b;
	Xgl_color		bg_color;

	DPRINTF("XGL_colorchange\n");

	if(MONOCHROME)
		return;

	color_soltab();		/* apply colors to the solid table */

	slotsused = MINSLOTS;

	FOR_ALL_SOLIDS(sp) {
		r = (float)sp->s_color[0] / 255.0;
		g = (float)sp->s_color[1] / 255.0;
		b = (float)sp->s_color[2] / 255.0;

		if((r == 1.0 && g == 1.0 && b == 1.0) ||
		   (r == 0.0 && g == 0.0 && b == 0.0)) {
			sp->s_dmindex = DM_WHITE;
			continue;
		}

		for(i = 0; i < slotsused; i++) {
			if(color_table[i].rgb.r == r &&
			   color_table[i].rgb.g == g &&
			   color_table[i].rgb.b == b) {
				sp->s_dmindex = (short)i;
				break;
			}
		}
		if(i < slotsused)
			continue;

		if(slotsused < maxcolors) {
			color_table[slotsused].rgb.r = r;
			color_table[slotsused].rgb.g = g;
			color_table[slotsused].rgb.b = b;
			sp->s_dmindex = slotsused++;
			continue;
		}

		sp->s_dmindex = DEFAULT_COLOR;
	}

	for(i = slotsused; i < maxcolors; i++)
		color_table[i].rgb = RED;

	/* Invalidate the last color since the table may have changed */
	last_color_index = 0xffffffff;
		
	if (color_type == XGL_COLOR_RGB) 
		return;

	if(CMAPDBUFFERING) 
		setcolortables(color_table_A, color_table_B);
	xgl_object_set(cmap,
		XGL_CMAP_COLOR_TABLE,		&cmap_info,
		0);

	if(CMAPDBUFFERING) {
		xgl_object_set(cmap_B,
			XGL_CMAP_COLOR_TABLE,		&cmap_info_B,
			0);
	}

	SetColor(bg_color, DM_BLACK);
	xgl_object_set (ctx_3d, XGL_CTX_BACKGROUND_COLOR, &bg_color, NULL);
	xgl_object_set (ctx_2d, XGL_CTX_BACKGROUND_COLOR, &bg_color, NULL);
	return;
}

/*
 *	XGL_debug
 */
void
XGL_debug(level)
int	level;
{
	DPRINTF("XGL_debug\n");

	XGL_debug_level = level;
	return;
}

/*
 *	XGL_window
 */
void
XGL_window(windowbounds)
register int windowbounds[];
{
	Xgl_bounds_d3d	bounds_d3d;
	static int	lastwindowbounds[6];

	DPRINTF("XGL_window\n");

	if(memcmp((char *)windowbounds, (char *)lastwindowbounds,
	    (6 * sizeof(int)))) {
		bcopy((char *)windowbounds, (char *)lastwindowbounds,
		    (6 * sizeof(int)));

		bounds_d3d.xmax = windowbounds[0] / 2047.0;
		bounds_d3d.xmin = windowbounds[1] / 2048.0;

		bounds_d3d.ymax = windowbounds[2] / 2047.0;
		bounds_d3d.ymin = windowbounds[3] / 2048.0;

		bounds_d3d.zmax = windowbounds[4] / 2047.0;
		bounds_d3d.zmin = windowbounds[5] / 2048.0;	

		xgl_object_set(ctx_3d,
			XGL_CTX_VIEW_CLIP_BOUNDS, &bounds_d3d,
			0);

	}

	return;
}

int
XGL_dm( int argc, char ** argv)
{

	bu_log(stderr, "No commands implemented\n");
	return 0;
}



/************************************************************************
 *									*
 * 	INITIALIZATION							*
 *									*
 ************************************************************************/

static void
init_globals()
{
	width		= 512;
	height		= 512;
	no_2d_flag	= 0;
	dbuffering	= 0;
}

/********************************************************************/

static int
X_setup( Display **dpy, int *screen, Window *win, int *w, int *h)
{
	XVisualInfo	visual_info;
	Visual		*visual;
        XSizeHints 	xsh;
	int		depth;
	int		x, y;
	XEvent  	event;
	Colormap        xcmap;
        static XSetWindowAttributes xswa;
	static XWMHints xwmh = {
       	 	(InputHint|StateHint),          /* flags */
       	 	False,                          /* input */
       	 	NormalState,                    /* initial_state */
       	 	0,                              /* icon pixmap */
       	 	0,                              /* icon window */
       	 	0, 0,                           /* icon location */
       	 	0,                              /* icon mask */
       	 	0                               /* Window group */
	};


	/* Use the DISPLAY environment variable. If not set, it's an error */
	if ((*dpy = XOpenDisplay (NULL)) == NULL) {
		bu_log("Could not open X display %s\n", XDisplayName(NULL));
		return (-1);
	}

	*screen = DefaultScreen (*dpy);
	visual =  DefaultVisual (*dpy, *screen);
        depth = DefaultDepth (*dpy, *screen);
        xcmap = CopyFromParent;

	/* Make the window full screen */
	*w = DisplayWidth(*dpy, *screen);
	*h = DisplayHeight(*dpy, *screen);
	/*
	 * Since there is no ICCCM protocol for determining the size of the
	 * window decoration, let's assume we're using olwm.
	 */
#define WM_BORDER_HEIGHT 30
#define WM_BORDER_WIDTH  10
	*h -= WM_BORDER_HEIGHT;
	*w -= WM_BORDER_WIDTH;
	if (*w < *h)
		*h = *w;
	if (*h < *w)
		*w = *h;

	/* Position window at lower-right side of screen */
	x = DisplayWidth(*dpy, *screen) - *w - WM_BORDER_WIDTH;
	y = DisplayHeight(*dpy, *screen) - *h - WM_BORDER_HEIGHT;
	/* 
	 * If we have a 24-bit display, let's use that visual. Otherwise use
	 * the default visual
	 */
	if (depth != 24) {
		/* 
		 * If we find 1 24-bit visual, must create a colormap for
		 * that visual, or CreateWindow will fail
		 */
		if (XMatchVisualInfo(*dpy, *screen, 24, TrueColor, 
				&visual_info)) {
			xcmap = XCreateColormap(*dpy, RootWindow(*dpy,*screen),
				visual_info.visual, AllocNone);
			depth = 24;
			visual = visual_info.visual;
		}
		else if (XMatchVisualInfo(*dpy, *screen, 24, DirectColor, 
                                &visual_info)) {
			xcmap = XCreateColormap(*dpy, RootWindow(*dpy,*screen),
				visual_info.visual, AllocAll);
			depth = 24;
			visual = visual_info.visual;
		}

	}

	xswa.colormap = xcmap;
	xswa.border_pixel = BlackPixel(*dpy, *screen);
	xswa.background_pixel = BlackPixel(*dpy, *screen);

	*win = XCreateWindow (*dpy, DefaultRootWindow(*dpy),  x, y, *w, *h,
		1, depth, InputOutput, visual,
		CWBorderPixel |CWColormap | CWBackPixel, &xswa);

	if (xwin == NULL) {
		bu_log("Could not create X window\n");
		return (-1);
	}
	xsh.flags = (PSize | PPosition);
	xsh.height = *h;
	xsh.width =  *w;
	xsh.x = x;
	xsh.y = y;
	XSetStandardProperties(*dpy, *win, "MGED", "MGED", 
			None, NULL, 0, &xsh );
	XSetWMHints(*dpy, *win, &xwmh );
        XSelectInput(*dpy, *win, 
		ExposureMask|ButtonPressMask|
		KeyPressMask |StructureNotifyMask);

	/* Set up the DELETE_WINDOW handshaking */
	wm_protocols_atom = XInternAtom(*dpy, "WM_PROTOCOLS", False);
	wm_delete_win_atom = XInternAtom(*dpy, "WM_DELETE_WINDOW", True);
	if (wm_delete_win_atom == None || wm_protocols_atom == None) 
	    bu_log("Couldn't get WM_PROTOCOLS or WM_DELETE_WINDOW atom\n");
	else 
		XSetWMProtocols (*dpy, *win, &wm_delete_win_atom, 1);

        XMapWindow(*dpy, *win );
 
        while( 1 ) {
                XNextEvent(*dpy, &event );
                if( event.type == Expose && event.xexpose.count == 0 ) {
                        XWindowAttributes xwa;
 
                        /* remove other exposure events */
                        while( XCheckTypedEvent(*dpy, Expose, &event) ) ;
 
                        if( XGetWindowAttributes( *dpy, *win, &xwa ) == 0 )
                                break;
 
                        *w= xwa.width;
                        *h= xwa.height;
                        break;
                }
        }
        return  0;
}

static int 
XGL_setup()
{
	
	Xgl_bounds_d2d	bounds_d2d;
	Xgl_bounds_d3d	bounds_d3d;
	int		bufs;
       	Xgl_inquire     *inq_info;
	Xgl_color	ln_color, bg_color;
	
	xgl_x_win.X_display = display;
	xgl_x_win.X_window = xwin;
	xgl_x_win.X_screen = screen_num;

	win_desc.win_ras.type = XGL_WIN_X;
	win_desc.win_ras.desc = &xgl_x_win;

	sys_state = xgl_open(0);

       if (!(inq_info = xgl_inquire(sys_state, &win_desc))) {
          	bu_log("error in getting inquiry\n");
          	exit(1);
        }
	bufs = inq_info->maximum_buffer;	/* if double buffering, its 2 */

	if (strcmp(inq_info->name, "Sun:GX") == 0) {
		if (bufs > 1)
			/* 2 buffers,  We must have a TGX+ */
			frame_buffer_type = FB_TGX;
		else
			frame_buffer_type = FB_GX;
	} else if (strcmp(inq_info->name, "Sun:LEO") == 0)
		frame_buffer_type = FB_ZX;
	else if (strcmp(inq_info->name, "Sun:SX") == 0)
		frame_buffer_type = FB_SX;

	/*
	 * HACK City!!! 12-23-94
	 * 	The GX and TGX frame buffers have performance problems
	 * 	and clipping problems when using DGA that have not yet been 
	 *	resolved. Until they are, we use PEX or Xlib.
	 */
	if (frame_buffer_type == FB_GX || frame_buffer_type == FB_TGX) {
		free(inq_info);
		xgl_close(sys_state);
		win_desc.win_ras.type = XGL_WIN_X | XGL_WIN_X_PROTO_PEX |
			XGL_WIN_X_PROTO_XLIB ;
		sys_state = xgl_open(0);
		if (!(inq_info = xgl_inquire(sys_state, &win_desc))) {
			bu_log("error in getting inquiry\n");
			exit(1);
		}
	}

	bufs = inq_info->maximum_buffer;	/* if double buffering, its 2 */
	/*
	 * Check whether the frame buffer supports Z-buffering. If so, turn
	 * on Z-buffering by default.
	 */
	if (inq_info->hlhsr_mode == XGL_HLHSR_Z_BUFFER &&
	    inq_info->hlhsr == XGL_INQ_HARDWARE)
		zbuff_on = 1;

	free(inq_info);
	
    	/* ras MUST be created before init_check_buffering() is called */
    	ras = xgl_object_create(sys_state, XGL_WIN_RAS, &win_desc,
		NULL);

	if((dbuffering = init_check_buffering(bufs)) < 0)
		return(-1);

	if(MONOCHROME) {
		ln_color.index = 1;
	} else {
		cmap_info.start_index	= 0;
		cmap_info.length	= colortablesize;
		cmap_info.colors	= color_table;

		if(CMAPDBUFFERING) {
			cmap_info_B.start_index	= 0;
			cmap_info_B.length	= colortablesize;
			cmap_info_B.colors	= color_table_B;
		}

		color_table[DM_BLACK ].rgb = BLACK;
		color_table[DM_RED   ].rgb = RED;
		color_table[DM_BLUE  ].rgb = BLUE;
		color_table[DM_YELLOW].rgb = YELLOW;
		color_table[DM_WHITE ].rgb = WHITE;
		for(slotsused=MINSLOTS; slotsused < maxcolors; ++slotsused)
			color_table[slotsused].rgb = RED;
		slotsused = MINSLOTS;
		if(CMAPDBUFFERING)
			setcolortables(color_table_A, color_table_B);

		SetColor(ln_color, DM_YELLOW);
		SetColor(bg_color, DM_BLACK);
		if (color_type == XGL_COLOR_INDEX) {
		    cmap = xgl_object_create(sys_state, XGL_CMAP, NULL,
			XGL_CMAP_COLOR_TABLE_SIZE,	colortablesize,
			XGL_CMAP_COLOR_TABLE,		&cmap_info,
			0);

		    if(CMAPDBUFFERING) {
			cmap_B = xgl_object_create(sys_state, XGL_CMAP, NULL,
				XGL_CMAP_COLOR_TABLE_SIZE,	colortablesize,
				XGL_CMAP_COLOR_TABLE,		&cmap_info_B,
				0);
		    }
		    xgl_object_set(ras,
			XGL_DEV_COLOR_MAP,	cmap,
			0);
		}
	}

	bounds_d2d.xmin = -2048.0;
	bounds_d2d.xmax = 2050.0;	/* I'm not sure why this has to be */
	bounds_d2d.ymin = -2052.0;	/* hacked with 2050 and 2052;  but, */
	bounds_d2d.ymax = 2048.0;	/* that's what works */

	bounds_d3d.xmax = 1.;
	bounds_d3d.xmin = -1.;
	bounds_d3d.ymax = 1.;
	bounds_d3d.ymin = -1.;
	bounds_d3d.zmax = PLOTBOUND;
	bounds_d3d.zmin = -PLOTBOUND;	

	ctx_3d = xgl_object_create (sys_state, XGL_3D_CTX, NULL,
		XGL_CTX_DEVICE,		ras,
		XGL_CTX_LINE_COLOR,	&ln_color,
		XGL_CTX_VDC_MAP,	XGL_VDC_MAP_ASPECT,
		XGL_CTX_VDC_ORIENTATION, XGL_Y_UP_Z_TOWARD,
		XGL_CTX_VDC_WINDOW,	&bounds_d3d,
		XGL_CTX_CLIP_PLANES,	XGL_CLIP_XMIN | XGL_CLIP_YMIN |
					XGL_CLIP_XMAX | XGL_CLIP_YMAX,
		XGL_CTX_RENDERING_ORDER, XGL_RENDERING_ORDER_HLHSR,
		XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
                XGL_CTX_LINE_PATTERN,   xgl_lpat_dashed,
		XGL_CTX_LINE_STYLE,	XGL_LINE_SOLID,
		XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASTI,
		XGL_CTX_BACKGROUND_COLOR, &bg_color,
		XGL_CTX_SURF_FRONT_COLOR_SELECTOR, XGL_SURF_COLOR_FACET,
		0);

	init_lights();	/* Create and init the lights */

	ctx_2d = xgl_object_create(sys_state, XGL_2D_CTX, NULL,
			XGL_CTX_DEVICE,		ras,
			XGL_CTX_LINE_COLOR,	&ln_color,
			XGL_CTX_VDC_MAP,	XGL_VDC_MAP_ASPECT,
			XGL_CTX_VDC_ORIENTATION, XGL_Y_UP_Z_TOWARD,
			XGL_CTX_VDC_WINDOW,	&bounds_d2d,
			XGL_CTX_BACKGROUND_COLOR, &bg_color,
			0);
	if(CMAPDBUFFERING) {
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
		current_buffer_is_A = 0;
		xgl_object_set(ctx_3d, XGL_CTX_PLANE_MASK, CMAP_MASK_B, 0);
		xgl_object_set(ctx_2d, XGL_CTX_PLANE_MASK, CMAP_MASK_B, 0);
		xgl_object_set(ctx_3d,
                    XGL_CTX_NEW_FRAME_ACTION,
                        (XGL_CTX_NEW_FRAME_CLEAR | XGL_CTX_NEW_FRAME_VRETRACE |
			 XGL_CTX_NEW_FRAME_HLHSR_ACTION),
			NULL);
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
	} else if (dbuffering == AB_DBUFF) {
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
		xgl_object_set(ras,
		    XGL_WIN_RAS_BUF_DISPLAY,		XGL_BUF_A,
		    XGL_WIN_RAS_BUF_DRAW,		XGL_BUF_B,
		    0);
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
		xgl_object_set(ctx_3d,
		    XGL_CTX_NEW_FRAME_ACTION,
			(XGL_CTX_NEW_FRAME_CLEAR
			    | XGL_CTX_NEW_FRAME_SWITCH_BUFFER 
			    | XGL_CTX_NEW_FRAME_HLHSR_ACTION),
		    0);

	} else {
		/* No double buffering */
		xgl_object_set(ctx_3d,
                    XGL_CTX_NEW_FRAME_ACTION,
                        (XGL_CTX_NEW_FRAME_CLEAR | XGL_CTX_NEW_FRAME_VRETRACE |
			 XGL_CTX_NEW_FRAME_HLHSR_ACTION),
		    0);
	}

	/*
	 * Initialize the z-buffer by clearing the frame 
	 * (XGL_CTX_NEW_FRAME_HLHSR_ACTION is already turned on). Then
	 * turn z-buffering back off if we don't have HLHSR in hardware.
	 */
	xgl_context_new_frame(ctx_3d);
	xgl_context_new_frame(ctx_2d);
	if (!zbuff_on)
		xgl_object_set(ctx_3d, XGL_3D_CTX_HLHSR_MODE,XGL_HLHSR_NONE,0);

	/* Get the View transformation */

	xgl_object_get (ctx_3d, XGL_CTX_VIEW_TRANS, &trans);
	xgl_object_set (trans, XGL_TRANS_DATA_TYPE, XGL_DATA_DBL, NULL); 

	/* Create a transform that we'll use for the inverse of the VIEW */
	itrans = xgl_object_create(sys_state, XGL_TRANS, NULL,
		XGL_TRANS_DATA_TYPE, XGL_DATA_DBL,
		XGL_TRANS_DIMENSION, XGL_TRANS_3D,
		NULL);

	/*
	 * Set Backing store if we're not h/w double-bufferred (XGL won't
	 * support both double-buffer AND backing store
	 */
        if (dbuffering != AB_DBUFF) {
		static XSetWindowAttributes xswa;

                xswa.backing_store = Always;
                XChangeWindowAttributes (display, xwin,
                       	CWBackingStore , &xswa);
                xgl_object_set (ras, XGL_WIN_RAS_BACKING_STORE, TRUE,0);
        }
	return(0);
}

static int
init_check_buffering( int bufs)
{
	Xgl_cmap	cmap;
	Xgl_usgn32	maxsize;
	Xgl_usgn32	nbufs;
	int		type;

	type = NO_DBUFF;
	maxcolors = 128;
	colortablesize = 128;

	xgl_object_get(ras, XGL_RAS_COLOR_TYPE, &color_type);
	if (bufs > 1) {
		/* Let's try double buffering because the H/W says it exists */
		xgl_object_set(ras, XGL_WIN_RAS_BUFFERS_REQUESTED, bufs, 0);
	}
	xgl_object_get(ras, XGL_WIN_RAS_BUFFERS_ALLOCATED, &nbufs);
	xgl_object_get(ras, XGL_DEV_COLOR_MAP, &cmap);
	xgl_object_get(cmap, XGL_CMAP_MAX_COLOR_TABLE_SIZE, &maxsize);

	if(nbufs > 1) {
		bu_log("\nA total of %d buffers are supported by this\n", 
				nbufs);
		bu_log("hardware. Hardware double buffering will be used.\n");

		return(AB_DBUFF);
	} else if(maxsize == 2) {

		bu_log("\nThis is a MONOCHROME frame buffer. No double \n");
		bu_log("buffering of any type can be used.\n\n");

		maxcolors = 2;
		colortablesize = 2;
		return(NO_DBUFF);

	} else if(maxsize != 256) {
		bu_log("\nUnknown frame buffer type ... aborting.\n\n");
		return(-1);

	} else {
		if (color_type == XGL_COLOR_INDEX) {
			bu_log(
			   "\nUsing colormap double buffering, 8 bit color.\n");
			switch(DEFAULT_DB) {
			case OPTION_ZERO:
				type = NO_DBUFF;
				maxcolors = 2;
				colortablesize = 2;
				break;
		 	case OPTION_ONE:
				type = NO_DBUFF;
				maxcolors = 128;
				colortablesize = 128;
				break;
		  	case OPTION_TWO:
				type = CMAP8_DBUFF;
				maxcolors = 8;
				colortablesize = 64;
				break;
		  	case OPTION_THREE:
				type = CMAP16_DBUFF;
				maxcolors = 16;
				colortablesize = 256;
				break;
		  	default:
			       bu_log("Undefined DEFAULT_DB option,exiting\n");
			       exit(1);
			}
		} else {
			/* 
			 * We don't have HW double buffering AND we're a
			 * 24-bit frame buffer (XGL_COLOR_RGB). So let's
			 * not do double-buffering at all so we can do 24-bit
			 * colors for lighting, etc.
			 *
			 * type is already set to NO_DBUFF
			 */
		}
	}
	return(type);
}
/*
 *  Only called when we *know* there is at least one event to process.
 *  (otherwise we would block in XNextEvent)
 *
 *	NOTE: This code was 'lifted' right from the dm-X module!
 */
static void
checkevents()
{
	XEvent	event;
	KeySym	key;
	char keybuf[4];
	char cmd_buf[32];
	int cnt;
	XComposeStatus compose_stat;
	static int prev_width = -1, prev_height = -1;

	while( XPending( display ) > 0 ) {
		XNextEvent( display, &event );
		if (event.type == sundials_ev_type) {
			/* Somebody turned a dial! */
			handle_sundials_event(&event);
			continue;
		}
		switch(event.type) {
		case Expose:
			if( event.xexpose.count == 0 ) {
				/*
				 * If we're colormap double-buffering, we 
				 * need to work around a bug where the 
				 * window manager paints the background with
				 * BlackPixel, which corresponds to XGL red.
				 */
				if(CMAPDBUFFERING) {
					if (current_buffer_is_A) 
						display_buff();
				}
				if (frame_buffer_type == FB_TGX || 
				    CMAPDBUFFERING) {
					/* Clear the screen before refresh */
					display_buff();
				}
				dmaflag = 1;
			}
			break;
		case DestroyNotify:
			XGL_close();
			break;
		case ConfigureNotify:
			if (prev_width != event.xconfigure.width ||
			    prev_height != event.xconfigure.height ) {
				width = prev_width = event.xconfigure.width;
				height = prev_height = event.xconfigure.height;
				if(width < height) height = width;
				if(height < width) width  = height;

				xgl_window_raster_resize(ras);
			}
			if (frame_buffer_type == FB_TGX) {
				/* First, clear the screen. This is necessary
				 * because any area that was obscured will
				 * not refresh properly after a window move 
				 * on the TGX
				 */
				display_buff();
				dmaflag = 1;
			}
			break;
		case ClientMessage:
			if (event.xclient.message_type == wm_protocols_atom &&
			    event.xclient.data.l[0] == wm_delete_win_atom) {
					/* Must have quit from menu */
			    		bu_vls_printf( 
						&dm_values.dv_string, 
						"q\n");
			}
			break;

		case MotionNotify:
			{
			    int	x, y;
			    x = (event.xmotion.x/(double)width - 0.5) * 4095;
			    y = (0.5 - event.xmotion.y/(double)height) * 4095;
			    bu_vls_printf( &dm_values.dv_string, "M 0 %d %d\n",
			    	x, y );
			}
			break;
		case ButtonPress:
			/* There may also be ButtonRelease events */
		    {
			int	x, y;
			/* In MGED this is a "penpress" */
			x = (event.xbutton.x/(double)width - 0.5) * 4095;
			y = (0.5 - event.xbutton.y/(double)height) * 4095;
			switch( event.xbutton.button ) {
			case Button1:
			    /* Left mouse: Zoom out */
			    bu_vls_strcat( &dm_values.dv_string, "zoom 0.5\n");
			    break;
			case Button2:
			    /* Middle mouse, up to down transition */
			    bu_vls_printf( &dm_values.dv_string, "M 1 %d %d\n",
			    	x, y);
			    break;
			case Button3:
			    /* Right mouse: Zoom in */
			    bu_vls_strcat( &dm_values.dv_string, "zoom 2\n");
			    break;
			}
		    }
		    break;
		case ButtonRelease:
		    {
			int	x, y;
			x = (event.xbutton.x/(double)width - 0.5) * 4095;
			y = (0.5 - event.xbutton.y/(double)height) * 4095;
			switch( event.xbutton.button ) {
			case Button1:
			    /* Left mouse: Zoom out.  Do nothing more */
			    break;
			case Button2:
			    /* Middle mouse, down to up transition */
			    bu_vls_printf( &dm_values.dv_string, "M 0 %d %d\n",
			    	x, y);
			    break;
			case Button3:
			    /* Right mouse: Zoom in.  Do nothing more. */
			    break;
			}
		    }
		    break;
		case KeyPress:
		    {
			/*Turn these into MGED "buttonpress" or knob functions*/

		    	cnt = XLookupString(&event.xkey, keybuf, sizeof(keybuf),
						&key, &compose_stat);

			if (key >= XK_F1 && key <= XK_F12 || 
			    key == SunXK_F36 || key == SunXK_F37) 
				/* F36 and F37 are really F11 and F12! */
				process_func_key(key);
			else {
				switch( key ) {
				case '?':
					bu_log( "\nKey Help Menu:\n\
0	Zero 'knobs'\n\
x	Increase xrot\n\
y	Increase yrot\n\
z	Increase zrot\n\
X	Increase Xslew\n\
Y	Increase Yslew\n\
Z	Increase Zslew\n\
f	Front view\n\
t	Top view\n\
b	Bottom view\n\
l	Left view\n\
r	Right view\n\
R	Rear view\n\
3	35,25 view\n\
4	45,45 view\n\
F	Toggle faceplate\n\
" );
					break;
				case 'F':
					/* Toggle faceplate on/off */
					toggle_faceplate();
					break;
				case '0':
					reset_sundials();
					bu_vls_printf( &dm_values.dv_string,
						"knob zero\n" );
					break;
				case 'x':
				case 'y':
				case 'z':
				case 'X':
				case 'Y':
				case 'Z':
					/* 6 degrees per unit */
					/* 
					 * Knob keys:
					 *
					 * If CTRL key is pressed, move negative.
					 * Else move positive
					 */

					if (event.xkey.state & ControlMask)
						sprintf(cmd_buf, "knob %c -0.1\n",
							key);
					else
						sprintf(cmd_buf, "knob %c 0.1\n",
							key);
					bu_vls_printf( &dm_values.dv_string,
						cmd_buf);
					break;
				case 'f':
					bu_vls_strcat( &dm_values.dv_string,
						"press front\n");
					break;
				case 't':
					bu_vls_strcat( &dm_values.dv_string,
						"press top\n");
					break;
				case 'b':
					bu_vls_strcat( &dm_values.dv_string,
						"press bottom\n");
					break;
				case 'l':
					bu_vls_strcat( &dm_values.dv_string,
						"press left\n");
					break;
				case 'r':
					bu_vls_strcat( &dm_values.dv_string,
						"press right\n");
					break;
				case 'R':
					bu_vls_strcat( &dm_values.dv_string,
						"press rear\n");
					break;
				case '3':
					bu_vls_strcat( &dm_values.dv_string,
						"press 35,25\n");
					break;
				case '4':
					bu_vls_strcat( &dm_values.dv_string,
						"press 45,45\n");
					break;
				case XK_Shift_L:
				case XK_Shift_R:
				case XK_Control_L:
				case XK_Control_R:
				case XK_Caps_Lock:
				case XK_Shift_Lock:
					break;	/* ignore */
				default:
					bu_log(
					  "dm-X:The key '%c' is not defined\n",
						key);
					break;
				}
		    	}
		    }
		    break;
		
		case UnmapNotify:
		case MapNotify:
		    /* No need for action here - we'll get the Expose */
		    break;
		default:
			bu_log( "Unknown event type\n" );
		}
	}
}

/* 
 * Process the SunDials event
 *
 *	SunDials:
 *
 *	    BUTTON 0 (X rot/ADC X)		BUTTON 4 (X slew)
 *	    BUTTON 1 (Y rot/ADC Y)		BUTTON 5 (Y slew)
 *	    BUTTON 2 (Z rot/ang2)		BUTTON 6 (Z slew)
 *	    BUTTON 3 (unused/ang1)		BUTTON 7 (Scale)
 *
 *	From the XDeviceMotionEvent event:
 *		first_axis:	the dial number (0-7)
 *		axis_data[0]:	tick value
 *
 *	The SunDials device reports values in units of 1/64th of a degree.
 *	All values are deltas - there is no concept of absolute positions.
 *	Clockwise turns are reported as positive values, counter-clockwise
 *	as negative. The smallest value returned is 90 units, or a little 
 *	less than 1.5 degrees.
 *
 *	The mged 'knob' command for rotate and slew wants a floating point
 *	value from -1.0 to 1.0. The greater the absolute value of this number
 *	the larger the delta for the operation. So, what we do with the
 *	SunDials device, assuming we start at 0 (neutral) is treat a 
 *	counter-clockwise 360-degree turn as going from 0.0 to -1.0. Any
 *	further turn will be pegged at -1.0. Likewise a clockwise full turn
 *	is treated as going from 0.0 to 1.0.
 *
 *	Finally, we don't want to process each event because the granularity
 *	is too fine for a user to be able to bring the dial back to 0. So, we
 *	divide up the 0.0 to 1.0 values into an increment (eg. .04).
 *
 */

#define SUNDIALS_UNITS_PER_DEGREE 	64	/* 64 units per degree */
#define SUNDIALS_UNITS_PER_REVOLUTION 	(360*SUNDIALS_UNITS_PER_DEGREE)
#define SUNDIALS_NUM_DIALS		8
#define	SUNDIALS_INCR			.04

/* Maintain current unit count and value (-1.- to 1.0) for each dial */
static int current_units[8] = {0,0,0,0,0,0,0,0};
static float current_val[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
				/* negative to left, positive to the right */

static void
reset_sundials() 
{
	int	i;
	for (i = 0; i < SUNDIALS_NUM_DIALS; i++) {
		current_units[i] = 0;
		current_val[i] = 0.0;
	}
}

static void
handle_sundials_event(XDeviceMotionEvent *ev)
{

	float val;	/* -1.0 to 1.0 */
	int   setting;	/* -2048 to 2048 - needed by ADC commands */
	int	dial_num;

	dial_num = ev->first_axis;

	current_units[dial_num] += ev->axis_data[0];
	if (current_units[dial_num] > SUNDIALS_UNITS_PER_REVOLUTION) 
		current_units[dial_num] = SUNDIALS_UNITS_PER_REVOLUTION;
	if (current_units[dial_num] < -SUNDIALS_UNITS_PER_REVOLUTION) 
		current_units[dial_num] = -SUNDIALS_UNITS_PER_REVOLUTION;

	val = (float)current_units[dial_num] / 
			(float)SUNDIALS_UNITS_PER_REVOLUTION;

	/* if we're between -INCR and +INCR, just call it zero */
	if (val >= -SUNDIALS_INCR && val <= SUNDIALS_INCR)
		/* Close enough to 0 */
		val = 0.0;

	/* 
	 * Now, if the change in value is less than INCR, let's not send a
	 * command, UNLESS we've got a 0.0 and the previous value as not 0.0.
	 * This is so we can turn off (reset) the operation.
	 */
	if ((fabs (val - current_val[dial_num]) < SUNDIALS_INCR) &&
	   (!(val == 0.0 && current_val[dial_num] != 0.0)))
		/* delta too small to worry about */
		return;

	current_val[dial_num] = val;
	setting = val*2048;
	
	if (XGL_debug_level)
		bu_log( "val = %f, setting = %d\n",val, setting);

	switch (dial_num) {	/* first_axis is button number */
	case 0:
		if(adcflag)
		    bu_vls_printf( &dm_values.dv_string , "knob xadc %d\n",
					setting);
		else
		    bu_vls_printf( &dm_values.dv_string , "knob x %f\n", val);
		break;
	case 1:
		if(adcflag)
		    bu_vls_printf( &dm_values.dv_string , "knob yadc %d\n",
					setting);
		else
		    bu_vls_printf( &dm_values.dv_string , "knob y %f\n", val);
		break;
	case 2:
		if(adcflag)
		    bu_vls_printf( &dm_values.dv_string , "knob ang2 %d\n",
					setting);
		else
		    bu_vls_printf( &dm_values.dv_string , "knob z %f\n", val);
		break;
	case 3:
		if(adcflag)
		    bu_vls_printf( &dm_values.dv_string, "knob ang1 %d\n",
					setting);
		break;
	case 4:
		bu_vls_printf( &dm_values.dv_string , "knob X %f\n", val);
		break;
	case 5:
		bu_vls_printf( &dm_values.dv_string , "knob Y %f\n", val);
		break;
	case 6:
		if(adcflag)
		    bu_vls_printf( &dm_values.dv_string , "knob distadc %d\n",
					setting );
		else
		    bu_vls_printf( &dm_values.dv_string , "knob Z %f\n", val);
		break;
	case 7:
		bu_vls_printf( &dm_values.dv_string , "knob S %f\n", val);
		break;
	}
}

static void fk_depth_cue(), fk_zclip(), fk_perspective(), 
	   fk_zbuffering(), fk_lighting(), fk_p_angle(), fk_faceplate(),
	   fk_anti_alias(), fk_zero_knobs(), fk_nop();
static void 
process_func_key(KeySym key) 
{

	switch (key) {
	case XK_F1:
		fk_depth_cue();
		break;
	case XK_F2:
		fk_zclip();
		break;
	case XK_F3:
		fk_perspective();
		break;
	case XK_F4:
		fk_zbuffering();
		break;
	case XK_F5:
		fk_lighting();
		break;
	case XK_F6:
		fk_p_angle();
		break;
	case XK_F7:
		fk_faceplate();
		break;
	case XK_F8:
		fk_anti_alias();
		break;
	case XK_F9:
	case XK_F10:
	case XK_F11:
	case SunXK_F36:
		fk_nop();
		break;
	case XK_F12:
	case SunXK_F37:
		fk_zero_knobs();
		break;
	}
}
static void
fk_depth_cue()
{
	Xgl_color	dc_color;
	double zvals[2] = {1.0, -1.0};	/* Z-planes between which to scale
					 *  depth-cue */
	/*
	 * Use two scale factors, one for perspective amd one for non-
	 * perspective. We do this so the screen is not so dark with the
	 * perspective set
	 */
	float scale_factors_non_persp[2] = {1.0, 0.2}; 
	float scale_factors_persp[2]     = {1.0, 0.5};
	
	if (color_type == XGL_COLOR_INDEX) {
		bu_log(
		    "Depth-cue support not available for this frame buffer\n");
		return;
	}
	SetColor(dc_color, DM_BLACK);
	if (dcue_on == 0) {
		xgl_object_set(ctx_3d, 
			XGL_3D_CTX_DEPTH_CUE_MODE, XGL_DEPTH_CUE_SCALED,
			XGL_3D_CTX_DEPTH_CUE_REF_PLANES, zvals,
			XGL_3D_CTX_DEPTH_CUE_SCALE_FACTORS, 
			    perspective_mode ? scale_factors_persp :
					       scale_factors_non_persp,
			XGL_3D_CTX_DEPTH_CUE_INTERP, TRUE,
			XGL_3D_CTX_DEPTH_CUE_COLOR, &dc_color,
			NULL);
	} else {
		xgl_object_set(ctx_3d, 
			XGL_3D_CTX_DEPTH_CUE_MODE, XGL_DEPTH_CUE_OFF,
			NULL);

	}
	dcue_on = !dcue_on;
	dmaflag = 1;
}
static void
fk_zclip()
{
	Xgl_bounds_d3d	bounds_d3d ;

	bounds_d3d.xmax = 1.;
	bounds_d3d.xmin = -1.;
	bounds_d3d.ymax = 1.;
	bounds_d3d.ymin = -1.;
	if (zclip_on) {
		bounds_d3d.zmax = PLOTBOUND;
		bounds_d3d.zmin = -PLOTBOUND;	
	} else {
		bounds_d3d.zmax = 1.;
		bounds_d3d.zmin = -1.;	
	}
	xgl_object_set (ctx_3d, XGL_CTX_VDC_WINDOW, &bounds_d3d, NULL);
	zclip_on = !zclip_on;
	dmaflag = 1;
}
static void
fk_perspective()
{
	/* 'Borrowed' from dm-4d.c */
	perspective_mode = 1 - perspective_mode;
        bu_vls_printf( &dm_values.dv_string, "set perspective %d\n",
        	perspective_mode ? perspective_table[perspective_angle] : -1 );
	/*
	 * If depth-cueing is on, re-set the depth-cue scale factors to allow
	 * for more light so the screen is not too dark.
	 */
	if (dcue_on) {
		dcue_on = 0; /* fk_depth_cue() will turn the flag back on */
		fk_depth_cue();
	}
	dmaflag = 1;
}
static void
fk_zbuffering()
{

	if (zbuff_on == 0) {
		xgl_object_set (ctx_3d, 
			XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_Z_BUFFER,
			NULL);
	} else {
		xgl_object_set (ctx_3d, 
			XGL_3D_CTX_HLHSR_MODE, XGL_HLHSR_NONE,
			NULL);
	}
	zbuff_on = !zbuff_on;
	dmaflag = 1;
}
static void
fk_lighting()
{
	if (color_type == XGL_COLOR_INDEX) {
		bu_log(
		    "Lighting support not available for this frame buffer\n");
		return;
	}
	if (lighting_on == 0) {
		/*
		 * We specify illumination of per-vertex (gouraud shading) 
		 * since we now receive vertex normals.
		 */
		light_switches[0] = light_switches[1] = TRUE;
		xgl_object_set(ctx_3d,
			XGL_3D_CTX_LIGHT_SWITCHES, light_switches,
			XGL_3D_CTX_SURF_FRONT_ILLUMINATION, 
				XGL_ILLUM_PER_VERTEX,	/* gouraud shading */
			NULL);
	} else {
		light_switches[0] = light_switches[1] = FALSE;
		xgl_object_set(ctx_3d,
			XGL_3D_CTX_LIGHT_SWITCHES, light_switches,
			XGL_3D_CTX_SURF_FRONT_ILLUMINATION, XGL_ILLUM_NONE,
			NULL);
	}
	lighting_on = !lighting_on;
	dmaflag = 1;

}
static void
reposition_light()
{
	Xgl_pt_d3d      pos;
	Xgl_pt		pt;

	/*
	 * Position the light in the lower,left-hand corner. Since the position
	 * gets the VIEW transform applied to it, we must take it through the
	 * inverse of the current transform.
	 */
	
	pos.x = -PLOTBOUND;
	pos.y = -PLOTBOUND;
	pos.z = PLOTBOUND;
	pt.pt_type = XGL_PT_D3D;
	pt.pt.d3d = &pos;

	xgl_transform_invert (itrans, trans);
	xgl_transform_point(itrans, &pt);

	xgl_object_set (lights[1],
		XGL_LIGHT_POSITION, &pos,
		NULL);
}

static void
fk_p_angle()
{
	/* 'Borrowed' from dm-4d.c */
	/* toggle perspective matrix */
       	if (--perspective_angle < 0) 
		perspective_angle = NUM_PERSPECTIVE_ANGLES-1;
       	if(perspective_mode) 
		bu_vls_printf( &dm_values.dv_string,
       		"set perspective %d\n", perspective_table[perspective_angle] );
	dmaflag = 1;

}
static void
fk_faceplate()
{
	toggle_faceplate();
}

static void
fk_anti_alias()
{

	if (anti_alias_on == 0) {
		xgl_object_set(ctx_3d, 
			XGL_CTX_LINE_AA_BLEND_EQ, XGL_BLEND_ARBITRARY_BG,
			XGL_CTX_LINE_AA_FILTER_WIDTH,3, 
			XGL_CTX_LINE_AA_FILTER_SHAPE, XGL_FILTER_GAUSSIAN,
			NULL);
	} else {
		xgl_object_set(ctx_3d, 
			XGL_CTX_LINE_AA_BLEND_EQ, XGL_BLEND_NONE,
			XGL_CTX_LINE_AA_FILTER_WIDTH, 1,
			XGL_CTX_LINE_AA_FILTER_SHAPE, XGL_FILTER_GAUSSIAN,
			NULL);
	}
	anti_alias_on = !anti_alias_on;
	dmaflag = 1;
}

static void
fk_zero_knobs()
{
	reset_sundials();
	bu_vls_printf( &dm_values.dv_string,
		"knob zero\n" );
}

static void
fk_nop()
{
	bu_log("Function Key Undefined\n");
}

static void
toggle_faceplate()
{
	no_faceplate = !no_faceplate;
	bu_vls_strcat( &dm_values.dv_string,
		no_faceplate ?
		"set faceplate 0\n" :
		"set faceplate 1\n" );
}

/*************************************************************************
 *
 *	Lighting Code
 */

static void
init_lights()
{
	Xgl_color	lt_color;

	SetColor (lt_color, DM_WHITE);
	xgl_object_set (ctx_3d,
		XGL_3D_CTX_LIGHT_NUM, NUM_LIGHTS,
		XGL_3D_CTX_SURF_FACE_DISTINGUISH, FALSE,
		XGL_3D_CTX_SURF_FRONT_AMBIENT, 0.1,
		XGL_3D_CTX_SURF_FRONT_DIFFUSE, 0.6,
		XGL_3D_CTX_SURF_FRONT_SPECULAR, 0.5,
		XGL_3D_CTX_SURF_FRONT_LIGHT_COMPONENT,
			XGL_LIGHT_ENABLE_COMP_AMBIENT |
			XGL_LIGHT_ENABLE_COMP_DIFFUSE |
			XGL_LIGHT_ENABLE_COMP_SPECULAR,
		XGL_3D_CTX_SURF_FACE_CULL, XGL_CULL_BACK,
		0);

	xgl_object_get(ctx_3d, XGL_3D_CTX_LIGHTS, lights);
	xgl_object_get(ctx_3d, XGL_3D_CTX_LIGHT_SWITCHES, light_switches);
	xgl_object_set (lights[0],
		XGL_LIGHT_TYPE, XGL_LIGHT_AMBIENT,
		XGL_LIGHT_COLOR, &lt_color,
		NULL);
	xgl_object_set (lights[1],
		XGL_LIGHT_TYPE, XGL_LIGHT_POSITIONAL,
		XGL_LIGHT_ATTENUATION_1, 1.0,
		XGL_LIGHT_ATTENUATION_2, 0.0,
		XGL_LIGHT_COLOR, &lt_color,
		NULL);
	light_switches[0] = FALSE;
	light_switches[1] = FALSE;
	xgl_object_set(ctx_3d, XGL_3D_CTX_LIGHT_SWITCHES, light_switches, NULL);
}

/***********************************************************************
 *
 * SunDials Processing Code.
 *
 *	This code uses the X Input Extensions to receive X11 events from
 *	a SunDails dial box.
 */
static void
init_sundials()
{
	
	int          	major_code, minor_code, firsterr, num_dev;
	int		i,j;
	XDeviceInfo	*xdevlist;
   	XAnyClassPtr 	classInfo;
	XEventClass	ev_class;
	XID 		sundials_id;
	XDevice 	*sundials_device;

	/* First, be sure we have the X extension */
	if (!XQueryExtension(display, "XInputDeviceEvents",
			&major_code, &minor_code,
			&firsterr)) {
		return;	/* Nope. No need to report an error, we just won't get
			   events */
	}

	/* Next get the list of input devices supported */
	xdevlist = XListInputDevices(display, &num_dev);

	/* Look for "sundials" */
	if (num_dev > 0) {
		for (i = 0; i < num_dev; i++)
			if (strcmp(xdevlist[i].name,"sundials") == 0)
				break;
	}
	if (num_dev <= 0 || i == num_dev) {
		/* No sundials found - just quietly return */
		return;
	}

	/* Find the Valuator device (sundials) */
	classInfo = xdevlist[i].inputclassinfo;
        for (j = 0; j < xdevlist[i].num_classes; j++) {
            switch(classInfo->class) {
                case ValuatorClass:
                    if ( ((XValuatorInfo *)(classInfo))->num_axes == 8) {
			sundials_id = xdevlist[i].id;
			sundials_device = XOpenDevice(display, sundials_id);
			if (sundials_device == 0) 
				return;	/* Device must not be present */

			/* 
			 * Finally, get the event type to look for in the
			 * X event processing loop and register interest
			 * in these events.
			 */
			DeviceMotionNotify(sundials_device, 
				sundials_ev_type, ev_class);
			XSelectExtensionEvent(display, xwin, &ev_class, 1);
		    }
                break;
                case ButtonClass:
                    if ( ((XButtonInfo *)(classInfo))->num_buttons ==32)
                        ; /* This would be a SunButtons device */
                break;
            }
	    classInfo = (XAnyClassPtr)(((char *)classInfo) + classInfo->length);
        }
	XFreeDeviceList(xdevlist);
	reset_sundials();
}



/************************************************************************
 *									*
 * 	MISCELLANEOUS							*
 *									*
 ************************************************************************/


static void
setcolortables(color_table_A, color_table_B)
Xgl_color	*color_table_A, *color_table_B;
{
	register unsigned int	i;

	for(i = maxcolors; i < colortablesize; i++) {
		color_table_A[i].rgb.r = color_table_A[i & CMAP_MASK_A].rgb.r;
		color_table_A[i].rgb.g = color_table_A[i & CMAP_MASK_A].rgb.g;
		color_table_A[i].rgb.b = color_table_A[i & CMAP_MASK_A].rgb.b;
	}

	for(i = 0; i < colortablesize; i++) {
		color_table_B[i].rgb.r = color_table_A[i >> COLORSHIFT].rgb.r;
		color_table_B[i].rgb.g = color_table_A[i >> COLORSHIFT].rgb.g;
		color_table_B[i].rgb.b = color_table_A[i >> COLORSHIFT].rgb.b;
	}

	return;
}



static void
display_buff() {
    /* If we ARE doing double buffering, it's time to switch		*/
    /* to the buffer we just finished drawing into;   else, don't	*/
    /* do anything.							*/

	switch(dbuffering) {
	  case NO_DBUFF:
	  default:
		if (color_type == XGL_COLOR_INDEX)
			xgl_object_set(ras, XGL_DEV_COLOR_MAP, cmap, 0);
		return;

	  case CMAP8_DBUFF:
	  case CMAP16_DBUFF:
		if(current_buffer_is_A) {
		    xgl_object_set(ras, XGL_DEV_COLOR_MAP, cmap_A, 0);
		    xgl_object_set(ctx_2d, XGL_CTX_PLANE_MASK, CMAP_MASK_B, 0);
		    xgl_object_set(ctx_3d, XGL_CTX_PLANE_MASK, CMAP_MASK_B, 0);
		} else {
		    xgl_object_set(ras, XGL_DEV_COLOR_MAP, cmap_B, 0);
		    xgl_object_set(ctx_2d, XGL_CTX_PLANE_MASK, CMAP_MASK_A, 0);
		    xgl_object_set(ctx_3d, XGL_CTX_PLANE_MASK, CMAP_MASK_A, 0);
		}
		current_buffer_is_A ^= 1;
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
		break;
	  case AB_DBUFF:
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
		break;
	}
}

#if 0
/******************************************************************
 *
 *	Miscellaneous Debug Code
 */

static struct timeval tp1, tp2;
static void
start_ts() {
	gettimeofday(&tp1);
}
static int
stop_ts() {
	gettimeofday(&tp2);
	return (((tp2.tv_sec*1000000 + tp2.tv_usec) -
                 (tp1.tv_sec*1000000 + tp1.tv_usec))/1000 );
}
static int
getms(struct timeval tp1, struct timeval tp2)
{
	return (((tp2.tv_sec*1000000 + tp2.tv_usec) - 
		 (tp1.tv_sec*1000000 + tp1.tv_usec))/1000 );
}
dump_pl(Xgl_pt_list *pl, int n)
{
 
        int i, j;
 
        for (i = 0; i < n; i++) {
                bu_log("PL[%d]: (%d pts)\n",i, pl[i].num_pts);
                for (j = 0; j < pl[i].num_pts; j++) {
                        bu_log( "       pt[%d]: %e,%e,%e, %d\n",j,
                                pl[i].pts.flag_f3d[j].x,
                                pl[i].pts.flag_f3d[j].y,
                                pl[i].pts.flag_f3d[j].z,
                                pl[i].pts.flag_f3d[j].flag);
                }
        }
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
