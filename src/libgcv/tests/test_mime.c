/* T E S T _ M I M E . C
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

#include <string.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "rt/db_io.h"
#include "rt/db_attr.h"
#include "rt/mater.h"
#include "wdb.h"
#include "rt/db_internal.h"
#include "rt/db_fullpath.h"
#include "rt/geom.h"
#include "rt/search.h"
#include "rt/db_instance.h"
#include "bu/log.h"
#include "bu/str.h"
#include "gcv/api.h"

int gcv_result_exception(struct gcv_context *, const struct gcv_opts *, const void *, const char *);
int gcv_result_unknown_exception(struct gcv_context *, const struct gcv_opts *, const void *, const char *);
#ifdef TEST_GCV_ASSETIMPORT
int gcv_sdk_profiles(const char *ifc_fixture);
int gcv_sdk_ifc_profiles(const char *ifc_fixture);
int gcv_sdk_assertion_profiles(void);
int gcv_sdk_xml_charset_profiles(void);
#endif

#define GCV_MIME_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("GCV MIME check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static int
mime_noop(struct gcv_context *UNUSED(context), const struct gcv_opts *UNUSED(options),
	  const void *UNUSED(data), const char *UNUSED(target))
{
    return 1;
}

static int
mime_directory_read(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *source)
{
    const struct bu_mime_trait field = BU_MIME_TRAIT_STRING("container", "directory");
    const struct bu_mime_traits observed = {&field, 1};
    if (!bu_file_directory(source) || !bu_mime_traits_match(&observed, NULL, gcv_target_request_traits(context)))
	return 0;
    bu_avs_add(&context->messages, "directory-read", "validated");
    return 1;
}

static int
mime_directory_test(void)
{
    struct gcv_context context;
    struct bu_vls named = BU_VLS_INIT_ZERO, type = BU_VLS_INIT_ZERO, messages = BU_VLS_INIT_ZERO;
    char anonymous[MAXPATHLEN];
    FILE *temporary = bu_temp_file(anonymous, sizeof(anonymous));
    const char *alias = "application/x-example-directory", *extension = "mimedir";
    const char *canonical = "application/vnd.example.directory";
    const struct bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	"application/vnd.example.directory", "Directory fixture", &alias, 1, &extension, 1, NULL, 0};
    const struct bu_mime_trait container = BU_MIME_TRAIT_STRING("container", "directory");
    const struct bu_mime_capability capability = {"application/vnd.example.directory", &container, 1,
	BU_MIME_READ, BU_MIME_DEFER_INPUT_VALIDATION};
    const struct gcv_conversion conversion = {"Directory fixture", GCV_FILTER_READ, NULL, NULL, mime_directory_read};
    const struct gcv_handler handler = {&conversion, &capability, 1, BU_MIME_MAX_PRIORITY, NULL, NULL};
    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), &definition, 1, &handler, 1};
    const struct gcv_filter *reader;
    struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    GCV_MIME_CHECK(temporary && !fclose(temporary) && bu_file_delete(anonymous));
    bu_vls_sprintf(&named, "%s.%s", anonymous, extension);
    bu_mkdir(anonymous);
    bu_mkdir(bu_vls_cstr(&named));
    GCV_MIME_CHECK(bu_file_directory(anonymous) && bu_file_directory(bu_vls_cstr(&named)));
    gcv_context_init(&context);
    GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, &messages));
    reader = gcv_resolve_variant(&context, GCV_FILTER_READ, bu_vls_cstr(&named), NULL,
	NULL, NULL, &type, &resolved, &messages);
    GCV_MIME_CHECK(reader && BU_STR_EQUAL(bu_vls_cstr(&type), canonical) && !resolved.count);
    GCV_MIME_CHECK(gcv_execute_variant(&context, reader, canonical, NULL, NULL, 0, NULL, bu_vls_cstr(&named)));
    GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "directory-read"), "validated"));
    GCV_MIME_CHECK(!gcv_resolve_filter(&context, GCV_FILTER_READ, anonymous, NULL, NULL, &type, &messages));
    reader = gcv_resolve_filter(&context, GCV_FILTER_READ, anonymous, alias, NULL, &type, &messages);
    GCV_MIME_CHECK(reader && BU_STR_EQUAL(bu_vls_cstr(&type), canonical));
    GCV_MIME_CHECK(gcv_execute_variant(&context, reader, canonical, NULL, NULL, 0, NULL, anonymous));
    {
	const struct bu_mime_trait unknown = BU_MIME_TRAIT_STRING("version", "unobserved");
	const struct bu_mime_traits request = {&unknown, 1};
	GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, canonical, &request, NULL, 0, NULL, anonymous));
    }
    {
	struct bu_mime_result *observations = NULL;
	GCV_MIME_CHECK(gcv_detect_file(&context, anonymous, &observations, &messages) == BU_MIME_ERROR && !observations);
    }
    bu_dirclear(bu_vls_cstr(&named));
    bu_dirclear(anonymous);
    GCV_MIME_CHECK(!bu_file_exists(bu_vls_cstr(&named), NULL) && !bu_file_exists(anonymous, NULL));
    temporary = fopen(bu_vls_cstr(&named), "wb");
    GCV_MIME_CHECK(temporary && !fclose(temporary));
    reader = gcv_resolve_filter(&context, GCV_FILTER_READ, bu_vls_cstr(&named), canonical, NULL, &type, &messages);
    GCV_MIME_CHECK(reader && !gcv_execute_variant(&context, reader, canonical, NULL, NULL, 0, NULL, bu_vls_cstr(&named)));
    GCV_MIME_CHECK(bu_file_delete(bu_vls_cstr(&named)));
    GCV_MIME_CHECK(!gcv_resolve_filter(&context, GCV_FILTER_READ, bu_vls_cstr(&named), canonical, NULL, &type, &messages));
    gcv_context_destroy(&context);
    bu_mime_traits_free(&resolved);
    bu_vls_free(&named);
    bu_vls_free(&type);
    bu_vls_free(&messages);
    return 0;
}

static int
mime_structural_test(struct gcv_context *context, struct bu_vls *messages)
{
    unsigned char binary[134] = {0};
    const unsigned char one[] = {0, 0, 0x80, 0x3f};
    struct bu_mime_probe_input input = {binary, sizeof(binary), 1, sizeof(binary), 1};
    struct bu_mime_result *result = NULL;
    struct bu_vls type = BU_VLS_INIT_ZERO;
    const struct gcv_filter *filter;
    char filename[MAXPATHLEN];
    FILE *fp;
    memcpy(binary, "solid binary header", 19);
    binary[80] = 1;
    memcpy(binary + 92, one, sizeof(one));
    memcpy(binary + 108, one, sizeof(one));
    memcpy(binary + 124, one, sizeof(one));
    GCV_MIME_CHECK(gcv_detect(context, "misleading.png", &input, &result, messages) == BU_MIME_MATCH);
    GCV_MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "model/stl"));
    GCV_MIME_CHECK(result->candidates[0].evidence == BU_MIME_EVIDENCE_SIGNATURE);
    bu_mime_result_free(result);
    fp = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(fp && fwrite(binary, 1, sizeof(binary), fp) == sizeof(binary));
    GCV_MIME_CHECK(!fclose(fp));
    filter = gcv_resolve_filter(context, GCV_FILTER_READ, filename, NULL, NULL, &type, messages);
    GCV_MIME_CHECK(filter && BU_STR_EQUAL(filter->name, "STL Reader"));
    GCV_MIME_CHECK(gcv_execute_format(context, filter, bu_vls_cstr(&type), NULL, 0, NULL, filename));
    GCV_MIME_CHECK(!gcv_target_mime(context));
    {
	struct directory **objects = NULL;
	size_t count = db_ls(context->dbip, DB_LS_PRIM, NULL, &objects);
	GCV_MIME_CHECK(count > 0);
	bu_free(objects, "database primitives");
    }
    GCV_MIME_CHECK(bu_file_delete(filename));
    fp = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(fp && !fclose(fp));
    GCV_MIME_CHECK(bu_file_delete(filename));
    filter = gcv_select_filter(context, GCV_FILTER_WRITE, "model/vnd.brlcad", NULL, messages);
    GCV_MIME_CHECK(filter && gcv_execute_format(context, filter, "model/vnd.brlcad", NULL, 0, NULL, filename));
    filter = gcv_resolve_filter(context, GCV_FILTER_READ, filename, NULL, NULL, &type, messages);
    GCV_MIME_CHECK(filter && BU_STR_EQUAL(bu_vls_cstr(&type), "model/vnd.brlcad"));
    GCV_MIME_CHECK(bu_file_delete(filename));
    bu_vls_free(&type);
    return 0;
}

static int
mime_discovery_test(struct gcv_context *context)
{
    const struct bu_ptbl *filters = gcv_list_filters(context);
    struct gcv_handler_info *info = NULL;
    size_t count = 0;

    GCV_MIME_CHECK(gcv_list_handlers(context, NULL, &info, &count));
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	const struct gcv_filter *filter = (const struct gcv_filter *)BU_PTBL_GET(filters, i);
	int advertised = 0;
	for (size_t j = 0; j < count; ++j) {
	    if (filter->filter_type == info[j].operation && BU_STR_EQUAL(filter->name, info[j].name)) {
		advertised = 1;
		break;
	    }
	}
	if (advertised != (filter->filter_type != GCV_FILTER_FILTER)) {
	    bu_log("Handler '%s' has incorrect MIME capability discovery\n", filter->name);
	    gcv_handler_list_free(info, count);
	    return 1;
	}
    }
    gcv_handler_list_free(info, count);
    return 0;
}

static int
mime_plugin_test(void)
{
    struct gcv_context context;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_mime_result *result = NULL;
    const struct gcv_filter *filter;
    char filename[MAXPATHLEN];
    FILE *fp;
    gcv_context_init(&context);
    GCV_MIME_CHECK(!mime_discovery_test(&context));
    GCV_MIME_CHECK(!mime_structural_test(&context, &messages));
#ifdef TEST_GCV_GDAL
    GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, "image/tiff", "GDAL Reader", &messages));
#endif
#ifdef TEST_GCV_ASSETIMPORT
    GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, "model/stl", "Assetimport Reader", &messages));
    GCV_MIME_CHECK(BU_STR_EQUAL(gcv_select_filter(&context, GCV_FILTER_READ, "model/stl", NULL, &messages)->name, "STL Reader"));
#endif
    GCV_MIME_CHECK(find_filter(GCV_FILTER_READ, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, NULL, &context) ==
	gcv_select_filter(&context, GCV_FILTER_READ, "model/vnd.brlcad", NULL, &messages));
    fp = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(fp);
    GCV_MIME_CHECK(fputs("MIMETEST", fp) >= 0);
    GCV_MIME_CHECK(!fclose(fp));
    GCV_MIME_CHECK(bu_mime_detect_file(gcv_mime_registry(&context), filename, &result) == BU_MIME_MATCH);
    GCV_MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, "application/vnd.example.mimetest"));
    filter = gcv_select_filter(&context, GCV_FILTER_READ, result->candidates[0].name, NULL, &messages);
    GCV_MIME_CHECK(filter && BU_STR_EQUAL(filter->name, "Runtime MIME fixture"));
    GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, "model/vnd.example.mimetest", NULL, &messages) == filter);
    GCV_MIME_CHECK(!gcv_execute_format(&context, filter, "image/jpeg", NULL, 0, NULL, filename));
    GCV_MIME_CHECK(!bu_avs_get(&context.messages, "mime-handler"));
    GCV_MIME_CHECK(!gcv_target_mime(&context));
    {
	struct gcv_context other;
	gcv_context_init(&other);
	GCV_MIME_CHECK(!gcv_execute_format(&other, filter, "image/png", NULL, 0, NULL, filename));
	gcv_context_destroy(&other);
    }
    GCV_MIME_CHECK(gcv_execute_format(&context, filter, "application/x-mimetest", NULL, 0, NULL, filename));
    GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "mime-handler"), "executed"));
    GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, "application/x-mimetest", NULL, &messages) == filter);
    GCV_MIME_CHECK(!gcv_select_filter(&context, GCV_FILTER_WRITE, result->candidates[0].name, NULL, &messages));
    bu_mime_result_free(result);
    GCV_MIME_CHECK(bu_file_delete(filename));

    {
	const struct bu_mime_capability types[] = {
	    BU_MIME_CAPABILITY("application/vnd.example.mimetest", NULL, 0, 0),
	};
	const struct gcv_conversion competitor = {"Other MIME handler", GCV_FILTER_READ, NULL, NULL, mime_noop};
	struct gcv_handler handler = {&competitor, types, 1, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
	struct gcv_plugin_v2 plugin = {
	    GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0, &handler, 1
	};
	GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, &messages));
	GCV_MIME_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, types[0].mime_type, NULL, &messages));
	GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, types[0].mime_type, "Runtime MIME fixture", &messages) == filter);
	GCV_MIME_CHECK(!gcv_register_plugin(&context, &plugin, &messages));
    }
    {
	struct bu_mime_definition definition = {0};
	const char *extension = "mimetest";
	const char *type = "application/vnd.example.second";
	const struct gcv_conversion alternative = {"Other file type", GCV_FILTER_READ, NULL, NULL, mime_noop};
	const struct bu_mime_capability capability = BU_MIME_CAPABILITY(type, NULL, 0, 0);
	const struct gcv_handler handler = {&alternative, &capability, 1, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
	struct gcv_plugin_v2 plugin = {
	    GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), &definition, 1, &handler, 1
	};
	struct bu_vls resolved = BU_VLS_INIT_ZERO;
	definition.abi_version = BU_MIME_ABI_VERSION;
	definition.struct_size = sizeof(definition);
	definition.name = type;
	definition.extensions = &extension;
	definition.extension_count = 1;
	GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, &messages));
	/* Ambiguous implementations of one candidate must not cause the
	 * resolver to silently choose a different candidate file type. */
	GCV_MIME_CHECK(!gcv_resolve_filter(&context, GCV_FILTER_READ, NULL, "mimetest", NULL, &resolved, &messages));
	GCV_MIME_CHECK(!bu_vls_strlen(&resolved));
	bu_vls_free(&resolved);
    }
    {
	struct bu_mime_definition definition = {0};
	struct gcv_handler invalid_handler = {0};
	struct gcv_plugin_v2 invalid = {
	    GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), &definition, 1, &invalid_handler, 1
	};
	struct bu_vls canonical = BU_VLS_INIT_ZERO;
	definition.abi_version = BU_MIME_ABI_VERSION;
	definition.struct_size = sizeof(definition);
	definition.name = "model/vnd.example.rollback";
	GCV_MIME_CHECK(!gcv_register_plugin(&context, &invalid, &messages));
	GCV_MIME_CHECK(!bu_mime_canonical(gcv_mime_registry(&context), definition.name, &canonical));
	bu_vls_free(&canonical);
    }
    {
	const struct gcv_conversion operation = {"Runtime operation", GCV_FILTER_FILTER, NULL, NULL, mime_noop};
	const struct gcv_handler handler = {&operation, NULL, 0, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
	const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2),
	    NULL, 0, &handler, 1};
	const struct bu_ptbl *filters;
	const struct gcv_filter *registered = NULL;
	GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, &messages));
	filters = gcv_list_filters(&context);
	for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	    const struct gcv_filter *candidate = (const struct gcv_filter *)BU_PTBL_GET(filters, i);
	    if (BU_STR_EQUAL(candidate->name, "Runtime operation"))
		registered = candidate;
	}
	GCV_MIME_CHECK(registered && gcv_execute(&context, registered, NULL, 0, NULL, NULL));
    }
    bu_vls_free(&messages);
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_native_probe_test(void)
{
    struct gcv_context context;
    struct gcv_handler_info *info = NULL;
    size_t count = 0;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const struct {
	const char *bytes;
	const char *type;
	enum bu_mime_evidence evidence;
    } samples[] = {
	{"# vtk DataFile Version 3.0\nexample\nASCII\nDATASET POLYDATA\n", "application/vnd.kitware.vtk", BU_MIME_EVIDENCE_SIGNATURE},
	{"<?xml version=\"1.0\"?>\n<!-- comment -->\n<VTKFile type=\"PolyData\">", "application/vnd.kitware.vtk+xml", BU_MIME_EVIDENCE_SIGNATURE},
	{"#VRML V2.0 utf8\nShape {}\n", "model/vrml", BU_MIME_EVIDENCE_SIGNATURE},
	{"#VRML V1.0 ascii\nSeparator {}\n", "model/vrml", BU_MIME_EVIDENCE_SIGNATURE},
	{"# OBJ\no triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", "model/obj", BU_MIME_EVIDENCE_HEURISTIC},
	{"v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3//1 -2//1 -1//1 # face\n", "model/obj", BU_MIME_EVIDENCE_HEURISTIC},
	{"{\"asset\":{\"version\":\"2.0\"},\"meshes\":[]}", "model/gltf+json", BU_MIME_EVIDENCE_HEURISTIC}
    };
    const char *invalid[] = {
	"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 2 3\n",
	"v nan 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
	"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 nope\n",
	"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1///1 2 3\n",
	"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\nordinary unrelated text\n",
	"<VTKFileSomethingElse>\n", "ordinary unrelated text\n",
	"{\"extras\":{\"asset\":{\"version\":\"2.0\"}}}",
	"{\"asset\":{\"version\":\"unrelated\"}}"
    };
    gcv_context_init(&context);
    GCV_MIME_CHECK(gcv_list_handlers(&context, "model/obj", &info, &count) && count);
    GCV_MIME_CHECK(BU_STR_EQUAL(info[0].mime_type, "model/obj"));
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
	struct bu_mime_result *result = NULL;
	size_t length = strlen(samples[i].bytes);
	unsigned char *bytes = (unsigned char *)bu_malloc(length, "probe sample");
	memcpy(bytes, samples[i].bytes, length);
	struct bu_mime_probe_input input = {bytes, length, 1, length, 1};
	GCV_MIME_CHECK(gcv_detect(&context, "wrong.png", &input, &result, &messages) == BU_MIME_MATCH);
	GCV_MIME_CHECK(BU_STR_EQUAL(result->candidates[0].name, samples[i].type));
	GCV_MIME_CHECK(result->candidates[0].evidence == samples[i].evidence);
	bu_mime_result_free(result);
	/* Every truncation is a valid bounded input, including empty data. */
	for (size_t size = 0; size < length; ++size) {
	    input.size = size;
	    input.complete = 0;
	    GCV_MIME_CHECK(gcv_detect(&context, NULL, &input, &result, &messages) != BU_MIME_ERROR);
	    bu_mime_result_free(result);
	}
	bu_free(bytes, "probe sample");
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
	struct bu_mime_result *result = NULL;
	size_t length = strlen(invalid[i]);
	struct bu_mime_probe_input input = {(const unsigned char *)invalid[i], length, 1, length, 1};
	GCV_MIME_CHECK(gcv_detect(&context, NULL, &input, &result, &messages) == BU_MIME_UNKNOWN_RESULT);
	bu_mime_result_free(result);
    }
    gcv_context_destroy(&context);
    GCV_MIME_CHECK(BU_STR_EQUAL(info[0].mime_type, "model/obj"));
    gcv_handler_list_free(info, count);
    bu_vls_free(&messages);
    return 0;
}

