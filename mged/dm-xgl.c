/*
 *			D M - X G L . C
 *
 *  An XGL interface for MGED on the Sun
 *  OpenWindows 2.0.
 *
 *  Authors -
 *	Saul Wold
 *	John Cummings  
 *  
 *  Source -
 *	SUN Microsystems, Inc.  
 *  	2550 Garcia Ave.
 *  	Mountain View, CA 94043
 *  
 *  Copyright Notices -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

/*
 *	Header files
 */
#include "conf.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include "machine.h"
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
#define __EXTENSIONS__
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/termsw.h>
#include <xview/canvas.h>
#include <xview/textsw.h>
#include <xview/tty.h>
#include <xview/cms.h>
#include <xview/attr.h>
#include <xview/cursor.h>
#undef __EXTENSIONS__

static unsigned short icon_bits[] = {
#include "./sunicon.h"
};

			/* struct button - describes one of our panel	*/
			/* 	buttons in convenient terms;  an array	*/
			/*	of these is used to describe all of 	*/
			/* 	our buttons.				*/
struct button {
	char		*label;			/* Panel_item label	*/
	char		*cmd;			/* associated mged cmd	*/
	int		(*button_proc)();	/* button procedure	*/
};


/*
 *	Degug macro
 */
				/* currently, only two debug "levels"	*/
				/* are recognized, ON and OFF;  may be	*/
				/* toggled by mged "redebug" command.	*/
static int		XGL_debug_level = 0;
#define	DPRINTF(S)	if(XGL_debug_level) (void)fprintf(stderr, S)




/*
 *	Command definitions for textsw child process
 */
#define	SET_POSITION	'\001'	/* re-position termsw - followed by 2 ints */
#define	SET_WIDTH	'\002'	/* set termsw width   - followed by 1 int  */
#define	SET_DISPLAY	'\003'	/* set termsw WIN_SHOW to true		   */

#define XGL_BUF_A	0	/* Used for double buffering */
#define XGL_BUF_B	1


/*
 *	Definitions for use with the F2-F8 function key buttons
 */
#define	ZOOM_BUTTON	2	/* F2 */
#define	X_SLEW_BUTTON	3	/* F3 */
#define	Y_SLEW_BUTTON	4	/* F4 */
#define	Z_SLEW_BUTTON	5	/* F5 */
#define	X_ROT_BUTTON	6	/* F6 */
#define	Y_ROT_BUTTON	7	/* F7 */
#define	Z_ROT_BUTTON	8	/* F8 */

#define	ZOOM_FLAG	0x01	/* These flags may be OR'ed in "sun_buttons" */
#define	X_SLEW_FLAG	0x02
#define	Y_SLEW_FLAG	0x04
#define	Z_SLEW_FLAG	0x08
#define	X_ROT_FLAG	0x10
#define	Y_ROT_FLAG	0x20
#define	Z_ROT_FLAG	0x40

int	sun_buttons = 0;	/* Which buttons are being held down?	    */



/*
 *	NON-dm functions
 */
						/* Initialization	*/
static void		init_globals();			/* init globals */
static void		init_create_icons();		/* create icons */
static void		init_termsw_child();		/* child setup */
static void		init_graphics_parent_xview();	/* parent setup */
static int		init_graphics_parent_xgl();	/* parent setup */
static int		init_check_buffering();		/* parent setup */
static int		init_editor();			/* set EDITOR */

						/* Miscellaneous	*/
static int		ok();				/* yes or no? */
static void		setcolortables();		/* init colortables */
static int		input_poll();			/* polling select */
static void		set_termsw_width();		/* set termsw width */
static void		set_termsw_position();		/* set termsw x y */
static void		set_termsw_display();		/* WIN_SHOW TRUE */

						/* Parent Xview functions */
static Notify_value	stop_func();			/* notify_stop() */
static Notify_value	destroy_func();			/* done */

static int		standard_button_proc();		/* cmd buttons */
static int		menu_button_proc();		/* menu button */
static int		detach_button_proc();		/* detach button */

static void		repaint_proc();			/* canvas repaint */
static void		resize_proc();			/* canvas resize */
static void		frame_event_proc();		/* frame event */
static void		pw_event_proc();		/* canvas event */
static void		set_sun_buttons();		/* canvas event */
static void		set_dm_values();		/* canvas event */

						/* Child Xview functions */
static Notify_value	copy_func();			/* I/O */
static Notify_value	signal_func();			/* pass signal */

/*
 *	dm (display manager) package interface
 */
extern struct device_values	dm_values;	/* values read from devices */


int		XGL_open();
void		XGL_close();
MGED_EXTERN(void	XGL_input, (fd_set *input, int noblock) );
void		XGL_prolog(), XGL_epilog();
void		XGL_normal(), XGL_newrot();
void		XGL_update();
void		XGL_puts(), XGL_2d_line(), XGL_light();
int		XGL_object();
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
	1000.0,
	"XGL", "XGL (X11)"
};



/*
 *	Global variables
 */
					/* miscellaneous */
static int		cmd_fd;			/* cmd file descriptor */
static int		pty_fd;			/* pty file descriptor */
static int		height, width;		/* canvas width & height */
static int		no_2d_flag;		/* ON - no display of 2d ctx */
static int		detached_flag;		/* ON - termsw is detached */
static int		mouse_tracking;		/* ON - we are tracing mouse */
static int		busy;			/* ON - busy cursor showing */
static int		xpen;			/* mouse/pen location (x) */
static int		ypen;			/* mouse/pen location (y) */

					/* Xview	*/
static Display		*display;		/* Display */
static Icon		icon;			/* Icon */
static Frame		frame;			/* Frame */
static Panel		panel;			/* Panel */
static Canvas		canvas;			/* Canvas */
static Xv_Window	pw;			/* Paint Window */
static Xv_Cursor	cursor_red;		/* Normal cursor */
static Xv_Cursor	cursor_busyred;		/* Busy cursor */
static Termsw		termsw;			/* Termsw (command window) */

					/* XGL		*/
