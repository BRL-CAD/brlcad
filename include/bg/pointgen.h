/*                       P O I N T G E N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/* @file pointgen.h */
/** @addtogroup bg_pointgen */
/** @{ */

/**
 *  @brief Routines for the generation of pseudo-random and quasi-random
 *  points.
 */

#ifndef BG_POINTGEN_H
#define BG_POINTGEN_H

#include "common.h"
#include "vmath.h"
#include "bn/numgen.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * @brief
 * Generate points on a sphere per Marsaglia (1972).
 *
 * The return code is the number of points generated.
 *
 * The user is responsible for selecting the numerical generator used to
 * supply pseudo or quasi-random numbers to bg_sph_sample - different
 * types of inputs may be needed depending on the application.
 */
BG_EXPORT extern size_t bg_sph_sample(point_t *pnts, size_t cnt, const point_t center, const fastf_t radius, bn_numgen n);

__END_DECLS

#endif  /* BG_POINTGEN_H */
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
