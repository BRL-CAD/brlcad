/*                D B _ H I S T O R Y _ C O D E C . C P P
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
#include <iterator>
#include <limits>
#include <set>
#include <utility>

#include "bu/vls.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "../libbu/picohash.h"
#include "db_history_codec.h"
#include "librt_private.h"

namespace db_history {
namespace {

constexpr char hash_domain[] = "BRLCAD-HISTORY";
constexpr std::array<uint8_t, 2> hash_version{0, 1};
constexpr size_t integer_size = sizeof(uint64_t);
constexpr size_t id_size = absent.size();
constexpr size_t min_encoded_name = integer_size + 1;
constexpr uint64_t max_hash_bytes = std::numeric_limits<uint64_t>::max() / 8;
constexpr std::array<uint8_t, 8> pack_signature{'B', 'R', 'L', 'C', 'A', 'D', 'H', 0};
constexpr uint16_t pack_major = 1;
constexpr uint16_t pack_minor = 0;
constexpr uint64_t pack_required_features = 0;
constexpr size_t max_manifest_distance = 64;
constexpr size_t pack_header_size = pack_signature.size() + 2 * sizeof(uint16_t) + 4 * integer_size + 2 * id_size;
constexpr size_t pack_entry_size = sizeof(uint8_t) + id_size + 2 * integer_size;

template <typename Integer>
std::array<uint8_t, sizeof(Integer)>
integer_bytes(Integer value)
{
    std::array<uint8_t, sizeof(Integer)> bytes;
    for (auto byte = bytes.rbegin(); byte != bytes.rend(); ++byte) {
	*byte = static_cast<uint8_t>(value);
	value >>= 8;
    }
    return bytes;
}

void
append(Bytes &output, ByteView view)
{
    if (view.size)
	output.insert(output.end(), view.data, view.data + view.size);
}

template <typename Integer = uint64_t>
void
append_integer(Bytes &output, uint64_t value)
{
    const auto bytes = integer_bytes(static_cast<Integer>(value));
    append(output, {bytes.data(), bytes.size()});
}

void
append_id(Bytes &output, const Id &id)
{
    append(output, {id.data(), id.size()});
}

void
append_field(Bytes &output, ByteView field)
{
    append_integer(output, field.size);
    append(output, field);
}

void
append_text(Bytes &output, std::string_view text)
{
    append_field(output, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
}

bool
valid_text(std::string_view text)
{
    return text.find('\0') == std::string_view::npos;
}

bool
valid_name(std::string_view name)
{
    return !name.empty() && valid_text(name);
}

class Reader {
public:
    explicit Reader(ByteView input) : remaining(input) {}

    template <typename Integer>
    bool integer(Integer &value)
    {
	ByteView field;
	if (!take(sizeof(value), field))
	    return false;
	value = 0;
	for (size_t i = 0; i < field.size; i++)
	    value = static_cast<Integer>((value << 8) | field.data[i]);
	return true;
    }

    bool id(Id &value)
    {
	ByteView field;
	if (!take(value.size(), field))
	    return false;
	std::copy_n(field.data, field.size, value.begin());
	return true;
    }

    bool field(ByteView &value)
    {
	uint64_t length;
	return integer(length) && take(length, value);
    }

    bool text(std::string_view &value)
    {
	ByteView bytes;
	if (!field(bytes))
	    return false;
	value = bytes.size ? std::string_view(reinterpret_cast<const char *>(bytes.data), bytes.size) : std::string_view{};
	return valid_text(value);
    }

    bool count(uint64_t &value, size_t minimum_item_size)
    {
	return integer(value) && value <= remaining.size / minimum_item_size;
    }

    bool done() const { return !remaining.size; }
    size_t size() const { return remaining.size; }

    bool take(uint64_t length, ByteView &value)
    {
	if (length > remaining.size)
	    return false;
	value = {remaining.data, static_cast<size_t>(length)};
	if (length) {
	    remaining.data += value.size;
	    remaining.size -= value.size;
	}
	return true;
    }

private:
    ByteView remaining;
};

struct SplitRecord {
    std::string_view name;
    ByteView prefix;
    ByteView body;
    ByteView suffix;
};

std::string_view
record_name(const struct db5_raw_internal &raw)
{
    return {reinterpret_cast<const char *>(raw.name.ext_buf), raw.name.ext_nbytes - 1};
}

bool
split_record(ByteView record, SplitRecord &split)
{
    struct db5_raw_internal raw;
    const auto *end = db5_get_raw_internal_bounded(&raw, record.data, record.size);
    if (!end || raw.object_length != record.size ||
	raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT ||
	!raw.h_name_present || raw.name.ext_nbytes <= 1)
	return false;

    split.name = record_name(raw);
    split.prefix = record;
    if (raw.body.ext_nbytes) {
	split.prefix.size = static_cast<size_t>(raw.body.ext_buf - record.data);
	split.body = {raw.body.ext_buf, raw.body.ext_nbytes};
	const size_t suffix_offset = split.prefix.size + split.body.size;
	split.suffix = {record.data + suffix_offset, record.size - suffix_offset};
    }
    return true;
}

bool
usable_global(const struct db5_raw_internal &raw, std::string_view name)
{
    if (name != DB5_GLOBAL_OBJECT_NAME)
	return true;
    if (raw.major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY)
	return false;

    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    const bool valid = db5_import_attributes(&attributes, &raw.attributes) >= 0;
    bu_avs_free(&attributes);
    return valid;
}

bool
valid_revision(const Revision &revision)
{
    const bool is_root = revision.parent == absent;
    if (revision.model == absent || is_root != revision.changes.empty() ||
	!valid_text(revision.label) || !valid_text(revision.author))
	return false;

    for (const auto &entry : revision.changes) {
	if (!valid_name(entry.first) || entry.second.before == entry.second.after)
	    return false;
    }

    std::set<std::string_view> destinations;
    for (const auto &rename : revision.renames) {
	const auto from = revision.changes.find(rename.first);
	const auto to = revision.changes.find(rename.second);
	if (from == revision.changes.end() || to == revision.changes.end() ||
	    from->second.after != absent || to->second.before != absent ||
	    !destinations.insert(rename.second).second)
	    return false;
    }

    return true;
}

Id
checksum(ByteView input)
{
    _picohash_sha256_ctx_t hash;
    _picohash_sha256_init(&hash);
    _picohash_sha256_update(&hash, input.data, input.size);
    Id id;
    _picohash_sha256_final(&hash, id.data());
    return id;
}

bool
artifact_kind(Kind kind)
{
    return kind == Kind::Body || kind == Kind::Object || kind == Kind::Manifest || kind == Kind::Revision;
}

bool
valid_pack_references(const Pack &pack)
{
    auto has_revision = [&pack](const Id &id) {
	return id != absent && pack.artifacts.count({Kind::Revision, id});
    };
    if (!has_revision(pack.selected_revision) || pack.selected_model == absent ||
	!pack.artifacts.count({Kind::Manifest, pack.selected_model}))
	return false;
    for (const auto &id : pack.boundaries)
	if (!has_revision(id))
	    return false;
    for (const auto &entry : pack.references)
	if (!valid_name(entry.first) || !has_revision(entry.second))
	    return false;
    for (const auto &entry : pack.preferences)
	if (entry.first == entry.second || !has_revision(entry.first) || !has_revision(entry.second))
	    return false;
    return true;
}

bool
decode_pack_metadata(ByteView bytes, Pack &pack)
{
    Reader reader(bytes);
    uint64_t count;
    if (!reader.count(count, id_size))
	return false;
    Id previous = absent;
    for (uint64_t i = 0; i < count; i++) {
	Id id;
	if (!reader.id(id) || id <= previous)
	    return false;
	pack.boundaries.emplace_hint(pack.boundaries.end(), id);
	previous = id;
    }
    if (!reader.count(count, min_encoded_name + id_size))
	return false;
    std::string_view previous_name;
    for (uint64_t i = 0; i < count; i++) {
	std::string_view name;
	Id id;
	if (!reader.text(name) || name <= previous_name || !reader.id(id) || id == absent)
	    return false;
	pack.references.emplace_hint(pack.references.end(), name, id);
	previous_name = name;
    }
    if (!reader.count(count, 2 * id_size))
	return false;
    previous = absent;
    for (uint64_t i = 0; i < count; i++) {
	Id parent, child;
	if (!reader.id(parent) || parent <= previous || !reader.id(child) || child == absent)
	    return false;
	pack.preferences.emplace_hint(pack.preferences.end(), parent, child);
	previous = parent;
    }
    return reader.done();
}

Bytes
encode_pack_metadata(const Pack &pack)
{
    Bytes bytes;
    append_integer(bytes, pack.boundaries.size());
    for (const auto &id : pack.boundaries)
	append_id(bytes, id);
    append_integer(bytes, pack.references.size());
    for (const auto &entry : pack.references) {
	append_text(bytes, entry.first);
	append_id(bytes, entry.second);
    }
    append_integer(bytes, pack.preferences.size());
    for (const auto &entry : pack.preferences) {
	append_id(bytes, entry.first);
	append_id(bytes, entry.second);
    }
    return bytes;
}

bool
write_payload(FILE *output, ByteView payload)
{
    return !payload.size || fwrite(payload.data, 1, payload.size, output) == payload.size;
}

struct PackStream {
    const Pack &pack;
    ByteView header;
};

int
write_pack_stream(FILE *output, void *data)
{
    const auto &stream = *static_cast<const PackStream *>(data);
    if (!write_payload(output, stream.header))
	return -1;
    for (const auto &artifact : stream.pack.artifacts)
	if (!write_payload(output, artifact.second))
	    return -1;
    return 0;
}

} // namespace

Id
identify(Kind kind, ByteView payload)
{
    constexpr size_t domain_size = sizeof(hash_domain) - 1;
    constexpr size_t framing_size = domain_size + hash_version.size() + sizeof(uint8_t) + integer_size;
    if (payload.size > max_hash_bytes - framing_size)
	return absent;

    const auto tag = static_cast<uint8_t>(kind);
    const auto length = integer_bytes(payload.size);
    _picohash_sha256_ctx_t hash;
    _picohash_sha256_init(&hash);
    _picohash_sha256_update(&hash, hash_domain, domain_size);
    _picohash_sha256_update(&hash, hash_version.data(), hash_version.size());
    _picohash_sha256_update(&hash, &tag, sizeof(tag));
    _picohash_sha256_update(&hash, length.data(), length.size());
    _picohash_sha256_update(&hash, payload.data, payload.size);
    Id id;
    _picohash_sha256_final(&hash, id.data());
    return id;
}

std::string
id_string(const Id &id)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(2 * id.size());
    for (const uint8_t byte : id) {
	hex += digits[byte >> 4];
	hex += digits[byte & 0xf];
    }
    return hex;
}

bool
factor_record(ByteView record, RecordArtifact &result)
{
    SplitRecord split;
    if (!split_record(record, split))
	return false;

    RecordArtifact factored;
    factored.name = split.name;
    factored.body = split.body;
    if (split.body.size) {
	factored.body_id = identify(Kind::Body, split.body);
	if (factored.body_id == absent)
	    return false;
    }
    append_id(factored.object, factored.body_id);
    append_field(factored.object, split.prefix);
    append_field(factored.object, split.suffix);
    factored.object_id = identify(Kind::Object, factored.object);
    if (factored.object_id == absent)
	return false;
    result = std::move(factored);
    return true;
}

bool
decode_object(ByteView payload, Object &result)
{
    Reader reader(payload);
    Object object;
    if (!reader.id(object.body_id) || !reader.field(object.prefix) || !reader.field(object.suffix) ||
	!reader.done() || !object.prefix.size)
	return false;
    const bool has_body = object.body_id != absent;
    if (has_body != (object.suffix.size > 0))
	return false;

    result = object;
    return true;
}

bool
record_metadata(const Object &object, struct db5_raw_internal &raw)
{
    if (object.body_id == absent) {
	if (object.suffix.size || !db5_get_raw_internal_bounded(&raw, object.prefix.data, object.prefix.size) ||
	    raw.object_length != object.prefix.size || raw.body.ext_nbytes)
	    return false;
    } else if (db5_get_raw_internal_split(&raw, object.prefix.data, object.prefix.size,
	object.suffix.data, object.suffix.size) < 0) {
	return false;
    }
    return raw.h_dli == DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT && raw.h_name_present && raw.name.ext_nbytes > 1;
}

bool
materialize_record(const Object &object, ByteView verified_body, Bytes &result)
{
    Bytes record;
    struct db5_raw_internal raw;
    if (!record_metadata(object, raw) || verified_body.size != raw.body.ext_nbytes ||
	raw.object_length > record.max_size())
	return false;

    record.reserve(raw.object_length);
    append(record, object.prefix);
    append(record, verified_body);
    append(record, object.suffix);
    result = std::move(record);
    return true;
}

bool
valid_named_record(const Object &object, ByteView verified_body, std::string_view name)
{
    struct db5_raw_internal raw;
    return record_metadata(object, raw) && verified_body.size == raw.body.ext_nbytes &&
	record_name(raw) == name && usable_global(raw, name);
}

bool
encode_manifest(const Manifest &manifest, Bytes &result)
{
    Bytes payload;
    append_integer(payload, manifest.size());
    for (const auto &entry : manifest) {
	if (!valid_name(entry.first) || entry.second == absent)
	    return false;
	append_text(payload, entry.first);
	append_id(payload, entry.second);
    }

    result = std::move(payload);
    return true;
}

bool
decode_manifest(ByteView payload, Manifest &result)
{
    Reader reader(payload);
    uint64_t count;
    if (!reader.count(count, min_encoded_name + id_size))
	return false;

    Manifest manifest;
    std::string_view previous;
    for (uint64_t i = 0; i < count; i++) {
	std::string_view name;
	Id object;
	if (!reader.text(name) || name <= previous || !reader.id(object) || object == absent)
	    return false;
	manifest.emplace_hint(manifest.end(), name, object);
	previous = name;
    }
    if (!reader.done())
	return false;

    result = std::move(manifest);
    return true;
}

bool
encode_revision(const Revision &revision, Bytes &result)
{
    if (!valid_revision(revision))
	return false;

    Bytes payload;
    append_id(payload, revision.parent);
    append_id(payload, revision.model);
    append_integer(payload, static_cast<uint64_t>(revision.time));
    append_text(payload, revision.label);
    append_text(payload, revision.author);
    append_integer(payload, revision.changes.size());
    for (const auto &entry : revision.changes) {
	append_text(payload, entry.first);
	append_id(payload, entry.second.before);
	append_id(payload, entry.second.after);
    }
    append_integer(payload, revision.renames.size());
    for (const auto &rename : revision.renames) {
	append_text(payload, rename.first);
	append_text(payload, rename.second);
    }

    result = std::move(payload);
    return true;
}

bool
decode_revision(ByteView payload, Revision &result)
{
    Reader reader(payload);
    uint64_t time;
    uint64_t count;
    std::string_view label, author;
    Revision revision;
    if (!reader.id(revision.parent) || !reader.id(revision.model) || !reader.integer(time) ||
	!reader.text(label) || !reader.text(author) ||
	!reader.count(count, min_encoded_name + 2 * id_size))
	return false;

    // Convert the signed wire value without relying on an overflowing cast.
    revision.time = time <= INT64_MAX ? static_cast<int64_t>(time) : -1 - static_cast<int64_t>(UINT64_MAX - time);
    revision.label = label;
    revision.author = author;
    std::string_view previous;
    for (uint64_t i = 0; i < count; i++) {
	std::string_view name;
	Change change;
	if (!reader.text(name) || name <= previous || !reader.id(change.before) || !reader.id(change.after))
	    return false;
	revision.changes.emplace_hint(revision.changes.end(), name, change);
	previous = name;
    }

    if (!reader.count(count, 2 * min_encoded_name))
	return false;

    previous = {};
    for (uint64_t i = 0; i < count; i++) {
	std::string_view from, to;
	if (!reader.text(from) || from <= previous || !reader.text(to) || to.empty())
	    return false;
	revision.renames.emplace_hint(revision.renames.end(), from, to);
	previous = from;
    }
    if (!reader.done() || !valid_revision(revision))
	return false;

    result = std::move(revision);
    return true;
}

static bool
apply_changes(Manifest &model, const Changes &changes, bool reverse)
{
    for (const auto &entry : changes) {
	const auto current = model.find(entry.first);
	const Id &existing = current != model.end() ? current->second : absent;
	const Id &before = reverse ? entry.second.after : entry.second.before;
	const Id &after = reverse ? entry.second.before : entry.second.after;
	if (existing != before)
	    return false;
	if (after != absent)
	    model.insert_or_assign(entry.first, after);
	else if (current != model.end())
	    model.erase(current);
    }
    return true;
}

static bool
matches_model(const Manifest &model, const Id &id)
{
    Bytes payload;
    return encode_manifest(model, payload) && identify(Kind::Manifest, payload) == id;
}

bool
apply_revision(const Revision &revision, const Manifest &parent, Manifest &result)
{
    if (revision.parent == absent || !valid_revision(revision))
	return false;
    Manifest model = parent;
    if (!apply_changes(model, revision.changes, false) || !matches_model(model, revision.model))
	return false;

    result = std::move(model);
    return true;
}

Changes
diff_manifests(const Manifest &before, const Manifest &after)
{
    Changes changes;
    for (const auto &previous : before) {
	const auto current = after.find(previous.first);
	const Id &id = current == after.end() ? absent : current->second;
	if (id != previous.second)
	    changes.emplace(previous.first, Change{previous.second, id});
    }
    for (const auto &current : after)
	if (!before.count(current.first))
	    changes.emplace(current.first, Change{absent, current.second});
    return changes;
}

bool
archive_payload(const struct db5_raw_internal &raw, ByteView &result)
{
    if (raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT || !raw.h_name_present ||
	!raw.h_name_hidden || raw.major_type != DB5_MAJORTYPE_BINARY_UNIF ||
	raw.minor_type != DB5_MINORTYPE_BINU_8BITINT_U || !raw.b_present ||
	raw.b_zzz != DB5_ZZZ_UNCOMPRESSED || raw.name.ext_nbytes != sizeof(archive_name) ||
	!std::equal(std::begin(archive_name), std::end(archive_name), raw.name.ext_buf) ||
	raw.body.ext_nbytes < pack_signature.size() ||
	!std::equal(pack_signature.begin(), pack_signature.end(), raw.body.ext_buf))
	return false;
    result = {raw.body.ext_buf, raw.body.ext_nbytes};
    return true;
}

PackStatus
read_pack(ByteView body, Pack &result)
{
    Reader reader(body);
    ByteView signature;
    uint16_t major, minor;
    uint64_t required_features;
    if (!reader.take(pack_signature.size(), signature) ||
	!std::equal(pack_signature.begin(), pack_signature.end(), signature.data) ||
	!reader.integer(major) || !reader.integer(minor) || !reader.integer(required_features))
	return PackStatus::Malformed;
    if (major != pack_major || minor != pack_minor || required_features != pack_required_features)
	return PackStatus::Unsupported;

    Pack pack;
    uint64_t total, metadata_length, count;
    ByteView metadata;
    if (!reader.integer(total) || total != body.size || !reader.id(pack.selected_revision) ||
	!reader.id(pack.selected_model) || !reader.integer(metadata_length) || !reader.integer(count) ||
	!reader.take(metadata_length, metadata) || reader.size() < id_size ||
	count > (reader.size() - id_size) / pack_entry_size)
	return PackStatus::Malformed;

    // Check the index and metadata together before trusting offsets or
    // allocating tables. Large payloads are hashed on demand.
    const size_t index_end = body.size - reader.size() + static_cast<size_t>(count) * pack_entry_size;
    if (index_end > max_hash_bytes)
	return PackStatus::Malformed;
    const Id digest = checksum({body.data, index_end});
    if (!std::equal(digest.begin(), digest.end(), body.data + index_end) ||
	!decode_pack_metadata(metadata, pack))
	return PackStatus::Malformed;

    size_t next_offset = index_end + id_size;
    for (uint64_t i = 0; i < count; i++) {
	uint8_t tag;
	Id id;
	uint64_t offset, length;
	if (!reader.integer(tag) || !reader.id(id) || id == absent ||
	    !reader.integer(offset) || !reader.integer(length) ||
	    offset != next_offset || !length || length > body.size - next_offset)
	    return PackStatus::Malformed;
	const ArtifactKey key{static_cast<Kind>(tag), id};
	if (!artifact_kind(key.first) || (!pack.artifacts.empty() && key <= pack.artifacts.rbegin()->first))
	    return PackStatus::Malformed;
	const auto size = static_cast<size_t>(length);
	pack.artifacts.emplace_hint(pack.artifacts.end(), key, ByteView{body.data + next_offset, size});
	next_offset += size;
    }
    if (next_offset != body.size || !valid_pack_references(pack))
	return PackStatus::Malformed;

    result = std::move(pack);
    return PackStatus::Ok;
}

bool
read_artifact(const Pack &pack, const ArtifactKey &key, ByteView &result)
{
    const auto artifact = pack.artifacts.find(key);
    if (artifact == pack.artifacts.end() || key.second == absent || !artifact_kind(key.first) ||
	!artifact->second.size || identify(key.first, artifact->second) != key.second)
	return false;
    result = artifact->second;
    return true;
}

bool
read_manifest(const Pack &pack, const Id &id, Manifest &result)
{
    ByteView payload;
    return read_artifact(pack, {Kind::Manifest, id}, payload) && decode_manifest(payload, result);
}

bool
read_revision(const Pack &pack, const Id &id, Revision &result)
{
    ByteView payload;
    return read_artifact(pack, {Kind::Revision, id}, payload) && decode_revision(payload, result);
}

bool
manifest_at_revision(const ArtifactLookup &find, const BoundaryLookup &boundary, const Id &id, Manifest &result)
{
    std::vector<Revision> steps;
    Id current = id;
    Manifest manifest;
    for (;;) {
	Revision revision;
	ByteView payload;
	if (current == absent || !find({Kind::Revision, current}, payload) ||
	    identify(Kind::Revision, payload) != current || !decode_revision(payload, revision))
	    return false;
	if (find({Kind::Manifest, revision.model}, payload)) {
	    if (identify(Kind::Manifest, payload) != revision.model || !decode_manifest(payload, manifest))
		return false;
	    break;
	}
	if (steps.size() == max_manifest_distance || revision.parent == absent || boundary(current))
	    return false;
	current = revision.parent;
	steps.push_back(std::move(revision));
    }
    for (auto step = steps.rbegin(); step != steps.rend(); ++step)
	if (!apply_revision(*step, manifest, manifest))
	    return false;
    result = std::move(manifest);
    return true;
}

bool
manifest_at_revision(const Pack &pack, const Id &id, Manifest &result)
{
    return manifest_at_revision([&](const ArtifactKey &key, ByteView &payload) {
	const auto artifact = pack.artifacts.find(key);
	if (artifact == pack.artifacts.end())
	    return false;
	payload = artifact->second;
	return true;
    }, [&](const Id &revision) { return pack.boundaries.count(revision); }, id, result);
}

namespace {

using Names = std::map<Id, std::string>;

bool
verify_failure(struct bu_vls *message, const char *reason)
{
    if (message)
	bu_vls_strcat(message, reason);
    return false;
}

bool
has_object_name(const Names &names, const Id &id, const std::string &expected)
{
    const auto found = names.find(id);
    return found != names.end() && found->second == expected;
}

bool
complete_model(const Manifest &model, const Names &names)
{
    if (!model.count(DB5_GLOBAL_OBJECT_NAME) || model.count(archive_name))
	return false;
    for (const auto &entry : model)
	if (!has_object_name(names, entry.second, entry.first))
	    return false;
    return true;
}

struct RevisionNode {
    Revision revision;
    std::vector<const RevisionNode *> children;
    bool snapshot = false;
};

bool
verify_tree(const Pack &pack, const RevisionNode &root, size_t &visited, struct bu_vls *message)
{
    Manifest model;
    const auto raw = pack.artifacts.find({Kind::Manifest, root.revision.model});
    if (raw == pack.artifacts.end() || !decode_manifest(raw->second, model))
	return verify_failure(message, "The initial revision or retention boundary has no usable snapshot");
    for (const auto &change : root.revision.changes) {
	const auto object = model.find(change.first);
	const auto &found = object == model.end() ? absent : object->second;
	if (found != change.second.after)
	    return verify_failure(message, "A retention boundary disagrees with its recorded changes");
    }

    // There is only one parent per node. A depth-first traversal can
    // reuse one model and reverse each child's change on the way back.
    struct Visit {
	const RevisionNode *node;
	size_t next_child;
	size_t distance;
    };
    std::vector<Visit> stack{{&root, 0, 0}};
    ++visited;
    while (!stack.empty()) {
	auto &visit = stack.back();
	if (visit.next_child == visit.node->children.size()) {
	    if (stack.size() > 1 && !apply_changes(model, visit.node->revision.changes, true))
		return verify_failure(message, "A recorded change could not be reversed");
	    stack.pop_back();
	    continue;
	}
	const auto *child = visit.node->children[visit.next_child++];
	const size_t distance = child->snapshot ? 0 : visit.distance + 1;
	if (distance > max_manifest_distance)
	    return verify_failure(message, "A revision exceeds the snapshot interval");
	if (!apply_changes(model, child->revision.changes, false) ||
	    !model.count(DB5_GLOBAL_OBJECT_NAME) || !matches_model(model, child->revision.model))
	    return verify_failure(message, "A revision does not reconstruct from its recorded parent");
	++visited;
	stack.push_back({child, 0, distance});
    }
    return true;
}

} // namespace

bool
verify_pack(const Pack &pack, struct bu_vls *message)
{
    if (message)
	bu_vls_trunc(message, 0);
    if (!valid_pack_references(pack))
	return verify_failure(message, "The selected head, a mark or a retained reference is missing");

    Names names;
    std::map<Id, RevisionNode> revisions;
    for (const auto &entry : pack.artifacts) {
	const auto &key = entry.first;
	const auto &payload = entry.second;
	if (key.second == absent || !artifact_kind(key.first) || !payload.size || identify(key.first, payload) != key.second)
	    return verify_failure(message, "An archived artifact does not match its ID");
	if (key.first == Kind::Object) {
	    Object object;
	    if (!decode_object(payload, object))
		return verify_failure(message, "An archived object descriptor is malformed");
	    ByteView body;
	    if (object.body_id != absent) {
		const auto found = pack.artifacts.find({Kind::Body, object.body_id});
		if (found == pack.artifacts.end())
		    return verify_failure(message, "An archived object is missing its body");
		body = found->second;
	    }
	    struct db5_raw_internal raw;
	    if (!record_metadata(object, raw) || body.size != raw.body.ext_nbytes || !usable_global(raw, record_name(raw)))
		return verify_failure(message, "An archived object does not reconstruct a valid DB5 record");
	    names.emplace(key.second, record_name(raw));
	}
	if (key.first == Kind::Revision) {
	    auto &node = revisions[key.second];
	    if (!decode_revision(payload, node.revision))
		return verify_failure(message, "An archived revision is malformed");
	    node.snapshot = pack.artifacts.count({Kind::Manifest, node.revision.model});
	}
    }

    for (const auto &entry : pack.artifacts) {
	if (entry.first.first != Kind::Manifest)
	    continue;
	Manifest model;
	if (!decode_manifest(entry.second, model) || !complete_model(model, names))
	    return verify_failure(message, "An archived snapshot is incomplete or has invalid object references");
    }

    if (revisions.at(pack.selected_revision).revision.model != pack.selected_model)
	return verify_failure(message, "The selected revision does not name the selected model");

    size_t initial = 0;
    std::vector<const RevisionNode *> roots;
    for (auto &entry : revisions) {
	auto &node = entry.second;
	const auto &rev = node.revision;
	for (const auto &change : rev.changes) {
	    if (change.first == archive_name ||
		(change.second.before != absent && !has_object_name(names, change.second.before, change.first)) ||
		(change.second.after != absent && !has_object_name(names, change.second.after, change.first)))
		return verify_failure(message, "A revision change refers to an invalid object");
	}
	const auto parent = revisions.find(rev.parent);
	const bool boundary = pack.boundaries.count(entry.first);
	if (boundary) {
	    if (rev.parent == absent || parent != revisions.end())
		return verify_failure(message, "A retention boundary has an invalid parent");
	    roots.push_back(&node);
	} else if (rev.parent == absent) {
	    if (++initial > 1)
		return verify_failure(message, "The archive has more than one initial revision");
	    roots.push_back(&node);
	} else {
	    if (parent == revisions.end())
		return verify_failure(message, "A revision is missing its recorded parent");
	    parent->second.children.push_back(&node);
	}
    }
    for (const auto &preference : pack.preferences) {
	if (revisions.at(preference.second).revision.parent != preference.first)
	    return verify_failure(message, "A preferred redo revision is not a child of its recorded parent");
    }

    size_t visited = 0;
    for (const auto *root : roots)
	if (!verify_tree(pack, *root, visited, message))
	    return false;
    return visited == revisions.size() || verify_failure(message, "The archive has a cycle or an unreachable revision");
}

static bool
write_pack_file(FILE *output, const Pack &pack, bool embedded)
{
    if (!output || !valid_pack_references(pack))
	return false;

    const auto metadata = encode_pack_metadata(pack);
    if (metadata.size() > max_hash_bytes - pack_header_size ||
	pack.artifacts.size() > (max_hash_bytes - pack_header_size - metadata.size()) / pack_entry_size)
	return false;
    const uint64_t index_end = pack_header_size + metadata.size() +
	static_cast<uint64_t>(pack.artifacts.size()) * pack_entry_size;
    uint64_t total = index_end + id_size;
    for (const auto &artifact : pack.artifacts) {
	const auto &key = artifact.first;
	const auto &payload = artifact.second;
	if (!artifact_kind(key.first) || key.second == absent || !payload.size ||
	    payload.size > UINT64_MAX - total || identify(key.first, payload) != key.second)
	    return false;
	total += payload.size;
    }

    Bytes header;
    if (index_end + id_size > header.max_size())
	return false;
    header.reserve(static_cast<size_t>(index_end + id_size));
    append(header, {pack_signature.data(), pack_signature.size()});
    append_integer<uint16_t>(header, pack_major);
    append_integer<uint16_t>(header, pack_minor);
    append_integer(header, pack_required_features);
    append_integer(header, total);
    append_id(header, pack.selected_revision);
    append_id(header, pack.selected_model);
    append_integer(header, metadata.size());
    append_integer(header, pack.artifacts.size());
    append(header, metadata);

    uint64_t offset = index_end + id_size;
    for (const auto &artifact : pack.artifacts) {
	append_integer<uint8_t>(header, static_cast<uint8_t>(artifact.first.first));
	append_id(header, artifact.first.second);
	append_integer(header, offset);
	append_integer(header, artifact.second.size);
	offset += artifact.second.size;
    }
    append_id(header, checksum(header));

    PackStream stream{pack, header};
    if (embedded)
	return total <= SIZE_MAX && db5_fwrite_hidden_u8(output, archive_name, static_cast<size_t>(total),
		write_pack_stream, &stream) == 0;
    return write_pack_stream(output, &stream) == 0;
}

bool
write_pack(FILE *output, const Pack &pack)
{
    return write_pack_file(output, pack, false);
}

bool
write_embedded_pack(FILE *output, const Pack &pack)
{
    return write_pack_file(output, pack, true);
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
