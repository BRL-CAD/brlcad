/*                    T E S T _ P L Y _ W R I T E . C
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
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "gcv.h"
#include "raytrace.h"
#include "wdb.h"

#ifdef GCV_PLY_TEST_WRITE_FAILURE
#include "../plugins/ply/rply.h"

enum ply_test_failure { PLY_TEST_NONE, PLY_TEST_HEADER, PLY_TEST_VALUE, PLY_TEST_CLOSE };
static enum ply_test_failure ply_test_failure;
static int ply_test_injected;

static int
ply_test_write_header(p_ply output)
{
    if (ply_test_failure == PLY_TEST_HEADER) {
	ply_test_failure = PLY_TEST_NONE;
	ply_test_injected = 1;
	return 0;
    }
    return ply_write_header(output);
}

static int
ply_test_write_value(p_ply output, double value)
{
    if (ply_test_failure == PLY_TEST_VALUE) {
	ply_test_failure = PLY_TEST_NONE;
	ply_test_injected = 1;
	return 0;
    }
    return ply_write(output, value);
}

static int
ply_test_close(p_ply output)
{
    int status = ply_close(output);
    if (ply_test_failure == PLY_TEST_CLOSE) {
	ply_test_failure = PLY_TEST_NONE;
	ply_test_injected = 1;
	return 0;
    }
    return status;
}

/* A successful close must not hide an earlier library write failure. */
#define ply_write_header ply_test_write_header
#define ply_write ply_test_write_value
#define ply_close ply_test_close
#include "../plugins/ply/ply_write.c"
#undef ply_write
#undef ply_write_header
#undef ply_close
#endif

static const struct gcv_filter *
select_writer(struct gcv_context *context)
{
#ifdef GCV_PLY_TEST_WRITE_FAILURE
    (void)context;
    return &gcv_conv_ply_write;
#else
    return find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_PLY, NULL, context);
#endif
}

static const char *
option_string(const struct bu_opt_desc *options, const char *shortopt)
{
    for (const struct bu_opt_desc *option = options; option->shortopt; ++option)
	if (BU_STR_EQUAL(option->shortopt, shortopt))
	    return *(const char * const *)option->set_var;
    return NULL;
}

static int
check_options(const struct gcv_filter *writer, int long_options)
{
    struct bu_opt_desc *options = NULL;
    void *data = NULL;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const char *arguments[] = {"-o", "mesh.ply", "-x", "1", "-X", "2"};
    const char *long_arguments[] = {"--output_file", "mesh.ply", "--rt_debug", "1", "--nmg_debug", "2"};
    writer->create_opts_fn(&options, &data);
    int valid = options && data;
    if (valid) {
	valid = bu_opt_parse(&messages, sizeof(arguments) / sizeof(arguments[0]),
	    long_options ? long_arguments : arguments, options) == 0;
	const char *output = option_string(options, "o");
	const char *rt_flags = option_string(options, "x");
	const char *nmg_flags = option_string(options, "X");
	valid = valid && output && BU_STR_EQUAL(output, "mesh.ply") &&
	    rt_flags && BU_STR_EQUAL(rt_flags, "1") && nmg_flags && BU_STR_EQUAL(nmg_flags, "2");
    }
    if (!valid)
	bu_log("PLY debug options changed the output path or lost their values: %s\n", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    bu_free(options, "PLY option descriptions");
    writer->free_opts_fn(data);
    return valid;
}


enum { TETRA_VERTICES = 4, TETRA_FACES = 4, TETRA_COUNT = 2 };

static const char *const region_names[TETRA_COUNT] = {"first.r", "second.r"};

static const fastf_t tetrahedra[TETRA_COUNT * TETRA_VERTICES * 3] = {
    1000, 0, 0, 0, 1000, 0, 0, 0, 1000, 0, 0, 0,
    4000, 0, 0, 3000, 1000, 0, 3000, 0, 1000, 3000, 0, 0
};

static int
create_geometry(struct gcv_context *context, int joined)
{
    const char *meshes[] = {"first.bot", "second.bot"};
    const int tetra_faces[] = {0, 1, 2, 3, 0, 2, 3, 2, 1, 3, 1, 0};
    struct rt_wdb *database = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!database) return 0;
    for (size_t region = 0; region < sizeof(meshes) / sizeof(meshes[0]); ++region) {
	fastf_t vertices[TETRA_VERTICES * 3];
	int faces[TETRA_FACES * 3];
	memcpy(vertices, tetrahedra + region * TETRA_VERTICES * 3, sizeof(vertices));
	memcpy(faces, tetra_faces, sizeof(faces));
	if (mk_bot(database, meshes[region], RT_BOT_SOLID, RT_BOT_CCW, 0,
		TETRA_VERTICES, TETRA_FACES, vertices, faces, NULL, NULL) < 0)
	    goto fail;
	if (joined) {
	    if (!mk_addmember(meshes[region], &members.l, NULL, WMOP_UNION)) goto fail;
	} else if (mk_comb1(database, region_names[region], meshes[region], 1) < 0) {
	    goto fail;
	}
    }
    if (joined)
	return mk_lcomb(database, "joined.r", &members, 1, NULL, NULL, NULL, 0) >= 0;
    return 1;
fail:
    mk_freemembers(&members.l);
    return 0;
}

