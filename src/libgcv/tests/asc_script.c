/*                    A S C _ S C R I P T . C
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
#include "bu/vls.h"
#include "gcv.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/mater.h"
#include "wdb.h"

static int
asc_operation(struct gcv_context *context, enum gcv_filter_type type, const char *path)
{
    const struct gcv_filter *filter = find_filter(type, BU_MIME_MODEL_VND_BRLCAD_PLUS_ASC, NULL, context);
    return filter ? gcv_execute(context, filter, NULL, 0, NULL, path) : -1;
}

/* A setup error must not satisfy a test expecting a converter failure. */
static int
read_script(struct gcv_context *context, const char *text, size_t length)
{
    char path[MAXPATHLEN] = {0};
    FILE *file = bu_temp_file(path, sizeof(path));
    int status = -1;
    if (!file) return status;
    int written = fwrite(text, 1, length, file) == length;
    int closed = !fclose(file);
    if (written && closed) {
	status = asc_operation(context, GCV_FILTER_READ, path);
    }
    bu_file_delete(path);
    return status;
}

static int
dispatch_test(void)
{
    struct gcv_context context;
    char path[MAXPATHLEN] = {0};
    struct bu_vls destination = BU_VLS_INIT_ZERO;
    const char *text = "title {dispatch title}";
    const char *invalid[] = {"", "# no database records\n", "unsupported command\n"};
    int failures = 0;
    FILE *file;
    gcv_context_init(&context);
    if (read_script(&context, text, strlen(text)) != 1 ||
	!BU_STR_EQUAL(context.dbip->dbi_title, "dispatch title")) {
	bu_log("ASC dispatch failed to read a final command without a newline\n");
	++failures;
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
	if (read_script(&context, invalid[i], strlen(invalid[i])) != 0) {
	    bu_log("ASC dispatch accepted empty or unsupported input %zu\n", i);
	    ++failures;
	}
    }
    file = bu_temp_file(path, sizeof(path));
    if (!file || fclose(file)) {
	++failures;
	goto done;
    }
    bu_vls_printf(&destination, "%s/output.asc", path);
    if (asc_operation(&context, GCV_FILTER_WRITE, bu_vls_cstr(&destination)) != 0) {
	bu_log("ASC dispatch did not propagate an output-open failure\n");
	++failures;
    }
done:
    if (path[0]) bu_file_delete(path);
    bu_vls_free(&destination);
    gcv_context_destroy(&context);
    return failures ? 1 : 0;
}

static int
attribute_value(struct gcv_context *context, const char *name, const char *key, const char *expected)
{
    struct directory *entry = db_lookup(context->dbip, name, LOOKUP_QUIET);
    struct bu_attribute_value_set attributes;
    int equal = 0;
    if (!entry) return 0;
    bu_avs_init_empty(&attributes);
    if (!db5_get_attributes(context->dbip, &attributes, entry)) {
	const char *actual = bu_avs_get(&attributes, key);
	equal = expected ? actual && BU_STR_EQUAL(actual, expected) : !actual;
    }
    bu_avs_free(&attributes);
    return equal;
}

