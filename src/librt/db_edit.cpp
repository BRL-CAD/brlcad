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

static const struct directory *
db_edit_lookup_name(const struct db_i *dbip, const std::string &name)
{
    const auto *dp = dbip->i->dbi_Head[db_dirhash(name.c_str())];
    for (; dp; dp = dp->d_forw) {
	if (name == dp->d_namep)
	    return dp;
    }
    return nullptr;
}


int
db_edit_visit(struct db_edit *edit, db_edit_visit_t visit, void *data)
{
    if (!edit || !edit->active || edit->failed || !visit)
	return -1;

    for (const auto &name : edit->touched) {
	const auto *before = db_edit_lookup_name(edit->base, name);
	const auto *after = db_edit_lookup_name(edit->view, name);
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

	const bool equal = before && after && old_record.ext_nbytes == new_record.ext_nbytes &&
	    memcmp(old_record.ext_buf, new_record.ext_buf, old_record.ext_nbytes) == 0;
	int result = 0;
	if (!equal)
	    result = visit(name.c_str(), before ? &old_record : nullptr,
		after ? &new_record : nullptr, data);
	bu_free_external(&old_record);
	bu_free_external(&new_record);
	if (result)
	    return result;
    }
    return 0;
}


static int
db_edit_validate_record(const char *name, const struct bu_external *before,
			const struct bu_external *after, void *UNUSED(data))
{
    if (!after)
	return BU_STR_EQUAL(name, DB5_GLOBAL_OBJECT_NAME) ? -1 : 0;

    struct db5_raw_internal raw;
    const auto *end = db5_get_raw_internal_bounded(&raw, after->ext_buf, after->ext_nbytes);
    if (!end || raw.object_length != after->ext_nbytes || !raw.h_name_present ||
	raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT ||
	!BU_STR_EQUAL(name, reinterpret_cast<const char *>(raw.name.ext_buf)))
	return -1;
    if (BU_STR_EQUAL(name, DB5_GLOBAL_OBJECT_NAME) &&
	(!before || raw.major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY))
	return -1;
    return 0;
}


struct db_i *
db_edit_commit(struct db_edit *edit)
{
    if (!edit || !edit->active || edit->view->dbi_read_only ||
	db_edit_visit(edit, db_edit_validate_record, nullptr) != 0)
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
