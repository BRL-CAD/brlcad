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
#include <limits>
#include <set>
#include <utility>

#include "rt/db_io.h"
#include "../libbu/picohash.h"
#include "db_history_codec.h"

namespace db_history {
namespace {

constexpr char hash_domain[] = "BRLCAD-HISTORY";
constexpr std::array<uint8_t, 2> hash_version{0, 1};
constexpr size_t integer_size = sizeof(uint64_t);
constexpr size_t id_size = absent.size();
constexpr size_t min_encoded_name = integer_size + 1;

std::array<uint8_t, integer_size>
integer_bytes(uint64_t value)
{
    std::array<uint8_t, integer_size> bytes;
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

void
append_integer(Bytes &output, uint64_t value)
{
    const auto bytes = integer_bytes(value);
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

    bool integer(uint64_t &value)
    {
	ByteView field;
	if (!take(integer_size, field))
	    return false;
	value = 0;
	for (size_t i = 0; i < field.size; i++)
	    value = (value << 8) | field.data[i];
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

private:
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

    ByteView remaining;
};

struct SplitRecord {
    std::string_view name;
    ByteView prefix;
    ByteView body;
    ByteView suffix;
};

bool
split_record(ByteView record, SplitRecord &split)
{
    struct db5_raw_internal raw;
    const auto *end = db5_get_raw_internal_bounded(&raw, record.data, record.size);
    if (!end || raw.object_length != record.size ||
	raw.h_dli != DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT ||
	!raw.h_name_present || raw.name.ext_nbytes <= 1)
	return false;

    split.name = {reinterpret_cast<const char *>(raw.name.ext_buf), raw.name.ext_nbytes - 1};
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

} // namespace

Id
identify(Kind kind, ByteView payload)
{
    constexpr size_t domain_size = sizeof(hash_domain) - 1;
    constexpr size_t framing_size = domain_size + hash_version.size() + sizeof(uint8_t) + integer_size;
    constexpr uint64_t max_hash_bytes = std::numeric_limits<uint64_t>::max() / 8;
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
materialize_record(const Object &object, ByteView verified_body, Bytes &result)
{
    Bytes record;
    const size_t limit = record.max_size();
    const bool has_body = object.body_id != absent;
    if (has_body != (verified_body.size > 0) || object.prefix.size > limit ||
	verified_body.size > limit - object.prefix.size ||
	object.suffix.size > limit - object.prefix.size - verified_body.size)
	return false;

    record.reserve(object.prefix.size + verified_body.size + object.suffix.size);
    append(record, object.prefix);
    append(record, verified_body);
    append(record, object.suffix);
    SplitRecord split;
    if (!split_record(record, split) || split.prefix.size != object.prefix.size ||
	split.body.size != verified_body.size || split.suffix.size != object.suffix.size)
	return false;

    result = std::move(record);
    return true;
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

bool
apply_revision(const Revision &revision, const Manifest &parent, Manifest &result)
{
    if (revision.parent == absent || !valid_revision(revision))
	return false;

    Manifest model = parent;
    for (const auto &entry : revision.changes) {
	const auto current = model.find(entry.first);
	const Id &before = current != model.end() ? current->second : absent;
	if (before != entry.second.before)
	    return false;
	if (entry.second.after == absent)
	    model.erase(current);
	else
	    model.insert_or_assign(entry.first, entry.second.after);
    }

    Bytes payload;
    if (!encode_manifest(model, payload) || identify(Kind::Manifest, payload) != revision.model)
	return false;

    result = std::move(model);
    return true;
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
