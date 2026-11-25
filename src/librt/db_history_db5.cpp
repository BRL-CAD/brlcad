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

} // namespace

static void
seed_artifact(Db5HistorySeed &seed, const ArtifactKey &key, Bytes &&payload)
{
    const auto &stored = seed.owned.emplace(key, std::move(payload)).first->second;
    seed.pack.artifacts.emplace(key, stored);
}

bool
seed_db5_history(const struct db_i *db, std::string_view label, int64_t time, Db5HistorySeed &result)
{
    if (!db || db_version(db) != 5 || !db->i->dbi_dir_built || db_lookup(db, archive_name, LOOKUP_QUIET))
	return false;

    Db5HistorySeed seed;
    Manifest manifest;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, db) {
	if (!(dp->d_flags & RT_DIR_INMEM) && dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;
	const DatabaseRecord record(db, dp);
	RecordArtifact artifact;
	if (!factor_record(record.bytes, artifact) || artifact.name != dp->d_namep ||
	    !manifest.emplace(artifact.name, artifact.object_id).second)
	    return false;
	seed_artifact(seed, {Kind::Object, artifact.object_id}, std::move(artifact.object));
	const ArtifactKey body_key{Kind::Body, artifact.body_id};
	if (artifact.body_id == absent || seed.pack.artifacts.count(body_key))
	    continue;
	if (record.mapped()) {
	    if (!seed.backing)
		seed.backing = std::shared_ptr<struct db_i>(db_clone_dbi(const_cast<struct db_i *>(db), nullptr), db_close);
	    seed.pack.artifacts.emplace(body_key, artifact.body);
	} else {
	    seed_artifact(seed, body_key, Bytes(artifact.body.data, artifact.body.data + artifact.body.size));
	}
    } FOR_ALL_DIRECTORY_END;

    Bytes model, revision;
    if (!manifest.count(DB5_GLOBAL_OBJECT_NAME) || !encode_manifest(manifest, model))
	return false;
    Revision root;
    root.model = identify(Kind::Manifest, model);
    root.label = label;
    root.time = time;
    if (!encode_revision(root, revision))
	return false;
    seed.pack.selected_model = root.model;
    seed.pack.selected_revision = identify(Kind::Revision, revision);
    if (seed.pack.selected_revision == absent)
	return false;
    seed_artifact(seed, {Kind::Manifest, root.model}, std::move(model));
    seed_artifact(seed, {Kind::Revision, seed.pack.selected_revision}, std::move(revision));
    result = std::move(seed);
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
