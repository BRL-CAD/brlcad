/* R H I N O _ P R O F I L E S . C P P
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
#include <string>
#include <vector>

#include "opennurbs.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "gcv/api.h"
#include "rt/search.h"
#include "wdb.h"

#define RHINO_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("Rhino profile check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static bool
import_profile(const char *filename, const bu_mime_traits *profile, bool expected, bool direct = false)
{
    gcv_context context;
    gcv_opts options;
    point_t center = VINIT_ZERO;
    gcv_context_init(&context);
    gcv_opts_default(&options);
    rt_wdb *writer = wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM);
    bool valid = writer && !mk_id_units(writer, "preserved title", "mm") && !mk_sph(writer, "preserved.s", center, 1);
    const gcv_filter *reader = gcv_select_filter(&context, GCV_FILTER_READ, "model/vnd.mcneel.3dm", "Rhino Reader", NULL);
    valid = valid && reader;
    if (valid) {
	bool imported = direct ? reader->filter_fn(&context, &options, NULL, filename) :
	    gcv_execute_variant(&context, reader, "model/vnd.mcneel.3dm", profile, &options, 0, NULL, filename);
	size_t count = db_ls(context.dbip, DB_LS_PRIM, NULL, NULL);
	valid = imported == expected && BU_STR_EQUAL(context.dbip->dbi_title, "preserved title") &&
	    db_lookup(context.dbip, "preserved.s", LOOKUP_QUIET) && (expected ? count > 1 :
		count == 1 && !db_ls(context.dbip, DB_LS_COMB, NULL, NULL) &&
		EQUAL(context.dbip->dbi_local2base, 1.0) && EQUAL(context.dbip->dbi_base2local, 1.0));
	if (!valid)
	    bu_log("3DM import returned %d (expected %d), primitive count %zu\n", imported, expected, count);
    }
    gcv_context_destroy(&context);
    return valid;
}

static bool
archive_version(const char *filename, int version)
{
    /* Change the declaration independently of the known body to exercise
     * historical V1/V2 repair and future archive input. */
    char field[9];
    if (snprintf(field, sizeof(field), "%8d", version) != 8)
	return false;
    FILE *stream = fopen(filename, "r+b");
    if (!stream)
	return false;
    bool valid = !bu_fseek(stream, sizeof("3D Geometry File Format ") - 1, SEEK_SET) &&
	fwrite(field, 1, sizeof(field) - 1, stream) == sizeof(field) - 1;
    if (fclose(stream))
	valid = false;
    return valid;
}

static bool
prepend(const char *source, const char *destination, size_t count)
{
    FILE *input = fopen(source, "rb");
    FILE *output = fopen(destination, "wb");
    bool valid = input && output;
    const std::vector<unsigned char> padding(count, 0);
    if (valid && count)
	valid = fwrite(padding.data(), 1, count, output) == count;
    unsigned char buffer[4096];
    size_t size;
    while (valid && (size = fread(buffer, 1, sizeof(buffer), input)))
	valid = fwrite(buffer, 1, size, output) == size;
    if (input) {
	if (ferror(input))
	    valid = false;
	if (fclose(input))
	    valid = false;
    }
    if (output && fclose(output))
	valid = false;
    return valid;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    ON::Begin();
    ONX_Model model;
    ON_Mesh mesh;
    mesh.SetVertex(0, ON_3fPoint(0, 0, 0));
    mesh.SetVertex(1, ON_3fPoint(1, 0, 0));
    mesh.SetVertex(2, ON_3fPoint(0, 1, 0));
    mesh.SetVertex(3, ON_3fPoint(0, 0, 1));
    mesh.SetTriangle(0, 0, 2, 1);
    mesh.SetTriangle(1, 0, 1, 3);
    mesh.SetTriangle(2, 1, 2, 3);
    mesh.SetTriangle(3, 2, 0, 3);
    RHINO_CHECK(!model.AddModelGeometryComponent(&mesh, NULL).IsEmpty());
    char filename[MAXPATHLEN], wrapped[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    RHINO_CHECK(stream && !fclose(stream));
    stream = bu_temp_file(wrapped, sizeof(wrapped));
    RHINO_CHECK(stream && !fclose(stream));
    std::vector<int> versions = {2, 3, 4};
    for (int version = 50; version <= ON_BinaryArchive::CurrentArchiveVersion(); version += 10)
	versions.push_back(version);
    for (int version : versions) {
	const std::string text = std::to_string(version);
	bu_mime_trait fact = BU_MIME_TRAIT_STRING("version", text.c_str());
	bu_mime_traits profile = {&fact, 1};
	bu_log("Checking 3DM archive version %s\n", text.c_str());
	RHINO_CHECK(model.Write(filename, version));
	RHINO_CHECK(import_profile(filename, &profile, true));
    }
    /* Wrapped input leaves version unknown to bounded selection; only the
     * native reader can establish or contradict the asserted profile. */
    RHINO_CHECK(model.Write(filename, 2));
    RHINO_CHECK(prepend(filename, wrapped, BU_MIME_DEFAULT_SCAN_LIMIT + 1));
    const bu_mime_trait requests[] = {
	BU_MIME_TRAIT_STRING("version", "2"), BU_MIME_TRAIT_STRING("version", "3"),
	BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("byte-order", "big-endian"),
	BU_MIME_TRAIT_STRING("rhino.unestablished", "assertion")};
    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); ++i) {
	bu_mime_traits profile = {&requests[i], 1};
	RHINO_CHECK(import_profile(wrapped, &profile, i == 0));
    }
    RHINO_CHECK(import_profile(wrapped, NULL, true));
    RHINO_CHECK(model.Write(filename, 2));
    RHINO_CHECK(archive_version(filename, 1));
    {
	bu_mime_trait version = BU_MIME_TRAIT_STRING("version", "1");
	bu_mime_traits profile = {&version, 1};
	RHINO_CHECK(import_profile(filename, &profile, true));
    }
    /* Future and undefined archive numbers are recognizable facts but are
     * outside this SDK's declarations, including direct legacy execution. */
    RHINO_CHECK(model.Write(filename, 0));
    RHINO_CHECK(archive_version(filename, ON_BinaryArchive::CurrentArchiveVersion() + 10));
    RHINO_CHECK(import_profile(filename, NULL, false));
    RHINO_CHECK(import_profile(filename, NULL, false, true));
    RHINO_CHECK(prepend(filename, wrapped, BU_MIME_DEFAULT_SCAN_LIMIT + 1));
    RHINO_CHECK(import_profile(wrapped, NULL, false));
    RHINO_CHECK(archive_version(filename, 51));
    RHINO_CHECK(import_profile(filename, NULL, false, true));
    RHINO_CHECK(bu_file_delete(filename) && bu_file_delete(wrapped));
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
