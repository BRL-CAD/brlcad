/*                 D B _ H I S T O R Y _ C O D E C . H
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
#ifndef LIBRT_DB_HISTORY_CODEC_H
#define LIBRT_DB_HISTORY_CODEC_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rt/db5.h"

struct bu_vls;

namespace db_history {

using Bytes = std::vector<uint8_t>;
using Id = std::array<uint8_t, 32>;
constexpr Id absent{};

struct ByteView {
    const uint8_t *data = nullptr;
    size_t size = 0;

    ByteView() = default;
    ByteView(const uint8_t *bytes, size_t length) : data(bytes), size(length) {}
    ByteView(const Bytes &bytes) : data(bytes.data()), size(bytes.size()) {}
};

enum class Kind : uint8_t {
    Body = 'B',
    Object = 'O',
    Manifest = 'M',
    Revision = 'R'
};

struct RecordArtifact {
    std::string_view name;
    ByteView body;
    Id body_id = absent;
    Id object_id = absent;
    Bytes object;
};

struct Object {
    Id body_id = absent;
    ByteView prefix;
    ByteView suffix;
};

using Manifest = std::map<std::string, Id, std::less<>>;

struct Change {
    Id before = absent;
    Id after = absent;
};

using Changes = std::map<std::string, Change, std::less<>>;
using Renames = std::map<std::string, std::string, std::less<>>;

struct Revision {
    Id parent = absent;
    Id model = absent;
    int64_t time = 0;
    std::string label;
    std::string author;
    Changes changes;
    Renames renames;
};

// Canonical encodings from doc/notes/database_history_format.txt.
RT_EXPORT Id identify(Kind kind, ByteView payload);
RT_EXPORT std::string id_string(const Id &id);

// Factoring borrows the name and body from the supplied DB5 record.
RT_EXPORT bool factor_record(ByteView record, RecordArtifact &result);
// Decoding borrows the prefix and suffix from the object payload.
RT_EXPORT bool decode_object(ByteView payload, Object &result);
// Read DB5 framing and directory metadata without fetching the body.
bool record_metadata(const Object &object, struct db5_raw_internal &raw);
// The artifact store first verifies the body payload against body_id.
RT_EXPORT bool materialize_record(const Object &object, ByteView verified_body, Bytes &result);
// Validate the framing, name and _GLOBAL type of a reconstructed record.
bool valid_named_record(const Object &object, ByteView verified_body, std::string_view name);

RT_EXPORT bool encode_manifest(const Manifest &manifest, Bytes &result);
RT_EXPORT bool decode_manifest(ByteView payload, Manifest &result);
RT_EXPORT bool encode_revision(const Revision &revision, Bytes &result);
RT_EXPORT bool decode_revision(ByteView payload, Revision &result);

// Apply a non-root revision. Require the recorded before and model IDs
// to match. Leave the result untouched on failure.
RT_EXPORT bool apply_revision(const Revision &revision, const Manifest &parent, Manifest &result);
RT_EXPORT Changes diff_manifests(const Manifest &before, const Manifest &after);

using ArtifactKey = std::pair<Kind, Id>;
using ArtifactIndex = std::map<ArtifactKey, ByteView>;

struct Pack {
    Id selected_revision = absent;
    Id selected_model = absent;
    std::set<Id> boundaries;
    std::map<std::string, Id, std::less<>> references;
    std::map<Id, Id> preferences;
    ArtifactIndex artifacts;
};

enum class PackStatus {
    Ok,
    Malformed,
    Unsupported
};

constexpr char archive_name[] = "_BRLCAD_HISTORY";

// Match the reserved name, DB5 wrapper and pack signature.
RT_EXPORT bool archive_payload(const struct db5_raw_internal &raw, ByteView &result);
// The decoded index borrows from the DB5 body. Payload hashes are
// checked by read_artifact when the indexed data is requested.
RT_EXPORT PackStatus read_pack(ByteView body, Pack &result);
RT_EXPORT bool read_artifact(const Pack &pack, const ArtifactKey &key, ByteView &result);
// Stream the validated header, index and payloads to a staging file.
RT_EXPORT bool write_pack(FILE *output, const Pack &pack);
RT_EXPORT bool write_embedded_pack(FILE *output, const Pack &pack);
RT_EXPORT bool read_manifest(const Pack &pack, const Id &id, Manifest &result);
RT_EXPORT bool read_revision(const Pack &pack, const Id &id, Revision &result);
// Replay bounded parent changes from the nearest stored manifest.
RT_EXPORT bool manifest_at_revision(const Pack &pack, const Id &id, Manifest &result);
// The lookup returns raw, borrowed bytes if the key exists. The replay
// checks their hashes and distinguishes a missing manifest from a bad one.
using ArtifactLookup = std::function<bool(const ArtifactKey &, ByteView &)>;
using BoundaryLookup = std::function<bool(const Id &)>;
bool manifest_at_revision(const ArtifactLookup &find, const BoundaryLookup &boundary,
			  const Id &id, Manifest &result);
// Verify payloads, record structure, every retained parent edge and all
// references. Supply an optional diagnostic for the first failure.
RT_EXPORT bool verify_pack(const Pack &pack, struct bu_vls *message = nullptr);

} // namespace db_history

#endif /* LIBRT_DB_HISTORY_CODEC_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
