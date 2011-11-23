/*                          D M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libdm */
/** @{ */
/** @file dm.h
 *
 */

#ifndef __DM_H__
#define __DM_H__

#include "common.h"

#include "bu.h"
#include "vmath.h"
#include "ged.h"

#define USE_FBSERV 1

#ifdef USE_FBSERV
#  include "fbserv_obj.h"
#endif

#ifndef DM_EXPORT
#  if defined(DM_DLL_EXPORTS) && defined(DM_DLL_IMPORTS)
#    error "Only DM_DLL_EXPORTS or DM_DLL_IMPORTS can be defined, not both."
#  elif defined(DM_DLL_EXPORTS)
#    define DM_EXPORT __declspec(dllexport)
#  elif defined(DM_DLL_IMPORTS)
#    define DM_EXPORT __declspec(dllimport)
#  else
#    define DM_EXPORT
#  endif
#endif

#define DM_NULL (struct dm *)NULL
#define DM_MIN (-2048)
#define DM_MAX (2047)

#define DM_O(_m) offsetof(struct dm, _m)

#define GED_MAX 2047.0
#define GED_MIN -2048.0
#define GED_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

/*
 * Display coordinate conversion:
 * GED is using -2048..+2048,
 * X is 0..width, 0..height
 */
#define DIVBY4096(x) (((double)(x))*INV_4096)
#define GED_TO_Xx(_dmp, x) ((int)((DIVBY4096(x)+0.5)*_dmp->dm_width))
#define GED_TO_Xy(_dmp, x) ((int)((0.5-DIVBY4096(x))*_dmp->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->dm_width - 0.5) * GED_RANGE))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->dm_height) * GED_RANGE))

/* +-2048 to +-1 */
#define GED_TO_PM1(x) (((fastf_t)(x))*INV_GED)

#ifdef IR_KNOBS
#  define NOISE 16		/* Size of dead spot on knob */
#endif

/* the font used depends on the size of the window opened */
#define FONTBACK "-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5 "5x7"
#define FONT6 "6x10"
#define FONT7 "7x13"
#define FONT8 "8x13"
#define FONT9 "9x15"
#define FONT10 "10x20"
#define FONT12 "12x24"

#if defined(_WIN32) && !defined(__CYGWIN__)
#  define DM_VALID_FONT_SIZE(_size) (14 <= (_size) && (_size) <= 29)
#else
#  define DM_VALID_FONT_SIZE(_size) (5 <= (_size) && (_size) <= 12 && (_size) != 11)
#  define DM_FONT_SIZE_TO_NAME(_size) (((_size) == 5) ? FONT5 : (((_size) == 6) ? FONT6 : (((_size) == 7) ? FONT7 : (((_size) == 8) ? FONT8 : (((_size) == 9) ? FONT9 : (((_size) == 10) ? FONT10 : FONT12))))))
#endif

/* Display Manager Types */
#define DM_TYPE_BAD     -1
#define DM_TYPE_NULL	0
#define DM_TYPE_PLOT	1
#define DM_TYPE_PS	2
#define DM_TYPE_X	3
#define DM_TYPE_OGL	4
#define DM_TYPE_GLX	5
#define DM_TYPE_PEX	6
#define DM_TYPE_WGL     7
#define DM_TYPE_TK	8
#define DM_TYPE_RTGL	9

/* Line Styles */
#define DM_SOLID_LINE 0
#define DM_DASHED_LINE 1

#define IS_DM_TYPE_NULL(_t) ((_t) == DM_TYPE_NULL)
#define IS_DM_TYPE_PLOT(_t) ((_t) == DM_TYPE_PLOT)
#define IS_DM_TYPE_PS(_t) ((_t) == DM_TYPE_PS)
#define IS_DM_TYPE_X(_t) ((_t) == DM_TYPE_X)
#define IS_DM_TYPE_TK(_t) ((_t) == DM_TYPE_TK)
#define IS_DM_TYPE_OGL(_t) ((_t) == DM_TYPE_OGL)
#define IS_DM_TYPE_GLX(_t) ((_t) == DM_TYPE_GLX)
#define IS_DM_TYPE_PEX(_t) ((_t) == DM_TYPE_PEX)
#define IS_DM_TYPE_WGL(_t) ((_t) == DM_TYPE_WGL)
#define IS_DM_TYPE_RTGL(_t) ((_t) == DM_TYPE_RTGL)