static int
mime_feature_test(void)
{
    struct gcv_context context;
    const char *features[] = {"feature-a", "feature-b"};
    const struct bu_mime_trait limited[] = {BU_MIME_TRAIT_SET("example.required-features", features, 1)};
    const struct bu_mime_trait broad[] = {BU_MIME_TRAIT_SET("example.required-features", features, 2)};
    const struct bu_mime_capability limited_cap = BU_MIME_CAPABILITY("model/vnd.example.features", limited, 1, 0);
    const struct bu_mime_capability broad_cap = BU_MIME_CAPABILITY("model/vnd.example.features", broad, 1, 0);
    const struct gcv_conversion limited_reader = {"Feature A Reader", GCV_FILTER_READ, NULL, NULL, mime_noop};
    const struct gcv_conversion broad_reader = {"Feature AB Reader", GCV_FILTER_READ, NULL, NULL, mime_noop};
    const struct gcv_handler handlers[] = {
	{&limited_reader, &limited_cap, 1, BU_MIME_MAX_PRIORITY, NULL, NULL},
	{&broad_reader, &broad_cap, 1, BU_MIME_DEFAULT_PRIORITY, NULL, NULL}
    };
    const struct bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	"model/vnd.example.features", NULL, NULL, 0, NULL, 0, NULL, 0};
    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), &definition, 1, handlers, 2};
    struct bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    struct gcv_handler_info *snapshot = NULL;
    size_t count = 0;
    gcv_context_init(&context);
    GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, NULL));
    GCV_MIME_CHECK(!gcv_select_variant(&context, GCV_FILTER_READ, definition.name, NULL, NULL, NULL));
    GCV_MIME_CHECK(bu_mime_traits_add_set(&traits, "example.required-features", features, 2));
    const struct gcv_filter *selected = gcv_select_variant(&context, GCV_FILTER_READ, definition.name, &traits, NULL, NULL);
    GCV_MIME_CHECK(selected && BU_STR_EQUAL(selected->name, "Feature AB Reader"));
    GCV_MIME_CHECK(!gcv_select_variant(&context, GCV_FILTER_READ, definition.name, &traits, "Feature A Reader", NULL));
    bu_mime_traits_free(&traits);
    GCV_MIME_CHECK(bu_mime_traits_add_set(&traits, "example.required-features", NULL, 0));
    selected = gcv_select_variant(&context, GCV_FILTER_READ, definition.name, &traits, NULL, NULL);
    GCV_MIME_CHECK(selected && BU_STR_EQUAL(selected->name, "Feature A Reader"));
    GCV_MIME_CHECK(gcv_list_handlers(&context, definition.name, &snapshot, &count) && count == 2);
    gcv_context_destroy(&context);
    for (size_t i = 0; i < count; ++i) {
	const char *const *members = NULL;
	size_t member_count = 0;
	GCV_MIME_CHECK(bu_mime_traits_get_set(&snapshot[i].traits, "example.required-features", &members, &member_count));
	GCV_MIME_CHECK(member_count && BU_STR_EQUAL(members[0], features[0]));
    }
    gcv_handler_list_free(snapshot, count);
    bu_mime_traits_free(&traits);
    return 0;
}

static int
mime_variant_callback(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *UNUSED(target))
{
    const struct bu_mime_traits *traits = gcv_target_traits(context);
    const char *encoding = bu_mime_traits_get(traits, "encoding");
    if (!BU_STR_EQUAL(gcv_target_mime(context), "model/stl") || !encoding)
	return 0;
    bu_avs_add(&context->messages, "variant", encoding);
    const struct bu_mime_traits *request = gcv_target_request_traits(context);
    const char *explicit_encoding = bu_mime_traits_get(request, "encoding");
    const char *explicit_order = bu_mime_traits_get(request, "byte-order");
    bu_avs_add(&context->messages, "request-present", request ? "yes" : "no");
    bu_avs_add(&context->messages, "request-encoding", explicit_encoding ? explicit_encoding : "none");
    bu_avs_add(&context->messages, "request-order", explicit_order ? explicit_order : "none");
    return 1;
}

static int
mime_nested_request_callback(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *target)
{
    const struct bu_mime_traits *request = gcv_target_request_traits(context);
    const struct bu_mime_traits *traits = gcv_target_traits(context);
    if (!request || !BU_STR_EQUAL(bu_mime_traits_get(request, "encoding"), "binary") ||
	bu_mime_traits_get(request, "byte-order") ||
	!BU_STR_EQUAL(bu_mime_traits_get(traits, "byte-order"), "little-endian"))
	return 0;
    const struct bu_mime_trait order = BU_MIME_TRAIT_STRING("byte-order", "little-endian");
    const struct bu_mime_traits nested_request = {&order, 1};
    const struct gcv_filter *nested = gcv_select_variant(context, GCV_FILTER_WRITE,
	"model/stl", &nested_request, "Variant writer", NULL);
    if (!nested || !gcv_execute_variant(context, nested, "model/stl", &nested_request, NULL, 0, NULL, target))
	return 0;
    return gcv_target_request_traits(context) == request && gcv_target_traits(context) == traits &&
	BU_STR_EQUAL(bu_avs_get(&context->messages, "request-encoding"), "none") &&
	BU_STR_EQUAL(bu_avs_get(&context->messages, "request-order"), "little-endian");
}

