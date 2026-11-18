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
	const struct gcv_filter *reader = find_filter(GCV_FILTER_READ,
	    BU_MIME_MODEL_VND_BRLCAD_PLUS_ASC, NULL, context);
	if (reader) status = gcv_execute(context, reader, NULL, 0, NULL, path);
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
    const struct gcv_filter *writer;
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
    writer = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_VND_BRLCAD_PLUS_ASC, NULL, &context);
    if (!writer || gcv_execute(&context, writer, NULL, 0, NULL, bu_vls_cstr(&destination))) {
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
	equal = actual && BU_STR_EQUAL(actual, expected);
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

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2) return 1;
    if (BU_STR_EQUAL(argv[1], "dispatch")) return dispatch_test();
    if (BU_STR_EQUAL(argv[1], "parser")) return parser_test();
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
