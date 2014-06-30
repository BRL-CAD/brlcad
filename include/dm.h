/*                          D M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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

#ifndef DM_H
#define DM_H

#include "common.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

#define USE_FBSERV 1

#ifdef USE_FBSERV
#  include "fbserv_obj.h"
#endif

#include "./dm/defines.h"

#define DM_NULL (struct dm *)NULL
#define DM_MIN (-2048)
#define DM_MAX (2047)

#define DM_O(_m) offsetof(struct dm, _m)

#define DM_RANGE 4095.0
#define INV_GED 0.00048828125
#define INV_4096 0.000244140625

/*
 * Display coordinate conversion:
 * GED is using -2048..+2048,
 * X is 0..width, 0..height
 */
#define DIVBY4096(x) (((double)(x))*INV_4096)
#define DM_TO_Xx(_dmp, x) ((int)((DIVBY4096(x)+0.5)*_dmp->dm_width))
#define DM_TO_Xy(_dmp, x) ((int)((0.5-DIVBY4096(x))*_dmp->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->dm_width - 0.5) * DM_RANGE))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->dm_height) * DM_RANGE))

/* +-2048 to +-1 */
#define DM_TO_PM1(x) (((fastf_t)(x))*INV_GED)

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
#define DM_TYPE_TXT	10
#define DM_TYPE_QT	11
#define DM_TYPE_OSG	12
#define DM_TYPE_X	3

/* Line Styles */
#define DM_SOLID_LINE 0
#define DM_DASHED_LINE 1

#define IS_DM_TYPE_NULL(_t) ((_t) == DM_TYPE_NULL)
#define IS_DM_TYPE_X(_t) ((_t) == DM_TYPE_X)
#define IS_DM_TYPE_TXT(_t) ((_t) == DM_TYPE_TXT)
#define IS_DM_TYPE_QT(_t) ((_t) == DM_TYPE_QT)
#define IS_DM_TYPE_OSG(_t) ((_t) == DM_TYPE_OSG)

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
#if defined(DM_X) || defined(DM_OGL) || defined(DM_OSG)
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
    void *pub_vars;
    void *priv_vars;
};


/**
 * Interface to a specific Display Manager
 */