static int
mime_variant_test(void)
{
    struct gcv_context context;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_vls type = BU_VLS_INIT_ZERO;
    struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    const struct bu_mime_trait ascii[] = {BU_MIME_TRAIT_STRING("encoding", "ascii")};
    const struct bu_mime_trait binary[] = {
        BU_MIME_TRAIT_STRING("encoding", "binary"),
        BU_MIME_TRAIT_STRING("byte-order", "little-endian"),
    };
    const struct bu_mime_capability ascii_cap = BU_MIME_CAPABILITY("model/stl", ascii, 1, 0);
    const struct bu_mime_capability binary_cap = BU_MIME_CAPABILITY("model/stl", binary, 2, 0);
    const struct bu_mime_capability output_caps[] = {
	BU_MIME_CAPABILITY("model/stl", ascii, 1, 0), BU_MIME_CAPABILITY("model/stl", binary, 2, 0)
    };
    const struct gcv_conversion ascii_reader = {"ASCII fixture", GCV_FILTER_READ, NULL, NULL, mime_variant_callback};
    const struct gcv_conversion binary_reader = {"Binary fixture", GCV_FILTER_READ, NULL, NULL, mime_variant_callback};
    const struct gcv_conversion writer = {"Variant writer", GCV_FILTER_WRITE, NULL, NULL, mime_variant_callback};
    const struct gcv_conversion nested_writer = {"Nested variant writer", GCV_FILTER_WRITE, NULL, NULL, mime_nested_request_callback};
    const struct gcv_handler handlers[] = {
	{&ascii_reader, &ascii_cap, 1, BU_MIME_MAX_PRIORITY, NULL, NULL},
	{&binary_reader, &binary_cap, 1, BU_MIME_MAX_PRIORITY, NULL, NULL},
	{&writer, output_caps, 2, BU_MIME_MAX_PRIORITY, NULL, NULL},
	{&nested_writer, &binary_cap, 1, BU_MIME_MAX_PRIORITY, NULL, NULL}
    };
    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0,
	handlers, sizeof(handlers) / sizeof(handlers[0])};
    const char *ascii_stl = "solid t\nfacet normal 0 0 1\n";
    unsigned char binary_stl[134] = {0};
    char filename[MAXPATHLEN];
    FILE *fp;
    const struct gcv_filter *filter;
    gcv_context_init(&context);
    GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, &messages));
    GCV_MIME_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, "model/stl", "ASCII fixture", &messages));
    fp = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(fp && fputs(ascii_stl, fp) >= 0 && !fclose(fp));
    filter = gcv_resolve_variant(&context, GCV_FILTER_READ, filename, NULL, NULL, NULL, &type, &resolved, &messages);
    GCV_MIME_CHECK(filter && BU_STR_EQUAL(filter->name, "ASCII fixture"));
    GCV_MIME_CHECK(gcv_execute_variant(&context, filter, bu_vls_cstr(&type), &resolved, NULL, 0, NULL, filename));
    GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "variant"), "ascii"));
    GCV_MIME_CHECK(!gcv_target_traits(&context));
    GCV_MIME_CHECK(!gcv_target_request_traits(&context) && !gcv_target_request_traits(NULL));
    GCV_MIME_CHECK(!gcv_resolve_variant(&context, GCV_FILTER_READ, filename, "model/stl", NULL, "Binary fixture", &type, &resolved, &messages));
    {
	struct bu_mime_traits request = {binary, 1};
	GCV_MIME_CHECK(!gcv_resolve_variant(&context, GCV_FILTER_READ, filename, "model/stl", &request, NULL, &type, &resolved, &messages));
    }
    fp = fopen(filename, "wb");
    memcpy(binary_stl, "solid deceptive binary", 22);
    binary_stl[80] = 1;
    GCV_MIME_CHECK(fp && fwrite(binary_stl, 1, sizeof(binary_stl), fp) == sizeof(binary_stl) && !fclose(fp));
    filter = gcv_resolve_variant(&context, GCV_FILTER_READ, filename, NULL, NULL, NULL, &type, &resolved, &messages);
    GCV_MIME_CHECK(filter && BU_STR_EQUAL(filter->name, "Binary fixture"));
    GCV_MIME_CHECK(gcv_execute_variant(&context, filter, bu_vls_cstr(&type), &resolved, NULL, 0, NULL, filename));
    GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "variant"), "binary"));
    {
	/* A partial request must pass the complete matching output clause. */
	struct bu_mime_traits request = {binary + 1, 1};
	filter = gcv_resolve_variant(&context, GCV_FILTER_WRITE, NULL, "model/stl", &request, "Variant writer", &type, &resolved, &messages);
	GCV_MIME_CHECK(filter && BU_STR_EQUAL(bu_mime_traits_get(&resolved, "encoding"), "binary"));
	GCV_MIME_CHECK(gcv_execute_variant(&context, filter, bu_vls_cstr(&type), &resolved, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(gcv_execute_variant(&context, filter, "model/stl", &request, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "request-encoding"), "none"));
	GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "request-order"), "little-endian"));
    }
    filter = gcv_resolve_variant(&context, GCV_FILTER_READ, filename, "model/stl", NULL,
	"Binary fixture", &type, &resolved, &messages);
    GCV_MIME_CHECK(filter && gcv_execute_variant(&context, filter, "model/stl", NULL, NULL, 0, NULL, filename));
    GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "request-present"), "no"));
    {
	const struct bu_mime_traits empty = BU_MIME_TRAITS_INIT_ZERO;
	GCV_MIME_CHECK(gcv_execute_variant(&context, filter, "model/stl", &empty, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(BU_STR_EQUAL(bu_avs_get(&context.messages, "request-present"), "yes"));
	const struct bu_mime_traits request = {binary, 1};
	filter = gcv_select_variant(&context, GCV_FILTER_WRITE, "model/stl", &request, "Nested variant writer", &messages);
	GCV_MIME_CHECK(filter && gcv_execute_variant(&context, filter, "model/stl", &request, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(!gcv_target_traits(&context) && !gcv_target_request_traits(&context));
    }
    fp = fopen(filename, "wb");
    GCV_MIME_CHECK(fp && fputs("# vtk DataFile Version 3.0\ntest\nASCII\nDATASET POLYDATA\n", fp) >= 0 && !fclose(fp));
    filter = gcv_resolve_filter(&context, GCV_FILTER_READ, filename, NULL, "VTK Reader", &type, &messages);
    GCV_MIME_CHECK(filter);
    fp = fopen(filename, "wb");
    GCV_MIME_CHECK(fp && fputs("# vtk DataFile Version 3.0\ntest\nBINARY\nDATASET POLYDATA\n", fp) >= 0 && !fclose(fp));
    GCV_MIME_CHECK(!gcv_resolve_filter(&context, GCV_FILTER_READ, filename, NULL, "VTK Reader", &type, &messages));
    GCV_MIME_CHECK(bu_file_delete(filename));
    bu_mime_traits_free(&resolved);
    bu_vls_free(&messages);
    bu_vls_free(&type);
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_stl_context_test(void)
{
    struct gcv_context context;
    const char *samples[] = {
	"solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\n",
	"solid test\nfacet normal 0 0 1\nouter loop\nvertex nope 0 0\nendloop\nendfacet\nendsolid test\n",
	"solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nvertex 1 1 0\nendloop\nendfacet\nendsolid test\n",
	"solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\n"
    };
    gcv_context_init(&context);
    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, "model/stl", "STL Reader", NULL);
    GCV_MIME_CHECK(reader);
    for (size_t i = 0; i <= sizeof(samples) / sizeof(samples[0]); ++i) {
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	GCV_MIME_CHECK(stream);
	if (i < sizeof(samples) / sizeof(samples[0])) {
	    GCV_MIME_CHECK(fwrite(samples[i], 1, strlen(samples[i]), stream) == strlen(samples[i]));
	} else {
	    unsigned char binary[85] = {0};
	    binary[80] = 1;
	    GCV_MIME_CHECK(fwrite(binary, 1, sizeof(binary), stream) == sizeof(binary));
	}
	GCV_MIME_CHECK(!fclose(stream));
	struct bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", i < sizeof(samples) / sizeof(samples[0]) ? "ascii" : "binary");
	struct bu_mime_traits request = {&encoding, 1};
	GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "model/stl", &request, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(bu_file_delete(filename));
    }
    {
	const char *facet = "facet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\n";
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	GCV_MIME_CHECK(stream && fprintf(stream, "solid first\n%sendsolid first\nsolid second\n%sendsolid second\n", facet, facet) > 0);
	GCV_MIME_CHECK(!fclose(stream));
	const char *arguments[] = {"--starting-ident", "731"};
	for (size_t pass = 0; pass < 2; ++pass) {
	    GCV_MIME_CHECK(gcv_execute_format(&context, reader, "model/stl", NULL, 2, arguments, filename));
	    struct directory **objects = NULL;
	    size_t count = db_ls(context.dbip, DB_LS_PRIM, NULL, &objects);
	    GCV_MIME_CHECK(count == (pass + 1) * 2);
	    for (size_t object = 0; object < count; ++object) {
		struct rt_db_internal internal;
		RT_DB_INTERNAL_INIT(&internal);
		GCV_MIME_CHECK(rt_db_get_internal(&internal, objects[object], context.dbip, NULL) == ID_BOT);
		struct rt_bot_internal *bot = (struct rt_bot_internal *)internal.idb_ptr;
		GCV_MIME_CHECK(bot->num_vertices == 3 && bot->num_faces == 1);
		rt_db_free_internal(&internal);
	    }
	    bu_free(objects, "STL primitives");
	}
	GCV_MIME_CHECK(bu_file_delete(filename));
    }
    {
	enum {header_size = 84, record_size = 50, count_at = 80};
	unsigned char binary[header_size + record_size] = {0};
	memcpy(binary, "solid binary STL", strlen("solid binary STL"));
	binary[count_at] = 1;
	/* IEEE binary32 normal Z, second vertex X and third vertex Y. */
	const size_t ones[] = {header_size + 2 * sizeof(uint32_t), header_size + 6 * sizeof(uint32_t), header_size + 10 * sizeof(uint32_t)};
	for (size_t i = 0; i < sizeof(ones) / sizeof(ones[0]); ++i) {
	    binary[ones[i] + 2] = 0x80;
	    binary[ones[i] + 3] = 0x3f;
	}
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	GCV_MIME_CHECK(stream && fwrite(binary, 1, sizeof(binary), stream) == sizeof(binary) && !fclose(stream));
	GCV_MIME_CHECK(!db_update_ident(context.dbip, "Before STL binary assertion", 10));
	const size_t before = db_directory_size(context.dbip);
	const struct bu_mime_trait unsupported[] = {
	    BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion"), BU_MIME_TRAIT_STRING("stl.utf8-bom", "false"),
	    BU_MIME_TRAIT_STRING("byte-order", "big-endian")
	};
	for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	    const struct bu_mime_traits request = {&unsupported[i], 1};
	    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "model/stl", &request, NULL, 0, NULL, filename));
	    GCV_MIME_CHECK(db_directory_size(context.dbip) == before && BU_STR_EQUAL(context.dbip->dbi_title, "Before STL binary assertion") && EQUAL(context.dbip->dbi_local2base, 10));
	}
	const struct bu_mime_trait values[] = {BU_MIME_TRAIT_STRING("stl.serialization", "binary"),
	    BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("byte-order", "little-endian")};
	const struct bu_mime_traits request = {values, sizeof(values) / sizeof(values[0])};
	GCV_MIME_CHECK(gcv_execute_variant(&context, reader, "model/stl", &request, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(db_directory_size(context.dbip) > before);
	const char *options[] = {"--binary"};
	GCV_MIME_CHECK(gcv_execute_variant(&context, reader, "model/stl", &request, NULL, 1, options, filename));
	stream = fopen(filename, "ab");
	GCV_MIME_CHECK(stream && fputc('x', stream) != EOF && !fclose(stream));
	GCV_MIME_CHECK(!db_update_ident(context.dbip, "Before STL trailer rejection", 10));
	const size_t after = db_directory_size(context.dbip);
	GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "model/stl", &request, NULL, 1, options, filename));
	GCV_MIME_CHECK(db_directory_size(context.dbip) == after && BU_STR_EQUAL(context.dbip->dbi_title, "Before STL trailer rejection") && EQUAL(context.dbip->dbi_local2base, 10));
	GCV_MIME_CHECK(bu_file_delete(filename));
    }
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_ply_profile_test(void)
{
    struct gcv_context context;
    struct bu_mime_traits asserted = BU_MIME_TRAITS_INIT_ZERO;
    char filename[MAXPATHLEN];
    const char *tail = "format ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
	"element face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n";
    gcv_context_init(&context);
    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, "application/x-ply", NULL, NULL);
    GCV_MIME_CHECK(reader && BU_STR_EQUAL(reader->name, "PLY Reader"));
    GCV_MIME_CHECK(find_filter(GCV_FILTER_READ, BU_MIME_MODEL_PLY, NULL, &context) == reader);
    GCV_MIME_CHECK(db_update_ident(context.dbip, "profile marker", 10) == 0);
    const size_t before = db_directory_size(context.dbip);
    const size_t padding = bu_mime_scan_limit(gcv_mime_registry(&context)) + 1;
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(stream && fputs("ply\n", stream) >= 0);
    for (size_t i = 0; i < padding; ++i) GCV_MIME_CHECK(fputc(' ', stream) != EOF);
    GCV_MIME_CHECK(fputs(tail, stream) >= 0 && !fclose(stream));
    GCV_MIME_CHECK(gcv_resolve_filter(&context, GCV_FILTER_READ, filename, "application/x-ply", "PLY Reader", NULL, NULL) == reader);
    GCV_MIME_CHECK(bu_mime_traits_add(&asserted, "encoding", "binary"));
    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "application/x-ply", &asserted, NULL, 0, NULL, filename));
    bu_mime_traits_free(&asserted);
    GCV_MIME_CHECK(bu_mime_traits_add(&asserted, "byte-order", "big-endian"));
    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "application/x-ply", &asserted, NULL, 0, NULL, filename));
    bu_mime_traits_free(&asserted);
    GCV_MIME_CHECK(bu_mime_traits_add(&asserted, "fixture.unestablished", "assertion"));
    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "application/x-ply", &asserted, NULL, 0, NULL, filename));
    GCV_MIME_CHECK(db_directory_size(context.dbip) == before && BU_STR_EQUAL(context.dbip->dbi_title, "profile marker") &&
	EQUAL(context.dbip->dbi_local2base, 10));
    bu_mime_traits_free(&asserted);
    GCV_MIME_CHECK(bu_mime_traits_add(&asserted, "encoding", "ascii"));
    GCV_MIME_CHECK(bu_mime_traits_add(&asserted, "version", "1.0"));
    GCV_MIME_CHECK(gcv_execute_variant(&context, reader, "application/x-ply", &asserted, NULL, 0, NULL, filename));
    GCV_MIME_CHECK(db_directory_size(context.dbip) == before + 2);
    stream = fopen(filename, "r+b");
    const b_off_t version_offset = (b_off_t)(strlen("ply\n") + padding + strlen("format ascii "));
    GCV_MIME_CHECK(stream && !bu_fseek(stream, version_offset, SEEK_SET));
    GCV_MIME_CHECK(fputs("1.1", stream) >= 0 && !fclose(stream));
    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "application/x-ply", &asserted, NULL, 0, NULL, filename));
    GCV_MIME_CHECK(db_directory_size(context.dbip) == before + 2);
    GCV_MIME_CHECK(bu_file_delete(filename));
    bu_mime_traits_free(&asserted);
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_ply_color_test(void)
{
    struct gcv_context context;
    struct gcv_opts options;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    const char *sample = "ply\nformat ascii 1.0\nelement vertex 4\nproperty float x\nproperty float y\nproperty float z\n"
	"element face 2\nproperty list uchar int vertex_indices\nproperty uchar red\nproperty uchar green\nproperty uchar blue\n"
	"end_header\n0 0 0\n1 0 0\n1 1 0\n0 1 0\n3 0 1 2 255 0 0\n3 0 2 3 0 255 0\n";
    char filename[MAXPATHLEN];
    gcv_context_init(&context);
    gcv_opts_default(&options);
    GCV_MIME_CHECK(db_update_ident(context.dbip, "color marker", 10) == 0);
    const size_t before = db_directory_size(context.dbip);
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(stream && fputs(sample, stream) >= 0 && !fclose(stream));
    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, "application/x-ply", "PLY Reader", NULL);
    GCV_MIME_CHECK(reader);
    options.reject_losses = GCV_LOSS_APPEARANCE;
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "application/x-ply", NULL, &options, 0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(result.loss_count == 1 && result.losses[0].kind == GCV_LOSS_APPEARANCE && result.losses[0].count == 2);
    GCV_MIME_CHECK(db_directory_size(context.dbip) == before && BU_STR_EQUAL(context.dbip->dbi_title, "color marker") &&
	EQUAL(context.dbip->dbi_local2base, 10));
    options.reject_losses = GCV_LOSS_GEOMETRY;
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "application/x-ply", NULL, &options, 0, NULL, filename, &result) == GCV_CONVERSION_PARTIAL);
    GCV_MIME_CHECK(result.loss_count == 1 && result.losses[0].kind == GCV_LOSS_APPEARANCE && result.losses[0].count == 2 &&
	db_directory_size(context.dbip) == before + 2);
    gcv_conversion_result_free(&result);
    GCV_MIME_CHECK(bu_file_delete(filename));
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_ply_output_colors_test(void)
{
    struct gcv_context context;
    struct gcv_opts options;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    const struct bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", "ascii");
    const struct bu_mime_traits request = {&encoding, 1};
    const unsigned char red[] = {255, 0, 0};
    const unsigned char green[] = {0, 255, 0};
    const point_t first_min = {0, 0, 0}, first_max = {1, 1, 1};
    const point_t second_min = {3, 0, 0}, second_max = {4, 1, 1};
    const char marker[] = "existing color output\n";
    char filename[MAXPATHLEN];
    char contents[sizeof(marker)];
    gcv_context_init(&context);
    gcv_opts_default(&options);
    struct rt_wdb *database = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
    GCV_MIME_CHECK(database && mk_rpp(database, "colored.s", first_min, first_max) == 0 &&
	mk_rpp(database, "plain.s", second_min, second_max) == 0 &&
	mk_region1(database, "colored.r", "colored.s", NULL, NULL, red) == 0 &&
	mk_comb1(database, "plain.r", "plain.s", 1) == 0);
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(stream && fputs(marker, stream) >= 0 && !fclose(stream));
    const struct gcv_filter *writer = gcv_select_variant(&context, GCV_FILTER_WRITE,
	"application/x-ply", &request, "PLY Writer", NULL);
    GCV_MIME_CHECK(writer);
    options.reject_losses = GCV_LOSS_APPEARANCE;
    GCV_MIME_CHECK(gcv_execute_result(&context, writer, "application/x-ply", &request, &options,
	0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(result.loss_count == 1 && result.losses[0].kind == GCV_LOSS_APPEARANCE &&
	result.losses[0].count == 1 && BU_STR_EQUAL(result.losses[0].feature, "ply.region-colors"));
    stream = fopen(filename, "rb");
    GCV_MIME_CHECK(stream && bu_fgets(contents, sizeof(contents), stream) &&
	BU_STR_EQUAL(contents, marker) && !fclose(stream));
    options.reject_losses = GCV_LOSS_GEOMETRY;
    GCV_MIME_CHECK(gcv_execute_result(&context, writer, "application/x-ply", &request, &options,
	0, NULL, filename, &result) == GCV_CONVERSION_PARTIAL);
    GCV_MIME_CHECK(result.loss_count == 1 && result.losses[0].kind == GCV_LOSS_APPEARANCE &&
	result.losses[0].count == 1);
    stream = fopen(filename, "rb");
    GCV_MIME_CHECK(stream && bu_fgets(contents, sizeof(contents), stream) &&
	BU_STR_EQUAL(contents, "ply\n") && !fclose(stream));
    GCV_MIME_CHECK(mk_region1(database, "plain.r", "plain.s", NULL, NULL, green) == 0);
    options.reject_losses = GCV_LOSS_APPEARANCE;
    GCV_MIME_CHECK(gcv_execute_result(&context, writer, "application/x-ply", &request, &options,
	0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE && !result.loss_count);
    gcv_conversion_result_free(&result);
    GCV_MIME_CHECK(bu_file_delete(filename));
    gcv_context_destroy(&context);
    return 0;
}


static int
mime_sdk_bindings_test(void)
{
#ifdef TEST_GCV_ASSETIMPORT
    struct gcv_context context;
    const char *types[] = {
	"model/vnd.autodesk.ase", "model/vnd.interquake.iqm",
	"model/vnd.idsoftware.md2", "model/vnd.idsoftware.md3", "image/x-lws",
	"model/vnd.idsoftware.mdc", "model/vnd.chumbalumsoft.ms3d", "model/vnd.izware.nendo",
	"model/vnd.nevercenter.sib", "model/vnd.caligari.cob", "application/terragen"
    };
    struct bu_mime_trait version[] = {BU_MIME_TRAIT_STRING("version", "4")};
    const struct bu_mime_traits request = {version, sizeof(version) / sizeof(version[0])};
    gcv_context_init(&context);
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
	GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, types[i], "Assetimport Reader", NULL));
    GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, "model/x3d+xml", "Assetimport Reader", NULL));
    GCV_MIME_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, "model/x3d+fastinfoset", "Assetimport Reader", NULL));
    GCV_MIME_CHECK(gcv_select_variant(&context, GCV_FILTER_WRITE,
	"model/vnd.pbrt.scene", &request, "Assetimport Writer", NULL));
    version[0].value = "3";
    GCV_MIME_CHECK(!gcv_select_variant(&context, GCV_FILTER_WRITE,
	"model/vnd.pbrt.scene", &request, "Assetimport Writer", NULL));
    struct bu_mime_trait order[] = {BU_MIME_TRAIT_STRING("byte-order", "big-endian")};
    const struct bu_mime_traits order_request = {order, sizeof(order) / sizeof(order[0])};
    GCV_MIME_CHECK(gcv_select_variant(&context, GCV_FILTER_READ, "model/vnd.izware.nendo", &order_request, "Assetimport Reader", NULL));
    GCV_MIME_CHECK(!gcv_select_variant(&context, GCV_FILTER_READ, "model/vnd.nevercenter.sib", &order_request, "Assetimport Reader", NULL));
    GCV_MIME_CHECK(!gcv_select_variant(&context, GCV_FILTER_READ, "model/vnd.caligari.cob", &order_request, "Assetimport Reader", NULL));
    order[0].value = "little-endian";
    GCV_MIME_CHECK(!gcv_select_variant(&context, GCV_FILTER_READ, "model/vnd.izware.nendo", &order_request, "Assetimport Reader", NULL));
    gcv_context_destroy(&context);
