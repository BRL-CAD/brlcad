/* D A T A B A S E . C
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

#include <string.h>

#include "bu/str.h"
#include "bu/units.h"
#include "gcv/runtime.h"
#include "gcv/util.h"
#include "rt/db4.h"
#include "rt/db5.h"
#include "rt/db_attr.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/db_instance.h"
#include "rt/mater.h"
#include "rt/primitives/pg.h"
#include "rt/wdb.h"

static int
append_only(const struct rt_wdb *writer)
{
    return writer->type == RT_WDB_TYPE_DB_DISK_APPEND_ONLY ||
	writer->type == RT_WDB_TYPE_DB_INMEM_APPEND_ONLY ||
	writer->type == RT_WDB_TYPE_DB_DEFAULT_APPEND_ONLY;
}

static int
v4_comb_attribute(int attribute)
{
    switch (attribute) {
	case ATTR_REGION:
	case ATTR_REGION_ID:
	case ATTR_MATERIAL_ID:
	case ATTR_AIR:
	case ATTR_LOS:
	case ATTR_COLOR:
	case ATTR_SHADER:
	case ATTR_INHERIT:
	    return 1;
	default:
	    return 0;
    }
}

static int
v4_preflight(struct gcv_context *context, struct db_i *source, int preserve_global)
{
    struct directory *entry;
    size_t metadata = 0;
    size_t appearance = 0;
    FOR_ALL_DIRECTORY_START(entry, source) {
	int global = BU_STR_EQUAL(entry->d_namep, DB5_GLOBAL_OBJECT_NAME);
	if (global && preserve_global) continue;
	if (!global && (strlen(entry->d_namep) >= NAMESIZE ||
	    (db_version(source) >= 5 && entry->d_major_type != DB5_MAJORTYPE_BRLCAD))) {
	    bu_log("V4 cannot represent database object %s without changing its identity or representation\n", entry->d_namep);
	    return 0;
	}
	if (db_version(source) < 5) continue;
	struct bu_attribute_value_set attributes;
	bu_avs_init_empty(&attributes);
	if (db5_get_attributes(source, &attributes, entry)) {
	    bu_avs_free(&attributes);
	    return 0;
	}
	for (size_t i = 0; i < attributes.count; ++i) {
	    const char *name = attributes.avp[i].name;
	    if (global && (BU_STR_EQUAL(name, "title") || BU_STR_EQUAL(name, "units"))) continue;
	    if (global && BU_STR_EQUAL(name, "regionid_colortable")) {
		if (attributes.avp[i].value[0]) ++appearance;
		continue;
	    }
	    int attribute = db5_standardize_attribute(name);
	    if (!global && (entry->d_flags & RT_DIR_COMB) && v4_comb_attribute(attribute))
		continue;
	    if (attribute == ATTR_COLOR || attribute == ATTR_SHADER || attribute == ATTR_MATERIAL_NAME)
		++appearance;
	    else
		++metadata;
	}
	bu_avs_free(&attributes);
    } FOR_ALL_DIRECTORY_END;
    if (!preserve_global) {
	if (db_version(source) == 4)
	    for (const struct mater *color = db_mater_head(source); color; color = color->mt_forw)
		++appearance;
	if (strlen(source->dbi_title) >= sizeof(((struct ident *)0)->i_title)) ++metadata;
	if (db_v4_get_units_code(bu_units_string(source->dbi_local2base)) < 0) ++metadata;
    }
    int permitted = 1;
    if (metadata)
	permitted &= gcv_report_loss(context, GCV_LOSS_METADATA, "brlcad.v4.metadata", metadata,
	    "V4 cannot preserve custom attributes, long titles or unrecognized display units");
    if (appearance)
	permitted &= gcv_report_loss(context, GCV_LOSS_APPEARANCE, "brlcad.v4.appearance", appearance,
	    "V4 copy does not preserve region color tables or appearance attributes outside combinations");
    return permitted;
}

int
gcv_db_copy(struct gcv_context *context, struct rt_wdb *destination, struct db_i *source)
{
    if (!context || !destination || !source || destination->dbip == source) return 0;
    int input_version = db_version(source);
    int output_version = db_version(destination->dbip);
    if ((input_version != 4 && input_version != 5) || (output_version != 4 && output_version != 5))
	return 0;
    int preserve_global = append_only(destination) &&
	(output_version == 4 || db_lookup(destination->dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET));
    if (output_version == 4 && !v4_preflight(context, source, preserve_global))
	return 0;
    if (!preserve_global && db_update_ident(destination->dbip, source->dbi_title, source->dbi_local2base) < 0)
	return 0;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, source) {
	int global = BU_STR_EQUAL(entry->d_namep, DB5_GLOBAL_OBJECT_NAME);
	if (global && (preserve_global || output_version == 4)) continue;
	if (input_version == output_version) {
	    struct bu_external object = BU_EXTERNAL_INIT_ZERO;
	    int status = db_get_external(&object, entry, source);
	    if (status >= 0)
		status = wdb_export_external(destination, &object, entry->d_namep,
		    entry->d_flags & ~RT_DIR_INMEM, entry->d_minor_type);
	    bu_free_external(&object);
	    if (status < 0) return 0;
	} else {
	    struct rt_db_internal object;
	    RT_DB_INTERNAL_INIT(&object);
	    int type = rt_db_get_internal(&object, entry, source, NULL);
	    if (type < 0) return 0;
	    if (type == ID_POLY && output_version == 5) {
		struct bn_tol tolerance = BN_TOL_INIT_ZERO;
		BN_TOL_INIT(&tolerance);
		tolerance.dist = 0;
		tolerance.dist_sq = 0;
		if (rt_pg_to_bot(&object, &tolerance) < 0) {
		    rt_db_free_internal(&object);
		    return 0;
		}
	    }
	    if (wdb_put_internal(destination, entry->d_namep, &object, 1.0) < 0)
		return 0;
	}
    } FOR_ALL_DIRECTORY_END;
    if (output_version == 5 && !preserve_global) {
	db_mater_free(destination->dbip);
	db_mater_set_head(destination->dbip, db_mater_dup(source));
	if (input_version == 4 && db5_put_color_table(destination->dbip) < 0) return 0;
    }
    return 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
