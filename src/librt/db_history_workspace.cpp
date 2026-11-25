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
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <shared_mutex>

#include "lmdb.h"

#include "bu/file.h"
#include "bu/vls.h"
#include "../libbu/picohash.h"
#include "db_history_workspace.h"

namespace db_history {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view bootstrap_key = "workspace";
constexpr std::string_view head_key = "head";
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
put(MDB_txn *txn, MDB_dbi table, ByteView key, ByteView payload)
{
    auto search = value(key);
    auto stored = value(payload);
    check(mdb_put(txn, table, &search, &stored, MDB_NOOVERWRITE));
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
store_named(MDB_txn *txn, MDB_dbi table, std::string_view name, const Id &id)
{
    Bytes row(id.begin(), id.end());
    row.insert(row.end(), name.begin(), name.end());
    put(txn, table, bytes(name_key(name)), row);
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
read_head(MDB_txn *txn, const Tables &tables, WorkspaceHead &head)
{
    ByteView stored;
    if (!get(txn, tables.metadata, bytes(head_key), stored) || stored.size != head_size)
	return false;
    std::copy_n(stored.data, head.revision.size(), head.revision.begin());
    const auto *model = stored.data + head.revision.size();
    std::copy_n(model, head.model.size(), head.model.begin());
    read_integer(model + head.model.size(), head.generation);
    ByteView payload;
    Revision selected;
    return head.model != absent && head.generation &&
	get_artifact(txn, tables.artifacts, {Kind::Revision, head.revision}, payload) &&
	decode_revision(payload, selected) && selected.model == head.model;
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

void
import_pack(MDB_env *env, const Pack &pack, const Manifest &manifest)
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

    std::array<uint8_t, head_size> head;
    auto *next = std::copy(pack.selected_revision.begin(), pack.selected_revision.end(), head.data());
    next = std::copy(pack.selected_model.begin(), pack.selected_model.end(), next);
    write_integer(next, initial_generation);
    put(txn.get(), tables.metadata, bytes(head_key), bytes(head));
    check(mdb_txn_commit(txn.release()));
}

} // namespace

struct Workspace::Store {
    Store(fs::path location, Environment &&handle) : path(std::move(location)), env(std::move(handle)) {}

    template <typename Reader>
    bool read(Reader reader, struct bu_vls *message) const
    {
	try {
	    for (;;) {
		{
		    const std::shared_lock<std::shared_mutex> lock(transactions);
		    MDB_txn *raw = nullptr;
		    const int status = mdb_txn_begin(env.get(), nullptr, MDB_RDONLY, &raw);
		    if (status != MDB_MAP_RESIZED) {
			check(status);
			const Transaction txn(raw, mdb_txn_abort);
			return reader(txn.get(), tables);
		    }
		}
		// Adopting a larger map requires all local readers to finish.
		const std::unique_lock<std::shared_mutex> lock(transactions);
		check(mdb_env_set_mapsize(env.get(), 0));
	    }
	} catch (const LmdbError &failure) {
	    return report(message, failure);
	}
    }

    fs::path path;
    Environment env;
    Tables tables;
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

Workspace::Workspace(Store *state) : store(state)
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
    if (!verify_pack(pack, message) || !read_manifest(pack, pack.selected_model, manifest))
	return false;

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
		import_pack(env.get(), pack, manifest);
		return true;
	    } catch (const LmdbError &failure) {
		if (failure.code != MDB_MAP_FULL)
		    throw;
		MDB_envinfo info;
		check(mdb_env_info(env.get(), &info));
		if (info.me_mapsize > std::numeric_limits<size_t>::max() / 2)
		    throw;
		check(mdb_env_set_mapsize(env.get(), info.me_mapsize * 2));
	    }
	}
    } catch (const LmdbError &failure) {
	// An interrupted import leaves no committed bootstrap. On a
	// reported failure remove only the files this new store used.
	const fs::path directory(path);
	bu_file_remove((directory / "lock.mdb").string().c_str());
	bu_file_remove((directory / "data.mdb").string().c_str());
	report(message, failure);
	if (!bu_file_remove(path) && message)
	    bu_vls_strcat(message, "\nCould not remove the incomplete workspace directory");
	return false;
    }
}