static int
check_geometry(struct gcv_context *context, size_t first_region, size_t region_count)
{
    const double coordinate_tolerance = 1.0e-3;
    struct directory *entry;
    size_t meshes = 0;
    int valid = 1;
    FOR_ALL_DIRECTORY_START(entry, context->dbip) {
	if (entry->d_major_type != DB5_MAJORTYPE_BRLCAD || entry->d_minor_type != ID_BOT)
	    continue;
	struct rt_db_internal object;
	++meshes;
	if (rt_db_get_internal(&object, entry, context->dbip, NULL) < 0) return 0;
	const struct rt_bot_internal *bot = (const struct rt_bot_internal *)object.idb_ptr;
	unsigned matched = 0;
	if (bot->num_vertices != region_count * TETRA_VERTICES || bot->num_faces != region_count * TETRA_FACES) valid = 0;
	for (size_t vertex = 0; valid && vertex < bot->num_vertices; ++vertex) {
	    int found = 0;
	    for (size_t expected = first_region * TETRA_VERTICES;
		expected < (first_region + region_count) * TETRA_VERTICES; ++expected) {
		if (VNEAR_EQUAL(bot->vertices + vertex * 3, tetrahedra + expected * 3, coordinate_tolerance)) {
		    unsigned bit = 1U << expected;
		    if (matched & bit) valid = 0;
		    matched |= bit;
		    found = 1;
		    break;
		}
	    }
	    if (!found) valid = 0;
	}
	double volume = 0;
	for (size_t face = 0; valid && face < bot->num_faces; ++face) {
	    const int *indices = bot->faces + face * 3;
	    for (size_t corner = 0; corner < 3; ++corner)
		if (indices[corner] < 0 || (size_t)indices[corner] >= bot->num_vertices) valid = 0;
	    if (!valid) break;
	    vect_t cross;
	    VCROSS(cross, bot->vertices + indices[1] * 3, bot->vertices + indices[2] * 3);
	    volume += VDOT(bot->vertices + indices[0] * 3, cross);
	}
	/* Each outward-oriented tetrahedron encloses 1e9 / 6 cubic mm. */
	if (!NEAR_EQUAL(volume, region_count * 1.0e9, 1.0)) valid = 0;
	rt_db_free_internal(&object);
    } FOR_ALL_DIRECTORY_END;
    return valid && meshes == 1;
}

static int
read_export(const char *path, const char *type, size_t first_region, size_t region_count)
{
    const char *format = BU_STR_EQUAL(type, "asc") ? "format ascii 1.0\n" :
	(BU_STR_EQUAL(type, "le") ? "format binary_little_endian 1.0\n" : "format binary_big_endian 1.0\n");
    char header[sizeof("format binary_little_endian 1.0\n")];
    FILE *file = fopen(path, "rb");
    int valid = file && bu_fgets(header, sizeof(header), file) && BU_STR_EQUAL(header, "ply\n") &&
	bu_fgets(header, sizeof(header), file) && BU_STR_EQUAL(header, format);
    if (file && fclose(file)) valid = 0;
    struct gcv_context imported;
    gcv_context_init(&imported);
    const struct gcv_filter *reader = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_PLY, NULL, &imported);
    valid = valid && reader && gcv_execute(&imported, reader, NULL, 0, NULL, path) == 1 &&
	check_geometry(&imported, first_region, region_count);
    gcv_context_destroy(&imported);
    return valid;
}

