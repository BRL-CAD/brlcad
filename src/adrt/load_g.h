/*                      L O A D _ G . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef ADRT_LOAD_G_H
#define ADRT_LOAD_G_H

#include "common.h"

#include "adrt.h"

struct db_i;
struct db_tree_state;
struct db_full_path;
struct rt_db_internal;
union tree;

__BEGIN_DECLS

RENDER_EXPORT extern int adrt_path_has_geometry(struct db_i *dbip,
	const char *path);
RENDER_EXPORT extern union tree *adrt_leaf_tess(struct db_tree_state *tsp,
	const struct db_full_path *pathp, struct rt_db_internal *ip,
	void *client_data);

__END_DECLS

#endif /* ADRT_LOAD_G_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
