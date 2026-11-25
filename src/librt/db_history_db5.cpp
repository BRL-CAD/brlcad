/*                  D B _ H I S T O R Y _ D B 5 . C P P
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

#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "db_history_db5.h"
#include "librt_private.h"

namespace db_history {
namespace {

class DatabaseRecord {
public:
    DatabaseRecord(const struct db_i *db, const struct directory *dp)
    {
	if (db->i->dbi_mf && !(dp->d_flags & RT_DIR_INMEM) && dp->d_addr >= 0) {
	    const auto *mapping = db->i->dbi_mf;
	    const auto offset = static_cast<uint64_t>(dp->d_addr);
	    if (offset <= mapping->buflen && dp->d_len <= mapping->buflen - offset)
		bytes = {static_cast<const uint8_t *>(mapping->buf) + offset, dp->d_len};
	    return;
	}
	if (db_get_external(&external, dp, db) == 0)
	    bytes = {external.ext_buf, external.ext_nbytes};
    }

    DatabaseRecord(const DatabaseRecord &) = delete;
    DatabaseRecord &operator=(const DatabaseRecord &) = delete;

    ~DatabaseRecord() { bu_free_external(&external); }

    bool mapped() const { return bytes.size && !external.ext_buf; }

    std::shared_ptr<const uint8_t> retain(const struct db_i *db)
    {
	if (external.ext_buf) {
	    const auto *owned = external.ext_buf;
	    external.ext_buf = nullptr;
	    external.ext_nbytes = 0;
	    return std::shared_ptr<const uint8_t>(owned, [](const uint8_t *p) {
		bu_free(const_cast<uint8_t *>(p), "db_get_ext ext_buf");
	    });
	}
	// Mapped record addresses are valid while any reader retains db.
	auto *retained = db_clone_dbi(const_cast<struct db_i *>(db), nullptr);
	return std::shared_ptr<const uint8_t>(bytes.data, [retained](const uint8_t *) { db_close(retained); });
    }

    ByteView bytes;

private:
    struct bu_external external = BU_EXTERNAL_INIT_ZERO;
};

bool
live_manifest(const struct db_i *db, const struct directory *archive, Manifest &result)
{
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, db) {
	if (entry == archive || (!(entry->d_flags & RT_DIR_INMEM) && entry->d_addr == RT_DIR_PHONY_ADDR))
	    continue;
	const DatabaseRecord record(db, entry);
	if (!add_model_record(result, entry->d_namep, record.bytes))
	    return false;
    } FOR_ALL_DIRECTORY_END;
    return true;
}

void
own_artifact(Db5HistoryDraft &draft, const ArtifactKey &key, Bytes &&payload)
{
    if (draft.pack.artifacts.count(key))
	return;
    const auto &stored = draft.owned.emplace(key, std::move(payload)).first->second;
    draft.pack.artifacts.emplace(key, stored);
}

bool
record_revision(Db5HistoryDraft &draft, const Manifest &manifest, Revision &revision)
{
    Bytes model, encoded_revision;
    if (!manifest.count(DB5_GLOBAL_OBJECT_NAME) || !encode_manifest(manifest, model))
	return false;
    revision.model = identify(Kind::Manifest, model);
    if (!encode_revision(revision, encoded_revision))
	return false;
    draft.pack.selected_model = revision.model;
    draft.pack.selected_revision = identify(Kind::Revision, encoded_revision);
    if (draft.pack.selected_revision == absent)
	return false;
    own_artifact(draft, {Kind::Manifest, revision.model}, std::move(model));
    own_artifact(draft, {Kind::Revision, draft.pack.selected_revision}, std::move(encoded_revision));
    return true;
}

struct CollectRevision {
    Db5HistoryDraft &draft;
    Manifest &manifest;
    Revision &revision;
};

int
collect_revision_change(const char *name, const struct bu_external *before,
			const struct bu_external *after, void *data)
{
    auto &collect = *static_cast<CollectRevision *>(data);
    auto &manifest = collect.manifest;
    const auto expected = manifest.find(name);
    if (BU_STR_EQUAL(name, archive_name) || static_cast<bool>(before) != (expected != manifest.end()))
	return -1;

    Change change;
    if (before) {
	RecordArtifact object;
	if (!factor_record({before->ext_buf, before->ext_nbytes}, object) ||
	    object.name != name || object.object_id != expected->second)
	    return -1;
	change.before = object.object_id;
    }
    if (after) {
	RecordArtifact object;
	if (!factor_record({after->ext_buf, after->ext_nbytes}, object) || object.name != name)
	    return -1;
	change.after = object.object_id;
	auto &draft = collect.draft;
	own_artifact(draft, {Kind::Object, object.object_id}, std::move(object.object));
	const ArtifactKey body_key{Kind::Body, object.body_id};
	if (object.body_id != absent && !draft.pack.artifacts.count(body_key))
	    own_artifact(draft, body_key, Bytes(object.body.data, object.body.data + object.body.size));
	manifest.insert_or_assign(name, change.after);
    } else {
	manifest.erase(expected);
    }
    collect.revision.changes.emplace(name, change);
    return 0;
}

bool
checkout_record(const Pack &pack, const Id &id, Bytes &record)
{
    if (id == absent)
	return true;
    ByteView descriptor, body;
    Object object;
    return read_artifact(pack, {Kind::Object, id}, descriptor) && decode_object(descriptor, object) &&
	(object.body_id == absent || read_artifact(pack, {Kind::Body, object.body_id}, body)) &&
	materialize_record(object, body, record);
}

bool
checkout_change(const Pack &pack, struct db_edit *edit, const std::string &name, const Id &from, const Id &to)
{
    if (from == to)
	return true;
    Bytes before, after;
    if (!checkout_record(pack, from, before) || !checkout_record(pack, to, after))
	return false;
    struct bu_external before_ext = BU_EXTERNAL_INIT_ZERO;
    struct bu_external after_ext = BU_EXTERNAL_INIT_ZERO;
    before_ext.ext_buf = before.data();
    before_ext.ext_nbytes = before.size();
    after_ext.ext_buf = after.data();
    after_ext.ext_nbytes = after.size();
    return db_edit_apply(edit, name.c_str(), before.empty() ? nullptr : &before_ext,
	after.empty() ? nullptr : &after_ext) == 0;
}

} // namespace

bool
seed_db5_history(const struct db_i *db, std::string_view label, int64_t time, Db5HistoryDraft &result)
{
    if (!db || db_version(db) != 5 || !db->i->dbi_dir_built || db_lookup(db, archive_name, LOOKUP_QUIET))
	return false;

    Db5HistoryDraft seed;
    Manifest manifest;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, db) {
	if (!(dp->d_flags & RT_DIR_INMEM) && dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	DatabaseRecord record(db, dp);
	RecordArtifact artifact;
	if (!factor_record(record.bytes, artifact) || artifact.name != dp->d_namep ||
	    !manifest.emplace(artifact.name, artifact.object_id).second)
	    return false;
	own_artifact(seed, {Kind::Object, artifact.object_id}, std::move(artifact.object));
	const ArtifactKey body_key{Kind::Body, artifact.body_id};
	if (artifact.body_id == absent || seed.pack.artifacts.count(body_key))
	    continue;
	if (record.mapped()) {
	    if (!seed.backing)
		seed.backing = record.retain(db);
	    seed.pack.artifacts.emplace(body_key, artifact.body);
	} else {
	    own_artifact(seed, body_key, Bytes(artifact.body.data, artifact.body.data + artifact.body.size));
	}
    } FOR_ALL_DIRECTORY_END;

    Revision root;
    root.label = label;
    root.time = time;
    if (!record_revision(seed, manifest, root))
	return false;
    result = std::move(seed);
    return true;
}

AppendStatus
append_db5_history(const EmbeddedPack &archive, struct db_edit *edit, int64_t time, Db5HistoryDraft &result)
{
    const char *label = db_edit_label(edit);
    if (!label || archive.pack.selected_revision == absent)
	return AppendStatus::Error;

    Db5HistoryDraft draft;
    draft.backing = archive.backing;
    draft.pack = archive.pack;
    Manifest manifest = archive.model;
    Revision revision;
    revision.parent = archive.pack.selected_revision;
    revision.label = label;
    revision.time = time;
    CollectRevision collect{draft, manifest, revision};
    if (db_edit_visit(edit, collect_revision_change, &collect) != 0)
	return AppendStatus::Error;
    if (revision.changes.empty())
	return AppendStatus::Unchanged;
    if (!record_revision(draft, manifest, revision))
	return AppendStatus::Error;
    result = std::move(draft);
    return AppendStatus::Ready;
}

bool
stage_db5_checkout(const EmbeddedPack &archive, const Id &target, struct db_edit *edit, Db5HistoryDraft &result)
{
    Revision current, revision;
    Manifest model;
    if (!db_edit_view(edit) || !read_revision(archive.pack, archive.pack.selected_revision, current) ||
	!read_revision(archive.pack, target, revision) || !manifest_at_revision(archive.pack, target, model) ||
	!model.count(DB5_GLOBAL_OBJECT_NAME) || model.count(archive_name))
	return false;

    for (const auto &entry : archive.model) {
	const auto updated = model.find(entry.first);
	if (!checkout_change(archive.pack, edit, entry.first, entry.second,
		updated != model.end() ? updated->second : absent))
	    return false;
    }
    for (const auto &entry : model)
	if (!archive.model.count(entry.first) && !checkout_change(archive.pack, edit, entry.first, absent, entry.second))
	    return false;

    Db5HistoryDraft draft;
    draft.backing = archive.backing;
    draft.pack = archive.pack;
    draft.pack.selected_revision = target;
    draft.pack.selected_model = revision.model;
    const ArtifactKey model_key{Kind::Manifest, revision.model};
    if (!draft.pack.artifacts.count(model_key)) {
	Bytes stored;
	if (!encode_manifest(model, stored) || identify(Kind::Manifest, stored) != revision.model)
	    return false;
	own_artifact(draft, model_key, std::move(stored));
    }
    if (target == current.parent)
	draft.pack.preferences.insert_or_assign(target, archive.pack.selected_revision);
    else if (archive.pack.selected_revision == revision.parent)
	draft.pack.preferences.insert_or_assign(revision.parent, target);
    result = std::move(draft);
    return true;
}

bool
add_model_record(Manifest &manifest, std::string_view name, ByteView record)
{
    RecordArtifact artifact;
    return factor_record(record, artifact) && artifact.name == name &&
	manifest.emplace(name, artifact.object_id).second;
}

bool
selected_manifest(const Pack &pack, Manifest &manifest)
{
    Revision selected;
    Manifest model;
    if (!read_revision(pack, pack.selected_revision, selected) || selected.model != pack.selected_model ||
	!read_manifest(pack, selected.model, model) || !model.count(DB5_GLOBAL_OBJECT_NAME) || model.count(archive_name))
	return false;

    for (const auto &entry : model) {
	ByteView payload;
	Object object;
	if (!read_artifact(pack, {Kind::Object, entry.second}, payload) || !decode_object(payload, object) ||
	    (object.body_id != absent && !pack.artifacts.count({Kind::Body, object.body_id})))
	    return false;
    }

    manifest = std::move(model);
    return true;
}

static EmbeddedStatus
read_db5_index(const struct db_i *db, const struct directory *dp, EmbeddedPack *result)
{
    if (!dp)
	return EmbeddedStatus::Missing;
    if (!(dp->d_flags & RT_DIR_HIDDEN) || dp->d_major_type != DB5_MAJORTYPE_BINARY_UNIF ||
	dp->d_minor_type != DB5_MINORTYPE_BINU_8BITINT_U)
	return EmbeddedStatus::Collision;

    DatabaseRecord record(db, dp);
    struct db5_raw_internal raw;
    if (!db5_get_raw_internal_bounded(&raw, record.bytes.data, record.bytes.size) ||
	raw.object_length != record.bytes.size)
	return EmbeddedStatus::Malformed;
    ByteView body;
    if (!archive_payload(raw, body))
	return EmbeddedStatus::Collision;

    Pack pack;
    switch (read_pack(body, pack)) {
	case PackStatus::Ok:
	    if (result) {
		result->backing = record.retain(db);
		result->pack = std::move(pack);
	    }
	    return EmbeddedStatus::Current;
	case PackStatus::Malformed:
	    return EmbeddedStatus::Malformed;
	case PackStatus::Unsupported:
	    return EmbeddedStatus::Unsupported;
    }
    return EmbeddedStatus::Malformed;
}

EmbeddedStatus
inspect_db5_history(const struct db_i *db)
{
    return read_db5_index(db, db_lookup(db, archive_name, LOOKUP_QUIET), nullptr);
}

EmbeddedStatus
read_db5_history(const struct db_i *db, EmbeddedPack &result)
{
    if (!db || db_version(db) != 5 || !db->i->dbi_dir_built)
	return EmbeddedStatus::Missing;

    const auto *dp = db_lookup(db, archive_name, LOOKUP_QUIET);
    EmbeddedPack archive;
    const auto status = read_db5_index(db, dp, &archive);
    if (status != EmbeddedStatus::Current)
	return status;

    Manifest current;
    if (!selected_manifest(archive.pack, archive.model) || !live_manifest(db, dp, current))
	return EmbeddedStatus::Malformed;

    const bool matches = current == archive.model;
    result = std::move(archive);
    return matches ? EmbeddedStatus::Current : EmbeddedStatus::ExternalEdits;
}

} // namespace db_history

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
