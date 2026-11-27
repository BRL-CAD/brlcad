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

#include <array>
#include <string>

#include "raytrace.h"
#include "wdb.h"

#include "./ged_facetize.h"

#include "./transfer.h"
#include "./worker.h"

static bool
is_partition(const struct bu_attribute_value_set &attributes)
{
    const char *method = bu_avs_get(&attributes, FACETIZE_METHOD_ATTR);
    return method && BU_STR_EQUAL(method, FACETIZE_PARTITION_METHOD);
}

bool
facetize_partition_result(struct db_i *dbip, const struct directory *dp)
{
    if (!dp || dp->d_major_type != DB5_MAJORTYPE_BRLCAD || dp->d_minor_type != ID_COMBINATION)
	return false;

    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    const bool partition = !db5_get_attributes(dbip, &attributes, dp) && is_partition(attributes);
    bu_avs_free(&attributes);
    return partition;
}

static bool
partition_bot(const struct rt_db_internal &part)
{
    if (part.idb_type != ID_BOT)
	return false;
    const auto *bot = static_cast<const struct rt_bot_internal *>(part.idb_ptr);
    return bot && bot->mode == RT_BOT_SOLID && bot->orientation == RT_BOT_CCW &&
	bot->num_faces && bot->num_vertices;
}

static void
remove_partition_object(struct db_i *dbip, const char *name)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp && (db_delete(dbip, dp) || db_dirdelete(dbip, dp)))
	bu_log("FACETIZE: unable to remove incomplete partition object '%s'\n", name);
}

static int
write_partition(struct db_i *target, const char *name,
	const std::array<struct rt_db_internal *, 2> &parts,
	const struct bu_attribute_value_set &attributes)
{
    struct directory *existing = db_lookup(target, name, LOOKUP_QUIET);
    if (existing && existing->d_minor_type != ID_BREP)
	return BRLCAD_ERROR;
    struct rt_wdb *wdbp = wdb_dbopen(target, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp)
	return BRLCAD_ERROR;

    std::array<std::string, 2> children;
    for (size_t i = 0; i < children.size(); ++i) {
	const std::string base = std::string(name) + (i ? ".partition-b.bot" : ".partition-a.bot");
	children[i] = base;
	for (size_t suffix = 1; db_lookup(target, children[i].c_str(), LOOKUP_QUIET); ++suffix)
	    children[i] = base + "." + std::to_string(suffix);

	bu_avs_remove(&parts[i]->idb_avs, FACETIZE_METHOD_ATTR);
	if (wdb_put_internal(wdbp, children[i].c_str(), parts[i], 1.0) < 0) {
	    for (size_t j = 0; j <= i; ++j)
		remove_partition_object(target, children[j].c_str());
	    return BRLCAD_ERROR;
	}
    }

    struct rt_comb_internal *comb;
    BU_ALLOC(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);
    std::array<struct rt_tree_array, 2> members;
    for (size_t i = 0; i < members.size(); ++i) {
	union tree *leaf;
	BU_GET(leaf, union tree);
	RT_TREE_INIT(leaf);
	leaf->tr_l.tl_op = OP_DB_LEAF;
	leaf->tr_l.tl_name = bu_strdup(children[i].c_str());
	leaf->tr_l.tl_mat = NULL;
	members[i] = {leaf, OP_UNION};
    }
    comb->tree = db_mkbool_tree(members.data(), members.size());
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_meth = &OBJ[ID_COMBINATION];
    intern.idb_ptr = comb;
    bu_avs_init_empty(&intern.idb_avs);
    if (attributes.magic == BU_AVS_MAGIC)
	bu_avs_merge(&intern.idb_avs, &attributes);
    bu_avs_add(&intern.idb_avs, FACETIZE_METHOD_ATTR, FACETIZE_PARTITION_METHOD);
    /* Keep the original record available until both child writes and the
     * parent replacement have completed. The direct command can write to
     * the live database, outside the worker's checkpoint. */
    struct bu_external original = BU_EXTERNAL_INIT_ZERO;
    const int flags = existing ? existing->d_flags : 0;
    const int minor_type = existing ? existing->d_minor_type : ID_NULL;
    if (existing && db_get_external(&original, existing, target) < 0) {
	rt_db_free_internal(&intern);
	for (const std::string &child : children)
	    remove_partition_object(target, child.c_str());
	return BRLCAD_ERROR;
    }
    const int written = wdb_put_internal(wdbp, name, &intern, 1.0);
    bool restored = true;
    if (written < 0) {
	if (!existing)
	    remove_partition_object(target, name);
	else
	    restored = wdb_export_external(wdbp, &original, name, flags, minor_type) == 0;
	if (restored) {
	    for (const std::string &child : children)
		remove_partition_object(target, child.c_str());
	} else {
	    bu_log("FACETIZE: unable to restore original partition source '%s'\n", name);
	}
    }
    bu_free_external(&original);
    return written == 0 ? BRLCAD_OK : BRLCAD_ERROR;
}