#endif
    return 0;
}


static int
mime_gdal_input_type_test(void)
{
#ifdef TEST_GCV_GDAL
    const unsigned char gif[] = "GIF89a\x01\0\x01\0\x80\0\0\0\0\0\xff\xff\xff"
	",\0\0\0\0\x01\0\x01\0\0\x02\x02\x44\x01\0;";
    const char *types[] = {"image/tiff", "image/png"};
    const char title[] = "Retain metadata when a GDAL type assertion fails";
    struct gcv_context context;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    char filename[MAXPATHLEN];
    FILE *file = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(file && fwrite(gif, 1, sizeof(gif) - 1, file) == sizeof(gif) - 1);
    GCV_MIME_CHECK(!fclose(file));
    gcv_context_init(&context);
    GCV_MIME_CHECK(!mk_id_units(wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM), title, "cm"));
    GCV_MIME_CHECK(gcv_select_filter(&context, GCV_FILTER_READ, "image/gif", "GDAL Reader", NULL));
    GCV_MIME_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, "application/terragen", "GDAL Reader", NULL));
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
	struct directory **objects = NULL;
	const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, types[i], "GDAL Reader", NULL);
	GCV_MIME_CHECK(reader);
	GCV_MIME_CHECK(gcv_execute_result(&context, reader, types[i], NULL, NULL, 0, NULL,
	    filename, &result) == GCV_CONVERSION_FAILED);
	GCV_MIME_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, title));
	GCV_MIME_CHECK(!db_ls(context.dbip, DB_LS_PRIM, NULL, &objects));
	bu_free(objects, "database primitives");
    }
    gcv_conversion_result_free(&result);
    gcv_context_destroy(&context);
    GCV_MIME_CHECK(bu_file_delete(filename));
#endif
    return 0;
}


static int
mime_sdk_input_type_test(void)
{
#ifdef TEST_GCV_ASSETIMPORT
    const char sample[] = "solid triangle\nfacet normal 0 0 1\nouter loop\n"
	"vertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid triangle\n";
    const char title[] = "Retain metadata when an input assertion fails";
    const char *wrong_options[] = {"--format", "obj"};
    const char *matching_options[] = {"--format", ".STL"};
    struct gcv_context context;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    char filename[MAXPATHLEN];
    FILE *file = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(file && fwrite(sample, 1, sizeof(sample) - 1, file) == sizeof(sample) - 1);
    GCV_MIME_CHECK(!fclose(file));
    gcv_context_init(&context);
    GCV_MIME_CHECK(!mk_id_units(wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM), title, "cm"));
    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ,
	"model/vnd.autodesk.ase", "Assetimport Reader", NULL);
    GCV_MIME_CHECK(reader);
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "model/vnd.autodesk.ase",
	NULL, NULL, 0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, title) && EQUAL(context.dbip->dbi_local2base, 10));
    GCV_MIME_CHECK(!db_lookup(context.dbip, "all.g", LOOKUP_QUIET));
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "model/stl",
	NULL, NULL, sizeof(wrong_options) / sizeof(wrong_options[0]), wrong_options,
	filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, title) && EQUAL(context.dbip->dbi_local2base, 10));
    GCV_MIME_CHECK(!db_lookup(context.dbip, "all.g", LOOKUP_QUIET));
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "model/stl",
	NULL, NULL, sizeof(matching_options) / sizeof(matching_options[0]), matching_options,
	filename, &result) == GCV_CONVERSION_COMPLETE);
    GCV_MIME_CHECK(db_lookup(context.dbip, "all.g", LOOKUP_QUIET));
    gcv_conversion_result_free(&result);
    gcv_context_destroy(&context);
    GCV_MIME_CHECK(bu_file_delete(filename));
#endif
    return 0;
}


static int
mime_observe_input(const struct bu_mime_probe_input *input, const char *UNUSED(type), struct bu_mime_traits *traits)
{
    const char *observations[] = {"header"};
    return !input->size || (bu_mime_traits_add(traits, "fixture.observed", "header") &&
	bu_mime_traits_add_set(traits, "fixture.observations", observations, sizeof(observations) / sizeof(observations[0])));
}

static int
mime_database_preserved(const struct gcv_context *context, size_t size, const char *title, fastf_t units)
{
    return db_directory_size(context->dbip) == size && BU_STR_EQUAL(context->dbip->dbi_title, title) &&
	EQUAL(context->dbip->dbi_local2base, units);
}


static int
mime_native_assertions_test(const char *sample_name, const char *mode_name)
{
    const struct {const char *name; const char *type; const char *handler; const char *text; const char *key; const char *value;} samples[] = {
	{"vtk", "application/vnd.kitware.vtk", "VTK Reader", "# vtk DataFile Version 3.0\nTriangle\nASCII\nDATASET POLYDATA\nPOINTS 3 float\n0 0 0 1 0 0 0 1 0\nPOLYGONS 1 4\n3 0 1 2\n", "vtk.dataset", "POLYDATA"},
	{"vrml1", "model/vrml", "VRML Reader", "#VRML V1.0 ascii\nSeparator { Cube { width 2 height 4 depth 6 } }\n", "version", "1.0"},
	{"vrml2", "model/vrml", "VRML Reader", "#VRML V2.0 utf8\nShape { geometry Box { size 2 4 6 } }\n", "version", "2.0"},
	{"stl", "model/stl", "STL Reader", "solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid triangle\n", "encoding", "ascii"},
	{"scad", "application/x-openscad", "OpenSCAD Reader", "cube([1,2,3]);\n", NULL, NULL},
	{"brlcad", "model/vnd.brlcad", "BRL-CAD Reader", NULL, "version", "5"},
	{"obj", "model/obj", "OBJ Reader", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", "encoding", "ascii"},
	{"ply", "application/x-ply", "PLY Reader", "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n", "encoding", "ascii"},
	{"asc", "application/vnd.brlcad.script", "ASC Reader", "title changed\nunits mm\nput triangle ell V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}\n", "asc.syntax", "tcl-commands"}
    };
    const struct gcv_conversion observer = {"Input observation fixture", GCV_FILTER_READ, NULL, NULL, mime_noop};
    const struct bu_mime_trait unknown = BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion");
    const struct bu_mime_traits invalid = {&unknown, 1}, empty = BU_MIME_TRAITS_INIT_ZERO;
    const char *observations[] = {"header"};
    const struct bu_mime_trait extra[] = {
	BU_MIME_TRAIT_STRING("fixture.observed", "header"),
	BU_MIME_TRAIT_SET("fixture.observations", observations, sizeof(observations) / sizeof(observations[0]))
    };
    const struct bu_mime_traits extra_request = {extra, sizeof(extra) / sizeof(extra[0])};
    const char *modes[] = {"none", "empty", "explicit", "observed", "observed-sdk"};
    for (size_t sample = 0; sample < sizeof(samples) / sizeof(samples[0]); ++sample)
	for (size_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
	    if (!BU_STR_EQUAL(sample_name, samples[sample].name) || !BU_STR_EQUAL(mode_name, modes[mode])) continue;
	    struct gcv_context context;
	    char filename[MAXPATHLEN];
	    gcv_context_init(&context);
	    const struct bu_mime_capability capability = BU_MIME_CAPABILITY(samples[sample].type, NULL, 0, 0);
	    const struct gcv_handler handler = {&observer, &capability, 1, BU_MIME_DEFAULT_PRIORITY, NULL, mime_observe_input};
	    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0, &handler, 1};
	    const int observing = BU_STR_EQUAL(mode_name, "observed") || BU_STR_EQUAL(mode_name, "observed-sdk");
	    const char *handler_name = BU_STR_EQUAL(mode_name, "observed-sdk") ? "Assetimport Reader" : samples[sample].handler;
	    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, samples[sample].type, handler_name, NULL);
	    GCV_MIME_CHECK(reader);
	    struct rt_wdb *database = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
	    const point_t center = {2, 3, 4};
	    const char *title = "Before native assertion";
	    GCV_MIME_CHECK(database && !mk_id_units(database, title, "cm") && !mk_sph(database, "kept.s", center, 1));
	    const fastf_t units = context.dbip->dbi_local2base;
	    const size_t before = db_directory_size(context.dbip);
	    FILE *file = bu_temp_file(filename, sizeof(filename));
	    GCV_MIME_CHECK(file);
	    if (BU_STR_EQUAL(sample_name, "brlcad")) {
		GCV_MIME_CHECK(!fclose(file));
		struct rt_wdb *source = wdb_fopen(filename);
		GCV_MIME_CHECK(source && !mk_id_units(source, "Source native database", "mm") &&
		    !mk_sph(source, "incoming.s", center, 1));
		wdb_close(source);
	    } else {
		/* Assertions must also work when recognition cannot see the header. */
		if ((BU_STR_EQUAL(sample_name, "asc") || BU_STR_EQUAL(sample_name, "stl")) && mode == 2) {
		    if (BU_STR_EQUAL(sample_name, "asc")) GCV_MIME_CHECK(fputc('#', file) != EOF);
		    for (size_t i = 0; i < BU_MIME_DEFAULT_SCAN_LIMIT; ++i) GCV_MIME_CHECK(fputc(' ', file) != EOF);
		    GCV_MIME_CHECK(fputc('\n', file) != EOF);
		}
		GCV_MIME_CHECK(fputs(samples[sample].text, file) >= 0 && !fclose(file));
	    }
	    if (observing) {
		GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, samples[sample].type, &extra_request, NULL, 0, NULL, filename));
		GCV_MIME_CHECK(mime_database_preserved(&context, before, title, units));
	    }
	    GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, NULL));
	    if (observing) {
		const struct bu_mime_trait contradiction = BU_MIME_TRAIT_STRING("fixture.observed", "different");
		const struct bu_mime_traits wrong = {&contradiction, 1};
		GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, samples[sample].type, &wrong, NULL, 0, NULL, filename));
		GCV_MIME_CHECK(mime_database_preserved(&context, before, title, units));
	    }
	    if ((!BU_STR_EQUAL(sample_name, "asc") && !BU_STR_EQUAL(sample_name, "stl")) || mode != 2) {
		struct bu_mime_result *detected = NULL;
		int contributed = 0;
		GCV_MIME_CHECK(gcv_detect_file(&context, filename, &detected, NULL) == BU_MIME_MATCH);
		for (size_t i = 0; detected && i < detected->count; ++i)
		    if (BU_STR_EQUAL(detected->candidates[i].name, samples[sample].type) &&
			BU_STR_EQUAL(bu_mime_traits_get(&detected->candidates[i].traits, "fixture.observed"), "header")) contributed = 1;
		bu_mime_result_free(detected);
		GCV_MIME_CHECK(contributed);
	    }
	    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, samples[sample].type, &invalid, NULL, 0, NULL, filename));
	    GCV_MIME_CHECK(mime_database_preserved(&context, before, title, units));
	    const char *complete[] = {"title", "units", "put"};
	    const struct bu_mime_trait facts[] = {
		BU_MIME_TRAIT_STRING(samples[sample].key, samples[sample].value),
		BU_MIME_TRAIT_SET("asc.commands", complete, sizeof(complete) / sizeof(complete[0]))
	    };
	    const int asc = BU_STR_EQUAL(sample_name, "asc");
	    if (asc) {
		const struct bu_mime_trait subset = BU_MIME_TRAIT_SET("asc.commands", complete, 2);
		const struct bu_mime_traits missing = {&subset, 1};
		GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, samples[sample].type, &missing, NULL, 0, NULL, filename));
		GCV_MIME_CHECK(mime_database_preserved(&context, before, title, units));
	    }
	    const struct bu_mime_traits valid = {facts, asc ? 2 : samples[sample].key ? 1 : 0};
	    const struct bu_mime_traits *request = observing ? &extra_request : mode == 0 ? NULL : mode == 1 ? &empty : &valid;
	    GCV_MIME_CHECK(gcv_execute_variant(&context, reader, samples[sample].type, request, NULL, 0, NULL, filename));
	    GCV_MIME_CHECK(db_directory_size(context.dbip) > before && db_lookup(context.dbip, "kept.s", LOOKUP_QUIET));
	    GCV_MIME_CHECK(bu_file_delete(filename));
	    gcv_context_destroy(&context);
	    return 0;
	}
    bu_log("Unknown native assertion fixture or request mode\n");
    return 1;
}

