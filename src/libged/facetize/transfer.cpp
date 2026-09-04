/*                   T R A N S F E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include "raytrace.h"

#include "./transfer.h"
#include "./worker.h"

int
facetize_transfer_staged_object(struct db_i *target_dbip,
	const char *result_file, const char *object_name,
	int expected_object_type)
{
    if (!target_dbip || !result_file || !object_name ||
	    expected_object_type <= ID_NULL ||
	    expected_object_type > ID_MAXIMUM)
	return BRLCAD_ERROR;

    struct db_i *result_dbip = db_open(result_file, DB_OPEN_READONLY);
    if (!result_dbip)
	return BRLCAD_ERROR;
    if (db_dirbuild(result_dbip) < 0) {
	db_close(result_dbip);
	return BRLCAD_ERROR;
    }

    struct directory *result_dp = db_lookup(result_dbip,
	    FACETIZE_WORKER_RESULT_OBJECT, LOOKUP_QUIET);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (!result_dp || result_dp->d_major_type != DB5_MAJORTYPE_BRLCAD ||
	    result_dp->d_minor_type != expected_object_type ||
	    rt_db_get_internal(&intern, result_dp, result_dbip, NULL) < 0 ||
	    intern.idb_type != expected_object_type) {
	if (intern.idb_ptr)
	    rt_db_free_internal(&intern);
	db_close(result_dbip);
	return BRLCAD_ERROR;
    }
    db_close(result_dbip);

    struct directory *old_dp = db_lookup(target_dbip, object_name,
	    LOOKUP_QUIET);
    if (old_dp && (db_delete(target_dbip, old_dp) != 0 ||
	    db_dirdelete(target_dbip, old_dp) != 0)) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    struct directory *target_dp = db_diradd(target_dbip, object_name,
	    RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (target_dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    if (rt_db_put_internal(target_dp, target_dbip, &intern) < 0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
