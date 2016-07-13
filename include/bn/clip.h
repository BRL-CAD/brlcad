/*                        C L I P . H
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_clip
 * @brief
 * Clipping functions
 */
/** @{ */
/** @file clip.h */

#ifndef BN_CLIP_H
#define BN_CLIP_H

#include "common.h"
#include "vmath.h"
#include "bn/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * Clip a 2-D integer line seg against the size of the display
 */
BN_EXPORT extern int bn_lseg_clip(fastf_t *xp1, fastf_t *yp1, fastf_t *xp2, fastf_t *yp2, fastf_t clip_min, fastf_t clip_max);

/**
 * @brief
 * Clip a ray against a rectangular parallelepiped (RPP)
 * that has faces parallel to the coordinate planes (a clipping RPP).
 * The RPP is defined by a minimum point and a maximum point.
 *
 * Returns -
 * 0 if ray does not hit RPP,
 * !0 if ray hits RPP.
 *
 * Implicit Return -
 * if !0 was returned, "a" and "b" have been clipped to the RPP.
 */
BN_EXPORT extern int bn_ray_vclip(vect_t a, vect_t b, fastf_t *min, fastf_t *max);

__END_DECLS

#endif  /* BN_CLIP_H */

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