static int
mime_vrml_profiles_test(void)
{
    const struct { const char *text; const char *version; const char *encoding; } samples[] = {
	{"#VRML V1.0 ascii\nSeparator { Cube { width 2 height 4 depth 6 } }\n", "1.0", "ascii"},
	{"#VRML V2.0 utf8\nShape { geometry Box { size 2 4 6 } }\n", "2.0", "utf-8"}
    };
    const struct bu_mime_trait output_values[] = {
	BU_MIME_TRAIT_STRING("version", "2.0"), BU_MIME_TRAIT_STRING("encoding", "utf-8")
    };
    const struct bu_mime_traits output_traits = {output_values, 2};
    for (size_t sample = 0; sample < sizeof(samples) / sizeof(samples[0]); ++sample) {
	struct gcv_context context;
	struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
	const struct bu_mime_trait input_values[] = {
	    BU_MIME_TRAIT_STRING("version", samples[sample].version),
	    BU_MIME_TRAIT_STRING("encoding", samples[sample].encoding)
	};
	const struct bu_mime_traits input_traits = {input_values, 2};
	char filename[MAXPATHLEN];
	gcv_context_init(&context);
	const struct gcv_filter *writer = gcv_select_variant(&context, GCV_FILTER_WRITE,
	    "model/vrml", &output_traits, "VRML Writer", NULL);
	GCV_MIME_CHECK(writer);
	const struct gcv_filter *reader = gcv_select_variant(&context, GCV_FILTER_READ,
	    "model/vrml", &input_traits, "VRML Reader", NULL);
	GCV_MIME_CHECK(reader && gcv_select_filter(&context, GCV_FILTER_READ, "model/vrml", "VRML Reader", NULL));
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	GCV_MIME_CHECK(stream && fputs(samples[sample].text, stream) >= 0 && !fclose(stream));
	GCV_MIME_CHECK(gcv_execute_result(&context, reader, "model/vrml", &input_traits,
	    NULL, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
	struct directory **objects = NULL;
	GCV_MIME_CHECK(db_ls(context.dbip, DB_LS_PRIM, NULL, &objects) == 1);
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	GCV_MIME_CHECK(rt_db_get_internal(&internal, objects[0], context.dbip, NULL) == ID_ARB8);
	const struct rt_arb_internal *box = (const struct rt_arb_internal *)internal.idb_ptr;
	for (size_t corner = 0; corner < 8; ++corner)
	    GCV_MIME_CHECK(EQUAL(fabs(box->pt[corner][X]), 1) &&
		EQUAL(fabs(box->pt[corner][Y]), 2) && EQUAL(fabs(box->pt[corner][Z]), 3));
	rt_db_free_internal(&internal);
	bu_free(objects, "VRML primitives");
	GCV_MIME_CHECK(gcv_execute_result(&context, writer, "model/vrml", &output_traits,
	    NULL, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
	struct bu_mime_result *observed = NULL;
	GCV_MIME_CHECK(gcv_detect_file(&context, filename, &observed, NULL) == BU_MIME_MATCH);
	GCV_MIME_CHECK(observed->count && BU_STR_EQUAL(observed->candidates[0].name, "model/vrml"));
	GCV_MIME_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&observed->candidates[0].traits, "version"), "2.0"));
	bu_mime_result_free(observed);
	gcv_conversion_result_free(&result);
	GCV_MIME_CHECK(bu_file_delete(filename));
	gcv_context_destroy(&context);
    }
    return 0;
}


static int
mime_vtk_context_test(void)
{
    struct gcv_context context;
    const char sample[] = "# vtk DataFile Version 3.0\nquad\nASCII\nDATASET POLYDATA\n"
	"POINTS 4 float\n0 0 0 1 0 0 1 1 0 0 1 0\nPOLYGONS 1 5\n4 0 1 2 3\n";
    char filename[MAXPATHLEN];
    gcv_context_init(&context);
    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ,
	"application/vnd.kitware.vtk", "VTK Reader", NULL);
    GCV_MIME_CHECK(reader);
    const char *families[] = {"POLYDATA", "UNSTRUCTURED_GRID", "STRUCTURED_POINTS", "RECTILINEAR_GRID", "STRUCTURED_GRID"};
    struct gcv_handler_info *info = NULL;
    size_t count = 0;
    GCV_MIME_CHECK(gcv_list_handlers(&context, "application/vnd.kitware.vtk", &info, &count));
    for (size_t family = 0; family < sizeof(families) / sizeof(families[0]); ++family) {
	int found = 0;
	for (size_t entry = 0; entry < count; ++entry) {
	    const char *kind = bu_mime_traits_get(&info[entry].traits, "vtk.dataset");
	    if (BU_STR_EQUAL(info[entry].name, "VTK Reader") && kind && BU_STR_EQUAL(kind, families[family]))
		found = 1;
	}
	GCV_MIME_CHECK(found);
    }
    gcv_handler_list_free(info, count);
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(stream && fputs(sample, stream) >= 0 && !fclose(stream));
    struct bu_mime_trait values[] = {
	BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("vtk.dataset", "UNSTRUCTURED_GRID")
    };
    const struct bu_mime_traits request = {values, sizeof(values) / sizeof(values[0])};
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    GCV_MIME_CHECK(db_update_ident(context.dbip, "VTK profile", 1) == 0);
    const size_t before = db_directory_size(context.dbip);
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "application/vnd.kitware.vtk", &request,
	NULL, 0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(db_directory_size(context.dbip) == before && BU_STR_EQUAL(context.dbip->dbi_title, "VTK profile"));
    values[1].value = "POLYDATA";
    GCV_MIME_CHECK(gcv_execute_result(&context, reader, "application/vnd.kitware.vtk", &request,
	NULL, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
    gcv_conversion_result_free(&result);
    struct directory **objects = NULL;
    GCV_MIME_CHECK(db_ls(context.dbip, DB_LS_PRIM, NULL, &objects) == 1);
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    GCV_MIME_CHECK(rt_db_get_internal(&internal, objects[0], context.dbip, NULL) == ID_BOT);
    const struct rt_bot_internal *bot = (const struct rt_bot_internal *)internal.idb_ptr;
    GCV_MIME_CHECK(bot->num_vertices == 4 && bot->num_faces == 2);
    rt_db_free_internal(&internal);
    bu_free(objects, "VTK primitives");
    GCV_MIME_CHECK(bu_file_delete(filename));
    gcv_context_destroy(&context);
    return 0;
}


static int
mime_ply_context_test(void)
{
    struct gcv_context contexts[2];
    const char *scales[] = {"1", "2", "3"};
    const char *sample = "ply\nformat ascii 1.0\nelement vertex 3\nproperty float z\nproperty float x\nproperty float y\n"
	"element face 1\nproperty list uchar int vertex_indices\nend_header\n3 1 2\n0 0 0\n1 1 1\n3 0 1 2\n";
    gcv_context_init(&contexts[0]);
    gcv_context_init(&contexts[1]);
    for (size_t pass = 0; pass < sizeof(scales) / sizeof(scales[0]); ++pass) {
	struct gcv_context *context = &contexts[pass % 2];
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	const char *arguments[] = {"--scale_factor", scales[pass]};
	const struct gcv_filter *reader;
	GCV_MIME_CHECK(stream && fputs(sample, stream) >= 0 && !fclose(stream));
	reader = gcv_resolve_filter(context, GCV_FILTER_READ, filename, "application/x-ply", "PLY Reader", NULL, NULL);
	GCV_MIME_CHECK(reader && gcv_execute_format(context, reader, "application/x-ply", NULL, 2, arguments, filename));
	struct directory **objects = NULL;
	size_t count = db_ls(context->dbip, DB_LS_PRIM, NULL, &objects);
	int found = 0;
	for (size_t i = 0; i < count; ++i) {
	    struct rt_db_internal internal;
	    RT_DB_INTERNAL_INIT(&internal);
	    GCV_MIME_CHECK(rt_db_get_internal(&internal, objects[i], context->dbip, NULL) == ID_BOT);
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)internal.idb_ptr;
	    double scale = (double)(pass + 1);
	    if (bot->num_vertices == 3 && EQUAL(bot->vertices[0], scale) &&
		EQUAL(bot->vertices[1], 2 * scale) && EQUAL(bot->vertices[2], 3 * scale)) {
		GCV_MIME_CHECK(bot->num_faces == 1 && bot->faces[0] == 0 && bot->faces[1] == 1 && bot->faces[2] == 2);
		found = 1;
	    }
	    rt_db_free_internal(&internal);
	}
	bu_free(objects, "PLY primitives");
	GCV_MIME_CHECK(found && bu_file_delete(filename));
    }
    gcv_context_destroy(&contexts[1]);
    gcv_context_destroy(&contexts[0]);
    return 0;
}

static int
mime_gltf_scene_test(void)
{
#ifdef TEST_GCV_GLTF
    const char *sample = "{\"asset\":{\"version\":\"2.0\"},"
	"\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,"
	"AACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAQAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABA\"}],"
	"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
	"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,1]},"
	"{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[2,2,2]}],"
	"\"meshes\":[{\"name\":\"duplicate mesh\",\"primitives\":[{\"attributes\":{\"POSITION\":0}}]},"
	"{\"name\":\"duplicate mesh\",\"primitives\":[{\"attributes\":{\"POSITION\":1}}]}],"
	"\"nodes\":[{\"mesh\":1,\"children\":[1,2],\"translation\":[10,20,30],"
	"\"rotation\":[0,0,0.7071067811865476,0.7071067811865476],\"scale\":[2,3,4]},"
	"{\"mesh\":0,\"translation\":[1,2,3],\"rotation\":[1,0,0,0],\"scale\":[-1,2,3]},"
	"{\"mesh\":0,\"matrix\":[-1,0,0,0,0,-2,0,0,0,0,-3,0,1,2,3,1]}],"
	"\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
    /* Expected world coordinates in meters, computed independently of
     * the database matrices. The two child instances are equivalent. */
    const fastf_t expected[2][9] = {
	{10, 24, 30, 4, 20, 30, 10, 20, 38},
	{4, 20, 42, 10, 22, 42, 4, 22, 30}
    };
    const size_t import_count = 3;
    const fastf_t millimeters_per_meter = 1000;
    struct gcv_context contexts[2];
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(stream && fputs(sample, stream) >= 0 && !fclose(stream));
    gcv_context_init(&contexts[0]);
    gcv_context_init(&contexts[1]);
    for (size_t pass = 0; pass < import_count; ++pass) {
	struct gcv_context *context = &contexts[pass % 2];
	struct gcv_opts options;
	const struct gcv_filter *reader = gcv_resolve_filter(context, GCV_FILTER_READ,
	    filename, "model/gltf+json", "GLTF Reader", NULL, NULL);
	gcv_opts_default(&options);
	options.scale_factor = (fastf_t)(pass + 1);
	struct gcv_conversion_result conversion = GCV_CONVERSION_RESULT_INIT_ZERO;
	GCV_MIME_CHECK(reader && gcv_execute_result(context, reader, "model/gltf+json", NULL, &options, 0, NULL, filename, &conversion) == GCV_CONVERSION_COMPLETE);
	GCV_MIME_CHECK(!conversion.loss_count);
	gcv_conversion_result_free(&conversion);
	db_update_nref(context->dbip);
	struct directory **primitives = NULL;
	size_t copies = pass / 2 + 1;
	GCV_MIME_CHECK(db_ls(context->dbip, DB_LS_PRIM, NULL, &primitives) == 2 * copies);
	bu_free(primitives, "glTF primitives");
	struct bu_ptbl paths = BU_PTBL_INIT_ZERO;
	GCV_MIME_CHECK(db_search(&paths, DB_SEARCH_TREE, "-type bot ! -below -type region",
	    0, NULL, context->dbip, NULL, NULL, NULL) == 0);
	db_search_free(&paths);
	GCV_MIME_CHECK(db_search(&paths, DB_SEARCH_TREE, "-type bot",
	    0, NULL, context->dbip, NULL, NULL, NULL) == (int)(3 * copies));
	size_t matched[3][2] = {{0}};
	for (size_t i = 0; i < BU_PTBL_LEN(&paths); ++i) {
	    struct db_full_path *path = (struct db_full_path *)BU_PTBL_GET(&paths, i);
	    mat_t matrix;
	    struct rt_db_internal internal;
	    RT_DB_INTERNAL_INIT(&internal);
	    GCV_MIME_CHECK(db_path_to_mat(context->dbip, path, matrix, (int)path->fp_len));
	    GCV_MIME_CHECK(rt_db_get_internal(&internal, DB_FULL_PATH_CUR_DIR(path), context->dbip, NULL) == ID_BOT);
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)internal.idb_ptr;
	    GCV_MIME_CHECK(bot->num_vertices == 3 && bot->num_faces == 1 && bot->mode == RT_BOT_SURFACE);
	    point_t vertices[3];
	    for (size_t vertex = 0; vertex < 3; ++vertex)
		MAT4X3PNT(vertices[vertex], matrix, &bot->vertices[vertex * 3]);
	    int found = 0;
	    for (size_t previous = pass % 2; previous <= pass; previous += 2) {
		fastf_t scale = millimeters_per_meter * (fastf_t)(previous + 1);
		for (size_t kind = 0; kind < 2; ++kind) {
		    int matches = 1;
		    for (size_t vertex = 0; vertex < 3; ++vertex)
			for (size_t axis = 0; axis < 3; ++axis)
			    if (!NEAR_EQUAL(vertices[vertex][axis], expected[kind][vertex * 3 + axis] * scale,
				    options.calculational_tolerance.dist))
				matches = 0;
		    if (matches) {
			++matched[previous][kind];
			found = 1;
		    }
		}
	    }
	    if (!found) {
		for (size_t vertex = 0; vertex < 3; ++vertex)
		    bu_log("Unexpected glTF vertex: %g %g %g\n", V3ARGS(vertices[vertex]));
	    }
	    GCV_MIME_CHECK(found);
	    rt_db_free_internal(&internal);
	}
	db_search_free(&paths);
	for (size_t previous = pass % 2; previous <= pass; previous += 2)
	    GCV_MIME_CHECK(matched[previous][0] == 1 && matched[previous][1] == 2);
    }
    gcv_context_destroy(&contexts[1]);
    gcv_context_destroy(&contexts[0]);
    GCV_MIME_CHECK(bu_file_delete(filename));
#endif
    return 0;
}

