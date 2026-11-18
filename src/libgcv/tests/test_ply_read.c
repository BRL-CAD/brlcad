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

static const char ordered[] =
    "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
    "element face 1\nproperty list uchar int vertex_indices\nend_header\n"
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
check_triangle(struct gcv_context *context, const char *path, double scale)
{
    const fastf_t expected[] = {1, 2, 3, 4, 2, 3, 1, 5, 3};
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
	    valid = bot->num_vertices == 3 && bot->num_faces == 1;
	    if (valid) {
		for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
		    if (!EQUAL(bot->vertices[i], expected[i] * scale)) valid = 0;
		for (int i = 0; i < 3; ++i)
		    if (bot->faces[i] != i) valid = 0;
	    }
	}
	rt_db_free_internal(&object);
    }
    if (!valid) bu_log("PLY import changed triangle geometry for %s\n", bu_vls_cstr(&name));
    bu_vls_free(&name);
    return valid;
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

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2) return 1;
    if (BU_STR_EQUAL(argv[1], "state")) return state_test();
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