struct dm {
    int (*dm_close)(struct dm *dmp);
    int (*dm_drawBegin)(struct dm *dmp);	/**< @brief formerly dmr_prolog */
    int (*dm_drawEnd)(struct dm *dmp);		/**< @brief formerly dmr_epilog */
    int (*dm_normal)(struct dm *dmp);
    int (*dm_loadMatrix)(struct dm *dmp, fastf_t *mat, int which_eye);
    int (*dm_loadPMatrix)(struct dm *dmp, fastf_t *mat);
    int (*dm_drawString2D)(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);	/**< @brief formerly dmr_puts */
    int (*dm_drawLine2D)(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);	/**< @brief formerly dmr_2d_line */
    int (*dm_drawLine3D)(struct dm *dmp, point_t pt1, point_t pt2);
    int (*dm_drawLines3D)(struct dm *dmp, int npoints, point_t *points, int sflag);
    int (*dm_drawPoint2D)(struct dm *dmp, fastf_t x, fastf_t y);
    int (*dm_drawPoint3D)(struct dm *dmp, point_t point);
    int (*dm_drawPoints3D)(struct dm *dmp, int npoints, point_t *points);
    int (*dm_drawVList)(struct dm *dmp, struct bn_vlist *vp);
    int (*dm_drawVListHiddenLine)(struct dm *dmp, register struct bn_vlist *vp);
    int (*dm_draw)(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);	/**< @brief formerly dmr_object */
    int (*dm_setFGColor)(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
    int (*dm_setBGColor)(struct dm *, unsigned char, unsigned char, unsigned char);
    int (*dm_setLineAttr)(struct dm *dmp, int width, int style);	/**< @brief currently - linewidth, (not-)dashed */
    int (*dm_configureWin)(struct dm *dmp, int force);
    int (*dm_setWinBounds)(struct dm *dmp, fastf_t *w);
    int (*dm_setLight)(struct dm *dmp, int light_on);
    int (*dm_setTransparency)(struct dm *dmp, int transparency_on);
    int (*dm_setDepthMask)(struct dm *dmp, int depthMask_on);
    int (*dm_setZBuffer)(struct dm *dmp, int zbuffer_on);
    int (*dm_debug)(struct dm *dmp, int lvl);		/**< @brief Set DM debug level */
    int (*dm_logfile)(struct dm *dmp, const char *filename); /**< @brief Set DM log file */
    int (*dm_beginDList)(struct dm *dmp, unsigned int list);
    int (*dm_endDList)(struct dm *dmp);
    void (*dm_drawDList)(unsigned int list);
    int (*dm_freeDLists)(struct dm *dmp, unsigned int list, int range);
    int (*dm_genDLists)(struct dm *dmp, size_t range);
    int (*dm_getDisplayImage)(struct dm *dmp, unsigned char **image);
    void (*dm_reshape)(struct dm *dmp, int width, int height);
    int (*dm_makeCurrent)(struct dm *dmp);
    unsigned long dm_id;          /**< @brief window id */
    int dm_displaylist;		/**< @brief !0 means device has displaylist */
    int dm_stereo;                /**< @brief stereo flag */
    double dm_bound;		/**< @brief zoom-in limit */
    int dm_boundFlag;
    const char *dm_name;		/**< @brief short name of device */
    const char *dm_lname;		/**< @brief long name of device */
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
    struct dm_vars dm_vars;	/**< @brief display manager dependent variables */
    struct bu_vls dm_pathName;	/**< @brief full Tcl/Tk name of drawing window */
    struct bu_vls dm_tkName;	/**< @brief short Tcl/Tk name of drawing window */
    struct bu_vls dm_dName;	/**< @brief Display name */
    unsigned char dm_bg[3];	/**< @brief background color */
    unsigned char dm_fg[3];	/**< @brief foreground color */
    vect_t dm_clipmin;		/**< @brief minimum clipping vector */
    vect_t dm_clipmax;		/**< @brief maximum clipping vector */
    int dm_debugLevel;		/**< @brief !0 means debugging */
    struct bu_vls dm_log;   /**< @brief !NULL && !empty means log debug output to the file */
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

/**********************************************************************************************/
/** EXPERIMENTAL - considering a new design for the display
 * manager structure.  DO NOT USE!! */
#if 0

/* Will need bu_dlopen and bu_dlsym for a plugin system - look at how
 * liboptical's shaders are set up. */

/* Window management functions */
struct dm_window_functions {
    int (*dm_open)(struct dm *dmp, void *data);
    int (*dm_close)(struct dm *dmp, void *data);
    void *(*dm_context)(struct dm *dmp, void *data);
    int (*dm_configureWin)(struct dm *dmp, int force);
    int (*dm_setWinBounds)(struct dm *dmp, fastf_t *w);
}
struct dm_window_state {
    struct bu_vls dm_fullName;	/**< @brief full Tcl/Tk name of drawing window */
    struct bu_vls dm_shortName;	/**< @brief short Tcl/Tk name of drawing window */
    int width;
    int height;
}

/* Geometry view management functions */
struct dm_view_state {
    int dm_zclip;		/**< @brief !0 means zclipping */
    vect_t dm_clipmin;		/**< @brief minimum clipping vector */
    vect_t dm_clipmax;		/**< @brief maximum clipping vector */
    int dm_perspective;		/**< @brief !0 means perspective on */
    int dm_light;		/**< @brief !0 means lighting on */
    int dm_transparency;	/**< @brief !0 means transparency on */
    int dm_depthMask;		/**< @brief !0 means depth buffer is writable */
    int dm_zbuffer;		/**< @brief !0 means zbuffer on */
    int dm_fontsize;		/**< @brief !0 override's the auto font size */
    int dm_displaylist;		/**< @brief !0 means device has displaylist */
    int dm_stereo;                /**< @brief stereo flag */
    unsigned char dm_bg[3];	/**< @brief background color */
    unsigned char dm_fg[3];	/**< @brief foreground color */
    int width;
    int height;
    double fov;
    double aspect;
    double near_l;
    double far_l;
}
struct dm_view_functions {
    int (*dm_normal)(struct dm *dmp);
    int (*dm_set_perspective_view)(struct dm *dmp, double fov, double aspect, double near_l, double far_l);
    int (*dm_loadMatrix)(struct dm *dmp, fastf_t *mat, int which_eye);
    int (*dm_loadPMatrix)(struct dm *dmp, fastf_t *mat);
    int (*dm_setFGColor)(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
    int (*dm_setBGColor)(struct dm *, unsigned char, unsigned char, unsigned char);
    int (*dm_setLineAttr)(struct dm *dmp, int width, int style);	/**< @brief currently - linewidth, (not-)dashed */
    int (*dm_setTransparency)(struct dm *dmp, int transparency_on);
    int (*dm_setDepthMask)(struct dm *dmp, int depthMask_on);
    int (*dm_setZBuffer)(struct dm *dmp, int zbuffer_on);
    void (*dm_reshape)(struct dm *dmp, int width, int height);
}


/* Geometry drawing functions */
struct dm_draw_state {
    struct bu_list *objects;
}

struct dm_draw_functions {
    int (*dm_drawBegin)(struct dm *dmp);	/**< @brief formerly dmr_prolog */
    int (*dm_drawEnd)(struct dm *dmp);		/**< @brief formerly dmr_epilog */
     int (*dm_drawString2D)(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);	/**< @brief formerly dmr_puts */
    int (*dm_drawLine2D)(struct dm *dmp, fastf_t x_1, fastf_t y_1, fastf_t x_2, fastf_t y_2);	/**< @brief formerly dmr_2d_line */
    int (*dm_drawLine3D)(struct dm *dmp, point_t pt1, point_t pt2);
    int (*dm_drawLines3D)(struct dm *dmp, int npoints, point_t *points, int sflag);
    int (*dm_drawPoint2D)(struct dm *dmp, fastf_t x, fastf_t y);
    int (*dm_drawPoint3D)(struct dm *dmp, point_t point);
    int (*dm_drawPoints3D)(struct dm *dmp, int npoints, point_t *points);
    int (*dm_drawVList)(struct dm *dmp, struct bn_vlist *vp);
    int (*dm_drawVListHiddenLine)(struct dm *dmp, register struct bn_vlist *vp);
    int (*dm_draw)(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);	/**< @brief formerly dmr_object */
    int (*dm_beginDList)(struct dm *dmp, unsigned int list);
    int (*dm_endDList)(struct dm *dmp);
    void (*dm_drawDList)(unsigned int list);
    int (*dm_freeDLists)(struct dm *dmp, unsigned int list, int range);
    int (*dm_genDLists)(struct dm *dmp, size_t range);
}

struct display_manager {
    struct dm_window_state *dm_win_s;
    struct dm_window_functions *dm_win_f;
    struct dm_view_state *dm_view_s;
    struct dm_view_functions *dm_view_f;
    struct dm_draw_state *dm_draw_s;
    struct dm_draw_functions *dm_draw_f;
    const char *dm_name;		/**< @brief short name of device */
    const char *dm_lname;		/**< @brief long name of device */
    int (*dm_debug)(struct dm *dmp, int lvl);		/**< @brief Set DM debug level */
    int (*dm_getDisplayImage)(struct dm *dmp, unsigned char **image);
    void *dm_interp;
}
#endif

/**********************************************************************************************/




#define DM_OPEN(_interp, _type, _argc, _argv) dm_open(_interp, _type, _argc, _argv)
#define DM_CLOSE(_dmp) _dmp->dm_close(_dmp)
#define DM_DRAW_BEGIN(_dmp) _dmp->dm_drawBegin(_dmp)
#define DM_DRAW_END(_dmp) _dmp->dm_drawEnd(_dmp)
#define DM_NORMAL(_dmp) _dmp->dm_normal(_dmp)
#define DM_LOADMATRIX(_dmp, _mat, _eye) _dmp->dm_loadMatrix(_dmp, _mat, _eye)
#define DM_LOADPMATRIX(_dmp, _mat) _dmp->dm_loadPMatrix(_dmp, _mat)
#define DM_DRAW_STRING_2D(_dmp, _str, _x, _y, _size, _use_aspect) _dmp->dm_drawString2D(_dmp, _str, _x, _y, _size, _use_aspect)
#define DM_DRAW_LINE_2D(_dmp, _x1, _y1, _x2, _y2) _dmp->dm_drawLine2D(_dmp, _x1, _y1, _x2, _y2)
#define DM_DRAW_LINE_3D(_dmp, _pt1, _pt2) _dmp->dm_drawLine3D(_dmp, _pt1, _pt2)
#define DM_DRAW_LINES_3D(_dmp, _npoints, _points, _sflag) _dmp->dm_drawLines3D(_dmp, _npoints, _points, _sflag)
#define DM_DRAW_POINT_2D(_dmp, _x, _y) _dmp->dm_drawPoint2D(_dmp, _x, _y)
#define DM_DRAW_POINT_3D(_dmp, _pt) _dmp->dm_drawPoint3D(_dmp, _pt)
#define DM_DRAW_POINTS_3D(_dmp, _npoints, _points) _dmp->dm_drawPoints3D(_dmp, _npoints, _points)
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
#define DM_LOGFILE(_dmp, _lvl) _dmp->dm_logfile(_dmp, _lvl)
#define DM_BEGINDLIST(_dmp, _list) _dmp->dm_beginDList(_dmp, _list)
#define DM_ENDDLIST(_dmp) _dmp->dm_endDList(_dmp)
#define DM_DRAWDLIST(_dmp, _list) _dmp->dm_drawDList(_list)
#define DM_FREEDLISTS(_dmp, _list, _range) _dmp->dm_freeDLists(_dmp, _list, _range)
#define DM_GEN_DLISTS(_dmp, _range) _dmp->dm_genDLists(_dmp, _range)
#define DM_GET_DISPLAY_IMAGE(_dmp, _image) _dmp->dm_getDisplayImage(_dmp, _image)
#define DM_MAKE_CURRENT(_dmp) _dmp->dm_makeCurrent(_dmp)

__BEGIN_DECLS

DM_EXPORT extern struct dm dm_osg;
DM_EXPORT extern struct dm dm_X;
DM_EXPORT extern struct dm dm_txt;
DM_EXPORT extern struct dm dm_qt;

DM_EXPORT extern int Dm_Init(void *interp);
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
DM_EXPORT extern int dm_processOptions(struct dm *dmp, struct bu_vls *init_proc_vls, int argc, char **argv);
DM_EXPORT extern int dm_limit(int i);
DM_EXPORT extern int dm_unlimit(int i);
DM_EXPORT extern fastf_t dm_wrap(fastf_t f);



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
    HIDDEN int _dmtype##_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag); \
    HIDDEN int _dmtype##_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y); \
    HIDDEN int _dmtype##_drawPoint3D(struct dm *dmp, point_t point); \
    HIDDEN int _dmtype##_drawPoints3D(struct dm *dmp, int npoints, point_t *points); \
    HIDDEN int _dmtype##_drawVList(struct dm *dmp, struct bn_vlist *vp); \
    HIDDEN int _dmtype##_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data); \
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

/* View and draw state moved from libged.  *not* final form, but merely
 * a first step in moving the logic from libged to libdm. */

struct dm_adc_state {
    int		gas_draw;
    int		gas_dv_x;
    int		gas_dv_y;
    int		gas_dv_a1;
    int		gas_dv_a2;
    int		gas_dv_dist;
    fastf_t	gas_pos_model[3];
    fastf_t	gas_pos_view[3];
    fastf_t	gas_pos_grid[3];
    fastf_t	gas_a1;
    fastf_t	gas_a2;
    fastf_t	gas_dst;
    int		gas_anchor_pos;
    int		gas_anchor_a1;
    int		gas_anchor_a2;
    int		gas_anchor_dst;
    fastf_t	gas_anchor_pt_a1[3];
    fastf_t	gas_anchor_pt_a2[3];
    fastf_t	gas_anchor_pt_dst[3];
    int		gas_line_color[3];
    int		gas_tick_color[3];
    int		gas_line_width;
};

struct dm_axes_state {
    int       gas_draw;
    point_t   gas_axes_pos;		/* in model coordinates */
    fastf_t   gas_axes_size; 		/* in view coordinates */
    int	      gas_line_width;    	/* in pixels */
    int	      gas_pos_only;
    int	      gas_axes_color[3];
    int	      gas_label_color[3];
    int	      gas_triple_color;
    int	      gas_tick_enabled;
    int	      gas_tick_length;		/* in pixels */
    int	      gas_tick_major_length; 	/* in pixels */
    fastf_t   gas_tick_interval; 	/* in mm */
    int	      gas_ticks_per_major;
    int	      gas_tick_threshold;
    int	      gas_tick_color[3];
    int	      gas_tick_major_color[3];
};

struct dm_data_arrow_state {
    int       gdas_draw;
    int	      gdas_color[3];
    int	      gdas_line_width;    	/* in pixels */
    int       gdas_tip_length;
    int       gdas_tip_width;
    int       gdas_num_points;
    point_t   *gdas_points;		/* in model coordinates */
};

struct dm_data_axes_state {
    int       gdas_draw;
    int	      gdas_color[3];
    int	      gdas_line_width;    	/* in pixels */
    fastf_t   gdas_size; 		/* in view coordinates */
    int       gdas_num_points;
    point_t   *gdas_points;		/* in model coordinates */
};

struct dm_data_label_state {
    int		gdls_draw;
    int		gdls_color[3];
    int		gdls_num_labels;
    int		gdls_size;
    char	**gdls_labels;
    point_t	*gdls_points;
};

struct dm_data_line_state {
    int       gdls_draw;
    int	      gdls_color[3];
    int	      gdls_line_width;    	/* in pixels */
    int       gdls_num_points;
    point_t   *gdls_points;		/* in model coordinates */
};

struct dm_grid_state {
    int		ggs_draw;		/* draw grid */
    int		ggs_snap;		/* snap to grid */
    fastf_t	ggs_anchor[3];
    fastf_t	ggs_res_h;		/* grid resolution in h */
    fastf_t	ggs_res_v;		/* grid resolution in v */
    int		ggs_res_major_h;	/* major grid resolution in h */
    int		ggs_res_major_v;	/* major grid resolution in v */
    int		ggs_color[3];
};

struct dm_other_state {
    int gos_draw;
    int	gos_line_color[3];
    int	gos_text_color[3];
};

struct dm_rect_state {
    int		grs_active;	/* 1 - actively drawing a rectangle */
    int		grs_draw;	/* draw rubber band rectangle */
    int		grs_line_width;
    int		grs_line_style;  /* 0 - solid, 1 - dashed */
    int		grs_pos[2];	/* Position in image coordinates */
    int		grs_dim[2];	/* Rectangle dimension in image coordinates */
    fastf_t	grs_x;		/* Corner of rectangle in normalized     */
    fastf_t	grs_y;		/* ------ view coordinates (i.e. +-1.0). */
    fastf_t	grs_width;	/* Width and height of rectangle in      */
    fastf_t	grs_height;	/* ------ normalized view coordinates.   */
    int		grs_bg[3];	/* Background color */
    int		grs_color[3];	/* Rectangle color */
    int		grs_cdim[2];	/* Canvas dimension in pixels */
    fastf_t	grs_aspect;	/* Canvas aspect ratio */
};

struct dm_run_rt {
    struct bu_list l;
#if defined(_WIN32) && !defined(__CYGWIN__)
    HANDLE fd;
    HANDLE hProcess;
    DWORD pid;

#  ifdef TCL_OK
    Tcl_Channel chan;
#  else
    void *chan;
#  endif
#else
    int fd;
    int pid;
#endif
    int aborted;
};

/* FIXME: should be private */
struct dm_qray_color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

/* FIXME: should be private */
struct dm_qray_fmt {
    char type;
    struct bu_vls fmt;
};

struct dm_drawable {
    struct bu_list		l;
    struct bu_list		*gd_headDisplay;		/**< @brief  head of display list */
    struct bu_list		*gd_headVDraw;		/**< @brief  head of vdraw list */
    struct vd_curve		*gd_currVHead;		/**< @brief  current vdraw head */
    struct solid		*gd_freeSolids;		/**< @brief  ptr to head of free solid list */

    char			**gd_rt_cmd;
    int				gd_rt_cmd_len;
    struct dm_run_rt		gd_headRunRt;		/**< @brief  head of forked rt processes */

    void			(*gd_rtCmdNotify)(int aborted);	/**< @brief  function called when rt command completes */

    int				gd_uplotOutputMode;	/**< @brief  output mode for unix plots */

    /* qray state */
    struct bu_vls		gd_qray_basename;	/**< @brief  basename of query ray vlist */
    struct bu_vls		gd_qray_script;		/**< @brief  query ray script */
    char			gd_qray_effects;	/**< @brief  t for text, g for graphics or b for both */
    int				gd_qray_cmd_echo;	/**< @brief  0 - don't echo command, 1 - echo command */
    struct dm_qray_fmt		*gd_qray_fmts;
    struct dm_qray_color	gd_qray_odd_color;
    struct dm_qray_color	gd_qray_even_color;
    struct dm_qray_color	gd_qray_void_color;
    struct dm_qray_color	gd_qray_overlap_color;
    int				gd_shaded_mode;		/**< @brief  1 - draw bots shaded by default */
};

typedef enum { gctUnion, gctDifference, gctIntersection, gctXor } DmClipType;

typedef struct {
    size_t    gpc_num_points;
    point_t   *gpc_point;		/* in model coordinates */
} dm_poly_contour;

typedef struct {
    size_t		gp_num_contours;
    int			gp_color[3];
    int			gp_line_width;    	/* in pixels */
    int			gp_line_style;
    int			*gp_hole;
    dm_poly_contour	*gp_contour;
} dm_polygon;

typedef struct {
    size_t	gp_num_polygons;
    dm_polygon	*gp_polygon;
} dm_polygons;

typedef struct {
    int			gdps_draw;
    int			gdps_color[3];
    int			gdps_line_width;    	/* in pixels */
    int			gdps_line_style;
    int			gdps_cflag;             /* contour flag */
    size_t		gdps_target_polygon_i;
    size_t		gdps_curr_polygon_i;
    size_t		gdps_curr_point_i;
    point_t		gdps_prev_point;
    DmClipType		gdps_clip_type;
    fastf_t		gdps_scale;
    point_t		gdps_origin;
    mat_t		gdps_rotation;
    mat_t		gdps_view2model;
    mat_t		gdps_model2view;
    dm_polygons	gdps_polygons;
    fastf_t		gdps_data_vZ;
} dm_data_polygon_state;

struct dm_view {
    struct bu_list		l;
    fastf_t			gv_scale;
    fastf_t			gv_size;		/**< @brief  2.0 * scale */
    fastf_t			gv_isize;		/**< @brief  1.0 / size */
    fastf_t			gv_perspective;		/**< @brief  perspective angle */
    vect_t			gv_aet;
    vect_t			gv_eye_pos;		/**< @brief  eye position */
    vect_t			gv_keypoint;
    char			gv_coord;		/**< @brief  coordinate system */
    char			gv_rotate_about;	/**< @brief  indicates what point rotations are about */
    mat_t			gv_rotation;
    mat_t			gv_center;
    mat_t			gv_model2view;
    mat_t			gv_pmodel2view;
    mat_t			gv_view2model;
    mat_t			gv_pmat;		/**< @brief  perspective matrix */
    void 			(*gv_callback)();	/**< @brief  called in dm_view_update with gvp and gv_clientData */
    void *			gv_clientData;		/**< @brief  passed to gv_callback */
    fastf_t			gv_prevMouseX;
    fastf_t			gv_prevMouseY;
    fastf_t			gv_minMouseDelta;
    fastf_t			gv_maxMouseDelta;
    fastf_t			gv_rscale;
    fastf_t			gv_sscale;
    int				gv_mode;
    int				gv_zclip;
    struct dm_adc_state 	gv_adc;
    struct dm_axes_state 	gv_model_axes;
    struct dm_axes_state 	gv_view_axes;
    struct dm_data_arrow_state gv_data_arrows;
    struct dm_data_axes_state 	gv_data_axes;
    struct dm_data_label_state gv_data_labels;
    struct dm_data_line_state  gv_data_lines;
    dm_data_polygon_state 	gv_data_polygons;
    struct dm_data_arrow_state	gv_sdata_arrows;
    struct dm_data_axes_state 	gv_sdata_axes;
    struct dm_data_label_state gv_sdata_labels;
    struct dm_data_line_state 	gv_sdata_lines;
    dm_data_polygon_state 	gv_sdata_polygons;
    struct dm_grid_state 	gv_grid;
    struct dm_other_state 	gv_center_dot;
    struct dm_other_state 	gv_prim_labels;
    struct dm_other_state 	gv_view_params;
    struct dm_other_state 	gv_view_scale;
    struct dm_rect_state 	gv_rect;
    int				gv_adaptive_plot;
    int				gv_redraw_on_zoom;
    int				gv_x_samples;
    int				gv_y_samples;
    fastf_t			gv_point_scale;
    fastf_t			gv_curve_scale;
    fastf_t			gv_data_vZ;
    /* from ged_dm_view */
    struct bu_vls		gdv_callback;
    struct bu_vls		gdv_edit_motion_delta_callback;
    struct bu_vls		gdv_name;
    struct dm			*gdv_dmp;
    struct fbserv_obj		gdv_fbs;
    /* GED pointer - must go away */
    void *gdv_gop;
};

struct dm_display_list {
    struct bu_list	l;
    struct directory	*gdl_dp;
    struct bu_vls	gdl_path;
    struct bu_list	gdl_headSolid;		/**< @brief  head of solid list for this object */
    int			gdl_wflag;
};

#define DM_DISPLAY_LIST_NULL ((struct dm_display_list *)0)
#define DM_DRAWABLE_NULL ((struct dm_drawable *)0)
#define DM_VIEW_NULL ((struct dm_view *)0)

/* axes.c */
DM_EXPORT extern void dm_draw_data_axes(struct dm *dmp,
					fastf_t viewSize,
					struct dm_data_axes_state *gdasp);

DM_EXPORT extern void dm_draw_axes(struct dm *dmp,
				   fastf_t viewSize,
				   const mat_t rmat,
				   struct dm_axes_state *gasp);

/* adc.c */
DM_EXPORT extern void dm_draw_adc(struct dm *dmp,
				  struct dm_view *gvp);

/* clip.c */
DM_EXPORT extern int clip(fastf_t *,
			  fastf_t *,
			  fastf_t *,
			  fastf_t *);
DM_EXPORT extern int vclip(fastf_t *,
			   fastf_t *,
			   fastf_t *,
			   fastf_t *);

/* grid.c */
DM_EXPORT extern void dm_draw_grid(struct dm *dmp,
				   struct dm_grid_state *ggsp,
				   struct dm_view *gvp,
				   fastf_t base2local);

/* labels.c */
DM_EXPORT extern int dm_draw_labels(struct dm *dmp,
				    struct rt_wdb *wdbp,
				    const char *name,
				    mat_t viewmat,
				    int *labelsColor,
				    int (*labelsHook)(struct dm *dmp_arg, struct rt_wdb *wdbp_arg,
                                                      const char *name_arg, mat_t viewmat_arg,
                                                      int *labelsColor_arg, ClientData labelsHookClientdata_arg),
				    ClientData labelsHookClientdata);

/* rect.c */
DM_EXPORT extern void dm_draw_rect(struct dm *dmp,
				   struct dm_rect_state *grsp);

/* scale.c */
DM_EXPORT extern void dm_draw_scale(struct dm *dmp,
				    fastf_t viewSize,
				    int *lineColor,
				    int *textColor);

/* vers.c */
DM_EXPORT extern const char *dm_version(void);

/* defined in adc.c */
DM_EXPORT extern void dm_calc_adc_pos(struct dm_view *gvp);
DM_EXPORT extern void dm_calc_adc_a1(struct dm_view *gvp);
DM_EXPORT extern void dm_calc_adc_a2(struct dm_view *gvp);
DM_EXPORT extern void dm_calc_adc_dst(struct dm_view *gvp);
DM_EXPORT extern void adc_view_to_adc_grid(struct dm_view *gvp);
DM_EXPORT extern void adc_grid_to_adc_view(struct dm_view *gvp);
DM_EXPORT extern void adc_model_to_adc_view(struct dm_view *gvp);

/* dm-generic.c */
DM_EXPORT extern void dm_view_update(struct dm_view *gvp);


__END_DECLS

#endif /* DM_H */

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
