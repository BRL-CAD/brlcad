/*                          D M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
#include "bu/vls.h"
#include "bn.h"
#include "bv.h"
#include "icv.h"

#include "./dm/defines.h"
#include "./dm/fbserv.h"
#include "./dm/util.h"
#include "./dm/view.h"

__BEGIN_DECLS

#define DM_NULL (struct dm *)NULL

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

DM_EXPORT extern struct dm dm_null;

DM_EXPORT extern void *dm_interp(struct dm *dmp);
DM_EXPORT extern void *dm_get_ctx(struct dm *dmp);
DM_EXPORT extern void *dm_get_udata(struct dm *dmp);
DM_EXPORT extern void dm_set_udata(struct dm *dmp, void *udata);
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
DM_EXPORT extern int dm_processOptions(struct dm *dmp, struct bu_vls *init_proc_vls, int argc, const char **argv);
DM_EXPORT extern int dm_limit(int i);
DM_EXPORT extern int dm_unlimit(int i);
DM_EXPORT extern fastf_t dm_wrap(fastf_t f);

/* adc.c */
DM_EXPORT extern void dm_draw_adc(struct dm *dmp,
				  struct bv_adc_state *adcp, mat_t view2model, mat_t model2view);

/* axes.c */
DM_EXPORT extern void dm_draw_data_axes(struct dm *dmp,
					fastf_t viewSize,
					struct bv_data_axes_state *bndasp);

DM_EXPORT extern void dm_draw_scene_axes(struct dm *dmp, struct bv_scene_obj *s);


DM_EXPORT extern void dm_draw_hud_axes(struct dm *dmp,
				   fastf_t viewSize,
				   const mat_t rmat,
				   struct bv_axes *bnasp);

/* clip.c */
DM_EXPORT extern int clip(fastf_t *,
			  fastf_t *,
			  fastf_t *,
			  fastf_t *);
DM_EXPORT extern int vclip(point_t,
			   point_t,
			   fastf_t *,
			   fastf_t *);

/* grid.c */
DM_EXPORT extern void dm_draw_grid(struct dm *dmp,
				   struct bv_grid_state *ggsp,
				   fastf_t scale,
				   mat_t model2view,
				   fastf_t base2local);

/* labels.c */
#ifdef DM_WITH_RT
#include "raytrace.h"
DM_EXPORT extern int dm_draw_prim_labels(struct dm *dmp,
				    struct rt_wdb *wdbp,
				    const char *name,
				    mat_t viewmat,
				    int *labelsColor,
				    int (*labelsHook)(struct dm *dmp_arg, struct rt_wdb *wdbp_arg,
						      const char *name_arg, mat_t viewmat_arg,
						      int *labelsColor_arg, void *labelsHookClientdata_arg),
				    void *labelsHookClientdata);
#endif

/* rect.c */
DM_EXPORT extern void dm_draw_rect(struct dm *dmp,
				   struct bv_interactive_rect_state *grsp);

/* scale.c */
DM_EXPORT extern void dm_draw_scale(struct dm *dmp,
				    fastf_t viewSize,
				    const char *unit,
				    int *lineColor,
				    int *textColor);

/* vers.c */
DM_EXPORT extern const char *dm_version(void);


/* Plugin related functions */
DM_EXPORT extern int dm_valid_type(const char *name, const char *dpy_string);
DM_EXPORT const char * dm_init_msgs();
DM_EXPORT extern struct dm *dm_open(void *ctx,
	                     void *interp,
			     const char *type,
			     int argc,
			     const char *argv[]);
DM_EXPORT extern void dm_list_types(struct bu_vls *list, const char *separator);
DM_EXPORT const char *dm_bestXType(const char *dpy_string);
DM_EXPORT extern int dm_have_graphics();

/* This reports the graphics system associated with the specified dm type (returned values
 * right now would be NULL, Tk or Qt - another eventual possibility is GLFW...) */
DM_EXPORT extern const char *dm_graphics_system(const char *dmtype);

/* functions to make a dm struct hideable - will need to
 * sort these out later */

