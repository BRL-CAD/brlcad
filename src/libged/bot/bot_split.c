/*                       B O T _ S P L I T . C
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
/** @file libged/bot_split.c
 *
 * Shared implementation for the bot split subcommand and the deprecated
 * bot_split command.
 */

#include "common.h"

#include <stdlib.h>

#include "bu/malloc.h"
#include "bu/path.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "wdb.h"
#include "../ged_private.h"


static void
free_generated_names(struct bu_vls *names, size_t count)
{
    if (!names)
	return;
    for (size_t i = 0; i < count; ++i)
	bu_vls_free(&names[i]);
    bu_free(names, "BOT split output names");
}


static void
rollback_created_bots(struct ged *gedp, struct bu_vls *names, size_t count,
	struct bu_vls *errors)
{
    for (size_t i = 0; i < count; ++i) {
	struct directory *dp = db_lookup(gedp->dbip, bu_vls_cstr(&names[i]),
	    LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    continue;
	int delete_result = db_delete(gedp->dbip, dp);
	int directory_result = db_dirdelete(gedp->dbip, dp);
	if (delete_result != 0 || directory_result != 0)
	    bu_vls_printf(errors, "Failed to roll back %s\n",
		bu_vls_cstr(&names[i]));
    }
}


static int
create_bot_group(struct ged *gedp, const char *group_name,
	struct bu_vls *names, size_t count, struct bu_vls *errors)
{
    struct wmember members;
    BU_LIST_INIT(&members.l);
    for (size_t i = 0; i < count; ++i) {
	if (!mk_addmember(bu_vls_cstr(&names[i]), &members.l, NULL,
		DB_OP_UNION)) {
	    bu_vls_printf(errors, "Cannot add %s to group %s\n",
		bu_vls_cstr(&names[i]), group_name);
	    mk_freemembers(&members.l);
	    return BRLCAD_ERROR;
	}
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip,
	RT_WDB_TYPE_DB_DEFAULT_APPEND_ONLY);
    if (!wdbp || mk_lcomb(wdbp, group_name, &members, 0, NULL, NULL, NULL,
	    0) != 0) {
	bu_vls_printf(errors, "Cannot create BOT split group %s\n", group_name);
	if (!BU_LIST_IS_EMPTY(&members.l))
	    mk_freemembers(&members.l);
	return BRLCAD_ERROR;
    }

