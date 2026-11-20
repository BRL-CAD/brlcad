/*          M I M E _ R E G I S T R Y . C P P
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
#include <atomic>
#include <cstring>
#include <cstdint>
#include <limits>
#include <sstream>
#include <thread>
#include <utility>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "bu/str.h"
#include "../mime_grib.h"
#include "../mime_lerc.h"
#include "mime_tar_fixture.h"
#include "mime_heightfield_fixture.h"
#include "mime_raster_label_fixture.h"
#include "mime_science_fixture.h"
#include "mime_product_fixture.h"
#include "../mime_xml.h"
#include "../mime_esric.h"
#include "../mime_noaa_grid.h"
#include "../mime_nsidc.h"
#include "../mime_iris.h"
#include "../mime_raster_label.h"
#include "../mime_hf2.h"
#include "../mime_sigdem.h"
#include "../mime_isg.h"

#define MIME_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("MIME check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

/* The fixture strings are ASCII; encode their code units explicitly. */
static std::vector<unsigned char>
mime_text_bytes(const std::string &text, size_t width, bool little, bool utf8_bom = false)
{
    std::vector<unsigned char> data;
    if (utf8_bom) data = {0xef, 0xbb, 0xbf};
    if (width == 2) data = little ? std::vector<unsigned char>{0xff, 0xfe} : std::vector<unsigned char>{0xfe, 0xff};
    if (width == 4) data = little ? std::vector<unsigned char>{0xff, 0xfe, 0, 0} : std::vector<unsigned char>{0, 0, 0xfe, 0xff};
    for (unsigned char c : text)
	for (size_t byte = 0; byte < width; ++byte) data.push_back((little ? byte : width - byte - 1) == 0 ? c : 0);
    return data;
}

static bu_mime_definition
mime_test_definition(const char *name)
{
    bu_mime_definition definition = {};
    definition.abi_version = BU_MIME_ABI_VERSION;
    definition.struct_size = sizeof(definition);
    definition.name = name;
    return definition;
}

static int
mime_registry_test(bu_mime_registry *registry)
{
    const char *extension = "newmesh";
    const char *alias = "MODEL/X-NEW-MESH";
    bu_mime_definition definition = mime_test_definition("model/vnd.example.mesh");
    definition.extensions = &extension;
    definition.extension_count = 1;
    definition.aliases = &alias;
    definition.alias_count = 1;
    definition.description = "Example mesh";
    bu_mime_token first = bu_mime_register(registry, &definition);
    MIME_CHECK(first);
    bu_vls canonical = BU_VLS_INIT_ZERO;
    MIME_CHECK(bu_mime_canonical(registry, alias, &canonical));
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&canonical), definition.name));
    MIME_CHECK(bu_mime_description(registry, alias, &canonical));
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&canonical), definition.description));
    bu_vls_free(&canonical);

    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, "PART.NEWMESH", NULL, 0, 0, &result) == BU_MIME_MATCH);
    MIME_CHECK(result->count == 1);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, definition.name));
    MIME_CHECK(result->candidates[0].evidence == BU_MIME_EVIDENCE_EXTENSION);

    bu_mime_info *info = NULL;
    size_t info_count = 0;
    MIME_CHECK(bu_mime_list(registry, alias, &info, &info_count));
    MIME_CHECK(info_count == 1 && BU_STR_EQUAL(info[0].name, definition.name));
    MIME_CHECK(info[0].extension_count == 1 && BU_STR_EQUAL(info[0].extensions[0], extension));
    MIME_CHECK(info[0].alias_count == 1 && BU_STR_EQUIV(info[0].aliases[0], alias));
    MIME_CHECK(BU_STR_EQUAL(info[0].description, definition.description));
    MIME_CHECK(bu_mime_resolve(registry, "NEWMESH", &canonical) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&canonical), definition.name));
    bu_vls_strcpy(&canonical, "NEWMESH");
    MIME_CHECK(bu_mime_resolve(registry, bu_vls_cstr(&canonical), &canonical) == BU_MIME_MATCH);
    bu_vls_free(&canonical);

    /* Independent contributions must survive removal of either owner. */
    bu_mime_token second = bu_mime_register(registry, &definition);
    MIME_CHECK(second && first != second);
    MIME_CHECK(bu_mime_unregister(registry, first));
    MIME_CHECK(!bu_mime_unregister(registry, first));
    MIME_CHECK(bu_mime_unregister(registry, second));
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, definition.name));
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_detect(registry, "part.newmesh", NULL, 0, 0, &result) == BU_MIME_UNKNOWN_RESULT);
    bu_mime_result_free(result);

    MIME_CHECK(BU_STR_EQUAL(info[0].name, definition.name));
    bu_mime_list_free(info, info_count);
    MIME_CHECK(bu_mime_list(registry, alias, &info, &info_count));
    MIME_CHECK(info_count == 0 && !info);

    definition = mime_test_definition("model/vnd.example.other");
    alias = "image/png";
    definition.aliases = &alias;
    definition.alias_count = 1;
    MIME_CHECK(!bu_mime_register(registry, &definition));
    MIME_CHECK(!bu_mime_canonical(registry, definition.name, &canonical));
    MIME_CHECK(!bu_vls_strlen(&canonical));
    bu_vls_free(&canonical);
    MIME_CHECK(!bu_mime_unregister(registry, 1));

    /* Descriptors and byte patterns belong to the caller only until return. */
    char mutable_name[] = "model/vnd.example.copied";
    char mutable_ext[] = "copied";
    extension = mutable_ext;
    definition = mime_test_definition(mutable_name);
    definition.extensions = &extension;
    definition.extension_count = 1;
    first = bu_mime_register(registry, &definition);
    MIME_CHECK(first);
    mutable_name[0] = 'x';
    mutable_ext[0] = 'x';
    MIME_CHECK(bu_mime_detect(registry, "file.copied", NULL, 0, 0, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/vnd.example.copied"));
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_unregister(registry, first));
    return 0;
}

static int
mime_signature_test(bu_mime_registry *registry)
{
    unsigned char signature[] = {'N', 'E', 'W', 'M'};
    unsigned char endian_value[] = {0x34, 0x12};
    unsigned char mask[] = {0xff, 0x0f};
    bu_mime_test tests[] = {
	{0, 0, signature, NULL, sizeof(signature)},
	{4, 6, endian_value, mask, sizeof(endian_value)}
    };
    bu_mime_rule rule = {tests, 2, BU_MIME_EVIDENCE_SIGNATURE, BU_MIME_DEFAULT_PRIORITY};
    bu_mime_definition definition = mime_test_definition("model/vnd.example.signature");
    definition.rules = &rule;
    definition.rule_count = 1;
    bu_mime_token first = bu_mime_register(registry, &definition);
    MIME_CHECK(first);
    signature[0] = 'X';
    unsigned char input[] = {'N', 'E', 'W', 'M', 0, 0, 0x34, 0xf2};
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, "wrong.png", input, sizeof(input), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, definition.name));
    MIME_CHECK(result->count >= 2);
    MIME_CHECK(result->candidates[1].evidence == BU_MIME_EVIDENCE_EXTENSION);
    MIME_CHECK(!result->incomplete);
    bu_mime_result_free(result);

    MIME_CHECK(bu_mime_detect(registry, NULL, input, 4, 0, &result) == BU_MIME_UNKNOWN_RESULT);
    MIME_CHECK(result->incomplete);
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_detect(registry, NULL, input, 4, 1, &result) == BU_MIME_UNKNOWN_RESULT);
    MIME_CHECK(!result->incomplete);
    bu_mime_result_free(result);

    signature[0] = 'N';
    definition.name = "model/vnd.example.tied";
    bu_mime_token second = bu_mime_register(registry, &definition);
    MIME_CHECK(second);
    MIME_CHECK(bu_mime_detect(registry, NULL, input, sizeof(input), 1, &result) == BU_MIME_AMBIGUOUS);
    MIME_CHECK(result->count == 2);
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_unregister(registry, second));

    unsigned char mismatch[] = {'X'};
    MIME_CHECK(bu_mime_detect(registry, NULL, mismatch, sizeof(mismatch), 0, &result) == BU_MIME_UNKNOWN_RESULT);
    MIME_CHECK(!result->incomplete);
    bu_mime_result_free(result);

    /* A complete empty input must not look like filename-only detection. */
    MIME_CHECK(bu_mime_detect(registry, NULL, input, 0, 1, &result) == BU_MIME_UNKNOWN_RESULT);
    MIME_CHECK(!result->incomplete);
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_unregister(registry, first));
    return 0;
}

static int
mime_alternatives_test(bu_mime_registry *registry)
{
    static const unsigned char first[] = {'A', 'A'};
    static const unsigned char second[] = {'B', 'B'};
    bu_mime_test tests[] = {{0, 0, first, NULL, sizeof(first)},
			   {0, 0, second, NULL, sizeof(second)}};
    bu_mime_rule rules[] = {{&tests[0], 1, BU_MIME_EVIDENCE_SIGNATURE, BU_MIME_DEFAULT_PRIORITY},
			   {&tests[1], 1, BU_MIME_EVIDENCE_SIGNATURE, BU_MIME_DEFAULT_PRIORITY}};
    const char *extensions[] = {"alt", "long.alt"};
    bu_mime_definition definition = mime_test_definition("model/vnd.example.alternatives");
    definition.extensions = extensions;
    definition.extension_count = 2;
    definition.rules = rules;
    definition.rule_count = 2;
    bu_mime_token first_token = bu_mime_register(registry, &definition);
    MIME_CHECK(first_token);
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, "name.long.alt", second, sizeof(second), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(result->count == 1);
    MIME_CHECK(result->candidates[0].evidence == BU_MIME_EVIDENCE_SIGNATURE);
    bu_mime_result_free(result);

    /* Ranking must not depend on registration order or filename agreement. */
    definition.name = "model/vnd.example.preferred";
    definition.extensions = NULL;
    definition.extension_count = 0;
    rules[1].priority++;
    bu_mime_token preferred_token = bu_mime_register(registry, &definition);
    MIME_CHECK(preferred_token);
    MIME_CHECK(bu_mime_detect(registry, "name.alt", second, sizeof(second), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, definition.name));
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_unregister(registry, preferred_token));
    MIME_CHECK(bu_mime_unregister(registry, first_token));

    bu_mime_registry *limited = bu_mime_registry_create(1);
    MIME_CHECK(!bu_mime_register(limited, &definition));
    bu_mime_registry_destroy(limited);
    return 0;
}

static int
mime_builtin_test(bu_mime_registry *registry)
{
    static const unsigned char png[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, "wrong.jpg", png, sizeof(png), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "image/png"));
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_legacy_type("IMAGE/PNG", BU_MIME_IMAGE) == BU_MIME_IMAGE_PNG);
    MIME_CHECK(BU_STR_EQUAL(bu_mime_legacy_name(BU_MIME_IMAGE_PNG, BU_MIME_IMAGE), "image/png"));
    MIME_CHECK(!bu_mime_legacy_name(-1, BU_MIME_IMAGE));
    MIME_CHECK(bu_mime_legacy_type("model/vnd.example.unknown", BU_MIME_MODEL) == -1);

    char path[MAXPATHLEN];
    FILE *fp = bu_temp_file(path, sizeof(path));
    MIME_CHECK(fp);
    MIME_CHECK(fwrite(png, 1, sizeof(png), fp) == sizeof(png));
    MIME_CHECK(!fclose(fp));
    MIME_CHECK(bu_mime_detect_file(registry, path, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "image/png"));
    bu_mime_result_free(result);
    MIME_CHECK(bu_file_delete(path));
    MIME_CHECK(bu_mime_detect_file(registry, path, &result) == BU_MIME_ERROR);
    MIME_CHECK(!result);
    return 0;
}

static int
mime_evidence_test(bu_mime_registry *registry)
{
    const unsigned char unknown[] = {'?'};
    bu_mime_result *result = NULL;
    bu_vls canonical = BU_VLS_INIT_ZERO;
    MIME_CHECK(bu_mime_detect(registry, NULL, unknown, sizeof(unknown), 1, &result) == BU_MIME_UNKNOWN_RESULT);
    MIME_CHECK(bu_mime_result_add(registry, result, "IMAGE/PNG", BU_MIME_EVIDENCE_SIGNATURE,
				BU_MIME_DEFAULT_PRIORITY) == BU_MIME_MATCH);
    MIME_CHECK(bu_mime_result_add(registry, result, "image/jpeg", BU_MIME_EVIDENCE_SIGNATURE,
				BU_MIME_DEFAULT_PRIORITY) == BU_MIME_AMBIGUOUS);
    MIME_CHECK(bu_mime_result_add(registry, result, "image/png", BU_MIME_EVIDENCE_SIGNATURE,
				BU_MIME_MAX_PRIORITY) == BU_MIME_MATCH);
    MIME_CHECK(result->count == 2 && BU_STR_EQUAL(result->candidates[0].name, "image/png"));
    MIME_CHECK(bu_mime_result_add(registry, result, "missing/type", BU_MIME_EVIDENCE_SIGNATURE, 0) == BU_MIME_ERROR);
    MIME_CHECK(result->count == 2);
    bu_mime_result_free(result);
    MIME_CHECK(!bu_mime_canonical(registry, "model/gltf", &canonical));
    MIME_CHECK(bu_mime_canonical(registry, "model/gltf+json", &canonical));
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&canonical), "model/gltf+json"));
    bu_vls_free(&canonical);
    const unsigned char glb[] = {'g', 'l', 'T', 'F', 2, 0, 0, 0};
    MIME_CHECK(bu_mime_detect(registry, "wrong.png", glb, sizeof(glb), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/gltf-binary"));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_validation_test(bu_mime_registry *registry)
{
    bu_mime_definition definition = mime_test_definition("bad name/type");
    MIME_CHECK(!bu_mime_register(registry, &definition));
    definition.name = "model/!bad";
    MIME_CHECK(!bu_mime_register(registry, &definition));
    definition.name = "model/example";
    definition.abi_version++;
    MIME_CHECK(!bu_mime_register(registry, &definition));
    definition = mime_test_definition("model/example");
    definition.struct_size--;
    MIME_CHECK(!bu_mime_register(registry, &definition));
    definition = mime_test_definition("model/example");
    unsigned char value = 1;
    bu_mime_test test = {BU_MIME_DEFAULT_SCAN_LIMIT, BU_MIME_DEFAULT_SCAN_LIMIT, &value, NULL, 1};
    bu_mime_rule rule = {&test, 1, BU_MIME_EVIDENCE_SIGNATURE, BU_MIME_DEFAULT_PRIORITY};
    definition.rules = &rule;
    definition.rule_count = 1;
    MIME_CHECK(!bu_mime_register(registry, &definition));
    test.offset = test.last_offset = 0;
    rule.priority = BU_MIME_MAX_PRIORITY + 1;
    MIME_CHECK(!bu_mime_register(registry, &definition));
    rule.priority = BU_MIME_DEFAULT_PRIORITY;
    test.last_offset = static_cast<size_t>(-1);
    MIME_CHECK(!bu_mime_register(registry, &definition));
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, NULL, 1, 0, &result) == BU_MIME_ERROR);
    MIME_CHECK(!result);
    return 0;
}

static int
mime_concurrency_test(bu_mime_registry *registry)
{
    std::atomic<int> failures(0);
    std::vector<std::thread> readers;
    const size_t iterations = 50;
    const size_t reader_count = 3;
    for (size_t i = 0; i < reader_count; ++i) {
	readers.emplace_back([&]() {
	    for (size_t j = 0; j < iterations; ++j) {
		bu_mime_result *result = NULL;
		if (bu_mime_detect(registry, "part.png", NULL, 0, 0, &result) != BU_MIME_MATCH)
		    ++failures;
		bu_mime_result_free(result);
	    }
	});
    }
    for (size_t i = 0; i < iterations; ++i) {
	bu_mime_definition definition = mime_test_definition("model/vnd.example.concurrent");
	bu_mime_token token = bu_mime_register(registry, &definition);
	if (!token || !bu_mime_unregister(registry, token))
	    ++failures;
    }
    for (auto &reader : readers)
	reader.join();
    MIME_CHECK(!failures.load());
    return 0;
}

static int
mime_trait_test()
{
    bu_mime_traits facts = BU_MIME_TRAITS_INIT_ZERO;
    bu_mime_traits copy = BU_MIME_TRAITS_INIT_ZERO;
    const bu_mime_trait binary[] = {
        BU_MIME_TRAIT_STRING("encoding", "binary"),
        BU_MIME_TRAIT_STRING("byte-order", "little-endian"),
    };
    const bu_mime_traits requested = {binary, 1};
    const bu_mime_capability capability = BU_MIME_CAPABILITY("model/stl", binary, 2, BU_MIME_READ | BU_MIME_WRITE);
    MIME_CHECK(!bu_mime_capability_match(&capability, NULL, BU_MIME_INPUT));
    MIME_CHECK(bu_mime_traits_add(&facts, "encoding", "binary"));
    MIME_CHECK(!bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    MIME_CHECK(bu_mime_traits_add(&facts, "byte-order", "little-endian"));
    MIME_CHECK(bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    MIME_CHECK(bu_mime_capability_resolve(&capability, &requested, BU_MIME_OUTPUT, &copy));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&copy, "byte-order"), "little-endian"));
    MIME_CHECK(bu_mime_traits_add(&facts, "encoding", "ascii"));
    MIME_CHECK(!bu_mime_traits_get(&facts, "encoding"));
    MIME_CHECK(!bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    MIME_CHECK(!bu_mime_traits_resolve(&copy, &facts, &requested));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&copy, "encoding"), "binary"));
    MIME_CHECK(bu_mime_traits_copy(&copy, &facts));
    bu_mime_traits_free(&facts);
    MIME_CHECK(copy.count == 3 && !bu_mime_traits_valid(&copy, 1));
    bu_mime_traits_free(&copy);
    const char *valid[] = {"encoding=binary"};
    const char *conflict[] = {"encoding=ascii"};
    const char *invalid[] = {"Encoding=binary"};
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, valid, &facts) == 1);
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, conflict, &facts) == -1);
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, invalid, &facts) == -1);
    bu_mime_traits_free(&facts);
    return 0;
}

static int
mime_set_test()
{
    const char *extensions[] = {"KHR_materials_unlit", "KHR_mesh_quantization"};
    const bu_mime_trait supported[] = {
	BU_MIME_TRAIT_STRING("version", "2.0"),
	BU_MIME_TRAIT_SET("gltf.required-extensions", extensions, 2)
    };
    const bu_mime_capability capability = BU_MIME_CAPABILITY("model/gltf+json", supported, 2, BU_MIME_READ | BU_MIME_WRITE);
    bu_mime_traits facts = BU_MIME_TRAITS_INIT_ZERO;
    bu_mime_traits request = BU_MIME_TRAITS_INIT_ZERO;
    bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    const char *const *members = NULL;
    size_t count = 0;
    MIME_CHECK(bu_mime_traits_add(&facts, "version", "2.0"));
    MIME_CHECK(!bu_mime_traits_get_set(&facts, "gltf.required-extensions", &members, &count));
    MIME_CHECK(!bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    bu_mime_capability deferred = capability;
    deferred.flags = BU_MIME_DEFER_INPUT_VALIDATION;
    MIME_CHECK(bu_mime_capability_match(&deferred, &facts, BU_MIME_INPUT));
    MIME_CHECK(bu_mime_traits_add_set(&facts, "gltf.required-extensions", NULL, 0));
    MIME_CHECK(bu_mime_traits_get_set(&facts, "gltf.required-extensions", &members, &count));
    MIME_CHECK(!count && !members && bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    bu_mime_traits_free(&facts);
    const char *reordered[] = {extensions[1], extensions[0], extensions[1]};
    MIME_CHECK(bu_mime_traits_add_set(&facts, "gltf.required-extensions", reordered, 3));
    MIME_CHECK(bu_mime_traits_add(&facts, "version", "2.0"));
    MIME_CHECK(bu_mime_traits_get_set(&facts, "gltf.required-extensions", &members, &count));
    MIME_CHECK(count == 2 && BU_STR_EQUAL(members[0], extensions[0]));
    MIME_CHECK(!bu_mime_traits_get(&facts, "gltf.required-extensions"));
    MIME_CHECK(bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    MIME_CHECK(bu_mime_traits_add_set(&request, "gltf.required-extensions", extensions, 1));
    MIME_CHECK(bu_mime_capability_resolve(&capability, &request, BU_MIME_OUTPUT, &resolved));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&resolved, "version"), "2.0"));
    MIME_CHECK(bu_mime_traits_get_set(&resolved, "gltf.required-extensions", &members, &count) && count == 1);
    MIME_CHECK(!bu_mime_traits_resolve(&resolved, &facts, &request));
    MIME_CHECK(bu_mime_traits_get_set(&resolved, "gltf.required-extensions", &members, &count) && count == 1);
    bu_mime_traits_free(&request);
    const char *unsupported[] = {"KHR_draco_mesh_compression"};
    MIME_CHECK(bu_mime_traits_add_set(&request, "gltf.required-extensions", unsupported, 1));
    MIME_CHECK(!bu_mime_capability_match(&capability, &request, BU_MIME_OUTPUT));
    MIME_CHECK(bu_mime_traits_add_set(&facts, "gltf.required-extensions", unsupported, 1));
    MIME_CHECK(bu_mime_traits_valid(&facts, 0) && !bu_mime_traits_valid(&facts, 1));
    MIME_CHECK(!bu_mime_traits_get_set(&facts, "gltf.required-extensions", &members, &count) && !count && !members);
    MIME_CHECK(!bu_mime_capability_match(&capability, &facts, BU_MIME_INPUT));
    MIME_CHECK(!bu_mime_capability_match(&deferred, &facts, BU_MIME_INPUT));
    MIME_CHECK(!bu_mime_capability_match(&deferred, &request, BU_MIME_OUTPUT));
    bu_mime_traits_free(&facts);
    bu_mime_traits_free(&request);
    bu_mime_traits_free(&resolved);
    char first[] = "KHR_materials_unlit";
    const char *mutable_members[] = {first};
    const bu_mime_trait temporary[] = {BU_MIME_TRAIT_SET("gltf.required-extensions", mutable_members, 1)};
    const bu_mime_capability borrowed = BU_MIME_CAPABILITY("model/gltf+json", temporary, 1, BU_MIME_READ);
    bu_mime_capability owned = {};
    MIME_CHECK(bu_mime_capability_copy(&owned, &borrowed));
    first[0] = 'X';
    MIME_CHECK(bu_mime_traits_add_set(&facts, "gltf.required-extensions", extensions, 1));
    MIME_CHECK(bu_mime_capability_match(&owned, &facts, BU_MIME_INPUT));
    bu_mime_capability_free(&owned);
    bu_mime_traits_free(&facts);
    const char *assignment[] = {"gltf.required-extensions[]=[\"KHR_mesh_quantization\",\"KHR_materials_unlit\",\"KHR_mesh_quantization\"]"};
    const char *same[] = {"gltf.required-extensions[]=[\"KHR_materials_unlit\",\"KHR_mesh_quantization\"]"};
    const char *conflicting[] = {"gltf.required-extensions[]=[]"};
    const char *invalid[] = {"gltf.required-extensions[]=[42]"};
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, assignment, &request) == 1);
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, same, &request) == 1);
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, conflicting, &request) == -1);
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, invalid, &request) == -1);
    MIME_CHECK(bu_mime_traits_get_set(&request, "gltf.required-extensions", &members, &count) && count == 2);
    MIME_CHECK(bu_mime_traits_add(&request, "gltf.required-extensions", "ordinary string"));
    MIME_CHECK(!bu_mime_traits_valid(&request, 1));
    bu_mime_traits_free(&request);
    MIME_CHECK(bu_opt_mime_trait(NULL, 1, conflicting, &facts) == 1);
    MIME_CHECK(bu_mime_traits_get_set(&facts, "gltf.required-extensions", &members, &count) && !count);
    bu_vls description = BU_VLS_INIT_ZERO;
    bu_mime_traits_describe(&facts, &description);
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&description), " gltf.required-extensions[]=[]"));
    bu_vls_free(&description);
    bu_mime_traits_free(&facts);
    return 0;
}