static int
parser_test(void)
{
    struct gcv_context context;
    const char *literal_quote = "title {Title with a literal \" quote}\n";
    const char *multiline =
	"units cm\r\n"
	"put sphere ell \\\r\n"
	"  V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}\r\n"
	"attr set sphere first \"line one\nline two\" second \"another\nmultiline value\"\n";
    const char *invalid[] = {
	"title {unterminated\n",
	"title good\nunsupported_command\n",
	"title good\nput invalid ell V not-a-vector\n",
	"title good\nattr set sphere key\n",
	"title good\n\\\n"
    };
    const char binary[] = "title before\n\0title after\n";
    const char *recover = "title recovered\nput second ell V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}\n";
    int failures = 0;
    gcv_context_init(&context);
    if (read_script(&context, literal_quote, strlen(literal_quote)) != 1 ||
	!BU_STR_EQUAL(context.dbip->dbi_title, "Title with a literal \" quote")) {
	bu_log("ASC parser mishandled a quote inside a brace word\n");
	++failures;
    }
    if (read_script(&context, multiline, strlen(multiline)) != 1 ||
	!EQUAL(context.dbip->dbi_local2base, 10) ||
	!attribute_value(&context, "sphere", "first", "line one\nline two") ||
	!attribute_value(&context, "sphere", "second", "another\nmultiline value")) {
	bu_log("ASC parser mishandled units, continued lines or multiline words\n");
	++failures;
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
	if (read_script(&context, invalid[i], strlen(invalid[i])) != 0) {
	    bu_log("ASC parser accepted incomplete, unsupported or failed command %zu\n", i);
	    ++failures;
	}
    }
    if (read_script(&context, binary, sizeof(binary) - 1) != 0) {
	bu_log("ASC parser accepted a NUL in text\n");
	++failures;
    }
    if (read_script(&context, recover, strlen(recover)) != 1 ||
	!BU_STR_EQUAL(context.dbip->dbi_title, "recovered") ||
	!db_lookup(context.dbip, "second", LOOKUP_QUIET)) {
	bu_log("ASC parser failed to reuse a context after failed input\n");
	++failures;
    }
    gcv_context_destroy(&context);
    return failures ? 1 : 0;
}

static int
attributes_test(void)
{
    struct gcv_context context;
    const char *text =
	"title initial\n"
	"put {part*} ell V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}\n"
	"put part-one ell V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}\n"
	"attr set {part*} note {literal target}\n"
	"put grouped comb region no tree {l part*}\n"
	"attr set grouped region R region_id 17\n"
	"attr set _GLOBAL title {attribute title} units 3.7 regionid_colortable {{100 200 10 20 30}}\n"
	"units cm\n";
    const char *invalid = "attr set _GLOBAL title invalid units not-units\n";
    const struct directory *region;
    const struct mater *color;
    int failures = 0;
    gcv_context_init(&context);
    if (read_script(&context, text, strlen(text)) != 1 ||
	!attribute_value(&context, "part*", "note", "literal target") ||
	!attribute_value(&context, "part-one", "note", NULL)) {
	bu_log("ASC attribute assignment did not use a literal object name\n");
	++failures;
    }
    region = db_lookup(context.dbip, "grouped", LOOKUP_QUIET);
    if (!region || !(region->d_flags & RT_DIR_REGION)) {
	bu_log("ASC region attributes did not refresh directory flags\n");
	++failures;
    }
    color = db_mater_head(context.dbip);
    if (!BU_STR_EQUAL(context.dbip->dbi_title, "attribute title") ||
	!EQUAL(context.dbip->dbi_local2base, 10) || !EQUAL(context.dbip->dbi_base2local, 0.1) ||
	!color || color->mt_low != 100 || color->mt_high != 200 ||
	color->mt_r != 10 || color->mt_g != 20 || color->mt_b != 30) {
	bu_log("ASC global attributes did not refresh cached database state\n");
	++failures;
    }
    if (read_script(&context, invalid, strlen(invalid)) != 0 ||
	!attribute_value(&context, DB5_GLOBAL_OBJECT_NAME, "title", "attribute title")) {
	bu_log("ASC accepted an invalid global unit scale or changed its attributes\n");
	++failures;
    }
    gcv_context_destroy(&context);
    return failures ? 1 : 0;
}