static const struct gcv_filter *
result_fixture_filter(struct gcv_context *context, const char *name)
{
    const struct bu_ptbl *filters = gcv_list_filters(context);
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	const struct gcv_filter *filter = (const struct gcv_filter *)BU_PTBL_GET(filters, i);
	if (BU_STR_EQUAL(filter->name, name)) return filter;
    }
    return NULL;
}

static int
result_loss_fixture(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *UNUSED(target))
{
    char feature[] = "fixture.omitted";
    char message[] = "Two objects were omitted";
    int permitted = gcv_report_loss(context, GCV_LOSS_GEOMETRY, feature, 2, message);
    feature[0] = 'x';
    message[0] = 'x';
    permitted &= gcv_report_loss(context, GCV_LOSS_APPEARANCE, "fixture.materials", 1, "Material was omitted");
    return permitted;
}

static int
result_ignored_rejection(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *UNUSED(target))
{
    (void)gcv_report_loss(context, GCV_LOSS_METADATA, "fixture.metadata", 1, "Metadata was omitted");
    return 1;
}

static int
result_failed_fixture(struct gcv_context *context, const struct gcv_opts *options,
    const void *data, const char *target)
{
    (void)result_loss_fixture(context, options, data, target);
    return 0;
}

static int
result_nested_fixture(struct gcv_context *context, const struct gcv_opts *options,
    const void *UNUSED(data), const char *UNUSED(target))
{
    struct gcv_conversion_result nested = GCV_CONVERSION_RESULT_INIT_ZERO;
    const struct gcv_filter *child = result_fixture_filter(context, "Result complete fixture");
    int permitted = gcv_report_loss(context, GCV_LOSS_BEHAVIOR, "fixture.before", 1, "Before nested conversion");
    enum gcv_conversion_status status = gcv_execute_result(context, child, NULL, NULL, options, 0, NULL, NULL, &nested);
    permitted &= status == GCV_CONVERSION_COMPLETE && !nested.loss_count;
    gcv_conversion_result_free(&nested);
    permitted &= gcv_report_loss(context, GCV_LOSS_BEHAVIOR, "fixture.after", 1, "After nested conversion");
    return permitted;
}

