/*                    T E S S E L L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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
/** @file tessellate.c
 *
 * Tessellate the specified objects at the region level.
 *
 */


#include "common.h"

#include "gcv/api.h"
#include "gcv/util.h"


struct tessellate_data {
    struct gcv_context *context;
    const struct gcv_opts *gcv_options;
    int success;
};


HIDDEN void
write_nmg_region(struct nmgregion *nmg_region, const struct db_full_path *path,
		 int UNUSED(region_id), int UNUSED(material_id), float *UNUSED(color),
		 void *client_data)
{
    struct tessellate_data * const data = (struct tessellate_data *)client_data;

    struct shell *current_shell;

    NMG_CK_REGION(nmg_region);
    NMG_CK_MODEL(nmg_region->m_p);
    RT_CK_FULL_PATH(path);

    for (BU_LIST_FOR(current_shell, shell, &nmg_region->s_hd)) {
	struct rt_db_internal internal;

	NMG_CK_SHELL(current_shell);

	/* fill in an rt_db_internal with our new bot so we can free it */
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[internal.idb_minor_type];
	internal.idb_ptr = nmg_bot(current_shell, &data->gcv_options->calculational_tolerance);

	if (rt_db_put_internal(DB_FULL_PATH_CUR_DIR(path), data->context->dbip, &internal, &rt_uniresource)) {
	    db_pr_full_path("rt_db_put_internal() failed: ", path);
	    data->success = 0;
	    rt_db_free_internal(&internal);
	}
    }
}


HIDDEN int
tessellate_filter(struct gcv_context *context, const struct gcv_opts *gcv_options,
	const void *UNUSED(options_data), const char *UNUSED(target))
{
    int ret;
    struct model *nmg_model;
    struct db_tree_state initial_tree_state;
    struct tessellate_data data;
    struct gcv_region_end_data gcv_data;

    data.context = context;
    data.gcv_options = gcv_options;
    data.success = 1;
    gcv_data.write_region = write_nmg_region;
    gcv_data.client_data = (void *)&data;

    initial_tree_state = rt_initial_tree_state;
    initial_tree_state.ts_tol = &gcv_options->calculational_tolerance;
    initial_tree_state.ts_ttol = &gcv_options->tessellation_tolerance;;
    initial_tree_state.ts_m = &nmg_model;

    nmg_model = nmg_mm();
    ret = db_walk_tree(context->dbip, (int)gcv_options->num_objects,
	    (const char **)gcv_options->object_names, 1,
	    &initial_tree_state, NULL,
	    gcv_region_end, nmg_booltree_leaf_tess, &gcv_data);
    nmg_km(nmg_model);

    if (ret) {
	bu_log("db_walk_tree() failed\n");
	return 0;
    }

    return data.success;
}


const struct gcv_filter gcv_filter_tessellate =
{"Tessellate", GCV_FILTER_FILTER, MIME_MODEL_UNKNOWN, NULL, NULL, tessellate_filter};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
