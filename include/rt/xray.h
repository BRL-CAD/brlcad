/*                          X R A Y . H
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
/** @addtogroup rt_xray
 * @brief All necessary information about a ray.
 */
/** @{ */
/** @file xray.h */

#ifndef RT_XRAY_H
#define RT_XRAY_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS
#define CORNER_PTS 4
#define RAY_NULL        ((struct xray *)0)
#define RT_CK_RAY(_p) BU_CKMAG(_p, RT_RAY_MAGIC, "struct xray");

/**
 * This plural xrays structure is a bu_list based container designed
 * to hold a list or bundle of xray(s). This bundle is utilized by
 * rt_shootrays() through its application bundle input.
 */
struct xrays
{
    struct bu_list l;
    struct xray ray;
};

/**
 * This structure is intended to describe the area and/or volume
 * represented by a ray.  In the case of the "rt" program it
 * represents the extent in model coordinates of the prism behind the
 * pixel being rendered.
 *
 * The r_pt values of the rays indicate the dimensions and location in
 * model space of the ray origin (usually the pixel to be rendered).
 * The r_dir vectors indicate the edges (and thus the shape) of the
 * prism which is formed from the projection of the pixel into space.
 */
struct pixel_ext {
    uint32_t magic;
    struct xray corner[CORNER_PTS];
};
/* This should have had an RT_ prefix */
#define BU_CK_PIXEL_EXT(_p) BU_CKMAG(_p, PIXEL_EXT_MAGIC, "struct pixel_ext")


__END_DECLS

#endif /* RT_XRAY_H */
/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