static int
transfer_partition(struct db_i *target, struct db_i *staged, const char *name,
	struct rt_db_internal &first)
{
    struct directory *original = db_lookup(target, name, LOOKUP_QUIET);
    struct directory *extra = db_lookup(staged, FACETIZE_WORKER_PART_OBJECT, LOOKUP_QUIET);
    struct rt_db_internal second;
    RT_DB_INTERNAL_INIT(&second);
    if (!original || original->d_minor_type != ID_BREP || !partition_bot(first) ||
	    !extra || extra->d_major_type != DB5_MAJORTYPE_BRLCAD ||
	    extra->d_minor_type != ID_BOT ||
	    rt_db_get_internal(&second, extra, staged, NULL) < 0 ||
	    !partition_bot(second) || !is_partition(second.idb_avs)) {
	rt_db_free_internal(&second);
	return BRLCAD_ERROR;
    }

    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    const int status = db5_get_attributes(target, &attributes, original) < 0 ?
	BRLCAD_ERROR : write_partition(target, name, {&first, &second}, attributes);
    bu_avs_free(&attributes);
    rt_db_free_internal(&second);
    return status;
}

int
facetize_copy_partition(struct db_i *target, const char *name,
	struct db_i *source, struct directory *partition)
{
    struct rt_db_internal root;
    RT_DB_INTERNAL_INIT(&root);
    if (rt_db_get_internal(&root, partition, source, NULL) != ID_COMBINATION ||
	    !is_partition(root.idb_avs)) {
	rt_db_free_internal(&root);
	return BRLCAD_ERROR;
    }

    auto *comb = static_cast<const struct rt_comb_internal *>(root.idb_ptr);
    const union tree *tree = comb->tree;
    if (comb->region_flag || !tree || tree->tr_op != OP_UNION) {
	rt_db_free_internal(&root);
	return BRLCAD_ERROR;
    }
    std::array<struct rt_db_internal, 2> parts;
    for (auto &part : parts)
	RT_DB_INTERNAL_INIT(&part);
    bool valid = true;
    size_t i = 0;
    for (const union tree *leaf : {tree->tr_b.tb_left, tree->tr_b.tb_right}) {
	struct directory *dp = leaf && leaf->tr_op == OP_DB_LEAF && !leaf->tr_l.tl_mat ?
	    db_lookup(source, leaf->tr_l.tl_name, LOOKUP_QUIET) : nullptr;
	if (!dp || dp->d_minor_type != ID_BOT ||
		rt_db_get_internal(&parts[i], dp, source, NULL) != ID_BOT ||
		!partition_bot(parts[i])) {
	    valid = false;
	    break;
	}
	++i;
    }
    const int status = valid ?
	write_partition(target, name, {&parts.front(), &parts.back()}, root.idb_avs) :
	BRLCAD_ERROR;
    for (auto &part : parts)
	rt_db_free_internal(&part);
    rt_db_free_internal(&root);
    return status;
}

int
facetize_transfer_staged_object(struct db_i *target_dbip,
	const char *result_file, const char *object_name,
	int expected_object_type, bool allow_partition)
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
    if (expected_object_type == ID_BOT && is_partition(intern.idb_avs)) {
	const int status = allow_partition ?
	    transfer_partition(target_dbip, result_dbip, object_name, intern) : BRLCAD_ERROR;
	rt_db_free_internal(&intern);
	db_close(result_dbip);
	return status;
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
