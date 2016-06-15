/*                   V L B L O C K . H
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
/** @file rt/vlblock.h
 *
 */

#ifndef RT_VLBLOCK_H
#define RT_VLBLOCK_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"

__BEGIN_DECLS

/* FIXME: Has some stuff mixed in here that should go in LIBBN */
/************************************************************************
 *                                                                      *
 *                      Generic VLBLOCK routines                        *
 *                                                                      *
 ************************************************************************/

RT_EXPORT extern struct bn_vlblock *bn_vlblock_init(struct bu_list      *free_vlist_hd, /* where to get/put free vlists */
                                                    int         max_ent);

RT_EXPORT extern struct bn_vlblock *    rt_vlblock_init(void);


RT_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

RT_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
                                                 int r,
                                                 int g,
                                                 int b);

__END_DECLS

#endif /* RT_VLBLOCK_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
