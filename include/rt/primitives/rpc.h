/*                        R P C . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @addtogroup rt_rpc */
/** @{ */
/** @file rt/primitives/rpc.h */

#ifndef RT_PRIMITIVES_RPC_H
#define RT_PRIMITIVES_RPC_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

RT_EXPORT extern int rt_mk_parabola(struct rt_pt_node *pts,
                                    fastf_t r,
                                    fastf_t b,
                                    fastf_t dtol,
                                    fastf_t ntol);


RT_EXPORT extern struct bn_vlblock *rt_vlblock_init(void);


RT_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

RT_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
                                                 int r,
                                                 int g,
                                                 int b);

__END_DECLS

/** @} */

#endif /* RT_PRIMITIVES_RPC_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