static Xgl_sys_state	sys_state;		/* system state */
static Xgl_X_window	xgl_x_win;		/* x window */
static Xgl_obj_desc	win_desc;
static Xgl_win_ras	ras;			/* window raster */
static Xgl_3d_ctx	ctx_3d;			/* 3d context (for objects) */
static Xgl_2d_ctx	ctx_2d;			/* 2d context (text & lines) */
static Xgl_trans	trans;			/* transformation */

/*
 * Point List Arrays
 *
 *	We use on array for solid lines and 1 for dash lines. This allows
 *	us to batch the polylines into just a few calls to xgl_multipolyline.
 *	If we reach MAX_PT_LISTS point lists, we simply flush what we have and
 *	start another batch.
 */
#define MAX_PT_LISTS	2000
static Xgl_usgn32	num_pt_lists = 0;	/* current number in pl[] */
static Xgl_usgn32	num_pt_dash_lists = 0;	/* current number in pl_dash[]*/
#define PolyLinesOutstanding (num_pt_lists || num_pt_dash_lists)
static Xgl_pt_list	pl[MAX_PT_LISTS];    	
				/* for xgl_multipolyline (solid) */
static Xgl_pt_list	pl_dash[MAX_PT_LISTS];    
				/* for xgl_multipolyline (dash) */
#define FlushPolyLines() if(PolyLinesOutstanding) \
		XGL_object ((struct solid *)0, (float *)0, 0.0, 0)

					/* XGL - color management */
static Xgl_cmap		cmap_A, cmap_B;		/* colormaps */
static Xgl_color_list	cmap_info_A, cmap_info_B; /* color lists */
static Xgl_color	color_table_A[256];	/* color tables */
static Xgl_color	color_table_B[256];
static int		dbuffering;		/* double buffering? */
static int		maxcolors;		/* max number of colors */
static int		slotsused;		/* used cmap index slots */
static int		colortablesize;		/* size of cmap index */
static int		current_buffer_is_A;	/* flag for cmap dbuffering */
static int		win_visual_class;	/* TrueColor, etc */
static int		win_depth;		/* 8, 24, etc */
static Xgl_color_type	color_type;		/* INDEX or RGB */

#define cmap		cmap_A			/* for clarity */
#define cmap_info	cmap_info_A
#define color_table	color_table_A

#define	NO_DBUFF	0x0			/* OR'ed for value found */
#define	CMAP8_DBUFF	0x1				/* in dbuffering */
#define CMAP16_DBUFF	0x2
#define	AB_DBUFF	0x4

#define OPTION_ZERO	'0'			/* monochrome option */
#define OPTION_ONE	'1'			/* no dbuffering option */
#define OPTION_TWO	'2'			/* 8 color dbuffering */
#define OPTION_THREE	'3'			/* 16 color dbuffering */

/*
 * Flags that control the color type, double buffering, etc.
 *
 * DO_DOUBLE_BUFF:	If non-0, do hardware double buffering. 
 *
 * DO_DASH_LINES:	if non-0, we process dashed lines. This is only turned
 *			off for determining how much of a performance hit
 *			dashed lines cause (considerable on DGA!).
 *
 * DO_24_BIT:		if non-o, we use 24-bit color if available. 
 */
#define DO_DOUBLE_BUFF	1			/* flag to do h/w double buff.*/
#define DO_DASH_LINES	1			/* flag to do dash lines */							/* Dash lines slow things down*/
#define DO_24_BIT	1			/* 24-bit color  */

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

static int get_1_pt_list(struct solid *, Xgl_pt_list *);
static void free_pts(Xgl_pt_list *, int);
static void switch_bg_color();
static void start_ts();
static int stop_ts();
static int getms(struct timeval , struct timeval );


/*
 *	Miscellaneous defines
 */
					/* Yes and No */
#define NO	0
#define YES	1

					/* Display coordinate conversion: */
 					/*	GED is using -2048..+2048, */
 					/*	xview is 0..width, 0..height */
#define	SUNPWx_TO_GED(x) (((x)/(double)width-0.5)*4095)
#define	SUNPWy_TO_GED(x) (-((x)/(double)height-0.5)*4095)


/*
 *	Buttons
 */
		/* The "standard cmds" buttons should be tailered to	*/
		/* best meet the needs of the installation.  Unused buttons */
		/* should have 1 space as a label and a single newline as */
		/* the command.						*/
		/*							*/
struct button	button_list[] = {

	{"STANDARD CMDS",	NULL,		NULL},

	{"E all",		"E all\n",	standard_button_proc},
	{"refresh",		"refresh\n",	standard_button_proc},
	{"debug",		"regdebug\n",	standard_button_proc},
	{"quit",		"q\n",		standard_button_proc},
	{"clear",		"Z\n",	        standard_button_proc},
	{" ",			"\n",		standard_button_proc},
	{" ",			"\n",		standard_button_proc},
	{" ",			"\n",		standard_button_proc},
	{" ",			"\n",		standard_button_proc},
	{" ",			"\n",		standard_button_proc},
	{" ",			"\n",		standard_button_proc},

	{"SPECIAL CMDS",	NULL,		NULL},

	{"menu toggle",		NULL,		menu_button_proc},
	{"detach",		NULL,		detach_button_proc},

