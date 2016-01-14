/*                      D E C I M A T E . C
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
/** @file decimate.c
 *
 * Brief description
 *
 */

#include "common.h"

#include "gcv/api.h"
#include "gcv/util.h"


HIDDEN int
decimate_filter(struct gcv_context *context, const struct gcv_opts *gcv_options,
	const void *UNUSED(options_data), const char *UNUSED(target))
{
    size_t i;

    for (i = 0; i < gcv_options->num_objects; ++i) {
	struct db_full_path path;
	struct rt_db_internal internal;
	struct rt_bot_internal *bot;

	if (db_string_to_path(&path, context->dbip, gcv_options->object_names[i])) {
	    bu_log("db_string_to_path() failed: %s\n", gcv_options->object_names[i]);
	    return 0;
	}

	if (rt_db_get_internal(&internal, DB_FULL_PATH_CUR_DIR(&path), context->dbip, NULL, &rt_uniresource) < 0) {
	    db_pr_full_path("rt_db_get_internal() failed: ", &path);
	    db_free_full_path(&path);
	    return 0;
	}

	RT_CK_DB_INTERNAL(&internal);

	if (internal.idb_minor_type != ID_BOT) {
	    db_pr_full_path("object is not a BoT mesh: ", &path);
	    rt_db_free_internal(&internal);
	    db_free_full_path(&path);
	    return 0;
	}

	bot = (struct rt_bot_internal *)internal.idb_ptr;
	RT_BOT_CK_MAGIC(bot);

	(void)rt_bot_decimate_gct(bot, gcv_options->calculational_tolerance.dist);

	if (rt_db_put_internal(DB_FULL_PATH_CUR_DIR(&path), context->dbip, &internal, &rt_uniresource)) {
	    db_pr_full_path("rt_db_put_internal() failed: ", &path);
	    rt_db_free_internal(&internal);
	    db_free_full_path(&path);
	    return 0;
	}

	db_free_full_path(&path);
    }

    return 1;
}


const struct gcv_filter gcv_filter_decimate =
{"Decimate", GCV_FILTER_FILTER, MIME_MODEL_UNKNOWN, NULL, NULL, decimate_filter};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
