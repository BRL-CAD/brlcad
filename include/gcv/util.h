/*                      G C V _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file gcv/util.h
 *
 * Utility functions provided by the LIBGCV geometry conversion
 * library.
 *
 */

#ifndef GCV_UTIL_H
#define GCV_UTIL_H

#include "common.h"

#include "gcv/defines.h"

#include "rt/geom.h"
#include "rt/tree.h"


__BEGIN_DECLS


/*
 * Tessellate the object at the specified path.
 */
struct rt_bot_internal *gcv_facetize(struct db_i *db, const struct db_full_path *path, const struct bn_tol *tol, const struct bg_tess_tol *tess_tol);


/*
 * Topological test for determining whether the given BoT satisfies
 * the conditions for solidity.
 *
 * Equivalent to gcv_bot_is_closed_fan() && gcv_bot_is_orientable()
 */
GCV_EXPORT extern int gcv_bot_is_solid(const struct rt_bot_internal *bot);

GCV_EXPORT extern int gcv_bot_is_closed_fan(const struct rt_bot_internal *bot);

GCV_EXPORT extern int gcv_bot_is_orientable(const struct rt_bot_internal *bot);


/**
 * write_region is a function pointer to a routine that will
 * write out the region in a given file format.
 *
 * This routine must be prepared to run in parallel.
 */
struct gcv_region_end_data
{
    void (*write_region)(struct nmgregion *r, const struct db_full_path *pathp, int region_id, int material_id, float color[3], void *client_data);
    void *client_data;
};

/**
 * Perform Boolean evaluation on a tree of tessellated leaf nodes.
 *
 * Usually specified as the db_walk_tree() region_end callback,
 * calling this routine for each positive region encountered.
 *
 * The client_data parameter is expected to point to a struct gcv_region_end_data.
 */
GCV_EXPORT extern union tree *gcv_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);

/**
 * Exact same as gcv_region_end, except using the marching cubes algorithm.
 */
GCV_EXPORT extern union tree *gcv_region_end_mc(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);


GCV_EXPORT extern union tree *gcv_bottess_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, void *client_data);


GCV_EXPORT extern union tree *gcv_bottess(int argc, const char **argv, struct db_i *dbip, struct bg_tess_tol *ttol);


__END_DECLS

#endif /* GCV_UTIL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
