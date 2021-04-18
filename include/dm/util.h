/*                         U T I L . H
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
/** @file dm/util.h
 *
 */

#include "common.h"

#include "vmath.h"

#include "dm/defines.h"

#ifndef DM_UTIL_H
#define DM_UTIL_H

__BEGIN_DECLS

DM_EXPORT unsigned long long dm_hash(struct dm *dmp);

DM_EXPORT void dm_generic_hook(const struct bu_structparse *sdp, const char *name, void *base, const char *value, void *data);

DM_EXPORT int dm_validXType(const char *dpy_string, const char *name);

DM_EXPORT int draw_Line3D(struct dm *dmp, point_t pt1, point_t pt2);

DM_EXPORT void flip_display_image_vertically(unsigned char *image, size_t width, size_t height, int alpha);

DM_EXPORT extern int _fb_disk_enable;

DM_EXPORT int fb_sim_view(struct fb *ifp, int xcenter, int ycenter, int xzoom, int yzoom);

DM_EXPORT int fb_sim_getview(struct fb *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom);

DM_EXPORT int fb_sim_cursor(struct fb *ifp, int mode, int x, int y);

DM_EXPORT int fb_sim_getcursor(struct fb *ifp, int *mode, int *x, int *y);

DM_EXPORT int fb_sim_readrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);

DM_EXPORT int fb_sim_bwreadrect(struct fb *ifp, int xmin, int ymin, int width, int height, unsigned char *pp);

__END_DECLS

#endif /* DM_UTIL_H */

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
