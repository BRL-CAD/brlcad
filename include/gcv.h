/*                           G C V . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file gcv.h
 *
 * Functions provided by the LIBGCV geometry conversion library.
 *
 */

#ifndef __GCV_H__
#define __GCV_H__

#include "raytrace.h"

__BEGIN_DECLS

#ifndef GCV_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef GCV_EXPORT_DLL
#      define GCV_EXPORT __declspec(dllexport)
#    else
#      define GCV_EXPORT __declspec(dllimport)
#    endif
#  else
#    define GCV_EXPORT
#  endif
#endif

/**
 * G C V _ R E G I O N _ E N D
 *
 * Usually specified as the db_walk_tree() region_end callback,
 * calling this routine for each positive region encountered.
 *
 * The client_data parameter is expected to be a function pointer for
 * a routine that will write out the region in a given file format:
 *
@code
void (*write_region)(struct nmgregion *r, const struct db_full_path *pathp, int region_id, int material_id, float color[3]);
@endcode
 *
 * This routine must be prepared to run in parallel.
 */
GCV_EXPORT extern union tree *gcv_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data);

/**
 * G C V _ R E G I O N _ E N D _ M C
 *
 * Exact same as gcv_region_end, except using the marching cubes algorithm.
 */
GCV_EXPORT extern union tree *gcv_region_end_mc(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data);


/**
 * G C V _ B O T T E S S _ R E G I O N _ E N D
 *
 */
GCV_EXPORT extern union tree *gcv_bottess_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data);


/**
 * G C V _ B O T T E S S
 *
 */
GCV_EXPORT extern union tree *gcv_bottess(int argc, const char **argv, struct db_i *dbip, struct rt_tess_tol *ttol);



__END_DECLS

#endif /* __GCV_H__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