DM_EXPORT extern struct dm *dm_get();
DM_EXPORT extern void dm_put(struct dm *dmp);
DM_EXPORT extern void dm_set_null(struct dm *dmp); /* TODO - HACK, need general set mechanism */
DM_EXPORT extern const char *dm_get_dm_name(const struct dm *dmp);
DM_EXPORT extern const char *dm_get_dm_lname(struct dm *dmp);
DM_EXPORT extern const char *dm_get_graphics_system(const struct dm *dmp);
DM_EXPORT extern int dm_get_width(struct dm *dmp);
DM_EXPORT extern int dm_get_height(struct dm *dmp);
DM_EXPORT extern void dm_set_width(struct dm *dmp, int width);
DM_EXPORT extern void dm_set_height(struct dm *dmp, int height);
DM_EXPORT extern void dm_geometry_request(struct dm *dmp, int width, int height);
DM_EXPORT extern void dm_internal_var(struct bu_vls *result, struct dm *dmp, const char *key); // ick
DM_EXPORT extern fastf_t dm_get_aspect(struct dm *dmp);
DM_EXPORT extern int dm_graphical(const struct dm *dmp);
DM_EXPORT extern const char *dm_get_type(struct dm *dmp);
DM_EXPORT extern unsigned long dm_get_id(struct dm *dmp);
DM_EXPORT extern void dm_set_id(struct dm *dmp, unsigned long new_id);
DM_EXPORT extern int dm_get_displaylist(struct dm *dmp);
DM_EXPORT extern int dm_close(struct dm *dmp);
DM_EXPORT extern unsigned char *dm_get_bg(struct dm *dmp);
DM_EXPORT extern int dm_set_bg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
DM_EXPORT extern unsigned char *dm_get_fg(struct dm *dmp);
DM_EXPORT extern int dm_set_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
DM_EXPORT extern int dm_reshape(struct dm *dmp, int width, int height);
DM_EXPORT extern int dm_make_current(struct dm *dmp);
DM_EXPORT extern int dm_doevent(struct dm *dmp, void *clientData, void *eventPtr);
DM_EXPORT extern int dm_get_dirty(struct dm *dmp);
DM_EXPORT extern void dm_set_dirty(struct dm *dmp, int i);
DM_EXPORT extern vect_t *dm_get_clipmin(struct dm *dmp);
DM_EXPORT extern vect_t *dm_get_clipmax(struct dm *dmp);
DM_EXPORT extern int dm_get_bound_flag(struct dm *dmp);
DM_EXPORT extern void dm_set_bound(struct dm *dmp, fastf_t val);
DM_EXPORT extern int dm_get_stereo(struct dm *dmp);
DM_EXPORT extern int dm_set_win_bounds(struct dm *dmp, fastf_t *w);
DM_EXPORT extern int dm_configure_win(struct dm *dmp, int force);
DM_EXPORT extern struct bu_vls *dm_get_pathname(struct dm *dmp);
DM_EXPORT extern void dm_set_pathname(struct dm *dmp, const char *pname);
DM_EXPORT extern struct bu_vls *dm_get_dname(struct dm *dmp);
DM_EXPORT extern const char *dm_get_name(const struct dm *dmp);
DM_EXPORT extern struct bu_vls *dm_get_tkname(struct dm *dmp);
DM_EXPORT extern int dm_get_fontsize(struct dm *dmp);
DM_EXPORT extern void dm_set_fontsize(struct dm *dmp, int size);
DM_EXPORT extern int dm_get_light(struct dm *dmp);
DM_EXPORT extern int dm_set_light(struct dm *dmp, int light);
DM_EXPORT extern int dm_get_transparency(struct dm *dmp);
DM_EXPORT extern int dm_set_transparency(struct dm *dmp, int transparency);
DM_EXPORT extern int dm_get_zbuffer(struct dm *dmp);
DM_EXPORT extern int dm_set_zbuffer(struct dm *dmp, int zbuffer);
DM_EXPORT extern int dm_get_linewidth(struct dm *dmp);
DM_EXPORT extern void dm_set_linewidth(struct dm *dmp, int linewidth);
DM_EXPORT extern int dm_get_linestyle(struct dm *dmp);
DM_EXPORT extern void dm_set_linestyle(struct dm *dmp, int linestyle);
DM_EXPORT extern int dm_get_zclip(struct dm *dmp);
DM_EXPORT extern void dm_set_zclip(struct dm *dmp, int zclip);
DM_EXPORT extern int dm_get_perspective(struct dm *dmp);
DM_EXPORT extern void dm_set_perspective(struct dm *dmp, fastf_t perspective);
DM_EXPORT extern int dm_get_display_image(struct dm *dmp, unsigned char **image, int flip, int alpha);
DM_EXPORT extern int dm_gen_dlists(struct dm *dmp, size_t range);
DM_EXPORT extern int dm_begin_dlist(struct dm *dmp, unsigned int list);
DM_EXPORT extern int dm_draw_dlist(struct dm *dmp, unsigned int list);
DM_EXPORT extern int dm_end_dlist(struct dm *dmp);
DM_EXPORT extern int dm_free_dlists(struct dm *dmp, unsigned int list, int range);
DM_EXPORT extern int dm_draw_vlist(struct dm *dmp, struct bv_vlist *vp);
DM_EXPORT extern int dm_draw_vlist_hidden_line(struct dm *dmp, struct bv_vlist *vp);
DM_EXPORT extern int dm_set_line_attr(struct dm *dmp, int width, int style);
DM_EXPORT extern int dm_draw_begin(struct dm *dmp);
DM_EXPORT extern int dm_draw_end(struct dm *dmp);
DM_EXPORT extern int dm_hud_begin(struct dm *dmp);
DM_EXPORT extern int dm_hud_end(struct dm *dmp);
DM_EXPORT extern int dm_loadmatrix(struct dm *dmp, fastf_t *mat, int eye);
DM_EXPORT extern int dm_loadpmatrix(struct dm *dmp, fastf_t *mat);
DM_EXPORT extern int dm_draw_string_2d(struct dm *dmp, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect);
DM_EXPORT extern int dm_string_bbox_2d(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect);
DM_EXPORT extern int dm_draw_line_2d(struct dm *dmp, fastf_t x1, fastf_t y1_2d, fastf_t x2, fastf_t y2);
DM_EXPORT extern int dm_draw_line_3d(struct dm *dmp, point_t pt1, point_t pt2);
DM_EXPORT extern int dm_draw_lines_3d(struct dm *dmp, int npoints, point_t *points, int sflag);
DM_EXPORT extern int dm_draw_point_2d(struct dm *dmp, fastf_t x, fastf_t y);
DM_EXPORT extern int dm_draw_point_3d(struct dm *dmp, point_t pt);
DM_EXPORT extern int dm_draw_points_3d(struct dm *dmp, int npoints, point_t *points);
DM_EXPORT extern int dm_draw(struct dm *dmp, struct bv_vlist *(*callback)(void *), void **data);
DM_EXPORT extern int dm_draw_obj(struct dm *dmp, struct display_list *obj);
DM_EXPORT extern int dm_set_depth_mask(struct dm *dmp, int d_on);
DM_EXPORT extern int dm_debug(struct dm *dmp, int lvl);
DM_EXPORT extern int dm_logfile(struct dm *dmp, const char *filename);
DM_EXPORT extern struct fb *dm_get_fb(struct dm *dmp);
DM_EXPORT extern int dm_get_fb_visible(struct dm *dmp);
DM_EXPORT extern int dm_set_fb_visible(struct dm *dmp, int is_fb_visible);

