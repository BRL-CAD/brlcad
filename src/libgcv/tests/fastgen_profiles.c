/* F A S T G E N _ P R O F I L E S . C
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

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "gcv/api.h"
#include "rt/search.h"
#include "wdb.h"

#define FASTGEN_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("FASTGEN profile check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static int
write_deck(const char *filename, const char *newline, int padded)
{
    FILE *stream = fopen(filename, "wb");
    int valid;
    if (!stream) return 0;
    if (padded)
	for (size_t size = 0; size <= BU_MIME_DEFAULT_SCAN_LIMIT; size += sizeof("$COMMENT padding\n") - 1)
	    fprintf(stream, "$COMMENT padding%s", newline);
    fprintf(stream, "%-8s%8d%8d%8d%s", "SECTION", 1, 1, 2, newline);
    fprintf(stream, "%-8s%8d%8s%8s%8s%8s%s", "GRID", 1, "", "0.0", "0.0", "0.0", newline);
    fprintf(stream, "%-8s%8d%8d%8d%8s%8s%8s%8s%8s%s", "CSPHERE", 1, 1, 1, "", "", "", "", "1.0", newline);
    fprintf(stream, "ENDDATA%s", newline);
    valid = !ferror(stream);
    if (fclose(stream)) valid = 0;
    return valid;
}

static int
sentinel_file(const char *filename, int write)
{
    const char sentinel[] = "preserved";
    char actual[sizeof(sentinel)];
    FILE *stream = fopen(filename, write ? "wb" : "rb");
    int valid;
    if (!stream) return 0;
    if (write)
	valid = fwrite(sentinel, 1, sizeof(sentinel), stream) == sizeof(sentinel);
    else
	valid = fread(actual, 1, sizeof(actual), stream) == sizeof(actual) &&
	    !memcmp(actual, sentinel, sizeof(sentinel)) && fgetc(stream) == EOF && !ferror(stream);
    if (fclose(stream)) valid = 0;
    return valid;
}

static int
check_input(const char *filename, const struct bu_mime_traits *profile, int expected,
    size_t argc, const char *const *argv)
{
    struct gcv_context context;
    struct gcv_opts options;
    const struct gcv_filter *reader;
    char *title;
    int valid;
    gcv_context_init(&context);
    gcv_opts_default(&options);
    title = bu_strdup(context.dbip->dbi_title);
    reader = gcv_select_filter(&context, GCV_FILTER_READ, "model/vnd.fastgen", "FASTGEN4 Reader", NULL);
    valid = reader && gcv_execute_variant(&context, reader, "model/vnd.fastgen", profile, &options, argc, argv, filename) == expected;
    if (expected)
	valid = valid && db_ls(context.dbip, DB_LS_PRIM, NULL, NULL) == 1;
    else
	valid = valid && !db_ls(context.dbip, DB_LS_PRIM | DB_LS_COMB, NULL, NULL) &&
	    BU_STR_EQUAL(title, context.dbip->dbi_title) && EQUAL(context.dbip->dbi_local2base, 1.0);
    bu_free(title, "original title");
    gcv_context_destroy(&context);
    return valid;
}

int
main(int UNUSED(argc), const char **argv)
{
    enum {input_file, output_file, muves_file, plot_file, file_count};
    char files[file_count][MAXPATHLEN];
    struct bu_vls colors = BU_VLS_INIT_ZERO;
    const struct bu_mime_trait dialect = BU_MIME_TRAIT_STRING("fastgen.dialect", "4");
    const struct bu_mime_traits profile = {&dialect, 1};
    const struct bu_mime_trait unsupported[] = {
	BU_MIME_TRAIT_STRING("fastgen.dialect", "3"), BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("version", "4.1"), BU_MIME_TRAIT_STRING("fastgen.unestablished", "assertion")};
    const char *arguments[] = {"--muves", files[muves_file], "--plot", files[plot_file]};
    bu_setprogname(argv[0]);
    for (size_t i = 0; i < file_count; ++i) {
	FILE *stream = bu_temp_file(files[i], sizeof(files[i]));
	FASTGEN_CHECK(stream && !fclose(stream));
    }
    bu_vls_printf(&colors, "%s.colors", files[output_file]);
    FASTGEN_CHECK(write_deck(files[input_file], "\n", 0));
    FASTGEN_CHECK(check_input(files[input_file], &profile, 1, 0, NULL));
    FASTGEN_CHECK(write_deck(files[input_file], "\r\n", 0));
    FASTGEN_CHECK(check_input(files[input_file], &profile, 1, 0, NULL));
    FASTGEN_CHECK(write_deck(files[input_file], "\n", 1));
    FASTGEN_CHECK(check_input(files[input_file], NULL, 1, 0, NULL));
    FASTGEN_CHECK(check_input(files[input_file], &profile, 1, 0, NULL));
    FASTGEN_CHECK(sentinel_file(files[muves_file], 1) && sentinel_file(files[plot_file], 1));
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	struct bu_mime_traits request = {&unsupported[i], 1};
	FASTGEN_CHECK(check_input(files[input_file], &request, 0, sizeof(arguments) / sizeof(arguments[0]), arguments));
	FASTGEN_CHECK(sentinel_file(files[muves_file], 0) && sentinel_file(files[plot_file], 0));
    }
    {
	FILE *stream = fopen(files[input_file], "wb");
	FASTGEN_CHECK(stream && fputs("BEGIN BULK\nGRID,1,,0,0,0\nENDDATA\n", stream) >= 0 && !fclose(stream));
	FASTGEN_CHECK(check_input(files[input_file], NULL, 0, sizeof(arguments) / sizeof(arguments[0]), arguments));
	FASTGEN_CHECK(sentinel_file(files[muves_file], 0) && sentinel_file(files[plot_file], 0));
    }
    {
	struct gcv_context context;
	struct gcv_opts options;
	struct rt_wdb *database;
	const struct gcv_filter *writer;
	const char *names[] = {"sphere.r"};
	point_t center = VINIT_ZERO;
	gcv_context_init(&context);
	gcv_opts_default(&options);
	options.object_names = names;
	options.num_objects = sizeof(names) / sizeof(names[0]);
	database = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
	FASTGEN_CHECK(database && !mk_sph(database, "sphere.s", center, 1) && !mk_comb1(database, names[0], "sphere.s", 1));
	writer = gcv_select_filter(&context, GCV_FILTER_WRITE, "model/vnd.fastgen", "FASTGEN4 Writer", NULL);
	FASTGEN_CHECK(writer && gcv_execute_variant(&context, writer, "model/vnd.fastgen", &profile, &options, 0, NULL, files[output_file]));
	FASTGEN_CHECK(check_input(files[output_file], &profile, 1, 0, NULL));
	FASTGEN_CHECK(sentinel_file(files[output_file], 1) && sentinel_file(bu_vls_cstr(&colors), 1));
	for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	    struct bu_mime_traits request = {&unsupported[i], 1};
	    FASTGEN_CHECK(!gcv_execute_variant(&context, writer, "model/vnd.fastgen", &request, &options, 0, NULL, files[output_file]));
	    FASTGEN_CHECK(sentinel_file(files[output_file], 0) && sentinel_file(bu_vls_cstr(&colors), 0));
	}
	gcv_context_destroy(&context);
    }
    for (size_t i = 0; i < file_count; ++i)
	FASTGEN_CHECK(bu_file_delete(files[i]));
    FASTGEN_CHECK(bu_file_delete(bu_vls_cstr(&colors)));
    bu_vls_free(&colors);
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