static int
mime_version_test()
{
    const struct { const char *left; const char *right; int comparison; } samples[] = {
	{"2.0", "02.00.0", 0}, {"1.99", "2", -1}, {"10.3", "9.5", 1},
	{"2.0.1", "2.0", 1}, {"99999999999999999999", "100000000000000000000", -1}
    };
    for (const auto &sample : samples) {
	int comparison = 0;
	MIME_CHECK(bu_mime_version_compare(sample.left, sample.right, &comparison));
	MIME_CHECK(comparison == sample.comparison);
    }
    const char *invalid[] = {"", ".2", "2.", "2..0", "-1.0", "2.0beta", "2.0 ", NULL};
    for (const char *value : invalid) {
	int comparison = 1;
	MIME_CHECK(!bu_mime_version_compare(value, "2.0", &comparison) && !comparison);
    }
    return 0;
}

static int
mime_scene_profiles_test(bu_mime_registry *registry)
{
    const char md2[] = "IDP2\x08\0\0\0";
    const char md2_future[] = "IDP2\x09\0\0\0";
    const char bsp38[] = "IBSP\x26\0\0\0";
    const char bsp46[] = "IBSP\x2e\0\0\0";
    const char bsp47[] = "IBSP\x2f\0\0\0";
    const char md3[] = "IDP3\x0f\0\0\0";
    const char iqm[] = "INTERQUAKEMODEL\0\x02\0\0\0";
    const char b3d[] = "BB3D\x04\0\0\0\x01\0\0\0";
    const char mdc[] = "IDPC\x02\0\0\0";
    const char ms3d[] = "MS3D000000\x04\0\0\0";
    const char terragen[] = "TERRAGENTERRAIN ";
    const char ase[] = "*3DSMAX_ASCIIEXPORT 200\n";
    const char lws[] = "LWSC\n5\n";
    const struct {
	const char *data;
	size_t size;
	const char *type;
	const char *version;
	const char *encoding;
    } samples[] = {
	{md2, sizeof(md2) - 1, "model/vnd.idsoftware.md2", "8", "binary"},
	{md2_future, sizeof(md2_future) - 1, "model/vnd.idsoftware.md2", "9", "binary"},
	{bsp38, sizeof(bsp38) - 1, "model/vnd.idsoftware.bsp", "38", "binary"},
	{bsp46, sizeof(bsp46) - 1, "model/vnd.idsoftware.bsp", "46", "binary"},
	{bsp47, sizeof(bsp47) - 1, "model/vnd.idsoftware.bsp", "47", "binary"},
	{md3, sizeof(md3) - 1, "model/vnd.idsoftware.md3", "15", "binary"},
	{iqm, sizeof(iqm) - 1, "model/vnd.interquake.iqm", "2", "binary"},
	{b3d, sizeof(b3d) - 1, "model/vnd.blitzresearch.b3d", "0.01", "binary"},
	{mdc, sizeof(mdc) - 1, "model/vnd.idsoftware.mdc", "2", "binary"},
	{ms3d, sizeof(ms3d) - 1, "model/vnd.chumbalumsoft.ms3d", "4", "binary"},
	{terragen, sizeof(terragen) - 1, "application/terragen", NULL, "binary"},
	{ase, sizeof(ase) - 1, "model/vnd.autodesk.ase", "200", "ascii"},
	{lws, sizeof(lws) - 1, "image/x-lws", "5", "ascii"}
    };
    for (const auto &sample : samples) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "scene.payload", sample.data, sample.size, 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(result->count && BU_STR_EQUAL(result->candidates[0].name, sample.type));
	const bu_mime_traits *traits = &result->candidates[0].traits;
	const char *version = bu_mime_traits_get(traits, "version");
	const char *encoding = bu_mime_traits_get(traits, "encoding");
	MIME_CHECK(sample.version ? version && BU_STR_EQUAL(version, sample.version) : !version);
	MIME_CHECK(encoding && BU_STR_EQUAL(encoding, sample.encoding));
	if (BU_STR_EQUAL(sample.encoding, "binary")) {
	    const char *order = bu_mime_traits_get(traits, "byte-order");
	    MIME_CHECK(order && BU_STR_EQUAL(order, "little-endian"));
	}
	bu_mime_result_free(result);
    }
    bu_mime_result *partial = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, md2, sizeof(md2) - 2, 0, &partial) == BU_MIME_MATCH);
    MIME_CHECK(partial->incomplete && !bu_mime_traits_get(&partial->candidates[0].traits, "version"));
    bu_mime_result_free(partial);
    MIME_CHECK(bu_mime_detect(registry, NULL, md2, sizeof("IDP") - 1, 0, &partial) == BU_MIME_UNKNOWN_RESULT);
    MIME_CHECK(partial->incomplete);
    bu_mime_result_free(partial);
    const char wrong_iqm[] = "INTERQUAKEMODELX\x02\0\0\0";
    bu_mime_result *unrelated = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, wrong_iqm, sizeof(wrong_iqm) - 1, 1, &unrelated) == BU_MIME_UNKNOWN_RESULT);
    bu_mime_result_free(unrelated);
    return 0;
}

static int
mime_iff_profiles_test(bu_mime_registry *registry)
{
    const char *forms[] = {"LWOB", "LWO2", "LWO3", "LXOB"};
    for (const char *form : forms) {
	unsigned char header[12] = {'F', 'O', 'R', 'M', 0, 0, 0, 4};
	memcpy(header + 8, form, 4);
	for (size_t size = 0; size <= sizeof(header); ++size) {
	    bu_mime_result *result = NULL;
	    auto status = bu_mime_detect(registry, NULL, header, size, 0, &result);
	    if (size < sizeof(header)) MIME_CHECK(status == BU_MIME_UNKNOWN_RESULT);
	    else {
		MIME_CHECK(status == BU_MIME_MATCH);
		const auto &candidate = result->candidates[0];
		MIME_CHECK(BU_STR_EQUAL(candidate.name, BU_STR_EQUAL(form, "LXOB") ? "model/vnd.foundry.lxo" : "image/x-lwo"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&candidate.traits, "iff.form"), form));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&candidate.traits, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&candidate.traits, "byte-order"), "big-endian"));
		MIME_CHECK(!bu_mime_traits_get(&candidate.traits, "version"));
	    }
	    bu_mime_result_free(result);
	}
    }
    unsigned char lxo[30] = {'F', 'O', 'R', 'M', 0, 0, 0, 22, 'L', 'X', 'O', 'B',
	'V', 'R', 'S', 'N', 0, 0, 0, 10, 0, 0, 0, 4, 0, 0, 0, 2, 'x', 0};
    for (size_t size = 12; size <= sizeof(lxo); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, lxo, size, 0, &result) == BU_MIME_MATCH);
	const char *version = bu_mime_traits_get(&result->candidates[0].traits, "version");
	if (size >= 28) MIME_CHECK(version && BU_STR_EQUAL(version, "4.2"));
	else MIME_CHECK(!version && result->incomplete);
	bu_mime_result_free(result);
    }
    for (unsigned char declared : {static_cast<unsigned char>(4), static_cast<unsigned char>(0)}) {
	lxo[7] = declared;
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, lxo, sizeof(lxo), 1, &result) == BU_MIME_MATCH);
	const char *version = bu_mime_traits_get(&result->candidates[0].traits, "version");
	if (declared) MIME_CHECK(!version);
	else MIME_CHECK(version && BU_STR_EQUAL(version, "4.2"));
	bu_mime_result_free(result);
    }
    std::vector<unsigned char> padded(lxo, lxo + 12);
    padded.insert(padded.end(), {'J', 'U', 'N', 'K', 0, 0, 0, 1, 'x', 0});
    padded.insert(padded.end(), lxo + 12, lxo + sizeof(lxo));
    padded[7] = static_cast<unsigned char>(padded.size() - 8);
    {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, padded.data(), padded.size(), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "version"), "4.2"));
	bu_mime_result_free(result);
    }
    const char *unrelated[] = {"AIFF", "ILBM", "LXPR", "LXPM", "LXPE", "LWO4"};
    for (const char *form : unrelated) {
	unsigned char header[12] = {'F', 'O', 'R', 'M', 0, 0, 0, 4};
	memcpy(header + 8, form, 4);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, header, sizeof(header), 1, &result) != BU_MIME_ERROR);
	for (size_t i = 0; i < result->count; ++i)
	    MIME_CHECK(!BU_STR_EQUAL(result->candidates[i].name, "image/x-lwo") && !BU_STR_EQUAL(result->candidates[i].name, "model/vnd.foundry.lxo"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_skeletal_profiles_test(bu_mime_registry *registry)
{
    const struct {const char *text; const char *type; const char *version; const char *kind;} cases[] = {
	{"MD5Version 10\ncommandline \"camera { numAnimatedComponents\"\nnumMeshes 1\nmesh {\n}\n", "model/vnd.idsoftware.md5", "10", "mesh"},
	{"MD5Version 11\nnumAnimatedComponents 1\nhierarchy {\n\"mesh\" -1 0 0\n}\n", "model/vnd.idsoftware.md5", "11", "animation"},
	{"MD5Version 10\nnumCuts 0\ncamera {\n}\n", "model/vnd.idsoftware.md5", "10", "camera"},
	{"MD5Version 10\nnumMeshes 1\nnumCuts 0\n", "model/vnd.idsoftware.md5", "10", "mixed"},
	{"version 01\nnodes\n0 \"vertexanimation\" -1\nend\ntriangles\n// vertexanimation\nend\n", "model/vnd.valve.smd", "1", "mesh"},
	{"version 2\nnodes\nend\nskeleton\nend\n", "model/vnd.valve.smd", "2", "skeleton"},
	{"version 1\nnodes\nend\nvertexanimation\nend\n", "model/vnd.valve.smd", "1", "vertex-animation"},
	{"version 1\ntriangles\nend\nvertexanimation\nend\n", "model/vnd.valve.smd", "1", "mixed"}};
    for (const auto &sample : cases)
	for (bool complete : {false, true}) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, sample.text, strlen(sample.text), complete, &result) == BU_MIME_MATCH);
	    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, sample.type));
	    const auto &traits = result->candidates[0].traits;
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "version"), sample.version));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "ascii"));
	    const char *key = BU_STR_EQUAL(sample.type, "model/vnd.valve.smd") ? "smd.kind" : "md5.kind";
	    const char *kind = bu_mime_traits_get(&traits, key);
	    if (complete) MIME_CHECK(kind && BU_STR_EQUAL(kind, sample.kind));
	    else MIME_CHECK(!kind && result->incomplete);
	    bu_mime_result_free(result);
	}
    const char *unrelated[] = {"version 1\n\"nodes\"\n", "// MD5Version 10\n// numMeshes 1\n",
	"MD5Version10\nnumMeshes 1\n", "version 1\nunrelated\n"};
    for (const char *text : unrelated) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_unreal_profiles_test(bu_mime_registry *registry)
{
    constexpr size_t data_header_size = 48, triangle_size = 16;
    const char *mesh_type = "model/vnd.epicgames.unreal.mesh";
    std::vector<unsigned char> data(data_header_size + 4 * triangle_size, 0);
    data[0] = 4; // Triangle count, little-endian uint16.
    data[2] = 4; // Vertex count.
    std::vector<unsigned char> animation(4 + 4 * sizeof(uint32_t), 0);
    animation[0] = 1;
    animation[2] = 4 * sizeof(uint32_t);
    for (bool aniv : {false, true}) {
	const auto &bytes = aniv ? animation : data;
	const char *name = aniv ? "mesh_a.3d" : "mesh_d.3d";
	bu_mime_probe_input input = {bytes.data(), bytes.size(), 1, bytes.size(), 1};
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_identify(registry, name, &input, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, mesh_type));
	const auto &traits = result->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "unreal.part"), aniv ? "animation" : "data"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
	MIME_CHECK(!bu_mime_traits_get(&traits, "unreal.vertex-stride") && !bu_mime_traits_get(&traits, "version"));
	if (aniv) MIME_CHECK(!bu_mime_traits_get(&traits, "unreal.vertices"));
	bu_mime_result_free(result);
	result = NULL;
	MIME_CHECK(bu_mime_identify(registry, NULL, &input, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    const std::string code = "class fixture\n expands Actor;\n// #exec MESH IMPORT MESH=comment\n"
	"var string S=\"#exec TEXTURE IMPORT NAME=literal\";\n"
	"#exec MESHMAP SCALE MESHMAP=fixture X=1 Y=1 Z=1\n#exec OBJ LOAD FILE=fixture\n";
    for (size_t width : {size_t(1), size_t(2), size_t(4)})
	for (bool little : {false, true}) {
	    if (width == 1 && little) continue;
	    std::vector<unsigned char> bytes;
	    if (width == 2) bytes = little ? std::vector<unsigned char>{0xff, 0xfe} : std::vector<unsigned char>{0xfe, 0xff};
	    if (width == 4) bytes = little ? std::vector<unsigned char>{0xff, 0xfe, 0, 0} : std::vector<unsigned char>{0, 0, 0xfe, 0xff};
	    for (unsigned char c : code)
		for (size_t byte = 0; byte < width; ++byte) bytes.push_back((little ? byte : width - byte - 1) == 0 ? c : 0);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) == BU_MIME_MATCH);
	    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "text/vnd.epicgames.unrealscript"));
	    const char *const *members = NULL;
	    size_t count = 0;
	    MIME_CHECK(bu_mime_traits_get_set(&result->candidates[0].traits, "unrealscript.exec-commands", &members, &count));
	    MIME_CHECK(count == 2 && BU_STR_EQUAL(members[0], "meshmap.scale") && BU_STR_EQUAL(members[1], "obj.load"));
	    MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "version"));
	    bu_mime_result_free(result);
	    result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 0, &result) == BU_MIME_MATCH);
	    MIME_CHECK(result->incomplete && !bu_mime_traits_get_set(&result->candidates[0].traits, "unrealscript.exec-commands", &members, &count));
	    bu_mime_result_free(result);
	}
    for (const char *text : {"// class fake expands Actor;\n", "\"#exec MESHMAP SCALE MESHMAP=f\"\n", "class Java extends Base { int field; }\n"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_ogre_profiles_test(bu_mime_registry *registry)
{
    const std::string prefix = "[MeshSerializer_v";
    for (bool big : {false, true})
	for (const char *revision : {"1.8", "1.100", "1.40", "2.1 R0 LEGACYV2"}) {
	    std::string header(2, '\0');
	    header[big ? 0 : 1] = 0x10; // Header chunk ID 0x1000.
	    const std::string serializer = prefix + revision + "]";
	    header += serializer + "\n";
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), header.size(), 1, &result) == BU_MIME_MATCH);
	    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/vnd.ogre.mesh"));
	    const auto &traits = result->candidates[0].traits;
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "byte-order"), big ? "big-endian" : "little-endian"));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "ogre.serializer"), serializer.c_str()));
	    const char *version = bu_mime_traits_get(&traits, "version");
	    MIME_CHECK(strchr(revision, ' ') ? !version : version && BU_STR_EQUAL(version, revision));
	    bu_mime_result_free(result);
	    for (size_t size = 0; size < header.size(); ++size) {
		result = NULL;
		bu_mime_status status = bu_mime_detect(registry, NULL, header.data(), size, 0, &result);
		MIME_CHECK(status == (size >= sizeof(uint16_t) + prefix.size() ? BU_MIME_MATCH : BU_MIME_UNKNOWN_RESULT));
		if (status == BU_MIME_MATCH && size < header.size() - 1)
		    MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "ogre.serializer"));
		bu_mime_result_free(result);
	    }
	}
    const std::string skeleton = std::string("\0\x10", sizeof(uint16_t)) + "[Serializer_v1.80]\n";
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, skeleton.data(), skeleton.size(), 1, &result) == BU_MIME_UNKNOWN_RESULT);
    bu_mime_result_free(result);
    return 0;
}