static int
round_trip(struct gcv_context *source, const struct gcv_filter *writer, const char *path, const char *type)
{
    const char *arguments[] = {"-t", type};
    int valid = gcv_execute(source, writer, NULL, 2, arguments, path) == 1 &&
	read_export(path, type, 0, TETRA_COUNT);
    if (!valid) bu_log("PLY %s export/import changed geometry or failed\n", type);
    return valid;
}

static int
geometry_test(int joined)
{
    struct gcv_context source;
    char path[MAXPATHLEN] = {0};
    const char *types[] = {"asc", "le", "be"};
    int valid;
    gcv_context_init(&source);
    valid = create_geometry(&source, joined);
    FILE *file = bu_temp_file(path, sizeof(path));
    if (!file) valid = 0;
    else if (fclose(file)) valid = 0;
    const struct gcv_filter *writer = select_writer(&source);
    if (!writer) valid = 0;
    for (size_t mode = 0; valid && mode < sizeof(types) / sizeof(types[0]); ++mode)
	valid = round_trip(&source, writer, path, types[mode]);
    if (path[0]) bu_file_delete(path);
    gcv_context_destroy(&source);
    return valid ? 0 : 1;
}

static void
separate_output_path(struct bu_vls *output, const char *path, size_t region)
{
    struct bu_vls directory = BU_VLS_INIT_ZERO;
    struct bu_vls basename = BU_VLS_INIT_ZERO;
    bu_path_component(&directory, path, BU_PATH_DIRNAME);
    bu_path_component(&basename, path, BU_PATH_BASENAME);
    bu_vls_printf(output, "%s/%s_%s", bu_vls_cstr(&directory), region_names[region], bu_vls_cstr(&basename));
    bu_vls_free(&directory);
    bu_vls_free(&basename);
}

static int
separate_test(void)
{
    struct gcv_context source;
    char path[MAXPATHLEN] = {0};
    const char *types[] = {"asc", "le", "be"};
    gcv_context_init(&source);
    int valid = create_geometry(&source, 0);
    FILE *file = bu_temp_file(path, sizeof(path));
    if (!file) valid = 0;
    else if (fclose(file)) valid = 0;
    const struct gcv_filter *writer = select_writer(&source);
    if (!writer) valid = 0;
    for (size_t mode = 0; valid && mode < sizeof(types) / sizeof(types[0]); ++mode) {
	const char *arguments[] = {"-s", "-t", types[mode]};
	valid = gcv_execute(&source, writer, NULL, 3, arguments, path) == 1;
	for (size_t region = 0; region < TETRA_COUNT; ++region) {
	    struct bu_vls output = BU_VLS_INIT_ZERO;
	    separate_output_path(&output, path, region);
	    if (!read_export(bu_vls_cstr(&output), types[mode], region, 1)) {
		bu_log("PLY separate output missing or incorrect: %s\n", bu_vls_cstr(&output));
		valid = 0;
	    }
	    bu_file_delete(bu_vls_cstr(&output));
	    bu_vls_free(&output);
	}
    }
    if (path[0]) bu_file_delete(path);
    gcv_context_destroy(&source);
    return valid ? 0 : 1;
}