std::unique_ptr<Workspace>
Workspace::open(const char *path, struct bu_vls *message)
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
    // LMDB requires one environment per path in a process. Keep the
    // same handle for readers in all documents using this workspace.
    auto &open = registry();
    const std::lock_guard<std::mutex> lock(open.mutex);
    const auto existing = open.stores.find(canonical);
    if (existing != open.stores.end())
	return std::unique_ptr<Workspace>(new Workspace(existing->second.get()));

    try {
	auto store = std::make_unique<Store>(canonical, open_environment(canonical.string().c_str(), MDB_RDONLY));
	auto txn = begin(store->env.get(), MDB_RDONLY);
	check(mdb_dbi_open(txn.get(), nullptr, 0, &store->tables.metadata));
	if (!check_bootstrap(txn.get(), store->tables.metadata, message))
	    return nullptr;
	open_tables(txn.get(), store->tables, 0);
	WorkspaceHead head;
	if (!read_head(txn.get(), store->tables, head)) {
	    report(message, "The workspace head is invalid");
	    return nullptr;
	}
	check(mdb_txn_commit(txn.release()));
	const auto inserted = open.stores.emplace(canonical, std::move(store)).first;
	return std::unique_ptr<Workspace>(new Workspace(inserted->second.get()));
    } catch (const LmdbError &failure) {
	report(message, failure);
	return nullptr;
    }
}

bool
Workspace::read_directory(WorkspaceHead &head, Manifest &model, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	WorkspaceHead selected;
	if (!read_head(txn, tables, selected))
	    return report(message, "The workspace head is invalid");
	MDB_cursor *raw = nullptr;
	check(mdb_cursor_open(txn, tables.directory, &raw));
	const Cursor cursor(raw, mdb_cursor_close);
	Manifest result;
	MDB_val key, row;
	int status = mdb_cursor_get(raw, &key, &row, MDB_FIRST);
	for (; status == MDB_SUCCESS; status = mdb_cursor_get(raw, &key, &row, MDB_NEXT)) {
	    std::string_view name;
	    Id id;
	    if (!read_named(view(row), name, id) || key.mv_size != id.size() ||
		std::memcmp(key.mv_data, name_key(name).data(), key.mv_size) ||
		!result.emplace(name, id).second)
		return report(message, "A workspace directory entry is invalid");
	}
	if (status != MDB_NOTFOUND)
	    check(status);
	Bytes encoded;
	if (!result.count(DB5_GLOBAL_OBJECT_NAME) || result.count(archive_name) ||
	    !encode_manifest(result, encoded) || identify(Kind::Manifest, encoded) != selected.model)
	    return report(message, "The workspace directory does not match the selected model");
	head = selected;
	model = std::move(result);
	return true;
    }, message);
}

bool
Workspace::lookup(std::string_view name, Id &id, struct bu_vls *message) const
{
    return store->read([&](MDB_txn *txn, const Tables &tables) {
	ByteView row;
	if (!get(txn, tables.directory, bytes(name_key(name)), row)) {
	    id = absent;
	    return true;
	}
	std::string_view stored;
	Id found;
	if (!read_named(row, stored, found) || stored != name)
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
	ByteView descriptor, body;
	Object object;
	const auto artifacts = tables.artifacts;
	if (!get_artifact(txn, artifacts, {Kind::Object, id}, descriptor) ||
	    !decode_object(descriptor, object) ||
	    (object.body_id != absent && !get_artifact(txn, artifacts, {Kind::Body, object.body_id}, body)) ||
	    !materialize_record(object, body, record))
	    return report(message, "The workspace object is missing or corrupt");
	return true;
    }, message);
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
