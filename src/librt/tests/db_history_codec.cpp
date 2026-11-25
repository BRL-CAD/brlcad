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

#include "bu/app.h"
#include "bu/log.h"
#include "rt/db_io.h"
#include "../db_history_codec.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("History codec test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

using namespace db_history;

static std::string
hex_id(const Id &id)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    for (uint8_t byte : id) {
	hex.push_back(digits[byte >> 4]);
	hex.push_back(digits[byte & 0xf]);
    }
    return hex;
}

static int
verify_vectors()
{
    // Independent fixtures in doc/notes/database_history_format.txt.
    const Bytes original{0x76, 0x24, 0x00, 0x20, 0x01, 0xf1, 0x02, 0x02,
	0x5a, 0x00, 0x03, 0xde, 0xad, 0xbe, 0xa5, 0x35};
    Bytes renamed = original;
    constexpr size_t name_offset = 8;
    renamed[name_offset] = 'z';
    RecordArtifact first, second;
    CHECK(factor_record(original, first) && first.name == "Z");
    CHECK(factor_record(renamed, second) && second.name == "z");
    CHECK(first.body_id == second.body_id);
    CHECK(hex_id(first.body_id) == "0fcc049939475cc8c22d8c24ea6e10edd14efb81af36dc29341a2594af20b79f");
    CHECK(hex_id(first.object_id) == "94928ae61fc307cbff2da1a8e933dd070e840ebc2674984180eda38fdb97fe1f");
    CHECK(hex_id(second.object_id) == "ee70ece995638d2f78a849516b9201c8b1398050ff1d216f4091c1d0d9835a60");

    Object object;
    Bytes restored;
    CHECK(decode_object(first.object, object) && object.body_id == first.body_id);
    CHECK(materialize_record(object, first.body, restored) && restored == original);
    CHECK(!materialize_record(object, {}, restored) && restored == original);
    // Keep the reconstruction bytes intact while moving the encoded
    // split, using a contiguous view of the original record prefix.
    const Object wrong_split{object.body_id, {original.data(), object.prefix.size + 1}, object.suffix};
    CHECK(!materialize_record(wrong_split, {first.body.data + 1, first.body.size - 1}, restored));
    for (size_t size = 0; size < first.object.size(); size++)
	CHECK(!decode_object({first.object.data(), size}, object));
    for (size_t size = 0; size < original.size(); size++)
	CHECK(!factor_record({original.data(), size}, first));

    const Manifest initial_model{{"Z", first.object_id}};
    const Manifest renamed_model{{"z", second.object_id}};
    Bytes initial_bytes, renamed_bytes;
    CHECK(encode_manifest(initial_model, initial_bytes) && encode_manifest(renamed_model, renamed_bytes));
    const Id initial_model_id = identify(Kind::Manifest, initial_bytes);
    const Id renamed_model_id = identify(Kind::Manifest, renamed_bytes);
    CHECK(hex_id(initial_model_id) == "86bc6e1b69433e36bd3780d3b7c9336cc60b3a164b622de3ebb37aa98f801b46");
    CHECK(hex_id(renamed_model_id) == "ff12b2da19962c7811252abf45c752bc4dba70d48666e4fa445cb48e83b5e6e3");
    Manifest decoded_model;
    CHECK(decode_manifest(initial_bytes, decoded_model) && decoded_model == initial_model);

    const Manifest combined{{"z", second.object_id}, {"Z", first.object_id}};
    Bytes invalid;
    CHECK(encode_manifest(combined, invalid));
    constexpr size_t first_name = 2 * sizeof(uint64_t);
    constexpr size_t second_name = first_name + 1 + absent.size() + sizeof(uint64_t);
    CHECK(invalid[first_name] == 'Z' && invalid[second_name] == 'z');
    std::swap(invalid[first_name], invalid[second_name]);
    CHECK(!decode_manifest(invalid, decoded_model) && decoded_model == initial_model);
    invalid.front() = 0xff;
    CHECK(!decode_manifest(invalid, decoded_model));

    Revision root;
    root.model = initial_model_id;
    root.label = "root";
    Bytes root_bytes;
    CHECK(encode_revision(root, root_bytes));
    const Id root_id = identify(Kind::Revision, root_bytes);
    CHECK(hex_id(root_id) == "fc4b939be9fd903dd9a1cc036fd6091f63b3533830b3b401d1219e581d27a7cf");

    Revision revision;
    revision.parent = root_id;
    revision.model = renamed_model_id;
    revision.time = -2;
    revision.label = "rename";
    revision.author = "cad";
    revision.changes = {{"Z", {first.object_id, absent}}, {"z", {absent, second.object_id}}};
    revision.renames = {{"Z", "z"}};
    Bytes revision_bytes;
    CHECK(encode_revision(revision, revision_bytes));
    CHECK(hex_id(identify(Kind::Revision, revision_bytes)) == "2bb83e50068fdc03cf1eddcded5edeb95b1fcd3704848d0f3dd45f7724303ee4");
    Revision decoded;
    CHECK(decode_revision(revision_bytes, decoded) && decoded.time == -2);
    CHECK(decoded.parent == root_id && decoded.author == "cad" && decoded.renames == revision.renames);
    Bytes roundtrip;
    CHECK(encode_revision(decoded, roundtrip) && roundtrip == revision_bytes);
    CHECK(apply_revision(decoded, initial_model, decoded_model) && decoded_model == renamed_model);
    CHECK(!apply_revision(decoded, Manifest{}, decoded_model) && decoded_model == renamed_model);
    decoded.model = initial_model_id;
    CHECK(!apply_revision(decoded, initial_model, decoded_model) && decoded_model == renamed_model);
    decoded.renames = {{"z", "Z"}};
    CHECK(!encode_revision(decoded, roundtrip));
    for (size_t size = 0; size < revision_bytes.size(); size++)
	CHECK(!decode_revision({revision_bytes.data(), size}, decoded));
    return 0;
}

