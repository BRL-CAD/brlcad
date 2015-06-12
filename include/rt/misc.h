/*                      M I S C . H
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
/** @file rt/misc.h
 *
 * Catch all for functions that don't have a clear sub-header yet.
 */

#ifndef RT_MISC_H
#define RT_MISC_H

#include "common.h"
#include "vmath.h"
#include "rt/defines.h"

__BEGIN_DECLS

RT_EXPORT extern int rt_find_paths(struct db_i *dbip,
                                   struct directory *start,
                                   struct directory *end,
                                   struct bu_ptbl *paths,
                                   struct resource *resp);

RT_EXPORT extern struct bu_bitv *rt_get_solidbitv(size_t nbits,
                                                  struct resource *resp);

/* table.c */
RT_EXPORT extern int rt_id_solid(struct bu_external *ep);
RT_EXPORT extern int rt_generic_xform(struct rt_db_internal     *op,
                                      const mat_t               mat,
                                      struct rt_db_internal     *ip,
                                      int                       avail,
                                      struct db_i               *dbip,
                                      struct resource           *resp);

/**
 * Used by MGED for labeling vertices of a solid.
 *
 * TODO - eventually this should fade into a general annotation
 * framework
 */
struct rt_point_labels {
    char str[8];
    point_t pt;
};

/* WARNING - The function below is *HIGHLY* experimental and will certainly
 * change */
RT_EXPORT void
rt_generate_mesh(int **faces, int *num_faces, point_t **points, int *num_pnts,
                                struct db_i *dbip, const char *obj, fastf_t delta);


__END_DECLS

#endif /* RT_MISC_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
