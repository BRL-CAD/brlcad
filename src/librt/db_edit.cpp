/*                     D B _ E D I T . C P P
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

#include <cstring>
#include <map>
#include <set>
#include <string>

#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"
#include "librt_private.h"

struct db_edit {
    struct db_i *base = nullptr;
    struct db_i *view = nullptr;
    std::map<const struct directory *, const struct directory *> inherited;
    std::set<std::string> touched;
    std::string label;
    bool failed = false;
    bool active = true;
};


struct db_edit *
db_edit_begin(struct db_i *base, const char *label)
{
    if (!base || !base->dbi_read_only || db_version(base) != 5 ||
	(base->i->dbi_edit && base->i->dbi_edit->active) ||
	(!base->i->dbi_dir_built && (base->i->dbi_fp || base->i->dbi_mf)))
	return nullptr;

    const unsigned int flags = db_uses_instance_specifiers(base) ?
	DB_OPEN_INMEM_COMB_INSTANCES : DB_OPEN_INMEM_DEFAULT;
    struct db_i *view = db_open_inmem_ex(flags);
    auto *edit = new db_edit;
    edit->view = view;
    edit->base = db_clone_dbi(base, nullptr);
    edit->label = label ? label : "";
    view->dbi_local2base = base->dbi_local2base;
    view->dbi_base2local = base->dbi_base2local;
    bu_free(view->dbi_title, "staged database title");
    view->dbi_title = bu_strdup(base->dbi_title);
    if (base->dbi_filepath) {
	const char *paths[] = {base->dbi_filepath[0], base->dbi_filepath[1]};
	view->dbi_filepath = bu_argv_dup(sizeof(paths) / sizeof(paths[0]), paths);
    }
    db_mater_set_head(view, db_mater_dup(base));

    // The native directory supports lookup and iteration in the view.
    // Unchanged serialized records are fetched from the retained base.
    // Offset zero is the DB5 header and never an object location.
    constexpr b_off_t inherited_record = 0;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, base) {
	unsigned char minor_type = entry->d_minor_type;
	const bool has_record = entry->d_len && entry->d_addr != RT_DIR_PHONY_ADDR;
	struct directory *copy = db_diradd(view, entry->d_namep,
	    has_record ? inherited_record : RT_DIR_PHONY_ADDR,
	    entry->d_len, entry->d_flags & ~RT_DIR_USED, &minor_type);
	if (!copy) {
	    db_close(view);
	    db_close(edit->base);
	    delete edit;
	    return nullptr;
	}
	copy->d_major_type = entry->d_major_type;
	copy->d_nref = entry->d_nref;
	if (has_record)
	    edit->inherited.emplace(copy, entry);
    } FOR_ALL_DIRECTORY_END;

    view->i->dbi_dir_built = 1;
    view->i->dbi_edit = edit;
    return edit;
}


struct db_i *
db_edit_view(struct db_edit *edit)
{
    return edit && edit->active ? edit->view : nullptr;
}

const char *
db_edit_label(const struct db_edit *edit)
{
    return edit && edit->active ? edit->label.c_str() : nullptr;
}


int
db_edit_read_base(const struct db_i *dbip, const struct directory *dp,
		  void *buffer, size_t count, int *found)
{
    *found = 0;
    const auto *edit = dbip->i->dbi_edit;
    if (!edit)
	return 0;
    const auto inherited = edit->inherited.find(dp);
    if (inherited == edit->inherited.end())
	return 0;
    *found = 1;
    return db_read_external_bytes(edit->base, inherited->second, buffer, count);
}


int
db_edit_detach_record(struct db_i *dbip, struct directory *dp)
{
    auto *edit = dbip->i->dbi_edit;
    if (!edit || !edit->inherited.erase(dp))
	return 0;

    dp->d_addr = RT_DIR_PHONY_ADDR;
    dp->d_len = 0;
    return 1;
}


void
db_edit_record_failure(struct db_i *dbip)
{
    auto *edit = dbip->i->dbi_edit;
    if (edit && edit->active)
	edit->failed = true;
}


void
db_edit_record_change(struct db_i *dbip, const char *name)
{
    auto *edit = dbip->i->dbi_edit;
    if (edit && edit->active)
	edit->touched.emplace(name);
}

static struct directory *
db_edit_lookup_name(const struct db_i *dbip, const char *name)
{
    auto *dp = dbip->i->dbi_Head[db_dirhash(name)];
    for (; dp; dp = dp->d_forw) {
	if (BU_STR_EQUAL(name, dp->d_namep))
	    return dp;
    }
    return nullptr;
}

static bool
db_edit_equal(const struct bu_external *a, const struct bu_external *b)
{
    if (!a || !b)
	return a == b;
    return a->ext_nbytes == b->ext_nbytes &&
	memcmp(a->ext_buf, b->ext_buf, a->ext_nbytes) == 0;
}


int
db_edit_visit(struct db_edit *edit, db_edit_visit_t visit, void *data)
{
    if (!edit || !edit->active || edit->failed || !visit)
	return -1;

    for (const auto &name : edit->touched) {
	const auto *before = db_edit_lookup_name(edit->base, name.c_str());
	const auto *after = db_edit_lookup_name(edit->view, name.c_str());
	if (!before && !after)
	    continue;

	struct bu_external old_record = BU_EXTERNAL_INIT_ZERO;
	struct bu_external new_record = BU_EXTERNAL_INIT_ZERO;
	if ((before && db_get_external(&old_record, before, edit->base) < 0) ||
	    (after && db_get_external(&new_record, after, edit->view) < 0)) {
	    bu_free_external(&old_record);
	    bu_free_external(&new_record);
	    edit->failed = true;
	    return -1;
	}

	const auto *previous = before ? &old_record : nullptr;
	const auto *current = after ? &new_record : nullptr;
	int result = 0;
	if (!db_edit_equal(previous, current))
	    result = visit(name.c_str(), previous, current, data);
	bu_free_external(&old_record);
	bu_free_external(&new_record);
	if (result)
	    return result;
    }
    return 0;
}


static bool
db_edit_decode(struct db5_raw_internal &raw, const char *name,
	       const struct bu_external *before, const struct bu_external *after)
{
    if (!after)
	return !BU_STR_EQUAL(name, DB5_GLOBAL_OBJECT_NAME);

    const auto *end = db5_get_raw_internal_bounded(&raw, after->ext_buf, after->ext_nbytes);
    if (!end || raw.object_length != after->ext_nbytes || !raw.h_name_present ||
	raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT ||
	!BU_STR_EQUAL(name, reinterpret_cast<const char *>(raw.name.ext_buf)))
	return false;
    if (BU_STR_EQUAL(name, DB5_GLOBAL_OBJECT_NAME) &&
	(!before || raw.major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY))
	return false;
    return true;
}


static int
db_edit_validate_record(const char *name, const struct bu_external *before,
			const struct bu_external *after, void *UNUSED(data))
{
    struct db5_raw_internal raw;
    return db_edit_decode(raw, name, before, after) ? 0 : -1;
}


int
db_edit_apply(struct db_edit *edit, const char *name, const struct bu_external *expected,
	      const struct bu_external *replacement)
{
    if (!edit || !edit->active || edit->failed)
	return -1;

    auto *view = edit->view;
    struct db5_raw_internal raw;
    if (!name || !*name || view->dbi_read_only ||
	!db_edit_decode(raw, name, expected, replacement)) {
	edit->failed = true;
	return -1;
    }

    auto *dp = db_edit_lookup_name(view, name);
    struct bu_external current = BU_EXTERNAL_INIT_ZERO;
    const bool matches = (!dp || db_get_external(&current, dp, view) == 0) &&
	db_edit_equal(dp ? &current : nullptr, expected);
    bu_free_external(&current);
    if (!matches) {
	edit->failed = true;
	return -1;
    }
    if (db_edit_equal(expected, replacement))
	return 0;

    if (dp && db_dirdelete(view, dp) != 0)
	return -1;
    if (replacement) {
	auto *bytes = bu_malloc(replacement->ext_nbytes, "staged DB5 record");
	memcpy(bytes, replacement->ext_buf, replacement->ext_nbytes);
	int in_memory = 1;
	auto *created = db5_diradd(view, &raw, (b_off_t)(intptr_t)bytes, &in_memory);
	if (!created) {
	    bu_free(bytes, "staged DB5 record");
	    edit->failed = true;
	    return -1;
	}
	if (BU_STR_EQUAL(name, DB5_GLOBAL_OBJECT_NAME) &&
	    db5_load_global(view, created) != 0) {
	    edit->failed = true;
	    return -1;
	}
    }
    return 0;
}


struct db_i *
db_edit_commit(struct db_edit *edit)
{
    if (!edit || !edit->active || edit->view->dbi_read_only ||
	db_edit_visit(edit, db_edit_validate_record, nullptr) != 0)
	return nullptr;

    if (edit->touched.count(DB5_GLOBAL_OBJECT_NAME) &&
	db5_load_global(edit->view, db_edit_lookup_name(edit->view, DB5_GLOBAL_OBJECT_NAME)) != 0)
	return nullptr;

    if (!edit->touched.empty())
	db_update_nref(edit->view);
    edit->active = false;
    edit->touched.clear();
    edit->view->dbi_read_only = 1;
    return edit->view;
}


void
db_edit_abort(struct db_edit *edit)
{
    if (edit && edit->active) {
	edit->active = false;
	edit->view->dbi_read_only = 1;
	db_close(edit->view);
    }
}


void
db_edit_free_view(struct db_i_internal *i)
{
    auto *edit = i->dbi_edit;
    if (!edit)
	return;

    db_close(edit->base);
    delete edit;
    i->dbi_edit = nullptr;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