// TODO - this should be using libicv  - right now this just moving the guts of
// dmo_png_cmd behind the call table, so only provides PNG...
DM_EXPORT extern int dm_write_image(struct bu_vls *msgs, FILE *fp, struct dm *dmp);

DM_EXPORT extern void dm_flush(struct dm *dmp);
DM_EXPORT extern void dm_sync(struct dm *dmp);

// Return 1 if same, 0 if different and -1 if no dmp available
typedef enum {
    DM_MOTION_NOTIFY,
    DM_BUTTON_PRESS,
    DM_BUTTON_RELEASE
} dm_event_t;
DM_EXPORT extern int dm_event_cmp(struct dm *dmp, dm_event_t type, int event);

/* TODO - dm_vp is supposed to go away, but until we figure it out
 * expose it here to allow dm hiding */
DM_EXPORT extern void dm_set_vp(struct dm *dmp, fastf_t *vp);

DM_EXPORT extern int dm_set_hook(const struct bu_structparse_map *map,
				 const char *key, void *data, struct dm_hook_data *hook);

DM_EXPORT extern struct bu_structparse *dm_get_vparse(struct dm *dmp);
DM_EXPORT extern void *dm_get_mvars(struct dm *dmp);

DM_EXPORT extern int dm_draw_display_list(struct dm *dmp,
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

DM_EXPORT extern const char *dm_default_type();

/*
 * Needed for fd_set, avoid including sys/select.h outright since it
 * conflicts on some systems (e.g. freebsd4).
 *
 * FIXME: would be nice to decouple this interface from fd_set as it's
 * only used in one place right now.
 */
#if defined(HAVE_SYS_TYPES_H)
#  include <sys/types.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif
#include <stdio.h> /* for FILE */

#include "bsocket.h"

#include "pkg.h"
#include "bu/magic.h"
#include "bu/vls.h"

/**
 * Format of disk pixels is .pix raw image files.  Formerly used as
 * arguments to many of the library routines, but has fallen into
 * disuse due to the difficulties with ANSI function prototypes, and
 * the fact that arrays are not real types in C.  The most notable
 * problem is that of passing a pointer to an array of RGBpixel.  It
 * looks doubly dimensioned, but isn't.
 */
typedef unsigned char RGBpixel[3];


/**
 * These generic color maps have up to 16 bits of significance,
 * left-justified in a short.  Think of this as fixed-point values
 * from 0 to 1.
 */
typedef struct {
    unsigned short cm_red[256];
    unsigned short cm_green[256];
    unsigned short cm_blue[256];
} ColorMap;


#define PIXEL_NULL (unsigned char *) 0
#define RGBPIXEL_NULL (unsigned char *) 0
#define COLORMAP_NULL (ColorMap *) 0

#define FB_NULL (struct fb *) 0

/**
 * assert the integrity of a framebuffer struct.
 */
#define FB_CK_FB(_p) BU_CKMAG(_p, FB_MAGIC, "FB")

/* Library entry points */

DM_EXPORT struct fb *fb_get();
DM_EXPORT struct fb *fb_raw(const char *type);
DM_EXPORT void  fb_put(struct fb *ifp);
DM_EXPORT struct dm *fb_get_dm(struct fb *ifp);
DM_EXPORT extern const char *fb_gettype(struct fb *ifp);
DM_EXPORT extern void fb_set_standalone(struct fb *ifp, int val);
DM_EXPORT extern int fb_get_standalone(struct fb *ifp);
DM_EXPORT extern int fb_get_max_width(struct fb *ifp);
DM_EXPORT extern int fb_get_max_height(struct fb *ifp);
DM_EXPORT extern int fb_getwidth(struct fb *ifp);
DM_EXPORT extern int fb_getheight(struct fb *ifp);
DM_EXPORT extern int fb_poll(struct fb *ifp);
/* Returns in microseconds the maximum recommended amount of time to linger
 * before polling for updates for a specific framebuffer instance (can be
 * implementation dependent.)  Zero means the fb_poll process does nothing
 * (for example, the NULL fb). */
DM_EXPORT extern long fb_poll_rate(struct fb *ifp);
DM_EXPORT extern int fb_help(struct fb *ifp);
DM_EXPORT extern int fb_free(struct fb *ifp);
DM_EXPORT extern int fb_clear(struct fb *ifp, unsigned char *pp);
DM_EXPORT extern ssize_t fb_read(struct fb *ifp, int x, int y, unsigned char *pp, size_t count);
DM_EXPORT extern ssize_t fb_write(struct fb *ifp, int x, int y, const unsigned char *pp, size_t count);
DM_EXPORT extern int fb_rmap(struct fb *ifp, ColorMap *cmap);
DM_EXPORT extern int fb_wmap(struct fb *ifp, const ColorMap *cmap);
DM_EXPORT extern int fb_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
DM_EXPORT extern int fb_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
DM_EXPORT extern int fb_setcursor(struct fb *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo);
DM_EXPORT extern int fb_cursor(struct fb *ifp, int mode, int x, int y);
DM_EXPORT extern int fb_getcursor(struct fb *ifp, int *mode, int *x, int *y);
DM_EXPORT extern int fb_readrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
DM_EXPORT extern int fb_writerect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);
DM_EXPORT extern int fb_bwreadrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
DM_EXPORT extern int fb_bwwriterect(struct fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);

