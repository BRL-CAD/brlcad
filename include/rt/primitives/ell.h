/*                        E L L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup rt_ell */
/** @{ */
/** @file rt/primitives/ell.h */

#ifndef RT_PRIMITIVES_ELL_H
#define RT_PRIMITIVES_ELL_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* TODO - can this move elsewhere? */
RT_EXPORT extern void rt_ell_16pnts(fastf_t *ov,
				   fastf_t *V,
				   fastf_t *A,
				   fastf_t *B);
/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_ELL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