    db_update_nref(gedp->dbip);
    return BRLCAD_OK;
}


int
_ged_bot_split_object(struct ged *gedp, const char *object_name,
	const char *group_name, struct bu_vls *output_names,
	struct bu_vls *errors)
{
    if (group_name && db_lookup(gedp->dbip, group_name,
	    LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(errors, "Object %s already exists\n", group_name);
	return -1;
    }

    struct directory *source_dp = db_lookup(gedp->dbip, object_name,
	LOOKUP_QUIET);
    if (source_dp == RT_DIR_NULL) {
	bu_vls_printf(errors, "Cannot find %s\n", object_name);
	return -1;
    }

    struct rt_db_internal source_internal;
    if (rt_db_get_internal(&source_internal, source_dp, gedp->dbip,
	    bn_mat_identity) < 0) {
	bu_vls_printf(errors, "Cannot read %s\n", object_name);
	return -1;
    }
    if (source_internal.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	    source_internal.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(errors, "%s is not a BOT solid\n", object_name);
	rt_db_free_internal(&source_internal);
	return -1;
    }

    struct rt_bot_internal *source_bot =
	(struct rt_bot_internal *)source_internal.idb_ptr;
    struct rt_bot_list *components = rt_bot_split(source_bot);
    if (!components) {
	bu_vls_printf(errors, "Failed to split %s\n", object_name);
	rt_db_free_internal(&source_internal);
	return -1;
    }

    size_t component_count = 0;
    struct rt_bot_list *entry;
    for (BU_LIST_FOR(entry, rt_bot_list, &components->l))
	++component_count;
    if (!component_count) {
	rt_bot_list_free(components, 1);
	rt_db_free_internal(&source_internal);
	return 0;
    }

    struct bu_vls *names = (struct bu_vls *)bu_calloc(component_count,
	sizeof(struct bu_vls), "BOT split output names");
    size_t suffix = 0;
    for (size_t i = 0; i < component_count; ++i) {
	bu_vls_init(&names[i]);
	do {
	    bu_vls_sprintf(&names[i], "%s.%zu", object_name, suffix++);
	} while ((group_name && BU_STR_EQUAL(bu_vls_cstr(&names[i]),
		group_name)) || db_lookup(gedp->dbip, bu_vls_cstr(&names[i]),
		LOOKUP_QUIET) != RT_DIR_NULL);
    }

    size_t written_count = 0;
    size_t component = 0;
    int failed = 0;
    for (BU_LIST_FOR(entry, rt_bot_list, &components->l)) {
	struct rt_db_internal output_internal;
	RT_DB_INTERNAL_INIT(&output_internal);
	output_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	output_internal.idb_type = ID_BOT;
	output_internal.idb_meth = &OBJ[ID_BOT];
	output_internal.idb_ptr = (void *)entry->bot;
	bu_avs_merge(&output_internal.idb_avs, &source_internal.idb_avs);

	struct directory *output_dp = db_diradd(gedp->dbip,
	    bu_vls_cstr(&names[component]), RT_DIR_PHONY_ADDR, 0,
	    RT_DIR_SOLID, (void *)&output_internal.idb_type);
	if (output_dp == RT_DIR_NULL) {
	    bu_vls_printf(errors, "Cannot add %s to the database\n",
		bu_vls_cstr(&names[component]));
	    bu_avs_free(&output_internal.idb_avs);
	    failed = 1;
	    break;
	}

	int write_result = rt_db_put_internal(output_dp, gedp->dbip,
	    &output_internal);
	// rt_db_put_internal clears idb_ptr when it releases the BOT.  Retain any
	// pointer it leaves behind so the common cleanup path owns it.
	entry->bot = (struct rt_bot_internal *)output_internal.idb_ptr;
	if (write_result < 0) {
	    bu_vls_printf(errors, "Failed to write %s\n",
		bu_vls_cstr(&names[component]));
	    (void)db_delete(gedp->dbip, output_dp);
	    (void)db_dirdelete(gedp->dbip, output_dp);
	    failed = 1;
	    break;
	}

	++written_count;
	++component;
    }

    if (failed) {
	rollback_created_bots(gedp, names, written_count, errors);
	rt_bot_list_free(components, 1);
	free_generated_names(names, component_count);
	rt_db_free_internal(&source_internal);
	return -1;
    }

    if (group_name && create_bot_group(gedp, group_name, names,
	    component_count, errors) != BRLCAD_OK) {
	rollback_created_bots(gedp, names, written_count, errors);
	rt_bot_list_free(components, 1);
	free_generated_names(names, component_count);
	rt_db_free_internal(&source_internal);
	return -1;
    }

    for (size_t i = 0; i < component_count; ++i)
	bu_vls_printf(output_names, "%s%s", i ? " " : "",
	    bu_vls_cstr(&names[i]));
    rt_bot_list_free(components, 1);
    free_generated_names(names, component_count);
    rt_db_free_internal(&source_internal);
    return (int)component_count;
}


int
ged_bot_split_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "bot [bot2 bot3 ...]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    struct bu_vls results = BU_VLS_INIT_ZERO;
    struct bu_vls errors = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_OK;
    for (int i = 1; i < argc; ++i) {
	char *object_name = bu_path_basename(argv[i], NULL);
	if (BU_STR_EQUAL(object_name, ".")) {
	    bu_free(object_name, "BOT split basename");
	    object_name = bu_strdup(argv[i]);
	}

	struct bu_vls names = BU_VLS_INIT_ZERO;
	int split_count = _ged_bot_split_object(gedp, object_name, NULL,
	    &names, &errors);
	if (split_count < 0)
	    ret = BRLCAD_ERROR;
	bu_vls_printf(&results, "{%s {%s}} ", object_name,
	    bu_vls_cstr(&names));
	bu_vls_free(&names);
	bu_free(object_name, "BOT split basename");
    }

    bu_vls_printf(gedp->ged_result_str, "%s{%s}", bu_vls_cstr(&results),
	bu_vls_cstr(&errors));
    bu_vls_free(&results);
    bu_vls_free(&errors);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