	{NULL,			NULL,		NULL},
};


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
	int		p0[2], p1[2], p3[2];
	int		pid;
	register int	fds;
	static int	once = 0;

	DPRINTF("XGL_open\n");

	if(once++) {
		fprintf(stderr, "Cannot re-attach to XGL\n");
		return(-1);
	}

	init_globals();			/* initialize global variables */

	pipe(p0);			/* set up 3 pipes for stdin, */
	pipe(p1);			/* stdout, and */
	pipe(p3);			/* a special "command" pipe */
	if((pid = fork()) < 0) {	/* FORK */
		perror("mged XGL_open");
		return(-1);
	}

	if(!pid) {		/* CHILD - the command terminal subwindow */
		cmd_fd = 3;
		dup2(p0[1], 1);		/* stdout to parent */
		dup2(p1[0], 0);		/* stdin from parent */
		dup2(p3[0], cmd_fd);	/* special "command" from parent */
#if defined(_SC_OPEN_MAX)
		fds = sysconf(_SC_OPEN_MAX);
#else
		fds = 32;
#endif
		for(; fds > 4; close(--fds));

		xv_init(0);		/* init xview */
		init_create_icons();	/* create icon */
		init_termsw_child(p0, p1, p3);	/* setup the term subwindow */
		notify_start(/*frame*/);	/* and, go */
		exit(0);		/* but, never return */

	} else {		/* PARENT - the canvas and panel */
		dup2(p0[0], 0);		/* stdin from child */
		dup2(p1[1], 1);		/* stdout to child */
		cmd_fd = p3[1];		/* special "command" to child */
		if(p0[0] != 0) close(p0[0]);
		if(p1[1] != 1) close(p1[1]);
		close(p0[1]);
		close(p1[0]);
		close(p3[0]);
		/*
		 * We must fork BEFORE the call to xgl_open()! Otherwise
		 * XGL gets very confused (as documented)
		 */
		if(fork()) exit(0);	/* "background" us */

		xv_init(0);		/* init xview */
		init_create_icons();	/* create icon */
		init_graphics_parent_xview(); /* setup xview stuff */
					      /* and, xgl stuff */
		if(init_graphics_parent_xgl(frame) < 0)
			goto bad_exit;
		xv_set(canvas,
			CANVAS_REPAINT_PROC,	repaint_proc,
			CANVAS_RESIZE_PROC,	resize_proc,
			0);

		if(init_editor() < 0)	/* set the EDITOR variable */
			goto bad_exit;

		return(0);		/* go back to GED */
	}

bad_exit:
	xv_destroy_safe(frame);
	xgl_close(sys_state);
	return(-1);
	
}

/*
 * 	XGL_close
 */
void
XGL_close()
{
	static int	once = 0;

	DPRINTF("XGL_close\n");

	if(once++) return;

	dup2(2, 1);
	dup2(2, 0);
	xv_destroy_safe(frame);
	xgl_close(sys_state);
	return;
}

/*
 *	XGL_prolog
 */
void
XGL_prolog()
{
	DPRINTF("XGL_prolog\n");
start_ts();

	if(!dmaflag || dbuffering)
		return;

    /* If we are NOT doing any double buffering, erase the canvas	*/
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

	FlushPolyLines();

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
/*fprintf(stderr,"XGL_epilog: %d ms from prolog\n", stop_ts());*/

	return;
}

static void
switch_bg_color()
{
	Xgl_color	bg;

	bg.index = COLORINDEX(DM_BLACK);
	xgl_object_set (ctx_2d, XGL_CTX_BACKGROUND_COLOR, &bg, 0);
	xgl_object_set (ctx_3d, XGL_CTX_BACKGROUND_COLOR, &bg, 0);
}

