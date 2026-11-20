/*                      J T _ P R O F I L E S . C
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
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "gcv/api.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/search.h"
#include "wdb.h"

#define JT_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("JT profile check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static int
legacy_conflict(struct gcv_context *context, const struct gcv_opts *options,
    const void *UNUSED(data), const char *target)
{
    const struct gcv_filter *writer = gcv_select_filter(context, GCV_FILTER_WRITE, "model/jt", "JT Writer", NULL);
    return writer && writer->filter_fn(context, options, "version=9", target);
}

static int
check_version(struct gcv_context *context, const char *filename, const char *version)
{
    struct bu_mime_result *detected = NULL;
    int valid = gcv_detect_file(context, filename, &detected, NULL) == BU_MIME_MATCH && detected->count &&
	BU_STR_EQUAL(detected->candidates[0].name, "model/jt") &&
	BU_STR_EQUAL(bu_mime_traits_get(&detected->candidates[0].traits, "version"), version) &&
	BU_STR_EQUAL(bu_mime_traits_get(&detected->candidates[0].traits, "byte-order"), "little-endian");
    bu_mime_result_free(detected);
    return valid;
}

int
main(int UNUSED(argc), const char **argv)
{
    fastf_t vertices[] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    int faces[] = {0, 2, 1, 0, 1, 3, 1, 2, 3, 2, 0, 3};
    const char *names[] = {"tetra"};
    const struct { const char *key; const char *value; const char *version; } modes[] = {
	{"version", "8.0", "8.0"}, {"jt.major-version", "8", "8.0"}};
    struct gcv_context source;
    struct gcv_opts options;
    struct rt_wdb *wdb;
    char filename[MAXPATHLEN];
    FILE *stream;
    const struct gcv_filter *writer;
    bu_setprogname(argv[0]);
    gcv_context_init(&source);
    gcv_opts_default(&options);
    options.object_names = names;
    options.num_objects = sizeof(names) / sizeof(names[0]);
    wdb = wdb_dbopen(source.dbip, RT_WDB_TYPE_DB_INMEM);
    JT_CHECK(wdb && !mk_id_units(wdb, "JT MIME fixture", "mm"));
    JT_CHECK(!mk_bot(wdb, names[0], RT_BOT_SOLID, RT_BOT_CCW, 0,
	sizeof(vertices) / sizeof(vertices[0]) / 3, sizeof(faces) / sizeof(faces[0]) / 3, vertices, faces, NULL, NULL));
    stream = bu_temp_file(filename, sizeof(filename));
    JT_CHECK(stream && !fclose(stream));
    writer = gcv_select_filter(&source, GCV_FILTER_WRITE, "model/jt", "JT Writer", NULL);
    JT_CHECK(writer);
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
	const struct bu_mime_trait property = BU_MIME_TRAIT_STRING(modes[i].key, modes[i].value);
	const struct bu_mime_traits profile = {&property, 1};
	struct gcv_context destination;
	struct bu_vls type = BU_VLS_INIT_ZERO;
	struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
	struct gcv_opts read_options;
	const struct gcv_filter *reader;
	gcv_opts_default(&read_options);
	read_options.verbosity_level = 1;
	JT_CHECK(gcv_select_variant(&source, GCV_FILTER_WRITE, "model/jt", &profile, "JT Writer", NULL) == writer);
	JT_CHECK(gcv_execute_variant(&source, writer, "model/jt", &profile, &options, 0, NULL, filename));
	JT_CHECK(check_version(&source, filename, modes[i].version));
	gcv_context_init(&destination);
	reader = gcv_resolve_variant(&destination, GCV_FILTER_READ, filename, NULL, NULL, "JT Reader", &type, &resolved, NULL);
	JT_CHECK(reader && BU_STR_EQUAL(bu_vls_cstr(&type), "model/jt"));
	bu_log("Checking JT %s input profile\n", modes[i].version);
	JT_CHECK(gcv_execute_variant(&destination, reader, bu_vls_cstr(&type), &resolved, &read_options, 0, NULL, filename));
	{
	    struct directory **objects = NULL;
	    size_t count = db_ls(destination.dbip, DB_LS_PRIM, NULL, &objects);
	    JT_CHECK(count > 0);
	    bu_free(objects, "JT imported primitives");
	}
	bu_vls_free(&type);
	bu_mime_traits_free(&resolved);
	gcv_context_destroy(&destination);
    }
    {
	struct gcv_context destination;
	const struct gcv_filter *reader;
	char *title;
	/* A complete empty wide-offset file isolates future-version validation
	 * from geometry decoding. Header: version, order, reserved, TOC, GUID. */
	enum {version_bytes = 80, toc_offset_position = version_bytes + 1 + 4,
	    header_bytes = toc_offset_position + 8 + 16};
	unsigned char future[header_bytes + 4] = {0};
	memset(future, ' ', version_bytes);
	memcpy(future, "Version 11.0", sizeof("Version 11.0") - 1);
	future[toc_offset_position] = header_bytes;
	gcv_context_init(&destination);
	title = bu_strdup(destination.dbip->dbi_title);
	reader = gcv_select_filter(&destination, GCV_FILTER_READ, "model/jt", "JT Reader", NULL);
	stream = fopen(filename, "wb");
	JT_CHECK(reader && stream && fwrite(future, 1, sizeof(future), stream) == sizeof(future) && !fclose(stream));
	JT_CHECK(check_version(&source, filename, "11.0"));
	JT_CHECK(!reader->filter_fn(&destination, &options, NULL, filename));
	JT_CHECK(BU_STR_EQUAL(title, destination.dbip->dbi_title));
	JT_CHECK(!db_ls(destination.dbip, DB_LS_PRIM, NULL, NULL));
	bu_free(title, "original database title");
	gcv_context_destroy(&destination);
    }
    {
	const char sentinel[] = "preserved";
	const struct bu_mime_trait unsupported[] = {
	    BU_MIME_TRAIT_STRING("version", "9.5"), BU_MIME_TRAIT_STRING("version", "10.3"),
	    BU_MIME_TRAIT_STRING("version", "11.0"), BU_MIME_TRAIT_STRING("byte-order", "big-endian")};
	stream = fopen(filename, "wb");
	JT_CHECK(stream && fwrite(sentinel, 1, sizeof(sentinel), stream) == sizeof(sentinel) && !fclose(stream));
	for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	    const struct bu_mime_traits profile = {&unsupported[i], 1};
	    JT_CHECK(!gcv_select_variant(&source, GCV_FILTER_WRITE, "model/jt", &profile, "JT Writer", NULL));
	    JT_CHECK(!gcv_execute_variant(&source, writer, "model/jt", &profile, &options, 0, NULL, filename));
	}
	JT_CHECK(!writer->filter_fn(&source, &options, "version=11", filename));
	{
	    const struct bu_mime_trait version = BU_MIME_TRAIT_STRING("version", "8.0");
	    const struct bu_mime_traits request = {&version, 1};
	    const struct bu_mime_capability format = BU_MIME_CAPABILITY("model/jt", &version, 1, 0);
	    const struct gcv_conversion conversion = {"JT legacy options fixture", GCV_FILTER_WRITE, NULL, NULL, legacy_conflict};
	    const struct gcv_handler handler = {&conversion, &format, 1, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
	    const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0, &handler, 1};
	    const struct gcv_filter *fixture;
	    JT_CHECK(gcv_register_plugin(&source, &plugin, NULL));
	    fixture = gcv_select_variant(&source, GCV_FILTER_WRITE, "model/jt", &request, conversion.name, NULL);
	    JT_CHECK(fixture && !gcv_execute_variant(&source, fixture, "model/jt", &request, &options, 0, NULL, filename));
	}
	char actual[sizeof(sentinel)];
	stream = fopen(filename, "rb");
	JT_CHECK(stream && fread(actual, 1, sizeof(actual), stream) == sizeof(actual));
	JT_CHECK(fgetc(stream) == EOF && !ferror(stream) && !fclose(stream));
	JT_CHECK(!memcmp(actual, sentinel, sizeof(sentinel)));
    }
    JT_CHECK(writer->filter_fn(&source, &options, "version=8.0", filename));
    JT_CHECK(check_version(&source, filename, "8.0"));
    JT_CHECK(bu_file_delete(filename));
    gcv_context_destroy(&source);
    return 0;
}
