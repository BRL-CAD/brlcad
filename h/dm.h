#ifndef SEEN_DM_H
#define SEEN_DM_H

#include "fbserv_obj.h"

#define DM_NULL (struct dm *)NULL
#define DM_MIN (-2048)
#define DM_MAX (2047)
#define X	0
#define Y	1
#define Z	2

#define DM_O(_m) offsetof(struct dm, _m)

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define DIVBY4096(x) (((double)(x))*0.0002441406)
#define	GED_TO_Xx(_dmp, x) ((int)((DIVBY4096(x)+0.5)*_dmp->dm_width))
#define	GED_TO_Xy(_dmp, x) ((int)((0.5-DIVBY4096(x))*_dmp->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->dm_width - 0.5) * 4095))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->dm_height) * 4095))

#if IR_KNOBS
#define NOISE 16		/* Size of dead spot on knob */
#endif

/* the font used depends on the size of the window opened */
#define FONTBACK	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5	"5x7"
#define FONT6	"6x10"
#define FONT7	"7x13"
#define FONT8	"8x13"
#define FONT9	"9x15"

/* Display Manager Types */
#define DM_TYPE_BAD     -1
#define DM_TYPE_NULL	0
#define DM_TYPE_PLOT	1
#define DM_TYPE_PS	2
#define DM_TYPE_X	3
#define DM_TYPE_OGL	4
#define DM_TYPE_GLX	5
#define DM_TYPE_PEX	6

/* Line Styles */
#define DM_SOLID_LINE 0
#define DM_DASHED_LINE 1

#define IS_DM_TYPE_NULL(_t) ((_t) == DM_TYPE_NULL)
#define IS_DM_TYPE_PLOT(_t) ((_t) == DM_TYPE_PLOT)
#define IS_DM_TYPE_PS(_t) ((_t) == DM_TYPE_PS)
#define IS_DM_TYPE_X(_t) ((_t) == DM_TYPE_X)
#define IS_DM_TYPE_OGL(_t) ((_t) == DM_TYPE_OGL)
#define IS_DM_TYPE_GLX(_t) ((_t) == DM_TYPE_GLX)
#define IS_DM_TYPE_PEX(_t) ((_t) == DM_TYPE_PEX)

#define GET_DM(p,structure,w,hp) { \
	register struct structure *tp; \
	for(BU_LIST_FOR(tp, structure, hp)) { \
		if(w == tp->win) { \
			(p) = tp; \
			break; \
		} \
	} \
\
	if(BU_LIST_IS_HEAD(tp, hp)) \
		p = (struct structure *)NULL; \
}

/*  Colors */
#define DM_COLOR_HI	((short)230)
#define DM_COLOR_LOW	((short)0)
#define DM_BLACK_R	DM_COLOR_LOW
#define DM_BLACK_G	DM_COLOR_LOW
#define DM_BLACK_B	DM_COLOR_LOW
#define DM_RED_R	DM_COLOR_HI
#define DM_RED_G	DM_COLOR_LOW
#define DM_RED_B	DM_COLOR_LOW
#define DM_BLUE_R	DM_COLOR_LOW
#define DM_BLUE_G	DM_COLOR_LOW
#define DM_BLUE_B	DM_COLOR_HI
#define DM_YELLOW_R	DM_COLOR_HI
#define DM_YELLOW_G	DM_COLOR_HI
#define DM_YELLOW_B	DM_COLOR_LOW
#define DM_WHITE_R	DM_COLOR_HI
#define DM_WHITE_G	DM_COLOR_HI
#define DM_WHITE_B	DM_COLOR_HI
#define DM_BLACK	DM_BLACK_R, DM_BLACK_G, DM_BLACK_B
#define DM_RED		DM_RED_R, DM_RED_G, DM_RED_B
#define DM_BLUE		DM_BLUE_R, DM_BLUE_G, DM_BLUE_B
#define DM_YELLOW	DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B
#define DM_WHITE	DM_WHITE_R, DM_WHITE_G, DM_WHITE_B
#define DM_COPY_COLOR(_dr,_dg,_db,_sr,_sg,_sb){\
	(_dr) = (_sr);\
	(_dg) = (_sg);\
	(_db) = (_sb); }