static int
verify_body_sharing()
{
    constexpr size_t body_bytes = 1024 * 1024;
    Bytes payload(body_bytes);
    for (size_t i = 0; i < payload.size(); ++i)
	payload[i] = static_cast<uint8_t>(i);
    struct bu_external body = BU_EXTERNAL_INIT_ZERO;
    body.ext_buf = payload.data();
    body.ext_nbytes = payload.size();
    uint8_t attribute_bytes[] = {'k', 0, '0', 0, 0, 0};
    struct bu_external attributes = BU_EXTERNAL_INIT_ZERO;
    attributes.ext_buf = attribute_bytes;
    attributes.ext_nbytes = sizeof(attribute_bytes);

    Id body_id = absent;
    Id last_object = absent;
    for (uint8_t value = '0'; value <= '8'; value++) {
	constexpr size_t attribute_value_offset = 2;
	attribute_bytes[attribute_value_offset] = value;
	struct bu_external record = BU_EXTERNAL_INIT_ZERO;
	db5_export_object3(&record, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT, "grid", 0,
		&attributes, &body, DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_8BITINT_U,
		DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
	RecordArtifact artifact;
	const bool factored = factor_record({record.ext_buf, record.ext_nbytes}, artifact);
	bu_free_external(&record);
	CHECK(factored);
	CHECK(artifact.object.size() <= 128 && artifact.object_id != last_object);
	last_object = artifact.object_id;
	if (body_id == absent)
	    body_id = artifact.body_id;
	CHECK(artifact.body_id == body_id);
    }
    CHECK(hex_id(body_id) == "96cb035f5ae26ba75803f68732950cc6a98e9f2f6224cad9f352493627bd8743");

    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    db5_export_object3(&record, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT, DB5_GLOBAL_OBJECT_NAME, 1,
	    &attributes, nullptr, DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0, DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
    const Bytes starting_record(record.ext_buf, record.ext_buf + record.ext_nbytes);
    bu_free_external(&record);
    RecordArtifact artifact;
    Object object;
    Bytes restored;
    CHECK(factor_record(starting_record, artifact) && artifact.body_id == absent);
    CHECK(decode_object(artifact.object, object) && object.suffix.size == 0);
    CHECK(materialize_record(object, {}, restored) && restored == starting_record);
    return 0;
}

int
main(int argc, const char *argv[])
{
    if (argc)
	bu_setprogname(argv[0]);
    return verify_vectors() || verify_body_sharing();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