static int
mime_xml_profiles_test(bu_mime_registry *registry)
{
    const struct {const char *xml; const char *type; const char *version; const char *qname; const char *uri;} cases[] = {
	{"<Product_Observational xmlns='http://pds.nasa.gov/pds4/pds/v1'/>", "application/vnd.nasa.pds4+xml", NULL, "Product_Observational", "http://pds.nasa.gov/pds4/pds/v1"},
	{"<p:Product_Ancillary xmlns:p='http://pds.nasa.gov/pds4/pds/v1' version='4.0'/>", "application/vnd.nasa.pds4+xml", NULL, "p:Product_Ancillary", "http://pds.nasa.gov/pds4/pds/v1"},
	{"<Product_Future xmlns='http://pds.nasa.gov/pds4/pds/v1'/>", "application/vnd.nasa.pds4+xml", NULL, "Product_Future", "http://pds.nasa.gov/pds4/pds/v1"},
	{"<MRF_META version='9.1'/>", "application/vnd.nasa.mrf+xml", NULL, "MRF_META", NULL},
	{"<kml xmlns='http://www.opengis.net/kml/2.2'/>", "application/vnd.google-earth.kml+xml", NULL, "kml", "http://www.opengis.net/kml/2.2"},
	{"<k:kml xmlns:k='http://www.opengis.net/kml/2.2' version='9.1'/>", "application/vnd.google-earth.kml+xml", "9.1", "k:kml", "http://www.opengis.net/kml/2.2"},
	{"<kml xmlns='http://earth.google.com/kml/2.1'/>", "application/vnd.google-earth.kml+xml", NULL, "kml", "http://earth.google.com/kml/2.1"},
	{"<kml xmlns='http://earth.google.com/kml/2.2'/>", "application/vnd.google-earth.kml+xml", NULL, "kml", "http://earth.google.com/kml/2.2"},
	{"<?xml version='1.0'?><mesh version='1.0' xmlns='http://irrlicht.sourceforge.net/IRRMESH_09_2007'/>", "model/vnd.irrlicht.mesh+xml", "1.0", "mesh", "http://irrlicht.sourceforge.net/IRRMESH_09_2007"},
	{"<!-- <mesh/> --><?info data?><irr_scene/>", "model/vnd.irrlicht.scene+xml", NULL, "irr_scene", NULL},
	{"<!DOCTYPE COLLADA [<!-- ] > --> <!ENTITY x '>'>]><COLLADA version='1.5.0' xmlns='http://www.collada.org/2008/03/COLLADASchema'/>", "model/vnd.collada+xml", "1.5.0", "COLLADA", "http://www.collada.org/2008/03/COLLADASchema"},
	{"<c:COLLADA xmlns:c='http:&#47;&#47;www.collada.org/2005/11/COLLADASchema' version='1.4.1'/>", "model/vnd.collada+xml", "1.4.1", "c:COLLADA", "http://www.collada.org/2005/11/COLLADASchema"},
	{"<amf unit='millimeter'/>", "model/amf", NULL, "amf", NULL},
	{"<?xml version='1.0'?><X3D version='3.3' profile='Interchange'/>", "model/x3d+xml", "3.3", "X3D", NULL},
	{"<?xml version='1.0'?><X3D profile='Full'/>", "model/x3d+xml", NULL, "X3D", NULL},
	{"<X3D xmlns='http://www.web3d.org/specifications/x3d-namespace' version='3.3'/>", "model/x3d+xml", "3.3", "X3D", "http://www.web3d.org/specifications/x3d-namespace"},
	{"<x:X3D xmlns:x='http://www.web3d.org/specifications/x3d-namespace' version='3.3'/>", "model/x3d+xml", "3.3", "x:X3D", "http://www.web3d.org/specifications/x3d-namespace"},
	{"<VTKFile type='PolyData'/>", "application/vnd.kitware.vtk+xml", NULL, "VTKFile", NULL}};
    for (const auto &sample : cases)
	for (size_t width : {size_t(1), size_t(2), size_t(4)})
	    for (bool little : {false, true}) {
		if (width == 1 && little) continue;
		const auto data = mime_text_bytes(sample.xml, width, little);
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, "geometry.payload", data.data(), data.size(), 1, &result) == BU_MIME_MATCH);
		MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, sample.type));
		const auto &traits = result->candidates[0].traits;
		const char *version = bu_mime_traits_get(&traits, "version");
		MIME_CHECK(sample.version ? version && BU_STR_EQUAL(version, sample.version) : !version);
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "xml.root-qname"), sample.qname));
		const char *const *uris = NULL;
		size_t count = 0;
		MIME_CHECK(bu_mime_traits_get_set(&traits, "xml.root-namespaces", &uris, &count));
		MIME_CHECK(sample.uri ? count == 1 && BU_STR_EQUAL(uris[0], sample.uri) : !count);
		std::string encoding = width == 1 ? "utf-8" : std::string(width == 2 ? "utf-16" : "utf-32") + (little ? "le" : "be");
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), encoding.c_str()));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "text.code-unit-width"), width == 1 ? "8" : width == 2 ? "16" : "32"));
		bu_mime_result_free(result);
	    }
    for (const char *text : {"<Product_Observational/>", "<Product_Observational xmlns='unrelated'/>",
	"<Product_Observational xmlns='http://pds.nasa.gov/pds4/pds/v2'/>",
	"<wrapper><Product_Observational xmlns='http://pds.nasa.gov/pds4/pds/v1'/></wrapper>",
	"<Unrelated xmlns='http://pds.nasa.gov/pds4/pds/v1'/>", "<MRF_META xmlns='unrelated'/>",
	"<!-- <MRF_META/> -->", "<wrapper><MRF_META/></wrapper>",
	"<kml/>", "<kml xmlns='unrelated'/>", "<k:kml xmlns:k='unrelated'/>",
	"<!-- <kml xmlns='http://www.opengis.net/kml/2.2'/> -->", "<wrapper><kml xmlns='http://www.opengis.net/kml/2.2'/></wrapper>",
	"<kml xmlns='http://www.opengis.net/kml/2.2' xmlns='http://earth.google.com/kml/2.1'/>",
	"<mesh/>", "<!-- <irr_scene/> -->", "<wrapper><irr_scene/></wrapper>",
	"<irr_scene version='1.0' version='2.0'/>", "<mesh xmlns='unrelated'/>", "<COLLADA xmlns='&unknown;' version='1.4.1'/>",
	"<!DOCTYPE irr_scene [<!ATTLIST irr_scene xmlns CDATA 'unrelated'>]><irr_scene/>",
	"<wrapper><X3D version='3.3'/></wrapper>", "<!-- <X3D version='3.3'/> -->", "<X3D xmlns='unrelated'/>",
	"<x:X3D xmlns:x='unrelated'/>", "<X3D version='3.3' version='4.0'/>"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    {
	const char *text = "<!DOCTYPE X3D PUBLIC 'ISO//Web3D//DTD X3D 3.3//EN' 'http://www.web3d.org/specifications/x3d-3.3.dtd'><X3D version='3.3' profile='Interchange'/>";
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/x3d+xml"));
	const auto &traits = result->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "version"), "3.3"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "x3d.profile"), "Interchange"));
	const char *const *uris = NULL;
	size_t count = 0;
	MIME_CHECK(!bu_mime_traits_get_set(&traits, "xml.root-namespaces", &uris, &count));
	bu_mime_result_free(result);
    }
    const std::string header = "<mesh xmlns='http://irrlicht.sourceforge.net/IRRMESH_09_2007' version='1.0'>";
    for (size_t size = 0; size < header.size(); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), size, 0, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_quick3d_pmx_profiles_test(bu_mime_registry *registry)
{
    for (const char *magic : {"quick3Do", "quick3Ds"}) {
	std::string header = std::string(magic) + "32";
	for (size_t size = 0; size <= header.size(); ++size) {
	    bu_mime_result *result = NULL;
	    bu_mime_status status = bu_mime_detect(registry, NULL, header.data(), size, 0, &result);
	    if (size < strlen(magic)) MIME_CHECK(status == BU_MIME_UNKNOWN_RESULT);
	    else {
		MIME_CHECK(status == BU_MIME_MATCH && BU_STR_EQUAL(result->candidates[0].name, "model/vnd.quick3d"));
		const auto &traits = result->candidates[0].traits;
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "quick3d.kind"), magic[7] == 'o' ? "object" : "scene"));
		const char *version = bu_mime_traits_get(&traits, "version");
		MIME_CHECK(size == header.size() ? version && BU_STR_EQUAL(version, "3.2") : !version);
	    }
	    bu_mime_result_free(result);
	}
    }
    for (bool utf8 : {false, true}) {
	unsigned char header[] = {'P', 'M', 'X', ' ', 0x66, 0x66, 0x06, 0x40, 8, static_cast<unsigned char>(utf8), 4, 1, 2, 4, 1, 2, 4};
	for (size_t size = 0; size <= sizeof(header); ++size) {
	    bu_mime_result *result = NULL;
	    bu_mime_status status = bu_mime_detect(registry, NULL, header, size, 0, &result);
	    if (size < 4) MIME_CHECK(status == BU_MIME_UNKNOWN_RESULT);
	    else {
		MIME_CHECK(status == BU_MIME_MATCH && BU_STR_EQUAL(result->candidates[0].name, "model/vnd.mikumikudance.pmx"));
		const auto &traits = result->candidates[0].traits;
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "byte-order"), "little-endian"));
		const char *version = bu_mime_traits_get(&traits, "version");
		MIME_CHECK(size >= 8 ? version && BU_STR_EQUAL(version, "2.1") : !version);
		const char *encoding = bu_mime_traits_get(&traits, "pmx.text-encoding");
		MIME_CHECK(size >= 10 ? encoding && BU_STR_EQUAL(encoding, utf8 ? "utf-8" : "utf-16le") : !encoding);
		const char *const *widths = NULL;
		size_t count = 0;
		MIME_CHECK(bu_mime_traits_get_set(&traits, "pmx.index-widths", &widths, &count) == (size == sizeof(header)));
		if (size == sizeof(header)) MIME_CHECK(count == 3 && BU_STR_EQUAL(widths[0], "1") && BU_STR_EQUAL(widths[1], "2") && BU_STR_EQUAL(widths[2], "4"));
	    }
	    bu_mime_result_free(result);
	}
    }
    const unsigned char future[] = {'P', 'M', 'X', ' ', 0, 0, 0x40, 0x40}; // IEEE binary32 3.0.
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, future, sizeof(future), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "version"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "pmx.version-bits"), "1077936128"));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_raw_profiles_test(bu_mime_registry *registry)
{
    const std::string triangle = "0 0 0 1 0 0 0 1 0\n";
    const struct {std::string text; const char *type; std::vector<const char *> features; bool shared = false;} cases[] = {
	{triangle, "model/vnd.povray.raw", {}},
	{"part\n" + triangle, "model/vnd.povray.raw", {"groups"}, true},
	{"first\n" + triangle + "second\n" + triangle, "model/vnd.povray.raw", {"groups", "multiple-groups"}, true},
	{"1 0 0 " + triangle, "model/vnd.povray.raw", {"face-colors"}},
	{"0 0 0 1 0 0 0 1 0 material.png\n", "model/vnd.povray.raw", {"textures"}},
	{"part thickf=0.5\n" + triangle, "model/vnd.iabg.raw", {"groups", "plate-thickness"}},
	{triangle + "0 1 2 3\n", "model/vnd.povray.raw", {"unknown-records"}},
	{"1 0 0 0 0 0 1 0 0 0 1 0 material.png\n", "model/vnd.povray.raw", {"colored-textures", "face-colors", "textures"}}};
    for (const auto &sample : cases)
	for (bool complete : {false, true}) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, "triangles.raw", sample.text.data(), sample.text.size(), complete, &result) ==
		(sample.shared ? BU_MIME_AMBIGUOUS : BU_MIME_MATCH));
	    const bu_mime_candidate *candidate = NULL;
	    bool shared_identity = false;
	    for (size_t i = 0; i < result->count; ++i) {
		if (BU_STR_EQUAL(result->candidates[i].name, sample.type)) candidate = &result->candidates[i];
		if (BU_STR_EQUAL(result->candidates[i].name, "model/vnd.iabg.raw") &&
		    result->candidates[i].evidence == BU_MIME_EVIDENCE_HEURISTIC) shared_identity = true;
	    }
	    MIME_CHECK(candidate && (!sample.shared || shared_identity));
	    const auto &traits = candidate->traits;
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "ascii"));
	    MIME_CHECK(!bu_mime_traits_get(&traits, "version"));
	    const char *const *members = NULL;
	    size_t count = 0;
	    MIME_CHECK(bu_mime_traits_get_set(&traits, "raw.features", &members, &count) == complete);
	    if (complete) {
		MIME_CHECK(count == sample.features.size());
		for (size_t i = 0; i < count; ++i) MIME_CHECK(BU_STR_EQUAL(members[i], sample.features[i]));
	    }
	    bu_mime_result_free(result);
	}
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, triangle.data(), triangle.size(), 1, &result) == BU_MIME_UNKNOWN_RESULT);
    bu_mime_result_free(result);
    const char *smd = "version 1\ntriangles\nmaterial\n0 0 0 0 0 0 1 0 0\n0 1 0 0 0 0 1 1 0\n0 0 1 0 0 0 1 0 1\nend\n";
    MIME_CHECK(bu_mime_detect(registry, "misnamed.raw", smd, strlen(smd), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/vnd.valve.smd"));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_motion_profiles_test(bu_mime_registry *registry)
{
    const struct {const char *text; const char *type; const char *version;} cases[] = {
	{"HIERARCHY\nROOT Hips\n{\n", "model/vnd.biovision.bvh", NULL},
	{"$Filename example.csm\n$Order\nmarker\n$Points\n", "model/vnd.autodesk.csm", NULL},
	{"LWMO\n3\n", "model/vnd.newtek.motion", "3"},
	{"LWMO\n", "model/vnd.newtek.motion", NULL},
	{"LWSC\n5\n", "image/x-lws", "5"}};
    for (const auto &sample : cases) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "misnamed.raw", sample.text, strlen(sample.text), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, sample.type));
	const auto &traits = result->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "ascii"));
	const char *version = bu_mime_traits_get(&traits, "version");
	MIME_CHECK(sample.version ? version && BU_STR_EQUAL(version, sample.version) : !version);
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_nff_profiles_test(bu_mime_registry *registry)
{
    const char *haines = "# Haines scene\nv\nfrom 0 0 5\nat 0 0 0\nup 0 1 0\np 3\n0 0 0\n1 0 0\n0 1 0\n";
    const struct {std::string text; const char *type; const char *version; const char *extension;} cases[] = {
	{haines, "model/prs.haines.nff", NULL, NULL},
	{std::string(haines) + "shader \"tess pb\"\n", "model/prs.haines.nff", NULL, "shader"},
	{std::string(haines) + "pb 4\n", "model/prs.haines.nff", NULL, "pb"},
	{"tess 2\ntet 0 0 0 1\n", "model/prs.haines.nff", NULL, "tess"},
	{"p 3\n0 0 0\n1 0 0\n0 1 0\n", "model/prs.haines.nff", NULL, NULL},
	{"nff\nversion 2.10\ntriangle\n3\n", "model/vnd.sense8.nff", "2.10", NULL},
	{"nff\ntriangle\n3\n", "model/vnd.sense8.nff", NULL, NULL},
	{"nff\nversion 2.0\nversion 2.10\nversion 2.0\n", "model/vnd.sense8.nff", NULL, NULL},
	{"nff\nversion invalid\n", "model/vnd.sense8.nff", NULL, NULL},
	{"// header comment\n nff\nversion 2.0\n", "model/vnd.sense8.nff", "2.0", NULL},
	{"\xef\xbb\xbfnff\r\nversion 2.0\r\n", "model/vnd.sense8.nff", "2.0", NULL}};
    for (const auto &sample : cases)
	for (bool complete : {false, true}) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, sample.text.data(), sample.text.size(), complete, &result) == BU_MIME_MATCH);
	    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, sample.type));
	    const auto &traits = result->candidates[0].traits;
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "ascii"));
	    const char *version = bu_mime_traits_get(&traits, "version");
	    MIME_CHECK(sample.version ? version && BU_STR_EQUAL(version, sample.version) : !version);
	    const char *const *members = NULL;
	    size_t count = 0;
	    bool known = complete && BU_STR_EQUAL(sample.type, "model/prs.haines.nff");
	    MIME_CHECK(bu_mime_traits_get_set(&traits, "nff.extensions", &members, &count) == known);
	    if (known) {
		if (sample.extension) {
		    bool found = false;
		    for (size_t i = 0; i < count; ++i) found = found || BU_STR_EQUAL(members[i], sample.extension);
		    MIME_CHECK(found);
		} else MIME_CHECK(!count);
	    }
	    bu_mime_result_free(result);
	}
    for (const char *text : {"nff-unrelated\n", "s zero one two three\n", "p 2\n0 0 0\n", "v\nunrelated 0 0 0\n", "# nff\n"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_mdl_profiles_test(bu_mime_registry *registry)
{
    const struct {const char *magic; const char *type; const char *version; const char *kind;} formats[] = {
	{"IDPO", "model/vnd.idsoftware.mdl", "6", NULL}, {"IDST", "model/vnd.valve.mdl", "10", "model"},
	{"IDSQ", "model/vnd.valve.mdl", "10", "sequence-group"},
	{"MDL2", "model/vnd.conitec.mdl", "2", NULL}, {"MDL3", "model/vnd.conitec.mdl", "3", NULL},
	{"MDL4", "model/vnd.conitec.mdl", "4", NULL}, {"MDL5", "model/vnd.conitec.mdl", "5", NULL},
	{"MDL7", "model/vnd.conitec.mdl", "7", NULL}, {"HMP4", "image/vnd.conitec.hmp", "4", NULL},
	{"HMP5", "image/vnd.conitec.hmp", "5", NULL}, {"HMP7", "image/vnd.conitec.hmp", "7", NULL}};
    for (const auto &format : formats) {
	const size_t magic_size = 4;
	unsigned char header[magic_size + sizeof(uint32_t)] = {0};
	memcpy(header, format.magic, magic_size);
	header[magic_size] = format.magic[0] == 'I' ? static_cast<unsigned char>(std::stoul(format.version)) : 99;
	for (size_t size = 0; size <= sizeof(header); ++size) {
	    bu_mime_result *result = NULL;
	    enum bu_mime_status status = bu_mime_detect(registry, NULL, header, size, 0, &result);
	    if (size < magic_size) {
		MIME_CHECK(status == BU_MIME_UNKNOWN_RESULT);
	    } else {
		MIME_CHECK(status == BU_MIME_MATCH && BU_STR_EQUAL(result->candidates[0].name, format.type));
		const auto &traits = result->candidates[0].traits;
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "byte-order"), "little-endian"));
		const char *version = bu_mime_traits_get(&traits, "version");
		if (format.magic[0] != 'I' || size == sizeof(header)) MIME_CHECK(version && BU_STR_EQUAL(version, format.version));
		else MIME_CHECK(!version && result->incomplete);
		if (format.kind) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "valve.mdl-kind"), format.kind));
	    }
	    bu_mime_result_free(result);
	}
    }
    unsigned char future[] = {'I', 'D', 'S', 'T', 48, 0, 0, 0};
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, "future.mdl", future, sizeof(future), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/vnd.valve.mdl"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "version"), "48"));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_fastgen_test(bu_mime_registry *registry)
{
    const char *decks[] = {
	"SECTION        1       1       2\n",
	"$COMMENT SECTION is a fixed-field card\r\nSECTION        1       1       1\r\n",
	"vehicle example\nCOMPSPLT       1       1       1       2     0.0\nSECTION        1       1       2"};
    for (const char *deck : decks) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "geometry.payload", deck, strlen(deck), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(result->count && BU_STR_EQUAL(result->candidates[0].name, "model/vnd.fastgen"));
	MIME_CHECK(result->candidates[0].evidence == BU_MIME_EVIDENCE_HEURISTIC);
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "fastgen.dialect"), "4"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "encoding"), "ascii"));
	MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "version"));
	bu_mime_result_free(result);
    }
    const char *unrelated[] = {"GRID           1             0.0     0.0     0.0\nENDDATA\n",
	"BEGIN BULK\nGRID,1,,0,0,0\nENDDATA\n", "$COMMENT SECTION        1       1       2\n",
	"SECTION prose about some geometry\n", "SECTION        1       1       3\n",
	"SECTION        1       1      12\n", "SECTION        x       1       2\n",
	"SECTION        1               2\n"};
    for (const char *text : unrelated) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    for (size_t size = 0; size < strlen(decks[0]); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, decks[0], size, 0, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    bu_mime_result *extension = NULL;
    MIME_CHECK(bu_mime_detect(registry, "nastran.bdf", NULL, 0, 1, &extension) != BU_MIME_ERROR);
    for (size_t i = 0; i < extension->count; ++i)
	MIME_CHECK(!BU_STR_EQUAL(extension->candidates[i].name, "model/vnd.fastgen"));
    bu_mime_result_free(extension);
    return 0;
}

static int
mime_cad_headers_test(bu_mime_registry *registry)
{
    const struct { const char *text; const char *major; const char *version; } jt_versions[] = {
	{"8.0", "8", "8.0"}, {"9.5", "9", "9.5"}, {"10.3", "10", "10.3"},
	{"11.0", "11", "11.0"}, {"08.00", "8", "8.0"},
	{"99999999999999999999999.02", "99999999999999999999999", "99999999999999999999999.2"}};
    const size_t jt_version_bytes = 80;
    for (const auto &version : jt_versions) {
	for (unsigned char order : {0, 1}) {
	    std::string header = std::string("Version ") + version.text;
	    header.resize(jt_version_bytes, ' ');
	    header += static_cast<char>(order);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), header.size(), 1, &result) == BU_MIME_MATCH);
	    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/jt"));
	    MIME_CHECK(result->candidates[0].evidence == BU_MIME_EVIDENCE_HEURISTIC);
	    const bu_mime_traits &traits = result->candidates[0].traits;
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "version"), version.version));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "jt.major-version"), version.major));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "byte-order"), order ? "big-endian" : "little-endian"));
	    bu_mime_result_free(result);
	    for (size_t prefix = 1; prefix < header.size(); ++prefix) {
		MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), prefix, 0, &result) != BU_MIME_ERROR);
		MIME_CHECK(result->incomplete);
		bu_mime_result_free(result);
	    }
	}
    }
    const char *rhino_versions[] = {"       1", "       2", "       4", "      50", "      80", "      90", "       X"};
    for (const char *version : rhino_versions) {
	std::string header = std::string("3D Geometry File Format ") + version;
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), header.size(), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/vnd.mcneel.3dm"));
	std::string expected = version;
	expected.erase(0, expected.find_first_not_of(' '));
	if (expected == "X") expected = "2";
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "version"), expected.c_str()));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "byte-order"), "little-endian"));
	bu_mime_result_free(result);
	for (size_t prefix = 1; prefix < header.size(); ++prefix) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), prefix, 0, &result) != BU_MIME_ERROR);
	    MIME_CHECK(result->incomplete);
	    bu_mime_result_free(result);
	}
    }
    for (const char *version : {"        ", "       0", "1       ", "      x1"}) {
	std::string header = std::string("3D Geometry File Format ") + version;
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), header.size(), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "version"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_jpeg_test(bu_mime_registry *registry)
{
    const unsigned char invalid_magic[] = {0xff, 0xd8, 0};
    bu_mime_result *invalid = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, invalid_magic, sizeof(invalid_magic), 1, &invalid) == BU_MIME_UNKNOWN_RESULT);
    bu_mime_result_free(invalid);
    const struct {
	unsigned char marker;
	unsigned char depth;
	std::vector<const char *> features;
    } samples[] = {
	{0xc0, 8, {}}, {0xc1, 12, {}}, {0xc2, 8, {"progressive"}}, {0xc3, 8, {"lossless"}},
	{0xc7, 8, {"differential", "lossless"}}, {0xc9, 8, {"arithmetic"}},
	{0xca, 8, {"arithmetic", "progressive"}}, {0xcb, 8, {"arithmetic", "lossless"}},
	{0xcf, 8, {"arithmetic", "differential", "lossless"}}
    };
    for (const auto &sample : samples) {
	const unsigned char header[] = {0xff, 0xd8, 0xff, 0xfe, 0, 4, 'x', 'x',
	    0xff, sample.marker, 0, 17, sample.depth, 0, 1, 0, 1, 3, 1, 0x11, 0, 2, 0x11, 0, 3, 0x11, 0};
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "misleading.png", header, sizeof(header), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "image/jpeg"));
	const bu_mime_traits &facts = result->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&facts, "bit-depth"), std::to_string(sample.depth).c_str()));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&facts, "channels"), "3"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&facts, "jpeg.sampling-factors"), "1x1,1x1,1x1"));
	const char *const *members = NULL;
	size_t count = 0;
	MIME_CHECK(bu_mime_traits_get_set(&facts, "jpeg.frame-features", &members, &count));
	MIME_CHECK(count == sample.features.size());
	for (size_t i = 0; i < count; ++i) MIME_CHECK(BU_STR_EQUAL(members[i], sample.features[i]));
	bu_mime_result_free(result);
	for (size_t prefix = 0; prefix < sizeof(header); ++prefix) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, header, prefix, 0, &result) != BU_MIME_ERROR);
	    for (size_t i = 0; i < result->count; ++i)
		MIME_CHECK(!bu_mime_traits_get_set(&result->candidates[i].traits, "jpeg.frame-features", &members, &count));
	    bu_mime_result_free(result);
	}
    }
    return 0;
}

static int
mime_pnm_test(bu_mime_registry *registry)
{
    const struct {
	const char *header;
	const char *encoding;
	const char *maxval;
	const char *depth;
    } samples[] = {
	{"P2\n# dimensions\n1 1\n15\n", "ascii", "15", NULL},
	{"P5\n1 1\n255\n", "binary", "255", "8"},
	{"P5\n1 1\n256\n", "binary", "256", "16"},
	{"P5\n1 1\n65535\n", "binary", "65535", "16"},
	{"P3\n# dimensions\n1 1\n65535\n", "ascii", "65535", NULL},
	{"P3# comment\n1 1\n15\n", "ascii", "15", NULL},
	{"P6\f1 1\v15\n", "binary", "15", "8"},
	{"P6\n1 1\n255\n", "binary", "255", "8"},
	{"P6\n1 1\n256\n", "binary", "256", "16"},
	{"P6\n1 1\n65535\n", "binary", "65535", "16"}
    };
    for (const auto &sample : samples) {
	size_t size = strlen(sample.header);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "misleading.stl", sample.header, size, 1, &result) == BU_MIME_MATCH);
	const bool gray = sample.header[1] == '2' || sample.header[1] == '5';
	const char *type = gray ? "image/x-portable-graymap" : "image/x-portable-pixmap";
	const char *maxval_key = gray ? "pgm.maxval" : "ppm.maxval";
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, type));
	const bu_mime_traits &traits = result->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), sample.encoding));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, maxval_key), sample.maxval));
	const char *depth = bu_mime_traits_get(&traits, "bit-depth");
	MIME_CHECK(sample.depth ? depth && BU_STR_EQUAL(depth, sample.depth) : !depth);
	const char *order = bu_mime_traits_get(&traits, "byte-order");
	MIME_CHECK(sample.depth && BU_STR_EQUAL(sample.depth, "16") ?
	    order && BU_STR_EQUAL(order, "big-endian") : !order);
	bu_mime_result_free(result);
	for (size_t prefix = 0; prefix < size; ++prefix) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, sample.header, prefix, 0, &result) != BU_MIME_ERROR);
	    for (size_t i = 0; i < result->count; ++i)
		MIME_CHECK(!bu_mime_traits_get(&result->candidates[i].traits, maxval_key));
	    bu_mime_result_free(result);
	}
    }
    const char *invalid[] = {"P5\n0 1\n255\n", "P5\n1 1\n65536\n", "P5\n1 1\n255# comment\n", "P6\n0 1\n255\n", "P6\n1 1\n0\n", "P6\n1 1\n65536\n",
	"P6\n1 1\n184467440737095516160\n", "P6\n1 1\n255X\n", "P6\n1 1\n255# comment\n"};
    for (const char *sample : invalid) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, sample, strlen(sample), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, sample[1] == '5' ? "pgm.maxval" : "ppm.maxval"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_container_scene_test(bu_mime_registry *registry)
{
    const struct {
	std::string data;
	const char *type;
	const char *version;
	const char *encoding;
	const char *order;
	bu_mime_evidence evidence;
    } samples[] = {
	{"nendo 1.0", "model/vnd.izware.nendo", "1.0", "binary", "big-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{"nendo 1.1", "model/vnd.izware.nendo", "1.1", "binary", "big-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{"nendo 1.2", "model/vnd.izware.nendo", "1.2", "binary", "big-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{"nendo 2.0", "model/vnd.izware.nendo", "2.0", "binary", "big-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{std::string("HEAD\x04\0\0\0\x01\0\0\0", 3 * sizeof(uint32_t)),
	    "model/vnd.nevercenter.sib", "1", "binary", "little-endian", BU_MIME_EVIDENCE_HEURISTIC},
	{std::string("HEAD\x04\0\0\0\x02\0\0\0", 3 * sizeof(uint32_t)),
	    "model/vnd.nevercenter.sib", "2", "binary", "little-endian", BU_MIME_EVIDENCE_HEURISTIC},
	{std::string("HEAD\x04\0\0\0\x03\0\0\0", 3 * sizeof(uint32_t)),
	    "model/vnd.nevercenter.sib", "3", "binary", "little-endian", BU_MIME_EVIDENCE_HEURISTIC},
	{"Caligari V00.01ALH", "model/vnd.caligari.cob", "00.01", "ascii", "little-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{"Caligari V00.01BLH", "model/vnd.caligari.cob", "00.01", "binary", "little-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{"Caligari V00.01BBH", "model/vnd.caligari.cob", "00.01", "binary", "big-endian", BU_MIME_EVIDENCE_SIGNATURE},
	{"Caligari V00.02ALH", "model/vnd.caligari.cob", "00.02", "ascii", "little-endian", BU_MIME_EVIDENCE_SIGNATURE}
    };
    for (const auto &sample : samples) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "scene.payload", sample.data.data(), sample.data.size(), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(result->count && BU_STR_EQUAL(result->candidates[0].name, sample.type));
	MIME_CHECK(result->candidates[0].evidence == sample.evidence);
	const struct { const char *key; const char *value; } values[] = {
	    {"version", sample.version}, {"encoding", sample.encoding}, {"byte-order", sample.order}};
	for (const auto &value : values) {
	    const char *observed = bu_mime_traits_get(&result->candidates[0].traits, value.key);
	    MIME_CHECK(observed && BU_STR_EQUAL(observed, value.value));
	}
	bu_mime_result_free(result);
	for (size_t size = 1; size < sample.data.size(); ++size) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, sample.data.data(), size, 0, &result) != BU_MIME_ERROR);
	    MIME_CHECK(result->incomplete);
	    bu_mime_result_free(result);
	}
    }
    const std::string invalid[] = {"nendo x.y", "Caligari V00.01QEH",
	std::string("HEAD\0\0\0\0\x01\0\0\0", 3 * sizeof(uint32_t)),
	std::string("HEAD\x08\0\0\0\x01\0\0\0", 3 * sizeof(uint32_t))};
    for (const auto &sample : invalid) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, sample.data(), sample.size(), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_vrml_test(bu_mime_registry *registry)
{
    const struct { const char *text; const char *version; const char *encoding; } samples[] = {
	{"#VRML V1.0 ascii\nSeparator {}\n", "1.0", "ascii"},
	{"#VRML V2.0 utf8\nShape {}\n", "2.0", "utf-8"},
	{"#VRML V2.1 utf8\nShape {}\n", "2.1", "utf-8"}
    };
    for (const auto &sample : samples) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "scene.payload", sample.text, strlen(sample.text), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(result->count && BU_STR_EQUAL(result->candidates[0].name, "model/vrml"));
	const char *version = bu_mime_traits_get(&result->candidates[0].traits, "version");
	const char *encoding = bu_mime_traits_get(&result->candidates[0].traits, "encoding");
	MIME_CHECK(version && BU_STR_EQUAL(version, sample.version));
	MIME_CHECK(encoding && BU_STR_EQUAL(encoding, sample.encoding));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_gltf_test(bu_mime_registry *registry)
{
    const struct { const char *text; int complete; int known; size_t members; } samples[] = {
	{"{\"asset\":{\"version\":\"2.0\"}}", 1, 1, 0},
	{"{\"asset\":{\"version\":\"2.0\"},\"extensionsRequired\":[\"KHR_mesh_quantization\",\"KHR_materials_unlit\"]}", 1, 1, 2},
	{"{\"asset\":{\"version\":\"2.0\"},\"extras\":{\"extensionsRequired\":[\"unrelated\"]}}", 1, 1, 0},
	{"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[", 0, 0, 0},
	{"{\"asset\":{\"version\":\"2.0\"},\"extensionsRequired\":[\"KHR_mesh_quantization\",", 0, 0, 0},
	{"{\"asset\":{\"version\":\"2.0\"},\"extensionsRequired\":[\"KHR_mesh_quantization\"],\"buffers\":[", 0, 1, 1},
	{"{\"asset\":{\"version\":\"2.0\"},\"extensionsRequired\":false}", 1, 0, 0}
    };
    for (const auto &sample : samples) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, sample.text, strlen(sample.text), sample.complete, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/gltf+json"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "encoding"), "utf-8"));
	const char *const *members = NULL;
	size_t count = 0;
	MIME_CHECK(bu_mime_traits_get_set(&result->candidates[0].traits, "gltf.required-extensions", &members, &count) == sample.known);
	MIME_CHECK(count == sample.members);
	bu_mime_result_free(result);
    }
    const struct {const char *fields; const char *extension; const char *version;} avatars[] = {
	{R"("extensions":{"VRM":{"specVersion":"0.0"}},"extensionsUsed":["VRM"])", "VRM", "0.0"},
	{R"("extensions":{"VRMC_vrm":{"specVersion":"1.0"}},"extensionsUsed":["VRMC_vrm"])", "VRMC_vrm", "1.0"},
	{R"("extensions":{"VRMC_vrm":{"specVersion":"1.0-draft"}})", "VRMC_vrm", "1.0-draft"},
	{R"("extensions":{"VRMC_vrm":{}})", "VRMC_vrm", nullptr},
	{R"("extras":{"extensions":{"VRMC_vrm":{"specVersion":"1.0"}}})", nullptr, nullptr},
	{R"("extensions":{"VRM":{},"VRMC_vrm":{"specVersion":"1.0"}})", nullptr, nullptr},
	{R"("extensions":{"VRMC_vrm":{"specVersion":"1.0","specVersion":"2.0"}})", nullptr, nullptr},
	{R"("extensions":{"VRMC_vrm":{"specVersion":7}})", "VRMC_vrm", nullptr}
    };
    for (const auto &sample : avatars) {
	std::string text = std::string(R"({"asset":{"version":"2.0"},)") + sample.fields + "}";
	bu_mime_result *identified = nullptr;
	MIME_CHECK(bu_mime_detect(registry, nullptr, text.data(), text.size(), 1, &identified) == BU_MIME_MATCH);
	const auto &traits = identified->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "version"), "2.0"));
	const char *extension = bu_mime_traits_get(&traits, "vrm.extension"), *version = bu_mime_traits_get(&traits, "vrm.version");
	MIME_CHECK(sample.extension ? extension && BU_STR_EQUAL(extension, sample.extension) : !extension);
	MIME_CHECK(sample.version ? version && BU_STR_EQUAL(version, sample.version) : !version);
	bu_mime_result_free(identified);
    }
    for (const auto &sample : samples) {
	std::string text(sample.text);
	const std::string key = "extensionsRequired";
	size_t at = text.find(key);
	if (at != std::string::npos) text.replace(at, key.size(), "extensionsUsed");
	bu_mime_result *identified = nullptr;
	MIME_CHECK(bu_mime_detect(registry, nullptr, text.data(), text.size(), sample.complete, &identified) == BU_MIME_MATCH);
	const char *const *members = nullptr;
	size_t count = 0;
	MIME_CHECK(bu_mime_traits_get_set(&identified->candidates[0].traits, "gltf.used-extensions", &members, &count) == sample.known);
	MIME_CHECK(count == sample.members);
	bu_mime_result_free(identified);
    }
    std::string json = "{\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],\"extensions\":{\"VRMC_vrm\":{\"specVersion\":\"1.0\"}},\"asset\":{\"version\":\"2.0\",\"minVersion\":\"2.0\"}}";
    const size_t json_offset = 20;
    std::vector<unsigned char> glb(json_offset + json.size(), 0);
    memcpy(glb.data(), "glTF", 4);
    glb[4] = 2;
    size_t total = glb.size(), length = json.size();
    for (size_t i = 0; i < 4; ++i) {
	glb[8+i] = static_cast<unsigned char>(total >> (i * 8));
	glb[12+i] = static_cast<unsigned char>(length >> (i * 8));
    }
    memcpy(glb.data() + 16, "JSON", 4);
    memcpy(glb.data() + json_offset, json.data(), json.size());
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, glb.data(), glb.size(), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/gltf-binary"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "encoding"), "binary"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "byte-order"), "little-endian"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "gltf.container-version"), "2"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "vrm.extension"), "VRMC_vrm"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "vrm.version"), "1.0"));
    const char *const *members = NULL;
    size_t count = 0;
    MIME_CHECK(bu_mime_traits_get_set(&result->candidates[0].traits, "gltf.required-extensions", &members, &count) && count == 1);
    MIME_CHECK(BU_STR_EQUAL(members[0], "KHR_draco_mesh_compression"));
    bu_mime_result_free(result);
    size_t prefix_size = json_offset + json.find(",\"asset\"");
    MIME_CHECK(bu_mime_detect(registry, NULL, glb.data(), prefix_size, 0, &result) == BU_MIME_MATCH);
    MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "version"));
    MIME_CHECK(bu_mime_traits_get_set(&result->candidates[0].traits, "gltf.required-extensions", &members, &count) && count == 1);
    bu_mime_result_free(result);
    return 0;
}

