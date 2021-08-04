/*                        C L I P . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bg_clip
 * @brief
 * Clipping functions
 */
/** @{ */
/** @file clip.h */

#ifndef BG_CLIP_H
#define BG_CLIP_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * Clip a 2-D integer line seg against the size of the display
 */
BG_EXPORT extern int bg_lseg_clip(fastf_t *xp1, fastf_t *yp1, fastf_t *xp2, fastf_t *yp2, fastf_t clip_min, fastf_t clip_max);


/**
 * @brief
 * Clip a line segment against a rectangular parallelepiped (RPP).
 *
 * The RPP has faces parallel to the coordinate planes and is defined
 * by a minimum point and a maximum point.
 *
 * FIXME: the function name implies this takes a point,dir for a,b but
 * it actually takes a line segment going from points a to b!
 *
 * Returns -
 * 0 if ray does not hit RPP,
 * !0 if ray hits RPP.
 *
 * Implicit Return -
 * if !0 was returned, "a" and "b" have been clipped to the RPP.
 */
BG_EXPORT extern int bg_ray_vclip(point_t a, point_t b, fastf_t *min_pt, fastf_t *max_pt);

__END_DECLS

#endif  /* BG_CLIP_H */

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