DM_EXPORT extern struct fb *fb_open(const char *file, int _width, int _height);
DM_EXPORT extern int fb_close(struct fb *ifp);
DM_EXPORT extern int fb_close_existing(struct fb *ifp);
DM_EXPORT extern int fb_genhelp(void);
DM_EXPORT extern int fb_ioinit(struct fb *ifp);
DM_EXPORT extern int fb_seek(struct fb *ifp, int x, int y);
DM_EXPORT extern int fb_tell(struct fb *ifp, int *xp, int *yp);
DM_EXPORT extern int fb_rpixel(struct fb *ifp, unsigned char *pp);
DM_EXPORT extern int fb_wpixel(struct fb *ifp, unsigned char *pp);
DM_EXPORT extern int fb_flush(struct fb *ifp);
DM_EXPORT extern int fb_configure_window(struct fb *, int, int);
DM_EXPORT extern int fb_refresh(struct fb *ifp, int x, int y, int w, int h);
#if !defined(_WIN32) || defined(__CYGWIN__)
DM_EXPORT extern void fb_log(const char *fmt, ...) _BU_ATTR_PRINTF12;
#endif
DM_EXPORT extern int fb_null(struct fb *ifp);
DM_EXPORT extern int fb_null_setcursor(struct fb *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig);

