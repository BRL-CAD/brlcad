/*                         A D J U S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/adjust.c
 *
 * The adjust command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/str.h"
#include "ged.h"
#include "ged/event_txn.h"
#include "rt/db_attr.h"


static void
adjust_classify_comb_fields(int *tree_changed, int *attr_changed,
	int *material_changed, int argc, const char *argv[])
{
    int i;

    *tree_changed = 0;
    *attr_changed = 0;
    *material_changed = 0;

    for (i = 0; i + 1 < argc; i += 2) {
	const char *attr = argv[i];

	if (!attr)
	    continue;

	if (BU_STR_EQUIV(attr, "tree")) {
	    *tree_changed = 1;
	} else if (BU_STR_EQUIV(attr, "shader") ||
		BU_STR_EQUIV(attr, "material") ||
		db5_standardize_attribute(attr) == ATTR_COLOR) {
	    *material_changed = 1;
	} else {
	    *attr_changed = 1;
	    if (BU_STR_EQUIV(attr, "id") ||
		    BU_STR_EQUIV(attr, "giftmater"))
		*material_changed = 1;
	}
    }
}


int
ged_adjust_core(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    int is_comb = 0;
    int tree_changed = 0;
    int attr_changed = 0;
    int material_changed = 0;
    struct directory *dp;
    char *name;
    struct rt_db_internal intern;
    static const char *usage = "object attr value ?attr value?";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    name = (char *)argv[1];

    GED_DB_LOOKUP(gedp, dp, name, LOOKUP_QUIET, BRLCAD_ERROR);

    GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    RT_CK_FUNCTAB(intern.idb_meth);

    if (!intern.idb_meth->ft_adjust) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) adjust failure", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    is_comb = (intern.idb_type == ID_COMBINATION);
    if (is_comb)
	adjust_classify_comb_fields(&tree_changed, &attr_changed,
		&material_changed, argc-2, argv+2);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    status = intern.idb_meth->ft_adjust(gedp->ged_result_str, &intern, argc-2, argv+2);
    if (status != BRLCAD_OK) {
	rt_db_free_internal(&intern);
	return status;
    }

    int event_depth = ged_event_batch_begin(gedp);
    if (wdb_put_internal(wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) failure", name);
	if (event_depth > 0)
	    (void)ged_event_batch_end(gedp, NULL);
	return BRLCAD_ERROR;
    }

    if (is_comb) {
	if (tree_changed)
	    (void)ged_event_notify_comb_tree_changed(gedp, name, 1, NULL);
	if (attr_changed)
	    (void)ged_event_notify_attribute_changed(gedp, name, 1, NULL);
	if (material_changed)
	    (void)ged_event_notify_object_material_changed(gedp, name, NULL);
    } else {
	(void)ged_event_notify_object_modified(gedp, name, 1, NULL);
    }

    if (event_depth > 0)
	(void)ged_event_batch_end(gedp, NULL);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_ADJUST_COMMANDS(X, XID) \
    X(adjust, ged_adjust_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ADJUST_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_adjust", 1, GED_ADJUST_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
