/*                      D E C I M A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
/** @file decimate.c
 *
 * Brief description
 *
 */

#include "common.h"

#include "raytrace.h"
#include "gcv/api.h"


HIDDEN int
decimate_filter(struct gcv_context *context, const struct gcv_opts *gcv_options,
	const void *UNUSED(options_data), const char *UNUSED(target))
{
    size_t i;

    for (i = 0; i < gcv_options->num_objects; ++i) {
	struct directory *dir;
	struct rt_db_internal internal;

	{
	    struct db_full_path path;

	    if (db_string_to_path(&path, context->dbip, gcv_options->object_names[i])) {
		bu_log("db_string_to_path() failed: '%s'\n", gcv_options->object_names[i]);
		db_free_full_path(&path);
		return 0;
	    }

	    dir = DB_FULL_PATH_CUR_DIR(&path);
	    db_free_full_path(&path);
	}

	if (rt_db_get_internal(&internal, dir, context->dbip, NULL, &rt_uniresource) < 0) {
	    bu_log("rt_db_get_internal() failed: '%s'\n", dir->d_namep);
	    return 0;
	}

	RT_CK_DB_INTERNAL(&internal);

	if (internal.idb_minor_type != ID_BOT) {
	    bu_log("object is not a BoT mesh: '%s'\n", dir->d_namep);
	    rt_db_free_internal(&internal);
	    return 0;
	}

	{
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)internal.idb_ptr;
	    size_t orig_num_faces, edges_removed;

	    RT_BOT_CK_MAGIC(bot);

	    if (gcv_options->verbosity_level)
		bu_log("decimating: '%s'\n", dir->d_namep);

	    orig_num_faces = bot->num_faces;
	    edges_removed = rt_bot_decimate_gct(bot, gcv_options->calculational_tolerance.dist);

	    if (gcv_options->verbosity_level) {
		bu_log("\toriginal face count = %zu\n", orig_num_faces);
		bu_log("\tedges removed = %zu\n", edges_removed);
		bu_log("\tnew face count = %zu\n", bot->num_faces);
	    }
	}

	if (rt_db_put_internal(dir, context->dbip, &internal, &rt_uniresource)) {
	    bu_log("rt_db_put_internal() failed: '%s'\n", dir->d_namep);
	    rt_db_free_internal(&internal);
	    return 0;
	}
    }

    return 1;
}


static const struct gcv_filter gcv_filter_decimate =
{"Decimate", GCV_FILTER_FILTER, BU_MIME_MODEL_UNKNOWN, BU_MIME_UNKNOWN, NULL, NULL, NULL, decimate_filter};

static const struct gcv_filter * const filters[] = {&gcv_filter_decimate, NULL};
const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *
	gcv_plugin_info(){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