/* utility functions */
DM_EXPORT extern int fb_common_file_size(size_t *widthp, size_t *heightp, const char *filename, int pixel_size);
DM_EXPORT extern int fb_common_image_size(size_t *widthp, size_t *heightp, size_t npixels);
DM_EXPORT extern int fb_common_name_size(size_t *widthp, size_t *heightp, const char *name);
DM_EXPORT extern int fb_write_fp(struct fb *ifp, FILE *fp, int req_width, int req_height, int crunch, int inverse, struct bu_vls *result);
DM_EXPORT extern int fb_read_fd(struct fb *ifp, int fd,  int file_width, int file_height, int file_xoff, int file_yoff, int scr_width, int scr_height, int scr_xoff, int scr_yoff, int fileinput, char *file_name, int one_line_only, int multiple_lines, int autosize, int inverse, int clear, int zoom, struct bu_vls *result);
DM_EXPORT extern int fb_read_icv(struct fb *ifp, icv_image_t *img, int file_xoff, int file_yoff, int file_maxwidth, int file_maxheight, int scr_xoff, int scr_yoff, int clear, int zoom, int inverse, int one_line_only, int multiple_lines, struct bu_vls *result);
DM_EXPORT extern icv_image_t *fb_write_icv(struct fb *ifp, int scr_xoff, int scr_yoff, int width, int height);
DM_EXPORT extern int fb_read_png(struct fb *ifp, FILE *fp_in, int file_xoff, int file_yoff, int scr_xoff, int scr_yoff, int clear, int zoom, int inverse, int one_line_only, int multiple_lines, int verbose, int header_only, double def_screen_gamma, struct bu_vls *result);

DM_EXPORT extern int fb_set_interface(struct fb *ifp, const char *interface_type);
DM_EXPORT extern const char *fb_get_name(const struct fb *ifp);
DM_EXPORT extern void fb_set_magic(struct fb *ifp, uint32_t magic);
DM_EXPORT extern long fb_get_pagebuffer_pixel_size(struct fb *ifp);

