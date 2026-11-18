/*                    T E S T _ P L Y _ R E A D . C
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
#include "bu/path.h"
#include "bu/str.h"
#include "gcv.h"
#include "raytrace.h"
#include "wdb.h"

#define PLY_TEST_HEADER(index_type) \
    "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n" \
    "element face 1\nproperty list uchar " index_type " vertex_indices\nend_header\n"

static const char ordered[] = PLY_TEST_HEADER("int")
    "1 2 3\n4 2 3\n1 5 3\n3 0 1 2\n";
static const char reordered[] =
    "ply\nformat ascii 1.0\nelement vertex 3\nproperty float z\nproperty float x\nproperty float y\n"
    "element face 1\nproperty list uchar int vertex_indices\nend_header\n"
    "3 1 2\n3 4 2\n3 1 5\n3 0 1 2\n";

static int
write_fixture(char path[MAXPATHLEN], const char *text)
{
    FILE *file = bu_temp_file(path, MAXPATHLEN);
    if (!file) return 0;
    size_t length = strlen(text);
    int written = fwrite(text, 1, length, file) == length;
    int closed = !fclose(file);
    return written && closed;
}

static int
read_fixture(struct gcv_context *context, const char *path, const char *scale)
{
    const struct gcv_filter *reader = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_PLY, NULL, context);
    const char *arguments[] = {"-s", scale};
    return reader ? gcv_execute(context, reader, NULL, scale ? 2 : 0, scale ? arguments : NULL, path) : -1;
}

static int
check_mesh(struct gcv_context *context, const char *path, size_t vertices, const fastf_t *expected,
    size_t faces, const int *expected_faces, double scale)
{
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct rt_db_internal object;
    struct directory *entry;
    int valid = 0;
    bu_path_component(&name, path, BU_PATH_BASENAME_EXTLESS);
    bu_vls_strcat(&name, ".bot");
    entry = db_lookup(context->dbip, bu_vls_cstr(&name), LOOKUP_QUIET);
    if (entry && rt_db_get_internal(&object, entry, context->dbip, NULL) >= 0) {
	if (object.idb_type == ID_BOT) {
	    const struct rt_bot_internal *bot = (const struct rt_bot_internal *)object.idb_ptr;
	    valid = bot->num_vertices == vertices && bot->num_faces == faces;
	    if (valid) {
		for (size_t i = 0; i < vertices * 3; ++i)
		    if (!EQUAL(bot->vertices[i], expected[i] * scale)) valid = 0;
		for (size_t face = 0; face < faces; ++face) {
		    int matched = 0;
		    for (size_t rotation = 0; rotation < 3; ++rotation) {
			int same = 1;
			for (size_t vertex = 0; vertex < 3; ++vertex)
			    if (bot->faces[face * 3 + (vertex + rotation) % 3] != expected_faces[face * 3 + vertex]) same = 0;
			matched |= same;
		    }
		    if (!matched) valid = 0;
		}
	    }
	}
	rt_db_free_internal(&object);
    }
    if (!valid) bu_log("PLY import changed mesh geometry for %s\n", bu_vls_cstr(&name));
    bu_vls_free(&name);
    return valid;
}

static int
check_triangle(struct gcv_context *context, const char *path, double scale)
{
    const fastf_t vertices[] = {1, 2, 3, 4, 2, 3, 1, 5, 3};
    const int faces[] = {0, 1, 2};
    return check_mesh(context, path, 3, vertices, 1, faces, scale);
}

static int
state_test(void)
{
    struct gcv_context contexts[2];
    const char *scales[] = {"1", "2", "3", NULL};
    const double expected_scales[] = {1, 2, 3, 1000};
    char paths[4][MAXPATHLEN] = {{0}};
    int failed = 0;
    gcv_context_init(&contexts[0]);
    gcv_context_init(&contexts[1]);
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
	if (!write_fixture(paths[i], i % 2 ? reordered : ordered) ||
	    read_fixture(&contexts[i % 2], paths[i], scales[i]) != 1) {
	    bu_log("PLY conversion %zu failed\n", i);
	    failed = 1;
	    goto done;
	}
	for (size_t j = 0; j <= i; ++j) {
	    if (!check_triangle(&contexts[j % 2], paths[j], expected_scales[j])) {
		failed = 1;
		goto done;
	    }
	}
    }
done:
    gcv_context_destroy(&contexts[1]);
    gcv_context_destroy(&contexts[0]);
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
	if (paths[i][0]) bu_file_delete(paths[i]);
    return failed;
}

static size_t
mesh_count(struct gcv_context *context)
{
    struct directory *entry;
    size_t count = 0;
    FOR_ALL_DIRECTORY_START(entry, context->dbip) {
	if (entry->d_major_type == DB5_MAJORTYPE_BRLCAD && entry->d_minor_type == ID_BOT) ++count;
    } FOR_ALL_DIRECTORY_END;
    return count;
}

static int
errors_test(void)
{
    struct gcv_context context;
    char path[MAXPATHLEN] = {0};
    char original[MAXPATHLEN] = {0};
    struct bu_vls missing = BU_VLS_INIT_ZERO;
    const char *invalid[] = {
	"", "unrelated text\n",
	PLY_TEST_HEADER("int") "1 2 3\n4 2 3\n1 5 3\n3 0 1 99\n",
	PLY_TEST_HEADER("int") "1 2 3\n4 2 3\n1 5 3\n3 0 1 -1\n",
	PLY_TEST_HEADER("float") "1 2 3\n4 2 3\n1 5 3\n3 0 1 0.25\n",
	PLY_TEST_HEADER("double") "1 2 3\n4 2 3\n1 5 3\n3 0 1 1e-100\n",
	PLY_TEST_HEADER("int") "nan 2 3\n4 2 3\n1 5 3\n3 0 1 2\n",
	PLY_TEST_HEADER("int") "1 2 3\n4 2 3\n1 5 3\n3 0 1\n",
	PLY_TEST_HEADER("int") "1 2 3\n4 2 3\n1 5 3\n2 0 1\n",
	("ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float z\n"
	"element face 1\nproperty list uchar int vertex_indices\nend_header\n1 3\n4 3\n1 3\n3 0 1 2\n")
    };
    int failed = 0;
    gcv_context_init(&context);
    if (!write_fixture(original, ordered) || read_fixture(&context, original, "1") != 1 ||
	db_update_ident(context.dbip, "keep title", 10) < 0) {
	failed = 1;
	goto done;
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
	if (!write_fixture(path, invalid[i])) {
	    failed = 1;
	    goto done;
	}
	if (read_fixture(&context, path, "1") != 0 || mesh_count(&context) != 1 ||
	    !BU_STR_EQUAL(context.dbip->dbi_title, "keep title") || !EQUAL(context.dbip->dbi_local2base, 10)) {
	    bu_log("PLY reader accepted invalid input or changed the destination for case %zu\n", i);
	    failed = 1;
	}
	bu_file_delete(path);
	path[0] = '\0';
    }
    bu_vls_printf(&missing, "%s/missing.ply", original);
    if (read_fixture(&context, bu_vls_cstr(&missing), "1") != 0) {
	bu_log("PLY reader did not return failure for an input-open error\n");
	failed = 1;
    }
    if (!check_triangle(&context, original, 1) || !write_fixture(path, ordered) ||
	read_fixture(&context, path, "2") != 1 || !check_triangle(&context, path, 2)) {
	bu_log("PLY reader did not preserve geometry or recover after input failure\n");
	failed = 1;
    }
done:
    if (path[0]) bu_file_delete(path);
    if (original[0]) bu_file_delete(original);
    bu_vls_free(&missing);
    gcv_context_destroy(&context);
    return failed;
}

static int
topology_test(void)
{
    struct gcv_context context;
    char path[MAXPATHLEN] = {0};
    const fastf_t vertices[] = {1, 2, 3, 4, 2, 3, 4, 5, 3, 1, 5, 3, 1, 2, 6};
    const int faces[] = {0, 1, 2, 0, 2, 3, 0, 3, 4};
    const char *text =
	"ply\nformat ascii 1.0\nelement vertex 5\nproperty float x\nproperty float y\nproperty float z\n"
	"element face 2\nproperty list uchar int vertex_indices\nend_header\n"
	"1 2 3\n4 2 3\n4 5 3\n1 5 3\n1 2 6\n4 0 1 2 3\n3 0 3 4\n";
    int failed;
    gcv_context_init(&context);
    failed = !write_fixture(path, text) || read_fixture(&context, path, "1") != 1 ||
	!check_mesh(&context, path, 5, vertices, 3, faces, 1);
    if (path[0]) bu_file_delete(path);
    gcv_context_destroy(&context);
    return failed;
}

static int
names_test(void)
{
    struct gcv_context context;
    char path[MAXPATHLEN] = {0};
    const char *scales[] = {"1", "2", "3"};
    struct bu_vls collision = BU_VLS_INIT_ZERO;
    struct bu_external before = BU_EXTERNAL_INIT_ZERO;
    struct bu_external after = BU_EXTERNAL_INIT_ZERO;
    struct directory *entry;
    struct rt_wdb *writer;
    point_t center = {0, 0, 0};
    int failed = 0;
    gcv_context_init(&context);
    if (!write_fixture(path, ordered)) {
	failed = 1;
	goto done;
    }
    bu_path_component(&collision, path, BU_PATH_BASENAME_EXTLESS);
    bu_vls_strcat(&collision, ".bot.r");
    writer = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
    if (!writer || mk_sph(writer, bu_vls_cstr(&collision), center, 5) < 0) {
	failed = 1;
	goto done;
    }
    entry = db_lookup(context.dbip, bu_vls_cstr(&collision), LOOKUP_QUIET);
    if (!entry || db_get_external(&before, entry, context.dbip) < 0) {
	failed = 1;
	goto done;
    }
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
	if (read_fixture(&context, path, scales[i]) != 1 || mesh_count(&context) != i + 1 ||
	    !check_triangle(&context, path, 1)) {
	    bu_log("PLY import reused an existing mesh name\n");
	    failed = 1;
	}
	entry = db_lookup(context.dbip, bu_vls_cstr(&collision), LOOKUP_QUIET);
	if (!entry || db_get_external(&after, entry, context.dbip) < 0 ||
	    before.ext_nbytes != after.ext_nbytes || memcmp(before.ext_buf, after.ext_buf, before.ext_nbytes)) {
	    bu_log("PLY import overwrote an existing object with a generated region\n");
	    failed = 1;
	}
	bu_free_external(&after);
    }
    db_update_nref(context.dbip);
    FOR_ALL_DIRECTORY_START(entry, context.dbip) {
	if (entry->d_major_type == DB5_MAJORTYPE_BRLCAD && entry->d_minor_type == ID_BOT && entry->d_nref != 1) {
	    bu_log("A generated PLY region does not reference its own mesh\n");
	    failed = 1;
	}
    } FOR_ALL_DIRECTORY_END;
done:
    bu_free_external(&before);
    bu_free_external(&after);
    bu_vls_free(&collision);
    if (path[0]) bu_file_delete(path);
    gcv_context_destroy(&context);
    return failed;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2) return 1;
    if (BU_STR_EQUAL(argv[1], "state")) return state_test();
    if (BU_STR_EQUAL(argv[1], "errors")) return errors_test();
    if (BU_STR_EQUAL(argv[1], "topology")) return topology_test();
    if (BU_STR_EQUAL(argv[1], "names")) return names_test();
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
