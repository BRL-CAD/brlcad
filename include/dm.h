/*                          D M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2016 United States Government as represented by
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
#include "fb.h"

#include "./dm/defines.h"

/* Use fbserv */
#define USE_FBSERV 1

#define DM_NULL (dm *)NULL
#define DM_MIN (-2048)
#define DM_MAX (2047)

#define DM_O(_m) offsetof(dm, _m)

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
#define DM_TYPE_WGL	7
#define DM_TYPE_TK	8
#define DM_TYPE_RTGL	9
#define DM_TYPE_TXT	10
#define DM_TYPE_QT	11
#define DM_TYPE_OSG	12
#define DM_TYPE_OSGL	13

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
#define IS_DM_TYPE_TXT(_t) ((_t) == DM_TYPE_TXT)
#define IS_DM_TYPE_QT(_t) ((_t) == DM_TYPE_QT)
#define IS_DM_TYPE_OSG(_t) ((_t) == DM_TYPE_OSG)
#define IS_DM_TYPE_OSGL(_t) ((_t) == DM_TYPE_OSGL)

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

/* This is how a parent application can pass a generic
 * hook function in when setting dm variables.  The dm_hook_data
 * container holds the bu_structparse hook function and data
 * needed by it in the dm_hook and dm_hook_data slots.  When
 * bu_struct_parse or one of the other libbu structure parsing
 * functions is called, the dm_hook_data container is passed
 * in as the data slot in that call.
 *
 * TODO - need example
 *
 */
struct dm_hook_data {
    void(*dm_hook)(const struct bu_structparse *, const char *, void *, const char *, void *);
    void *dmh_data;
};

/* Hide the dm structure behind a typedef */
typedef struct dm_internal dm;

#define DM_OPEN(_interp, _type, _argc, _argv) dm_open(_interp, _type, _argc, _argv)

__BEGIN_DECLS

DM_EXPORT extern dm dm_ogl;
DM_EXPORT extern dm dm_plot;
DM_EXPORT extern dm dm_ps;
DM_EXPORT extern dm dm_rtgl;
DM_EXPORT extern dm dm_tk;
DM_EXPORT extern dm dm_wgl;
DM_EXPORT extern dm dm_X;
DM_EXPORT extern dm dm_txt;
DM_EXPORT extern dm dm_qt;
DM_EXPORT extern dm dm_osgl;

DM_EXPORT extern int Dm_Init(void *interp);
DM_EXPORT extern dm *dm_open(Tcl_Interp *interp,
				    int type,
				    int argc,
				    const char *argv[]);
DM_EXPORT extern void *dm_interp(dm *dmp);
DM_EXPORT extern int dm_share_dlist(dm *dmp1,
				    dm *dmp2);
DM_EXPORT extern fastf_t dm_Xx2Normal(dm *dmp,
				      int x);
DM_EXPORT extern int dm_Normal2Xx(dm *dmp,
				  fastf_t f);
DM_EXPORT extern fastf_t dm_Xy2Normal(dm *dmp,
				      int y,
				      int use_aspect);
DM_EXPORT extern int dm_Normal2Xy(dm *dmp,
				  fastf_t f,
				  int use_aspect);
DM_EXPORT extern void dm_fogHint(dm *dmp,
				 int fastfog);
DM_EXPORT extern int dm_processOptions(dm *dmp, struct bu_vls *init_proc_vls, int argc, char **argv);
DM_EXPORT extern int dm_limit(int i);
DM_EXPORT extern int dm_unlimit(int i);
DM_EXPORT extern fastf_t dm_wrap(fastf_t f);

/* adc.c */
DM_EXPORT extern void dm_draw_adc(dm *dmp,
				  struct bview_adc_state *adcp, mat_t view2model, mat_t model2view);

/* axes.c */
DM_EXPORT extern void dm_draw_data_axes(dm *dmp,
					fastf_t viewSize,
					struct bview_data_axes_state *bndasp);

DM_EXPORT extern void dm_draw_axes(dm *dmp,
				   fastf_t viewSize,
				   const mat_t rmat,
				   struct bview_axes_state *bnasp);

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
DM_EXPORT extern void dm_draw_grid(dm *dmp,
				   struct bview_grid_state *ggsp,
				   fastf_t scale,
				   mat_t model2view,
				   fastf_t base2local);