static int
mime_step_test(bu_mime_registry *registry)
{
    const struct {
	const char *schema;
	const char *ap;
	const char *edition;
    } samples[] = {
	{"CONFIG_CONTROL_DESIGN", "203", "1"},
	{"CONFIG_CONTROL_DESIGN','GEOMETRIC_VALIDATION_PROPERTIES_MIM", "203", "2"},
	{"AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF", "203", "2"},
	{"AUTOMOTIVE_DESIGN {1 0 10303 214 3 1 1}", "214", "3"},
	{"AUTOMOTIVE_DESIGN {1 0 10303 214 1 1 1 1}", "214", "3"},
	{" AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF { 1 0 10303 442 1 1 4 } ", "242", "1"},
	{"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 3 1 4}", "242", "2"},
	{"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 4 1 4}", "242", "3"},
	{"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 5 1 4}", "242", "4"},
	{"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 7 1 4}", "242", "4"},
	{"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF", "242", NULL},
	{"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 999 1 4}", "242", NULL}
    };
    for (const auto &sample : samples) {
	std::string text = "ISO-10303-21; HEADER; FILE_DESCRIPTION((''),'2;1'); /* FILE_SCHEMA(('FAKE')); */ FILE_SCHEMA(('";
	text += sample.schema;
	text += "')); ENDSEC; DATA; ENDSEC; END-ISO-10303-21;";
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "wrong.png", text.data(), text.size(), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/step"));
	const bu_mime_traits &traits = result->candidates[0].traits;
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "step.ap"), sample.ap));
	const char *edition = bu_mime_traits_get(&traits, "step.edition");
	MIME_CHECK(sample.edition ? edition && BU_STR_EQUAL(edition, sample.edition) : !edition);
	const char *const *profiles = NULL;
	size_t profile_count = 0;
	MIME_CHECK(bu_mime_traits_get_set(&traits, "step.profiles", &profiles, &profile_count) == (sample.edition != NULL));
	MIME_CHECK(profile_count == (sample.edition ? 1 : 0));
	bu_mime_result_free(result);
	for (size_t n = 0; n < text.size(); ++n) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), n, 0, &result) != BU_MIME_ERROR);
	    bu_mime_result_free(result);
	}
    }
    const char *multiple = "ISO-10303-21; HEADER; FILE_SCHEMA(('AUTOMOTIVE_DESIGN','AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF')); ENDSEC;";
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, multiple, strlen(multiple), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(!bu_mime_traits_get(&result->candidates[0].traits, "step.ap"));
    MIME_CHECK(bu_mime_traits_valid(&result->candidates[0].traits, 1));
    const char *const *members = NULL;
    size_t count = 0;
    MIME_CHECK(bu_mime_traits_get_set(&result->candidates[0].traits, "step.application-protocols", &members, &count) && count == 2);
    MIME_CHECK(BU_STR_EQUAL(members[0], "214") && BU_STR_EQUAL(members[1], "242"));
    bu_mime_result_free(result);
    const char *mixed = "ISO-10303-21; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN','IFC4')); ENDSEC;";
    MIME_CHECK(bu_mime_detect(registry, "misleading.step", mixed, strlen(mixed), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "application/p21"));
    MIME_CHECK(bu_mime_traits_get_set(&result->candidates[0].traits, "p21.schemas", &members, &count) && count == 2);
    bu_mime_result_free(result);
    const char *combined = "ISO-10303-21; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN','AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF { 1 0 10303 442 1 1 4 }')); ENDSEC;";
    MIME_CHECK(bu_mime_detect(registry, NULL, combined, strlen(combined), 1, &result) == BU_MIME_MATCH);
    const char *profiles[] = {"ap203e1", "ap242e1"};
    const bu_mime_trait supported[] = {BU_MIME_TRAIT_SET("step.profiles", profiles, 2)};
    const bu_mime_capability capability = BU_MIME_CAPABILITY("model/step", supported, 1, BU_MIME_READ);
    MIME_CHECK(bu_mime_capability_match(&capability, &result->candidates[0].traits, BU_MIME_INPUT));
    const bu_mime_trait ap242_only[] = {BU_MIME_TRAIT_SET("step.profiles", profiles + 1, 1)};
    const bu_mime_capability limited = BU_MIME_CAPABILITY("model/step", ap242_only, 1, BU_MIME_READ);
    MIME_CHECK(!bu_mime_capability_match(&limited, &result->candidates[0].traits, BU_MIME_INPUT));
    bu_mime_result_free(result);
    const char *other = "ISO-10303-21; HEADER; FILE_SCHEMA(('IFC4')); ENDSEC;";
    MIME_CHECK(bu_mime_detect(registry, "misleading.step", other, strlen(other), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "application/p21"));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "p21.schema"), "IFC4"));
    bu_mime_result_free(result);
    const struct {const char *declaration; size_t schemas;} declarations[] = {
	{"FILE_SCHEMA(('IFC2X3'));", 1}, {"FILE_SCHEMA(('IFC2X3','IFC4'));", 2},
	{"FILE_SCHEMA(('IFC2X3'',''IFC4'));", 1}, {"FILE_SCHEMA('('('IFC2X3'));", 0},
	{"FILE_SCHEMA(('IFC2X3',));", 0}, {"FILE_SCHEMA(('IFC2X3')';'", 0},
	{"/* FILE_SCHEMA(('IFC2X3')); */", 0},
	{"FILE_DESCRIPTION(('FILE_SCHEMA((''IFC2X3''));'),'2;1');", 0}
    };
    for (const auto &sample : declarations) {
	std::string text = std::string("ISO-10303-21; HEADER; ") + sample.declaration + " ENDSEC;";
	MIME_CHECK(bu_mime_detect(registry, "sample.ifc", text.data(), text.size(), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "application/p21"));
	const auto &traits = result->candidates[0].traits;
	MIME_CHECK(bu_mime_traits_get_set(&traits, "p21.schemas", &members, &count) == (sample.schemas != 0));
	MIME_CHECK(count == sample.schemas);
	MIME_CHECK(!bu_mime_traits_get(&traits, "encoding"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_catalog_test(bu_mime_registry *registry)
{
    const struct { const char *extension; const char *type; } samples[] = {
	{"step", "model/step"}, {"stl", "model/stl"}, {"obj", "model/obj"},
	{"igs", "model/iges"}, {"iges", "model/iges"},
	{"fcstd", "application/x-extension-fcstd"}, {"scad", "application/x-openscad"},
	{"ply", "application/x-ply"}, {"3dm", "model/vnd.mcneel.3dm"},
	{"g", "model/vnd.brlcad"}, {"asc", "application/vnd.brlcad.script"}, {"asc", "application/x-ascii-grid"},
	{"shp", "application/vnd.shp"}, {"dxf", "image/vnd.dxf"},
	{"acc", "model/vnd.inivis.ac3d"}, {"off", "model/vnd.geomview.off"}, {"amf", "model/amf"},
	{"ase", "model/vnd.autodesk.ase"}, {"ask", "model/vnd.autodesk.ase"},
	{"zae", "model/vnd.collada+xml+zip"}, {"ifc", "application/p21"}, {"ifczip", "application/zip"},
	{"b3d", "model/vnd.blitzresearch.b3d"}, {"iqm", "model/vnd.interquake.iqm"},
	{"md2", "model/vnd.idsoftware.md2"}, {"md3", "model/vnd.idsoftware.md3"},
	{"lws", "image/x-lws"}, {"pbrt", "model/vnd.pbrt.scene"},
	{"mdc", "model/vnd.idsoftware.mdc"}, {"ms3d", "model/vnd.chumbalumsoft.ms3d"},
	{"ndo", "model/vnd.izware.nendo"}, {"sib", "model/vnd.nevercenter.sib"},
	{"cob", "model/vnd.caligari.cob"}, {"scn", "model/vnd.caligari.cob"},
	{"ter", "application/terragen"}, {"terrain", "application/terragen"},
	{"kap", "image/bsb"}, {"cub", "application/x-isis3"}, {"hgt", "application/x-srtmhgt"},
	{"kro", "application/x-kro"}, {"lcp", "application/x-lcp"}, {"grd", "application/x-nwt-grd"},
	{"grc", "application/x-nwt-grc"}, {"hdr", "image/x-mff"}, {"hdr", "application/x-coasp"},
	{"hdr", "application/x-snodas"}, {"ACE2", "application/x-ace2"}, {"gsb", "application/x-ntv2"}
    };
    bu_vls name = BU_VLS_INIT_ZERO;
    for (const auto &sample : samples) {
	bu_mime_result *result = NULL;
	std::string filename = std::string("sample.") + sample.extension;
	MIME_CHECK(bu_mime_detect(registry, filename.c_str(), NULL, 0, 0, &result) != BU_MIME_ERROR);
	bool found = false;
	for (size_t i = 0; i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, sample.type)) {
		found = true;
		MIME_CHECK(result->candidates[i].evidence == BU_MIME_EVIDENCE_EXTENSION && !result->candidates[i].traits.count);
	    }
	MIME_CHECK(found);
	bu_mime_result_free(result);
    }
    const unsigned char zip_prefix[] = {'P', 'K', 3, 4, 0, 0, 0, 0};
    for (const char *filename : {static_cast<const char *>(nullptr), "example.zae"}) {
	bu_mime_result *identified = nullptr;
	MIME_CHECK(bu_mime_detect(registry, filename, zip_prefix, sizeof(zip_prefix), 0, &identified) != BU_MIME_ERROR);
	bool found = false;
	for (size_t i = 0; i < identified->count; ++i) {
	    if (!BU_STR_EQUAL(identified->candidates[i].name, "model/vnd.collada+xml+zip")) continue;
	    found = true;
	    MIME_CHECK(identified->candidates[i].evidence == BU_MIME_EVIDENCE_EXTENSION);
	    MIME_CHECK(!bu_mime_traits_get(&identified->candidates[i].traits, "version") &&
		!bu_mime_traits_get(&identified->candidates[i].traits, "collada.document-version"));
	}
	MIME_CHECK(found == (filename != nullptr));
	bu_mime_result_free(identified);
    }
    bu_mime_result *candidates = NULL;
    MIME_CHECK(bu_mime_resolve_candidates(registry, "stl", &candidates) == BU_MIME_AMBIGUOUS);
    MIME_CHECK(candidates && candidates->count > 1);
    bu_mime_result_free(candidates);
    MIME_CHECK(bu_mime_canonical(registry, "application/arcgrid", &name) && BU_STR_EQUAL(bu_vls_cstr(&name), "application/x-ascii-grid"));
    MIME_CHECK(!bu_mime_canonical(registry, "image/vnd.esri.ascii-grid", &name));
    MIME_CHECK(bu_mime_canonical(registry, "application/iges", &name));
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&name), "model/iges"));
    MIME_CHECK(bu_mime_canonical(registry, "application/brlcad-v4", &name));
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&name), "model/vnd.brlcad"));
    MIME_CHECK(bu_mime_canonical(registry, "application/brlcad-v5", &name));
    MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&name), "model/vnd.brlcad"));
    const char *retired[] = {"model/vnd.brlcad+binary", "model/vnd.brlcad+asc", "model/dxf", "model/ply", "model/vnd.vtk"};
    for (const char *type : retired)
	MIME_CHECK(!bu_mime_canonical(registry, type, &name));
    MIME_CHECK(BU_STR_EQUAL(bu_mime_legacy_name(BU_MIME_MODEL_PLY, BU_MIME_MODEL), "application/x-ply"));
    bu_vls_free(&name);
    return 0;
}

static int
mime_brlcad_profiles_test(bu_mime_registry *registry)
{
    const unsigned char v5[] = {0x76, 1, 0, 0, 0, 0, 1, 0x35};
    unsigned char v4[128] = {'I', 1, 'v', '4'};
    const struct { const void *data; size_t size; const char *version; } samples[] = {
	{v5, sizeof(v5), "5"}, {v4, sizeof(v4), "4"}
    };
    for (const auto &sample : samples) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "database.payload", sample.data, sample.size, 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(result && result->count && BU_STR_EQUAL(result->candidates[0].name, "model/vnd.brlcad"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "version"), sample.version));
	bu_mime_result_free(result);
    }
    const struct { const char *text; const char *syntax; } scripts[] = {
	{"# comment\ntitle {ASC test}\n", "tcl-commands"}, {"I 1 v4\nASC test\n", "legacy-records"}
    };
    for (const auto &script : scripts) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "script.payload", script.text, strlen(script.text), 1, &result) == BU_MIME_MATCH);
	MIME_CHECK(result && result->count && BU_STR_EQUAL(result->candidates[0].name, "application/vnd.brlcad.script"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "asc.syntax"), script.syntax));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_sun_test(bu_mime_registry *registry)
{
    /* Header declarations remain observable even for unfamiliar raster/map
     * representations, independently of pixel decoder availability. */
    const unsigned char bytes[] = {
	0x59, 0xa6, 0x6a, 0x95, 0, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 24,
	0, 0, 0, 18, 0, 0, 3, 0xe7, 0, 0, 0, 77, 0, 0, 0, 0
    };
    for (size_t size = 0; size <= sizeof(bytes); ++size) {
	bu_mime_result *result = NULL;
	const auto status = bu_mime_detect(registry, NULL, bytes, size, 0, &result);
	MIME_CHECK(status != BU_MIME_ERROR && result);
	if (size < sizeof(uint32_t)) {
	    MIME_CHECK(!result->count && (size == 0 || result->incomplete));
	} else {
	    MIME_CHECK(result->count && BU_STR_EQUAL(result->candidates[0].name, "image/x-sun-raster"));
	    const auto &traits = result->candidates[0].traits;
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
	    const struct {const char *key; const char *value; size_t end;} fields[] = {
		{"bit-depth", "24", 4 * sizeof(uint32_t)},
		{"sun.raster-type", "999", 6 * sizeof(uint32_t)},
		{"sun.map-type", "77", 7 * sizeof(uint32_t)}
	    };
	    for (const auto &field : fields) {
		const char *value = bu_mime_traits_get(&traits, field.key);
		MIME_CHECK(size < field.end ? !value : BU_STR_EQUAL(value, field.value));
	    }
	    MIME_CHECK(size == sizeof(bytes) || result->incomplete);
	}
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_stl_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "model/stl")) return &result->candidates[i].traits;
	return NULL;
    };
    for (bool bom : {false, true}) {
	const std::string data = std::string(bom ? "\xef\xbb\xbf" : "") +
	    "solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid triangle\n";
	const size_t header_end = data.find("normal") + strlen("normal") + 1;
	for (size_t size = 0; size <= data.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result);
	    MIME_CHECK((traits != NULL) == (size >= header_end));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "stl.serialization"), "text"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "stl.utf8-bom"), bom ? "true" : "false"));
		const char *encoding = bu_mime_traits_get(traits, "encoding");
		MIME_CHECK(bom ? BU_STR_EQUAL(encoding, "utf-8") : size == data.size() ? BU_STR_EQUAL(encoding, "ascii") : !encoding);
		MIME_CHECK(!bu_mime_traits_get(traits, "byte-order"));
	    }
	    bu_mime_result_free(result);
	}
    }
    for (const char *text : {"SOLID triangle\rFACET NORMAL ", "sOlId triangle\nFaCeT nOrMaL ", "solid triangle\nfacet normal \xc3\xa9"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) != BU_MIME_ERROR);
	const auto *traits = find(result);
	MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "stl.serialization"), "text"));
	const char *encoding = bu_mime_traits_get(traits, "encoding");
	MIME_CHECK(strstr(text, "\xc3\xa9") ? !encoding : BU_STR_EQUAL(encoding, "ascii"));
	bu_mime_result_free(result);
    }
    constexpr size_t header_size = 84, count_at = 80, record_size = 50;
    std::vector<unsigned char> data(header_size + record_size, 0);
    memcpy(data.data(), "solid binary STL", strlen("solid binary STL"));
    data[count_at] = 1;
    for (size_t size = 0; size <= data.size(); ++size) {
	bu_mime_result *result = NULL;
	const bu_mime_probe_input input = {data.data(), size, size == data.size(), data.size(), 1};
	MIME_CHECK(bu_mime_identify(registry, NULL, &input, &result) != BU_MIME_ERROR);
	const auto *traits = find(result);
	MIME_CHECK((traits != NULL) == (size >= header_size));
	if (traits) {
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary") &&
		BU_STR_EQUAL(bu_mime_traits_get(traits, "stl.serialization"), "binary") &&
		BU_STR_EQUAL(bu_mime_traits_get(traits, "byte-order"), "little-endian"));
	    MIME_CHECK(!bu_mime_traits_get(traits, "stl.utf8-bom"));
	}
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_grid_families_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result, const char *type) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type)) return &result->candidates[i].traits;
	return NULL;
    };
    const std::string grass = "north: 50\nsouth: 49\neast: 11\nwest: 10\nrows: 2\ncols: 2\n";
    std::string grass_crlf;
    for (char c : grass) {if (c == '\n') grass_crlf += '\r'; grass_crlf += c;}
    const std::string surfer = "DSAA\n2 2\n0 1\n0 1\n1 4\n";
    const std::string gxf = "#POINTS\n2\n#ROWS\n2\n#GRID\n";
    const std::string gxf_inline = "#POINTS 2\n#ROWS 2\n#GTYPE 2\n#GRID\n";
    const std::string zmap_start = "@Original, GRID, 4\n";
    const std::string zmap = zmap_start + "20,-9999,,7,1\n";
    const struct {std::string header; const char *type; size_t signature_size;} cases[] = {
	{grass, "application/grass-ascii-grid", grass.size()},
	{grass_crlf, "application/grass-ascii-grid", grass_crlf.size() - 1},
	{surfer, "image/vnd.goldensoftware.surfer-grid", strlen("DSAA")},
	{gxf, "application/x-gxf", gxf.size()},
	{gxf_inline, "application/x-gxf", gxf_inline.size()},
	{zmap, "application/x-zmap", zmap_start.size()}
    };
    for (const auto &sample : cases) {
	const std::string text = sample.header + "1 2\n3 4\n";
	for (size_t size = 0; size <= text.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), size, size == text.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result, sample.type);
	    MIME_CHECK((traits != NULL) == (size >= sample.signature_size));
	    if (traits) {
		MIME_CHECK(!bu_mime_traits_get(traits, "version"));
		if (BU_STR_EQUAL(sample.type, "application/x-gxf")) {
		    const bool inline_values = sample.header == gxf_inline;
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "gxf.inline-header-values"), inline_values ? "true" : "false"));
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "gxf.gtype"), inline_values ? "2" : "0"));
		}
		if (size == text.size()) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "ascii"));
		else MIME_CHECK(!bu_mime_traits_get(traits, "encoding"));
	    }
	    bu_mime_result_free(result);
	}
	for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	    const auto data = mime_text_bytes(text, width, true, true);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), data.size(), 1, &result) == BU_MIME_MATCH);
	    const auto *traits = find(result, sample.type);
	    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "text.code-unit-width"), std::to_string(width * 8).c_str()));
	    bu_mime_result_free(result);
	}
    }
    for (bool tagged : {false, true}) {
	std::vector<unsigned char> bytes(12, 0);
	memcpy(bytes.data(), tagged ? "DSRB" : "DSBB", 4);
	bytes[4] = 4; bytes[8] = 99;
	for (size_t size = 0; size <= bytes.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), size, size == bytes.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result, "image/vnd.goldensoftware.surfer-grid");
	    MIME_CHECK((traits != NULL) == (size >= 4));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "surfer.layout"), tagged ? "tagged" : "classic") &&
		    BU_STR_EQUAL(bu_mime_traits_get(traits, "surfer.serialization"), "binary") &&
		    BU_STR_EQUAL(bu_mime_traits_get(traits, "bit-depth"), tagged ? "64" : "32") &&
		    BU_STR_EQUAL(bu_mime_traits_get(traits, "sample-format"), "ieee754"));
		MIME_CHECK(!bu_mime_traits_get(traits, "version"));
		if (tagged && size == bytes.size()) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "surfer.header-version"), "99"));
	    }
	    bu_mime_result_free(result);
	}
    }
    for (bool labels : {false, true}) {
	for (bool historical : {false, true}) {
	    constexpr size_t record_size = 80, designator_size = 5;
	    const size_t prefix_size = labels ? 2 * record_size : 0;
	    const size_t field_offset = historical ? 118 : 59;
	    const size_t description_offset = prefix_size + record_size;
	    std::vector<unsigned char> bytes(description_offset + field_offset + designator_size, ' ');
	    if (labels) {memcpy(bytes.data(), "VOL1", 4); memcpy(bytes.data() + record_size, "HDR1", 4);}
	    memcpy(bytes.data() + prefix_size, historical ? "UHL1 " : "UHL10", 5);
	    memcpy(bytes.data() + description_offset, "DSI", 3);
	    memcpy(bytes.data() + description_offset + field_offset, "DTED9", designator_size);
	    for (size_t size = 0; size <= bytes.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), size, size == bytes.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result, "application/dted");
		MIME_CHECK((traits != NULL) == (size >= description_offset + 3));
		if (labels && (size == record_size || size == prefix_size)) MIME_CHECK(result->incomplete);
		if (traits) {
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary") &&
			BU_STR_EQUAL(bu_mime_traits_get(traits, "bit-depth"), "16") &&
			BU_STR_EQUAL(bu_mime_traits_get(traits, "byte-order"), "big-endian"));
		    MIME_CHECK(!bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "dted.integer-representation"));
		    if (size == bytes.size()) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "dted.level"), "9"));
		}
		bu_mime_result_free(result);
	    }
	}
    }
    for (const char *gtype : {"", "0", "2", "99"}) {
	const std::string text = std::string("#POINTS\n2\n#ROWS\n2\n#GTYPE\n") + gtype + "\n#GRID\n1 2\n3 4\n";
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), text.size(), 1, &result) != BU_MIME_ERROR);
	const auto *traits = find(result, "application/x-gxf");
	MIME_CHECK(traits);
	if (*gtype) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "gxf.gtype"), gtype));
	else MIME_CHECK(!bu_mime_traits_get(traits, "gxf.gtype"));
	const char *layout = bu_mime_traits_get(traits, "gxf.value-encoding");
	if (!*gtype || BU_STR_EQUAL(gtype, "99")) MIME_CHECK(!layout);
	else MIME_CHECK(layout && BU_STR_EQUAL(layout, BU_STR_EQUAL(gtype, "0") ? "decimal" : "base90"));
	bu_mime_result_free(result);
    }
    {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "grid.gxf", NULL, 0, 0, &result) == BU_MIME_AMBIGUOUS);
	MIME_CHECK(find(result, "application/gxf") && find(result, "application/x-gxf"));
	bu_mime_result_free(result);
    }
    for (const auto &sample : {std::pair<std::string, const char *>{gxf, "application/x-gxf"}, {zmap, "application/x-zmap"}}) {
	std::string malformed = sample.first;
	malformed.insert(malformed.find('\n'), 1, '\0');
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, malformed.data(), malformed.size(), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!find(result, sample.second));
	bu_mime_result_free(result);
    }
    const std::string xyz = "0 0 1\n1 0 2\n0 1 3\n1 1 4\n";
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, xyz.data(), xyz.size(), 1, &result) != BU_MIME_ERROR && !find(result, "application/x-ascii-xyz"));
    bu_mime_result_free(result);
    MIME_CHECK(bu_mime_detect(registry, "data.xyz", xyz.data(), xyz.size(), 1, &result) == BU_MIME_AMBIGUOUS);
    MIME_CHECK(find(result, "application/x-ascii-xyz") && find(result, "chemical/x-xyz"));
    bu_mime_result_free(result);
    bu_vls canonical = BU_VLS_INIT_ZERO;
    for (const auto &alias : {std::pair<const char *, const char *>{"application/x-xyz", "application/x-ascii-xyz"},
	{"application/x-gsag", "image/vnd.goldensoftware.surfer-grid"}, {"application/x-gsbg", "image/vnd.goldensoftware.surfer-grid"},
	{"application/x-gs7bg", "image/vnd.goldensoftware.surfer-grid"}, {"image/hfa", "application/x-erdas-hfa"}, {"application/aaigrid", "application/x-ascii-grid"}}) {
	MIME_CHECK(bu_mime_canonical(registry, alias.first, &canonical) && BU_STR_EQUAL(bu_vls_cstr(&canonical), alias.second));
    }
    MIME_CHECK(!bu_mime_canonical(registry, "image/vnd.vtp.bt", &canonical));
    bu_vls_free(&canonical);
    return 0;
}

