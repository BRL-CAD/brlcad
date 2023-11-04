/*                        C O N V . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2023 United States Government as represented by
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
/** @file rt/conv.h */

#ifndef RT_CONV_H
#define RT_CONV_H

#include "common.h"
#include "bu/list.h"
#include "bn/tol.h"
#include "rt/defines.h"
#include "rt/db_internal.h"
#include "rt/resource.h"
#include "nmg.h"

__BEGIN_DECLS

RT_EXPORT extern union tree *rt_booltree_leaf_tess(struct db_tree_state *tsp,
						    const struct db_full_path *pathp,
						    struct rt_db_internal *ip,
						    void *client_data);

RT_EXPORT extern union tree *rt_booltree_evaluate(union tree *tp,
						   struct bu_list *vlfree,
						   const struct bn_tol *tol,
						   struct resource *resp,
						   int (*do_bool)(union tree *, union tree *, union tree *, int op, struct bu_list *, const struct bn_tol *, void *),
						   int verbose,
						   void *data
						   );

__END_DECLS

#endif /* RT__CONV_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
