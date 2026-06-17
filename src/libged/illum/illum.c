/*                         I L L U M . C
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
/** @file libged/illum.c
 *
 * The illum command.
 *
 */

#include "common.h"
#include <string.h>

#include "dm.h" // For labelvert - see if we really need the dm_set_native_repaint_pending call there...

#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/render.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "ged/view.h"
#include "../ged_private.h"

/* Callback data for labelvert */
struct labelvert_data {
    struct directory *dp;
    double base2local;
    struct bsg_feature_label_data *labels;
    size_t label_count;
    size_t label_capacity;
};

#define LABELVERT_FEATURE_NAME "_LABELVERT_ffffff"

static int
labelvert_record_matches(struct db_i *dbip, const struct bsg_export_record *rec, struct directory *dp)
{
    if (!dbip || !rec || !dp || rec->source.scope != BSG_RENDER_SOURCE_SCOPE_DATABASE)
	return 0;

    struct db_full_path fp;
    db_full_path_init(&fp);
    if (db_string_to_path(&fp, dbip, bu_vls_cstr(&rec->path)) < 0)
	return 0;
    int ret = db_full_path_search(&fp, dp);
    db_free_full_path(&fp);
    return ret;
}

static void
labelvert_data_free(struct labelvert_data *lvd)
{
    if (!lvd)
	return;

    for (size_t i = 0; i < lvd->label_count; i++) {
	if (lvd->labels[i].text)
	    bu_free((char *)lvd->labels[i].text, "labelvert label text");
    }
    if (lvd->labels)
	bu_free(lvd->labels, "labelvert label data");
    lvd->labels = NULL;
    lvd->label_count = 0;
    lvd->label_capacity = 0;
}

static int
labelvert_append_label(struct labelvert_data *lvd, const point_t pt)
{
    char label[256];

    if (!lvd)
	return 0;

    if (lvd->label_count + 1 > lvd->label_capacity) {
	size_t ncap = lvd->label_capacity ? lvd->label_capacity * 2 : 64;
	lvd->labels = (struct bsg_feature_label_data *)bu_realloc(lvd->labels,
		ncap * sizeof(struct bsg_feature_label_data), "labelvert label data");
	for (size_t i = lvd->label_capacity; i < ncap; i++) {
	    struct bsg_feature_label_data init = BSG_FEATURE_LABEL_DATA_INIT;
	    lvd->labels[i] = init;
	}
	lvd->label_capacity = ncap;
    }

    snprintf(label, sizeof(label), " %g, %g, %g",
	    pt[0] * lvd->base2local,
	    pt[1] * lvd->base2local,
	    pt[2] * lvd->base2local);
    struct bsg_feature_label_data init = BSG_FEATURE_LABEL_DATA_INIT;
    lvd->labels[lvd->label_count] = init;
    lvd->labels[lvd->label_count].text = bu_strdup(label);
    VMOVE(lvd->labels[lvd->label_count].point, pt);
    lvd->labels[lvd->label_count].color_valid = 1;
    VSET(lvd->labels[lvd->label_count].color, 255, 255, 255);
    lvd->label_count++;
    return 1;
}

static int
labelvert_point_cb(const point_t pt, void *data)
{
    return labelvert_append_label((struct labelvert_data *)data, pt);
}

static void
labelvert_export_record(struct db_i *dbip, const struct bsg_export_record *rec, struct labelvert_data *lvd)
{
    if (!lvd || !labelvert_record_matches(dbip, rec, lvd->dp))
	return;

    (void)bsg_export_record_foreach_point(rec, labelvert_point_cb, lvd);
}

/* Usage:  labelvert solid(s) */
int
ged_labelvert_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct labelvert_data lvd;
    static const char *usage = "object(s) - label vertices of wireframes of objects";

    if (!gedp || !gedp->dbip)
	return BRLCAD_ERROR;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    memset(&lvd, 0, sizeof(lvd));
    lvd.base2local = gedp->dbip->dbi_base2local;

    for (i=1; i<argc; i++) {
	struct directory *dp;
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	/* Find displayed uses of this database object. */
	lvd.dp = dp;
	struct bsg_export_request request;
	bsg_export_request_init(&request, gedp->ged_gvp);
	request.query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY | BSG_EXPORT_QUERY_DB_OBJECTS;
	request.render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
	struct bsg_export_result *result = bsg_export_query(&request);
	if (!result)
	    continue;
	for (size_t ri = 0; ri < bsg_export_result_count(result); ri++)
	    labelvert_export_record(gedp->dbip, bsg_export_result_get(result, ri), &lvd);
	bsg_export_result_free(result);
    }

    (void)bsg_feature_remove(gedp->ged_gvp, LABELVERT_FEATURE_NAME);
    if (lvd.label_count) {
	bsg_feature_ref ref = bsg_feature_create_label(gedp->ged_gvp,
		LABELVERT_FEATURE_NAME, 0);
	if (bsg_feature_ref_is_null(ref) ||
		!bsg_feature_labels_replace(ref, lvd.labels, lvd.label_count)) {
	    labelvert_data_free(&lvd);
	    bu_vls_printf(gedp->ged_result_str, "failed to create labelvert feature\n");
	    return BRLCAD_ERROR;
	}
    }

    labelvert_data_free(&lvd);
    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (dmp)
	dm_set_native_repaint_pending(dmp, 1);
    return BRLCAD_OK;
}


/*
 * Illuminate/highlight database object
 *
 * Usage:
 * illum [-n] obj
 *
 */
int
ged_illum_core(struct ged *gedp, int argc, const char *argv[])
{
    int illum = 1;
    static const char *usage = "[-n] obj";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc == 3) {
	if (argv[1][0] == '-' && argv[1][1] == 'n')
	    illum = 0;
	else
	    goto bad;

	--argc;
	++argv;
    }

    if (argc != 2)
	goto bad;

    struct ged_draw_transaction txn =
	ged_draw_transaction_make_value(GED_DRAW_TXN_HIGHLIGHT,
					argv[1], (double)illum);
    struct ged_draw_transaction_result result;
    ged_draw_transaction_result_init(&result);
    int changed = ged_draw_apply_transaction(gedp, &txn, &result);
    ged_draw_transaction_result_free(&result);
    if (!changed) {
	bu_vls_printf(gedp->ged_result_str, "illum: %s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_ILLUM_COMMANDS(X, XID) \
    X(illum, ged_illum_core, GED_CMD_DEFAULT) \
    X(labelvert, ged_labelvert_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ILLUM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_illum", 1, GED_ILLUM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
