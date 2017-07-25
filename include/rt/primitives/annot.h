/*                        A N N O T . H
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
/** @addtogroup rt_annot */
/** @{ */
/** @file rt/primitives/annot.h */

#ifndef RT_PRIMITIVES_ANNOT_H
#define RT_PRIMITIVES_ANNOT_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "rt/defines.h"

__BEGIN_DECLS

RT_EXPORT int rt_pos_flag(int *pos_flag, int p_hor, int p_ver);

RT_EXPORT int rt_check_pos(const struct txt_seg *tsg, char **rel_pos);

/**RT_EXPORT extern int ant_to_vlist(struct bu_list             *vhead,
                                    const struct rt_tess_tol *ttol,
                                    point_t                   V,
                                    struct rt_annot_internal *annot_ip,
                                    struct rt_ant             *ant);
**/
RT_EXPORT extern int rt_check_ant(const struct rt_ant *ant,
                                    const struct rt_annot_internal *annot_ip,
                                    int noisy);

RT_EXPORT extern void rt_copy_ant(struct rt_ant *ant_out,
                                    const struct rt_ant *ant_in);

RT_EXPORT extern void rt_ant_free(struct rt_ant *ant);

RT_EXPORT extern struct rt_annot_internal *rt_copy_annot(const struct rt_annot_internal *annot_ip);
RT_EXPORT extern int ant_to_tcl_list(struct bu_vls *vls,
                                       struct rt_ant *ant);

__END_DECLS

/** @} */

#endif /* RT_PRIMITIVES_ANNOT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