static void
apply_matrix(mat_t mat)
{
	int i;
	static Xgl_matrix_f3d	xgl_mat;
        Xgl_pt               	pt;
        Xgl_pt_f3d           	pt_f3d;
	float			w;

	for (i = 0; i < 16; i++)
		xgl_mat[i/4][i%4] = (fabs(mat[i]) < 1.0e-15) ? 0.0 : mat[i];
	/*
	 * The following matrix is passed in:
	 *	1.0	0.0	0.0	Tx
	 *	0.0	1.0	0.0	Ty
	 *	0.0	0.0	1.0	Tz
	 *	0.0	0.0	0.0	W
	 *
	 * This (for some as yet unknown reason) produces very slow DGA results.
	 * So, we perform the rotation, translation, and scaling in 3 separate 
	 * calls. We use the W value for the scale factor.
	*/

	/*
	 * First, rotate it
	 */
	xgl_transform_identity(trans);
	xgl_transform_write_specific (trans, xgl_mat, 
                   		 XGL_TRANS_MEMBER_ROTATION);
	xgl_transform_transpose(trans, trans);

	/*
	 * Next, translate it
	 */
	pt.pt_type = XGL_PT_F3D;
	pt.pt.f3d = &pt_f3d;
	pt_f3d.x = xgl_mat[0][3];
	pt_f3d.y = xgl_mat[1][3];
	pt_f3d.z = xgl_mat[2][3];
	xgl_transform_translate (trans, &pt, XGL_TRANS_POSTCONCAT);

	/*
	 * Finally, scale it
	 */
	w = xgl_mat[3][3];
        pt.pt_type = XGL_PT_F3D;
        pt.pt.f3d = &pt_f3d;
        pt_f3d.x = 1.0/w;
        pt_f3d.y = 1.0/w;
        pt_f3d.z = 1.0/w;
 
        xgl_transform_scale (trans, &pt, XGL_TRANS_POSTCONCAT);

	xgl_object_set(ctx_3d,
		XGL_CTX_LOCAL_MODEL_TRANS, trans,
		0);
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
 */
int
XGL_object(sp, mat, ratio, white)
struct solid *sp;
mat_t mat;
double ratio;
{
	Xgl_color			ln_color;

	Xgl_color_index			color_index;
	float				width_factor;
	static Xgl_color_index		last_color_index = DM_YELLOW;
	static float			last_width_factor = 0.0;
	int				diff = 0;
static struct timeval tp1, tp2;

/*	DPRINTF("XGL_object\n");*/

	if (sp) {
		/* Set the line color (s_dmindex was set in XGL_colorchange) */
		if(MONOCHROME) {
			color_index = 1;
		} else if(white) {
			color_index = DM_WHITE;
		} else {
			color_index = sp->s_dmindex;
		}

		SetColor(ln_color, color_index);

		/* And, the width. */
		if(white) {
			width_factor = 2.0;
		} else {
			width_factor = 1.0;
		}

		if (color_index  != last_color_index ||
	    	    width_factor != last_width_factor) {

			diff = 1;
		}
	}

	if (sp == NULL || 
	    diff || 
	    num_pt_lists == MAX_PT_LISTS ||
	    num_pt_dash_lists == MAX_PT_LISTS) {
		if (num_pt_lists) {
			xgl_object_set(ctx_3d,
				XGL_CTX_LINE_STYLE,     XGL_LINE_SOLID,
				0);
			xgl_multipolyline(ctx_3d, NULL, 
					num_pt_lists, pl);
			free_pts (pl, num_pt_lists);
			num_pt_lists = 0;
		}
		if (num_pt_dash_lists) {
			xgl_object_set(ctx_3d,
				XGL_CTX_LINE_STYLE,     XGL_LINE_PATTERNED,
				0);
			xgl_multipolyline(ctx_3d, NULL, 
					num_pt_dash_lists, pl_dash);
			free_pts (pl_dash, num_pt_dash_lists);
			num_pt_dash_lists = 0;
		}
		if (diff) {
			xgl_object_set(ctx_3d,
               			XGL_CTX_LINE_COLOR,    &ln_color,
/* Don't use Line Width. It's not defined for 3D 
               	 		XGL_CTX_LINE_WIDTH_SCALE_FACTOR, width_factor,
*/
               	 		0);
			last_color_index = color_index;
			last_width_factor = width_factor;
		}
	}

	if (sp) {
#if DO_DASH_LINES
		if (sp->s_soldash) {
			if (get_1_pt_list (sp, 
			    		&pl_dash[num_pt_dash_lists]) == 0) {
				fprintf(stderr, "mged XGL_object: no memory\n");
                		return 0;
        		}
			num_pt_dash_lists++;
		} else {
#endif
			if (get_1_pt_list (sp, &pl[num_pt_lists]) == 0) {
				fprintf(stderr, "mged XGL_object: no memory\n");
                		return 0;
        		}
			num_pt_lists++;
		}
#if DO_DASH_LINES
	}
#endif

	return(1);
}

static  int
get_1_pt_list(struct solid *sp, Xgl_pt_list *pt_list)
{

	struct rt_vlist         *vp;
	Xgl_pt_flag_f3d		*ptr;
	
	pt_list->pt_type = XGL_PT_FLAG_F3D;
	pt_list->num_pts = sp->s_vlen;
	if((pt_list->pts.flag_f3d = (Xgl_pt_flag_f3d *)calloc(
			sp->s_vlen, sizeof(Xgl_pt_flag_f3d)))
            		== (Xgl_pt_flag_f3d *)0)
                	return(0);
	ptr = pt_list->pts.flag_f3d;

	for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
		int    i;
		int    nused = vp->nused;
                int    *cmd = vp->cmd;
                point_t *pt = vp->pt;
                for( i = 0; i < nused; i++,cmd++,pt++,ptr++ )  {
                        switch( *cmd )  {
                        case RT_VLIST_POLY_START:
                                continue;
                        case RT_VLIST_POLY_MOVE:
                        case RT_VLIST_LINE_MOVE:
                                /* Move, not draw */
                                ptr->x = pt[0][0];
                                ptr->y = pt[0][1];
                                ptr->z = pt[0][2];
                                ptr->flag = 0x00;
                                continue;
                        case RT_VLIST_POLY_DRAW:
                        case RT_VLIST_POLY_END:
                        case RT_VLIST_LINE_DRAW:
                                /* draw */
                                ptr->x = pt[0][0];
                                ptr->y = pt[0][1];
                                ptr->z = pt[0][2];
                                ptr->flag = 0x01;
                                continue;
                        }
                }
	}
	return (1);
}
static void
free_pts(Xgl_pt_list pt_list[], int num)
{
	int	i;

	for (i = 0; i < num; i++) 
		free (pt_list[i].pts.flag_f3d);
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
	FlushPolyLines();

	pos.x = x;
	pos.y = y;

	if (MONOCHROME)
		ln_color.index;
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
	FlushPolyLines();

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
	static int		once = 0;
	struct itimerval	timeout;
	fd_set          files;

	DPRINTF("XGL_input\n");

	files = *input;         /* save, for restore on each loop */
        FD_SET( display->fd, &files );      /* check X fd too */

	if(!once++) {
						/* first time through */
		xv_set(frame, WIN_SHOW,	TRUE, 0); /* display the canvas */
		XFlush(display);
		set_termsw_display();		/* and, the command window */
	}

	if(busy) {				/* if busy cursor is up, */
		busy = 0;			/* change to normal cursor */
		xv_set(pw,    WIN_CURSOR, cursor_red,  0);
		XFlush(display);
	}

	dm_values.dv_buttonpress = 0;
	dm_values.dv_flagadc     = 0;
	dm_values.dv_penpress    = 0;

    /* If there is a command waiting  to be read, have the notifier call */
    /* stop_func() which will call notify_stop(), so we can get back to ged */
    /*									*/

	notify_set_input_func(frame, stop_func, fileno(stdin) /* input_fd */);
	notify_set_input_func(frame, stop_func, display->fd /* input_fd */);

	if(noblock) {

    /* If we are not using any type of double buffering, slow it down */
    /* a bit so the user isn't too confused by the constant jerkiness */
    /*									*/
		if(!dbuffering)
			sleep(1);

    /* It would be nice to be able to use notify_dispatch() here.	*/
    /* But, we seem to drop a lot of events if that happens.   In fact,	*/
    /* using notify_dispatch() here can cause openwin to completely	*/
    /* lock up.								*/
    /*									*/
    /* Using notify_start() and nofify_stop() seems to work best.	*/
    /*									*/
		bzero((char *)&timeout, sizeof(timeout));
		timeout.it_value.tv_usec = 1L;
		notify_set_itimer_func(pw, stop_func, ITIMER_VIRTUAL,
			&timeout, NULL);
		notify_start();
	} else {
		notify_start();
		xv_set(pw,    WIN_CURSOR, cursor_busyred,   0);
		++busy;
	}
	XFlush(display);

	/* XXX This won't work well, do select() here! */
	if (!input_poll(fileno(stdin)))
		FD_CLR(fileno(stdin), input);
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

	switch(newstate) {
	case ST_VIEW:
		mouse_tracking = 0;
		break;

	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_PICK:
		mouse_tracking = 1;
		break;

	case ST_O_EDIT:
	case ST_S_EDIT:
		mouse_tracking = 0;
		break;

	default:
		DPRINTF("XGL_statechange:  UNKNOWN STATE\n");
		return;
	}
	if(XGL_debug_level) fprintf(stderr, "XGL_statechange from %s to %s\n",
		state_str[oldstate], state_str[newstate]);

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
				sp->s_dmindex = i;
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
	Xgl_bounds_d2d	bounds_d2d;
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
	detached_flag	= 0;
	mouse_tracking	= 0;
	dbuffering	= 0;
	busy		= 1;
}