static int
failure_test(const char *mode, const char *full_device)
{
    struct gcv_context source;
    struct gcv_opts options;
    struct bu_vls unavailable = BU_VLS_INIT_ZERO;
    char path[MAXPATHLEN] = {0};
    const char *arguments[] = {"-t", "asc", "-s"};
    int separate = strstr(mode, "separate") != NULL;
    const char *missing_object[] = {"missing.r"};
    const char marker[] = "existing output\n";
    gcv_context_init(&source);
    gcv_opts_default(&options);
    int empty = BU_STR_EQUAL(mode, "empty");
    int valid = empty || create_geometry(&source, 0);
    FILE *file = bu_temp_file(path, sizeof(path));
    if (!file) valid = 0;
    else {
	int written = fwrite(marker, 1, sizeof(marker) - 1, file) == sizeof(marker) - 1;
	int closed = fclose(file) == 0;
	if (!written || !closed) valid = 0;
    }
    const struct gcv_filter *writer = select_writer(&source);
    if (!writer) valid = 0;
    const char *target = path;
    if (BU_STR_EQUAL(mode, "open") || BU_STR_EQUAL(mode, "separate-open")) {
	/* A regular file cannot serve as the parent directory. */
	bu_vls_printf(&unavailable, "%s/mesh.ply", path);
	target = bu_vls_cstr(&unavailable);
    } else if (BU_STR_EQUAL(mode, "type")) {
	arguments[1] = "unsupported";
    } else if (BU_STR_EQUAL(mode, "objects")) {
	options.object_names = missing_object;
	options.num_objects = 1;
    } else if (empty) {
	options.verbosity_level = 1;
    } else if (BU_STR_EQUAL(mode, "close") && full_device) {
	target = full_device;
#ifdef GCV_PLY_TEST_WRITE_FAILURE
    } else if (BU_STR_EQUAL(mode, "header") || BU_STR_EQUAL(mode, "header-separate")) {
	ply_test_failure = PLY_TEST_HEADER;
    } else if (BU_STR_EQUAL(mode, "value") || BU_STR_EQUAL(mode, "value-separate")) {
	ply_test_failure = PLY_TEST_VALUE;
    } else if (BU_STR_EQUAL(mode, "close-separate")) {
	ply_test_failure = PLY_TEST_CLOSE;
#endif
    } else {
	valid = 0;
    }
    if (valid && gcv_execute(&source, writer, &options, separate ? 3 : 2, arguments, target) != 0) {
	bu_log("PLY writer did not reject %s failure\n", mode);
	valid = 0;
    }
#ifdef GCV_PLY_TEST_WRITE_FAILURE
    if (valid && !ply_test_injected) {
	bu_log("PLY write failure was not exercised\n");
	valid = 0;
    }
#endif
    if (valid && (empty || BU_STR_EQUAL(mode, "objects") || BU_STR_EQUAL(mode, "type"))) {
	char contents[sizeof(marker)] = {0};
	file = fopen(path, "rb");
	valid = file && fread(contents, 1, sizeof(marker) - 1, file) == sizeof(marker) - 1 &&
	    BU_STR_EQUAL(contents, marker) && fgetc(file) == EOF && !ferror(file);
	if (file && fclose(file)) valid = 0;
	if (!valid) bu_log("PLY writer changed existing output before rejecting %s\n", mode);
    }
    if (valid && empty) valid = create_geometry(&source, 0);
    if (valid) valid = round_trip(&source, writer, path, "asc");
    if (separate) {
	for (size_t region = 0; region < TETRA_COUNT; ++region) {
	    struct bu_vls output = BU_VLS_INIT_ZERO;
	    separate_output_path(&output, path, region);
	    bu_file_delete(bu_vls_cstr(&output));
	    bu_vls_free(&output);
	}
    }
    if (path[0]) bu_file_delete(path);
    bu_vls_free(&unavailable);
    gcv_context_destroy(&source);
    return valid ? 0 : 1;
}

int
main(int argc, const char **argv)
{
    struct gcv_context context;
    bu_setprogname(argv[0]);
    if (argc == 2 && BU_STR_EQUAL(argv[1], "geometry")) return geometry_test(0);
    if (argc == 2 && BU_STR_EQUAL(argv[1], "boolean")) return geometry_test(1);
    if (argc == 2 && BU_STR_EQUAL(argv[1], "separate")) return separate_test();
    if (argc == 2 && (BU_STR_EQUAL(argv[1], "open") || BU_STR_EQUAL(argv[1], "type") ||
	    BU_STR_EQUAL(argv[1], "objects") || BU_STR_EQUAL(argv[1], "empty")))
	return failure_test(argv[1], NULL);
#ifdef GCV_PLY_TEST_WRITE_FAILURE
    if (argc == 2 && (BU_STR_EQUAL(argv[1], "header") || BU_STR_EQUAL(argv[1], "value") ||
	    BU_STR_EQUAL(argv[1], "header-separate") || BU_STR_EQUAL(argv[1], "value-separate") ||
	    BU_STR_EQUAL(argv[1], "close-separate")))
	return failure_test(argv[1], NULL);
#endif
    if (argc == 2 && BU_STR_EQUAL(argv[1], "separate-open")) return failure_test(argv[1], NULL);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "close")) return failure_test(argv[1], argv[2]);
    if (argc != 1) return 1;
    gcv_context_init(&context);
    const struct gcv_filter *writer = select_writer(&context);
    int valid = writer && writer->create_opts_fn && writer->free_opts_fn;
    if (valid)
	valid = check_options(writer, 0) && check_options(writer, 1);
    gcv_context_destroy(&context);
    return valid ? 0 : 1;
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