#define DM_SAME_COLOR(_dr,_dg,_db,_sr,_sg,_sb)(\
	(_dr) == (_sr) &&\
	(_dg) == (_sg) &&\
	(_db) == (_sb))

/* Command parameter to dmr_viewchange() */
#define DM_CHGV_REDO	0	/* Display has changed substantially */
#define DM_CHGV_ADD	1	/* Add an object to the display */
#define DM_CHGV_DEL	2	/* Delete an object from the display */
#define DM_CHGV_REPL	3	/* Replace an object */
#define DM_CHGV_ILLUM	4	/* Make new object the illuminated object */

/*
 * Definitions for dealing with the buttons and lights.
 * BV are for viewing, and BE are for editing functions.
 */
#define LIGHT_OFF	0
#define LIGHT_ON	1
#define LIGHT_RESET	2		/* all lights out */

struct dm_vars {
  genptr_t pub_vars;
  genptr_t priv_vars;
};

/* Interface to a specific Display Manager */
struct dm {
  int (*dm_close)();
  int (*dm_drawBegin)();	/* formerly dmr_prolog */
  int (*dm_drawEnd)();		/* formerly dmr_epilog */
  int (*dm_normal)();
  int (*dm_loadMatrix)();
  int (*dm_drawString2D)();	/* formerly dmr_puts */
  int (*dm_drawLine2D)();	/* formerly dmr_2d_line */
  int (*dm_drawPoint2D)();
  int (*dm_drawVList)();	/* formerly dmr_object */
  int (*dm_setFGColor)();
  int (*dm_setBGColor)();
  int (*dm_setLineAttr)();	/* currently - linewidth, (not-)dashed */
  int (*dm_configureWin)();
  int (*dm_setWinBounds)();
  int (*dm_setLight)();
  int (*dm_setZBuffer)();
  int (*dm_debug)();		/* Set DM debug level */
  int (*dm_beginDList)();
  int (*dm_endDList)();
  int (*dm_drawDList)();
  int (*dm_freeDLists)();
  unsigned long dm_id;          /* window id */
  int dm_displaylist;		/* !0 means device has displaylist */
  int dm_stereo;                /* stereo flag */
  double dm_bound;		/* zoom-in limit */
  int dm_boundFlag;
  char *dm_name;		/* short name of device */
  char *dm_lname;		/* long name of device */
  int dm_type;			/* display manager type */
  int dm_top;                   /* !0 means toplevel window */
  int dm_width;
  int dm_height;
  int dm_lineWidth;
  int dm_lineStyle;
  fastf_t dm_aspect;
  fastf_t *dm_vp;		/* XXX--ogl still depends on this--Viewscale pointer */
  struct dm_vars dm_vars;	/* display manager dependant variables */
  struct bu_vls dm_pathName;	/* full Tcl/Tk name of drawing window */
  struct bu_vls dm_tkName;	/* short Tcl/Tk name of drawing window */
  struct bu_vls dm_dName;	/* Display name */
  unsigned char dm_bg[3];	/* background color */
  unsigned char dm_fg[3];	/* foreground color */
  vect_t dm_clipmin;		/* minimum clipping vector */
  vect_t dm_clipmax;		/* maximum clipping vector */
  int dm_debugLevel;		/* !0 means debugging */
  int dm_perspective;		/* !0 means perspective on */
  int dm_light;			/* !0 means lighting on */
  int dm_zbuffer;		/* !0 means zbuffer on */
  int dm_zclip;			/* !0 means zclipping */
  Tcl_Interp *dm_interp;	/* Tcl interpreter */
};

/*
 *			D M _ O B J
 *
 * A display manager object is used for interacting with a display manager.
 */
