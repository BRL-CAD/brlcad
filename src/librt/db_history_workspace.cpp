/*           D B _ H I S T O R Y _ W O R K S P A C E . C P P
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

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <shared_mutex>

#include "lmdb.h"

#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "../libbu/picohash.h"
#include "db_history_db5.h"
#include "db_history_workspace.h"
#include "librt_private.h"

struct db_workspace_view {
    std::unique_ptr<db_history::Workspace> workspace;
    db_history::Manifest model;
    db_history::WorkspaceHead head;
};

namespace db_history {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view bootstrap_key = "workspace";
constexpr std::string_view head_key = "head";
constexpr std::string_view export_key = "last_export";
constexpr std::array<uint8_t, 8> signature{'B', 'R', 'L', 'C', 'A', 'D', 'W', 0};
constexpr uint16_t schema_major = 1;
constexpr uint16_t schema_minor = 0;
constexpr uint64_t required_features = 0;
constexpr uint16_t artifact_encoding = 1;
constexpr size_t bootstrap_size = signature.size() + 3 * sizeof(uint16_t) + sizeof(uint64_t);
constexpr size_t head_size = 2 * absent.size() + sizeof(uint64_t);
constexpr uint64_t initial_generation = 1;
constexpr size_t initial_map_size = 16 * 1024 * 1024;
constexpr unsigned int database_count = 5;

using Environment = std::unique_ptr<MDB_env, decltype(&mdb_env_close)>;
using Transaction = std::unique_ptr<MDB_txn, decltype(&mdb_txn_abort)>;
using Cursor = std::unique_ptr<MDB_cursor, decltype(&mdb_cursor_close)>;

struct Tables {
    MDB_dbi metadata = 0;
    MDB_dbi artifacts = 0;
    MDB_dbi directory = 0;
    MDB_dbi references = 0;
    MDB_dbi preferences = 0;
    MDB_dbi boundaries = 0;
};

enum class ChangeStatus { Error, Unchanged, Changed };

struct LmdbError {
    int code;
};

void
check(int status)
{
    if (status != MDB_SUCCESS)
	throw LmdbError{status};
}

bool
report(struct bu_vls *message, const char *reason)
{
    if (message)
	bu_vls_strcpy(message, reason);
    return false;
}

bool
report(struct bu_vls *message, const LmdbError &error)
{
    return report(message, mdb_strerror(error.code));
}

ChangeStatus
fail_change(struct bu_vls *message, const char *reason)
{
    report(message, reason);
    return ChangeStatus::Error;
}

ByteView
bytes(std::string_view value)
{
    return {reinterpret_cast<const uint8_t *>(value.data()), value.size()};
}

template <size_t Size>
ByteView
bytes(const std::array<uint8_t, Size> &value)
{
    return {value.data(), value.size()};
}

MDB_val
value(ByteView data)
{
    return {data.size, const_cast<uint8_t *>(data.data)};
}

ByteView
view(const MDB_val &entry)
{
    return {static_cast<const uint8_t *>(entry.mv_data), entry.mv_size};
}

bool
get(MDB_txn *txn, MDB_dbi table, ByteView key, ByteView &result)
{
    auto search = value(key);
    MDB_val found;
    const int status = mdb_get(txn, table, &search, &found);
    if (status == MDB_NOTFOUND)
	return false;
    check(status);
    result = view(found);
    return true;
}

void
put(MDB_txn *txn, MDB_dbi table, ByteView key, ByteView payload, unsigned int flags = MDB_NOOVERWRITE)
{
    auto search = value(key);
    auto stored = value(payload);
    check(mdb_put(txn, table, &search, &stored, flags));
}

void
erase(MDB_txn *txn, MDB_dbi table, ByteView key)
{
    auto search = value(key);
    check(mdb_del(txn, table, &search, nullptr));
}

Transaction
begin(MDB_env *env, unsigned int flags)
{
    MDB_txn *txn = nullptr;
    const int status = mdb_txn_begin(env, nullptr, flags, &txn);
    if (status == MDB_MAP_RESIZED) {
	// The new environment is still private to the opener.
	check(mdb_env_set_mapsize(env, 0));
	check(mdb_txn_begin(env, nullptr, flags, &txn));
    } else {
	check(status);
    }
    return Transaction(txn, mdb_txn_abort);
}

void
open_tables(MDB_txn *txn, Tables &tables, unsigned int flags)
{
    check(mdb_dbi_open(txn, "artifacts", flags, &tables.artifacts));
    check(mdb_dbi_open(txn, "directory", flags, &tables.directory));
    check(mdb_dbi_open(txn, "references", flags, &tables.references));
    check(mdb_dbi_open(txn, "preferences", flags, &tables.preferences));
    check(mdb_dbi_open(txn, "boundaries", flags, &tables.boundaries));
}

Environment
open_environment(const char *path, unsigned int flags, size_t map_size = 0)
{
    MDB_env *raw = nullptr;
    check(mdb_env_create(&raw));
    Environment env(raw, mdb_env_close);
    check(mdb_env_set_maxdbs(raw, database_count));
    if (map_size)
	check(mdb_env_set_mapsize(raw, map_size));
    check(mdb_env_open(raw, path, flags, 0660));
    return env;
}

void
grow_map(MDB_env *env)
{
    MDB_envinfo info;
    check(mdb_env_info(env, &info));
    if (info.me_mapsize > std::numeric_limits<size_t>::max() / 2)
	throw LmdbError{MDB_MAP_FULL};
    check(mdb_env_set_mapsize(env, info.me_mapsize * 2));
}

template <typename Integer>
uint8_t *
write_integer(uint8_t *target, Integer number)
{
    for (size_t i = sizeof(number); i; --i) {
	target[i - 1] = static_cast<uint8_t>(number);
	number >>= 8;
    }
    return target + sizeof(number);
}

template <typename Integer>
const uint8_t *
read_integer(const uint8_t *source, Integer &number)
{
    number = 0;
    for (size_t i = 0; i < sizeof(number); ++i)
	number = static_cast<Integer>((number << 8) | source[i]);
    return source + sizeof(number);
}

bool
check_bootstrap(MDB_txn *txn, MDB_dbi table, struct bu_vls *message)
{
    ByteView format;
    if (!get(txn, table, bytes(bootstrap_key), format) || format.size < signature.size() ||
	!std::equal(signature.begin(), signature.end(), format.data))
	return report(message, "The LMDB environment is missing the BRL-CAD workspace signature");
    if (format.size < signature.size() + sizeof(schema_major))
	return report(message, "The workspace bootstrap is truncated");
    uint16_t major;
    const auto *rest = read_integer(format.data + signature.size(), major);
    if (major != schema_major)
	return report(message, "Unsupported BRL-CAD workspace schema");
    if (format.size != bootstrap_size)
	return report(message, "The workspace bootstrap has an invalid length");
    uint16_t minor, encoding;
    uint64_t required;
    rest = read_integer(rest, minor);
    rest = read_integer(rest, required);
    read_integer(rest, encoding);
    if (minor != schema_minor || (required & ~required_features) || encoding != artifact_encoding)
	return report(message, "Unsupported BRL-CAD workspace schema, features or artifact encoding");
    return true;
}

Id
name_key(std::string_view name)
{
    // A fixed key supports DB5 names longer than LMDB's key size limit.
    // The stored value carries the original name to check hash matches.
    Id key;
    picohash_ctx_t hash;
    picohash_init_sha256(&hash);
    picohash_update(&hash, name.data(), name.size());
    picohash_final(&hash, key.data());
    return key;
}

std::array<uint8_t, 1 + absent.size()>
artifact_key(const ArtifactKey &key)
{
    std::array<uint8_t, 1 + absent.size()> encoded;
    encoded.front() = static_cast<uint8_t>(key.first);
    std::copy(key.second.begin(), key.second.end(), encoded.begin() + 1);
    return encoded;
}

void
store_named(MDB_txn *txn, MDB_dbi table, std::string_view name, const Id &id, unsigned int flags = MDB_NOOVERWRITE)
{
    Bytes row(id.begin(), id.end());
    row.insert(row.end(), name.begin(), name.end());
    put(txn, table, bytes(name_key(name)), row, flags);
}

bool
read_named(ByteView value, std::string_view &name, Id &id)
{
    if (value.size <= id.size())
	return false;
    std::copy_n(value.data, id.size(), id.begin());
    name = {reinterpret_cast<const char *>(value.data + id.size()), value.size - id.size()};
    return id != absent && name.find('\0') == std::string_view::npos;
}

bool
get_artifact(MDB_txn *txn, MDB_dbi table, const ArtifactKey &key, ByteView &payload)
{
    return key.second != absent && get(txn, table, bytes(artifact_key(key)), payload) &&
	payload.size && identify(key.first, payload) == key.second;
}

bool
load_object(MDB_txn *txn, const Tables &tables, const Id &id, Object &object)
{
    ByteView payload;
    return get_artifact(txn, tables.artifacts, {Kind::Object, id}, payload) &&
	decode_object(payload, object);
}

bool
load_record(MDB_txn *txn, const Tables &tables, const Id &id, Object &object, ByteView &body)
{
    return load_object(txn, tables, id, object) &&
	(object.body_id == absent || get_artifact(txn, tables.artifacts, {Kind::Body, object.body_id}, body));
}

bool
copy_record(MDB_txn *txn, const Tables &tables, const Id &id, void *record, size_t size)
{
    ByteView body;
    Object object;
    struct db5_raw_internal raw;
    if (!load_record(txn, tables, id, object, body) || !record_metadata(object, raw) ||
	body.size != raw.body.ext_nbytes || size != raw.object_length)
	return false;
    auto *output = static_cast<uint8_t *>(record);
    for (const auto &part : {object.prefix, body, object.suffix}) {
	if (!part.size)
	    continue;
	std::memcpy(output, part.data, part.size);
	output += part.size;
    }
    return true;
}

bool
lookup_named(MDB_txn *txn, MDB_dbi table, std::string_view name, Id &id)
{
    ByteView row;
    if (!get(txn, table, bytes(name_key(name)), row)) {
	id = absent;
	return true;
    }
    std::string_view stored;
    return read_named(row, stored, id) && name == stored;
}

bool
load_revision(MDB_txn *txn, const Tables &tables, const Id &id, Revision &revision)
{
    ByteView payload;
    return get_artifact(txn, tables.artifacts, {Kind::Revision, id}, payload) &&
	decode_revision(payload, revision);
}

bool
read_head(MDB_txn *txn, const Tables &tables, WorkspaceHead &head, Revision *revision = nullptr)
{
    ByteView stored;
    if (!get(txn, tables.metadata, bytes(head_key), stored) || stored.size != head_size)
	return false;
    std::copy_n(stored.data, head.revision.size(), head.revision.begin());
    const auto *model = stored.data + head.revision.size();
    std::copy_n(model, head.model.size(), head.model.begin());
    read_integer(model + head.model.size(), head.generation);
    Revision selected;
    if (head.model == absent || !head.generation ||
	!load_revision(txn, tables, head.revision, selected) || selected.model != head.model)
	return false;
    if (revision)
	*revision = std::move(selected);
    return true;
}

bool
load_model_at(MDB_txn *txn, const Tables &tables, const Id &id, Manifest &model)
{
    return manifest_at_revision([&](const ArtifactKey &key, ByteView &payload) {
	return get(txn, tables.artifacts, bytes(artifact_key(key)), payload);
    }, [&](const Id &entry) {
	ByteView ignored;
	return get(txn, tables.boundaries, bytes(entry), ignored);
    }, id, model);
}

template <typename Visit>
bool
scan_table(MDB_txn *txn, MDB_dbi table, ByteView prefix, Visit visit)
{
    MDB_cursor *raw = nullptr;
    check(mdb_cursor_open(txn, table, &raw));
    const Cursor cursor(raw, mdb_cursor_close);
    MDB_val key = value(prefix), row;
    int status = mdb_cursor_get(raw, &key, &row, prefix.size ? MDB_SET_RANGE : MDB_FIRST);
    while (status == MDB_SUCCESS) {
	const auto name = view(key);
	if (prefix.size && (name.size < prefix.size || std::memcmp(name.data, prefix.data, prefix.size)))
	    return true;
	if (!visit(name, view(row)))
	    return false;
	status = mdb_cursor_get(raw, &key, &row, MDB_NEXT);
    }
    if (status != MDB_NOTFOUND)
	check(status);
    return true;
}

bool
read_id(ByteView raw, Id &id)
{
    if (raw.size != id.size())
	return false;
    std::copy_n(raw.data, raw.size, id.begin());
    return id != absent;
}

bool
load_pack(MDB_txn *txn, const Tables &tables, const WorkspaceHead &head, bool graph_only, Pack &pack)
{
    const uint8_t revision_tag = static_cast<uint8_t>(Kind::Revision);
    const ByteView prefix = graph_only ? ByteView{&revision_tag, sizeof(revision_tag)} : ByteView{};
    pack.selected_revision = head.revision;
    pack.selected_model = head.model;
    return scan_table(txn, tables.artifacts, prefix, [&](ByteView key, ByteView payload) {
	Id id;
	if (key.size != id.size() + 1 || !read_id({key.data + 1, key.size - 1}, id) || !payload.size)
	    return false;
	return pack.artifacts.emplace(ArtifactKey{static_cast<Kind>(key.data[0]), id}, payload).second;
    }) && scan_table(txn, tables.references, {}, [&](ByteView key, ByteView payload) {
	std::string_view name;
	Id id, hashed;
	return read_named(payload, name, id) && read_id(key, hashed) && hashed == name_key(name) &&
	    pack.references.emplace(name, id).second;
    }) && scan_table(txn, tables.preferences, {}, [&](ByteView key, ByteView payload) {
	Id parent, child;
	return read_id(key, parent) && read_id(payload, child) && pack.preferences.emplace(parent, child).second;
    }) && scan_table(txn, tables.boundaries, {}, [&](ByteView key, ByteView payload) {
	Id boundary;
	return !payload.size && read_id(key, boundary) && pack.boundaries.insert(boundary).second;
    }) && pack.artifacts.count({Kind::Revision, head.revision});
}

void
copy_pack(Pack &&pack, Db5HistoryDraft &result)
{
    ArtifactIndex borrowed;
    borrowed.swap(pack.artifacts);
    Db5HistoryDraft draft;
    draft.pack = std::move(pack);
    for (const auto &entry : borrowed)
	draft.store(entry.first, Bytes(entry.second.data, entry.second.data + entry.second.size));
    result = std::move(draft);
}

void
store_head(MDB_txn *txn, const Tables &tables, const WorkspaceHead &head, unsigned int flags)
{
    std::array<uint8_t, head_size> encoded;
    auto *next = std::copy(head.revision.begin(), head.revision.end(), encoded.data());
    next = std::copy(head.model.begin(), head.model.end(), next);
    write_integer(next, head.generation);
    put(txn, tables.metadata, bytes(head_key), bytes(encoded), flags);
}

bool
complete_model(const Manifest &model)
{
    return model.count(DB5_GLOBAL_OBJECT_NAME) && !model.count(archive_name);
}

bool
read_model(MDB_txn *txn, const Tables &tables, const Id &selected, Manifest &model, struct bu_vls *message)
{
    Manifest result;
    if (!scan_table(txn, tables.directory, {}, [&](ByteView key, ByteView row) {
	std::string_view name;
	Id id, hashed;
	return read_named(row, name, id) && read_id(key, hashed) && hashed == name_key(name) &&
	    result.emplace(name, id).second;
    }))
	return report(message, "A workspace directory entry is invalid");
    Bytes encoded;
    if (!complete_model(result) || !encode_manifest(result, encoded) || identify(Kind::Manifest, encoded) != selected)
	return report(message, "The workspace directory does not match the selected model");
    model = std::move(result);
    return true;
}

bool
write_directory(MDB_txn *txn, const Tables &tables, const Changes &changes, struct bu_vls *message)
{
    for (const auto &entry : changes) {
	const auto &name = entry.first;
	const auto &change = entry.second;
	Id existing;
	if (!lookup_named(txn, tables.directory, name, existing) || existing != change.before)
	    return report(message, "A workspace name conflicts with the recorded before state");
	if (change.after == absent)
	    erase(txn, tables.directory, bytes(name_key(name)));
	else
	    store_named(txn, tables.directory, name, change.after, existing == absent ? MDB_NOOVERWRITE : 0);
    }
    return true;
}

void
store_artifact(MDB_txn *txn, const Tables &tables, const ArtifactKey &key, ByteView payload)
{
    ByteView existing;
    if (!get(txn, tables.artifacts, bytes(artifact_key(key)), existing)) {
	put(txn, tables.artifacts, bytes(artifact_key(key)), payload);
	return;
    }
    if (existing.size != payload.size || std::memcmp(existing.data, payload.data, payload.size))
	throw LmdbError{MDB_CORRUPTED};
}

bool
store_model(MDB_txn *txn, const Tables &tables, const Id &id, const Manifest &model)
{
    Bytes encoded;
    if (!complete_model(model) || !encode_manifest(model, encoded) || identify(Kind::Manifest, encoded) != id)
	return false;
    store_artifact(txn, tables, {Kind::Manifest, id}, encoded);
    return true;
}

bool
candidate_artifact(MDB_txn *txn, const Tables &tables, const ArtifactIndex &artifacts,
		   const ArtifactKey &key, ByteView &payload, bool &stored)
{
    stored = get(txn, tables.artifacts, bytes(artifact_key(key)), payload);
    if (!stored) {
	const auto candidate = artifacts.find(key);
	if (candidate == artifacts.end())
	    return false;
	payload = candidate->second;
    }
    return payload.size && identify(key.first, payload) == key.second;
}

bool
store_record(MDB_txn *txn, const Tables &tables, const ArtifactIndex &artifacts, const Id &id, std::string_view name)
{
    const ArtifactKey object_key{Kind::Object, id};
    ByteView descriptor, body;
    Object object;
    bool have_object = false, have_body = false;
    if (!candidate_artifact(txn, tables, artifacts, object_key, descriptor, have_object) ||
	!decode_object(descriptor, object))
	return false;
    const ArtifactKey body_key{Kind::Body, object.body_id};
    if ((object.body_id != absent && !candidate_artifact(txn, tables, artifacts, body_key, body, have_body)) ||
	!valid_named_record(object, body, name))
	return false;
    if (!have_object)
	put(txn, tables.artifacts, bytes(artifact_key(object_key)), descriptor);
    if (object.body_id != absent && !have_body)
	put(txn, tables.artifacts, bytes(artifact_key(body_key)), body);
    return true;
}

size_t
import_map_size(const Pack &pack)
{
    // Include slack for page rounding and B-tree pages. Map space is
    // virtual and a writer can grow it if the estimate is too small.
    size_t estimate = initial_map_size;
    for (const auto &artifact : pack.artifacts) {
	const size_t payload = artifact.second.size;
	if (payload > std::numeric_limits<size_t>::max() - estimate ||
	    payload / 4 > std::numeric_limits<size_t>::max() - estimate - payload)
	    throw LmdbError{MDB_MAP_FULL};
	estimate += payload + payload / 4;
    }
    return estimate;
}

bool
import_pack(MDB_env *env, const Pack &pack, const Manifest &manifest, struct bu_vls *message)
{
    auto txn = begin(env, 0);
    Tables tables;
    check(mdb_dbi_open(txn.get(), nullptr, 0, &tables.metadata));
    open_tables(txn.get(), tables, MDB_CREATE);
    std::array<uint8_t, bootstrap_size> format;
    auto *rest = std::copy(signature.begin(), signature.end(), format.data());
    rest = write_integer(rest, schema_major);
    rest = write_integer(rest, schema_minor);
    rest = write_integer(rest, required_features);
    write_integer(rest, artifact_encoding);
    put(txn.get(), tables.metadata, bytes(bootstrap_key), bytes(format));

    for (const auto &artifact : pack.artifacts)
	put(txn.get(), tables.artifacts, bytes(artifact_key(artifact.first)), artifact.second);
    for (const auto &entry : manifest)
	store_named(txn.get(), tables.directory, entry.first, entry.second);
    for (const auto &ref : pack.references)
	store_named(txn.get(), tables.references, ref.first, ref.second);
    for (const auto &preference : pack.preferences)
	put(txn.get(), tables.preferences, bytes(preference.first), bytes(preference.second));
    for (const auto &boundary : pack.boundaries)
	put(txn.get(), tables.boundaries, bytes(boundary), {});

    const WorkspaceHead head{pack.selected_revision, pack.selected_model, initial_generation};
    store_head(txn.get(), tables, head, MDB_NOOVERWRITE);

    // Verify the copied bytes. A source DB5 mapping can change while
    // an external writer still holds the original file open.
    WorkspaceHead selected;
    Manifest copied;
    Pack stored;
    if (!read_head(txn.get(), tables, selected) || !load_pack(txn.get(), tables, selected, false, stored))
	return report(message, "The imported workspace archive is invalid");
    if (!read_model(txn.get(), tables, selected.model, copied, message) || !verify_pack(stored, message))
	return false;
    check(mdb_txn_commit(txn.release()));
    return true;
}

} // namespace

struct Workspace::Store {
    Store(fs::path location, Environment &&handle, bool can_write) :
	path(std::move(location)), env(std::move(handle)), writable(can_write) {}

    template <typename Run>
    auto transact(unsigned int flags, Run run) const
    {
	for (;;) {
	    try {
		const std::shared_lock<std::shared_mutex> lock(transactions);
		MDB_txn *raw = nullptr;
		check(mdb_txn_begin(env.get(), nullptr, flags, &raw));
		Transaction txn(raw, mdb_txn_abort);
		return run(txn, handles);
	    } catch (const LmdbError &failure) {
		if (failure.code != MDB_MAP_RESIZED && (flags == MDB_RDONLY || failure.code != MDB_MAP_FULL))
		    throw;
		// The transaction has ended. Resizing requires all other
		// local transactions to finish too.
		const std::unique_lock<std::shared_mutex> lock(transactions);
		if (failure.code == MDB_MAP_RESIZED)
		    check(mdb_env_set_mapsize(env.get(), 0));
		else
		    grow_map(env.get());
	    }
	}
    }

    template <typename Reader>
    bool read(Reader reader, struct bu_vls *message) const
    {
	try {
	    return transact(MDB_RDONLY, [&](const Transaction &txn, const Tables &tables) {
		return reader(txn.get(), tables);
	    });
	} catch (const LmdbError &failure) {
	    return report(message, failure);
	}
    }

    template <typename Writer>
    WorkspaceWriteStatus write(const WorkspaceHead &expected, WorkspaceHead &actual,
			       Writer writer, struct bu_vls *message)
    {
	if (message)
	    bu_vls_trunc(message, 0);
	try {
	    return transact(0, [&](Transaction &txn, const Tables &tables) {
		WorkspaceHead current;
		Revision selected;
		if (!read_head(txn.get(), tables, current, &selected)) {
		    report(message, "The workspace head is invalid");
		    return WorkspaceWriteStatus::Error;
		}
		if (current.generation != expected.generation || current.revision != expected.revision ||
		    current.model != expected.model) {
		    if (message)
			bu_vls_printf(message, "Workspace base changed from %s (generation %ju) to %s (generation %ju)",
			    id_string(expected.revision).c_str(), (uintmax_t)expected.generation,
			    id_string(current.revision).c_str(), (uintmax_t)current.generation);
		    actual = current;
		    return WorkspaceWriteStatus::Stale;
		}
		WorkspaceHead next = current;
		const auto outcome = writer(txn.get(), tables, current, selected, next);
		if (outcome == ChangeStatus::Error)
		    return WorkspaceWriteStatus::Error;
		if (outcome == ChangeStatus::Changed) {
		    if (current.generation == std::numeric_limits<uint64_t>::max()) {
			report(message, "The workspace generation cannot advance");
			return WorkspaceWriteStatus::Error;
		    }
		    ++next.generation;
		    store_head(txn.get(), tables, next, 0);
		    check(mdb_txn_commit(txn.release()));
		}
		actual = next;
		return WorkspaceWriteStatus::Ok;
	    });
	} catch (const LmdbError &failure) {
	    report(message, failure);
	    return WorkspaceWriteStatus::Error;
	}
    }

    fs::path path;
    Environment env;
    Tables handles;
    bool writable;
    size_t clients = 0;
    // LMDB needs all local transactions drained before resizing a map.
    mutable std::shared_mutex transactions;
};

struct Workspace::Registry {
    std::mutex mutex;
    std::map<fs::path, std::unique_ptr<Store>> stores;
};

Workspace::Registry &
Workspace::registry()
{
    static Registry open;
    return open;
}

Workspace::Workspace(Store *state, bool write_access) : store(state), writable(write_access)
{
    ++store->clients;
}

Workspace::~Workspace()
{
    auto &open = registry();
    const std::lock_guard<std::mutex> lock(open.mutex);
    if (!--store->clients)
	open.stores.erase(store->path);
}

bool
Workspace::create(const char *path, const Pack &pack, struct bu_vls *message)
{
    if (!path || !*path)
	return report(message, "A workspace directory is required");
    Manifest manifest;
    if (!read_manifest(pack, pack.selected_model, manifest) || !complete_model(manifest))
	return report(message, "The source archive is missing its selected model");

    std::error_code error;
    if (!fs::create_directory(path, error))
	return report(message, error ? error.message().c_str() : "The workspace directory already exists");

    // Do not let another local handle open the new environment while
    // it is being initialized.
    const std::lock_guard<std::mutex> lock(registry().mutex);
    try {
	const auto env = open_environment(path, 0, import_map_size(pack));
	for (;;) {
	    try {
		if (import_pack(env.get(), pack, manifest, message))
		    return true;
		break;
	    } catch (const LmdbError &failure) {
		if (failure.code != MDB_MAP_FULL)
		    throw;
		grow_map(env.get());
	    }
	}
    } catch (const LmdbError &failure) {
	report(message, failure);
    }
    // An interrupted import leaves no committed bootstrap. On a
    // reported failure remove only the files this new store used.
    const fs::path directory(path);
    bu_file_remove((directory / "lock.mdb").string().c_str());
    bu_file_remove((directory / "data.mdb").string().c_str());
    if (!bu_file_remove(path) && message)
	bu_vls_strcat(message, "\nCould not remove the incomplete workspace directory");
    return false;
}

std::unique_ptr<Workspace>
Workspace::open(const char *path, struct bu_vls *message, bool writable)
{
    if (message)
	bu_vls_trunc(message, 0);
    if (!path || !*path) {
	report(message, "A workspace directory is required");
	return nullptr;
    }

    std::error_code error;
    const auto canonical = fs::canonical(path, error);
    if (error) {
	report(message, error.message().c_str());
	return nullptr;
    }
    const auto file_size = fs::file_size(canonical / "data.mdb", error);
    if (error || !file_size) {
	report(message, "The workspace directory has no LMDB data file");
	return nullptr;
    }
    // LMDB requires one environment per path in a process. Keep the
    // same handle for readers in all documents using this workspace.
    auto &open = registry();
    const std::lock_guard<std::mutex> lock(open.mutex);
    const auto existing = open.stores.find(canonical);
    if (existing != open.stores.end()) {
	if (writable && !existing->second->writable) {
	    report(message, "The workspace is open read-only");
	    return nullptr;
	}
	return std::unique_ptr<Workspace>(new Workspace(existing->second.get(), writable));
    }

    try {
	// Share one writable environment when filesystem permissions permit
	// it. Each document handle still enforces its own requested access.
	Environment env(nullptr, mdb_env_close);
	bool can_write = true;
	const auto location = canonical.string();
	try {
	    env = open_environment(location.c_str(), 0);
	} catch (const LmdbError &failure) {
	    if (writable || (failure.code != EACCES && failure.code != EPERM && failure.code != EROFS))
		throw;
	    env = open_environment(location.c_str(), MDB_RDONLY);
	    can_write = false;
	}
	auto store = std::make_unique<Store>(canonical, std::move(env), can_write);
	auto txn = begin(store->env.get(), MDB_RDONLY);
	check(mdb_dbi_open(txn.get(), nullptr, 0, &store->handles.metadata));
	if (!check_bootstrap(txn.get(), store->handles.metadata, message))
	    return nullptr;
	open_tables(txn.get(), store->handles, 0);
	WorkspaceHead head;
	if (!read_head(txn.get(), store->handles, head)) {
	    report(message, "The workspace head is invalid");
	    return nullptr;
	}
	check(mdb_txn_commit(txn.release()));
	const auto inserted = open.stores.emplace(canonical, std::move(store)).first;
	return std::unique_ptr<Workspace>(new Workspace(inserted->second.get(), writable));
    } catch (const LmdbError &failure) {
	report(message, failure);
	return nullptr;
    }
}

struct db_i *
Workspace::open_snapshot(const char *path, WorkspaceHead &head, struct bu_vls *message)
{
    auto workspace = open(path, message);
    if (!workspace)
	return nullptr;

    const auto location = workspace->store->path.string();
    const auto parent = workspace->store->path.parent_path().string();
    auto state = std::make_unique<db_workspace_view>();
    std::unique_ptr<struct db_i, decltype(&db_close)> db(db_open_inmem(), db_close);
    WorkspaceHead selected;
    const bool built = workspace->store->read([&](MDB_txn *txn, const Tables &tables) {
	if (!read_head(txn, tables, selected))
	    return report(message, "The workspace head is invalid");
	if (!read_model(txn, tables, selected.model, state->model, message))
	    return false;
	// Workspace records have no DB5 file offsets. Reads use the
	// immutable object IDs pinned by the view's directory.
	constexpr b_off_t stored_record = 0;
	for (const auto &entry : state->model) {
	    Object object;
	    struct db5_raw_internal raw;
	    if (!load_object(txn, tables, entry.second, object) || !record_metadata(object, raw) ||
		entry.first != reinterpret_cast<const char *>(raw.name.ext_buf) ||
		!db5_diradd(db.get(), &raw, stored_record, nullptr))
		return report(message, "Could not build the workspace database directory");
	}
	return true;
    }, message);
    if (!built)
	return nullptr;

    auto *dbip = db.get();
    dbip->dbi_filename = bu_strdup(location.c_str());
    const char *paths[] = {".", parent.c_str()};
    dbip->dbi_filepath = bu_argv_dup(sizeof(paths) / sizeof(paths[0]), paths);
    dbip->dbi_read_only = 1;
    dbip->i->dbi_edit_guard = 1;
    dbip->i->dbi_dir_built = 1;
    dbip->i->dbi_nrec = state->model.size();
    state->head = selected;
    state->workspace = std::move(workspace);
    dbip->i->dbi_workspace = state.release();
    if (db5_load_global(dbip, db_lookup(dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET)) != 0) {
	report(message, "The workspace database attributes could not be loaded");
	return nullptr;
    }
    head = selected;
    return db.release();
}

const Manifest *
Workspace::snapshot_model(const struct db_i *db, WorkspaceHead *head)
{
    const auto *state = db ? db->i->dbi_workspace : nullptr;
    if (!state)
	return nullptr;
    if (head)
	*head = state->head;
    return &state->model;
}

bool
Workspace::read_directory(WorkspaceHead &head, Manifest &model, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	WorkspaceHead selected;
	if (!read_head(txn, tables, selected))
	    return report(message, "The workspace head is invalid");
	if (!read_model(txn, tables, selected.model, model, message))
	    return false;
	head = selected;
	return true;
    }, message);
}

bool
Workspace::lookup(std::string_view name, Id &id, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	Id found;
	if (!lookup_named(txn, tables.directory, name, found))
	    return report(message, "The workspace directory entry is invalid");
	id = found;
	return true;
    }, message);
}

bool
Workspace::fetch_artifact(const ArtifactKey &key, Bytes &payload, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	ByteView stored;
	if (!get_artifact(txn, tables.artifacts, key, stored))
	    return report(message, "The workspace artifact is missing or corrupt");
	payload.assign(stored.data, stored.data + stored.size);
	return true;
    }, message);
}

bool
Workspace::fetch_record(const Id &id, Bytes &record, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	ByteView body;
	Object object;
	if (!load_record(txn, tables, id, object, body) ||
	    !materialize_record(object, body, record))
	    return report(message, "The workspace object is missing or corrupt");
	return true;
    }, message);
}

bool
Workspace::read_record(const Id &id, void *record, size_t size) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	return copy_record(txn, tables, id, record, size);
    }, nullptr);
}

bool
Workspace::read_graph(const WorkspaceHead &selected, WorkspaceHead &current,
		      Db5HistoryDraft &result, struct bu_vls *message, Id *last_export) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	WorkspaceHead observed;
	Pack graph;
	if (!read_head(txn, tables, observed) || !load_pack(txn, tables, selected, true, graph))
	    return report(message, "The workspace revision graph is invalid");
	Id exported = absent;
	ByteView saved;
	if (last_export && get(txn, tables.metadata, bytes(export_key), saved) && !read_id(saved, exported))
	    return report(message, "The last exported revision is invalid");
	copy_pack(std::move(graph), result);
	current = observed;
	if (last_export)
	    *last_export = exported;
	return true;
    }, message);
}

bool
Workspace::read_model_at(const Id &revision, Manifest &model, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	return load_model_at(txn, tables, revision, model) ||
	    report(message, "The target workspace revision cannot be reconstructed");
    }, message);
}

bool
Workspace::copy_to(const WorkspaceHead &selected, const char *destination, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	Pack archive;
	if (!load_pack(txn, tables, selected, false, archive))
	    return report(message, "The workspace archive is invalid");
	return create(destination, archive, message);
    }, message);
}

enum db5_save_result
Workspace::export_snapshot(const struct db_i *snapshot, const char *destination, bool with_history,
			   struct bu_vls *message) const
{
    const auto *state = snapshot ? snapshot->i->dbi_workspace : nullptr;
    if (!state || state->workspace->store != store) {
	report(message, "The export requires a pinned view of this workspace");
	return DB5_SAVE_ERROR;
    }
    auto result = DB5_SAVE_ERROR;
    const bool read = store->read([&](MDB_txn *txn, const Tables &tables) {
	Pack archive;
	if (with_history) {
	    if (!load_pack(txn, tables, state->head, false, archive))
		return report(message, "The workspace archive is invalid");
	    if (!verify_pack(archive, message))
		return false;
	}
	const auto reader = [&](const struct directory *dp, struct bu_external *record) {
	    const auto object = state->model.find(dp->d_namep);
	    if (object == state->model.end() || !dp->d_len)
		return false;
	    record->ext_nbytes = dp->d_len;
	    record->ext_buf = static_cast<uint8_t *>(bu_malloc(dp->d_len, "workspace export record"));
	    if (copy_record(txn, tables, object->second, record->ext_buf, record->ext_nbytes))
		return true;
	    bu_free_external(record);
	    return false;
	};
	result = export_db5_snapshot(snapshot, destination, with_history ? &archive : nullptr, message, reader);
	return true;
    }, message);
    return read ? result : DB5_SAVE_ERROR;
}

bool
Workspace::verify(struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	WorkspaceHead head;
	Manifest model;
	Pack archive;
	if (!read_head(txn, tables, head) || !load_pack(txn, tables, head, false, archive))
	    return report(message, "The workspace archive is invalid");
	return read_model(txn, tables, head.model, model, message) && verify_pack(archive, message);
    }, message);
}

bool
Workspace::record_export(const Id &revision, struct bu_vls *message)
{
    if (!writable)
	return report(message, "The workspace is open read-only");
    try {
	return store->transact(0, [&](Transaction &txn, const Tables &tables) {
	    Revision exported;
	    if (!load_revision(txn.get(), tables, revision, exported))
		return report(message, "The exported revision is unavailable");
	    put(txn.get(), tables.metadata, bytes(export_key), bytes(revision), 0);
	    check(mdb_txn_commit(txn.release()));
	    return true;
	});
    } catch (const LmdbError &failure) {
	return report(message, failure);
    }
}

WorkspaceWriteStatus
Workspace::commit(const WorkspaceHead &expected, const Revision &revision, const ArtifactIndex &artifacts,
		  WorkspaceHead &actual, struct bu_vls *message)
{
    if (!writable) {
	report(message, "The workspace is open read-only");
	return WorkspaceWriteStatus::Error;
    }
    return store->write(expected, actual, [&](MDB_txn *txn, const Tables &tables,
	const WorkspaceHead &base, const Revision &, WorkspaceHead &next) {
	Bytes encoded;
	if (revision.parent != base.revision || !encode_revision(revision, encoded))
	    return fail_change(message, "The workspace revision is invalid for the selected parent");
	Manifest current, model;
	if (!read_model(txn, tables, base.model, current, message))
	    return ChangeStatus::Error;
	if (!apply_revision(revision, current, model) || !complete_model(model))
	    return fail_change(message, "The workspace edit does not produce its recorded model");

	for (const auto &entry : revision.changes) {
	    const auto &id = entry.second.after;
	    if (id != absent && !store_record(txn, tables, artifacts, id, entry.first))
		return fail_change(message, "A changed record has missing or invalid workspace artifacts");
	}
	if (!store_model(txn, tables, revision.model, model))
	    return fail_change(message, "The workspace model cannot be stored");
	const auto id = identify(Kind::Revision, encoded);
	if (id == absent)
	    return fail_change(message, "The workspace revision cannot be identified");
	store_artifact(txn, tables, {Kind::Revision, id}, encoded);
	if (!write_directory(txn, tables, revision.changes, message))
	    return ChangeStatus::Error;
	next.revision = id;
	next.model = revision.model;
	return ChangeStatus::Changed;
    }, message);
}

WorkspaceWriteStatus
Workspace::checkout(const WorkspaceHead &expected, const Id &target, WorkspaceHead &actual, struct bu_vls *message)
{
    if (!writable) {
	report(message, "The workspace is open read-only");
	return WorkspaceWriteStatus::Error;
    }
    return store->write(expected, actual, [&](MDB_txn *txn, const Tables &tables,
	const WorkspaceHead &base, const Revision &selected, WorkspaceHead &next) {
	if (target == base.revision)
	    return ChangeStatus::Unchanged;
	Revision revision;
	Manifest model;
	if (!load_revision(txn, tables, target, revision) ||
	    !load_model_at(txn, tables, target, model) || !complete_model(model))
	    return fail_change(message, "The target workspace revision cannot be reconstructed");

	Manifest current;
	if (!read_model(txn, tables, base.model, current, message))
	    return ChangeStatus::Error;
	if (!store_model(txn, tables, revision.model, model))
	    return fail_change(message, "The workspace model cannot be stored");
	if (!write_directory(txn, tables, diff_manifests(current, model), message))
	    return ChangeStatus::Error;
	if (target == selected.parent)
	    put(txn, tables.preferences, bytes(target), bytes(base.revision), 0);
	else if (base.revision == revision.parent)
	    put(txn, tables.preferences, bytes(base.revision), bytes(target), 0);
	next.revision = target;
	next.model = revision.model;
	return ChangeStatus::Changed;
    }, message);
}

WorkspaceWriteStatus
Workspace::mark(const WorkspaceHead &expected, std::string_view name, bool move, WorkspaceHead &actual,
		struct bu_vls *message)
{
    if (!writable) {
	report(message, "The workspace is open read-only");
	return WorkspaceWriteStatus::Error;
    }
    return store->write(expected, actual, [&](MDB_txn *txn, const Tables &tables,
	const WorkspaceHead &base, const Revision &, WorkspaceHead &) {
	if (name.empty() || name.find('\0') != std::string_view::npos)
	    return fail_change(message, "A mark name is required");
	Id existing;
	if (!lookup_named(txn, tables.references, name, existing))
	    return fail_change(message, "The workspace mark is invalid");
	if (existing != absent && !move)
	    return fail_change(message, "Mark already exists. Use --move to retarget it.");
	if (existing == base.revision)
	    return ChangeStatus::Unchanged;
	store_named(txn, tables.references, name, base.revision, existing == absent ? MDB_NOOVERWRITE : 0);
	return ChangeStatus::Changed;
    }, message);
}

} // namespace db_history

void
db_workspace_free_view(struct db_i_internal *i)
{
    delete i->dbi_workspace;
    i->dbi_workspace = nullptr;
}

int
db_workspace_read(const struct db_i *dbip, const struct directory *dp, void *buffer, size_t count)
{
    const auto &view = *dbip->i->dbi_workspace;
    const auto object = view.model.find(dp->d_namep);
    return object != view.model.end() && view.workspace->read_record(object->second, buffer, count) ? 0 : -1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