static void
init_create_icons()
{
	Server_image	icon_image;

	icon_image = (Server_image)xv_create(XV_NULL, SERVER_IMAGE,
		XV_WIDTH,		64,
		XV_HEIGHT,		64,
		SERVER_IMAGE_BITS,	icon_bits,
		0);
	icon  = (Icon)xv_create(XV_NULL, ICON,
		ICON_IMAGE,		icon_image,
		0);

	return;
}

static void
init_termsw_child()
{
	frame = (Frame)xv_create(XV_NULL, FRAME,
		FRAME_ICON,		icon,
		FRAME_LABEL,		"MGED Display",
		0);

        display = (Display *)xv_get(frame, XV_DISPLAY);

	termsw = (Termsw)xv_create(frame, TERMSW,
		TTY_ARGV,		TTY_ARGV_DO_NOT_FORK,
		WIN_ROWS,		10,
		0);
	pty_fd = (int)xv_get(termsw, TTY_TTY_FD);
	setsid();
	tcsetpgrp(pty_fd, getpgrp(0));

	window_fit(frame);

	notify_set_input_func(frame, copy_func, 0);
	notify_set_input_func(frame, copy_func, pty_fd);
	notify_set_input_func(frame, copy_func, cmd_fd);

	notify_set_signal_func(frame, signal_func, SIGINT, NOTIFY_SYNC);

	return;
}

static void
init_graphics_parent_xview()
{
	register struct button	*butts;
	Panel_item		last_button;
	int			y;
	Xv_singlecolor		redish;
	XVisualInfo		visual_info;

	frame = (Frame)xv_create(XV_NULL, FRAME,
		WIN_DYNAMIC_VISUAL,	TRUE,
		OPENWIN_AUTO_CLEAR,	FALSE,
		FRAME_ICON,		icon,
		FRAME_LABEL,		"MGED Display",
		WIN_EVENT_PROC,		frame_event_proc,
		WIN_CONSUME_EVENTS,	WIN_NO_EVENTS,
					WIN_RESIZE,
					WIN_REPAINT,
					0,
		0);
	notify_interpose_destroy_func(frame, destroy_func);

        display = (Display *)xv_get(frame, XV_DISPLAY);
#if DO_24_BIT
	/*
	 * Get the visual information
	 */
	if (XMatchVisualInfo (display, DefaultScreen(display),
		24, TrueColor, &visual_info) ||
	    XMatchVisualInfo (display, DefaultScreen(display),
		24, DirectColor, &visual_info)) {

		win_visual_class = visual_info.class;
		win_depth = 24;

	} else {
		win_visual_class = DefaultVisual(display, 
				DefaultScreen(display))->class;
		win_depth = DisplayPlanes (display, DefaultScreen(display));
	}
/*fprintf(stderr, "Visual Class is %d, depth = %d\n",win_visual_class,win_depth);*/
#endif
	panel = (Panel)xv_create(frame, PANEL,
		PANEL_LAYOUT,		PANEL_VERTICAL,
		0);

	last_button = (Panel_item)0;
	for(butts = button_list; butts->label != NULL; ++butts) {
		if(butts->button_proc == NULL) {
			if(last_button == (Panel_item)0) {
				y = 15;
			} else {
				y = (int)xv_get(last_button, PANEL_ITEM_Y) +
				    (int)xv_get(last_button, XV_HEIGHT)    +
				    30;
			}
			xv_create(panel, PANEL_MESSAGE,
				PANEL_LABEL_STRING,	butts->label,
				XV_Y,			y,
				0);
			continue;
		}

		last_button = (Panel_item)xv_create(panel, PANEL_BUTTON,
			PANEL_LABEL_STRING,		butts->label,
			PANEL_LABEL_WIDTH,		100,
			PANEL_CLIENT_DATA,		butts->cmd,
			PANEL_NOTIFY_PROC,		butts->button_proc,
			XV_X,				30,
			0);
	}
	window_fit_width(panel);


	canvas = (Canvas)xv_create(frame, CANVAS,
		WIN_RIGHT_OF,		panel,
		XV_HEIGHT,		height,
		XV_WIDTH,		width,
		XV_HELP_DATA,		"dm-XGL:canvas",
#if DO_24_BIT
		XV_DEPTH,		win_depth,
		XV_VISUAL_CLASS,	win_visual_class,
#endif
		CANVAS_AUTO_CLEAR, FALSE,
		CANVAS_FIXED_IMAGE, FALSE,
		CANVAS_RETAINED,	FALSE,
		0);
	pw = (Xv_Window)canvas_paint_window(canvas);

	redish.red   = 255;
	redish.green =  80;
	redish.blue  =   0;

	cursor_red     = (Cursor)xv_create(XV_NULL, CURSOR,
		CURSOR_SRC_CHAR,		OLC_BASIC_PTR,
		CURSOR_FOREGROUND_COLOR,	&redish,
		0);

	cursor_busyred = (Cursor)xv_create(XV_NULL, CURSOR,
		CURSOR_SRC_CHAR,		OLC_BUSY_PTR,
		CURSOR_FOREGROUND_COLOR,	&redish,
		0);

	xv_set(pw,
		WIN_EVENT_PROC,		pw_event_proc,
		WIN_CONSUME_EVENTS,	WIN_NO_EVENTS,
					WIN_TOP_KEYS,
					WIN_UP_EVENTS,
					WIN_MOUSE_BUTTONS,
					LOC_WINENTER,
					LOC_MOVE,
					WIN_RESIZE, WIN_REPAINT,
					0,
		WIN_CURSOR,		cursor_busyred,
		0);

	window_fit(frame);

	return;
}

