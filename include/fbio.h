/*                         F B I O . H
 * BRL-CAD
 *
 * Copyright (c) 2005-2014 United States Government as represented by
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
/** @file fbio.h
 *
 * BRL-CAD Framebuffer Library I/O Interfaces
 *
 */

#ifndef FBIO_H
#define FBIO_H

#include "common.h"

#include "bu.h"


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

/**
 * R G B p i x e l
 *
 * Format of disk pixels is .pix raw image files.  Formerly used as
 * arguments to many of the library routines, but has fallen into
 * disuse due to the difficulties with ANSI function prototypes, and
 * the fact that arrays are not real types in C.  The most notable
 * problem is that of passing a pointer to an array of RGBpixel.  It
 * looks doubly dimensioned, but isn't.
 */
typedef unsigned char RGBpixel[3];


/**
 * C o l o r M a p
 *
 * These generic color maps have up to 16 bits of significance,
 * left-justified in a short.  Think of this as fixed-point values
 * from 0 to 1.
 */
/* FIXME: ColorMap is same as RLEColorMap defined in 'orle.h' */
typedef struct {
    unsigned short cm_red[256];
    unsigned short cm_green[256];
    unsigned short cm_blue[256];
} ColorMap;


#define PIXEL_NULL (unsigned char *) 0
#define RGBPIXEL_NULL (unsigned char *) 0
#define COLORMAP_NULL (ColorMap *) 0
#define FBIO_NULL (FBIO *) 0


/**
 * F B I O
 *@brief
 * A frame-buffer IO structure.
 *
 * One of these is allocated for each active framebuffer.  A pointer
 * to this structure is the first argument to all the library
 * routines.
 */
typedef struct FBIO_ {
    uint32_t if_magic;
    /* Static information: per device TYPE.	*/
    int (*if_open)(struct FBIO_ *ifp, const char *file, int _width, int _height);			/**< @brief open device */
    int (*if_close)(struct FBIO_ *ifp);									/**< @brief close device */
    int (*if_clear)(struct FBIO_ *ifp, unsigned char *pp);						/**< @brief clear device */
    ssize_t (*if_read)(struct FBIO_ *ifp, int x, int y, unsigned char *pp, size_t count);		/**< @brief read pixels */
    ssize_t (*if_write)(struct FBIO_ *ifp, int x, int y, const unsigned char *pp, size_t count);	/**< @brief write pixels */
    int (*if_rmap)(struct FBIO_ *ifp, ColorMap *cmap);							/**< @brief read colormap */
    int (*if_wmap)(struct FBIO_ *ifp, const ColorMap *cmap);						/**< @brief write colormap */
    int (*if_view)(struct FBIO_ *ifp, int xcent, int ycent, int xzoom, int yzoom);			/**< @brief set view */
    int (*if_getview)(struct FBIO_ *ifp, int *xcent, int *ycent, int *xzoom, int *yzoom);		/**< @brief get view */
    int (*if_setcursor)(struct FBIO_ *ifp, const unsigned char *bits, int xb, int yb, int xo, int yo);	/**< @brief define cursor */
    int (*if_cursor)(struct FBIO_ *ifp, int mode, int x, int y);					/**< @brief set cursor */
    int (*if_getcursor)(struct FBIO_ *ifp, int *mode, int *x, int *y);					/**< @brief get cursor */
    int (*if_readrect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);		/**< @brief read rectangle */
    int (*if_writerect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);	/**< @brief write rectangle */
    int (*if_bwreadrect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);		/**< @brief read monochrome rectangle */
    int (*if_bwwriterect)(struct FBIO_ *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);	/**< @brief write rectangle */
    int (*if_poll)(struct FBIO_ *ifp);		/**< @brief handle events */
    int (*if_flush)(struct FBIO_ *ifp);		/**< @brief flush output */
    int (*if_free)(struct FBIO_ *ifp);		/**< @brief free resources */
    int (*if_help)(struct FBIO_ *ifp);		/**< @brief print useful info */
    char *if_type;	/**< @brief what "open" calls it */
    int if_max_width;	/**< @brief max device width */
    int if_max_height;	/**< @brief max device height */
    /* Dynamic information: per device INSTANCE. */
    char *if_name;	/**< @brief what the user called it */
    int if_width;	/**< @brief current values */
    int if_height;
    int if_selfd;	/**< @brief select(fd) for input events if >= 0 */
    /* Internal information: needed by the library.	*/
    int if_fd;		/**< @brief internal file descriptor */
    int if_xzoom;	/**< @brief zoom factors */
    int if_yzoom;
    int if_xcenter;	/**< @brief pan position */
    int if_ycenter;
    int if_cursmode;	/**< @brief cursor on/off */
    int if_xcurs;	/**< @brief cursor position */
    int if_ycurs;
    unsigned char *if_pbase;/**< @brief Address of malloc()ed page buffer.	*/
    unsigned char *if_pcurp;/**< @brief Current pointer into page buffer.	*/
    unsigned char *if_pendp;/**< @brief End of page buffer.			*/
    int if_pno;		/**< @brief Current "page" in memory.		*/
    int if_pdirty;	/**< @brief Page modified flag.			*/
    long if_pixcur;	/**< @brief Current pixel number in framebuffer. */
    long if_ppixels;	/**< @brief Sizeof page buffer (pixels).		*/
    int if_debug;	/**< @brief Buffered IO debug flag.		*/
    /* State variables for individual interface modules */
    union {
	char *p;
	size_t l;
    } u1, u2, u3, u4, u5, u6;
} FBIO;

/**
 * assert the integrity of an FBIO struct.
 */
#define FB_CK_FBIO(_p) BU_CKMAG(_p, FB_MAGIC, "FBIO")

/* declare all the possible interfaces */
#ifdef IF_REMOTE
FB_EXPORT extern FBIO remote_interface;	/* not in list[] */
#endif

#ifdef IF_OGL
FB_EXPORT extern FBIO ogl_interface;
#endif

#ifdef IF_WGL
FB_EXPORT extern FBIO wgl_interface;
#endif

#ifdef IF_X
FB_EXPORT extern FBIO X24_interface;
FB_EXPORT extern FBIO X_interface;
#endif

#ifdef IF_TK
FB_EXPORT extern FBIO tk_interface;
#endif


/* Always included */
FB_EXPORT extern FBIO debug_interface, disk_interface, stk_interface;
FB_EXPORT extern FBIO memory_interface, null_interface;

#endif  /* FBIO_H */
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
