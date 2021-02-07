/*                        D S P . H
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
/** @addtogroup rt_dsp */
/** @{ */
/** @file rt/primitives/dsp.h */

#ifndef RT_PRIMITIVES_DSP_H
#define RT_PRIMITIVES_DSP_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"
#include "rt/soltab.h"

__BEGIN_DECLS

RT_EXPORT extern int dsp_pos(point_t out,
			     struct soltab *stp,
			     point_t p);


__END_DECLS

#endif /* RT_PRIMITIVES_DSP_H */

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