static int
mime_result_test(void)
{
    struct gcv_context context;
    struct gcv_context other;
    struct gcv_opts options;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const struct gcv_conversion conversions[] = {
	{"Result complete fixture", GCV_FILTER_FILTER, NULL, NULL, mime_noop},
	{"Result loss fixture", GCV_FILTER_FILTER, NULL, NULL, result_loss_fixture},
	{"Result ignored rejection", GCV_FILTER_FILTER, NULL, NULL, result_ignored_rejection},
	{"Result failed fixture", GCV_FILTER_FILTER, NULL, NULL, result_failed_fixture},
	{"Result nested fixture", GCV_FILTER_FILTER, NULL, NULL, result_nested_fixture},
	{"Result exception fixture", GCV_FILTER_FILTER, NULL, NULL, gcv_result_exception},
	{"Result unknown exception", GCV_FILTER_FILTER, NULL, NULL, gcv_result_unknown_exception}
    };
    struct gcv_handler handlers[sizeof(conversions) / sizeof(conversions[0])] = {{0}};
    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0,
	handlers, sizeof(handlers) / sizeof(handlers[0])};
    gcv_context_init(&context);
    gcv_context_init(&other);
    gcv_opts_default(&options);
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); ++i)
	handlers[i].conversion = &conversions[i];
    GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, &messages));
    const struct gcv_filter *loss = result_fixture_filter(&context, "Result loss fixture");
    const struct gcv_filter *complete = result_fixture_filter(&context, "Result complete fixture");
    const struct gcv_filter *ignored = result_fixture_filter(&context, "Result ignored rejection");
    const struct gcv_filter *failed = result_fixture_filter(&context, "Result failed fixture");
    const struct gcv_filter *nested = result_fixture_filter(&context, "Result nested fixture");
    const struct gcv_filter *exception = result_fixture_filter(&context, "Result exception fixture");
    const struct gcv_filter *unknown_exception = result_fixture_filter(&context, "Result unknown exception");
    GCV_MIME_CHECK(loss && complete && ignored && failed && nested && exception && unknown_exception);
    GCV_MIME_CHECK(!gcv_report_loss(&context, GCV_LOSS_GEOMETRY, "outside", 1, "Outside execution"));
    GCV_MIME_CHECK(gcv_execute_result(&context, loss, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_PARTIAL);
    GCV_MIME_CHECK(result.status == GCV_CONVERSION_PARTIAL && result.loss_count == 2);
    GCV_MIME_CHECK(result.losses[0].kind == GCV_LOSS_GEOMETRY && result.losses[0].count == 2);
    GCV_MIME_CHECK(BU_STR_EQUAL(result.losses[0].feature, "fixture.omitted") && BU_STR_EQUAL(result.losses[0].message, "Two objects were omitted"));
    gcv_conversion_result_describe(&result, &messages);
    GCV_MIME_CHECK(strstr(bu_vls_cstr(&messages), "Partial conversion") && strstr(bu_vls_cstr(&messages), "geometry: fixture.omitted (2)"));
    GCV_MIME_CHECK(gcv_execute(&context, loss, &options, 0, NULL, NULL));
    options.reject_losses = GCV_LOSS_BEHAVIOR;
    GCV_MIME_CHECK(gcv_execute_result(&context, loss, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_PARTIAL);
    options.reject_losses = GCV_LOSS_GEOMETRY;
    GCV_MIME_CHECK(gcv_execute_result(&context, loss, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED && result.loss_count == 2);
    options.reject_losses = GCV_LOSS_METADATA;
    GCV_MIME_CHECK(gcv_execute_result(&context, ignored, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED && result.loss_count == 1);
    GCV_MIME_CHECK(strstr(bu_vls_cstr(&result.messages), "loss policy"));
    options.reject_losses = 0;
    GCV_MIME_CHECK(gcv_execute_result(&context, failed, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED && result.loss_count == 2);
    GCV_MIME_CHECK(gcv_execute_result(&context, nested, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_PARTIAL && result.loss_count == 2);
    GCV_MIME_CHECK(BU_STR_EQUAL(result.losses[0].feature, "fixture.before") && BU_STR_EQUAL(result.losses[1].feature, "fixture.after"));
    GCV_MIME_CHECK(gcv_execute_result(&context, complete, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_COMPLETE);
    GCV_MIME_CHECK(!result.losses && !result.loss_count && !bu_vls_strlen(&result.messages));
    GCV_MIME_CHECK(gcv_execute_result(&context, exception, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(result.loss_count == 1 && strstr(bu_vls_cstr(&result.messages), "Conversion fixture exception"));
    GCV_MIME_CHECK(!context.dbip->dbi_read_only);
    GCV_MIME_CHECK(gcv_execute_result(&context, unknown_exception, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(!result.loss_count && strstr(bu_vls_cstr(&result.messages), "unknown exception"));
    GCV_MIME_CHECK(!gcv_report_loss(&context, GCV_LOSS_GEOMETRY, "outside", 1, "Outside execution"));
    GCV_MIME_CHECK(gcv_execute_result(&context, complete, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_COMPLETE);
    GCV_MIME_CHECK(!result.loss_count && !bu_vls_strlen(&result.messages));
    const char *unknown[] = {"--unknown"};
    GCV_MIME_CHECK(gcv_execute_result(&context, loss, NULL, NULL, &options, 1, unknown, NULL, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(!result.loss_count && strstr(bu_vls_cstr(&result.messages), "Unknown arguments"));
    GCV_MIME_CHECK(gcv_execute_result(&other, loss, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(strstr(bu_vls_cstr(&result.messages), "does not belong"));
    options.reject_losses = GCV_LOSS_METADATA << 1;
    GCV_MIME_CHECK(gcv_execute_result(&context, complete, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_FAILED);
    options.reject_losses = 0;
    const char *category[] = {"geometry"};
    GCV_MIME_CHECK(gcv_opt_loss_policy(&messages, 1, category, &options.reject_losses) == 1 && options.reject_losses == GCV_LOSS_GEOMETRY);
    category[0] = "appearance";
    GCV_MIME_CHECK(gcv_opt_loss_policy(&messages, 1, category, &options.reject_losses) == 1 && options.reject_losses == (GCV_LOSS_GEOMETRY | GCV_LOSS_APPEARANCE));
    category[0] = "unknown";
    GCV_MIME_CHECK(gcv_opt_loss_policy(&messages, 1, category, &options.reject_losses) == -1);
    options.reject_losses = 0;
    GCV_MIME_CHECK(gcv_execute_result(&context, loss, NULL, NULL, &options, 0, NULL, NULL, &result) == GCV_CONVERSION_PARTIAL);
    gcv_context_destroy(&other);
    gcv_context_destroy(&context);
    GCV_MIME_CHECK(BU_STR_EQUAL(result.losses[0].feature, "fixture.omitted") && result.losses[1].count == 1);
    gcv_conversion_result_free(&result);
    gcv_conversion_result_free(&result);
    bu_vls_free(&messages);
    return 0;
}

static int
mime_unused_path(char *filename, size_t size)
{
    FILE *stream = bu_temp_file(filename, size);
    if (!stream) return 0;
    int status = !fclose(stream);
    return bu_file_delete(filename) && status;
}

static int
mime_read_text_file(struct bu_vls *contents, const char *filename, int *ascii)
{
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;
    bu_vls_trunc(contents, 0);
    *ascii = 1;
    for (int c = fgetc(file); c != EOF; c = fgetc(file)) {
	if (c > 0x7f) *ascii = 0;
	bu_vls_putc(contents, c);
    }
    int valid = !ferror(file);
    if (fclose(file)) valid = 0;
    return valid;
}

static int
mime_stl_output_test(void)
{
    struct gcv_context context;
    struct gcv_opts options;
    struct bu_vls contents = BU_VLS_INIT_ZERO;
    const point_t minimum = {0, 0, 0}, maximum = {1, 1, 1};
    const char *unicode_region = "caf\xc3\xa9.r", *unicode_leaf = "caf\xc3\xa9.s", *unicode_parent = "groupe.\xc3\xa9";
    const struct bu_mime_trait ascii_value = BU_MIME_TRAIT_STRING("encoding", "ascii");
    const struct bu_mime_traits ascii_request = {&ascii_value, 1};
    const struct bu_mime_trait text_value = BU_MIME_TRAIT_STRING("stl.serialization", "text");
    const struct bu_mime_traits text_request = {&text_value, 1};
    gcv_context_init(&context);
    gcv_opts_default(&options);
    struct rt_wdb *database = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
    GCV_MIME_CHECK(database && !mk_id_units(database, "STL output profiles", "mm"));
    GCV_MIME_CHECK(!mk_rpp(database, "plain.s", minimum, maximum) && !mk_rpp(database, unicode_leaf, minimum, maximum));
    GCV_MIME_CHECK(!mk_comb1(database, "plain.r", "plain.s", 1) && !mk_comb1(database, unicode_region, "plain.s", 1));
    GCV_MIME_CHECK(!mk_comb1(database, "leaf-name.r", unicode_leaf, 1) && !mk_comb1(database, unicode_parent, "plain.r", 0));
    const size_t source_objects = db_directory_size(context.dbip);
    const struct gcv_filter *writer = gcv_select_filter(&context, GCV_FILTER_WRITE, "model/stl", "STL Writer", NULL);
    GCV_MIME_CHECK(writer);
    char filename[MAXPATHLEN];
    GCV_MIME_CHECK(mime_unused_path(filename, sizeof(filename)));
    struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    GCV_MIME_CHECK(gcv_resolve_variant(&context, GCV_FILTER_WRITE, filename, "model/stl", &text_request,
	"STL Writer", NULL, &resolved, NULL) == writer);
    GCV_MIME_CHECK(!bu_mime_traits_get(&resolved, "encoding") &&
	BU_STR_EQUAL(bu_mime_traits_get(&resolved, "stl.serialization"), "text"));
    bu_mime_traits_free(&resolved);
    const char *selected[] = {NULL};
    options.num_objects = 1;
    options.object_names = selected;
    const char *good[] = {"plain.r", "leaf-name.r", "plain.s"};
    for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); ++i) {
	selected[0] = good[i];
	GCV_MIME_CHECK(gcv_execute_variant(&context, writer, "model/stl", &ascii_request, &options, 0, NULL, filename));
	int ascii = 0;
	GCV_MIME_CHECK(mime_read_text_file(&contents, filename, &ascii) && ascii && strstr(bu_vls_cstr(&contents), "facet normal"));
	struct bu_vls label = BU_VLS_INIT_ZERO;
	bu_vls_printf(&label, "solid %s\n", good[i]);
	GCV_MIME_CHECK(strstr(bu_vls_cstr(&contents), bu_vls_cstr(&label)));
	bu_vls_free(&label);
	GCV_MIME_CHECK(bu_file_delete(filename));
    }
    const char marker[] = "Keep this destination on profile rejection.\n";
    const char *bad[] = {unicode_region, unicode_parent, "groupe.\xc3\xa9/plain.r"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
	selected[0] = bad[i];
	FILE *file = fopen(filename, "wb");
	GCV_MIME_CHECK(file && fwrite(marker, 1, sizeof(marker) - 1, file) == sizeof(marker) - 1 && !fclose(file));
	GCV_MIME_CHECK(!gcv_execute_variant(&context, writer, "model/stl", &ascii_request, &options, 0, NULL, filename));
	int ascii = 0;
	GCV_MIME_CHECK(mime_read_text_file(&contents, filename, &ascii) && bu_vls_strlen(&contents) == sizeof(marker) - 1 &&
	    !memcmp(bu_vls_cstr(&contents), marker, sizeof(marker) - 1));
	GCV_MIME_CHECK(bu_file_delete(filename));
	GCV_MIME_CHECK(!gcv_execute_variant(&context, writer, "model/stl", &ascii_request, &options, 0, NULL, filename) && !bu_file_exists(filename, NULL));
    }
    selected[0] = unicode_region;
    for (size_t mode = 0; mode < 2; ++mode) {
	GCV_MIME_CHECK(gcv_execute_variant(&context, writer, "model/stl", mode ? &text_request : NULL, &options, 0, NULL, filename));
	int ascii = 1;
	GCV_MIME_CHECK(mime_read_text_file(&contents, filename, &ascii) && !ascii && strstr(bu_vls_cstr(&contents), unicode_region));
	GCV_MIME_CHECK(bu_file_delete(filename));
    }
    const char *binary_options[] = {"--binary"};
    const struct bu_mime_trait binary_value = BU_MIME_TRAIT_STRING("encoding", "binary");
    const struct bu_mime_traits binary_request = {&binary_value, 1};
    for (size_t mode = 0; mode < 2; ++mode) {
	GCV_MIME_CHECK(gcv_execute_variant(&context, writer, "model/stl", mode ? &binary_request : NULL, &options,
	    mode ? 0 : 1, mode ? NULL : binary_options, filename));
	enum {header_size = 84, count_at = 80, record_size = 50, box_triangles = 12};
	unsigned char header[header_size];
	FILE *file = fopen(filename, "rb");
	GCV_MIME_CHECK(file && fread(header, 1, sizeof(header), file) == sizeof(header));
	const uint32_t count = (uint32_t)header[count_at] | ((uint32_t)header[count_at + 1] << 8) |
	    ((uint32_t)header[count_at + 2] << 16) | ((uint32_t)header[count_at + 3] << 24);
	GCV_MIME_CHECK(count == box_triangles && !bu_fseek(file, 0, SEEK_END) && bu_ftell(file) == (b_off_t)(sizeof(header) + count * record_size));
	GCV_MIME_CHECK(!fclose(file) && bu_file_delete(filename));
    }
    selected[0] = "plain.r";
    GCV_MIME_CHECK(!gcv_execute_variant(&context, writer, "model/stl", &text_request, &options, 1, binary_options, filename));
    GCV_MIME_CHECK(!bu_file_exists(filename, NULL) && !context.dbip->dbi_read_only);
    GCV_MIME_CHECK(db_directory_size(context.dbip) == source_objects &&
	BU_STR_EQUAL(context.dbip->dbi_title, "STL output profiles") && EQUAL(context.dbip->dbi_local2base, 1));
    bu_vls_free(&contents);
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_native_sphere(struct db_i *database)
{
    struct directory *entry = db_lookup(database, "sphere", LOOKUP_QUIET);
    struct rt_db_internal object;
    RT_DB_INTERNAL_INIT(&object);
    GCV_MIME_CHECK(entry && rt_db_get_internal(&object, entry, database, NULL) == ID_ELL);
    struct rt_ell_internal *sphere = (struct rt_ell_internal *)object.idb_ptr;
    GCV_MIME_CHECK(EQUAL(sphere->v[0], 1) && EQUAL(sphere->v[1], 2) && EQUAL(sphere->v[2], 3));
    GCV_MIME_CHECK(EQUAL(MAGNITUDE(sphere->a), 4));
    rt_db_free_internal(&object);
    return 0;
}

static int
mime_database_test(void)
{
    struct gcv_context source;
    struct gcv_opts options;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    const point_t center = {1, 2, 3};
    gcv_context_init(&source);
    gcv_opts_default(&options);
    options.reject_losses = GCV_LOSS_ALL;
    struct rt_wdb *writer = wdb_dbopen(source.dbip, RT_WDB_TYPE_DB_INMEM);
    GCV_MIME_CHECK(writer && mk_id_units(writer, "Database version test", "cm") >= 0 && mk_sph(writer, "sphere", center, 4) >= 0);
    GCV_MIME_CHECK(EQUAL(source.dbip->dbi_local2base, 10));
    source.dbip->dbi_read_only = 1;
    GCV_MIME_CHECK(db_update_ident(source.dbip, "rejected title", 1000) < 0);
    source.dbip->dbi_read_only = 0;
    GCV_MIME_CHECK(db_update_ident(source.dbip, "invalid scale", 0) < 0);
    GCV_MIME_CHECK(BU_STR_EQUAL(source.dbip->dbi_title, "Database version test") && EQUAL(source.dbip->dbi_local2base, 10));
    const struct gcv_filter *output = gcv_select_filter(&source, GCV_FILTER_WRITE, "model/vnd.brlcad", "BRL-CAD Writer", NULL);
    GCV_MIME_CHECK(output);
    const char *versions[] = {"4", "5"};
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
	struct bu_mime_trait version = BU_MIME_TRAIT_STRING("version", versions[i]);
	struct bu_mime_traits request = {&version, 1};
	char filename[MAXPATHLEN];
	GCV_MIME_CHECK(mime_unused_path(filename, sizeof(filename)));
	GCV_MIME_CHECK(gcv_execute_result(&source, output, "model/vnd.brlcad", &request, &options, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
	GCV_MIME_CHECK(db_filetype(filename) == (i ? 5 : 4));
	struct gcv_context imported;
	gcv_context_init(&imported);
	const struct gcv_filter *input = gcv_select_filter(&imported, GCV_FILTER_READ, "model/vnd.brlcad", "BRL-CAD Reader", NULL);
	GCV_MIME_CHECK(input && gcv_execute_result(&imported, input, "model/vnd.brlcad", &request, &options, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
	GCV_MIME_CHECK(db_version(imported.dbip) == 5 && !imported.dbip->dbi_read_only && !mime_native_sphere(imported.dbip));
	GCV_MIME_CHECK(BU_STR_EQUAL(imported.dbip->dbi_title, "Database version test") && EQUAL(imported.dbip->dbi_local2base, 10));
	struct rt_wdb *import_writer = wdb_dbopen(imported.dbip, RT_WDB_TYPE_DB_INMEM);
	GCV_MIME_CHECK(mk_sph(import_writer, "added", center, 1) >= 0);
	struct db_i *disk = db_open(filename, DB_OPEN_READONLY);
	GCV_MIME_CHECK(disk && !db_dirbuild(disk) && !db_lookup(disk, "added", LOOKUP_QUIET) && !mime_native_sphere(disk));
	GCV_MIME_CHECK(db_update_ident(disk, "rejected title", 1000) < 0);
	GCV_MIME_CHECK(BU_STR_EQUAL(disk->dbi_title, "Database version test") && EQUAL(disk->dbi_local2base, 10));
	db_close(disk);
	gcv_context_destroy(&imported);
	version.value = i ? "4" : "5";
	GCV_MIME_CHECK(gcv_execute_result(&source, output, "model/vnd.brlcad", &request, &options, 0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
	GCV_MIME_CHECK(db_filetype(filename) == (i ? 5 : 4));
	GCV_MIME_CHECK(bu_file_delete(filename));
    }
    GCV_MIME_CHECK(db5_update_attribute("sphere", "custom", "retained only in V5", source.dbip) == 0);
    struct bu_mime_trait version = BU_MIME_TRAIT_STRING("version", "4");
    struct bu_mime_traits request = {&version, 1};
    char filename[MAXPATHLEN];
    GCV_MIME_CHECK(mime_unused_path(filename, sizeof(filename)));
    GCV_MIME_CHECK(gcv_execute_result(&source, output, "model/vnd.brlcad", &request, &options, 0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(!bu_file_exists(filename, NULL) && result.loss_count == 1 && result.losses[0].kind == GCV_LOSS_METADATA);
    options.reject_losses = 0;
    GCV_MIME_CHECK(gcv_execute_result(&source, output, "model/vnd.brlcad", &request, &options, 0, NULL, filename, &result) == GCV_CONVERSION_PARTIAL);
    GCV_MIME_CHECK(db_filetype(filename) == 4 && bu_file_delete(filename));
    GCV_MIME_CHECK(mk_sph(writer, "object_name_exceeding_v4_limit", center, 1) >= 0);
    GCV_MIME_CHECK(gcv_execute_result(&source, output, "model/vnd.brlcad", &request, &options, 0, NULL, filename, &result) == GCV_CONVERSION_FAILED);
    GCV_MIME_CHECK(!bu_file_exists(filename, NULL));
    gcv_conversion_result_free(&result);
    gcv_context_destroy(&source);
    return 0;
}

static int
mime_asc_roundtrip_test(void)
{
    struct gcv_context source;
    struct gcv_context imported;
    struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    const char *name = "odd{sphere}\"\\$[*]";
    const char *key = "odd{key}\"\\$[*]";
    const char *value = "literal braces { and } quotes \" slash \\ newline\nsecond line\t$[command]";
    const char *title = "Title with \" { } \\ $[text]\nnext line";
    const point_t center = {0, 0, 0};
    char filename[MAXPATHLEN];
    gcv_context_init(&source);
    gcv_context_init(&imported);
    struct rt_wdb *writer = wdb_dbopen(source.dbip, RT_WDB_TYPE_DB_INMEM);
    GCV_MIME_CHECK(mk_id_units(writer, title, "mm") >= 0 && mk_sph(writer, name, center, 1) >= 0);
    const double custom_units = 3.7;
    GCV_MIME_CHECK(db_update_ident(source.dbip, title, custom_units) == 0);
    GCV_MIME_CHECK(db5_update_attribute(name, key, value, source.dbip) == 0);
    GCV_MIME_CHECK(mk_comb1(writer, "attribute-region", name, 0) == 0);
    GCV_MIME_CHECK(db5_update_attribute("attribute-region", "region", "R", source.dbip) == 0);
    GCV_MIME_CHECK(db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, key, value, source.dbip) == 0);
    GCV_MIME_CHECK(db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, "regionid_colortable", "{100 200 10 20 30}", source.dbip) == 0);
    GCV_MIME_CHECK(mime_unused_path(filename, sizeof(filename)));
    const struct gcv_filter *output = gcv_select_filter(&source, GCV_FILTER_WRITE, "application/vnd.brlcad.script", "ASC Writer", NULL);
    GCV_MIME_CHECK(output && gcv_execute_result(&source, output, "application/vnd.brlcad.script", NULL, NULL, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
    const struct gcv_filter *input = gcv_select_filter(&imported, GCV_FILTER_READ, "application/vnd.brlcad.script", "ASC Reader", NULL);
    GCV_MIME_CHECK(input && gcv_execute_result(&imported, input, "application/vnd.brlcad.script", NULL, NULL, 0, NULL, filename, &result) == GCV_CONVERSION_COMPLETE);
    GCV_MIME_CHECK(BU_STR_EQUAL(imported.dbip->dbi_title, title));
    GCV_MIME_CHECK(EQUAL(imported.dbip->dbi_local2base, custom_units) &&
	EQUAL(imported.dbip->dbi_base2local, 1 / custom_units));
    const struct directory *attribute_region = db_lookup(imported.dbip, "attribute-region", LOOKUP_QUIET);
    GCV_MIME_CHECK(attribute_region && (attribute_region->d_flags & RT_DIR_REGION));
    const char *objects[] = {name, DB5_GLOBAL_OBJECT_NAME};
    for (size_t i = 0; i < sizeof(objects) / sizeof(objects[0]); ++i) {
	struct directory *entry = db_lookup(imported.dbip, objects[i], LOOKUP_QUIET);
	struct bu_attribute_value_set attributes;
	bu_avs_init_empty(&attributes);
	GCV_MIME_CHECK(entry && !db5_get_attributes(imported.dbip, &attributes, entry));
	const char *actual = bu_avs_get(&attributes, key);
	GCV_MIME_CHECK(actual && BU_STR_EQUAL(actual, value));
	bu_avs_free(&attributes);
    }
    const struct mater *color = db_mater_head(imported.dbip);
    GCV_MIME_CHECK(color && color->mt_low == 100 && color->mt_high == 200 && color->mt_r == 10 && color->mt_g == 20 && color->mt_b == 30);
    GCV_MIME_CHECK(bu_file_delete(filename));
    gcv_conversion_result_free(&result);
    gcv_context_destroy(&imported);
    gcv_context_destroy(&source);
    return 0;
}

static int
mime_invalid_status(struct gcv_context *UNUSED(context), const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *UNUSED(target))
{
    return -1;
}

static int
mime_invalid_status_test(void)
{
    struct gcv_context context;
    const struct gcv_conversion conversion = {"Invalid status fixture", GCV_FILTER_FILTER, NULL, NULL, mime_invalid_status};
    const struct gcv_handler handler = {&conversion, NULL, 0, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0, &handler, 1};
    const struct bu_ptbl *filters;
    int found = 0;
    gcv_context_init(&context);
    GCV_MIME_CHECK(gcv_register_plugin(&context, &plugin, NULL));
    filters = gcv_list_filters(&context);
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	const struct gcv_filter *filter = (const struct gcv_filter *)BU_PTBL_GET(filters, i);
	if (BU_STR_EQUAL(filter->name, conversion.name)) {
	    GCV_MIME_CHECK(!gcv_execute(&context, filter, NULL, 0, NULL, NULL));
	    found = 1;
	}
    }
    GCV_MIME_CHECK(found);
    gcv_context_destroy(&context);
    return 0;
}

static int
mime_obj_profiles_test(void)
{
#ifdef TEST_GCV_OBJ
    const char text[] = "o tetra\nv 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
	"f 1 3 2\nf 1 2 4\nf 1 4 3\nf 2 3 4\n";
    const struct bu_mime_trait assertions[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("version", "1.0"),
	BU_MIME_TRAIT_STRING("obj.materials", "none"), BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion")
    };
    struct gcv_context context;
    char filename[MAXPATHLEN];
    struct directory **objects = NULL;
    struct rt_db_internal object;
    RT_DB_INTERNAL_INIT(&object);
    gcv_context_init(&context);
    struct rt_wdb *database = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
    const point_t center = {2, 3, 4};
    GCV_MIME_CHECK(database && !mk_id_units(database, "Before OBJ profile validation", "cm") &&
	!mk_sph(database, "kept.s", center, 1));
    const fastf_t original_units = context.dbip->dbi_local2base;
    FILE *file = bu_temp_file(filename, sizeof(filename));
    GCV_MIME_CHECK(file && fwrite(text, 1, sizeof(text) - 1, file) == sizeof(text) - 1 && !fclose(file));
    const struct gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, "model/obj", "OBJ Reader", NULL);
    GCV_MIME_CHECK(reader);
    for (size_t i = 0; i < sizeof(assertions) / sizeof(assertions[0]); ++i) {
	const struct bu_mime_traits request = {assertions + i, 1};
	GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "model/obj", &request, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, "Before OBJ profile validation") &&
	    EQUAL(context.dbip->dbi_local2base, original_units) && db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL) == 1 && db_lookup(context.dbip, "kept.s", LOOKUP_QUIET));
    }
    struct bu_vls source = BU_VLS_INIT_ZERO;
    bu_vls_strcpy(&source, text);
    bu_vls_putc(&source, '#');
    for (size_t i = 0; i < BU_MIME_DEFAULT_SCAN_LIMIT; ++i) bu_vls_putc(&source, ' ');
    const size_t prefix_size = bu_vls_strlen(&source);
    const struct {const char *suffix; const char *key; const char *value;} sources[] = {
	{"caf\xc3\xa9\n", "encoding", "ascii"}, {"bad \xff\n", "encoding", "utf-8"},
	{"\nmtllib missing-sidecar.mtl\n", "obj.material-library-reference", "false"}
    };
    const char *prefixes[] = {"", "\xef\xbb\xbf"};
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
	bu_vls_trunc(&source, prefix_size);
	bu_vls_strcat(&source, sources[i].suffix);
	for (size_t prefix = 0; prefix < sizeof(prefixes) / sizeof(prefixes[0]); ++prefix) {
	    file = fopen(filename, "wb");
	    GCV_MIME_CHECK(file && fputs(prefixes[prefix], file) >= 0 &&
		fwrite(bu_vls_cstr(&source), 1, bu_vls_strlen(&source), file) == bu_vls_strlen(&source) && !fclose(file));
	    const struct bu_mime_trait value = BU_MIME_TRAIT_STRING(sources[i].key, sources[i].value);
	    const struct bu_mime_traits request = {&value, 1};
	    GCV_MIME_CHECK(!gcv_execute_variant(&context, reader, "model/obj", &request, NULL, 0, NULL, filename));
	    GCV_MIME_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, "Before OBJ profile validation") &&
		EQUAL(context.dbip->dbi_local2base, original_units) && db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL) == 1);
	}
    }
    const struct {const unsigned char *bom; size_t size; size_t width; int little;} wide[] = {
	{(const unsigned char *)"\xff\xfe", 2, 2, 1}, {(const unsigned char *)"\xfe\xff", 2, 2, 0},
	{(const unsigned char *)"\xff\xfe\0\0", 4, 4, 1}, {(const unsigned char *)"\0\0\xfe\xff", 4, 4, 0}
    };
    for (size_t i = 0; i < sizeof(wide) / sizeof(wide[0]); ++i) {
	file = fopen(filename, "wb");
	GCV_MIME_CHECK(file && fwrite(wide[i].bom, 1, wide[i].size, file) == wide[i].size);
	for (size_t c = 0; c < sizeof(text) - 1; ++c)
	    for (size_t byte = 0; byte < wide[i].width; ++byte)
		GCV_MIME_CHECK(fputc((wide[i].little ? byte : wide[i].width - byte - 1) == 0 ? text[c] : 0, file) != EOF);
	GCV_MIME_CHECK(!fclose(file) && !gcv_execute_variant(&context, reader, "model/obj", NULL, NULL, 0, NULL, filename));
	GCV_MIME_CHECK(BU_STR_EQUAL(context.dbip->dbi_title, "Before OBJ profile validation") &&
	    EQUAL(context.dbip->dbi_local2base, original_units) && db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL) == 1);
    }
    bu_vls_free(&source);
    file = fopen(filename, "wb");
    GCV_MIME_CHECK(file && fwrite(text, 1, sizeof(text) - 1, file) == sizeof(text) - 1 && !fclose(file));
    const struct bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", "ascii");
    const struct bu_mime_traits profile = {&encoding, 1};
    GCV_MIME_CHECK(gcv_execute_variant(&context, reader, "model/obj", &profile, NULL, 0, NULL, filename));
    const size_t primitive_count = db_ls(context.dbip, DB_LS_PRIM, NULL, &objects);
    GCV_MIME_CHECK(primitive_count == 2);
    size_t bots = 0;
    for (size_t i = 0; i < primitive_count; ++i) {
	const int type = rt_db_get_internal(&object, objects[i], context.dbip, NULL);
	GCV_MIME_CHECK(type >= 0);
	if (type == ID_BOT) {
	    const struct rt_bot_internal *bot = (const struct rt_bot_internal *)object.idb_ptr;
	    GCV_MIME_CHECK(bot->num_faces == 4 && bot->num_vertices >= 4);
	    point_t minimum, maximum;
	    const point_t expected_minimum = {0, 0, 0}, expected_maximum = {1, 1, 1};
	    VMOVE(minimum, bot->vertices);
	    VMOVE(maximum, bot->vertices);
	    for (size_t vertex = 1; vertex < bot->num_vertices; ++vertex)
		VMINMAX(minimum, maximum, bot->vertices + vertex * ELEMENTS_PER_VECT);
	    GCV_MIME_CHECK(VNEAR_EQUAL(minimum, expected_minimum, SMALL_FASTF) &&
		VNEAR_EQUAL(maximum, expected_maximum, SMALL_FASTF));
	    ++bots;
	}
	rt_db_free_internal(&object);
    }
    GCV_MIME_CHECK(bots == 1 && !gcv_target_traits(&context) && !gcv_target_request_traits(&context));
    bu_free(objects, "OBJ profile primitives");
    const struct gcv_filter *writer = gcv_select_filter(&context, GCV_FILTER_WRITE, "model/obj", "OBJ Writer", NULL);
    struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    const struct bu_mime_trait text_value = BU_MIME_TRAIT_STRING("obj.serialization", "text");
    const struct bu_mime_traits text_request = {&text_value, 1};
    GCV_MIME_CHECK(writer && gcv_resolve_variant(&context, GCV_FILTER_WRITE, filename, "model/obj", &text_request,
	"OBJ Writer", NULL, &resolved, NULL) == writer && !bu_mime_traits_get(&resolved, "encoding"));
    bu_mime_traits_free(&resolved);
    const char *unicode = "caf\xc3\xa9.r", *parent = "group\xc3\xa9";
    GCV_MIME_CHECK(!mk_comb1(database, unicode, "kept.s", 1) &&
	!mk_comb1(database, "plain.r", "kept.s", 1) && !mk_comb1(database, parent, "plain.r", 0));
    const char *selected[] = {unicode};
    struct gcv_opts options;
    gcv_opts_default(&options);
    options.num_objects = 1;
    options.object_names = selected;
    const char marker[] = "Keep this file on an OBJ profile rejection.\n";
    const char *bad[] = {unicode, parent, "group\xc3\xa9/plain.r", "plain.r"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
	selected[0] = bad[i];
	struct bu_vls output = BU_VLS_INIT_ZERO, material = BU_VLS_INIT_ZERO;
	const char *suffix = i + 1 == sizeof(bad) / sizeof(bad[0]) ? "-caf\xc3\xa9" : "";
	bu_vls_printf(&output, "%s%s.obj", filename, suffix);
	bu_vls_printf(&material, "%s%s.mtl", filename, suffix);
	const char *paths[] = {bu_vls_cstr(&output), bu_vls_cstr(&material)};
	for (size_t path = 0; path < sizeof(paths) / sizeof(paths[0]); ++path) {
	    FILE *destination = fopen(paths[path], "wb");
	    GCV_MIME_CHECK(destination && fwrite(marker, 1, sizeof(marker) - 1, destination) == sizeof(marker) - 1 && !fclose(destination));
	}
	GCV_MIME_CHECK(!gcv_execute_variant(&context, writer, "model/obj", &profile, &options, 0, NULL, bu_vls_cstr(&output)));
	for (size_t path = 0; path < sizeof(paths) / sizeof(paths[0]); ++path) {
	    struct bu_vls contents = BU_VLS_INIT_ZERO;
	    int ascii = 0;
	    GCV_MIME_CHECK(mime_read_text_file(&contents, paths[path], &ascii) && BU_STR_EQUAL(bu_vls_cstr(&contents), marker));
	    bu_vls_free(&contents);
	    GCV_MIME_CHECK(bu_file_delete(paths[path]));
	}
	GCV_MIME_CHECK(!gcv_execute_variant(&context, writer, "model/obj", &profile, &options, 0, NULL, bu_vls_cstr(&output)) &&
	    !bu_file_exists(bu_vls_cstr(&output), NULL) && !bu_file_exists(bu_vls_cstr(&material), NULL));
	bu_vls_free(&output);
	bu_vls_free(&material);
    }
    GCV_MIME_CHECK(bu_file_delete(filename));
    gcv_context_destroy(&context);
#endif
    return 0;
}

int
main(int argc, const char **argv)
{
    const int context_iterations = 3;
    if (argc == 2 && BU_STR_EQUAL(argv[1], "directory-input")) return mime_directory_test();
    if (argc == 2 && BU_STR_EQUAL(argv[1], "variants")) return mime_variant_test();
    if (argc == 2 && BU_STR_EQUAL(argv[1], "obj-profiles")) return mime_obj_profiles_test();
    if (argc == 4 && BU_STR_EQUAL(argv[1], "native-assertions")) return mime_native_assertions_test(argv[2], argv[3]);
    if (argc == 2 && BU_STR_EQUAL(argv[1], "ply-profiles")) return mime_ply_profile_test();
    if (argc == 2 && BU_STR_EQUAL(argv[1], "stl-output-profiles")) return mime_stl_output_test();
    if (argc == 2 && BU_STR_EQUAL(argv[1], "stl-profiles")) return mime_stl_context_test();
#ifdef TEST_GCV_ASSETIMPORT
    if (argc == 2 && BU_STR_EQUAL(argv[1], "sdk-xml-charsets")) return gcv_sdk_xml_charset_profiles();
    if (argc == 2 && BU_STR_EQUAL(argv[1], "sdk-assertions")) return gcv_sdk_assertion_profiles();
    if (argc == 3 && BU_STR_EQUAL(argv[2], "ifc")) return gcv_sdk_ifc_profiles(argv[1]);
    if (argc != 2) {
	bu_log("Expected the IFC fixture filename\n");
	return 1;
    }
    if (gcv_sdk_profiles(argv[1])) return 1;
#else
    (void)argc;
#endif
    if (mime_obj_profiles_test() || mime_database_test() || mime_asc_roundtrip_test() || mime_result_test() || mime_feature_test() || mime_stl_context_test() || mime_ply_profile_test() || mime_ply_color_test() || mime_ply_output_colors_test() || mime_vtk_context_test() || mime_sdk_bindings_test() || mime_sdk_input_type_test() || mime_gdal_input_type_test() || mime_vrml_profiles_test() || mime_ply_context_test() || mime_gltf_scene_test() ||
	mime_invalid_status_test() || mime_variant_test() || mime_native_probe_test())
	return 1;
    bu_setprogname(argv[0]);
    for (int i = 0; i < context_iterations; ++i)
	if (mime_plugin_test())
	    return 1;
    return 0;
}
