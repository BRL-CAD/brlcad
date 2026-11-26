/*                      G V E R S I O N . C P P
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

/** @file gversion.cpp
 * Experimental DB5 session undo and embedded revision history.
 *
 * Run one GED command per line on standard input. History, undo, redo
 * with an optional child revision, and checkout by revision number
 * operate on the session. Double quotes group command arguments.
 *
 * Session mode exports plain DB5. The default exports DB5 with history
 * in one hidden binary object. LMDB mode persists each successful
 * change or checkout and can export either DB5 representation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lmdb.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/mapped_file.h"
#include "bu/parse.h"
#include "bu/uuid.h"
#include "ged.h"
#include "raytrace.h"

namespace {

namespace fs = std::filesystem;

constexpr char history_name[] = "_BRLCAD_HISTORY";
constexpr std::string_view history_magic = "BRLGHIST1";
constexpr size_t db5_alignment = 8;
constexpr uint64_t root_revision = 0;
constexpr uint64_t absent_record = 0;

using Digest = std::array<uint8_t, 16>;
const Digest history_namespace = {
    0xaa, 0xcc, 0x84, 0x5b, 0x88, 0xa4, 0x4c, 0x81,
    0xaf, 0xfa, 0xfe, 0xab, 0x0f, 0xb2, 0x0e, 0x7d
};

Digest
digest(std::string_view bytes)
{
    // Fixed-size blocks keep the UUID API's 32-bit length conversion
    // from truncating records larger than 4 GiB.
    constexpr size_t block_size = 64 * 1024 * 1024;
    Digest result = history_namespace;
    while (!bytes.empty()) {
	const auto block = bytes.substr(0, block_size);
	Digest next;
	bu_uuid_create(next.data(), block.size(),
	    reinterpret_cast<const uint8_t *>(block.data()), result.data());
	result = next;
	bytes.remove_prefix(block.size());
    }
    return result;
}

struct Cursor {
    std::string_view bytes;

    std::string_view take(size_t count)
    {
	if (count > bytes.size())
	    throw std::runtime_error("truncated database or history");
	const auto result = bytes.substr(0, count);
	bytes.remove_prefix(count);
	return result;
    }

    uint64_t number(size_t width = sizeof(uint64_t))
    {
	uint64_t value = 0;
	for (unsigned char byte : take(width))
	    value = (value << 8) | byte;
	return value;
    }

    std::string_view field(size_t width = sizeof(uint64_t))
    {
	const uint64_t count = number(width);
	if (count > bytes.size())
	    throw std::runtime_error("truncated record field");
	return take(static_cast<size_t>(count));
    }
};

void
append_number(std::string &output, uint64_t number)
{
    for (size_t i = sizeof(number); i > 0; --i)
	output.push_back(static_cast<char>(number >> ((i - 1) * 8)));
}

void
append_field(std::string &output, std::string_view field)
{
    append_number(output, field.size());
    output.append(field);
}

size_t
db5_width(uint8_t flags, unsigned mask, unsigned shift)
{
    const size_t width = size_t{1} << ((flags & mask) >> shift);
    if (width > sizeof(size_t))
	throw std::runtime_error("DB5 length exceeds the platform's supported width");
    return width;
}

std::string_view
record_bytes(Cursor &input)
{
    Cursor prefix{input.bytes};
    const auto header_data = prefix.take(sizeof(db5_ondisk_header));
    const auto *header = reinterpret_cast<const db5_ondisk_header *>(header_data.data());
    const uint64_t chunks = prefix.number(db5_width(header->db5h_hflags,
	DB5HDR_HFLAGS_OBJECT_WIDTH_MASK, DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT));
    if (chunks == 0 || chunks > input.bytes.size() / db5_alignment)
	throw std::runtime_error("invalid DB5 record length");
    return input.take(static_cast<size_t>(chunks) * db5_alignment);
}

db5_raw_internal
unpack_record(std::string_view bytes)
{
    Cursor record{bytes};
    if (record_bytes(record).size() != bytes.size())
	throw std::runtime_error("invalid embedded DB5 record length");

    const auto *header = reinterpret_cast<const db5_ondisk_header *>(bytes.data());
    const uint8_t hflags = header->db5h_hflags;
    const uint8_t aflags = header->db5h_aflags;
    const uint8_t bflags = header->db5h_bflags;
    const size_t prefix = sizeof(*header) + db5_width(hflags,
	DB5HDR_HFLAGS_OBJECT_WIDTH_MASK, DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT);
    if (prefix >= bytes.size())
	throw std::runtime_error("missing DB5 record terminator");

    // The existing pointer decoder assumes all field lengths are in bounds.
    Cursor fields{bytes.substr(prefix, bytes.size() - prefix - 1)};
    if (hflags & DB5HDR_HFLAGS_NAME_PRESENT) {
	const auto name = fields.field(db5_width(hflags,
	    DB5HDR_HFLAGS_NAME_WIDTH_MASK, DB5HDR_HFLAGS_NAME_WIDTH_SHIFT));
	if (name.empty() || name.back() != '\0' || name.find('\0') != name.size() - 1)
	    throw std::runtime_error("invalid DB5 object name");
    }
    if (aflags & DB5HDR_AFLAGS_PRESENT)
	fields.field(db5_width(aflags,
	    DB5HDR_AFLAGS_WIDTH_MASK, DB5HDR_AFLAGS_WIDTH_SHIFT));
    if (bflags & DB5HDR_BFLAGS_PRESENT)
	fields.field(db5_width(bflags,
	    DB5HDR_BFLAGS_WIDTH_MASK, DB5HDR_BFLAGS_WIDTH_SHIFT));

    db5_raw_internal raw = {};
    raw.magic = DB5_RAW_INTERNAL_MAGIC;
    const auto *start = reinterpret_cast<const unsigned char *>(bytes.data());
    if (db5_get_raw_internal_ptr(&raw, start) != start + bytes.size())
	throw std::runtime_error("invalid DB5 record");
    return raw;
}

struct Record {
    std::string name;
    std::string bytes;
    Digest id;
};

using Blob = std::shared_ptr<const Record>;
using Directory = std::map<std::string, Blob>;

bool
same_record(const Blob &left, const Blob &right)
{
    return left == right || (left && right && left->id == right->id && left->bytes == right->bytes);
}

Blob
make_record(const db5_raw_internal &raw, std::string_view bytes)
{
    if (raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT || raw.name.ext_nbytes <= 1)
	throw std::runtime_error("expected a named DB5 application object");
    const std::string name(reinterpret_cast<const char *>(raw.name.ext_buf), raw.name.ext_nbytes - 1);
    return std::make_shared<Record>(Record{name, std::string(bytes), digest(bytes)});
}

struct Model {
    std::string header;
    Directory objects;
    std::string history;
};

struct MappedFileCloser {
    void operator()(bu_mapped_file *file) const
    {
	bu_close_mapped_file(file);
	bu_free_mapped_files(0);
    }
};

Model
read_model(const fs::path &path, const Directory &previous = {})
{
    std::unique_ptr<bu_mapped_file, MappedFileCloser> file(
	bu_open_mapped_file(path.string().c_str(), nullptr));
    if (!file)
	throw std::runtime_error("cannot open " + path.string());

    Cursor input{std::string_view(static_cast<const char *>(file->buf), file->buflen)};
    const auto header = input.take(db5_alignment);
    if (!db5_header_is_valid(reinterpret_cast<const unsigned char *>(header.data())))
	throw std::runtime_error("expected a DB5 database");
    Model model{std::string(header), {}, {}};
    while (!input.bytes.empty()) {
	const auto bytes = record_bytes(input);
	const auto raw = unpack_record(bytes);
	if (raw.h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE)
	    continue;
	if (raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT || raw.name.ext_nbytes <= 1)
	    throw std::runtime_error("expected a named DB5 application object");
	const std::string name(reinterpret_cast<const char *>(raw.name.ext_buf), raw.name.ext_nbytes - 1);
	if (name == history_name) {
	    if (!model.history.empty() || !raw.h_name_hidden ||
		raw.major_type != DB5_MAJORTYPE_BINARY_UNIF ||
		raw.minor_type != DB5_MINORTYPE_BINU_8BITINT_U ||
		raw.b_zzz != DB5_ZZZ_UNCOMPRESSED || !raw.body.ext_nbytes)
		throw std::runtime_error("reserved history object has an unexpected format");
	    model.history.assign(reinterpret_cast<const char *>(raw.body.ext_buf), raw.body.ext_nbytes);
	    continue;
	}

	const auto old = previous.find(name);
	const Blob object = old != previous.end() && old->second->bytes == bytes
	    ? old->second : make_record(raw, bytes);
	if (!model.objects.emplace(name, object).second)
	    throw std::runtime_error("duplicate DB5 object " + name);
    }
    if (model.objects.count(DB5_GLOBAL_OBJECT_NAME) == 0)
	throw std::runtime_error("missing _GLOBAL record");
    return model;
}

void
write_model(const fs::path &path, const std::string &header, const Directory &directory,
    std::string_view history = {})
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << header;
    for (const auto &entry : directory)
	output << entry.second->bytes;
    if (!history.empty()) {
	bu_external body = BU_EXTERNAL_INIT_ZERO;
	body.ext_buf = reinterpret_cast<uint8_t *>(const_cast<char *>(history.data()));
	body.ext_nbytes = history.size();

	bu_external record = BU_EXTERNAL_INIT_ZERO;
	const std::unique_ptr<bu_external, decltype(&bu_free_external)> cleanup(&record, &bu_free_external);
	db5_export_object3(&record, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT, history_name,
	    DB5HDR_HFLAGS_HIDDEN_OBJECT, nullptr, &body, DB5_MAJORTYPE_BINARY_UNIF,
	    DB5_MINORTYPE_BINU_8BITINT_U, DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
	output << std::string_view(reinterpret_cast<const char *>(record.ext_buf), record.ext_nbytes);
    }
    output.close();
}

Digest
directory_digest(const Directory &directory)
{
    std::string ids;
    for (const auto &entry : directory)
	ids.append(reinterpret_cast<const char *>(entry.second->id.data()), entry.second->id.size());
    return digest(ids);
}

struct Change {
    Blob before;
    Blob after;
};

using Changes = std::map<std::string, Change>;

Changes
compare(const Directory &before, const Directory &after)
{
    Changes result;
    for (const auto &entry : before) {
	const auto found = after.find(entry.first);
	if (found == after.end())
	    result.emplace(entry.first, Change{entry.second, nullptr});
	else if (!same_record(entry.second, found->second))
	    result.emplace(entry.first, Change{entry.second, found->second});
    }
    for (const auto &entry : after) {
	if (before.count(entry.first) == 0)
	    result.emplace(entry.first, Change{nullptr, entry.second});
    }
    return result;
}

struct Revision {
    uint64_t parent;
    std::string label;
    Changes changes;
};

struct SavePoint {
    Directory directory;
    uint64_t head;
    size_t revision_count;
};

template <typename WriteRecord>
std::string
encode_revision(const Revision &revision, WriteRecord write_record)
{
    std::string output;
    append_number(output, revision.parent);
    append_field(output, revision.label);
    append_number(output, revision.changes.size());
    for (const auto &entry : revision.changes) {
	write_record(output, entry.second.before);
	write_record(output, entry.second.after);
    }
    return output;
}

template <typename ReadRecord>
Revision
decode_revision(Cursor &input, uint64_t ordinal, ReadRecord read_record)
{
    const auto parent = input.number();
    auto label = std::string(input.field());
    const auto count = input.number();
    if (parent >= ordinal || count == 0)
	throw std::runtime_error("invalid revision header");
    Changes changes;
    for (uint64_t i = 0; i < count; ++i) {
	const Blob before = read_record(input);
	const Blob after = read_record(input);
	if (same_record(before, after) || (before && after && before->name != after->name))
	    throw std::runtime_error("invalid object change");
	const auto &name = before ? before->name : after->name;
	if (name == history_name ||
	    (name == DB5_GLOBAL_OBJECT_NAME && (!before || !after)) ||
	    !changes.emplace(name, Change{before, after}).second)
	    throw std::runtime_error("invalid history entry for " + name);
    }
    return {parent, std::move(label), std::move(changes)};
}

class History {
public:
    explicit History(Directory objects) : directory(std::move(objects)) {}

    Directory directory;
    uint64_t head = root_revision;
    std::vector<Revision> revisions = {{root_revision, "initial model", {}}};

    void record(std::string label, Directory objects)
    {
	auto edits = compare(directory, objects);
	if (edits.empty())
	    return;
	revisions.push_back({head, std::move(label), std::move(edits)});
	head = revisions.size() - 1;
	directory = std::move(objects);
    }

    void checkout(uint64_t target)
    {
	if (target >= revisions.size())
	    throw std::runtime_error("unknown revision");
	auto result = directory;
	auto ancestor = head;
	auto next = target;
	std::vector<uint64_t> forward;
	while (ancestor != next) {
	    // A parent always has a lower ordinal than its children.
	    if (ancestor > next) {
		apply(result, revisions[ancestor], false);
		ancestor = revisions[ancestor].parent;
	    } else {
		forward.push_back(next);
		next = revisions[next].parent;
	    }
	}
	for (auto it = forward.rbegin(); it != forward.rend(); ++it)
	    apply(result, revisions[*it], true);
	directory = std::move(result);
	head = target;
    }

    std::string serialize() const;
    void load(std::string_view bytes);

private:
    static void apply(Directory &objects, const Revision &revision, bool forward)
    {
	for (const auto &entry : revision.changes) {
	    const auto &expected = forward ? entry.second.before : entry.second.after;
	    const auto &replacement = forward ? entry.second.after : entry.second.before;
	    const auto found = objects.find(entry.first);
	    if (!same_record(expected, found == objects.end() ? nullptr : found->second))
		throw std::runtime_error("revision does not match object " + entry.first);
	    if (replacement)
		objects.insert_or_assign(entry.first, replacement);
	    else
		objects.erase(found);
	}
    }
};

std::string
History::serialize() const
{
    std::map<Digest, uint64_t> ordinals;
    std::vector<Blob> records;
    auto ordinal = [&](const Blob &blob) {
	if (!blob)
	    return absent_record;
	const auto inserted = ordinals.emplace(blob->id, records.size() + 1);
	const auto index = inserted.first->second;
	if (inserted.second)
	    records.push_back(blob);
	else if (!same_record(records[index - 1], blob))
	    throw std::runtime_error("history record hash collision");
	return index;
    };

    std::string edits;
    for (size_t i = 1; i < revisions.size(); ++i) {
	edits += encode_revision(revisions[i], [&](std::string &output, const Blob &blob) {
	    append_number(output, ordinal(blob));
	});
    }

    std::string output(history_magic);
    const auto hash = directory_digest(directory);
    output.append(reinterpret_cast<const char *>(hash.data()), hash.size());
    append_number(output, head);
    append_number(output, records.size());
    for (const auto &record : records)
	append_field(output, record->bytes);
    append_number(output, revisions.size() - 1);
    output += edits;
    const auto checksum = digest(output);
    output.append(reinterpret_cast<const char *>(checksum.data()), checksum.size());
    return output;
}

void
History::load(std::string_view bytes)
{
    if (bytes.size() < Digest{}.size())
	throw std::runtime_error("truncated embedded history");
    const auto content = bytes.substr(0, bytes.size() - Digest{}.size());
    const auto checksum = digest(content);
    if (bytes.substr(content.size()) !=
	std::string_view(reinterpret_cast<const char *>(checksum.data()), checksum.size()))
	throw std::runtime_error("invalid embedded history checksum");
    Cursor input{content};
    if (input.take(history_magic.size()) != history_magic)
	throw std::runtime_error("unsupported embedded history format");
    const auto hash = directory_digest(directory);
    const std::string_view expected(reinterpret_cast<const char *>(hash.data()), hash.size());
    if (input.take(hash.size()) != expected)
	throw std::runtime_error("embedded history does not match the current model");

    const auto saved_head = input.number();
    std::vector<Blob> records = {nullptr};
    const auto record_count = input.number();
    for (uint64_t i = 0; i < record_count; ++i) {
	const auto object = input.field();
	const auto record = make_record(unpack_record(object), object);
	if (record->name == history_name)
	    throw std::runtime_error("history contains its own reserved record");
	records.push_back(record);
    }

    const auto revision_count = input.number();
    for (uint64_t i = 0; i < revision_count; ++i) {
	revisions.push_back(decode_revision(input, revisions.size(), [&](Cursor &encoded) {
	    const auto index = encoded.number();
	    if (index >= records.size())
		throw std::runtime_error("invalid history record number");
	    return records[index];
	}));
    }
    if (!input.bytes.empty() || saved_head >= revisions.size())
	throw std::runtime_error("invalid embedded history ending");
    head = saved_head;
}

std::string_view
digest_bytes(const Digest &id)
{
    return {reinterpret_cast<const char *>(id.data()), id.size()};
}

std::string
store_key(char table, std::string_view suffix)
{
    return std::string(1, table).append(suffix);
}

std::string
revision_key(uint64_t revision)
{
    std::string key("r");
    append_number(key, revision);
    return key;
}

void
check_mdb(int status)
{
    if (status != MDB_SUCCESS)
	throw std::runtime_error(std::string("LMDB: ") + mdb_strerror(status));
}

MDB_val
mdb_value(std::string_view bytes)
{
    return {bytes.size(), const_cast<char *>(bytes.data())};
}

class Transaction {
public:
    Transaction(MDB_env *env, unsigned int flags) : txn(nullptr, &mdb_txn_abort)
    {
	MDB_txn *handle = nullptr;
	check_mdb(mdb_txn_begin(env, nullptr, flags, &handle));
	txn.reset(handle);
	check_mdb(mdb_dbi_open(txn.get(), nullptr, 0, &db));
    }

    std::optional<std::string_view> find(std::string_view key) const
    {
	auto k = mdb_value(key);
	MDB_val value;
	const int status = mdb_get(txn.get(), db, &k, &value);
	if (status == MDB_NOTFOUND)
	    return std::nullopt;
	check_mdb(status);
	return std::string_view(static_cast<const char *>(value.mv_data), value.mv_size);
    }

    std::string_view get(std::string_view key) const
    {
	const auto value = find(key);
	if (!value)
	    throw std::runtime_error("missing LMDB repository entry");
	return *value;
    }

    void put(std::string_view key, std::string_view value)
    {
	auto k = mdb_value(key);
	auto v = mdb_value(value);
	check_mdb(mdb_put(txn.get(), db, &k, &v, 0));
    }

    void erase(std::string_view key)
    {
	auto k = mdb_value(key);
	check_mdb(mdb_del(txn.get(), db, &k, nullptr));
    }

    template <typename Visit>
    void scan(char table, Visit visit) const
    {
	MDB_cursor *handle = nullptr;
	check_mdb(mdb_cursor_open(txn.get(), db, &handle));
	const std::unique_ptr<MDB_cursor, decltype(&mdb_cursor_close)> cursor(handle, &mdb_cursor_close);
	auto k = mdb_value(std::string_view(&table, 1));
	MDB_val v;
	int status = mdb_cursor_get(handle, &k, &v, MDB_SET_RANGE);
	while (status == MDB_SUCCESS) {
	    const std::string_view key(static_cast<const char *>(k.mv_data), k.mv_size);
	    if (key.empty() || key.front() != table)
		return;
	    visit(key.substr(1), std::string_view(static_cast<const char *>(v.mv_data), v.mv_size));
	    status = mdb_cursor_get(handle, &k, &v, MDB_NEXT);
	}
	if (status != MDB_NOTFOUND)
	    check_mdb(status);
    }

    void commit()
    {
	check_mdb(mdb_txn_commit(txn.release()));
    }

private:
    std::unique_ptr<MDB_txn, decltype(&mdb_txn_abort)> txn;
    MDB_dbi db = 0;
};

constexpr std::string_view repository_key = "metadata";
constexpr std::string_view repository_magic = "BRLGLMDB1";
constexpr char directory_table = 'd';
constexpr char blob_table = 'b';

class Repository {
public:
    explicit Repository(const fs::path &path) : env(nullptr, &mdb_env_close)
    {
	MDB_env *handle = nullptr;
	check_mdb(mdb_env_create(&handle));
	env.reset(handle);
	// The prototype leaves sizing policy to a production implementation.
	constexpr size_t map_size = 512 * 1024 * 1024;
	check_mdb(mdb_env_set_mapsize(handle, map_size));
	check_mdb(mdb_env_open(handle, path.string().c_str(), 0, 0664));
    }

    std::pair<std::string, History> load() const
    {
	const Transaction txn(env.get(), MDB_RDONLY);
	Cursor metadata{txn.get(repository_key)};
	if (metadata.take(repository_magic.size()) != repository_magic)
	    throw std::runtime_error("unsupported LMDB repository format");
	std::string header(metadata.take(db5_alignment));
	if (!db5_header_is_valid(reinterpret_cast<const unsigned char *>(header.data())))
	    throw std::runtime_error("invalid LMDB repository DB5 header");

	History history({});
	history.head = metadata.number();
	const uint64_t count = metadata.number();
	if (!metadata.bytes.empty() || history.head > count)
	    throw std::runtime_error("invalid LMDB repository head");

	std::map<std::string_view, Blob> records;
	auto get_record = [&](std::string_view id) {
	    const auto known = records.find(id);
	    if (known != records.end())
		return known->second;
	    const auto bytes = txn.get(store_key(blob_table, id));
	    const auto blob = make_record(unpack_record(bytes), bytes);
	    if (id != digest_bytes(blob->id) || blob->name == history_name)
		throw std::runtime_error("invalid LMDB repository object");
	    // Cursor keys and values stay mapped for the lifetime of this read transaction.
	    return records.emplace(id, blob).first->second;
	};

	txn.scan(directory_table, [&](std::string_view key, std::string_view id) {
	    const auto blob = get_record(id);
	    if (key != digest_bytes(digest(blob->name)) ||
		!history.directory.emplace(blob->name, blob).second)
		throw std::runtime_error("invalid LMDB repository directory entry");
	});
	if (history.directory.count(DB5_GLOBAL_OBJECT_NAME) == 0)
	    throw std::runtime_error("missing LMDB repository _GLOBAL record");

	while (history.revisions.size() <= count) {
	    Cursor encoded{txn.get(revision_key(history.revisions.size()))};
	    history.revisions.push_back(decode_revision(encoded, history.revisions.size(),
		[&](Cursor &input) {
		    const auto id = input.field(1);
		    return id.empty() ? nullptr : get_record(id);
		}));
	    if (!encoded.bytes.empty())
		throw std::runtime_error("invalid LMDB revision ending");
	}
	return {std::move(header), std::move(history)};
    }

    void save(std::string_view header, const SavePoint *previous, const History &history)
    {
	Transaction txn(env.get(), 0);
	const auto saved = txn.find(repository_key);
	if (previous ? (!saved || *saved != encode_metadata(header, previous->head,
	    previous->revision_count)) : saved.has_value())
	    throw std::runtime_error("LMDB repository changed during this session");

	auto store_record = [&](const Blob &blob) {
	    const auto key = store_key(blob_table, digest_bytes(blob->id));
	    const auto stored = txn.find(key);
	    if (!stored)
		txn.put(key, blob->bytes);
	    else if (*stored != blob->bytes)
		throw std::runtime_error("LMDB object hash collision");
	};

	auto update_directory = [&](const std::string &name, const Change &change) {
	    const auto key = store_key(directory_table, digest_bytes(digest(name)));
	    const auto value = txn.find(key);
	    if (change.before ? (!value || *value != digest_bytes(change.before->id)) : value.has_value())
		throw std::runtime_error("LMDB object name hash collision or mismatched directory");
	    if (!change.after) {
		txn.erase(key);
		return;
	    }
	    store_record(change.after);
	    txn.put(key, digest_bytes(change.after->id));
	};
	if (previous) {
	    const auto changes = compare(previous->directory, history.directory);
	    for (const auto &entry : changes)
		update_directory(entry.first, entry.second);
	} else {
	    for (const auto &entry : history.directory)
		update_directory(entry.first, {nullptr, entry.second});
	}

	const size_t first = previous ? previous->revision_count : 1;
	for (size_t i = first; i < history.revisions.size(); ++i) {
	    const auto bytes = encode_revision(history.revisions[i], [&](std::string &output, const Blob &blob) {
		if (blob) {
		    store_record(blob);
		    output.push_back(static_cast<char>(blob->id.size()));
		    output.append(digest_bytes(blob->id));
		} else {
		    output.push_back('\0');
		}
	    });
	    txn.put(revision_key(i), bytes);
	}

	txn.put(repository_key, encode_metadata(header, history.head, history.revisions.size()));
	txn.commit();
    }

private:
    static std::string encode_metadata(std::string_view header, uint64_t head, size_t count)
    {
	std::string output(repository_magic);
	output.append(header);
	append_number(output, head);
	append_number(output, count - 1);
	return output;
    }

    std::unique_ptr<MDB_env, decltype(&mdb_env_close)> env;
};

class StagingDirectory {
public:
    explicit StagingDirectory(const fs::path &destination)
    {
	Digest id;
	std::array<uint8_t, 37> name;
	bu_uuid_create(id.data(), 0, nullptr, nullptr);
	bu_uuid_encode(id.data(), name.data());
	path = destination.parent_path() / (std::string(".gversion-") +
	    reinterpret_cast<const char *>(name.data()));
	if (!fs::create_directory(path))
	    throw std::runtime_error("cannot create a staging directory");
    }

    ~StagingDirectory()
    {
	bu_file_delete((path / "edit.g").string().c_str());
	bu_file_delete((path / "output.g").string().c_str());
	bu_file_delete(path.string().c_str());
    }

    fs::path path;
};

uint64_t
revision_number(const std::string &argument)
{
    uint64_t revision = 0;
    const char *end = argument.data() + argument.size();
    const auto parsed = std::from_chars(argument.data(), end, revision);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
	throw std::runtime_error("expected a revision number");
    return revision;
}

void
run_edit(History &history, const std::string &header, const fs::path &path,
    const std::vector<std::string> &words, const std::string &label)
{
    static const std::array<std::string_view, 17> commands = {
	"adjust", "attr", "comb", "g", "get", "hide", "in", "kill", "killtree",
	"ls", "make", "mv", "mvall", "title", "unhide", "units", "tops"
    };
    if (std::find(commands.begin(), commands.end(), words.front()) == commands.end())
	throw std::runtime_error("unsupported prototype command " + words.front());

    write_model(path, header, history.directory);
    std::unique_ptr<struct ged, decltype(&ged_close)> editor(
	ged_open("db", path.string().c_str(), 1), &ged_close);
    if (!editor || editor->dbip->dbi_read_only)
	throw std::runtime_error("could not open the scratch model for editing");

    std::vector<const char *> args;
    for (const auto &word : words)
	args.push_back(word.c_str());
    args.push_back(nullptr);
    const int status = ged_exec(editor.get(), static_cast<int>(words.size()), args.data());
    const std::string result = bu_vls_addr(editor->ged_result_str);
    if (status != BRLCAD_OK)
	throw std::runtime_error("GED command failed: " + result);
    if (!result.empty())
	std::cout << result << '\n';

    editor.reset();
    auto edited = read_model(path, history.directory);
    if (!edited.history.empty())
	throw std::runtime_error("the editor created the reserved history object");
    history.record(label, std::move(edited.objects));
}

void
run_session(History &history, const std::string &header, const fs::path &scratch, Repository *repo)
{
    std::string line;
    while (std::getline(std::cin, line)) {
	std::istringstream input(line);
	std::vector<std::string> words;
	std::string word;
	while (input >> std::ws && !input.eof()) {
	    if (!(input >> std::quoted(word)))
		throw std::runtime_error("invalid command quoting");
	    words.push_back(word);
	}
	if (words.empty() || (!words.front().empty() && words.front().front() == '#'))
	    continue;
	const auto previous = repo
	    ? std::make_optional(SavePoint{history.directory, history.head, history.revisions.size()})
	    : std::nullopt;
	const auto &command = words.front();
	if (command == "undo" && words.size() == 1) {
	    if (history.head == root_revision)
		throw std::runtime_error("already at the initial model");
	    history.checkout(history.revisions[history.head].parent);
	} else if (command == "redo" && words.size() <= 2) {
	    std::vector<uint64_t> children;
	    for (size_t i = 1; i < history.revisions.size(); ++i) {
		if (history.revisions[i].parent == history.head)
		    children.push_back(i);
	    }
	    if (words.size() == 1 && children.size() != 1)
		throw std::runtime_error("redo requires one available child or an explicit child revision");
	    const uint64_t child = words.size() == 1 ? children.front() : revision_number(words[1]);
	    if (std::find(children.begin(), children.end(), child) == children.end())
		throw std::runtime_error("redo target is not a child of the current revision");
	    history.checkout(child);
	} else if (command == "checkout" && words.size() == 2) {
	    history.checkout(revision_number(words[1]));
	} else if (command == "history" && words.size() == 1) {
	    for (size_t i = 0; i < history.revisions.size(); ++i) {
		const auto &revision = history.revisions[i];
		std::cout << (i == history.head ? "* " : "  ") << i << " "
		    << revision.parent << " " << revision.changes.size() << " " << revision.label << '\n';
	    }
	} else {
	    run_edit(history, header, scratch, words, line);
	}
	if (previous && (previous->head != history.head || previous->revision_count != history.revisions.size()))
	    repo->save(header, &*previous, history);
    }
    if (std::cin.bad() || !std::cin.eof())
	throw std::runtime_error("could not read session commands");
}

} // namespace

int
main(int argc, const char *argv[])
{
    bu_setprogname(argv[0]);
    int argument = 1;
    const bool session_only = argument < argc && std::string_view(argv[argument]) == "--session";
    if (session_only)
	++argument;
    fs::path repository_path;
    if (argument + 1 < argc && std::string_view(argv[argument]) == "--lmdb") {
	repository_path = argv[argument + 1];
	argument += 2;
    }
    if (argc != argument + 2) {
	std::cerr << "Usage: gversion [--session] INPUT.g|- OUTPUT.g < commands\n"
	    "       gversion [--session] --lmdb REPOSITORY INPUT.g|- OUTPUT.g|- < commands\n";
	return 1;
    }
    try {
	const bool use_repository = !repository_path.empty();
	const fs::path input(argv[argument]);
	const fs::path destination(argv[argument + 1]);
	const bool export_model = !use_repository || destination != "-";
	if (export_model && fs::exists(destination))
	    throw std::runtime_error("output database already exists");
	StagingDirectory staging(export_model ? destination : repository_path);
	const auto scratch = staging.path / "edit.g";
	std::string header;
	History history({});
	std::unique_ptr<Repository> repo;
	if (use_repository && input == "-") {
	    if (!fs::exists(repository_path / "data.mdb"))
		throw std::runtime_error("LMDB repository does not exist");
	    repo = std::make_unique<Repository>(repository_path);
	    auto loaded = repo->load();
	    header = std::move(loaded.first);
	    history = std::move(loaded.second);
	} else {
	    if (input == "-") {
		struct db_i *database = db_create(scratch.string().c_str(), 5);
		if (!database)
		    throw std::runtime_error("cannot create a DB5 model");
		db_close(database);
	    }
	    auto model = read_model(input == "-" ? scratch : input);
	    header = std::move(model.header);
	    history = History(std::move(model.objects));
	    if (!model.history.empty())
		history.load(model.history);
	    if (use_repository) {
		if (!fs::create_directory(repository_path))
		    throw std::runtime_error("LMDB repository path already exists");
		repo = std::make_unique<Repository>(repository_path);
		repo->save(header, nullptr, history);
	    }
	}

	run_session(history, header, scratch, repo.get());
	if (export_model) {
	    const auto staged = staging.path / "output.g";
	    write_model(staged, header, history.directory,
		session_only ? std::string() : history.serialize());
	    // Publishing a complete file also keeps a concurrently created output intact.
	    fs::create_hard_link(staged, destination);
	}
    } catch (const std::exception &error) {
	std::cerr << "gversion: " << error.what() << '\n';
	return 1;
    }
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
