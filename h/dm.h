#ifndef SEEN_DM_H
#define SEEN_DM_H

#if IR_KNOBS
#define NOISE 32		/* Size of dead spot on knob */
#endif

/* the font used depends on the size of the window opened */
#define FONTBACK	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5	"5x7"
#define FONT6	"6x10"
#define FONT7	"7x13"
#define FONT8	"8x13"
#define FONT9	"9x15"

/* Display Manager Types */
#define DM_TYPE_NULL	0
#define DM_TYPE_PLOT	1
#define DM_TYPE_PS	2
#define DM_TYPE_X	3
#define DM_TYPE_PEX	4
#define DM_TYPE_OGL	5
#define DM_TYPE_GLX	6

#define IS_DM_TYPE_NULL(_t) ((_t) == DM_TYPE_NULL)
#define IS_DM_TYPE_PLOT(_t) ((_t) == DM_TYPE_PLOT)
#define IS_DM_TYPE_PS(_t) ((_t) == DM_TYPE_PS)
#define IS_DM_TYPE_X(_t) ((_t) == DM_TYPE_X)
#define IS_DM_TYPE_PEX(_t) ((_t) == DM_TYPE_PEX)
#define IS_DM_TYPE_OGL(_t) ((_t) == DM_TYPE_OGL)
#define IS_DM_TYPE_GLX(_t) ((_t) == DM_TYPE_GLX)

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
#define DM_COLOR_HI	(short)230
#define DM_COLOR_LOW	(short)0
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
#define DM_BLACK	DM_BLACK_R,DM_BLACK_G,DM_BLACK_B
#define DM_RED		DM_RED_R,DM_RED_G,DM_RED_B
#define DM_BLUE		DM_BLUE_R,DM_BLUE_G,DM_BLUE_B
#define DM_YELLOW	DM_YELLOW_R,DM_YELLOW_G,DM_YELLOW_B
#define DM_WHITE	DM_WHITE_R,DM_WHITE_G,DM_WHITE_B
#define DM_SET_COLOR(dr,dg,db,sr,sg,sb){\
	(dr) = (sr);\
	(dg) = (sg);\
	(db) = (sb); }
#define DM_SAME_COLOR(dr,dg,db,sr,sg,sb)(\
	(dr) == (sr) &&\
	(dg) == (sg) &&\
	(db) == (sb))

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

/* Interface to a specific Display Manager */
struct dm {
  int (*dm_open)();
  int (*dm_close)();
  int (*dm_drawBegin)();	/* was dmr_prolog */
  int (*dm_drawEnd)();		/* was dmr_epilog */
  int (*dm_normal)();
  int (*dm_newrot)();
  int (*dm_drawString2D)();	/* was dmr_puts */
  int (*dm_drawLine2D)();	/* was dmr_2d_line */
  int (*dm_drawVertex2D)();
  int (*dm_drawVList)();	/* was dmr_object */
  int (*dm_setColor)();
  int (*dm_setLineAttr)();	/* currently - linewidth, (not-)dashed */
  unsigned (*dm_cvtvecs)();	/* returns size requirement of subr */
  unsigned (*dm_load)();	/* DMA the subr to device */
  int (*dm_setWinBounds)();
  int (*dm_debug)();		/* Set DM debug level */
  int (*dm_eventHandler)();	/* application provided dm-specific event handler */
  int dm_displaylist;		/* !0 means device has displaylist */
  double dm_bound;		/* zoom-in limit */
  char *dm_name;		/* short name of device */
  char *dm_lname;		/* long name of device */
  struct mem_map *dm_map;	/* displaylist mem map */
  genptr_t dm_vars;		/* pointer to display manager dependant variables */
  struct bu_vls dm_pathName;	/* full Tcl/Tk name of drawing window */
  struct bu_vls dm_initWinProc; /* Tcl/Tk procedure for initializing the drawing window */
  char dm_dname[80];		/* Display name */
  fastf_t *dm_vp;		/* XXX--ogl still depends on this--XXX Viewscale pointer */
};

extern int dm_process_options();
extern int dm_limit();
extern int dm_unlimit();
extern fastf_t dm_wrap();
extern void Nu_void();
extern int Nu_int0();
extern unsigned Nu_unsign();
extern Tcl_Interp *interp;   /* This must be defined by the application */
#endif /* SEEN_DM_H */