#define GET_DM(p, structure, w, hp) { \
	register struct structure *tp; \
	for (BU_LIST_FOR(tp, structure, hp)) { \
	    if (w == tp->win) { \
		(p) = tp; \
		break; \
	    } \
	} \
	\
	if (BU_LIST_IS_HEAD(tp, hp)) \
	    p = (struct structure *)NULL; \
    }


/* Colors */
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
#define DM_COPY_COLOR(_dr, _dg, _db, _sr, _sg, _sb) {\
	(_dr) = (_sr);\
	(_dg) = (_sg);\
	(_db) = (_sb); }
#define DM_SAME_COLOR(_dr, _dg, _db, _sr, _sg, _sb)(\
	(_dr) == (_sr) &&\
	(_dg) == (_sg) &&\
	(_db) == (_sb))
#if defined(DM_X) || defined(DM_OGL)
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask) {	\
	_shift = 24 - _shift;				\
	switch (_shift) {				\
	    case 0:					\
		_mask >>= 24;				\
		break;					\
	    case 8:					\
		_mask >>= 8;				\
		break;					\
	    case 16:					\
		_mask <<= 8;				\
		break;					\
	    case 24:					\
		_mask <<= 24;				\
		break;					\
	}						\
    }
#else
/* Do nothing */
#define DM_REVERSE_COLOR_BYTE_ORDER(_shift, _mask)
#endif

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


/**
 * Interface to a specific Display Manager
 */