/* labels.c */
DM_EXPORT extern int dm_draw_labels(dm *dmp,
				    struct rt_wdb *wdbp,
				    const char *name,
				    mat_t viewmat,
				    int *labelsColor,
				    int (*labelsHook)(dm *dmp_arg, struct rt_wdb *wdbp_arg,
                                                      const char *name_arg, mat_t viewmat_arg,
                                                      int *labelsColor_arg, ClientData labelsHookClientdata_arg),
				    ClientData labelsHookClientdata);

/* rect.c */
DM_EXPORT extern void dm_draw_rect(dm *dmp,
				   struct bview_interactive_rect_state *grsp);

/* scale.c */
DM_EXPORT extern void dm_draw_scale(dm *dmp,
				    fastf_t viewSize,
				    int *lineColor,
				    int *textColor);

/* vers.c */
DM_EXPORT extern const char *dm_version(void);



/* functions to make a dm struct hideable - will need to
 * sort these out later */

DM_EXPORT extern dm *dm_get();
DM_EXPORT extern void dm_put(dm *dmp);
DM_EXPORT extern void dm_set_null(dm *dmp); /* TODO - HACK, need general set mechanism */
DM_EXPORT extern const char *dm_get_dm_name(dm *dmp);
DM_EXPORT extern const char *dm_get_dm_lname(dm *dmp);
DM_EXPORT extern int dm_get_width(dm *dmp);
DM_EXPORT extern int dm_get_height(dm *dmp);
DM_EXPORT extern void dm_set_width(dm *dmp, int width);
DM_EXPORT extern void dm_set_height(dm *dmp, int height);
DM_EXPORT extern fastf_t dm_get_aspect(dm *dmp);
DM_EXPORT extern int dm_get_type(dm *dmp);
DM_EXPORT void *dm_get_xvars(dm *dmp);
DM_EXPORT extern struct bu_vls *dm_list_types(const char separator); /* free return list with bu_vls_free(list); BU_PUT(list, struct bu_vls); */
DM_EXPORT extern unsigned long dm_get_id(dm *dmp);
DM_EXPORT extern void dm_set_id(dm *dmp, unsigned long new_id);
DM_EXPORT extern int dm_get_displaylist(dm *dmp);
DM_EXPORT extern int dm_close(dm *dmp);
DM_EXPORT extern unsigned char *dm_get_bg(dm *dmp);
DM_EXPORT extern int dm_set_bg(dm *dmp, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern unsigned char *dm_get_fg(dm *dmp);
DM_EXPORT extern int dm_set_fg(dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
DM_EXPORT extern int dm_make_current(dm *dmp);
DM_EXPORT extern vect_t *dm_get_clipmin(dm *dmp);
DM_EXPORT extern vect_t *dm_get_clipmax(dm *dmp);
DM_EXPORT extern int dm_get_bound_flag(dm *dmp);
DM_EXPORT extern void dm_set_bound(dm *dmp, fastf_t val);
DM_EXPORT extern int dm_get_stereo(dm *dmp);
DM_EXPORT extern int dm_set_win_bounds(dm *dmp, fastf_t *w);
DM_EXPORT extern int dm_configure_win(dm *dmp, int force);
DM_EXPORT extern struct bu_vls *dm_get_pathname(dm *dmp);
DM_EXPORT extern struct bu_vls *dm_get_dname(dm *dmp);
DM_EXPORT extern struct bu_vls *dm_get_tkname(dm *dmp);
DM_EXPORT extern int dm_get_fontsize(dm *dmp);
DM_EXPORT extern void dm_set_fontsize(dm *dmp, int size);
DM_EXPORT extern int dm_get_light_flag(dm *dmp);
DM_EXPORT extern void dm_set_light_flag(dm *dmp, int size);
DM_EXPORT extern int dm_set_light(dm *dmp, int light);
DM_EXPORT extern void *dm_get_public_vars(dm *dmp);
DM_EXPORT extern void *dm_get_private_vars(dm *dmp);
DM_EXPORT extern int dm_get_transparency(dm *dmp);
DM_EXPORT extern int dm_set_transparency(dm *dmp, int transparency);
DM_EXPORT extern int dm_get_zbuffer(dm *dmp);
DM_EXPORT extern int dm_set_zbuffer(dm *dmp, int zbuffer);
DM_EXPORT extern int dm_get_linewidth(dm *dmp);
DM_EXPORT extern void dm_set_linewidth(dm *dmp, int linewidth);
DM_EXPORT extern int dm_get_linestyle(dm *dmp);
DM_EXPORT extern void dm_set_linestyle(dm *dmp, int linestyle);
DM_EXPORT extern int dm_get_zclip(dm *dmp);
DM_EXPORT extern void dm_set_zclip(dm *dmp, int zclip);
DM_EXPORT extern int dm_get_perspective(dm *dmp);
DM_EXPORT extern void dm_set_perspective(dm *dmp, fastf_t perspective);
DM_EXPORT extern int dm_get_display_image(dm *dmp, unsigned char **image);
DM_EXPORT extern int dm_gen_dlists(dm *dmp, size_t range);
DM_EXPORT extern int dm_begin_dlist(dm *dmp, unsigned int list);
DM_EXPORT extern void dm_draw_dlist(dm *dmp, unsigned int list);
DM_EXPORT extern int dm_end_dlist(dm *dmp);
DM_EXPORT extern int dm_free_dlists(dm *dmp, unsigned int list, int range);
DM_EXPORT extern int dm_draw_vlist(dm *dmp, struct bn_vlist *vp);
DM_EXPORT extern int dm_draw_vlist_hidden_line(dm *dmp, struct bn_vlist *vp);
DM_EXPORT extern int dm_set_line_attr(dm *dmp, int width, int style);
DM_EXPORT extern int dm_draw_begin(dm *dmp);
DM_EXPORT extern int dm_draw_end(dm *dmp);
DM_EXPORT extern int dm_normal(dm *dmp);
DM_EXPORT extern int dm_loadmatrix(dm *dmp, fastf_t *mat, int eye);
DM_EXPORT extern int dm_loadpmatrix(dm *dmp, fastf_t *mat);
DM_EXPORT extern int dm_draw_string_2d(dm *dmp, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect);
DM_EXPORT extern int dm_draw_line_2d(dm *dmp, fastf_t x1, fastf_t y1_2d, fastf_t x2, fastf_t y2);
DM_EXPORT extern int dm_draw_line_3d(dm *dmp, point_t pt1, point_t pt2);
DM_EXPORT extern int dm_draw_lines_3d(dm *dmp, int npoints, point_t *points, int sflag);
DM_EXPORT extern int dm_draw_point_2d(dm *dmp, fastf_t x, fastf_t y);
DM_EXPORT extern int dm_draw_point_3d(dm *dmp, point_t pt);
DM_EXPORT extern int dm_draw_points_3d(dm *dmp, int npoints, point_t *points);
DM_EXPORT extern int dm_draw(dm *dmp, struct bn_vlist *(*callback)(void *), void **data);
DM_EXPORT extern int dm_draw_obj(dm *dmp, struct display_list *obj);
DM_EXPORT extern int dm_set_depth_mask(dm *dmp, int d_on);
DM_EXPORT extern int dm_debug(dm *dmp, int lvl);
DM_EXPORT extern int dm_logfile(dm *dmp, const char *filename);
DM_EXPORT extern fb *dm_get_fb(dm *dmp);
DM_EXPORT extern int dm_get_fb_visible(dm *dmp);
DM_EXPORT extern int dm_set_fb_visible(dm *dmp, int is_fb_visible);

/* TODO - dm_vp is supposed to go away, but until we figure it out
 * expose it here to allow dm hiding */
DM_EXPORT extern fastf_t *dm_get_vp(dm *dmp);
DM_EXPORT extern void dm_set_vp(dm *dmp, fastf_t *vp);

DM_EXPORT extern int dm_set_hook(const struct bu_structparse_map *map,
       	const char *key, void *data, struct dm_hook_data *hook);

DM_EXPORT extern struct bu_structparse *dm_get_vparse(dm *dmp);
DM_EXPORT extern void *dm_get_mvars(dm *dmp);

DM_EXPORT extern int dm_draw_display_list(dm *dmp,
	struct bu_list *dl,
	fastf_t transparency_threshold,
	fastf_t inv_viewsize,
	short r, short g, short b,
	int line_width,
	int draw_style,
	int draw_edit,
	unsigned char *gdc,
	int solids_down,
	int mv_dlist
	);

DM_EXPORT extern int dm_default_type();

/* For backwards compatibility, define macros and expose struct dm */
#ifdef EXPOSE_DM_HEADER
#  include "../src/libdm/dm_private.h"
#endif

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
