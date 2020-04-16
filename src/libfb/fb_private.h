/*                   F B _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file fb_private.h
 *
 * Private header for libfb.
 *
 */

#ifndef LIBFB_FB_PRIVATE_H
#define LIBFB_FB_PRIVATE_H

#include "common.h"

#include <limits.h> /* For INT_MAX */
#include <stdlib.h>

#include "fb.h"
#include "fb/calltable.h"

/* declare all the possible interfaces */
#ifdef IF_X
FB_EXPORT extern struct fb X24_interface;
#endif
#ifdef IF_OGL
FB_EXPORT extern struct fb ogl_interface;
#endif
#ifdef IF_OSGL
FB_EXPORT extern struct fb osgl_interface;
#endif
#ifdef IF_WGL
FB_EXPORT extern struct fb wgl_interface;
#endif
#ifdef IF_TK
FB_EXPORT extern struct fb tk_interface;
#endif
#ifdef IF_QT
FB_EXPORT extern struct fb qt_interface;
#endif
#ifdef IF_REMOTE
FB_EXPORT extern struct fb remote_interface; /* not in list[] */
#endif

/* Always included */
FB_EXPORT extern struct fb debug_interface, disk_interface, stk_interface;
FB_EXPORT extern struct fb memory_interface, fb_null_interface;


/* Shared memory (shmget et. al.) key common to multiple framebuffers */
#define SHMEM_KEY 42

/* Maximum memory buffer allocation.
 *
 * Care must be taken as this can result in a large default memory
 * allocation that can have an impact on performance or minimum system
 * requirements.  For example, 20*1024 results in a 20480x20480 buffer
 * and a 1.6GB allocation.  Using 32*1024 results in a 4GB allocation.
 */
#define FB_XMAXSCREEN 20*1024 /* 1.6GB */
#define FB_YMAXSCREEN 20*1024 /* 1.6GB */

/* setting to 1 turns on general intrface debugging for all fb types */
#define FB_DEBUG 0


__BEGIN_DECLS

/*
 * Structure of color map in shared memory region.  Has exactly the
 * same format as the SGI hardware "gammaramp" map Note that only the
 * lower 8 bits are significant.
 */
struct fb_cmap {
    short cmr[256];
    short cmg[256];
    short cmb[256];
};


/*
 * This defines the format of the in-memory framebuffer copy.  The
 * alpha component and reverse order are maintained for compatibility
 * with /dev/sgi
 */
struct fb_pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
};


/* Clipping structure for zoom/pan operations */
struct fb_clip {
    int xpixmin;	/* view clipping planes clipped to pixel memory space*/
    int xpixmax;
    int ypixmin;
    int ypixmax;
    int xscrmin;	/* view clipping planes */
    int xscrmax;
    int yscrmin;
    int yscrmax;
    double oleft;	/* glOrtho parameters */
    double oright;
    double otop;
    double obottom;
};

/*
 * Some functions and variables we couldn't hide.
 * Not for general consumption.
 */
FB_EXPORT extern int _fb_disk_enable;
FB_EXPORT extern int fb_sim_readrect(struct fb *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_writerect(struct fb *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_bwreadrect(struct fb *ifp, int xmin, int ymin, int _width, int _height, unsigned char *pp);
FB_EXPORT extern int fb_sim_bwwriterect(struct fb *ifp, int xmin, int ymin, int _width, int _height, const unsigned char *pp);
FB_EXPORT extern int fb_sim_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom);
FB_EXPORT extern int fb_sim_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);
FB_EXPORT extern int fb_sim_cursor(struct fb *ifp, int mode, int x, int y);
FB_EXPORT extern int fb_sim_getcursor(struct fb *ifp, int *mode, int *x, int *y);

__END_DECLS

#endif /* LIBFB_FB_PRIVATE_H */

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