static int
init_graphics_parent_xgl(frame)
Frame	frame;
{
        Atom		catom;
	Window		frame_window;
	Window		canvas_window;
	Xgl_color	ln_color;
	Xgl_bounds_d2d	bounds_d2d;
	Xgl_bounds_d3d	bounds_d3d;
	int		bufs;
	
	canvas_window = (Window)xv_get(pw, XV_XID);
	frame_window = (Window)xv_get(frame, XV_XID);

#if 0
	/* Set the window background */
	XSetWindowBackground(display, 
		canvas_window, BlackPixel(display,DefaultScreen(display))); 
#endif
	catom = XInternAtom(display, "WM_COLORMAP_WINDOWS", False);
	XChangeProperty(display, frame_window, catom, XA_WINDOW, 32,
		PropModeAppend, (unsigned char *)&canvas_window, 1);

	xgl_x_win.X_display = (void *)XV_DISPLAY_FROM_WINDOW(pw);
	xgl_x_win.X_window = (Xgl_usgn32)canvas_window;
	xgl_x_win.X_screen = (int)DefaultScreen(display);

	win_desc.win_ras.type = XGL_WIN_X ;
	win_desc.win_ras.desc = &xgl_x_win;

	sys_state = xgl_open(0);
{
       Xgl_inquire          *inq_info;
       if (!(inq_info = xgl_inquire(sys_state, &win_desc))) {
          printf("error in getting inquiry\n");
          exit(1);
        }
	bufs = inq_info->maximum_buffer;	/* if double buffering, its 2 */
}
    if (win_depth == 24)
	color_type = XGL_COLOR_RGB;
    else
	color_type = XGL_COLOR_INDEX;

    /* ras MUST be created before init_check_buffering() is called */
    	ras = xgl_object_create(sys_state, XGL_WIN_RAS, &win_desc,
		XGL_RAS_COLOR_TYPE, color_type,
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
	bounds_d3d.zmax = 1.;
	bounds_d3d.zmin = -1.;	

	ctx_3d = xgl_object_create (sys_state, XGL_3D_CTX, NULL,
			XGL_CTX_DEVICE,		ras,
			XGL_CTX_LINE_COLOR,	&ln_color,
			XGL_CTX_VDC_MAP,	XGL_VDC_MAP_ASPECT,
			XGL_CTX_VDC_ORIENTATION, XGL_Y_UP_Z_TOWARD,
			XGL_CTX_VDC_WINDOW,	&bounds_d3d,
			XGL_CTX_CLIP_PLANES,	XGL_CLIP_XMIN | XGL_CLIP_YMIN,
                        XGL_CTX_LINE_PATTERN,   xgl_lpat_dashed,
			XGL_CTX_LINE_STYLE,	XGL_LINE_SOLID,
			XGL_CTX_DEFERRAL_MODE, XGL_DEFER_ASTI,
			0);
	ctx_2d = xgl_object_create(sys_state, XGL_2D_CTX, NULL,
			XGL_CTX_DEVICE,		ras,
			XGL_CTX_LINE_COLOR,	&ln_color,
			XGL_CTX_VDC_MAP,	XGL_VDC_MAP_ASPECT,
			XGL_CTX_VDC_ORIENTATION, XGL_Y_UP_Z_TOWARD,
			XGL_CTX_VDC_WINDOW,	&bounds_d2d,
			0);
	/*switch_bg_color();*/
	if(CMAPDBUFFERING) {
		xgl_context_new_frame(ctx_3d);
		xgl_context_new_frame(ctx_2d);
		current_buffer_is_A = 0;
		xgl_object_set(ctx_3d, XGL_CTX_PLANE_MASK, CMAP_MASK_B, 0);
		xgl_object_set(ctx_2d, XGL_CTX_PLANE_MASK, CMAP_MASK_B, 0);
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
			    | XGL_CTX_NEW_FRAME_SWITCH_BUFFER),
		    0);
	}

	trans = xgl_object_create(sys_state, XGL_TRANS, NULL, NULL);

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
#if DO_DOUBLE_BUFF
	if (bufs > 1) {
		/* Let's try double buffering because the H/W says it exists */
		xgl_object_set(ras, XGL_WIN_RAS_BUFFERS_REQUESTED, bufs, 0);
	}
#endif
	xgl_object_get(ras, XGL_WIN_RAS_BUFFERS_ALLOCATED, &nbufs);
	xgl_object_get(ras, XGL_DEV_COLOR_MAP, &cmap);
	xgl_object_get(cmap, XGL_CMAP_MAX_COLOR_TABLE_SIZE, &maxsize);

	if(nbufs > 1) {

	printf("\nA total of %d buffers are supported by this\n", nbufs);
	printf("hardware.  Hardware double buffering will be turned on.\n");

		return(AB_DBUFF);

	} else if(maxsize == 2) {

	printf("\nThis is a MONOCHROME frame buffer.  No double buffering\n");
	printf("of any type can be used.\n\n");

		maxcolors = 2;
		colortablesize = 2;
		return(NO_DBUFF);

	} else if(maxsize != 256) {

	printf("\nUnknown frame buffer type ... aborting.\n\n");

		return(-1);

	} else {

		printf("\nUsing colormap double buffering, 8 bit color.\n");

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
			printf("Undefined DEFAULT_DB option, exiting\n");
			exit(1);
		}
	}
	return(type);
}

static int
init_editor()
{
	char		*openwinhome, *editor;
	static char	buff[80];
	FILE		*file;

	if((editor = getenv("EDITOR")) != NULL) {
		if((openwinhome = getenv("OPENWINHOME")) == NULL) {
			fprintf(stderr,
			    "mged XGL_open:  $OPENWINHOME not set\n");
			return(-1);
		}
		if((file = fopen("/tmp/MGED.editor", "w+")) == NULL) {
			perror("mged XGL_open");
			return(-1);
		}
		fchmod(fileno(file), 0755);
		fprintf(file, "#!/bin/sh\n");
		fprintf(file, "%s/bin/xview/shelltool %s $1\n", openwinhome, editor);
		fclose(file);
		strcpy(buff, "EDITOR=/tmp/MGED.editor");
	} else {
		strcpy(buff, "EDITOR=/bin/ed");
	}

	if(putenv(buff)) {
		perror("mged XGL_open");
		return(-1);
	}
	return(0);
}


/************************************************************************
 *									*
 * 	MISCELLANEOUS							*
 *									*
 ************************************************************************/

static int
ok(defalt)
int	defalt;
{
	char	line[80];

	for(;;) {
		printf("Is it OK to proceed [%s]? ",
			(defalt == YES) ? "yes" : "no");
		(void)fgets( line, sizeof(line), stdin ); /* \n, null terminated */
		line[strlen(line)-1] = '\0';		/* remove newline */
		if(feof(stdin)) continue;
		switch(*line) {
		  case '\0':
			return(defalt);
		  case 'y':
		  case 'Y':
			return(YES);
		  case 'n':
		  case 'N':
			return(NO);
		  default:
			break;
		}
	}
}