static int
mime_terrain_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result, const char *type) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type)) return &result->candidates[i].traits;
	return NULL;
    };
    const char pcidsk[] = "PCIDSK  producer version 9.9";
    for (size_t size = 0; size < sizeof(pcidsk); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, "wrong.g", pcidsk, size, size + 1 == sizeof(pcidsk), &result) != BU_MIME_ERROR);
	const auto *traits = find(result, "application/x-pcidsk");
	MIME_CHECK((traits != NULL) == (size >= strlen("PCIDSK  ")));
	if (traits) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary") &&
	    !bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "bit-depth") && !bu_mime_traits_get(traits, "byte-order"));
	bu_mime_result_free(result);
    }
    const char hfa[] = "EHFA_HEADER_TAG\0\0producer version 9.9";
    for (size_t size = 0; size < sizeof(hfa); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, hfa, size, size + 1 == sizeof(hfa), &result) != BU_MIME_ERROR);
	const auto *traits = find(result, "application/x-erdas-hfa");
	MIME_CHECK((traits != NULL) == (size >= strlen("EHFA_HEADER_TAG")));
	if (traits) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary") && !bu_mime_traits_get(traits, "version"));
	bu_mime_result_free(result);
    }
    for (const auto &layout : {std::pair<unsigned char, unsigned char>{2, 0}, {4, 0}, {4, 1}, {8, 7}}) {
	std::vector<unsigned char> bt(256, 0);
	const char magic[] = "binterr9.9";
	memcpy(bt.data(), magic, sizeof(magic) - 1);
	bt[18] = layout.first; bt[20] = layout.second;
	for (size_t size = 0; size <= bt.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bt.data(), size, size == bt.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result, "application/x-bt");
	    MIME_CHECK((traits != NULL) == (size >= strlen("binterr")));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary") &&
		    BU_STR_EQUAL(bu_mime_traits_get(traits, "byte-order"), "little-endian"));
		if (size >= sizeof(magic) - 1) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "version"), "9.9"));
		else MIME_CHECK(!bu_mime_traits_get(traits, "version"));
		if (size >= 22) {
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "bt.sample-size"), std::to_string(layout.first).c_str()) &&
			BU_STR_EQUAL(bu_mime_traits_get(traits, "bt.float-flag"), std::to_string(layout.second).c_str()));
		    if (layout.first == 8) MIME_CHECK(!bu_mime_traits_get(traits, "bit-depth") && !bu_mime_traits_get(traits, "sample-format"));
		    else MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "bit-depth"), layout.first == 2 ? "16" : "32") &&
			BU_STR_EQUAL(bu_mime_traits_get(traits, "sample-format"), layout.second ? "ieee754" : "signed-integer"));
		}
	    }
	    bu_mime_result_free(result);
	}
    }
    for (bool center : {false, true})
	for (bool rectangular : {false, true}) {
	    const std::string header = std::string("ncols 2\nnrows 2\n") +
		(center ? "xllcenter 10\nyllcenter 50\n" : "xllcorner 10\nyllcorner 50\n") +
		(rectangular ? "dx 0.01\ndy 0.02\n" : "cellsize 0.01\n");
	    const std::string text = header + "1 2\n3 4\n";
	    for (size_t size = 0; size <= text.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, "wrong.g", text.data(), size, size == text.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result, "application/x-ascii-grid");
		MIME_CHECK((traits != NULL) == (size >= header.size()));
		if (traits) {
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "esri-grid.serialization"), "text"));
		    MIME_CHECK(!bu_mime_traits_get(traits, "version"));
		    if (size == text.size()) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "ascii"));
		    else MIME_CHECK(!bu_mime_traits_get(traits, "encoding"));
		}
		bu_mime_result_free(result);
	    }
	    for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
		const auto data = mime_text_bytes(text, width, true, true);
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), data.size(), 1, &result) == BU_MIME_MATCH);
		const auto *traits = find(result, "application/x-ascii-grid");
		MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "text.code-unit-width"), std::to_string(width * 8).c_str()) &&
		    BU_STR_EQUAL(bu_mime_traits_get(traits, "esri-grid.text-bom"), "true"));
		bu_mime_result_free(result);
	    }
	}
    for (const char *text : {"ncols 2\n", "ncols 0\nnrows 2\nxllcorner 1\nyllcorner 2\ncellsize 1\n",
	"ncols 1.5\nnrows 2\nxllcorner 1\nyllcorner 2\ncellsize 1\n", "ncols 2\nnrows 2\nxllcenter 1\nyllcorner 2\ncellsize 1\n",
	"ncols 2\nnrows 2\nxllcorner nan\nyllcorner 2\ncellsize 1\n", "ncols 2\nnrows 2\nxllcorner 1\nyllcorner 2\ncellsize 0\n"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) != BU_MIME_ERROR && !find(result, "application/x-ascii-grid"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_obj_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "model/obj")) return &result->candidates[i].traits;
	return NULL;
    };
    const std::string geometry = "# mtllib fake.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf -3//1 -2//1 -1//1\n";
    const std::string reference = "mtllib actual.mtl\n";
    for (size_t width : {size_t(1), size_t(2), size_t(4)})
	for (bool little : {false, true}) {
	    const bool bom = width > 1 || little;
	    const auto prefix = mime_text_bytes(geometry, width, little, width == 1 && bom);
	    const auto data = mime_text_bytes(geometry + reference, width, little, width == 1 && bom);
	    for (size_t size = 0; size <= data.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result);
		MIME_CHECK((traits != NULL) == (size >= prefix.size()));
		if (traits) {
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "obj.serialization"), "text") &&
			BU_STR_EQUAL(bu_mime_traits_get(traits, "text.code-unit-width"), std::to_string(width * 8).c_str()) &&
			BU_STR_EQUAL(bu_mime_traits_get(traits, "obj.text-bom"), bom ? "true" : "false"));
		    MIME_CHECK(!bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "obj.materials"));
		    const char *materials = bu_mime_traits_get(traits, "obj.material-library-reference");
		    if (size == data.size()) {
			MIME_CHECK(BU_STR_EQUAL(materials, "true") && BU_STR_EQUAL(bu_mime_traits_get(traits, "text.ascii-only"), "true"));
			if (width == 1) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), bom ? "utf-8" : "ascii"));
		    } else MIME_CHECK(!materials && !bu_mime_traits_get(traits, "text.ascii-only"));
		}
		bu_mime_result_free(result);
	    }
	}
    {
	const std::string prefix = reference + geometry + "# unfinished";
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, prefix.data(), prefix.size(), 0, &result) == BU_MIME_MATCH);
	const auto *traits = find(result);
	MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "obj.material-library-reference"), "true") &&
	    !bu_mime_traits_get(traits, "encoding") && !bu_mime_traits_get(traits, "text.ascii-only"));
	bu_mime_result_free(result);
    }
    for (const char *ending : {"", "# caf\xc3\xa9\n", "# bad \xff\n"}) {
	const std::string text = geometry + ending;
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), text.size(), 1, &result) == BU_MIME_MATCH);
	const auto *traits = find(result);
	MIME_CHECK(traits);
	if (!*ending) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "ascii") &&
	    BU_STR_EQUAL(bu_mime_traits_get(traits, "obj.material-library-reference"), "false"));
	else if (strstr(ending, "caf")) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "utf-8") &&
	    BU_STR_EQUAL(bu_mime_traits_get(traits, "text.ascii-only"), "false"));
	else MIME_CHECK(!bu_mime_traits_get(traits, "encoding") && !bu_mime_traits_get(traits, "text.ascii-only"));
	bu_mime_result_free(result);
    }
    for (const char *text : {"mtllib only.mtl\n", "# v 0 0 0\n# f 1 2 3\n", "v nan 0 0\nf 1 2 3\n",
	"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 2 3\n", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/ 2 3\n",
	"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\nordinary text\n"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) != BU_MIME_ERROR && !find(result));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_opengex_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "model/vnd.opengex")) return &result->candidates[i].traits;
	return NULL;
    };
    const std::string start = "/* GeometryObject $fake {} */\nMetric(key=\"distance\") {float {1}}\nGeometryNode $node\n{";
    for (size_t width : {size_t(1), size_t(2), size_t(4)})
	for (bool little : {false, true}) {
	    const bool bom = width > 1 || little;
	    const auto prefix = mime_text_bytes(start, width, little, width == 1 && bom);
	    const auto data = mime_text_bytes(start + "\nName {string {\"GeometryObject $quoted {}\"}}\n}\n", width, little, width == 1 && bom);
	    for (size_t size = 0; size <= data.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result);
		MIME_CHECK((traits != NULL) == (size >= prefix.size()));
		if (traits) {
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "opengex.serialization"), "text"));
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "opengex.text-bom"), bom ? "true" : "false"));
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "text.code-unit-width"), std::to_string(width * 8).c_str()));
		    MIME_CHECK(!bu_mime_traits_get(traits, "version"));
		    if (size == data.size()) {
			MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "text.ascii-only"), "true"));
			if (width == 1) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "utf-8"));
		    } else MIME_CHECK(!bu_mime_traits_get(traits, "text.ascii-only"));
		}
		bu_mime_result_free(result);
	    }
	}
    for (const char *text : {"Metric(key=\"distance\") {float{1}}", "// GeometryNode $node {}\n", "/* GeometryObject $node {} */",
	"Generic {GeometryNode $nested {}}", "Generic {string {\"GeometryObject $quoted {}\"}}", "{GeometryNode $nested {}}",
	"GeometryNodeOther $node {}"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!find(result));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_fbx_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "application/vnd.autodesk.fbx")) return &result->candidates[i].traits;
	return NULL;
    };
    const unsigned char magic[] = "Kaydara FBX Binary  \0\x1a";
    for (const auto &entry : {std::pair<uint32_t, const char *>{7400, "7400"}, {7500, "7500"}, {9900, "9900"}}) {
	std::vector<unsigned char> data(magic, magic + sizeof(magic));
	for (size_t byte = 0; byte < sizeof(entry.first); ++byte) data.push_back(static_cast<unsigned char>(entry.first >> (8 * byte)));
	for (size_t size = 0; size <= data.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result);
	    MIME_CHECK((traits != NULL) == (size >= sizeof(magic)));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "fbx.serialization"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "byte-order"), "little-endian"));
		const char *version = bu_mime_traits_get(traits, "fbx.binary-header-version-code");
		MIME_CHECK(size == data.size() ? BU_STR_EQUAL(version, entry.second) : !version);
		MIME_CHECK(!bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "fbx.document-version-code"));
	    }
	    bu_mime_result_free(result);
	}
    }
    for (size_t width : {size_t(1), size_t(2), size_t(4)})
	for (bool little : {false, true}) {
	    const bool bom = width > 1 || little;
	    const std::string start = "; FBX 1.0.0; FBXHeaderExtension: { is a producer comment\rFBXHeaderExtension ; comment\r: {";
	    const auto prefix = mime_text_bytes(start, width, little, width == 1 && bom);
	    const auto data = mime_text_bytes(start + "\nFBXVersion: 7500\n}\n", width, little, width == 1 && bom);
	    for (size_t size = 0; size <= data.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result);
		MIME_CHECK((traits != NULL) == (size >= prefix.size()));
		if (traits) {
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "fbx.serialization"), "text"));
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "text.code-unit-width"), std::to_string(width * 8).c_str()));
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "fbx.text-bom"), bom ? "true" : "false"));
		    MIME_CHECK(!bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "fbx.document-version-code"));
		    const char *encoding = bu_mime_traits_get(traits, "encoding");
		    if (!bom) MIME_CHECK(size == data.size() ? BU_STR_EQUAL(encoding, "ascii") : !encoding);
		}
		bu_mime_result_free(result);
	    }
	}
    for (const char *text : {"; FBXHeaderExtension: {\n", "\"FBXHeaderExtension: {\"", "Objects: {FBXHeaderExtension: {}}", "FBXHeaderExtensionOther: {}"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!find(result));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_3ds_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "image/x-3ds")) return &result->candidates[i].traits;
	return NULL;
    };
    constexpr size_t header_size = sizeof(uint16_t) + sizeof(uint32_t);
    for (const auto &id : {std::vector<unsigned char>{0x4d, 0x4d}, std::vector<unsigned char>{0x3d, 0xc2}}) {
	std::vector<unsigned char> data = id;
	data.insert(data.end(), {header_size + 2, 0, 0, 0, 0, 0});
	for (size_t size = 0; size <= data.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result);
	    MIME_CHECK((traits != NULL) == (size >= header_size));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "byte-order"), "little-endian"));
		MIME_CHECK(!bu_mime_traits_get(traits, "version"));
	    }
	    bu_mime_result_free(result);
	}
	for (unsigned char length : {0, 2, 5, 9}) {
	    data[sizeof(uint16_t)] = length;
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), data.size(), 1, &result) != BU_MIME_ERROR);
	    MIME_CHECK(!find(result));
	    bu_mime_result_free(result);
	}
    }
    std::vector<unsigned char> tiff = {'M', 'M', 0, 42, 0, 0, 0, 8};
    /* This TIFF's size also makes its first six bytes a plausible 3DS
     * root header. Its stronger signature must retain priority. */
    const size_t plausible_3ds_length = 42 << 8;
    tiff.resize(plausible_3ds_length);
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, tiff.data(), tiff.size(), 1, &result) == BU_MIME_MATCH);
    MIME_CHECK(result && result->count && BU_STR_EQUAL(result->candidates[0].name, "image/tiff") &&
	result->candidates[0].evidence == BU_MIME_EVIDENCE_SIGNATURE);
    MIME_CHECK(find(result));
    for (size_t i = 0; i < result->count; ++i)
	if (BU_STR_EQUAL(result->candidates[i].name, "image/x-3ds"))
	    MIME_CHECK(result->candidates[i].evidence == BU_MIME_EVIDENCE_HEURISTIC);
    bu_mime_result_free(result);
    return 0;
}

static int
mime_ply_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "application/x-ply")) return &result->candidates[i].traits;
	return NULL;
    };
    for (const char *format : {"ascii", "binary_little_endian", "binary_big_endian"})
	for (const char *newline : {"\n", "\r", "\r\n"}) {
	    const std::string ending = newline;
	    const std::string header = "ply" + ending + "format " + format + " 1.7" + ending;
	    const std::string data = header + "end_header" + ending;
	    const size_t first_end = sizeof("ply"), format_end = header.size() - ending.size() + 1;
	    for (size_t size = 0; size <= data.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result);
		MIME_CHECK((traits != NULL) == (size >= first_end));
		if (traits) {
		    const char *version = bu_mime_traits_get(traits, "version");
		    const char *encoding = bu_mime_traits_get(traits, "encoding");
		    const char *order = bu_mime_traits_get(traits, "byte-order");
		    if (size < format_end) MIME_CHECK(!version && !encoding && !order);
		    else {
			const bool binary = !BU_STR_EQUAL(format, "ascii");
			MIME_CHECK(BU_STR_EQUAL(version, "1.7") && BU_STR_EQUAL(encoding, binary ? "binary" : "ascii"));
			MIME_CHECK(binary ? BU_STR_EQUAL(order, BU_STR_EQUAL(format, "binary_big_endian") ? "big-endian" : "little-endian") : !order);
		    }
		}
		bu_mime_result_free(result);
	    }
	}
    const struct {const char *text; const char *version; const char *encoding;} fixtures[] = {
	{"PLY\rformat ascii 1.0\r", "1.0", "ascii"}, {"pLy\nformat binary_big_endian 1.0\n", "1.0", "binary"},
	{"ply\nformat unknown 9.9\n", "9.9", NULL}, {"ply\nformat ascii\n", NULL, NULL},
	{"ply\ncomment format ascii 1.0\n", NULL, NULL}, {"ply\nformat ascii 1.0", "1.0", "ascii"}
    };
    for (const auto &fixture : fixtures) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, fixture.text, strlen(fixture.text), 1, &result) != BU_MIME_ERROR);
	const auto *traits = find(result);
	MIME_CHECK(traits);
	const char *version = bu_mime_traits_get(traits, "version"), *encoding = bu_mime_traits_get(traits, "encoding");
	MIME_CHECK(fixture.version ? BU_STR_EQUAL(version, fixture.version) : !version);
	MIME_CHECK(fixture.encoding ? BU_STR_EQUAL(encoding, fixture.encoding) : !encoding);
	bu_mime_result_free(result);
    }
    const std::string invalid = std::string("ply\nformat ascii 1.") + '\0' + "0\n";
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, invalid.data(), invalid.size(), 1, &result) != BU_MIME_ERROR);
    const auto *traits = find(result);
    MIME_CHECK(traits && !bu_mime_traits_get(traits, "version"));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_x_assbin_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result, const char *type) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type)) return &result->candidates[i].traits;
	return NULL;
    };
    for (const char *format : {"txt ", "bin ", "tzip", "bzip"})
	for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	    const bool binary = format[0] == 'b', compressed = format[1] == 'z';
	    const std::string header = std::string("xof 0303") + format + "0064";
	    const auto data = mime_text_bytes(header + "\nbody\n", width, true);
	    const size_t bom = width == 1 ? 0 : width;
	    for (size_t size = 0; size <= data.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result, "model/vnd.microsoft.x");
		MIME_CHECK((traits != NULL) == (size >= bom + width * 4));
		if (traits) {
		    const std::string units = std::to_string(width * 8);
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "x.header-code-unit-width"), units.c_str()));
		    const struct {const char *key; const char *value; size_t end;} fields[] = {
			{"version", "3.3", 8}, {"x.version-code", "0303", 8},
			{"x.serialization", binary ? "binary" : "text", 12}, {"compression", compressed ? "mszip" : "none", 12},
			{"x.float-width", "64", 16}
		    };
		    for (const auto &field : fields) {
			const char *value = bu_mime_traits_get(traits, field.key);
			MIME_CHECK(size < bom + width * field.end ? !value : BU_STR_EQUAL(value, field.value));
		    }
		    const char *encoding = bu_mime_traits_get(traits, "encoding");
		    if (size < bom + width * 12) MIME_CHECK(!encoding);
		    else if (binary || compressed) MIME_CHECK(BU_STR_EQUAL(encoding, "binary"));
		    else if (width > 1) MIME_CHECK(BU_STR_EQUAL(encoding, width == 2 ? "utf-16le" : "utf-32le"));
		    else MIME_CHECK(size == data.size() ? BU_STR_EQUAL(encoding, "ascii") : !encoding);
		}
		bu_mime_result_free(result);
	    }
	}
    for (const char *text : {"xof 9909wat 0080", "xof xx03txt 0032"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text, strlen(text), 1, &result) == BU_MIME_MATCH);
	const auto *traits = find(result, "model/vnd.microsoft.x");
	MIME_CHECK(traits && !bu_mime_traits_get(traits, text[4] == 'x' ? "version" : "x.serialization"));
	bu_mime_result_free(result);
    }
    const char magic[] = "ASSIMP.binary-dump.";
    std::vector<unsigned char> header(magic, magic + sizeof(magic) - 1);
    header.resize(64);
    header[44] = 1;
    header[52] = 7;
    header[56] = 0x20;
    header[62] = 1;
    for (bool big : {false, true}) {
	auto ordered = header;
	if (big) {
	    for (size_t at : {size_t(44), size_t(48), size_t(52), size_t(56)})
		std::reverse(ordered.begin() + at, ordered.begin() + at + sizeof(uint32_t));
	    for (size_t at : {size_t(60), size_t(62)})
		std::reverse(ordered.begin() + at, ordered.begin() + at + sizeof(uint16_t));
	}
	for (size_t size = 0; size <= header.size(); ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, ordered.data(), size, size == ordered.size(), &result) != BU_MIME_ERROR);
	    const auto *traits = find(result, "model/vnd.assimp.binary");
	    MIME_CHECK((traits != NULL) == (size >= sizeof(magic) - 1));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary"));
		const char *order = bu_mime_traits_get(traits, "byte-order");
		MIME_CHECK(size < 52 ? !order : BU_STR_EQUAL(order, big ? "big-endian" : "little-endian"));
		const struct {const char *key; const char *value; size_t end;} fields[] = {
		    {"version", "1.0", 52}, {"assimp.producer-revision", "7", 56}, {"assimp.compile-flags", "32", 60},
		    {"assimp.real-width", "64", 60}, {"assimp.shortened", "false", 62}, {"compression", "zlib", 64}
		};
		for (const auto &field : fields) {
		    const char *value = bu_mime_traits_get(traits, field.key);
		    MIME_CHECK(size < field.end ? !value : BU_STR_EQUAL(value, field.value));
		}
	    }
	    bu_mime_result_free(result);
	}
	for (size_t offset : {size_t(44), size_t(60), size_t(62)}) {
	    auto unknown = ordered;
	    const size_t width = offset == 44 ? sizeof(uint32_t) : sizeof(uint16_t);
	    unknown[offset + (big ? width - 1 : 0)] = 2;
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, unknown.data(), unknown.size(), 1, &result) == BU_MIME_MATCH);
	    const auto *traits = find(result, "model/vnd.assimp.binary");
	    MIME_CHECK(traits);
	    if (offset == 44) MIME_CHECK(!bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "byte-order") &&
		BU_STR_EQUAL(bu_mime_traits_get(traits, "assimp.major-word-le"), big ? "33554432" : "2") && !bu_mime_traits_get(traits, "assimp.real-width"));
	    else MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, offset == 60 ? "assimp.shortened" : "compression"),
		offset == 60 ? "true" : "zlib"));
	    bu_mime_result_free(result);
	}
    }
    return 0;
}

