/*                            F B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @addtogroup libfb */
/** @{ */
/** @file fb.h
 *
 * "Generic" Framebuffer Library Interface Defines.
 *
 */

#ifndef FB_H
#define FB_H

#ifndef FB_EXPORT
#  if defined(FB_DLL_EXPORTS) && defined(FB_DLL_IMPORTS)
#    error "Only FB_DLL_EXPORTS or FB_DLL_IMPORTS can be defined, not both."
#  elif defined(FB_DLL_EXPORTS)
#    define FB_EXPORT __declspec(dllexport)
#  elif defined(FB_DLL_IMPORTS)
#    define FB_EXPORT __declspec(dllimport)
#  else
#    define FB_EXPORT
#  endif
#endif

/*
 * Needed for fd_set, avoid including sys/select.h outright since it
 * conflicts on some systems (e.g. freebsd4).
 *
 * FIXME: would be nice to decouple this interface from fd_set as it's
 * only used in one place right now.
 */
#include "common.h"
#if defined(HAVE_SYS_TYPES_H)
#  include <sys/types.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif

#include "bsocket.h"
#include "bio.h"

#include "tcl.h"
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

/* Use a typedef to hide the details of the framebuffer structure */
typedef struct fb_internal fb;
#define FB_NULL (fb *) 0

/**
 * assert the integrity of a framebuffer struct.
 */
#define FB_CK_FB(_p) BU_CKMAG(_p, FB_MAGIC, "FB")

__BEGIN_DECLS

/* Library entry points */

FB_EXPORT fb *fb_get();
FB_EXPORT void  fb_put(fb *ifp);
FB_EXPORT extern char *fb_gettype(fb *ifp);
FB_EXPORT extern int fb_get_max_width(fb *ifp);
FB_EXPORT extern int fb_get_max_height(fb *ifp);
FB_EXPORT extern int fb_getwidth(fb *ifp);
FB_EXPORT extern int fb_getheight(fb *ifp);
FB_EXPORT extern int fb_poll(fb *ifp);
/* Returns in microseconds the maximum recommended amount of time to linger
 * before polling for updates for a specific framebuffer instance (can be
 * implementation dependent.)  Zero means the fb_poll process does nothing
 * (for example, the NULL fb). */