DM_EXPORT extern int fb_is_set_fd(struct fb *ifp, fd_set *infds);
DM_EXPORT extern int fb_set_fd(struct fb *ifp, fd_set *select_list);
DM_EXPORT extern int fb_clear_fd(struct fb *ifp, fd_set *select_list);

/* color mapping */
DM_EXPORT extern int fb_is_linear_cmap(const ColorMap *cmap);
DM_EXPORT extern void fb_make_linear_cmap(ColorMap *cmap);

/* open_existing functionality. */
struct fb_platform_specific {uint32_t magic; void *data;};
DM_EXPORT extern struct fb_platform_specific *fb_get_platform_specific(uint32_t magic);
DM_EXPORT extern void fb_put_platform_specific(struct fb_platform_specific *fb_p);
DM_EXPORT extern struct fb *fb_open_existing(const char *file, int _width, int _height, struct fb_platform_specific *fb_p);
DM_EXPORT extern void fb_setup_existing(struct fb *fbp, int _width, int _height, struct fb_platform_specific *fb_p);

/* backward compatibility hacks */
DM_EXPORT extern int fb_reset(struct fb *ifp);
DM_EXPORT extern int fb_viewport(struct fb *ifp, int left, int top, int right, int bottom);
DM_EXPORT extern int fb_window(struct fb *ifp, int xcenter, int ycenter);
DM_EXPORT extern int fb_zoom(struct fb *ifp, int xzoom, int yzoom);
DM_EXPORT extern int fb_scursor(struct fb *ifp, int mode, int x, int y);

/*
 * Copy one RGB pixel to another.
 */
#define COPYRGB(to, from) { (to)[RED]=(from)[RED];\
	(to)[GRN]=(from)[GRN];\
	(to)[BLU]=(from)[BLU]; }

/* Debug Bitvector Definition */
#define FB_DEBUG_BIO 1	/* Buffered io calls (less r/wpixel) */
#define FB_DEBUG_CMAP 2	/* Contents of colormaps */
#define FB_DEBUG_RW 4	/* Contents of reads and writes */
#define FB_DEBUG_BRW 8	/* Buffered IO rpixel and wpixel */

/**
 * report version information about LIBFB
 */
DM_EXPORT extern const char *fb_version(void);


/* To avoid breaking things too badly, temporarily expose
 * what is now internal API */
#ifdef EXPOSE_FB_HEADER
#  include "../src/libfb/fb_private.h"
#endif

typedef struct fb_internal FBIO;


/*
 * Types of packages used for the remote frame buffer
 * communication
 */
#define MSG_FBOPEN        1
#define MSG_FBCLOSE       2
#define MSG_FBCLEAR       3
#define MSG_FBREAD        4
#define MSG_FBWRITE       5
#define MSG_FBCURSOR      6             /**< @brief fb_cursor() */
#define MSG_FBWINDOW      7             /**< @brief OLD */
#define MSG_FBZOOM        8             /**< @brief OLD */
#define MSG_FBSCURSOR     9             /**< @brief OLD */
#define MSG_FBVIEW        10            /**< @brief NEW */
#define MSG_FBGETVIEW     11            /**< @brief NEW */
#define MSG_FBRMAP        12
#define MSG_FBWMAP        13
#define MSG_FBHELP        14
#define MSG_FBREADRECT    15
#define MSG_FBWRITERECT   16
#define MSG_FBFLUSH       17
#define MSG_FBFREE        18
#define MSG_FBGETCURSOR   19            /**< @brief NEW */
#define MSG_FBPOLL        30            /**< @brief NEW */
#define MSG_FBSETCURSOR   31            /**< @brief NEW in Release 4.4 */
#define MSG_FBBWREADRECT  32            /**< @brief NEW in Release 4.6 */
#define MSG_FBBWWRITERECT 33            /**< @brief NEW in Release 4.6 */

#define MSG_DATA          20
#define MSG_RETURN        21
#define MSG_CLOSE         22
#define MSG_ERROR         23

#define MSG_NORETURN     100

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