static void
setcolortables(color_table_A, color_table_B)
Xgl_color	*color_table_A, *color_table_B;
{
	register int	i;

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

static int
input_poll(fd)
int	fd;
{
	fd_set		readfds;
	struct timeval	timeout;
	int		width;

	bzero((char *)&timeout, sizeof(timeout));
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

#if defined(_SC_OPEN_MAX)
	width = sysconf(_SC_OPEN_MAX);
#else
	width = 32;
#endif

	if(select(width, &readfds, NULL, NULL, &timeout) > 0) {
		return(1);
	} else {
		return(0);
	}
}

static void
set_termsw_width(w)
int	w;
{
	char	c;

	c = SET_WIDTH;
	write(cmd_fd, &c, 1);
	write(cmd_fd, &w, sizeof(w));

	return;
}

static void
set_termsw_position(x, y)
int	x, y;
{
	char	c;

	c = SET_POSITION;
	write(cmd_fd, &c, 1);
	write(cmd_fd, &x, sizeof(x));
	write(cmd_fd, &y, sizeof(y));

	return;
}

static void
set_termsw_display()
{
	char	c;

	c = SET_DISPLAY;
	write(cmd_fd, &c, 1);
}



/************************************************************************
 *									*
 * 	XVIEW ROUTINES FOR THE PARENT PROCESS				*
 *									*
 ************************************************************************/

/*
 *	Notifier funcs
 */


static Notify_value
stop_func(client, fd)
Notify_client	client;
int		fd;
{
/*fprintf (stderr,"stop_func:calling notify_stop()\n");*/
	notify_stop();
/*fprintf (stderr,"stop_func:back from notify_stop()\n");*/

	return(NOTIFY_DONE);
}

static Notify_value
destroy_func(client, status)
Notify_client client;
Destroy_status status;
{
	static int	once = 0;

	if(once++) return(NOTIFY_DONE);

	quit();	

	return(NOTIFY_DONE);
}


/*
 *	Button procs
 */

static int
standard_button_proc(item, event)
Panel_item	item;
Event		*event;
{
	char	*buff;

	buff = (char *)xv_get(item, PANEL_CLIENT_DATA);
	write(cmd_fd, buff, strlen(buff));
	
	return(0);
}

static int
menu_button_proc(item, event)
Panel_item	item;
Event		*event;
{
	no_2d_flag ^= 0x01;

	dmaflag = 1;
	stop_func((Notify_client)0, 0);

	return(0);
}

static int
detach_button_proc(item, event)
Panel_item	item;
Event		*event;
{
	int	h, x, y;
	Rect	r;

	detached_flag ^= 0x01;

	frame_get_rect (frame, &r);
	h = r.r_height;
	x = r.r_left;
	y = r.r_top + h;
	if(detached_flag) {
		xv_set(item, PANEL_LABEL_STRING, "attach", 0);
	} else {
		xv_set(item, PANEL_LABEL_STRING, "detach", 0);
		set_termsw_width((int)xv_get(frame, XV_WIDTH));
	}
	set_termsw_position(x, y);

	return(0);
}


/*
 *	Resize and Repaint
 */

static void
repaint_proc(canvas, paint_window, repaint_area)
Canvas		canvas;
Xv_Window	paint_window;
Rectlist	*repaint_area;
{
	DPRINTF("repaint_proc\n");

	dmaflag = 1;
	stop_func((Notify_client)0, 0);

	return;
}

static void
resize_proc(canvas, w, h)
Canvas		canvas;
int		w, h;
{
	DPRINTF("resize_proc\n");

	width  = w;
	height = h;

	if(width < height) height = width;
	if(height < width) width  = height;

	XSync (display, 0);
	xgl_window_raster_resize(ras);

	dmaflag = 1;
	stop_func((Notify_client)0, 0);

	return;
}

/*
 *	Event handlers
 */

static void
frame_event_proc(win, event, arg)
Window	win;
Event	*event;
caddr_t	*arg;
{
	int		h, x, y;
	Rect	r;

	switch(event_id(event)) {
	case WIN_RESIZE:
		if(detached_flag) break;

		frame_get_rect (frame, &r);
		h = r.r_height;
		x = r.r_left;
		y = r.r_top + h;
		set_termsw_position(x, y);
		set_termsw_width((int)xv_get(frame, XV_WIDTH));
		break;
	default:
		break;
	}
}

static void
pw_event_proc(win, event, arg)
Window	win;
Event	*event;
caddr_t	*arg;
{
	struct itimerval	timeout;
	int			x, y;


	x = event_x(event);
	y = event_y(event);

	if(x >= width)  x = width  - 1;
	if(y >= height) y = height - 1;
	
	xpen = SUNPWx_TO_GED(x);
	ypen = SUNPWy_TO_GED(y);

	switch(event_id(event)) {
	case WIN_RESIZE:
		break;
	case WIN_REPAINT:
		break;
	case ACTION_SELECT:
	case MS_LEFT:
		if (event_is_down(event)) {
			rt_vls_strcat( &dm_values.dv_string , "zoom .5\n" );
			stop_func((Notify_client)0, 0);
		}
		break;
	case ACTION_ADJUST:
	case MS_MIDDLE:
		if (!no_2d_flag && event_is_down(event)) {
			rt_vls_printf(  &dm_values.dv_string , "M 1 %d %d\n", 
				xpen, ypen );
			stop_func((Notify_client)0, 0);
		}
	 	break;
	case ACTION_MENU:
	case MS_RIGHT:
		if (event_is_down(event)) {
			rt_vls_strcat( &dm_values.dv_string , "zoom 2\n" );
			stop_func((Notify_client)0, 0);
		}
		break;

	case LOC_WINENTER:
		break;
	case LOC_MOVE:
		if(sun_buttons)
			set_dm_values();

	/*********************************************************************/
	/* Delay the effect of mouse tracking to make things smoother.	     */
	/*********************************************************************/
		if(mouse_tracking || sun_buttons) {
			rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n",
                                xpen, ypen );
			bzero((char *)&timeout, sizeof(timeout));
			timeout.it_value.tv_usec = 500000L;
			notify_set_itimer_func(pw, stop_func, ITIMER_REAL,
		    		&timeout, NULL);
		}

		break;

	case KEY_TOP(ZOOM_BUTTON):
		set_sun_buttons(event, ZOOM_FLAG);
		break;

	case KEY_TOP(X_SLEW_BUTTON):
		set_sun_buttons(event, X_SLEW_FLAG);
		break;

	case KEY_TOP(Y_SLEW_BUTTON):
		set_sun_buttons(event, Y_SLEW_FLAG);
		break;

	case KEY_TOP(Z_SLEW_BUTTON):
		set_sun_buttons(event, Z_SLEW_FLAG);
		break;

	case KEY_TOP(X_ROT_BUTTON):
		set_sun_buttons(event, X_ROT_FLAG);
		break;

	case KEY_TOP(Y_ROT_BUTTON):
		set_sun_buttons(event, Y_ROT_FLAG);
		break;

	case KEY_TOP(Z_ROT_BUTTON):
		set_sun_buttons(event, Z_ROT_FLAG);
		break;

	default:
		break;

	}
	return;
}