static int
mime_scene_text_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result, const char *type) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type)) return &result->candidates[i].traits;
	return NULL;
    };
    const struct {const char *header; const char *type; const char *key; const char *version; size_t identity;} formats[] = {
	{"AC3Db\n", "model/vnd.inivis.ac3d", "ac3d.serialization", "11", sizeof("AC3D") - 1},
	{"*3DSMAX_ASCIIEXPORT 200\n", "model/vnd.autodesk.ase", "ase.serialization", "200", sizeof("*3DSMAX_ASCIIEXPORT ") - 1}
    };
    for (const auto &format : formats)
	for (const char *encoding : {"ascii", "utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"}) {
	    const std::string name = encoding;
	    const size_t width = name.find("16") != std::string::npos ? 2 : name.find("32") != std::string::npos ? 4 : 1;
	    const bool little = name.find("le") != std::string::npos;
	    const size_t bom_size = width == 1 ? name == "utf-8" ? 3 : 0 : width;
	    const std::string text = std::string(format.header) + "scene body\n";
	    const auto data = mime_text_bytes(text, width, little, name == "utf-8");
	    for (size_t size = 0; size <= data.size(); ++size) {
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), size, size == data.size(), &result) != BU_MIME_ERROR);
		const auto *traits = find(result, format.type);
		MIME_CHECK((traits != NULL) == (size >= bom_size + width * format.identity));
		if (traits) {
		    const std::string units = std::to_string(width * 8);
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, format.key), "text"));
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "text.code-unit-width"), units.c_str()));
		    const char *version = bu_mime_traits_get(traits, "version");
		    MIME_CHECK(size < bom_size + width * strlen(format.header) ? !version : BU_STR_EQUAL(version, format.version));
		    const char *charset = bu_mime_traits_get(traits, "encoding");
		    MIME_CHECK(name == "ascii" && size < data.size() ? !charset : BU_STR_EQUAL(charset, encoding));
		}
		bu_mime_result_free(result);
	    }
	}
    const char *valid_utf8[] = {"\xc2\xa2", "\xe2\x82\xac", "\xf0\x9f\x98\x80"};
    const char *invalid_utf8[] = {"\xe9", "\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\x80", "\xc2", "\xe2\x82"};
    for (bool valid : {false, true}) {
	const char *const *samples = valid ? valid_utf8 : invalid_utf8;
	const size_t count = valid ? sizeof(valid_utf8) / sizeof(valid_utf8[0]) : sizeof(invalid_utf8) / sizeof(invalid_utf8[0]);
	for (size_t i = 0; i < count; ++i) {
	    const std::string text = std::string("AC3Db\nname ") + samples[i];
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), text.size(), 1, &result) == BU_MIME_MATCH);
	    const auto *traits = find(result, "model/vnd.inivis.ac3d");
	    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "version"), "11"));
	    const char *encoding = bu_mime_traits_get(traits, "encoding");
	    MIME_CHECK(valid ? BU_STR_EQUAL(encoding, "utf-8") : !encoding);
	    bu_mime_result_free(result);
	}
    }
    const std::string comment = "*COMMENT \"*3DSMAX_ASCIIEXPORT 999\"\n*3DSMAX_ASCIIEXPORT 200\n";
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, comment.data(), comment.size(), 1, &result) == BU_MIME_MATCH);
    const auto *traits = find(result, "model/vnd.autodesk.ase");
    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "version"), "200"));
    bu_mime_result_free(result);
    for (const char *other : {"\"AC3Db\"", "*COMMENT \"*3DSMAX_ASCIIEXPORT 999\"\n", "\"*3DSMAX_ASCIIEXPORT 200\""}) {
	result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, other, strlen(other), 1, &result) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    const unsigned char b3d[] = {'B', 'B', '3', 'D', 4, 0, 0, 0, 1, 0, 0, 0};
    for (size_t size = 0; size <= sizeof(b3d); ++size) {
	result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, b3d, size, size == sizeof(b3d), &result) != BU_MIME_ERROR);
	traits = find(result, "model/vnd.blitzresearch.b3d");
	MIME_CHECK((traits != NULL) == (size >= sizeof(uint32_t)));
	if (traits) {
	    const char *length = bu_mime_traits_get(traits, "b3d.payload-size");
	    MIME_CHECK(size < 2 * sizeof(uint32_t) ? !length : BU_STR_EQUAL(length, "4"));
	    const char *version = bu_mime_traits_get(traits, "version");
	    const char *code = bu_mime_traits_get(traits, "b3d.version-code");
	    MIME_CHECK(size < sizeof(b3d) ? !version && !code : BU_STR_EQUAL(version, "0.01") && BU_STR_EQUAL(code, "1"));
	}
	bu_mime_result_free(result);
    }
    for (unsigned char length : {0, 3, 4}) {
	std::vector<unsigned char> bytes(b3d, b3d + sizeof(b3d));
	bytes[sizeof(uint32_t)] = length;
	std::fill(bytes.begin() + 2 * sizeof(uint32_t), bytes.end(), 0xff);
	result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) == BU_MIME_MATCH);
	traits = find(result, "model/vnd.blitzresearch.b3d");
	MIME_CHECK(traits && !bu_mime_traits_get(traits, "version"));
	const char *code = bu_mime_traits_get(traits, "b3d.version-code");
	MIME_CHECK(length < sizeof(uint32_t) ? !code : BU_STR_EQUAL(code, "4294967295"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_blender_test(bu_mime_registry *registry)
{
    const auto find = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "application/x-blender")) return &result->candidates[i].traits;
	return NULL;
    };
    const struct {const char *header; const char *format; const char *width; const char *order; const char *version; const char *raw;} samples[] = {
	{"BLENDER_v250", "0", "32", "little-endian", "2.50", "250"},
	{"BLENDER_V279", "0", "32", "big-endian", "2.79", "279"},
	{"BLENDER-v305", "0", "64", "little-endian", "3.5", "305"},
	{"BLENDER-V405", "0", "64", "big-endian", "4.5", "405"},
	{"BLENDER17-01v0405", "1", "64", "little-endian", "4.5", "0405"},
	{"BLENDER17-01v0500", "1", "64", "little-endian", "5.0", "0500"}
    };
    for (const auto &sample : samples) {
	const size_t length = strlen(sample.header);
	for (size_t size = 0; size <= length; ++size) {
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, sample.header, size, size == length, &result) != BU_MIME_ERROR);
	    const auto *traits = find(result);
	    MIME_CHECK((traits != NULL) == (size >= sizeof("BLENDER") - 1));
	    if (traits) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(traits, "compression"), "none"));
		const struct {const char *key; const char *value;} fields[] = {
		    {"blender.format-version", sample.format}, {"blender.pointer-width", sample.width},
		    {"byte-order", sample.order}, {"version", sample.version}, {"blender.file-version", sample.raw}
		};
		for (const auto &field : fields) {
		    const char *value = bu_mime_traits_get(traits, field.key);
		    MIME_CHECK(size < length ? !value : BU_STR_EQUAL(value, field.value));
		}
	    }
	    if (size && size < length) MIME_CHECK(result->incomplete);
	    bu_mime_result_free(result);
	}
    }
    const char *unknown[] = {"BLENDER17-02v0500", "BLENDER19-01v0500xx", "BLENDER?x250", "BLENDER_v2xx", "BLENDER_x250"};
    for (const char *header : unknown) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, header, strlen(header), 1, &result) == BU_MIME_MATCH);
	const auto *traits = find(result);
	MIME_CHECK(traits && !bu_mime_traits_get(traits, "version") && !bu_mime_traits_get(traits, "blender.format-version"));
	MIME_CHECK(!bu_mime_traits_get(traits, "blender.pointer-width") && !bu_mime_traits_get(traits, "byte-order"));
	bu_mime_result_free(result);
    }
    const unsigned char gzip_header[] = {0x1f, 0x8b, 0x08, 0, 0, 0, 0, 0, 0, 0};
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, gzip_header, sizeof(gzip_header), 1, &result) != BU_MIME_ERROR);
    MIME_CHECK(!find(result));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_amf_test(bu_mime_registry *registry)
{
    const auto traits = [](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "model/amf")) return &result->candidates[i].traits;
	return NULL;
    };
    const std::string text = "<?xml version='1.0'?><!-- <amf unit='inch'/> --><amf version='1.1' unit='milli&#109;eter'/>";
    for (size_t size = 0; size <= text.size(); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), size, size == text.size(), &result) != BU_MIME_ERROR);
	const auto *facts = traits(result);
	MIME_CHECK((facts != NULL) == (size == text.size()));
	if (facts) {
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "container"), "none"));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "amf.unit-declaration"), "millimeter"));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "version"), "1.1"));
	}
	bu_mime_result_free(result);
    }
    const char *unknown[] = {"<?xml version='1.0'?><amf/>",
	"<!DOCTYPE amf [<!ENTITY unit 'inch'>]><amf unit='&unit;'/>",
	"<amf><object unit='inch'/></amf>"};
    for (const char *source : unknown) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, source, strlen(source), 1, &result) == BU_MIME_MATCH);
	const auto *facts = traits(result);
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "amf.unit-declaration") && !bu_mime_traits_get(facts, "version"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "container"), "none"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_dxf_test(bu_mime_registry *registry)
{
    const char *type = "image/vnd.dxf";
    const std::string text = "999\n$ACADVER AC9999 in a comment\n0\nSECTION\n2\nHEADER\n"
	"9\n$ACADVER\n1\nAC1027\n9\n$DWGCODEPAGE\n3\nANSI_1252\n0\nENDSEC\n";
    const auto field_end = [&](const char *field) {return text.find(field) + strlen(field);};
    const size_t identity_end = field_end("2\nHEADER\n");
    const size_t revision_end = field_end("1\nAC1027\n");
    const size_t codepage_end = field_end("3\nANSI_1252\n");
    const auto traits = [&](const bu_mime_result *result) -> const bu_mime_traits * {
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type)) return &result->candidates[i].traits;
	return NULL;
    };
    for (size_t size = 0; size <= text.size(); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, text.data(), size, size == text.size(), &result) != BU_MIME_ERROR);
	const auto *facts = traits(result);
	MIME_CHECK((facts != NULL) == (size >= identity_end));
	if (facts) {
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "dxf.serialization"), "text"));
	    MIME_CHECK((bu_mime_traits_get(facts, "dxf.acad-version") != NULL) == (size >= revision_end));
	    MIME_CHECK((bu_mime_traits_get(facts, "dxf.codepage") != NULL) == (size >= codepage_end));
	    if (size >= revision_end) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "dxf.acad-version"), "AC1027"));
	    if (size >= codepage_end) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "dxf.codepage"), "ANSI_1252"));
	    MIME_CHECK(!bu_mime_traits_get(facts, "version") && !bu_mime_traits_get(facts, "encoding"));
	}
	bu_mime_result_free(result);
    }
    const char binary[] = "AutoCAD Binary DXF\r\n\x1a";
    for (size_t size = 0; size <= sizeof(binary); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, binary, size, size == sizeof(binary), &result) != BU_MIME_ERROR);
	const auto *facts = traits(result);
	MIME_CHECK((facts != NULL) == (size == sizeof(binary)));
	if (facts) {
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "dxf.serialization"), "binary"));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "binary"));
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "byte-order"), "little-endian"));
	    MIME_CHECK(!bu_mime_traits_get(facts, "dxf.acad-version"));
	} else if (size) MIME_CHECK(result->incomplete);
	bu_mime_result_free(result);
    }
    for (const std::string &ending : {std::string("\n"), std::string("\r\n"), std::string("\r")}) {
	std::string encoded = "\xef\xbb\xbf";
	for (char c : text) encoded += c == '\n' ? ending : std::string(1, c);
	encoded += "0" + ending + "SECTION" + ending + "2" + ending + "ENTITIES" + ending +
	    "9" + ending + "$ACADVER" + ending + "1" + ending + "AC9999" + ending;
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, encoded.data(), encoded.size(), 1, &result) == BU_MIME_MATCH);
	const auto *facts = traits(result);
	MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "utf-8"));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "dxf.acad-version"), "AC1027"));
	bu_mime_result_free(result);
    }
    const std::string conflict = "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1027\n9\n$ACADVER\n1\nAC9999\n0\nENDSEC\n";
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, conflict.data(), conflict.size(), 1, &result) == BU_MIME_MATCH);
    const auto *facts = traits(result);
    MIME_CHECK(facts && !bu_mime_traits_get(facts, "dxf.acad-version") && !bu_mime_traits_valid(facts, 1));
    bu_mime_result_free(result);
    return 0;
}

static int
mime_off_test(bu_mime_registry *registry)
{
    const std::string text = "STCN4nOFF\n3\n3 1 3\n0 0 0\n";
    const size_t keyword_end = text.find('\n');
    const size_t dimension_end = text.find('\n', keyword_end + 1);
    for (size_t prefix = 0; prefix <= text.size(); ++prefix) {
	bu_mime_result *result = NULL;
	bu_mime_probe_input input = {};
	input.data = reinterpret_cast<const unsigned char *>(text.data());
	input.size = prefix;
	input.complete = prefix == text.size();
	MIME_CHECK(bu_mime_identify(registry, NULL, &input, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = NULL;
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "model/vnd.geomview.off")) facts = &result->candidates[i].traits;
	MIME_CHECK((facts != NULL) == (prefix > keyword_end));
	if (facts) {
	    const char *const *members = NULL;
	    size_t count = 0;
	    const char *attributes[] = {"colors", "homogeneous-coordinates", "normals", "texture-coordinates"};
	    MIME_CHECK(bu_mime_traits_get_set(facts, "off.vertex-attributes", &members, &count) &&
		count == sizeof(attributes) / sizeof(attributes[0]));
	    for (size_t i = 0; i < count; ++i) MIME_CHECK(BU_STR_EQUAL(members[i], attributes[i]));
	    const char *dimension = bu_mime_traits_get(facts, "off.dimension");
	    MIME_CHECK((dimension != NULL) == (prefix > dimension_end));
	    if (dimension) MIME_CHECK(BU_STR_EQUAL(dimension, "3"));
	    MIME_CHECK(!bu_mime_traits_get(facts, "version"));
	}
	bu_mime_result_free(result);
    }
    const struct {
	const char *header;
	const char *encoding;
	const char *dimension;
	const char *comments;
	bool dimension_word;
    } cases[] = {
	{"OFF\n3 1 3\n0 0 0\n", "ascii", "3", "false", false},
	{"\xef\xbb\xbfOFF\n3 1 3\n0 0 0\n", "utf-8", "3", "false", false},
	{"# comment\nnOFF\n4\n3 1 3\n0 0 0 0\n", "ascii", "4", "true", false},
	{"OFF BINARY\n", "binary", "3", "false", false},
	{"nOFF BINARY\n", "binary", "4", "false", true}
    };
    for (const auto &fixture : cases) {
	std::string data = fixture.header;
	if (fixture.dimension_word) data.append("\0\0\0\4", sizeof(uint32_t));
	const bool binary = BU_STR_EQUAL(fixture.encoding, "binary");
	if (binary) {
	    const char counts[] = "\0\0\0\3\0\0\0\1\0\0\0\3";
	    data.append(counts, sizeof(counts) - 1);
	}
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, data.data(), data.size(), 1, &result) == BU_MIME_MATCH);
	const bu_mime_traits *facts = NULL;
	for (size_t i = 0; result && i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, "model/vnd.geomview.off")) facts = &result->candidates[i].traits;
	MIME_CHECK(facts);
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), fixture.encoding));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "off.dimension"), fixture.dimension));
	MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "off.header-comments"), fixture.comments));
	if (binary) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "byte-order"), "big-endian"));
	bu_mime_result_free(result);
    }
    return 0;
}

static const bu_mime_traits *
mime_result_traits(const bu_mime_result *result, const char *type)
{
    for (size_t i = 0; result && i < result->count; ++i)
	if (BU_STR_EQUAL(result->candidates[i].name, type)) return &result->candidates[i].traits;
    return NULL;
}

static int
mime_product_test(struct bu_mime_registry *registry)
{
    namespace fixture = mime_product_fixture;
    for (size_t width : {size_t(1), size_t(2), size_t(4)})
	for (bool little : {false, true}) {
	    const auto bytes = mime_text_bytes("<X3D/>", width, little, width == 1);
	    const std::string data(bytes.begin(), bytes.end());
	    for (bool truncated : {false, true}) {
		std::istringstream input(data + (truncated ? std::string("\xc0") : ""));
		const auto root = mime_xml::inspect(input);
		MIME_CHECK(root.valid);
		input.clear(); input.seekg(0);
		const auto text = mime_xml::inspect_text(input, root);
		MIME_CHECK(text.encoding.empty() == truncated);
		bu_mime_traits facts = BU_MIME_TRAITS_INIT_ZERO;
		MIME_CHECK(text.append_traits(root, facts));
		MIME_CHECK((bu_mime_traits_get(&facts, "encoding") != NULL) == !truncated &&
		    (bu_mime_traits_get(&facts, "text.ascii-only") != NULL) == !truncated);
		bu_mime_traits_free(&facts);
	    }
	}
    for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	const auto bytes = mime_text_bytes("<?xml version='1.0' encoding='US-ASCII'?><X3D/>", width, true, width == 1);
	std::istringstream input(std::string(bytes.begin(), bytes.end()));
	const auto root = mime_xml::inspect(input);
	MIME_CHECK(root.valid);
	input.clear(); input.seekg(0);
	MIME_CHECK(mime_xml::inspect_text(input, root).encoding.empty());
    }
    for (const char *charset : {"ISO-8859-1", "windows-1252"}) {
	const std::string data = std::string("<?xml version='1.0' encoding='") + charset + "'?><X3D/><!--caf\xe9-->";
	std::istringstream input(data);
	const auto root = mime_xml::inspect(input);
	input.clear(); input.seekg(0);
	const auto text = mime_xml::inspect_text(input, root);
	MIME_CHECK((!text.encoding.empty()) == BU_STR_EQUAL(charset, "ISO-8859-1"));
	if (!text.encoding.empty()) MIME_CHECK(!text.ascii && text.encoding == "iso-8859-1");
    }
    for (const std::string &profile : {std::string("esa/safe/sentinel-1.0/sentinel-1/sar/level-1/grd/standard/iwdp"),
	std::string("esa/safe/sentinel/1.1/sentinel-2/msi/archive_l2a_user_product")})
	for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	    const auto bytes = mime_text_bytes(fixture::safe(profile), width, true);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto *facts = mime_result_traits(result, mime_xml::safe_type);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "safe.profile"), profile.c_str()) &&
		!bu_mime_traits_get(facts, "version") && !mime_result_traits(result, mime_xml::sentinel2_type));
	    bu_mime_result_free(result);
	}
    for (const char *level : {"1B", "1C", "2A"})
	for (const char *role : {"User_Product", "DataStrip_ID", BU_STR_EQUAL(level, "1B") ? "Granule_ID" : "Tile_ID"}) {
	    const auto bytes = fixture::sentinel2(level, role, "99");
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto *facts = mime_result_traits(result, mime_xml::sentinel2_type);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "sentinel2.processing-level"), level) &&
		bu_mime_traits_get(facts, "sentinel2.metadata-role") && !bu_mime_traits_get(facts, "version"));
	    bu_mime_result_free(result);
	}
    for (const auto &bytes : {fixture::safe("other/profile"), fixture::safe("1.0"), std::string("<XFDU/>"),
	std::string("<Level-1C_User_Product/>"),
	std::string("<n:Level-1C_User_Product xmlns:n=\"https://psd-14.sentinel2.eo.esa.int.example/PSD/User_Product_Level-1C.xsd\"/>")}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!mime_result_traits(result, mime_xml::safe_type) && !mime_result_traits(result, mime_xml::sentinel2_type));
	bu_mime_result_free(result);
    }
    for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	const auto bytes = mime_text_bytes(fixture::esri_xml(), width, true);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	const auto *facts = mime_result_traits(result, mime_esric::xml_type);
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "version") && !bu_mime_traits_get(facts, "esri-cache.packet-size") &&
	    !bu_mime_traits_get(facts, "bit-depth"));
	bu_mime_result_free(result);
    }
    const std::string source = fixture::esri_json();
    for (const auto &change : {std::make_pair("\"version\":1.0", "\"version\":1.0"),
	std::make_pair("\"version\":1.0", "\"version\":9.0"),
	std::make_pair("\"packetSize\":128", "\"packetSize\":128,\"packetSize\":64"),
	std::make_pair("\"format\":\"PNG\"", "\"format\":\"PNG\",\"format\":\"JPEG\"")}) {
	std::string bytes = source;
	bytes.replace(bytes.find(change.first), strlen(change.first), change.second);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	const auto *facts = mime_result_traits(result, mime_esric::json_type);
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "version") && !bu_mime_traits_get(facts, "bit-depth"));
	const bool repeated = std::string(change.second).find(',') != std::string::npos;
	MIME_CHECK((bu_mime_traits_get(facts, "esri-cache.storage-format") != NULL) == !repeated);
	if (!repeated) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "esri-cache.package-version"),
	    std::string(change.second).find("9.0") != std::string::npos ? "9.0" : "1.0"));
	bu_mime_result_free(result);
    }
    for (size_t size = 0; size < source.size() - 1; ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, source.data(), size, 0, &result) != BU_MIME_ERROR);
	MIME_CHECK(!mime_result_traits(result, mime_esric::json_type));
	bu_mime_result_free(result);
    }
    for (const auto &bytes : {std::string("{\"tileBundlesPath\":\"tile\"}"), std::string("{\"itemInfo\":{}}")}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!mime_result_traits(result, mime_esric::json_type));
	bu_mime_result_free(result);
    }
    bu_mime_result *hint = NULL;
    MIME_CHECK(bu_mime_detect(registry, "uninspected.tpkx", NULL, 0, 0, &hint) != BU_MIME_ERROR);
    const auto *facts = mime_result_traits(hint, mime_esric::zip_type);
    MIME_CHECK(facts && !facts->count);
    bu_mime_result_free(hint);
    return 0;
}