FB_EXPORT extern long fb_poll_rate(fb *ifp);
FB_EXPORT extern int fb_help(fb *ifp);
FB_EXPORT extern int fb_free(fb *ifp);
FB_EXPORT extern int fb_clear(fb *ifp, unsigned char *pp);
FB_EXPORT extern ssize_t fb_read(fb *ifp, int x, int y, unsigned char *pp, size_t count);
FB_EXPORT extern ssize_t fb_write(fb *ifp, int x, int y, const unsigned char *pp, size_t count);
FB_EXPORT extern int fb_rmap(fb *ifp, ColorMap *cmap);
FB_EXPORT extern int fb_wmap(fb *ifp, const ColorMap *cmap);
FB_EXPORT extern int fb_view(fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
FB_EXPORT extern int fb_getview(fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
FB_EXPORT extern int fb_setcursor(fb *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo);
FB_EXPORT extern int fb_cursor(fb *ifp, int mode, int x, int y);
FB_EXPORT extern int fb_getcursor(fb *ifp, int *mode, int *x, int *y);
FB_EXPORT extern int fb_readrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
FB_EXPORT extern int fb_writerect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);
FB_EXPORT extern int fb_bwreadrect(fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);
FB_EXPORT extern int fb_bwwriterect(fb *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp);

FB_EXPORT extern fb *fb_open(const char *file, int _width, int _height);
FB_EXPORT extern int fb_close(fb *ifp);
FB_EXPORT extern int fb_close_existing(fb *ifp);
FB_EXPORT extern int fb_genhelp(void);
FB_EXPORT extern int fb_ioinit(fb *ifp);
FB_EXPORT extern int fb_seek(fb *ifp, int x, int y);
FB_EXPORT extern int fb_tell(fb *ifp, int *xp, int *yp);
FB_EXPORT extern int fb_rpixel(fb *ifp, unsigned char *pp);
FB_EXPORT extern int fb_wpixel(fb *ifp, unsigned char *pp);
FB_EXPORT extern int fb_flush(fb *ifp);
FB_EXPORT extern int fb_configure_window(fb *, int, int);
FB_EXPORT extern int fb_refresh(fb *ifp, int x, int y, int w, int h);
#if !defined(_WIN32) || defined(__CYGWIN__)
FB_EXPORT extern void fb_log(const char *fmt, ...) _BU_ATTR_PRINTF12;
#endif
FB_EXPORT extern int fb_null(fb *ifp);
FB_EXPORT extern int fb_null_setcursor(fb *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig);

/* utility functions */
FB_EXPORT extern int fb_common_file_size(size_t *widthp, size_t *heightp, const char *filename, int pixel_size);
FB_EXPORT extern int fb_common_image_size(size_t *widthp, size_t *heightp, size_t npixels);
FB_EXPORT extern int fb_common_name_size(size_t *widthp, size_t *heightp, const char *name);
FB_EXPORT extern int fb_write_fp(fb *ifp, FILE *fp, int req_width, int req_height, int crunch, int inverse, struct bu_vls *result);
FB_EXPORT extern int fb_read_fd(fb *ifp, int fd,  int file_width, int file_height, int file_xoff, int file_yoff, int scr_width, int scr_height, int scr_xoff, int scr_yoff, int fileinput, char *file_name, int one_line_only, int multiple_lines, int autosize, int inverse, int clear, int zoom, struct bu_vls *result);

FB_EXPORT extern void fb_set_interface(fb *ifp, const char *interface_type);
FB_EXPORT extern void fb_set_name(fb *ifp, const char *name);
FB_EXPORT extern char *fb_get_name(fb *ifp);
FB_EXPORT extern void fb_set_magic(fb *ifp, uint32_t magic);
FB_EXPORT extern long fb_get_pagebuffer_pixel_size(fb *ifp);

FB_EXPORT extern int fb_is_set_fd(fb *ifp, fd_set *infds);
FB_EXPORT extern int fb_set_fd(fb *ifp, fd_set *select_list);
FB_EXPORT extern int fb_clear_fd(fb *ifp, fd_set *select_list);

/* color mapping */
FB_EXPORT extern int fb_is_linear_cmap(const ColorMap *cmap);
FB_EXPORT extern void fb_make_linear_cmap(ColorMap *cmap);

/* open_existing functionality. */
struct fb_platform_specific {uint32_t magic; void *data;};
FB_EXPORT extern struct fb_platform_specific *fb_get_platform_specific(uint32_t magic);
FB_EXPORT extern void fb_put_platform_specific(struct fb_platform_specific *fb_p);
FB_EXPORT extern fb *fb_open_existing(const char *file, int _width, int _height, struct fb_platform_specific *fb_p);

/* backward compatibility hacks */
FB_EXPORT extern int fb_reset(fb *ifp);
FB_EXPORT extern int fb_viewport(fb *ifp, int left, int top, int right, int bottom);
FB_EXPORT extern int fb_window(fb *ifp, int xcenter, int ycenter);
FB_EXPORT extern int fb_zoom(fb *ifp, int xzoom, int yzoom);
FB_EXPORT extern int fb_scursor(fb *ifp, int mode, int x, int y);

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

/* tcl.c */
/* The presence of Tcl_Interp as an arg prevents giving arg list */
FB_EXPORT extern void fb_tcl_setup(void);
FB_EXPORT extern int Fb_Init(Tcl_Interp *interp);

/**
 * report version information about LIBFB
 */
FB_EXPORT extern const char *fb_version(void);


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
#define MSG_FBOPEN      1
#define MSG_FBCLOSE     2
#define MSG_FBCLEAR     3
#define MSG_FBREAD      4
#define MSG_FBWRITE     5
#define MSG_FBCURSOR    6               /**< @brief fb_cursor() */
#define MSG_FBWINDOW    7               /**< @brief OLD */
#define MSG_FBZOOM      8               /**< @brief OLD */
#define MSG_FBSCURSOR   9               /**< @brief OLD */
#define MSG_FBVIEW      10              /**< @brief NEW */
#define MSG_FBGETVIEW   11              /**< @brief NEW */
#define MSG_FBRMAP      12
#define MSG_FBWMAP      13
#define MSG_FBHELP      14
#define MSG_FBREADRECT  15
#define MSG_FBWRITERECT 16
#define MSG_FBFLUSH     17
#define MSG_FBFREE      18
#define MSG_FBGETCURSOR 19              /**< @brief NEW */
#define MSG_FBPOLL      30              /**< @brief NEW */
#define MSG_FBSETCURSOR 31              /**< @brief NEW in Release 4.4 */
#define MSG_FBBWREADRECT 32             /**< @brief NEW in Release 4.6 */
#define MSG_FBBWWRITERECT 33            /**< @brief NEW in Release 4.6 */

#define MSG_DATA        20
#define MSG_RETURN      21
#define MSG_CLOSE       22
#define MSG_ERROR       23

#define MSG_NORETURN    100


/* Framebuffer server object */

#define NET_LONG_LEN 4 /**< @brief # bytes to network long */
#define MAX_CLIENTS 32
#define MAX_PORT_TRIES 100
#define FBS_CALLBACK_NULL (void (*)())NULL
#define FBSERV_OBJ_NULL (struct fbserv_obj *)NULL

struct fbserv_listener {
    int fbsl_fd;                        /**< @brief socket to listen for connections */
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_Channel fbsl_chan;
#endif
    int fbsl_port;                      /**< @brief port number to listen on */
    int fbsl_listen;                    /**< @brief !0 means listen for connections */
    struct fbserv_obj *fbsl_fbsp;       /**< @brief points to its fbserv object */
};


struct fbserv_client {
    int fbsc_fd;
#if defined(_WIN32) && !defined(__CYGWIN__)
    Tcl_Channel fbsc_chan;
    Tcl_FileProc *fbsc_handler;
#endif
    struct pkg_conn *fbsc_pkg;
    struct fbserv_obj *fbsc_fbsp;       /**< @brief points to its fbserv object */
};


struct fbserv_obj {
    fb *fbs_fbp;                        /**< @brief framebuffer pointer */
    Tcl_Interp *fbs_interp;             /**< @brief tcl interpreter */
    struct fbserv_listener fbs_listener;                /**< @brief data for listening */
    struct fbserv_client fbs_clients[MAX_CLIENTS];      /**< @brief connected clients */
    void (*fbs_callback)(void *clientData);             /**< @brief callback function */
    void *fbs_clientData;
    int fbs_mode;                       /**< @brief 0-off, 1-underlay, 2-interlay, 3-overlay */
};

FB_EXPORT extern int fbs_open(struct fbserv_obj *fbsp, int port);
FB_EXPORT extern int fbs_close(struct fbserv_obj *fbsp);



__END_DECLS

#endif /* FB_H */

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