static int
roundtrip_test(void)
{
    struct gcv_context source, imported;
    char path[MAXPATHLEN] = {0};
    const char *name = "odd{sphere}\"\\$[*]";
    const char *key = "odd{key}\"\\$[*]";
    const char *value = "quote \" slash \\ newline\nsecond line\t$[command] {unclosed";
    const char *title = "Title with \" { } \\ $[text]\nnext line";
    const double scale = 3.7;
    point_t center = {1, 2, 3};
    struct rt_wdb *writer;
    struct directory *entry;
    struct rt_db_internal object;
    FILE *file;
    int failures = 0;
    int close_status;
    gcv_context_init(&source);
    gcv_context_init(&imported);
    file = bu_temp_file(path, sizeof(path));
    if (!file) {
	failures = 1;
	goto done;
    }
    close_status = fclose(file);
    if (close_status) {
	failures = 1;
	goto done;
    }
    writer = wdb_dbopen(source.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!writer || db_update_ident(source.dbip, title, scale) < 0 ||
	mk_sph(writer, name, center, 2) < 0 || mk_comb1(writer, "grouped", name, 1) < 0 ||
	db5_update_attribute(name, key, value, source.dbip) ||
	db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, key, value, source.dbip) ||
	db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, "regionid_colortable", "{100 200 10 20 30}", source.dbip)) {
	bu_log("Could not prepare the ASC serialization fixture\n");
	failures = 1;
	goto done;
    }
    if (asc_operation(&source, GCV_FILTER_WRITE, path) != 1 ||
	asc_operation(&imported, GCV_FILTER_READ, path) != 1 ||
	!BU_STR_EQUAL(imported.dbip->dbi_title, title) || !EQUAL(imported.dbip->dbi_local2base, scale) ||
	!attribute_value(&imported, name, key, value) ||
	!attribute_value(&imported, DB5_GLOBAL_OBJECT_NAME, key, value) ||
	!attribute_value(&imported, DB5_GLOBAL_OBJECT_NAME, "regionid_colortable", "{100 200 10 20 30}")) {
	bu_log("ASC round trip changed literal names, metadata or units\n");
	failures = 1;
	goto done;
    }
    entry = db_lookup(imported.dbip, "grouped", LOOKUP_QUIET);
    if (!entry || !(entry->d_flags & RT_DIR_REGION)) {
	bu_log("ASC round trip lost a region\n");
	failures = 1;
	goto done;
    }
    entry = db_lookup(imported.dbip, name, LOOKUP_QUIET);
    if (!entry || rt_db_get_internal(&object, entry, imported.dbip, NULL) < 0) {
	failures = 1;
	goto done;
    }
    if (object.idb_type != ID_ELL) {
	failures = 1;
    } else {
	const struct rt_ell_internal *ellipsoid = (const struct rt_ell_internal *)object.idb_ptr;
	if (!VNEAR_EQUAL(ellipsoid->v, center, SMALL_FASTF) || !EQUAL(MAGNITUDE(ellipsoid->a), 2)) {
	    bu_log("ASC round trip changed sphere geometry\n");
	    failures = 1;
	}
    }
    rt_db_free_internal(&object);
    gcv_context_destroy(&imported);
    gcv_context_init(&imported);
    if (db_update_ident(source.dbip, "", scale) < 0 ||
	asc_operation(&source, GCV_FILTER_WRITE, path) != 1 ||
	asc_operation(&imported, GCV_FILTER_READ, path) != 1 || imported.dbip->dbi_title[0]) {
	bu_log("ASC serialization did not preserve an empty title\n");
	failures = 1;
    }
done:
    if (path[0]) bu_file_delete(path);
    gcv_context_destroy(&imported);
    gcv_context_destroy(&source);
    return failures;
}

static int
write_failure_test(const char *path)
{
    struct gcv_context context;
    point_t center = {0, 0, 0};
    struct rt_wdb *writer;
    int failed;
    gcv_context_init(&context);
    writer = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
    failed = !writer || mk_sph(writer, "sphere", center, 1) < 0 ||
	asc_operation(&context, GCV_FILTER_WRITE, path) != 0;
    if (failed) bu_log("ASC writer did not report a delayed output failure\n");
    gcv_context_destroy(&context);
    return failed;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "write-failure")) return write_failure_test(argv[2]);
    if (argc != 2) return 1;
    if (BU_STR_EQUAL(argv[1], "dispatch")) return dispatch_test();
    if (BU_STR_EQUAL(argv[1], "parser")) return parser_test();
    if (BU_STR_EQUAL(argv[1], "attributes")) return attributes_test();
    if (BU_STR_EQUAL(argv[1], "roundtrip")) return roundtrip_test();
    return 1;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