static int
mime_science_test(bu_mime_registry *registry)
{
    for (int kind : {-1, 0, 1, 2, 3})
	for (bool little : {false, true}) {
	    const std::string bytes = mime_science_fixture::noaa(kind, little);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto *facts = mime_result_traits(result, mime_noaa_grid::type);
	    const std::string code = std::to_string(kind);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "noaa-grid.kind"), code.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "byte-order"), little ? "little-endian" : "big-endian") &&
		!bu_mime_traits_get(facts, "version"));
	    MIME_CHECK((bu_mime_traits_get(facts, "bit-depth") != NULL) == (kind >= 0 && kind <= 2));
	    bu_mime_result_free(result);
	}
    {
	std::string bytes = mime_science_fixture::noaa();
	mime_heightfield_fixture::word(bytes, 20, UINT64_C(0x4000000000000000), 8, false); // Two-degree spacing.
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	const auto *facts = mime_result_traits(result, mime_noaa_grid::type);
	MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "noaa-grid.steps-within-one-degree"), "false"));
	bu_mime_result_free(result);
    }
    for (bool south : {false, true})
	for (const char *scale : {"250", "500"}) {
	    const std::string bytes = mime_science_fixture::nsidc(south, scale);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto *facts = mime_result_traits(result, mime_nsidc::type);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "nsidc.scaling-factor"), scale) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "nsidc.hemisphere"), south ? "south" : "north") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "bit-depth"), "8") && !bu_mime_traits_get(facts, "byte-order") &&
		!bu_mime_traits_get(facts, "version"));
	    bu_mime_result_free(result);
	}
    for (unsigned int other : {0u, 1u, 2u}) {
	std::string bytes = mime_science_fixture::nsidc();
	if (other == 0) bytes.replace(60, 5, "18 tb");
	else if (other == 1) bytes.replace(114, 5, "    1");
	else bytes[60] = static_cast<char>(0xff);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!mime_result_traits(result, mime_nsidc::type));
	bu_mime_result_free(result);
    }
    for (unsigned int product : {1u, 3u, 5u, 7u, 8u, 10u, 16u})
	for (unsigned int quantity : {1u, 2u, 3u, 8u, 9u, 32u, 33u, 35u, 37u, 31u}) {
	    const std::string bytes = mime_science_fixture::iris(product, quantity);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) == BU_MIME_MATCH);
	    const auto *facts = mime_result_traits(result, mime_iris::type);
	    const std::string product_code = std::to_string(product), quantity_code = std::to_string(quantity);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "iris.product-header-version"), "8") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "iris.configuration-version"), "6") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "iris.product-code"), product_code.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "iris.quantity-code"), quantity_code.c_str()) && !bu_mime_traits_get(facts, "version"));
	    MIME_CHECK(bu_mime_traits_get(facts, "bit-depth") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "sample-format"), quantity == 31 ? "ieee754" : "unsigned-integer"));
	    if (product == 1 || product == 3) {
		const char *view = product == 1 ? mime_iris::ppi_type : mime_iris::cappi_type;
		MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, view) && mime_result_traits(result, view));
	    }
	    bu_mime_result_free(result);
	}
    for (const auto &revision : {std::make_pair(size_t(2), uint64_t(9)), std::make_pair(size_t(14), uint64_t(7))}) {
	std::string bytes = mime_science_fixture::iris();
	mime_heightfield_fixture::word(bytes, revision.first, revision.second, 2);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) == BU_MIME_MATCH);
	const auto *facts = mime_result_traits(result, mime_iris::type);
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "iris.quantity-code") && !mime_result_traits(result, mime_iris::ppi_type));
	bu_mime_result_free(result);
    }
    const std::string partial = mime_science_fixture::iris(3, 2, 2);
    for (size_t prefix : {size_t(128), size_t(640)}) {
	const bu_mime_probe_input input = {reinterpret_cast<const unsigned char *>(partial.data()), prefix, 0, partial.size(), 1};
	bu_mime_result *observations = NULL;
	MIME_CHECK(bu_mime_identify(registry, NULL, &input, &observations) != BU_MIME_ERROR);
	const auto *header = mime_result_traits(observations, mime_iris::type);
	MIME_CHECK(header && (bu_mime_traits_get(header, "iris.payload-layout") != NULL) == (prefix == 640));
	if (prefix < 640) MIME_CHECK(observations->incomplete && !bu_mime_traits_get(header, "iris.quantity-code"));
	bu_mime_result_free(observations);
    }
    const std::string big = mime_science_fixture::iris(3, 9, 1, false);
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, big.data(), big.size(), 1, &result) == BU_MIME_MATCH);
    const auto *facts = mime_result_traits(result, mime_iris::type);
    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "byte-order"), "big-endian"));
    bu_mime_result_free(result);
    std::string unrelated = mime_science_fixture::iris();
    mime_heightfield_fixture::word(unrelated, 12, 0, 2);
    result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, unrelated.data(), unrelated.size(), 1, &result) != BU_MIME_ERROR);
    MIME_CHECK(!mime_result_traits(result, mime_iris::type));
    bu_mime_result_free(result);
    for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	const auto bytes = mime_text_bytes(mime_science_fixture::photomod(), width, true);
	result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) == BU_MIME_MATCH);
	facts = mime_result_traits(result, mime_raster_label::photomod_label);
	MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "raster.entrypoint"), "label") &&
	    !bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "photomod.filename-kind"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_raster_label_test(bu_mime_registry *registry)
{
    namespace labels = mime_raster_label;
    namespace fixture = mime_raster_label_fixture;
    const struct {const char *type; std::string text; const char *key; const char *value;} cases[] = {
	{labels::rraster_label, fixture::rraster(), "rraster.datatype", "FLT4S"},
	{labels::ehdr_label, fixture::ehdr(), "ehdr.interleave", "BIL"},
	{labels::roipac_label, fixture::roipac(), "roi-pac.unit-transform", "true"}
    };
    for (const auto &sample : cases) {
	for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	    const auto bytes = mime_text_bytes(sample.text, width, true);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto *facts = mime_result_traits(result, sample.type);
	    const std::string bits = std::to_string(width * 8);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, sample.key), sample.value) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "text.code-unit-width"), bits.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "raster.entrypoint"), "label") && !bu_mime_traits_get(facts, "bit-depth"));
	    bu_mime_result_free(result);
	}
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, sample.text.data(), sample.text.size(), 0, &result) != BU_MIME_ERROR);
	const auto *facts = mime_result_traits(result, sample.type);
	MIME_CHECK(facts && result->incomplete && !bu_mime_traits_get(facts, sample.key));
	bu_mime_result_free(result);
    }
    const std::pair<std::string, const char *> ambiguous[] = {
	{fixture::rraster() + "[data]\ndatatype=FLT4S\n", labels::rraster_label},
	{fixture::ehdr() + "LAYOUT BIL\n", labels::ehdr_label},
	{fixture::roipac() + "Z_SCALE 1\n", labels::roipac_label}
    };
    for (const auto &sample : ambiguous) {
	std::istringstream input(sample.first);
	const auto label = labels::inspect(input, true);
	MIME_CHECK(label.identified && !label.header_known && BU_STR_EQUAL(label.type(), sample.second));
    }
    std::string text = fixture::rraster();
    text.replace(text.find("byteorder=little\n"), strlen("byteorder=little\n"), "");
    std::istringstream input(text);
    const auto label = labels::inspect(input, true);
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    MIME_CHECK(label.header_known && label.append_traits(observed) &&
	BU_STR_EQUAL(bu_mime_traits_get(&observed, "rraster.byte-order"), "NATIVE") && !bu_mime_traits_get(&observed, "byte-order"));
    bu_mime_traits_free(&observed);
    MIME_CHECK(labels::integer("+0032") == "32" && labels::integer("-1").empty());
    MIME_CHECK(labels::number_is("1.000", 1.0) && labels::number_is("-0", 0.0) &&
	!labels::number_is("1e-20", 0.0) && !labels::number_is("nan", 0.0));
    text = fixture::rraster();
    text.replace(text.find("[data]"), strlen("[data]"), "[extension]");
    std::istringstream foreign(text);
    MIME_CHECK(!labels::inspect(foreign, true).identified);
    for (const char *root : {"<imageFile/>", "<imageFile xmlns=\"urn:fixture:metadata\"/>"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, root, strlen(root), 1, &result) != BU_MIME_ERROR);
	const auto *facts = mime_result_traits(result, labels::isce_label);
	MIME_CHECK((facts != NULL) == (std::string(root).find("xmlns") == std::string::npos));
	if (facts) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "raster.entrypoint"), "label"));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_heightfield_test(bu_mime_registry *registry)
{
    for (bool hf2 : {false, true}) {
	const char *type = hf2 ? mime_hf2::type : mime_sigdem::type;
	const size_t offset = hf2 ? mime_hf2::version_offset : mime_sigdem::version_offset;
	for (uint16_t revision : {0, 1, 2, 65535}) {
	    std::string bytes(hf2 ? "HF2" : "SIGDEM");
	    if (hf2) bytes.push_back(0);
	    bytes.resize(offset + sizeof(revision));
	    mime_heightfield_fixture::word(bytes, offset, revision, sizeof(revision), hf2);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const bu_mime_traits *facts = mime_result_traits(result, type);
	    const std::string version = std::to_string(revision);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "version"), version.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "binary"));
	    const bool known = revision == (hf2 ? 0 : 1);
	    MIME_CHECK(known == (bu_mime_traits_get(facts, "byte-order") != NULL));
	    if (hf2) {
		MIME_CHECK(!bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "sample-format") &&
		    BU_STR_EQUAL(bu_mime_traits_get(facts, "compression"), "none"));
		if (known) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "hf2.sample-encoding"), "scaled-integer-deltas"));
	    } else if (known) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "compression"), "none") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "bit-depth"), "32") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "sample-format"), "signed-integer"));
	    else MIME_CHECK(!bu_mime_traits_get(facts, "bit-depth"));
	    bu_mime_result_free(result);
	    for (size_t size = 0; size < bytes.size(); ++size) {
		MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), size, 0, &result) != BU_MIME_ERROR);
		facts = mime_result_traits(result, type);
		MIME_CHECK(!facts || !bu_mime_traits_get(facts, "version"));
		bu_mime_result_free(result);
	    }
	}
    }
    for (size_t sample_size : {size_t(2), size_t(4)})
	for (bool little : {false, true})
	    for (bool unit : {false, true}) {
		const std::string bytes = mime_heightfield_fixture::byn(sample_size, little, unit);
		bu_mime_result *result = NULL;
		MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
		const auto *facts = mime_result_traits(result, mime_byn::type);
		const std::string bits = std::to_string(sample_size * 8);
		MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "bit-depth"), bits.c_str()) &&
		    BU_STR_EQUAL(bu_mime_traits_get(facts, "byte-order"), little ? "little-endian" : "big-endian") &&
		    BU_STR_EQUAL(bu_mime_traits_get(facts, "byn.header-byte-order"), "little-endian") &&
		    BU_STR_EQUAL(bu_mime_traits_get(facts, "byn.unit-scale"), unit ? "true" : "false") &&
		    !bu_mime_traits_get(facts, "version"));
		bu_mime_result_free(result);
	    }
    for (const char *version : {"1.0", "1.01", "2.0", "3.0"}) {
	const auto text = mime_heightfield_fixture::isg(version);
	for (size_t width : {size_t(1), size_t(2), size_t(4)}) {
	    const auto bytes = mime_text_bytes(text, width, true);
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto *facts = mime_result_traits(result, mime_isg::type);
	    const std::string bits = std::to_string(width * 8);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "isg.format-version"), version) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "text.code-unit-width"), bits.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "isg.data-format"), "grid") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "isg.coordinate-type"), "geodetic") &&
		!bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "version"));
	    bu_mime_result_free(result);
	}
	std::string duplicate = text;
	duplicate.insert(duplicate.find("end_of_head"), "ISG format = 9.0\n");
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, duplicate.data(), duplicate.size(), 1, &result) != BU_MIME_ERROR);
	const auto *facts = mime_result_traits(result, mime_isg::type);
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "isg.format-version"));
	bu_mime_result_free(result);
    }
    std::string truncated = mime_heightfield_fixture::isg("2.0");
    truncated.resize(truncated.find("end_of_head"));
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, truncated.data(), truncated.size(), 0, &result) != BU_MIME_ERROR);
    const auto *facts = mime_result_traits(result, mime_isg::type);
    MIME_CHECK(facts && !bu_mime_traits_get(facts, "isg.format-version") && result->incomplete);
    bu_mime_result_free(result);
    return 0;
}

static int
mime_leveller_test(bu_mime_registry *registry)
{
    const char *type = "application/leveller";
    for (unsigned int revision : {0, 3, 4, 5, 6, 7, 12, 13, 255}) {
	for (const char *magic : {"trrn", "TRRN"}) {
	    std::string bytes(magic);
	    bytes.push_back(static_cast<char>(revision));
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const auto value = std::to_string(revision);
	    const bu_mime_traits *facts = mime_result_traits(result, type);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "version"), value.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "binary") && !bu_mime_traits_get(facts, "leveller.reference-kind"));
	    if (revision >= 4 && revision <= 12) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "bit-depth"), "32") &&
		    BU_STR_EQUAL(bu_mime_traits_get(facts, "byte-order"), "little-endian") &&
		    BU_STR_EQUAL(bu_mime_traits_get(facts, "sample-format"), revision < 6 ? "signed-fixed-point" : "ieee754"));
		if (revision < 6) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "leveller.fractional-bits"), "16"));
		else MIME_CHECK(!bu_mime_traits_get(facts, "leveller.fractional-bits"));
	    } else MIME_CHECK(!bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "sample-format"));
	    bu_mime_result_free(result);
	}
    }
    for (size_t size = 0; size < 5; ++size) {
	const unsigned char bytes[] = {'t', 'r', 'r', 'n', 7};
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes, size, 0, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = mime_result_traits(result, type);
	MIME_CHECK(!facts || !bu_mime_traits_get(facts, "version"));
	bu_mime_result_free(result);
    }
    for (const char *other : {"trrm", "TERRAGENTERRAIN  ", "prefixtrrn"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, other, strlen(other), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!mime_result_traits(result, type));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_dwg_test(bu_mime_registry *registry)
{
    const char *type = "image/vnd.dwg";
    for (const char *code : {"MC0.0", "AC1.2", "AC1.3", "AC1.40", "AC1.50", "AC2.10", "AC2.21", "AC2.22",
	"AC1001", "AC1009", "AC1015", "AC1018", "AC1032", "AC9999"}) {
	std::string header(code);
	if (header.size() < 6) header.push_back('\0');
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), header.size(), 1, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = mime_result_traits(result, type);
	MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "dwg.header-version"), code) &&
	    BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "binary") && !bu_mime_traits_get(facts, "version") &&
	    !bu_mime_traits_get(facts, "byte-order") && !bu_mime_traits_get(facts, "dwg.raster-reference-count"));
	bu_mime_result_free(result);
	for (size_t size = 0; size < header.size(); ++size) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, header.data(), size, 0, &result) != BU_MIME_ERROR);
	    MIME_CHECK(!mime_result_traits(result, type));
	    bu_mime_result_free(result);
	}
    }
    for (const char *lookalike : {"ACabcd", "AC101x", "AC1.41", "MC0.1", "prefixAC1015", "ac1015"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, lookalike, strlen(lookalike), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!mime_result_traits(result, type));
	bu_mime_result_free(result);
    }
    return 0;
}

static int
mime_lerc_test(bu_mime_registry *registry)
{
    const char *type = "image/vnd.esri.lerc";
    const auto put = [](std::vector<unsigned char> &bytes, size_t at, uint32_t value) {
	for (size_t i = 0; i < sizeof(value); ++i) bytes[at + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    const struct {const char *type; const char *bits; const char *kind;} cases[] = {
	{"int8", "8", "signed-integer"}, {"uint8", "8", "unsigned-integer"},
	{"int16", "16", "signed-integer"}, {"uint16", "16", "unsigned-integer"},
	{"int32", "32", "signed-integer"}, {"uint32", "32", "unsigned-integer"},
	{"float32", "32", "ieee754"}, {"float64", "64", "ieee754"}
    };
    std::vector<unsigned char> example;
    for (uint32_t revision : {1, 2, 3, 4, 5, 6}) {
	for (size_t datatype = 0; datatype < sizeof(cases) / sizeof(cases[0]); ++datatype) {
	    std::vector<unsigned char> bytes(96, 0);
	    memcpy(bytes.data(), "Lerc2 ", 6);
	    put(bytes, 6, revision);
	    size_t field = revision >= 3 ? 14 : 10;
	    put(bytes, field, 2); field += sizeof(uint32_t);
	    put(bytes, field, 2); field += sizeof(uint32_t);
	    if (revision >= 4) { put(bytes, field, 1); field += sizeof(uint32_t); }
	    put(bytes, field, 4); field += sizeof(uint32_t);
	    put(bytes, field, 8); field += sizeof(uint32_t);
	    put(bytes, field, static_cast<uint32_t>(bytes.size())); field += sizeof(uint32_t);
	    put(bytes, field, static_cast<uint32_t>(datatype));
	    bu_mime_result *result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	    const bu_mime_traits *facts = mime_result_traits(result, type);
	    const auto version = std::to_string(revision);
	    MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.codec"), "lerc2") &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.version"), version.c_str()) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.data-type"), cases[datatype].type) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "bit-depth"), cases[datatype].bits) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "sample-format"), cases[datatype].kind) &&
		BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.blob-count"), "1") && !bu_mime_traits_get(facts, "version"));
	    bu_mime_result_free(result);
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 0, &result) != BU_MIME_ERROR);
	    facts = mime_result_traits(result, type);
	    MIME_CHECK(facts && !bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "lerc.blob-count"));
	    bu_mime_result_free(result);
	    example = bytes;
	}
    }
    for (size_t padding : {size_t(3), size_t(4)}) {
	std::vector<unsigned char> padded = example;
	padded.resize(padded.size() + padding, 0xbe);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, padded.data(), padded.size(), 1, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = mime_result_traits(result, type);
	MIME_CHECK(facts);
	MIME_CHECK(padding == 3 ? BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.trailing-bytes"), "3") :
	    !bu_mime_traits_get(facts, "lerc.blob-count"));
	bu_mime_result_free(result);
    }
    for (size_t size = 0; size < example.size(); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, example.data(), size, 0, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = mime_result_traits(result, type);
	MIME_CHECK(!facts || (!bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "lerc.blob-count")));
	bu_mime_result_free(result);
    }
    for (uint32_t revision : {uint32_t(99), std::numeric_limits<uint32_t>::max()}) {
	put(example, 6, revision);
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, example.data(), example.size(), 1, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = mime_result_traits(result, type);
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "lerc.first-blob.data-type") && !bu_mime_traits_get(facts, "bit-depth"));
	if (revision == 99) MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.version"), "99"));
	else MIME_CHECK(!bu_mime_traits_get(facts, "lerc.first-blob.version"));
	bu_mime_result_free(result);
    }
    std::vector<unsigned char> legacy(78, 0);
    memcpy(legacy.data(), "CntZImage ", 10);
    put(legacy, 10, 11); put(legacy, 14, 8);
    put(legacy, 18, 2); put(legacy, 22, 2);
    put(legacy, 42, 4); put(legacy, 54, 1); put(legacy, 58, 1); put(legacy, 62, 8);
    for (bool extra_blob : {false, true}) {
	std::vector<unsigned char> bytes = legacy;
	if (extra_blob) bytes.insert(bytes.end(), legacy.begin(), legacy.end());
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), bytes.size(), 1, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = mime_result_traits(result, type);
	MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.codec"), "lerc1") &&
	    BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.version"), "11") &&
	    BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.image-type"), "8") &&
	    !bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "sample-format") &&
	    !bu_mime_traits_get(facts, "lerc.first-blob.data-type"));
	MIME_CHECK(extra_blob ? !bu_mime_traits_get(facts, "lerc.blob-count") : BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.blob-count"), "1"));
	bu_mime_result_free(result);
    }
    constexpr size_t mask_size = 512, data_header = 50 + mask_size, data_size = 8;
    std::vector<unsigned char> large_mask(data_header + 16 + data_size, 0);
    std::copy(legacy.begin(), legacy.begin() + 50, large_mask.begin());
    put(large_mask, 42, mask_size);
    put(large_mask, data_header, 1); put(large_mask, data_header + 4, 1); put(large_mask, data_header + 8, data_size);
    const auto bounded = mime_lerc::inspect(large_mask.data(), mime_lerc::largest_header_size, false);
    MIME_CHECK(bounded.header_known && !bounded.single_blob && bounded.incomplete);
    std::istringstream stream(std::string(reinterpret_cast<const char *>(large_mask.data()), large_mask.size()));
    const auto full = mime_lerc::inspect(stream);
    MIME_CHECK(full.header_known && full.single_blob && !full.incomplete && !stream.bad());

    return 0;
}