struct dm_obj {
  struct bu_list	l;
  struct bu_vls		dmo_name;		/* display manager object name/cmd */
  struct dm		*dmo_dmp;		/* display manager pointer */
#ifdef USE_FBSERV
  struct fbserv_obj	dmo_fbs;		/* fbserv object */
#endif
  struct bu_observer	dmo_observers;		/* fbserv observers */
};

#define DM_OPEN(_type,_argc,_argv) dm_open(_type,_argc,_argv)
#define DM_CLOSE(_dmp) _dmp->dm_close(_dmp)
#define DM_DRAW_BEGIN(_dmp) _dmp->dm_drawBegin(_dmp)
#define DM_DRAW_END(_dmp) _dmp->dm_drawEnd(_dmp)
#define DM_NORMAL(_dmp) _dmp->dm_normal(_dmp)
#define DM_LOADMATRIX(_dmp,_mat,_eye) _dmp->dm_loadMatrix(_dmp,_mat,_eye)
#define DM_DRAW_STRING_2D(_dmp,_str,_x,_y,_size,_use_aspect)\
     _dmp->dm_drawString2D(_dmp,_str,_x,_y,_size,_use_aspect)
#define DM_DRAW_LINE_2D(_dmp,_x1,_y1,_x2,_y2) _dmp->dm_drawLine2D(_dmp,_x1,_y1,_x2,_y2)
#define DM_DRAW_POINT_2D(_dmp,_x,_y) _dmp->dm_drawPoint2D(_dmp,_x,_y)
#define DM_DRAW_VLIST(_dmp,_vlist) _dmp->dm_drawVList(_dmp,_vlist)
#define DM_SET_FGCOLOR(_dmp,_r,_g,_b,_strict) _dmp->dm_setFGColor(_dmp,_r,_g,_b,_strict)
#define DM_SET_BGCOLOR(_dmp,_r,_g,_b) _dmp->dm_setBGColor(_dmp,_r,_g,_b)
#define DM_SET_LINE_ATTR(_dmp,_width,_dashed) _dmp->dm_setLineAttr(_dmp,_width,_dashed)
#define DM_CONFIGURE_WIN(_dmp) _dmp->dm_configureWin(_dmp)
#define DM_SET_WIN_BOUNDS(_dmp,_w) _dmp->dm_setWinBounds(_dmp,_w)
#define DM_SET_LIGHT(_dmp,_on) _dmp->dm_setLight(_dmp,_on)
#define DM_SET_ZBUFFER(_dmp,_on) _dmp->dm_setZBuffer(_dmp,_on)
#define DM_DEBUG(_dmp,_lvl) _dmp->dm_debug(_dmp,_lvl)
#define DM_BEGINDLIST(_dmp,_list) _dmp->dm_beginDList(_dmp,_list)
#define DM_ENDDLIST(_dmp) _dmp->dm_endDList(_dmp)
#define DM_DRAWDLIST(_dmp,_list) _dmp->dm_drawDList(_dmp,_list)
#define DM_FREEDLISTS(_dmp,_list,_range) _dmp->dm_freeDLists(_dmp,_list,_range)

extern int Dm_Init();
extern struct dm *dm_open();
extern int dm_share_dlist();
extern fastf_t dm_Xx2Normal();
extern int dm_Normal2Xx();
extern fastf_t dm_Xy2Normal();
extern int dm_Normal2Xy();
extern int dm_processOptions();
extern int dm_limit();
extern int dm_unlimit();
extern fastf_t dm_wrap();
extern void Nu_void();
extern int Nu_int0();
extern unsigned Nu_unsign();
extern void dm_configureWindowShape();
extern void dm_zbuffer();
extern void dm_lighting();
#if 0
extern Tcl_Interp *interp;   /* This must be defined by the application */
#endif

/* vers.c (created by libdm/Cakefile) */
extern char dm_version[];

/* clip.c */
extern int clip(vect_t, vect_t, vect_t, vect_t);
extern int vclip(vect_t, vect_t, register fastf_t *, register fastf_t *);

#endif /* SEEN_DM_H */