static void
set_sun_buttons(event, button_flag)
Event	*event;
int	button_flag;
{
	if(event_is_up(event)) {
		sun_buttons &= ~button_flag;
	} else if(event_is_down(event)) {
		sun_buttons |= button_flag;
	}
	set_dm_values();
	stop_func((Notify_client)0, 0);
	return;
}

static void
set_dm_values()
{
	float		xval, yval, zoomval;
	char		str_buf[128];

	if(sun_buttons) {
		xval = (float) xpen / 2048.;
		if (xval < -1.0) xval = -1.0;
		if (xval > 1.0)  xval = 1.0;

		yval = (float) ypen / 2048.;
		if (yval < -1.0) yval = -1.0;
		if (yval > 1.0)  yval = 1.0;

		zoomval = (yval * yval) / 2;
		if(yval < 0) zoomval = -zoomval;
	}

	if(sun_buttons & ZOOM_FLAG)
	{
		(void)sprintf( str_buf , "zoom %f\n", zoomval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}
	if(sun_buttons & X_SLEW_FLAG)
	{
		(void)sprintf( str_buf , "knob X %f\n", xval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}
	if(sun_buttons & Y_SLEW_FLAG)
	{
		(void)sprintf( str_buf , "knob Y %f\n", yval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}
	if(sun_buttons & Z_SLEW_FLAG)
	{
		(void)sprintf( str_buf , "knob Z %f\n", yval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}
	if(sun_buttons & X_ROT_FLAG)
	{
		(void)sprintf( str_buf , "knob x %f\n", xval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}
	if(sun_buttons & Y_ROT_FLAG)
	{
		(void)sprintf( str_buf , "knob Y %f\n", yval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}
	if(sun_buttons & Z_ROT_FLAG)
	{
		(void)sprintf( str_buf , "knob Z %f\n", yval );
		rt_vls_strcat( &dm_values.dv_string , str_buf );
	}

	return;
}



/************************************************************************
 *									*
 * 	XVIEW ROUTINES FOR THE CHILD PROCESS				*
 *									*
 ************************************************************************/

/*
 *	Notifier funcs
 */
#define IGNORED_MSG	"The \'%\' command is ignored in this environment...\n"
static Notify_value
copy_func(client, ifd)
Notify_client	client;
int		ifd;
{
	int		ofd, i, x, y, w;
	char		c;
	static char	b = '\n';

	if(ifd == 0)
		ofd = pty_fd;
	else if(ifd == pty_fd)
		ofd = 1;
	else if(ifd == cmd_fd)
		ofd = -1;

	while(input_poll(ifd)) {
		if((i = read(ifd, &c, 1)) < 0) {
			perror("mged XGL termsw read");
			sleep(5);
			exit(1);
		}
		if(!i) {
			xv_destroy_safe(frame);
			return(NOTIFY_DONE);
		}
		if(ifd == cmd_fd) {
			if(isprint(c) || isspace(c)) {
				ttysw_input(termsw, &c, 1);
			} else {
				switch(c) {
				   case SET_POSITION:
					read(ifd, (char *)&x, sizeof(x));
					read(ifd, (char *)&y, sizeof(y));
					xv_set(frame,
						XV_X,		x,
						XV_Y,		y,
						0);
					break;
	
				   case SET_WIDTH:
					read(ifd, (char *)&w, sizeof(w));
					xv_set(frame, XV_WIDTH, w, 0);
					break;
	
				   case SET_DISPLAY:
					xv_set(frame, WIN_SHOW, TRUE, 0);
					break;
	
				   default:
					break;
				}
			}
		} else {
			if(ofd == 1) {
    /*									*/
    /* Ignore the "Escape to shell command";   it only causes problems,	*/
    /* and there is really no need for it in an Openwindows environment	*/
    /*									*/
				if(isspace(b) && (c == '%')) {
				    write(pty_fd, IGNORED_MSG,
					sizeof(IGNORED_MSG));
			    	    continue;
				}
				b = c;
			}
			if(write(ofd, &c, 1) < 0) {
				perror("mged XGL termsw write");
				sleep(5);
				exit(1);
			}
		}

    /* get back to the notifier, in case we have a lot of stuff to read */
	if(c == '\n') break;
	}
		
	return(NOTIFY_DONE);
}

static Notify_value
signal_func(client, sig, when)
Notify_client		client;
int			sig;
Notify_signal_mode	when;
{
	kill(getppid(), sig);

	return(NOTIFY_DONE);
}


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
		fprintf(stderr, "PL[%d]: (%d pts)\n",i, pl[i].num_pts);
		for (j = 0; j < pl[i].num_pts; j++) {
			fprintf(stderr, "	pt[%j]: %f,%f,%f, %d\n",
				pl[i].pts.flag_f3d[j].x,
				pl[i].pts.flag_f3d[j].y,
				pl[i].pts.flag_f3d[j].z,
				pl[i].pts.flag_f3d[j].flag);
		}
	}
}

	