static int
mime_raster_container_test(bu_mime_registry *registry)
{
    const auto find = mime_result_traits;
    const std::string mrf_label = "<MRF_META><Raster><Compression>DEFLATE</Compression></Raster></MRF_META>";
    for (auto format : {mime_tar_fixture::format::posix, mime_tar_fixture::format::gnu, mime_tar_fixture::format::empty_prefix_padding}) {
	const std::string archive = mime_tar_fixture::entry("mrf_label.mrf", mrf_label, format) + std::string(2 * mime_tar_fixture::block_size, '\0');
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, archive.data(), archive.size(), 1, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = find(result, "application/x-tar");
	MIME_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "container"), "tar") &&
	    BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "binary") &&
	    BU_STR_EQUAL(bu_mime_traits_get(facts, "tar.first-member-type"), "application/vnd.nasa.mrf+xml") &&
	    !bu_mime_traits_get(facts, "text.code-unit-width") && !bu_mime_traits_get_set(facts, "xml.root-namespaces", NULL, NULL) &&
	    !find(result, "application/vnd.nasa.mrf+xml"));
	bu_mime_result_free(result);
	for (size_t size : {size_t(0), size_t(511), size_t(512), size_t(515)}) {
	    MIME_CHECK(bu_mime_detect(registry, NULL, archive.data(), size, 0, &result) != BU_MIME_ERROR);
	    facts = find(result, "application/x-tar");
	    MIME_CHECK(!facts || !bu_mime_traits_get(facts, "tar.first-member-type"));
	    bu_mime_result_free(result);
	}
	std::string damaged = archive;
	damaged[0] = 'x';
	MIME_CHECK(bu_mime_detect(registry, NULL, damaged.data(), damaged.size(), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!find(result, "application/x-tar"));
	bu_mime_result_free(result);
    }
    std::string extended = mime_tar_fixture::entry("mrf_label.mrf", mrf_label);
    extended[156] = 'x';
    mime_tar_fixture::checksum(extended);
    std::string prefixed = mime_tar_fixture::entry("mrf_label.mrf", mrf_label);
    prefixed.replace(345, 6, "subdir");
    mime_tar_fixture::checksum(prefixed);
    const std::string unknown = mime_tar_fixture::entry("opaque.bin", "unrelated");
    for (const std::string &archive : {extended, prefixed, unknown + mime_tar_fixture::entry("mrf_label.mrf", mrf_label),
	mime_tar_fixture::entry("subdir/mrf_label.mrf", mrf_label), mime_tar_fixture::entry("mrf_label.mrf", "<Unrelated/>"),
	mime_tar_fixture::entry("mrf_label.mrf", mrf_label).substr(0, mime_tar_fixture::block_size + mrf_label.size() - 1)}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, archive.data(), archive.size(), 1, &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = find(result, "application/x-tar");
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "tar.first-member-type"));
	bu_mime_result_free(result);
    }
    const struct {
	const char *text;
	const char *kind;
	size_t versions;
	size_t extensions;
	bool consistent;
	bool assets = false;
	bool templates = false;
    } stac[] = {
	{R"({"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0","assets":{"raster":{}},"asset_templates":{"tile":{}}})", "item", 1, 0, true, true, true},
	{R"({"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0","assets":{"one":{}},"assets":{"two":{}}})", NULL, 0, 0, false},
	{R"({"type":"Feature","geometry":null,"properties":{}})", NULL, 0, 0, true},
	{R"({"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0"})", "item", 1, 0, true},
	{R"({"type":"Feature","geometry":null,"properties":{},"stac_version":"9.0.0","stac_extensions":["future","known","future"]})", "item", 1, 2, true},
	{R"({"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0","stac_version":"9.0.0"})", NULL, 0, 0, false},
	{R"({"type":"Feature","geometry":null,"properties":{"stac_version":"1","stac_version":"2"},"stac_version":"1.0.0"})", "item", 1, 0, true},
	{R"({"type":"FeatureCollection","features":[]})", NULL, 0, 0, true},
	{R"({"type":"FeatureCollection","features":[{"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0","stac_extensions":["one"]},{"type":"Feature","geometry":null,"properties":{},"stac_version":"1.1.0","stac_extensions":["two"]}]})", "item-collection", 2, 2, true},
	{R"({"type":"FeatureCollection","features":[{"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0"},{"type":"Feature","geometry":null,"properties":{}}]})", NULL, 0, 0, true},
	{R"({"type":"FeatureCollection","features":[{"type":"Feature","geometry":null,"properties":{},"stac_version":"9.0.0","stac_version":"1.0.0"}]})", NULL, 0, 0, false},
	{R"({"type":"Feature","geometry":null,"properties":{},"stac_version":"1.0.0","stac_extensions":[1]})", NULL, 0, 0, false}
    };
    for (const auto &sample : stac) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, sample.text, strlen(sample.text), 1, &result) != BU_MIME_ERROR);
	const auto *facts = find(result, "application/geo+json");
	MIME_CHECK(facts && (sample.consistent || !facts->count));
	if (sample.consistent) {
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "utf-8"));
	    const char *kind = bu_mime_traits_get(facts, "stac.object-kind");
	    MIME_CHECK(sample.kind ? kind && BU_STR_EQUAL(kind, sample.kind) : !kind);
	    if (sample.kind) {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "stac.items-have-assets"), sample.assets ? "true" : "false"));
		if (BU_STR_EQUAL(sample.kind, "item"))
		    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "stac.asset-templates"), sample.templates ? "present" : "absent"));
	    }
	    for (const auto &property : {std::pair<const char *, size_t>{"stac.versions", sample.versions},
		std::pair<const char *, size_t>{"stac.declared-extensions", sample.extensions}}) {
		const char *const *members = NULL;
		size_t count = 0;
		const bool known = bu_mime_traits_get_set(facts, property.first, &members, &count);
		MIME_CHECK(sample.kind ? known && count == property.second : !known);
	    }
	    MIME_CHECK(!bu_mime_traits_get(facts, "version") && !bu_mime_traits_get(facts, "bit-depth"));
	}
	bu_mime_result_free(result);
    }
    for (size_t size = 0; size < strlen(stac[0].text); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, stac[0].text, size, 0, &result) != BU_MIME_ERROR);
	MIME_CHECK(!find(result, "application/geo+json"));
	bu_mime_result_free(result);
    }
    for (const char *other : {R"({"type":"Collection","stac_version":"1.0.0"})",
	R"({"type":"Feature","stac_version":"1.0.0"})", R"({"properties":{"type":"Feature","stac_version":"1.0.0"}})"}) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, other, strlen(other), 1, &result) != BU_MIME_ERROR);
	MIME_CHECK(!find(result, "application/geo+json"));
	bu_mime_result_free(result);
    }
    const struct {const char *alias; const char *type;} aliases[] = {
	{"image/x-bmp", "image/bmp"}, {"image/x-MS-bmp", "image/bmp"}, {"application/x-grib", "application/grib"}, {"application/vnd+zarr", "application/vnd.zarr"}
    };
    bu_vls canonical = BU_VLS_INIT_ZERO;
    for (const auto &alias : aliases) {
	MIME_CHECK(bu_mime_canonical(registry, alias.alias, &canonical));
	MIME_CHECK(BU_STR_EQUAL(bu_vls_cstr(&canonical), alias.type));
    }
    bu_vls_free(&canonical);
    bu_mime_result *names = NULL;
    MIME_CHECK(bu_mime_resolve_candidates(registry, "bmp", &names) == BU_MIME_MATCH);
    MIME_CHECK(names && names->count == 1 && BU_STR_EQUAL(names->candidates[0].name, "image/bmp"));
    bu_mime_result_free(names);

    /* These controls assert encoded header fields. Actual converter fixtures
     * independently verify complete files through their SDK. */
    const struct {const char *text; const char *version; const char *kind; const char *consolidated; bool consistent;} zarr[] = {
	{"{\"zarr_format\":2,\"shape\":[2,2]}", "2", NULL, NULL, true},
	{"{\"zarr_format\":3,\"node_type\":\"array\"}", "3", "array", NULL, true},
	{"{\"zarr_format\":9,\"node_type\":\"future\"}", "9", "future", NULL, true},
	{"{\"zarr_format\":2,\"zarr_format\":3,\"node_type\":\"array\"}", NULL, NULL, NULL, false},
	{"{\"zarr_format\":3,\"node_type\":\"array\",\"node_type\":\"group\"}", NULL, NULL, NULL, false},
	{"{\"zarr_format\":3,\"node_type\":\"array\",\"attributes\":{\"zarr_format\":1,\"zarr_format\":2}}", "3", "array", NULL, true},
	{"{\"zarr_consolidated_format\":1,\"metadata\":{\".zgroup\":{\"zarr_format\":2}}}", "2", "group", "1", true},
	{"{\"zarr_consolidated_format\":1,\"metadata\":{}}", "2", NULL, "1", true},
	{"{\"zarr_consolidated_format\":2,\"metadata\":{}}", NULL, NULL, "2", true},
	{"{\"zarr_consolidated_format\":1,\"metadata\":{\".zarray\":{\"zarr_format\":9,\"zarr_format\":2}}}", NULL, NULL, NULL, false},
	{"{\"zarr_consolidated_format\":1,\"metadata\":{\".zarray\":{\"zarr_format\":2},\".zgroup\":{\"zarr_format\":2}}}", NULL, NULL, NULL, false}
    };
    for (const auto &sample : zarr) {
	bu_mime_result *metadata = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, sample.text, strlen(sample.text), 1, &metadata) != BU_MIME_ERROR);
	const auto *facts = find(metadata, "application/vnd.zarr");
	MIME_CHECK(facts);
	if (!sample.consistent) MIME_CHECK(!facts->count);
	else {
	    MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "zarr.entrypoint"), "metadata"));
	    const struct {const char *key; const char *value;} fields[] = {
		{"version", sample.version}, {"zarr.node-type", sample.kind}, {"zarr.consolidated-format", sample.consolidated}
	    };
	    for (const auto &field : fields) {
		const char *actual = bu_mime_traits_get(facts, field.key);
		MIME_CHECK(field.value ? BU_STR_EQUAL(actual, field.value) : !actual);
	    }
	    MIME_CHECK(!bu_mime_traits_get(facts, "byte-order") && !bu_mime_traits_get(facts, "bit-depth"));
	}
	bu_mime_result_free(metadata);
    }
    for (size_t size = 0; size < strlen(zarr[1].text); ++size) {
	bu_mime_result *metadata = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, zarr[1].text, size, 0, &metadata) != BU_MIME_ERROR);
	MIME_CHECK(!find(metadata, "application/vnd.zarr"));
	bu_mime_result_free(metadata);
    }
    for (const char *other : {"{\"zarr_format\":\"3\"}", "{\"zarr_format\":3.0}", "{\"zarr_format\":-1}",
	"{\"attributes\":{\"zarr_format\":3}}", "{\"version\":1,\"refs\":{\".zarray\":\"zarr_format=2\"}}"}) {
	bu_mime_result *metadata = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, other, strlen(other), 1, &metadata) != BU_MIME_ERROR);
	MIME_CHECK(!find(metadata, "application/vnd.zarr"));
	bu_mime_result_free(metadata);
    }
    bu_mime_result *store_hint = NULL;
    MIME_CHECK(bu_mime_detect(registry, "uninspected.zarr", NULL, 0, 0, &store_hint) != BU_MIME_ERROR);
    MIME_CHECK(find(store_hint, "application/vnd.zarr") && !find(store_hint, "application/vnd.zarr")->count);
    bu_mime_result_free(store_hint);

    const char vicar[] = "LBLSIZE=512 FORMAT='BYTE' TYPE='IMAGE' ORG='BSQ'";
    const size_t vicar_declaration = strlen("LBLSIZE=512 ");
    for (size_t size = 0; size <= strlen(vicar); ++size) {
	bu_mime_result *prefix = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, vicar, size, 0, &prefix) != BU_MIME_ERROR);
	const auto *facts = find(prefix, "application/vnd.nasa.vicar");
	MIME_CHECK(size < vicar_declaration ? !facts : facts && facts->count == 1 &&
	    BU_STR_EQUAL(bu_mime_traits_get(facts, "vicar.header-layout"), "lblsize"));
	bu_mime_result_free(prefix);
    }
    for (const char *label : {"LBLSIZE = +512 FORMAT='BYTE'", "LBLSIZE=1024 ", "LBLSIZE=4096\tTYPE='TABULAR'"}) {
	bu_mime_result *vicar_result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, label, strlen(label), 1, &vicar_result) != BU_MIME_ERROR);
	const auto *facts = find(vicar_result, "application/vnd.nasa.vicar");
	MIME_CHECK(facts && facts->count == 1 && BU_STR_EQUAL(bu_mime_traits_get(facts, "vicar.header-layout"), "lblsize"));
	bu_mime_result_free(vicar_result);
    }
    for (const char *label : {"LBLSIZE=0 ", "LBLSIZE=1 ", "LBLSIZE=-512 ", "LBLSIZE=512x ",
	"LBLSIZEX=512 ", "LBLSIZE=18446744073709551616 ",
	"NOTE='LBLSIZE=512'", "PDS_VERSION_ID=PDS3\nLBLSIZE=512 FORMAT='BYTE'"}) {
	bu_mime_result *vicar_result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, label, strlen(label), 1, &vicar_result) != BU_MIME_ERROR);
	MIME_CHECK(!find(vicar_result, "application/vnd.nasa.vicar"));
	bu_mime_result_free(vicar_result);
    }

    constexpr size_t cals_record_size = 128, cals_header_size = 2048;
    std::vector<unsigned char> cals(cals_header_size, ' ');
    const auto cals_record = [&](size_t index, const char *value) {
	memset(cals.data() + index * cals_record_size, ' ', cals_record_size);
	memcpy(cals.data() + index * cals_record_size, value, strlen(value));
    };
    cals_record(0, "srcdocid: NONE");
    cals_record(6, "rtype: 1");
    cals_record(7, "rorient: 000,270");
    cals_record(8, "rpelcnt: 000002,000002");
    const size_t cals_prefixes[] = {0, 5, 8, 127, 128, 767, 775, 896, 1024, 1280, 2047, cals_header_size};
    for (size_t size : cals_prefixes) {
	bu_mime_result *prefix = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, cals.data(), size, 0, &prefix) != BU_MIME_ERROR);
	const auto *facts = find(prefix, "application/cals-1840");
	if (size < strlen("srcdocid:")) MIME_CHECK(!facts);
	else {
	    MIME_CHECK(facts);
	    const char *kind = bu_mime_traits_get(facts, "cals.raster-type");
	    MIME_CHECK(size < cals_header_size ? !kind : BU_STR_EQUAL(kind, "1"));
	    MIME_CHECK(!bu_mime_traits_get(facts, "version") && !bu_mime_traits_get(facts, "byte-order"));
	    if (size < cals_header_size) MIME_CHECK(prefix->incomplete && !bu_mime_traits_get(facts, "bit-depth"));
	    else {
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "encoding"), "binary"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "bit-depth"), "1"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "compression"), "ccitt-group4"));
		MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(facts, "cals.header-layout"), "rtype"));
	    }
	}
	bu_mime_result_free(prefix);
    }
    for (const char *kind : {"2", "10", "1junk"}) {
	const std::string declaration = std::string("rtype: ") + kind;
	cals_record(6, declaration.c_str());
	bu_mime_result *future = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, cals.data(), cals.size(), 1, &future) != BU_MIME_ERROR);
	const auto *facts = find(future, "application/cals-1840");
	MIME_CHECK(facts && !bu_mime_traits_get(facts, "encoding") && !bu_mime_traits_get(facts, "bit-depth"));
	const char *literal = bu_mime_traits_get(facts, "cals.raster-type");
	MIME_CHECK(BU_STR_EQUAL(kind, "1junk") ? !literal : BU_STR_EQUAL(literal, kind));
	bu_mime_result_free(future);
    }
    cals_record(6, "rtype: 1");
    cals_record(15, "rtype: 2");
    bu_mime_result *conflict = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, cals.data(), cals.size(), 1, &conflict) != BU_MIME_ERROR);
    const auto *cals_facts = find(conflict, "application/cals-1840");
    MIME_CHECK(cals_facts && !bu_mime_traits_get(cals_facts, "cals.raster-type"));
    bu_mime_result_free(conflict);
    cals_record(15, "");
    cals_record(6, "notes: rtype: 1");
    cals.insert(cals.end(), {'r', 't', 'y', 'p', 'e', ':', ' ', '1'});
    conflict = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, cals.data(), cals.size(), 1, &conflict) != BU_MIME_ERROR);
    cals_facts = find(conflict, "application/cals-1840");
    MIME_CHECK(cals_facts && !bu_mime_traits_get(cals_facts, "cals.raster-type"));
    bu_mime_result_free(conflict);

    std::vector<unsigned char> bitmap(66, 0);
    bitmap[0] = 'B'; bitmap[1] = 'M'; bitmap[2] = 66; bitmap[10] = 62;
    bitmap[14] = 40; bitmap[18] = bitmap[22] = bitmap[26] = bitmap[28] = 1;
    bitmap[34] = 4;
    bitmap[58] = bitmap[59] = bitmap[60] = 255;
    bitmap[62] = 0x80;
    for (size_t size = 0; size <= bitmap.size(); ++size) {
	bu_mime_result *result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, bitmap.data(), size, 0, &result) != BU_MIME_ERROR);
	const auto *traits = find(result, "image/bmp");
	if (size < 2) MIME_CHECK(!traits);
	else {
	    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "encoding"), "binary"));
	    const struct {const char *key; const char *value; size_t end;} fields[] = {
		{"bmp.header-size", "40", 18}, {"bit-depth", "1", 30}, {"bmp.compression-code", "0", 34}
	    };
	    for (const auto &field : fields) {
		const char *value = bu_mime_traits_get(traits, field.key);
		MIME_CHECK(size < field.end ? !value : BU_STR_EQUAL(value, field.value));
	    }
	    MIME_CHECK(!bu_mime_traits_get(traits, "version"));
	}
	bu_mime_result_free(result);
    }
    bitmap[14] = 0xe7; bitmap[15] = 3;
    bu_mime_result *result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, bitmap.data(), bitmap.size(), 1, &result) == BU_MIME_MATCH);
    const auto *traits = find(result, "image/bmp");
    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "bmp.header-size"), "999"));
    MIME_CHECK(!bu_mime_traits_get(traits, "bit-depth") && !bu_mime_traits_get(traits, "bmp.compression-code"));
    bu_mime_result_free(result);

    const struct {const char *header; const char *standard; const char *version;} nitf[] = {
	{"NITF01.10", "nitf", "1.1"}, {"NITF02.00", "nitf", "2.0"}, {"NITF02.10", "nitf", "2.1"},
	{"NSIF01.00", "nsif", "1.0"}, {"NITF09.99", "nitf", NULL}
    };
    for (const auto &sample : nitf)
	for (size_t size = 0; size <= strlen(sample.header); ++size) {
	    result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, sample.header, size, 0, &result) != BU_MIME_ERROR);
	    traits = find(result, "application/vnd.nitf");
	    if (size < 4) MIME_CHECK(!traits);
	    else {
		MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "nitf.standard"), sample.standard));
		const char *version = bu_mime_traits_get(traits, "version");
		MIME_CHECK(size < 9 || !sample.version ? !version : BU_STR_EQUAL(version, sample.version));
		const char *literal = bu_mime_traits_get(traits, "nitf.header-version");
		MIME_CHECK(size < 9 ? !literal : BU_STR_EQUAL(literal, sample.header + 4));
	    }
	    bu_mime_result_free(result);
	}

    std::vector<unsigned char> geopackage(100, 0);
    memcpy(geopackage.data(), "SQLite format 3", 16);
    memcpy(geopackage.data() + 68, "GPKG", 4);
    geopackage[62] = 0x28; geopackage[63] = 0x3c;
    for (size_t size = 0; size <= geopackage.size(); ++size) {
	result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, geopackage.data(), size, 0, &result) != BU_MIME_ERROR);
	traits = find(result, "application/geopackage+sqlite3");
	if (size < 72) MIME_CHECK(!traits);
	else {
	    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "container"), "sqlite3"));
	    const char *version = bu_mime_traits_get(traits, "version");
	    const char *raw = bu_mime_traits_get(traits, "geopackage.user-version");
	    MIME_CHECK(BU_STR_EQUAL(version, "1.3") && BU_STR_EQUAL(raw, "10300"));
	}
	bu_mime_result_free(result);
    }
    geopackage[62] = geopackage[63] = 0;
    const struct {const char *id; const char *version;} historical[] = {{"GP10", "1.0"}, {"GP11", "1.1"}, {"NONE", NULL}};
    for (const auto &sample : historical) {
	memcpy(geopackage.data() + 68, sample.id, 4);
	result = NULL;
	MIME_CHECK(bu_mime_detect(registry, NULL, geopackage.data(), geopackage.size(), 1, &result) != BU_MIME_ERROR);
	traits = find(result, "application/geopackage+sqlite3");
	MIME_CHECK(sample.version ? traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "version"), sample.version) : !traits);
	bu_mime_result_free(result);
    }

    const std::vector<unsigned char> grib1 = {'G','R','I','B',0,0,12,1,'7','7','7','7'};
    const std::vector<unsigned char> grib2 = {'G','R','I','B',0,0,0,2,0,0,0,0,0,0,0,20,'7','7','7','7'};
    for (const auto &bytes : {grib1, grib2})
	for (size_t size = 0; size <= bytes.size(); ++size) {
	    result = NULL;
	    MIME_CHECK(bu_mime_detect(registry, NULL, bytes.data(), size, 0, &result) != BU_MIME_ERROR);
	    traits = find(result, "application/grib");
	    if (size < 4) MIME_CHECK(!traits);
	    else {
		MIME_CHECK(traits && !bu_mime_traits_get(traits, "version"));
		const char *edition = bu_mime_traits_get(traits, "grib.first-edition");
		MIME_CHECK(size < 8 ? !edition : BU_STR_EQUAL(edition, bytes[7] == 1 ? "1" : "2"));
	    }
	    bu_mime_result_free(result);
	}
    auto mixed = grib1;
    mixed.insert(mixed.end(), grib2.begin(), grib2.end());
    result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, mixed.data(), mixed.size(), 1, &result) == BU_MIME_MATCH);
    traits = find(result, "application/grib");
    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "grib.first-edition"), "1"));
    MIME_CHECK(!bu_mime_traits_get(traits, "version"));
    const char *const *editions = NULL;
    size_t count = 0;
    MIME_CHECK(!bu_mime_traits_get_set(traits, "grib.editions", &editions, &count));
    bu_mime_result_free(result);
    auto future = grib2;
    future[7] = 99;
    result = NULL;
    MIME_CHECK(bu_mime_detect(registry, NULL, future.data(), future.size(), 1, &result) == BU_MIME_MATCH);
    traits = find(result, "application/grib");
    MIME_CHECK(traits && BU_STR_EQUAL(bu_mime_traits_get(traits, "grib.first-edition"), "99"));
    MIME_CHECK(!bu_mime_traits_get(traits, "version"));
    bu_mime_result_free(result);
    std::istringstream unsupported(std::string(reinterpret_cast<const char *>(future.data()), future.size()));
    const auto declared = mime_grib::inspect(unsupported);
    MIME_CHECK(declared.edition_known && declared.edition == 99 && !declared.header_known);
    const std::string text_prefix = "GRIB discussion in a preamble\n";
    std::istringstream text(text_prefix + std::string(reinterpret_cast<const char *>(grib2.data()), grib2.size()));
    const auto following = mime_grib::inspect(text);
    MIME_CHECK(following.header_known && following.edition == 2 && following.offset == text_prefix.size());
    const std::string padding(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'x');
    std::istringstream stream(padding + std::string(reinterpret_cast<const char *>(grib2.data()), grib2.size()));
    const auto full = mime_grib::inspect(stream);
    MIME_CHECK(full.header_known && full.edition == 2 && full.offset == padding.size());
    return 0;
}

static int
mime_assertion_test()
{
    const char *members[] = {"a", "b"};
    const char *subset[] = {"a"};
    const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_SET("fixture.features", members, sizeof(members) / sizeof(members[0]))};
    const bu_mime_traits facts = {fields, sizeof(fields) / sizeof(fields[0])};
    const bu_mime_traits empty = BU_MIME_TRAITS_INIT_ZERO;
    MIME_CHECK(bu_mime_traits_match(&facts, NULL, &facts));
    MIME_CHECK(bu_mime_traits_match(NULL, &facts, &facts));
    MIME_CHECK(!bu_mime_traits_match(NULL, NULL, &facts));
    MIME_CHECK(bu_mime_traits_match(NULL, NULL, NULL));
    MIME_CHECK(bu_mime_traits_match(NULL, NULL, &empty));
    const bu_mime_trait partial = BU_MIME_TRAIT_SET("fixture.features", subset, 1);
    const bu_mime_traits partial_request = {&partial, 1};
    MIME_CHECK(!bu_mime_traits_match(&facts, NULL, &partial_request));
    const bu_mime_trait none = BU_MIME_TRAIT_SET("fixture.features", NULL, 0);
    const bu_mime_traits no_features = {&none, 1};
    MIME_CHECK(bu_mime_traits_match(NULL, &no_features, &no_features));
    MIME_CHECK(!bu_mime_traits_match(NULL, NULL, &no_features));
    MIME_CHECK(!bu_mime_traits_match(&facts, &no_features, &no_features));
    MIME_CHECK(bu_mime_traits_match(&no_features, &facts, &no_features));
    const bu_mime_trait conflicting_fields[] = {fields[0], BU_MIME_TRAIT_STRING("encoding", "ascii")};
    const bu_mime_traits conflicting = {conflicting_fields, sizeof(conflicting_fields) / sizeof(conflicting_fields[0])};
    const bu_mime_traits binary = {fields, 1};
    MIME_CHECK(!bu_mime_traits_match(&conflicting, &facts, &binary));
    MIME_CHECK(!bu_mime_traits_match(NULL, &conflicting, &binary));
    MIME_CHECK(!bu_mime_traits_match(&facts, NULL, &conflicting));
    MIME_CHECK(bu_mime_traits_match(&facts, &conflicting, &facts));
    const bu_mime_trait invalid_field = BU_MIME_TRAIT_STRING("invalid key", "value");
    const bu_mime_traits invalid = {&invalid_field, 1};
    MIME_CHECK(!bu_mime_traits_match(&invalid, NULL, NULL));
    MIME_CHECK(!bu_mime_traits_match(NULL, &invalid, &empty));
    MIME_CHECK(!bu_mime_traits_match(NULL, NULL, &invalid));
    return 0;
}

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    bu_mime_registry *registry = bu_mime_registry_create(0);
    int status = mime_product_test(registry) || mime_assertion_test() || mime_science_test(registry) || mime_raster_label_test(registry) || mime_heightfield_test(registry) || mime_leveller_test(registry) || mime_dwg_test(registry) || mime_lerc_test(registry) || mime_grid_families_test(registry) || mime_terrain_test(registry) || mime_obj_test(registry) || mime_opengex_test(registry) || mime_fbx_test(registry) || mime_3ds_test(registry) || mime_stl_test(registry) || mime_ply_test(registry) || mime_x_assbin_test(registry) || mime_scene_text_test(registry) || mime_blender_test(registry) || mime_amf_test(registry) || mime_dxf_test(registry) || mime_off_test(registry) || mime_raster_container_test(registry) || mime_sun_test(registry) || mime_brlcad_profiles_test(registry) || mime_set_test() || mime_trait_test() || mime_version_test() || mime_scene_profiles_test(registry) || mime_cad_headers_test(registry) || mime_fastgen_test(registry) || mime_mdl_profiles_test(registry) || mime_skeletal_profiles_test(registry) || mime_nff_profiles_test(registry) || mime_raw_profiles_test(registry) || mime_quick3d_pmx_profiles_test(registry) || mime_unreal_profiles_test(registry) || mime_ogre_profiles_test(registry) || mime_xml_profiles_test(registry) || mime_motion_profiles_test(registry) || mime_iff_profiles_test(registry) || mime_jpeg_test(registry) || mime_pnm_test(registry) || mime_container_scene_test(registry) || mime_vrml_test(registry) || mime_gltf_test(registry) || mime_step_test(registry) || mime_catalog_test(registry) || mime_registry_test(registry) || mime_signature_test(registry) ||
	mime_alternatives_test(registry) || mime_builtin_test(registry) || mime_evidence_test(registry) || mime_validation_test(registry) || mime_concurrency_test(registry);
    bu_mime_registry_destroy(registry);
    return status;
}