struct dm {
    int (*dm_close)();
    int (*dm_drawBegin)();	/**< @brief formerly dmr_prolog */
    int (*dm_drawEnd)();		/**< @brief formerly dmr_epilog */
    int (*dm_normal)();
    int (*dm_loadMatrix)();
    int (*dm_drawString2D)();	/**< @brief formerly dmr_puts */
    int (*dm_drawLine2D)();	/**< @brief formerly dmr_2d_line */
    int (*dm_drawLine3D)();
    int (*dm_drawLines3D)();
    int (*dm_drawPoint2D)();
    int (*dm_drawVList)();
    int (*dm_drawVListHiddenLine)();
    int (*dm_draw)(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data);	/**< @brief formerly dmr_object */
    int (*dm_setFGColor)(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
    int (*dm_setBGColor)(struct dm *, unsigned char, unsigned char, unsigned char);
    int (*dm_setLineAttr)();	/**< @brief currently - linewidth, (not-)dashed */
    int (*dm_configureWin)();
    int (*dm_setWinBounds)();
    int (*dm_setLight)();
    int (*dm_setTransparency)();
    int (*dm_setDepthMask)();
    int (*dm_setZBuffer)();
    int (*dm_debug)();		/**< @brief Set DM debug level */
    int (*dm_beginDList)();
    int (*dm_endDList)();
    int (*dm_drawDList)();
    int (*dm_freeDLists)();
    int (*dm_getDisplayImage)(struct dm *dmp, unsigned char **image);
    void (*dm_reshape)();
    unsigned long dm_id;          /**< @brief window id */
    int dm_displaylist;		/**< @brief !0 means device has displaylist */
    int dm_stereo;                /**< @brief stereo flag */
    double dm_bound;		/**< @brief zoom-in limit */
    int dm_boundFlag;
    char *dm_name;		/**< @brief short name of device */
    char *dm_lname;		/**< @brief long name of device */
    int dm_type;			/**< @brief display manager type */
    int dm_top;                   /**< @brief !0 means toplevel window */
    int dm_width;
    int dm_height;
    int dm_bytes_per_pixel;
    int dm_bits_per_channel;  /* bits per color channel */
    int dm_lineWidth;
    int dm_lineStyle;
    fastf_t dm_aspect;
    fastf_t *dm_vp;		/**< @brief (FIXME: ogl still depends on this) Viewscale pointer */
    struct dm_vars dm_vars;	/**< @brief display manager dependant variables */
    struct bu_vls dm_pathName;	/**< @brief full Tcl/Tk name of drawing window */
    struct bu_vls dm_tkName;	/**< @brief short Tcl/Tk name of drawing window */
    struct bu_vls dm_dName;	/**< @brief Display name */
    unsigned char dm_bg[3];	/**< @brief background color */
    unsigned char dm_fg[3];	/**< @brief foreground color */
    vect_t dm_clipmin;		/**< @brief minimum clipping vector */
    vect_t dm_clipmax;		/**< @brief maximum clipping vector */
    int dm_debugLevel;		/**< @brief !0 means debugging */
    int dm_perspective;		/**< @brief !0 means perspective on */
    int dm_light;			/**< @brief !0 means lighting on */
    int dm_transparency;		/**< @brief !0 means transparency on */
    int dm_depthMask;		/**< @brief !0 means depth buffer is writable */
    int dm_zbuffer;		/**< @brief !0 means zbuffer on */
    int dm_zclip;			/**< @brief !0 means zclipping */
    int dm_clearBufferAfter;	/**< @brief 1 means clear back buffer after drawing and swap */
    int dm_fontsize;		/**< @brief !0 override's the auto font size */
    Tcl_Interp *dm_interp;	/**< @brief Tcl interpreter */
};


/**
 * D M _ O B J
 *@brief
 * A display manager object is used for interacting with a display manager.
 */
struct dm_obj {
    struct bu_list l;
    struct bu_vls dmo_name;		/**< @brief display manager object name/cmd */
    struct dm *dmo_dmp;		/**< @brief display manager pointer */
#ifdef USE_FBSERV
    struct fbserv_obj dmo_fbs;		/**< @brief fbserv object */
#endif
    struct bu_observer dmo_observers;		/**< @brief fbserv observers */
    mat_t viewMat;
    int (*dmo_drawLabelsHook)();
    void *dmo_drawLabelsHookClientData;
};


#define DM_OPEN(_interp, _type, _argc, _argv) dm_open(_interp, _type, _argc, _argv)
#define DM_CLOSE(_dmp) _dmp->dm_close(_dmp)
#define DM_DRAW_BEGIN(_dmp) _dmp->dm_drawBegin(_dmp)
#define DM_DRAW_END(_dmp) _dmp->dm_drawEnd(_dmp)
#define DM_NORMAL(_dmp) _dmp->dm_normal(_dmp)
#define DM_LOADMATRIX(_dmp, _mat, _eye) _dmp->dm_loadMatrix(_dmp, _mat, _eye)
#define DM_DRAW_STRING_2D(_dmp, _str, _x, _y, _size, _use_aspect) _dmp->dm_drawString2D(_dmp, _str, _x, _y, _size, _use_aspect)
#define DM_DRAW_LINE_2D(_dmp, _x1, _y1, _x2, _y2) _dmp->dm_drawLine2D(_dmp, _x1, _y1, _x2, _y2)
#define DM_DRAW_LINE_3D(_dmp, _pt1, _pt2) _dmp->dm_drawLine3D(_dmp, _pt1, _pt2)
#define DM_DRAW_LINES_3D(_dmp, _npoints, _points, _sflag) _dmp->dm_drawLines3D(_dmp, _npoints, _points, _sflag)
#define DM_DRAW_POINT_2D(_dmp, _x, _y) _dmp->dm_drawPoint2D(_dmp, _x, _y)
#define DM_DRAW_VLIST(_dmp, _vlist) _dmp->dm_drawVList(_dmp, _vlist)
#define DM_DRAW_VLIST_HIDDEN_LINE(_dmp, _vlist) _dmp->dm_drawVListHiddenLine(_dmp, _vlist)
#define DM_DRAW(_dmp, _callback, _data) _dmp->dm_draw(_dmp, _callback, _data)
#define DM_SET_FGCOLOR(_dmp, _r, _g, _b, _strict, _transparency) _dmp->dm_setFGColor(_dmp, _r, _g, _b, _strict, _transparency)
#define DM_SET_BGCOLOR(_dmp, _r, _g, _b) _dmp->dm_setBGColor(_dmp, _r, _g, _b)
#define DM_SET_LINE_ATTR(_dmp, _width, _dashed) _dmp->dm_setLineAttr(_dmp, _width, _dashed)
#define DM_CONFIGURE_WIN(_dmp, _force) _dmp->dm_configureWin((_dmp), (_force))
#define DM_SET_WIN_BOUNDS(_dmp, _w) _dmp->dm_setWinBounds(_dmp, _w)
#define DM_SET_LIGHT(_dmp, _on) _dmp->dm_setLight(_dmp, _on)
#define DM_SET_TRANSPARENCY(_dmp, _on) _dmp->dm_setTransparency(_dmp, _on)
#define DM_SET_DEPTH_MASK(_dmp, _on) _dmp->dm_setDepthMask(_dmp, _on)
#define DM_SET_ZBUFFER(_dmp, _on) _dmp->dm_setZBuffer(_dmp, _on)
#define DM_DEBUG(_dmp, _lvl) _dmp->dm_debug(_dmp, _lvl)
#define DM_BEGINDLIST(_dmp, _list) _dmp->dm_beginDList(_dmp, _list)
#define DM_ENDDLIST(_dmp) _dmp->dm_endDList(_dmp)
#define DM_DRAWDLIST(_dmp, _list) _dmp->dm_drawDList(_dmp, _list)
#define DM_FREEDLISTS(_dmp, _list, _range) _dmp->dm_freeDLists(_dmp, _list, _range)
#define DM_GET_DISPLAY_IMAGE(_dmp, _image) _dmp->dm_getDisplayImage(_dmp, _image)

DM_EXPORT extern struct dm dm_Null;
DM_EXPORT extern struct dm dm_ogl;
DM_EXPORT extern struct dm dm_plot;
DM_EXPORT extern struct dm dm_ps;
DM_EXPORT extern struct dm dm_rtgl;
DM_EXPORT extern struct dm dm_tk;
DM_EXPORT extern struct dm dm_wgl;
DM_EXPORT extern struct dm dm_X;

DM_EXPORT extern int Dm_Init();
DM_EXPORT extern struct dm *dm_open(Tcl_Interp *interp,
				    int type,
				    int argc,
				    const char *argv[]);
DM_EXPORT extern int dm_share_dlist(struct dm *dmp1,
				    struct dm *dmp2);
DM_EXPORT extern fastf_t dm_Xx2Normal(struct dm *dmp,
				      int x);
DM_EXPORT extern int dm_Normal2Xx(struct dm *dmp,
				  fastf_t f);
DM_EXPORT extern fastf_t dm_Xy2Normal(struct dm *dmp,
				      int y,
				      int use_aspect);
DM_EXPORT extern int dm_Normal2Xy(struct dm *dmp,
				  fastf_t f,
				  int use_aspect);
DM_EXPORT extern void dm_fogHint(struct dm *dmp,
				 int fastfog);
DM_EXPORT extern int dm_processOptions();
DM_EXPORT extern int dm_limit(int i);
DM_EXPORT extern int dm_unlimit(int i);
DM_EXPORT extern fastf_t dm_wrap(fastf_t f);
DM_EXPORT extern void Nu_void();
DM_EXPORT extern int Nu_int0();
DM_EXPORT extern unsigned Nu_unsign();

/* adc.c */
DM_EXPORT extern void dm_draw_adc(struct dm *dmp,
				  struct ged_view *gvp);

/* axes.c */
DM_EXPORT extern void dm_draw_data_axes(struct dm *dmp,
					fastf_t viewSize,
					struct ged_data_axes_state *gdasp);

DM_EXPORT extern void dm_draw_axes(struct dm *dmp,
				   fastf_t viewSize,
				   const mat_t rmat,
				   struct ged_axes_state *gasp);

/* clip.c */
DM_EXPORT extern int clip(fastf_t *,
			  fastf_t *,
			  fastf_t *,
			  fastf_t *);
DM_EXPORT extern int vclip(fastf_t *,
			   fastf_t *,
			   fastf_t *,
			   fastf_t *);

/* focus.c */
DM_EXPORT extern void dm_applicationfocus(void);

/* grid.c */
DM_EXPORT extern void dm_draw_grid(struct dm *dmp,
				   struct ged_grid_state *ggsp,
				   struct ged_view *gvp,
				   fastf_t base2local);

/* labels.c */
DM_EXPORT extern int dm_draw_labels(struct dm *dmp,
				    struct rt_wdb *wdbp,
				    char *name,
				    mat_t viewmat,
				    int *labelsColor,
				    int (*labelsHook)(),
				    ClientData labelsHookClientdata);

/* rect.c */
DM_EXPORT extern void dm_draw_rect(struct dm *dmp,
				   struct ged_rect_state *grsp);

/* scale.c */
DM_EXPORT extern void dm_draw_scale(struct dm *dmp,
				    fastf_t viewSize,
				    int *lineColor,
				    int *textColor);

/* vers.c */
DM_EXPORT extern const char *dm_version(void);


/************************************************/
/* dm-*.c macros for autogenerating common code */
/************************************************/

#define HIDDEN_DM_FUNCTION_PROTOTYPES(_dmtype) \
    HIDDEN int _dmtype##_close(struct dm *dmp); \
    HIDDEN int _dmtype##_drawBegin(struct dm *dmp); \
    HIDDEN int _dmtype##_drawEnd(struct dm *dmp); \
    HIDDEN int _dmtype##_normal(struct dm *dmp); \
    HIDDEN int _dmtype##_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye); \
    HIDDEN int _dmtype##_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int size, int use_aspect); \
    HIDDEN int _dmtype##_drawLine2D(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2); \
    HIDDEN int _dmtype##_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2); \
    HIDDEN int _dmtype##_drawLines3D(struct dm *dmp, int npoints, point_t *points); \
    HIDDEN int _dmtype##_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y); \
    HIDDEN int _dmtype##_drawVList(struct dm *dmp, struct bn_vlist *vp); \
    HIDDEN int _dmtype##_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data); \
    HIDDEN int _dmtype##_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency); \
    HIDDEN int _dmtype##_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b); \
    HIDDEN int _dmtype##_setLineAttr(struct dm *dmp, int width, int style); \
    HIDDEN int _dmtype##_configureWin_guts(struct dm *dmp, int force); \
    HIDDEN int _dmtype##_configureWin(struct dm *dmp, int force);		      \
    HIDDEN int _dmtype##_setLight(struct dm *dmp, int lighting_on); \
    HIDDEN int _dmtype##_setTransparency(struct dm *dmp, int transparency_on); \
    HIDDEN int _dmtype##_setDepthMask(struct dm *dmp, int depthMask_on); \
    HIDDEN int _dmtype##_setZBuffer(struct dm *dmp, int zbuffer_on); \
    HIDDEN int _dmtype##_setWinBounds(struct dm *dmp, fastf_t *w); \
    HIDDEN int _dmtype##_debug(struct dm *dmp, int lvl); \
    HIDDEN int _dmtype##_beginDList(struct dm *dmp, unsigned int list); \
    HIDDEN int _dmtype##_endDList(struct dm *dmp); \
    HIDDEN int _dmtype##_drawDList(struct dm *dmp, unsigned int list); \
    HIDDEN int _dmtype##_freeDLists(struct dm *dmp, unsigned int list, int range); \
    HIDDEN int _dmtype##_getDisplayImage(struct dm *dmp, unsigned char **image);

#endif /* __DM_H__ */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
