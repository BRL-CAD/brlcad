/*                        T O R . H
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
/** @addtogroup rt_tor */
/** @{ */
/** @file rt/primitives/tor.h */

#ifndef RT_PRIMITIVES_TOR_H
#define RT_PRIMITIVES_TOR_H

#include "common.h"
#include "rt/defines.h"

__BEGIN_DECLS

RT_EXPORT extern int rt_num_circular_segments(double maxerr,
					      double radius);

__END_DECLS

/** @} */

#endif /* RT_PRIMITIVES_TOR_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
