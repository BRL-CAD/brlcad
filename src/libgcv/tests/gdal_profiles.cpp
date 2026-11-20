/* G D A L _ P R O F I L E S . C P P
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
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <locale>
#include <vector>
#include <set>
#include <sstream>
#include <string>
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "gcv/api.h"
#include "wdb.h"
#include "../plugins/gdal/format_map.h"
#include "../../libbu/json.hpp"
#include "../../libbu/mime_binary.h"
#include "../../libbu/tests/mime_tar_fixture.h"
#include "../../libbu/tests/mime_heightfield_fixture.h"
#include "../../libbu/tests/mime_raster_label_fixture.h"
#include "../../libbu/mime_raster_label.h"
#include "../../libbu/tests/mime_science_fixture.h"
#include "../../libbu/tests/mime_product_fixture.h"
#include "../../libbu/mime_esric.h"
#include "../../libbu/mime_xml.h"
#include "../../libbu/mime_noaa_grid.h"
#include "../../libbu/mime_nsidc.h"
#include "../../libbu/mime_iris.h"
#include "../../libbu/mime_sigdem.h"
#include "../../libbu/mime_isg.h"
#include "../../libbu/mime_hf2.h"

/* vmath supplies BRL-CAD's floating-point EQUAL before GDAL headers. */
#include <gdal.h>
#include <gdal_utils.h>
#include <ogr_spatialref.h>
#include <cpl_conv.h>
#include <cpl_string.h>
#include <cpl_minixml.h>

#define GDAL_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("GDAL profile check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static int
inspection_only_conversion(gcv_context *, const gcv_opts *, const void *, const char *)
{
    return 0;
}

static int
png_header_observation(const bu_mime_probe_input *input, const char *mime_type, bu_mime_traits *traits)
{
    const unsigned char magic[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (BU_STR_EQUAL(mime_type, "image/png") && input->size >= sizeof(magic) &&
	!memcmp(input->data, magic, sizeof(magic)))
	return bu_mime_traits_add(traits, "fixture.png.header", "present");
    return 1;
}

static const gcv_conversion inspection_conversion = {
    "PNG inspection control", GCV_FILTER_READ, NULL, NULL, inspection_only_conversion
};
static const bu_mime_capability inspection_format = BU_MIME_CAPABILITY("image/png", NULL, 0, 0);
static const gcv_handler inspection_handler = {&inspection_conversion, &inspection_format, 1, 0, NULL, png_header_observation};
static const gcv_plugin_v2 inspection_plugin = {
    GCV_PLUGIN_ABI_VERSION, sizeof(gcv_plugin_v2), NULL, 0, &inspection_handler, 1
};

static bool
import_profile(const char *filename, const char *type, const bu_mime_traits *profile, bool expected, const char *input_format = NULL, const gcv_plugin_v2 *plugin = NULL)
{
    gcv_context context;
    gcv_context_init(&context);
    const bool registered = !plugin || gcv_register_plugin(&context, plugin, NULL);
    rt_wdb *writer = registered ? wdb_dbopen(context.dbip, RT_WDB_TYPE_DB_INMEM) : NULL;
    const char title[] = "Preserve metadata on rejected raster profile";
    point_t center = VINIT_ZERO;
    bool valid = writer && !mk_id_units(writer, title, "cm") && !mk_sph(writer, "retained.s", center, 1);
    bu_vls resolved = BU_VLS_INIT_ZERO;
    const gcv_filter *reader = input_format ?
	gcv_resolve_variant(&context, GCV_FILTER_READ, filename, input_format, profile, "GDAL Reader", &resolved, NULL, NULL) :
	gcv_select_filter(&context, GCV_FILTER_READ, type, "GDAL Reader", NULL);
    valid = valid && reader && (!input_format || BU_STR_EQUAL(bu_vls_cstr(&resolved), type));
    bu_vls_free(&resolved);
    gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
    if (valid) {
	const gcv_conversion_status status = gcv_execute_result(&context, reader, type, profile,
	    NULL, 0, NULL, filename, &result);
	const bool imported = status == GCV_CONVERSION_COMPLETE || status == GCV_CONVERSION_PARTIAL;
	struct directory **objects = NULL;
	const size_t count = db_ls(context.dbip, DB_LS_PRIM, NULL, &objects);
	bool has_dsp = false;
	for (size_t i = 0; i < count; ++i)
	    if (objects[i]->d_minor_type == DB5_MINORTYPE_BRLCAD_DSP) has_dsp = true;
	bu_free(objects, "raster import primitives");
	valid = imported == expected && BU_STR_EQUAL(context.dbip->dbi_title, title) &&
	    EQUAL(context.dbip->dbi_local2base, 10.0) && EQUAL(context.dbip->dbi_base2local, 0.1) &&
	    db_lookup(context.dbip, "retained.s", LOOKUP_QUIET) && (expected ? count > 1 && has_dsp : count == 1 && !has_dsp);
	if (!valid) bu_log("GDAL import status %d, expected success %d, primitives %zu\n", status, expected, count);
    }
    gcv_conversion_result_free(&result);
    gcv_context_destroy(&context);
    return valid;
}

static bool
read_fixture_grid(GDALDatasetH source, double *transform, std::vector<unsigned char> &samples)
{
    const int columns = GDALGetRasterXSize(source), rows = GDALGetRasterYSize(source);
    if (columns <= 0 || rows <= 0) return false;
    samples.resize(static_cast<size_t>(columns) * rows);
    return GDALGetGeoTransform(source, transform) == CE_None &&
	GDALRasterIO(GDALGetRasterBand(source, 1), GF_Read, 0, 0, columns, rows,
	    samples.data(), columns, rows, GDT_Byte, 0, 0) == CE_None;
}

static bool
write_gxf_fixture(const char *filename, GDALDatasetH source, char **options)
{
    const int columns = GDALGetRasterXSize(source), rows = GDALGetRasterYSize(source);
    double transform[6];
    std::vector<unsigned char> samples;
    if (!read_fixture_grid(source, transform, samples)) return false;
    const char *gtype = CSLFetchNameValueDef(options, "GTYPE", "0");
    const unsigned digits = BU_STR_EQUAL(gtype, "0") ? 0 : BU_STR_EQUAL(gtype, "1") ? 1 : 2;
    const char *separator = CSLFetchNameValue(options, "INLINE_GTYPE") ? " " : "\n";
    if (digits == 2 && !BU_STR_EQUAL(gtype, "2")) return false;
    std::ofstream grid(filename, std::ios::binary);
    grid.imbue(std::locale::classic());
    grid.precision(std::numeric_limits<double>::max_digits10);
    grid << "#TITLE\nOriginal grid control\n#POINTS\n" << columns << "\n#ROWS\n" << rows
	<< "\n#PTSEPARATION\n" << transform[1] << "\n#RWSEPARATION\n" << -transform[5]
	<< "\n#XORIGIN\n" << transform[0] + transform[1] / 2 << "\n#YORIGIN\n" << transform[3] + transform[5] / 2
	<< "\n#SENSE\n-2\n#GTYPE" << separator << gtype
	<< "\n#MAP_PROJECTION\n\"Geographic\"\n\"WGS 84\",6378137,0.0818191908426215\n\"Geographic\"\n#GRID\n";
    constexpr unsigned radix = 90;
    constexpr char digit_zero = '%';
    for (int row = 0; row < rows; ++row) {
	for (int column = 0; column < columns; ++column) {
	    const unsigned value = samples[static_cast<size_t>(row) * columns + column];
	    if (!digits) grid << value << ' ';
	    else {
		if (digits == 1 && value >= radix) return false;
		if (digits == 2) grid.put(static_cast<char>(digit_zero + value / radix));
		grid.put(static_cast<char>(digit_zero + value % radix));
	    }
	}
	grid << '\n';
    }
    grid.close();
    return !grid.fail();
}

static bool
write_dted_fixture(const char *filename, GDALDatasetH source)
{
    // Level 0 cells below 50 degrees latitude have 121 samples per axis.
    constexpr int side = 121;
    constexpr double spacing = 1.0 / (side - 1);
    double original_transform[6];
    std::vector<unsigned char> original;
    if (!read_fixture_grid(source, original_transform, original)) return false;
    std::vector<int16_t> samples(static_cast<size_t>(side) * side);
    for (size_t i = 0; i < samples.size(); ++i) samples[i] = original[i % original.size()];
    double transform[] = {10.0 - spacing / 2, spacing, 0, 41.0 + spacing / 2, 0, -spacing};
    GDALDatasetH grid = GDALCreate(GDALGetDriverByName("MEM"), "", side, side, 1, GDT_Int16, NULL);
    if (!grid) return false;
    bool valid = GDALSetGeoTransform(grid, transform) == CE_None &&
	GDALSetProjection(grid, GDALGetProjectionRef(source)) == CE_None &&
	GDALRasterIO(GDALGetRasterBand(grid, 1), GF_Write, 0, 0, side, side,
	    samples.data(), side, side, GDT_Int16, 0, 0) == CE_None;
    GDALDatasetH output = valid ? GDALCreateCopy(GDALGetDriverByName("DTED"), filename, grid, FALSE, NULL, NULL, NULL) : NULL;
    valid = valid && output;
    if (output) GDALClose(output);
    GDALClose(grid);
    return valid;
}

static bool
write_grass_fixture(const char *filename, GDALDatasetH source)
{
    const int columns = GDALGetRasterXSize(source), rows = GDALGetRasterYSize(source);
    double transform[6];
    std::vector<unsigned char> samples;
    if (!read_fixture_grid(source, transform, samples)) return false;
    std::ofstream grid(filename, std::ios::binary);
    grid.imbue(std::locale::classic());
    grid.precision(std::numeric_limits<double>::max_digits10);
    grid << "north: " << transform[3] << "\nsouth: " << transform[3] + rows * transform[5]
	<< "\neast: " << transform[0] + columns * transform[1] << "\nwest: " << transform[0]
	<< "\nrows: " << rows << "\ncols: " << columns << '\n';
    for (int row = 0; row < rows; ++row) {
	for (int column = 0; column < columns; ++column)
	    grid << static_cast<unsigned>(samples[static_cast<size_t>(row) * columns + column]) << ' ';
	grid << '\n';
    }
    grid.close();
    const std::string path(filename);
    std::ofstream projection(path.substr(0, path.find_last_of('.')) + ".prj", std::ios::binary);
    projection << GDALGetProjectionRef(source);
    projection.close();
    return !grid.fail() && !projection.fail();
}

static bool
copy_fixture(const char *driver_name, const char *filename, GDALDatasetH dataset, char **options = NULL)
{
    if (BU_STR_EQUAL(driver_name, "GRASSASCIIGrid")) return write_grass_fixture(filename, dataset);
    if (BU_STR_EQUAL(driver_name, "GXF")) return write_gxf_fixture(filename, dataset, options);
    if (BU_STR_EQUAL(driver_name, "DTED")) return write_dted_fixture(filename, dataset);
    GDALDriverH driver = GDALGetDriverByName(driver_name);
    GDALDatasetH copy = driver && dataset ? GDALCreateCopy(driver, filename, dataset, FALSE, options, NULL, NULL) : NULL;
    if (!copy) return false;
    const bool units = !BU_STR_EQUAL(driver_name, "RST") ||
	GDALSetRasterUnitType(GDALGetRasterBand(copy, 1), "m") == CE_None;
    double transform[6];
    /* These drivers author their own georeferencing during CreateCopy;
     * GRIB and GeoPackage cannot reset it after raster publication. */
    const bool native = BU_STR_EQUAL(driver_name, "NITF") || BU_STR_EQUAL(driver_name, "GPKG") || BU_STR_EQUAL(driver_name, "GRIB") || BU_STR_EQUAL(driver_name, "AAIGrid") || BU_STR_EQUAL(driver_name, "GTX");
    bool valid = units && GDALGetGeoTransform(dataset, transform) == CE_None &&
	(native || (GDALSetGeoTransform(copy, transform) == CE_None && GDALSetProjection(copy, GDALGetProjectionRef(dataset)) == CE_None));
    GDALClose(copy);
    /* These simple raster drivers use a world file for their transform.
     * Their projection still comes from the PAM sidecar. */
    if (valid && (BU_STR_EQUAL(driver_name, "PNM") || BU_STR_EQUAL(driver_name, "BMP")))
	valid = GDALWriteWorldFile(filename, "wld", transform) != 0;
    copy = GDALOpen(filename, GA_ReadOnly);
    valid = valid && copy && GDALGetGeoTransform(copy, transform) == CE_None && GDALGetProjectionRef(copy)[0];
    if (copy) GDALClose(copy);
    return valid;
}

static bool
vicar_wrapper_identity(const char *filename, int side)
{
    std::ifstream source(filename, std::ios::binary);
    std::string declaration;
    source >> declaration;
    const std::string keyword = "LBLSIZE=";
    if (declaration.compare(0, keyword.size(), keyword)) return false;
    size_t label_size = 0;
    std::istringstream size_value(declaration.substr(keyword.size()));
    if (!(size_value >> label_size)) return false;
    source.clear();
    source.seekg(0);
    constexpr size_t wrapper_size = 512;
    std::ostringstream label;
    label << "PDS_VERSION_ID=PDS3\nRECORD_BYTES=1\n^IMAGE_HEADER=" << wrapper_size + 1
	<< "\n^IMAGE=" << wrapper_size + label_size + 1
	<< "\nOBJECT=IMAGE\nBANDS=1\nBAND_STORAGE_TYPE=\"BAND SEQUENTIAL\"\n"
	<< "LINES=" << side << "\nLINE_SAMPLES=" << side
	<< "\nSAMPLE_TYPE=LSB_UNSIGNED_INTEGER\nSAMPLE_BITS=8\nEND_OBJECT=IMAGE\nEND\n";
    std::string prefix = label.str();
    if (prefix.size() > wrapper_size) return false;
    prefix.resize(wrapper_size, ' ');
    const std::string path = std::string(filename) + ".pds";
    std::ofstream output(path, std::ios::binary);
    output << prefix << source.rdbuf();
    output.close();
    source.close();
    if (output.fail() || source.bad()) return false;

    const char *setting = CPLGetThreadLocalConfigOption("GDAL_TRY_PDS3_WITH_VICAR", NULL);
    const bool had_setting = setting != NULL;
    const std::string saved = setting ? setting : "";
    CPLSetThreadLocalConfigOption("GDAL_TRY_PDS3_WITH_VICAR", "YES");
    GDALDatasetH dataset = GDALOpen(path.c_str(), GA_ReadOnly);
    const bool decoded = dataset && BU_STR_EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(dataset)), "VICAR") &&
	GDALGetRasterXSize(dataset) == side && GDALGetRasterYSize(dataset) == side;
    if (dataset) GDALClose(dataset);
    const bool rejected = import_profile(path.c_str(), "application/vnd.nasa.vicar", NULL, false);
    const bool pds = import_profile(path.c_str(), "application/x-pds", NULL, true);
    CPLSetThreadLocalConfigOption("GDAL_TRY_PDS3_WITH_VICAR", had_setting ? saved.c_str() : NULL);
    const bool removed = bu_file_delete(path.c_str());
    const std::string sidecar = path + ".aux.xml";
    const bool sidecar_removed = !bu_file_exists(sidecar.c_str(), NULL) || bu_file_delete(sidecar.c_str());
    return decoded && rejected && pds && removed && sidecar_removed;
}

static bool
fixture(const char *tiff, const char *png)
{
    constexpr int side = 2;
    unsigned short samples[side * side] = {1, 2, 3, 4};
    GDALDriverH driver = GDALGetDriverByName("GTiff");
    GDALDatasetH dataset = driver ? GDALCreate(driver, tiff, side, side, 1, GDT_UInt16, NULL) : NULL;
    if (!dataset) return false;
    /* An ordinary UTM raster exercises the existing terrain path without
     * introducing unsupported local engineering coordinate systems. */
    double transform[] = {500000.0, 10.0, 0.0, 5000000.0, 0.0, -10.0};
    OGRSpatialReference reference;
    char *wkt = NULL;
    bool valid = reference.SetWellKnownGeogCS("WGS84") == OGRERR_NONE &&
	reference.SetUTM(32, TRUE) == OGRERR_NONE && reference.exportToWkt(&wkt) == OGRERR_NONE;
    valid = valid && GDALSetGeoTransform(dataset, transform) == CE_None &&
	GDALSetProjection(dataset, wkt) == CE_None &&
	GDALRasterIO(GDALGetRasterBand(dataset, 1), GF_Write, 0, 0, side, side,
	    samples, side, side, GDT_UInt16, 0, 0) == CE_None;
    CPLFree(wkt);
    valid = valid && copy_fixture("PNG", png, dataset);
    GDALClose(dataset);
    return valid;
}

static int
kml_profiles(const char *tiff)
{
    GDALDriverH driver = GDALGetDriverByName("KMLSUPEROVERLAY");
    if (!driver) return 0;
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    for (bool archive : {false, true}) {
	char temporary[MAXPATHLEN];
	FILE *file = bu_temp_file(temporary, sizeof(temporary));
	GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
	bu_mkdir(temporary);
	GDAL_CHECK(bu_file_directory(temporary));
	const std::string filename = std::string(temporary) + (archive ? "/doc.kmz" : "/doc.kml");
	const char *type = archive ? "application/vnd.google-earth.kmz" : "application/vnd.google-earth.kml+xml";
	const char *other = archive ? "application/vnd.google-earth.kml+xml" : "application/vnd.google-earth.kmz";
	char **options = CSLSetNameValue(NULL, "FORMAT", "PNG");
	GDALDatasetH copy = GDALCreateCopy(driver, filename.c_str(), source, FALSE, options, NULL, NULL);
	CSLDestroy(options);
	GDAL_CHECK(copy);
	GDALClose(copy);
	GDALDatasetH opened = GDALOpen(filename.c_str(), GA_ReadOnly);
	GDAL_CHECK(opened && BU_STR_EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(opened)), "KMLSUPEROVERLAY"));
	GDALClose(opened);
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, true));
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, true, ""));
	GDAL_CHECK(import_profile(filename.c_str(), other, NULL, false));
	const bu_mime_trait fields[] = {
	    BU_MIME_TRAIT_STRING("container", archive ? "zip" : "none"),
	    BU_MIME_TRAIT_STRING("encoding", archive ? "binary" : "utf-8")
	};
	const bu_mime_traits requested = {fields, 2};
	GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true, type));
	const bu_mime_trait unsupported[] = {
	    BU_MIME_TRAIT_STRING("container", archive ? "none" : "zip"),
	    BU_MIME_TRAIT_STRING("encoding", archive ? "utf-8" : "binary"),
	    BU_MIME_TRAIT_STRING("version", "2.2"), BU_MIME_TRAIT_STRING("bit-depth", "8")
	};
	for (const auto &field : unsupported) {
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &wrong, false));
	}
	if (!archive) {
	    const bu_mime_trait ascii = BU_MIME_TRAIT_STRING("text.ascii-only", "true");
	    const bu_mime_traits repertoire = {&ascii, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &repertoire, true));
	}
	gcv_context context;
	gcv_context_init(&context);
	bu_mime_result *result = NULL;
	GDAL_CHECK(bu_mime_detect_file(gcv_mime_registry(&context), filename.c_str(), &result) != BU_MIME_ERROR);
	const bu_mime_candidate *candidate = NULL;
	for (size_t i = 0; i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, type)) candidate = &result->candidates[i];
	GDAL_CHECK(candidate);
	if (archive) GDAL_CHECK(candidate->evidence == BU_MIME_EVIDENCE_EXTENSION && !candidate->traits.count);
	else GDAL_CHECK(candidate->evidence == BU_MIME_EVIDENCE_SIGNATURE &&
	    !bu_mime_traits_get(&candidate->traits, "version") &&
	    import_profile(filename.c_str(), type, &candidate->traits, true));
	bu_mime_result_free(result);
	gcv_context_destroy(&context);
	if (!archive) {
	    std::ifstream input(filename, std::ios::binary);
	    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
	    const size_t root = bytes.find("<kml "), end = bytes.find('>', root);
	    GDAL_CHECK(!input.bad() && root != std::string::npos && end != std::string::npos);
	    input.close();
	    const auto write_document = [&filename](const std::string &text) {
		std::ofstream output(filename, std::ios::binary | std::ios::trunc);
		output << text;
		output.close();
		return !output.fail();
	    };
	    for (const char *version : {"2.3.0", "9.1"}) {
		std::string document = bytes;
		document.insert(end, std::string(" version=\"") + version + "\"");
		GDAL_CHECK(write_document(document));
		opened = GDALOpen(filename.c_str(), GA_ReadOnly);
		GDAL_CHECK(opened);
		GDALClose(opened);
		const bu_mime_trait declared = BU_MIME_TRAIT_STRING("version", version);
		const bu_mime_traits requested_version = {&declared, 1};
		GDAL_CHECK(import_profile(filename.c_str(), type, &requested_version, BU_STR_EQUAL(version, "2.3.0")));
		GDAL_CHECK(import_profile(filename.c_str(), type, NULL, BU_STR_EQUAL(version, "2.3.0")));
	    }
	    const std::string invalid = std::string("\xef\xbb\xbf") + bytes + "<!--" +
		std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') + "\xff-->";
	    GDAL_CHECK(write_document(invalid));
	    opened = GDALOpen(filename.c_str(), GA_ReadOnly);
	    GDAL_CHECK(opened);
	    GDALClose(opened);
	    GDAL_CHECK(import_profile(filename.c_str(), type, NULL, true));
	    GDAL_CHECK(import_profile(filename.c_str(), type, &requested, false));
	    GDAL_CHECK(write_document(bytes));
	}
	bu_dirclear(temporary);
	GDAL_CHECK(!bu_file_exists(temporary, NULL));
    }
    GDALClose(source);
    return 0;
}

static bool
write_fixture_text(const std::string &filename, const std::string &text)
{
    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    return !output.fail();
}

static bool
opened_by(const char *filename, const char *driver)
{
    GDALDatasetH source = GDALOpen(filename, GA_ReadOnly);
    const bool matches = source && BU_STR_EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(source)), driver);
    if (source) GDALClose(source);
    return matches;
}

static int
label_profiles(const char *tiff)
{
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    const struct {const char *driver; const char *type; const char *option; const char *value;} cases[] = {
	{"PDS4", "application/vnd.nasa.pds4+xml", "IMAGE_FORMAT", "RAW"},
	{"PDS4", "application/vnd.nasa.pds4+xml", "IMAGE_FORMAT", "GEOTIFF"},
	{"MRF", "application/vnd.nasa.mrf+xml", "COMPRESS", "PNG"},
	{"MRF", "application/vnd.nasa.mrf+xml", "COMPRESS", "DEFLATE"}
    };
    for (const auto &sample : cases) {
	GDALDriverH driver = GDALGetDriverByName(sample.driver);
	if (!driver) continue;
	const bool pds = BU_STR_EQUAL(sample.driver, "PDS4");
	char temporary[MAXPATHLEN];
	FILE *file = bu_temp_file(temporary, sizeof(temporary));
	GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
	bu_mkdir(temporary);
	GDAL_CHECK(bu_file_directory(temporary));
	const std::string filename = std::string(temporary) + (pds ? "/label.xml" : "/label.mrf");
	char **options = CSLSetNameValue(NULL, sample.option, sample.value);
	GDALDatasetH copy = GDALCreateCopy(driver, filename.c_str(), source, FALSE, options, NULL, NULL);
	CSLDestroy(options);
	GDAL_CHECK(copy);
	GDALClose(copy);
	GDAL_CHECK(opened_by(filename.c_str(), sample.driver));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true, ""));
	GDAL_CHECK(import_profile(filename.c_str(), "application/x-pds", NULL, false));
	GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
	const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	    BU_MIME_TRAIT_STRING("bit-depth", "16"), BU_MIME_TRAIT_STRING("version", pds ? "4" : "1")};
	for (const auto &field : unsupported) {
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	}
	std::ifstream input(filename, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(input)), {});
	GDAL_CHECK(!input.bad() && !bytes.empty());
	input.close();
	const char *key = pds ? "pds4.information-model-version" : "mrf.tile-compression";
	const char *field_name = pds ? "information_model_version" : "Compression";
	const std::string closing = std::string("</") + field_name + ">";
	if (!pds && BU_STR_EQUAL(sample.value, "PNG") && bytes.find(closing) == std::string::npos) {
	    const bu_mime_trait inferred = BU_MIME_TRAIT_STRING(key, "PNG");
	    const bu_mime_traits implicit = {&inferred, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &implicit, true));
	    const size_t raster_end = bytes.find("</Raster>");
	    GDAL_CHECK(raster_end != std::string::npos);
	    bytes.insert(raster_end, "<Compression>PNG</Compression>");
	    GDAL_CHECK(write_fixture_text(filename, bytes));
	}
	const size_t end = bytes.find(closing);
	const std::string opening = std::string("<") + field_name + ">";
	const size_t start = bytes.find(opening);
	GDAL_CHECK(start != std::string::npos && end != std::string::npos && end > start + opening.size());
	const std::string value = bytes.substr(start + opening.size(), end - start - opening.size());
	const bu_mime_trait declared = BU_MIME_TRAIT_STRING(key, value.c_str());
	const bu_mime_traits requested = {&declared, 1};
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, true));
	const bu_mime_trait incorrect = BU_MIME_TRAIT_STRING(key, "unestablished");
	const bu_mime_traits wrong = {&incorrect, 1};
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	std::string foreign = bytes;
	foreign.insert(start, std::string("<other:") + field_name + " xmlns:other=\"urn:fixture:metadata\">unestablished</other:" + field_name + ">");
	GDAL_CHECK(write_fixture_text(filename, foreign) && opened_by(filename.c_str(), sample.driver));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, true));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	std::string duplicate = bytes;
	duplicate.insert(start, opening + value + closing);
	GDAL_CHECK(write_fixture_text(filename, duplicate) && opened_by(filename.c_str(), sample.driver));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, false));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	GDAL_CHECK(write_fixture_text(filename, bytes));
	gcv_context context;
	gcv_context_init(&context);
	bu_mime_result *result = NULL;
	GDAL_CHECK(bu_mime_detect_file(gcv_mime_registry(&context), filename.c_str(), &result) != BU_MIME_ERROR);
	const bu_mime_traits *facts = NULL;
	for (size_t i = 0; i < result->count; ++i)
	    if (BU_STR_EQUAL(result->candidates[i].name, sample.type)) facts = &result->candidates[i].traits;
	GDAL_CHECK(facts && !bu_mime_traits_get(facts, "version") && !bu_mime_traits_get(facts, "bit-depth"));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, facts, true));
	bu_mime_result_free(result);
	gcv_context_destroy(&context);
	bu_dirclear(temporary);
	GDAL_CHECK(!bu_file_exists(temporary, NULL));
    }
    GDALClose(source);
    return 0;
}

static bool
sidecar_samples(const char *filename, size_t bands, bool logical = false)
{
    using namespace mime_raster_label_fixture;
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) return false;
    bool valid = GDALGetRasterXSize(dataset) == side && GDALGetRasterYSize(dataset) == side &&
	GDALGetRasterCount(dataset) == static_cast<int>(bands);
    for (size_t band = 0; valid && band < bands; ++band) {
	double values[cells];
	valid = GDALRasterIO(GDALGetRasterBand(dataset, static_cast<int>(band + 1)), GF_Read,
	    0, 0, side, side, values, side, side, GDT_Float64, 0, 0) == CE_None;
	for (size_t i = 0; valid && i < cells; ++i) {
	    const double expected = logical ? ((i == 1 || i == 2) != (band != 0)) : band * 10 + i + 1;
	    valid = NEAR_EQUAL(values[i], expected, SMALL_FASTF);
	    if (!valid) bu_log("Incorrect raster sample: band %zu cell %zu got %g expected %g\n", band, i, values[i], expected);
	}
    }
    GDALClose(dataset);
    return valid;
}

static int
sidecar_profiles()
{
    using namespace mime_raster_label_fixture;
    namespace names = mime_raster_label;
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    const std::string base = std::string(temporary) + "/grid";
    gcv_context context;
    gcv_context_init(&context);
    for (const char *type : {names::rraster_data, names::roipac_label, names::isce_label, names::ehdr_label})
	GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, type, "GDAL Reader", NULL));
    bu_vls canonical = BU_VLS_INIT_ZERO;
    for (const char *type : {names::ehdr_data, names::bil_data}) {
	GDAL_CHECK(bu_mime_canonical(gcv_mime_registry(&context), type, &canonical));
	GDAL_CHECK(BU_STR_EQUAL(bu_vls_cstr(&canonical), type));
    }
    bu_vls_free(&canonical);
    gcv_context_destroy(&context);
    if (GDALGetDriverByName("RRASTER")) {
	const struct {const char *code; size_t size; bool floating; bool logical;} types[] = {
	    {"LOG1S", 1, false, true}, {"INT1S", 1, false, false}, {"INT2S", 2, false, false},
	    {"INT4S", 4, false, false}, {"INT1U", 1, false, false}, {"INT2U", 2, false, false},
	    {"INT4U", 4, false, false}, {"FLT4S", 4, true, false}, {"FLT8S", 8, true, false}
	};
	for (const auto &type : types)
	    for (const char *layout : {"BIL", "BIP", "BSQ"}) {
		const bool little = !BU_STR_EQUAL(layout, "BSQ");
		const std::string path = base + ".grd";
		GDAL_CHECK(write_fixture_text(path, rraster(type.code, layout, little)) &&
		    write_fixture_text(base + ".gri", pixels(layout, type.size, type.floating, little, 2, type.logical)));
		GDAL_CHECK(opened_by(path.c_str(), "RRASTER") && sidecar_samples(path.c_str(), 2, type.logical));
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("rraster.datatype", type.code),
		    BU_MIME_TRAIT_STRING("rraster.interleave", layout), BU_MIME_TRAIT_STRING("rraster.byte-order", little ? "LITTLE" : "BIG"),
		    BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("raster.entrypoint", "label")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(path.c_str(), names::rraster_label, &requested, true, ""));
		const bu_mime_trait field = BU_MIME_TRAIT_STRING("rraster.interleave", BU_STR_EQUAL(layout, "BIL") ? "BIP" : "BIL");
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(path.c_str(), names::rraster_label, &wrong, false));
	    }
	const std::string path = base + ".grd";
	std::string duplicate = rraster();
	duplicate.insert(duplicate.find("[georeference]"), "datatype=FLT4S\n");
	GDAL_CHECK(write_fixture_text(base + ".gri", pixels("BIL", 4, true, true)) && write_fixture_text(path, duplicate));
	GDAL_CHECK(opened_by(path.c_str(), "RRASTER") && import_profile(path.c_str(), names::rraster_label, NULL, false));
	GDAL_CHECK(write_fixture_text(base + ".data", rraster()));
	GDAL_CHECK(import_profile((base + ".data").c_str(), names::rraster_label, NULL, false));
	std::string padded = std::string("\xef\xbb\xbf") + rraster() + "#" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 32, ' ');
	const bu_mime_trait charset = BU_MIME_TRAIT_STRING("encoding", "utf-8");
	const bu_mime_traits utf8 = {&charset, 1};
	GDAL_CHECK(write_fixture_text(path, padded) && opened_by(path.c_str(), "RRASTER"));
	GDAL_CHECK(import_profile(path.c_str(), names::rraster_label, &utf8, true));
	padded.back() = static_cast<char>(0xff);
	GDAL_CHECK(write_fixture_text(path, padded) && opened_by(path.c_str(), "RRASTER"));
	GDAL_CHECK(import_profile(path.c_str(), names::rraster_label, &utf8, false));
	GDAL_CHECK(bu_file_delete(path.c_str()) && bu_file_delete((base + ".gri").c_str()));
    }
    if (GDALGetDriverByName("EHdr")) {
	const char wgs84[] = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],"
	    "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";
	GDAL_CHECK(write_fixture_text(base + ".prj", wgs84));
	for (const char *layout : {"BIL", "BIP", "BSQ"}) {
	    const bool bil = BU_STR_EQUAL(layout, "BIL"), little = !BU_STR_EQUAL(layout, "BSQ");
	    /* A misleading .bil must never relabel BIP/BSQ as the published BIL type. */
	    const std::string path = base + ".bil";
	    GDAL_CHECK(write_fixture_text(base + ".hdr", ehdr(layout, little)) &&
		write_fixture_text(path, pixels(layout, 4, true, little)));
	    GDAL_CHECK(opened_by(path.c_str(), "EHdr") && sidecar_samples(path.c_str(), 2));
	    const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("ehdr.interleave", layout), BU_MIME_TRAIT_STRING("ehdr.nbits", "32"),
		BU_MIME_TRAIT_STRING("ehdr.pixeltype", "FLOAT"), BU_MIME_TRAIT_STRING("ehdr.byte-order", little ? "I" : "M"),
		BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("raster.entrypoint", "payload")};
	    const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
	    GDAL_CHECK(import_profile(path.c_str(), names::ehdr_data, &requested, true, names::ehdr_data));
	    GDAL_CHECK(import_profile(path.c_str(), names::bil_data, NULL, bil));
	    if (bil) GDAL_CHECK(import_profile(path.c_str(), names::bil_data, &requested, true, ""));
	    const bu_mime_trait field = BU_MIME_TRAIT_STRING("ehdr.nbits", "16");
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(path.c_str(), names::ehdr_data, &wrong, false));
	}
	GDAL_CHECK(bu_file_delete((base + ".hdr").c_str()) && bu_file_delete((base + ".bil").c_str()));
    }
    if (GDALGetDriverByName("ROI_PAC")) {
	for (const char *kind : {"dem", "flg", "amp", "cor", "hgt", "unw", "msk", "trans"}) {
	    const bool dem = BU_STR_EQUAL(kind, "dem"), flag = BU_STR_EQUAL(kind, "flg");
	    const size_t bands = dem || flag ? 1 : 2;
	    const std::string path = base + "." + kind;
	    const char *layout = BU_STR_EQUAL(kind, "amp") ? "BIP" : "BIL";
	    GDAL_CHECK(write_fixture_text(path, pixels(layout, dem ? 2 : flag ? 1 : 4, !dem && !flag, true, bands)) &&
		write_fixture_text(path + ".rsc", roipac()));
	    GDAL_CHECK(opened_by(path.c_str(), "ROI_PAC") && sidecar_samples(path.c_str(), bands));
	    const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("roi-pac.filename-kind", kind),
		BU_MIME_TRAIT_STRING("roi-pac.unit-transform", "true"), BU_MIME_TRAIT_STRING("encoding", "binary")};
	    const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
	    GDAL_CHECK(import_profile(path.c_str(), names::roipac_data, &requested, true, names::roipac_data));
	    const bu_mime_trait field = BU_MIME_TRAIT_STRING("roi-pac.filename-kind", dem ? "flg" : "dem");
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(path.c_str(), names::roipac_data, &wrong, false));
	    for (const auto &transform : {std::make_pair("2", "0"), std::make_pair("1", "1e-20")}) {
		GDAL_CHECK(write_fixture_text(path + ".rsc", roipac(transform.first, transform.second)) && opened_by(path.c_str(), "ROI_PAC"));
		GDAL_CHECK(import_profile(path.c_str(), names::roipac_data, NULL, false));
	    }
	    GDAL_CHECK(bu_file_delete(path.c_str()) && bu_file_delete((path + ".rsc").c_str()));
	}
    }
    if (GDALGetDriverByName("ISCE")) {
	const struct {const char *code; size_t size; bool floating;} types[] = {
	    {"BYTE", 1, false}, {"CHAR", 1, false}, {"SHORT", 2, false}, {"INT", 4, false},
	    {"LONG", 8, false}, {"FLOAT", 4, true}, {"DOUBLE", 8, true}
	};
	const std::string path = base + ".bin";
	for (const auto &type : types)
	    for (const char *layout : {"BIL", "BIP", "BSQ"}) {
		const bool little = !BU_STR_EQUAL(layout, "BSQ");
		GDAL_CHECK(write_fixture_text(path, pixels(layout, type.size, type.floating, little)) &&
		    write_fixture_text(path + ".xml", isce(type.code, layout, little)));
		GDAL_CHECK(opened_by(path.c_str(), "ISCE") && sidecar_samples(path.c_str(), 2));
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("isce.datatype", type.code), BU_MIME_TRAIT_STRING("isce.scheme", layout),
		    BU_MIME_TRAIT_STRING("isce.byte-order", little ? "L" : "B"), BU_MIME_TRAIT_STRING("encoding", "binary")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(path.c_str(), names::isce_data, &requested, true, names::isce_data));
		const bu_mime_trait field = BU_MIME_TRAIT_STRING("isce.scheme", BU_STR_EQUAL(layout, "BIL") ? "BIP" : "BIL");
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(path.c_str(), names::isce_data, &wrong, false));
	    }
	GDAL_CHECK(write_fixture_text(path, pixels("BIL", 4, true, true)));
	for (const char *extra : {"<extra:property xmlns:extra=\"urn:fixture:metadata\" name=\"SCHEME\"><extra:value>BSQ</extra:value></extra:property>",
	    "<property xmlns=\"urn:fixture:metadata\" name=\"SCHEME\"><value>BSQ</value></property>",
	    "<property name=\"SCHEME\"><value>BIL</value></property>",
	    "<PROPERTY name=\"SCHEME\"><value>BSQ</value></PROPERTY>",
	    "<property name=\"SCHEME\" value=\"BSQ\"><value>BIL</value></property>",
	    "<property><name>SCHEME</name><value>BSQ</value></property>"}) {
	    std::string text = isce();
	    text.insert(text.find("</imageFile>"), extra);
	    GDAL_CHECK(write_fixture_text(path + ".xml", text) && opened_by(path.c_str(), "ISCE"));
	    GDAL_CHECK(import_profile(path.c_str(), names::isce_data, NULL, std::string(extra).find("<extra:") == 0));
	}
	GDAL_CHECK(write_fixture_text(path + ".xml", std::string("\xef\xbb\xbf") + isce()));
	GDAL_CHECK(import_profile(path.c_str(), names::isce_data, NULL, true));
	std::string complex = isce("CFLOAT");
	GDAL_CHECK(write_fixture_text(path, pixels("BIL", 8, true, true)) && write_fixture_text(path + ".xml", complex));
	GDAL_CHECK(opened_by(path.c_str(), "ISCE") && import_profile(path.c_str(), names::isce_data, NULL, false));
    }
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static bool
heightfield_samples(const char *filename, int band_count = 1)
{
    constexpr int side = 2;
    constexpr double tolerance = 1.0e-6;
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) return false;
    double values[side * side];
    bool valid = GDALGetRasterXSize(dataset) == side && GDALGetRasterYSize(dataset) == side &&
	GDALGetRasterCount(dataset) == band_count && GDALRasterIO(GDALGetRasterBand(dataset, 1), GF_Read,
	    0, 0, side, side, values, side, side, GDT_Float64, 0, 0) == CE_None;
    for (int i = 0; valid && i < side * side; ++i) valid = NEAR_EQUAL(values[i], i + 1, tolerance);
    GDALClose(dataset);
    return valid;
}

static bool
science_tiff_tile(const std::string &filename, GDALDataType type)
{
    GDALDatasetH source = GDALCreate(GDALGetDriverByName("GTiff"), filename.c_str(), 2, 2, 1, type, NULL);
    if (!source) return false;
    double samples[] = {1, 2, 3, 4};
    const bool wrote = GDALRasterIO(GDALGetRasterBand(source, 1), GF_Write, 0, 0, 2, 2, samples, 2, 2, GDT_Float64, 0, 0) == CE_None;
    GDALClose(source);
    return wrote;
}

static bool
esric_tile(const char *filename, unsigned int tile_size, const char *driver)
{
    const int bands = BU_STR_EQUAL(driver, "JPEG") ? 3 : 4;
    GDALDatasetH memory = GDALCreate(GDALGetDriverByName("MEM"), "", tile_size, tile_size, bands, GDT_Byte, NULL);
    if (!memory) return false;
    std::vector<unsigned char> pixels(static_cast<size_t>(tile_size) * tile_size, 0);
    pixels[0] = 1; pixels[1] = 2; pixels[tile_size] = 3; pixels[tile_size + 1] = 4;
    bool valid = true;
    for (int band = 1; band <= bands; ++band)
	valid = valid && GDALRasterIO(GDALGetRasterBand(memory, band), GF_Write, 0, 0, tile_size, tile_size,
	    pixels.data(), tile_size, tile_size, GDT_Byte, 0, 0) == CE_None;
    GDALDatasetH copy = valid ? GDALCreateCopy(GDALGetDriverByName(driver), filename, memory, FALSE, NULL, NULL, NULL) : NULL;
    if (copy) GDALClose(copy);
    GDALClose(memory);
    return copy != NULL;
}

static int
product_profiles(const char *tiff)
{
    namespace fixture = mime_product_fixture;
    gcv_context context;
    gcv_context_init(&context);
    GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, mime_xml::safe_type, "GDAL Reader", NULL));
    GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, mime_xml::sentinel2_type, "GDAL Reader", NULL));
    gcv_context_destroy(&context);
    if (!GDALGetDriverByName("ESRIC")) return 0;
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    const std::string base = temporary, json = base + "/root.json", xml = base + "/conf.xml";
    const std::string tile = base + "/tile.png", bundle = base + "/tile/L00/R0000C0000.bundle";
    bu_mkdir((base + "/tile/L00").c_str());
    bu_mkdir((base + "/_alllayers/L00").c_str());
    const char *types[] = {mime_esric::xml_type, mime_esric::json_type, mime_esric::zip_type};
    for (const char *type : types) GDAL_CHECK(import_profile(tiff, type, NULL, false));
    for (unsigned int size : {256u, 512u})
	for (const char *format : {"PNG", "JPEG", "MIXED"}) {
	    const char *driver = BU_STR_EQUAL(format, "JPEG") ? "JPEG" : "PNG";
	    GDAL_CHECK(esric_tile(tile.c_str(), size, driver));
	    std::ifstream input(tile, std::ios::binary);
	    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
	    GDAL_CHECK(!input.bad() && !bytes.empty());
	    input.close();
	    const std::string payload = fixture::bundle(bytes);
	    GDAL_CHECK(write_fixture_text(bundle, payload));
	    GDAL_CHECK(write_fixture_text(base + "/_alllayers/L00/R0000C0000.bundle", payload));
	    GDAL_CHECK(write_fixture_text(json, fixture::esri_json(size, format)) && write_fixture_text(xml, fixture::esri_xml(size, format)));
	    const std::string archive = base + "/cache.tpkx";
	    if (bu_file_exists(archive.c_str(), NULL)) GDAL_CHECK(bu_file_delete(archive.c_str()));
	    const std::string zip = "/vsizip/{" + archive + "}";
	    GDAL_CHECK(CPLCopyFile((zip + "/root.json").c_str(), json.c_str()) == 0);
	    GDAL_CHECK(CPLCopyFile((zip + "/tile/L00/R0000C0000.bundle").c_str(), bundle.c_str()) == 0);
	    const std::string paths[] = {xml, json, archive};
	    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
		const auto &path = paths[i];
		GDAL_CHECK(opened_by(path.c_str(), "ESRIC"));
		if (!BU_STR_EQUAL(format, "JPEG")) GDAL_CHECK(heightfield_samples(path.c_str(), 4));
		const std::string width = std::to_string(size);
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("esri-cache.packet-size", "128"),
		    BU_MIME_TRAIT_STRING("esri-cache.tile-rows", width.c_str()), BU_MIME_TRAIT_STRING("esri-cache.tile-format", format),
		    BU_MIME_TRAIT_STRING("container", i == 2 ? "zip" : "none")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(path.c_str(), types[i], &requested, true, ""));
		for (const char *type : types) if (!BU_STR_EQUAL(type, types[i])) GDAL_CHECK(import_profile(path.c_str(), type, NULL, false));
		const bu_mime_trait rejected[] = {BU_MIME_TRAIT_STRING("esri-cache.packet-size", "64"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
		    BU_MIME_TRAIT_STRING("version", "2"), BU_MIME_TRAIT_STRING("esri-cache.tile-format", "LERC")};
		for (const auto &field : rejected) {
		    const bu_mime_traits wrong = {&field, 1};
		    GDAL_CHECK(import_profile(path.c_str(), types[i], &wrong, false));
		}
	    }
	    const std::string renamed = base + "/package.data";
	    GDAL_CHECK(CPLCopyFile(renamed.c_str(), archive.c_str()) == 0);
	    GDAL_CHECK(import_profile(renamed.c_str(), mime_esric::zip_type, NULL, true));
	    GDAL_CHECK(bu_file_delete(renamed.c_str()));
	}
    const std::string source = fixture::esri_json();
    for (const auto &change : {std::make_pair("\"version\":1.0", "\"version\":9.0"),
	std::make_pair("\"version\":1.0", "\"version\":\"1.0\""),
	std::make_pair("\"packetSize\":128", "\"packetSize\":128,\"packetSize\":128"),
	std::make_pair("\"format\":\"PNG\"", "\"format\":\"PNG\",\"format\":\"JPEG\"")}) {
	std::string bytes = source;
	bytes.replace(bytes.find(change.first), strlen(change.first), change.second);
	GDAL_CHECK(write_fixture_text(json, bytes) && opened_by(json.c_str(), "ESRIC"));
	GDAL_CHECK(import_profile(json.c_str(), mime_esric::json_type, NULL, false));
	const std::string archive = base + "/rejected.tpkx", zip = "/vsizip/{" + archive + "}";
	GDAL_CHECK(CPLCopyFile((zip + "/root.json").c_str(), json.c_str()) == 0);
	GDAL_CHECK(CPLCopyFile((zip + "/tile/L00/R0000C0000.bundle").c_str(), bundle.c_str()) == 0);
	GDAL_CHECK(opened_by(archive.c_str(), "ESRIC"));
	GDAL_CHECK(import_profile(archive.c_str(), mime_esric::zip_type, NULL, false));
	GDAL_CHECK(bu_file_delete(archive.c_str()));
    }
    std::string integer_revision = source;
    integer_revision.replace(integer_revision.find("\"version\":1.0"), strlen("\"version\":1.0"), "\"version\":1");
    GDAL_CHECK(write_fixture_text(json, integer_revision) && opened_by(json.c_str(), "ESRIC"));
    GDAL_CHECK(import_profile(json.c_str(), mime_esric::json_type, NULL, true));
    std::string omitted = source;
    omitted.erase(omitted.find("\"version\":1.0,"), strlen("\"version\":1.0,"));
    GDAL_CHECK(write_fixture_text(json, omitted) && opened_by(json.c_str(), "ESRIC"));
    GDAL_CHECK(import_profile(json.c_str(), mime_esric::json_type, NULL, true));
    const bu_mime_trait version = BU_MIME_TRAIT_STRING("esri-cache.package-version", "1.0");
    const bu_mime_traits revision = {&version, 1};
    GDAL_CHECK(import_profile(json.c_str(), mime_esric::json_type, &revision, false));
    for (const auto &change : {std::make_pair("<PacketSize>128</PacketSize>", "<PacketSize>128</PacketSize><PacketSize>128</PacketSize>"),
	std::make_pair("<StorageFormat>", "<StorageFormat xmlns=\"urn:other\">"),
	std::make_pair("<CacheTileFormat>PNG</CacheTileFormat>", "<CacheTileFormat>PNG</CacheTileFormat><CacheTileFormat>JPEG</CacheTileFormat>")}) {
	std::string bytes = fixture::esri_xml();
	bytes.replace(bytes.find(change.first), strlen(change.first), change.second);
	GDAL_CHECK(write_fixture_text(xml, bytes) && opened_by(xml.c_str(), "ESRIC"));
	GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, NULL, false));
    }
    omitted = fixture::esri_xml();
    omitted.erase(omitted.find("<PacketSize>128</PacketSize>"), strlen("<PacketSize>128</PacketSize>"));
    GDAL_CHECK(write_fixture_text(xml, omitted) && opened_by(xml.c_str(), "ESRIC"));
    GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, NULL, true));
    const bu_mime_trait packet = BU_MIME_TRAIT_STRING("esri-cache.packet-size", "128");
    const bu_mime_traits packets = {&packet, 1};
    GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, &packets, false));
    for (const char *charset : {"ISO-8859-1", "windows-1252"}) {
	const std::string declaration = std::string("<?xml version=\"1.0\" encoding=\"") + charset + "\"?>";
	const std::string label = declaration + fixture::esri_xml() + "<!--caf\xe9-->";
	GDAL_CHECK(write_fixture_text(xml, label) && opened_by(xml.c_str(), "ESRIC"));
	GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, NULL, true, ""));
	const char *encoding = BU_STR_EQUAL(charset, "ISO-8859-1") ? "iso-8859-1" : "windows-1252";
	const bu_mime_trait field = BU_MIME_TRAIT_STRING("encoding", encoding);
	const bu_mime_traits requested = {&field, 1};
	GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, &requested, BU_STR_EQUAL(charset, "ISO-8859-1")));
	const bu_mime_trait ascii = BU_MIME_TRAIT_STRING("text.ascii-only", "false");
	const bu_mime_traits characters = {&ascii, 1};
	GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, &characters, BU_STR_EQUAL(charset, "ISO-8859-1")));
	const bu_mime_trait utf8 = BU_MIME_TRAIT_STRING("encoding", "utf-8");
	const bu_mime_traits wrong = {&utf8, 1};
	GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, &wrong, false));
    }
    std::string padded = std::string("\xef\xbb\xbf") + fixture::esri_xml() + "<!--" +
	std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 512, ' ') + "-->";
    GDAL_CHECK(write_fixture_text(xml, padded) && opened_by(xml.c_str(), "ESRIC"));
    const bu_mime_trait utf8 = BU_MIME_TRAIT_STRING("encoding", "utf-8");
    const bu_mime_traits repertoire = {&utf8, 1};
    GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, &repertoire, true));
    padded[padded.size() - 4] = static_cast<char>(0xc0);
    GDAL_CHECK(write_fixture_text(xml, padded) && opened_by(xml.c_str(), "ESRIC"));
    GDAL_CHECK(import_profile(xml.c_str(), mime_esric::xml_type, &repertoire, false));
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static int
science_profiles(const char *tiff)
{
    namespace fixture = mime_science_fixture;
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    const std::string base = std::string(temporary) + "/grid";
    gcv_context context;
    gcv_context_init(&context);
    GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, mime_nsidc::type, "GDAL Reader", NULL));
    gcv_context_destroy(&context);
    if (GDALGetDriverByName("NOAA_B")) {
	GDAL_CHECK(import_profile(tiff, mime_noaa_grid::type, NULL, false));
	for (int kind : {0, 1, 2})
	    for (bool little : {false, true}) {
		const std::string path = base + ".b";
		const std::string bytes = fixture::noaa(kind, little), code = std::to_string(kind);
		GDAL_CHECK(write_fixture_text(path, bytes) && opened_by(path.c_str(), "NOAA_B") && heightfield_samples(path.c_str()));
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("noaa-grid.kind", code.c_str()),
		    BU_MIME_TRAIT_STRING("byte-order", little ? "little-endian" : "big-endian"),
		    BU_MIME_TRAIT_STRING("bit-depth", kind == 2 ? "16" : "32"),
		    BU_MIME_TRAIT_STRING("sample-format", kind == 1 ? "ieee754" : "signed-integer")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(path.c_str(), mime_noaa_grid::type, &requested, true, ""));
		const bu_mime_trait field = BU_MIME_TRAIT_STRING("noaa-grid.kind", "-1");
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(path.c_str(), mime_noaa_grid::type, &wrong, false));
		GDAL_CHECK(write_fixture_text(base + ".data", bytes));
		GDAL_CHECK(import_profile((base + ".data").c_str(), mime_noaa_grid::type, NULL, false));
	    }
	std::string coarse = fixture::noaa();
	mime_heightfield_fixture::word(coarse, 20, UINT64_C(0x4000000000000000), 8, false);
	GDAL_CHECK(write_fixture_text(base + ".b", coarse));
	GDAL_CHECK(import_profile((base + ".b").c_str(), mime_noaa_grid::type, NULL, false));
	GDAL_CHECK(write_fixture_text(base + ".b", fixture::noaa(-1)));
	GDAL_CHECK(import_profile((base + ".b").c_str(), mime_noaa_grid::type, NULL, false));
	GDAL_CHECK(bu_file_delete((base + ".b").c_str()));
    }
    if (GDALGetDriverByName("NSIDCbin"))
	for (bool south : {false, true}) {
	    const std::string path = base + ".bin";
	    GDAL_CHECK(write_fixture_text(path, fixture::nsidc(south)) && opened_by(path.c_str(), "NSIDCbin"));
	    GDALDatasetH source = GDALOpen(path.c_str(), GA_ReadOnly);
	    GDAL_CHECK(source);
	    int known = FALSE;
	    const double scale = GDALGetRasterScale(GDALGetRasterBand(source, 1), &known);
	    GDALClose(source);
	    GDAL_CHECK(known && NEAR_EQUAL(scale, 0.4, SMALL_FASTF));
	}
    if (GDALGetDriverByName("IRIS")) {
	for (const char *type : {mime_iris::type, mime_iris::ppi_type, mime_iris::cappi_type})
	    GDAL_CHECK(import_profile(tiff, type, NULL, false));
	for (unsigned int product : {1u, 3u, 5u, 7u, 8u, 10u, 16u})
	    for (unsigned int quantity : {1u, 2u, 3u, 8u, 9u, 32u, 33u, 35u, 37u}) {
		const std::string path = base + ".data", product_code = std::to_string(product), quantity_code = std::to_string(quantity);
		GDAL_CHECK(write_fixture_text(path, fixture::iris(product, quantity)) && opened_by(path.c_str(), "IRIS") && heightfield_samples(path.c_str()));
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("iris.product-code", product_code.c_str()),
		    BU_MIME_TRAIT_STRING("iris.quantity-code", quantity_code.c_str()), BU_MIME_TRAIT_STRING("iris.product-header-version", "8"),
		    BU_MIME_TRAIT_STRING("iris.configuration-version", "6"), BU_MIME_TRAIT_STRING("iris.payload-layout", "plain-cartesian")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		const char *view = product == 1 ? mime_iris::ppi_type : product == 3 ? mime_iris::cappi_type : mime_iris::type;
		GDAL_CHECK(import_profile(path.c_str(), view, &requested, true, ""));
		if (product == 1 || product == 3) GDAL_CHECK(import_profile(path.c_str(), mime_iris::type, &requested, true));
		const bu_mime_trait field = BU_MIME_TRAIT_STRING("iris.quantity-code", "31");
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(path.c_str(), view, &wrong, false));
	    }
	const std::string path = base + ".data";
	GDAL_CHECK(write_fixture_text(path, fixture::iris(3, 2, 2)) && sidecar_samples(path.c_str(), 2));
	GDAL_CHECK(import_profile(path.c_str(), mime_iris::cappi_type, NULL, true));
	GDAL_CHECK(write_fixture_text(path, fixture::iris(3, 8, 2)) && opened_by(path.c_str(), "IRIS"));
	GDAL_CHECK(sidecar_samples(path.c_str(), 2));
	GDAL_CHECK(import_profile(path.c_str(), mime_iris::cappi_type, NULL, true));
	for (unsigned int quantity : {31u, 4u}) {
	    GDAL_CHECK(write_fixture_text(path, fixture::iris(1, quantity)) && opened_by(path.c_str(), "IRIS"));
	    GDAL_CHECK(import_profile(path.c_str(), mime_iris::ppi_type, NULL, false));
	}
	std::string future = fixture::iris();
	mime_heightfield_fixture::word(future, 2, 9, 2);
	GDAL_CHECK(write_fixture_text(path, future) && opened_by(path.c_str(), "IRIS"));
	GDAL_CHECK(import_profile(path.c_str(), mime_iris::type, NULL, false));
	GDAL_CHECK(write_fixture_text(path, fixture::iris() + std::string(1024, 0)) && opened_by(path.c_str(), "IRIS"));
	GDAL_CHECK(import_profile(path.c_str(), mime_iris::type, NULL, false));
	GDAL_CHECK(write_fixture_text(path, fixture::iris(3)));
	GDAL_CHECK(import_profile(path.c_str(), mime_iris::ppi_type, NULL, false));
    }
    if (GDALGetDriverByName("PRF")) {
	GDAL_CHECK(import_profile(tiff, mime_raster_label::photomod_label, NULL, false));
	const char wgs84[] = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],"
	    "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";
	const struct {GDALDataType type; const char *channel; const char *bytes;} types[] = {
	    {GDT_Byte, "U", "1"}, {GDT_UInt16, "U", "2"}, {GDT_UInt32, "U", "4"}, {GDT_Float32, "F", "4"}, {GDT_Float64, "F", "8"}
	};
	for (const auto &type : types)
	    for (bool dem : {false, true}) {
		const std::string path = base + (dem ? ".x-dem" : ".prf");
		bu_mkdir(base.c_str());
		const std::string tile = base + (dem ? "/tile.demtif" : "/tile.tif");
		GDAL_CHECK(science_tiff_tile(tile, type.type));
		const std::string text = fixture::photomod(dem, type.channel, type.bytes);
		GDAL_CHECK(write_fixture_text(path, text) && write_fixture_text(base + ".prj", wgs84));
		GDAL_CHECK(opened_by(path.c_str(), "PRF") && heightfield_samples(path.c_str()));
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("photomod.filename-kind", dem ? "x-dem" : "prf"),
		    BU_MIME_TRAIT_STRING("photomod.channel-type", type.channel), BU_MIME_TRAIT_STRING("photomod.channel-bytes", type.bytes),
		    BU_MIME_TRAIT_STRING("photomod.unit-offset", "true"), BU_MIME_TRAIT_STRING("photomod.channel-count", "1")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, &requested, true, ""));
		const bu_mime_trait field = BU_MIME_TRAIT_STRING("photomod.filename-kind", dem ? "prf" : "x-dem");
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, &wrong, false));
		if (dem) {
		    GDAL_CHECK(write_fixture_text(path, fixture::photomod(dem, type.channel, type.bytes, "1e-20")) && opened_by(path.c_str(), "PRF"));
		    GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, NULL, false));
		}
		std::string duplicate = text;
		duplicate.insert(duplicate.find("</phini>"), fixture::scalar("QChans", "1"));
		GDAL_CHECK(write_fixture_text(path, duplicate) && opened_by(path.c_str(), "PRF"));
		GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, NULL, false));
		std::string foreign = text;
		foreign.insert(foreign.find("</phini>"), "<s xmlns=\"urn:fixture:metadata\" n=\"QChans\" v=\"1\"/>");
		GDAL_CHECK(write_fixture_text(path, foreign) && opened_by(path.c_str(), "PRF"));
		GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, NULL, false));
		GDAL_CHECK(bu_file_delete(path.c_str()));
	    }
	GDAL_CHECK(science_tiff_tile(base + "/tile.tif", GDT_Byte));
	const std::string path = base + ".prf";
	GDALDriverH jp2 = GDALGetDriverByName("JP2OpenJPEG");
	if (jp2) {
	    GDALDatasetH source = GDALOpen((base + "/tile.tif").c_str(), GA_ReadOnly);
	    GDAL_CHECK(source);
	    char **options = CSLSetNameValue(NULL, "REVERSIBLE", "YES");
	    options = CSLSetNameValue(options, "QUALITY", "100");
	    options = CSLSetNameValue(options, "RESOLUTIONS", "1"); // The tile is only two pixels wide.
	    GDALDatasetH copy = GDALCreateCopy(jp2, (base + "/tile.jp2").c_str(), source, FALSE, options, NULL, NULL);
	    CSLDestroy(options);
	    GDALClose(source);
	    GDAL_CHECK(copy);
	    GDALClose(copy);
	    std::string text = fixture::photomod();
	    text.insert(text.find("</phini>"), fixture::scalar("parts_ext", "jp2"));
	    GDAL_CHECK(write_fixture_text(path, text) && opened_by(path.c_str(), "PRF") && heightfield_samples(path.c_str()));
	    const bu_mime_trait field = BU_MIME_TRAIT_STRING("photomod.parts-extension", "jp2");
	    const bu_mime_traits requested = {&field, 1};
	    GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, &requested, true));
	    text.replace(text.find("<s n=\"parts_ext\""), sizeof("<s") - 1, "<l");
	    GDAL_CHECK(write_fixture_text(path, text) && opened_by(path.c_str(), "PRF"));
	    GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, &requested, false));
	}
	std::string padded = std::string("\xef\xbb\xbf") + fixture::photomod() + "<!--" +
	    std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 32, ' ') + "-->";
	const bu_mime_trait field = BU_MIME_TRAIT_STRING("encoding", "utf-8");
	const bu_mime_traits utf8 = {&field, 1};
	GDAL_CHECK(write_fixture_text(path, padded) && opened_by(path.c_str(), "PRF"));
	GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, &utf8, true));
	padded[padded.size() - sizeof("-->")] = static_cast<char>(0xff);
	GDAL_CHECK(write_fixture_text(path, padded) && opened_by(path.c_str(), "PRF"));
	GDAL_CHECK(import_profile(path.c_str(), mime_raster_label::photomod_label, &utf8, false));
    }
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static int
heightfield_profiles(const char *tiff)
{
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    const struct {const char *driver; const char *type; const char *suffix; bool compressed;} cases[] = {
	{"HF2", mime_hf2::type, ".hf2", false}, {"HF2", mime_hf2::type, ".hfz", true},
	{"SIGDEM", mime_sigdem::type, ".sigdem", false}
    };
    for (const auto &sample : cases) {
	GDALDriverH driver = GDALGetDriverByName(sample.driver);
	if (!driver) continue;
	GDAL_CHECK(import_profile(tiff, sample.type, NULL, false));
	const std::string filename = std::string(temporary) + "/height" + sample.suffix;
	char **options = sample.compressed ? CSLSetNameValue(NULL, "COMPRESS", "YES") : NULL;
	GDALDatasetH copy = GDALCreateCopy(driver, filename.c_str(), source, FALSE, options, NULL, NULL);
	CSLDestroy(options);
	GDAL_CHECK(copy);
	GDALClose(copy);
	GDAL_CHECK(opened_by(filename.c_str(), sample.driver) && heightfield_samples(filename.c_str()));
	const bool hf2 = BU_STR_EQUAL(sample.driver, "HF2");
	const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	    BU_MIME_TRAIT_STRING("version", hf2 ? "0" : "1"),
	    BU_MIME_TRAIT_STRING(hf2 ? "compression" : "sample-format", hf2 ? (sample.compressed ? "gzip" : "none") : "signed-integer"),
	    BU_MIME_TRAIT_STRING(hf2 ? "hf2.sample-encoding" : "compression", hf2 ? "scaled-integer-deltas" : "none")};
	const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, true));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, true, ""));
	GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
	const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("sample-format", "ieee754"),
	    BU_MIME_TRAIT_STRING("version", "99"), BU_MIME_TRAIT_STRING("bit-depth", "64"),
	    BU_MIME_TRAIT_STRING("compression", "zip")};
	for (const auto &field : unsupported) {
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	}
	std::ifstream input(filename, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(input)), {});
	GDAL_CHECK(!input.bad() && !bytes.empty());
	const std::string neutral = std::string(temporary) + "/height.data";
	GDAL_CHECK(write_fixture_text(neutral, bytes));
	GDAL_CHECK(import_profile(neutral.c_str(), sample.type, &requested, true, ""));
	if (hf2) {
	    const bu_mime_trait wrong_compression = BU_MIME_TRAIT_STRING("compression", sample.compressed ? "none" : "gzip");
	    const bu_mime_traits wrong = {&wrong_compression, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	    if (sample.compressed) {
		const std::string compound = std::string(temporary) + "/height.hf2.gz";
		GDAL_CHECK(write_fixture_text(compound, bytes));
		GDAL_CHECK(import_profile(compound.c_str(), sample.type, &requested, true, ""));
	    }
	    const bu_mime_trait guessed_precision = BU_MIME_TRAIT_STRING("bit-depth", "32");
	    const bu_mime_traits precision = {&guessed_precision, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &precision, false));
	}
	if (!sample.compressed) {
	    mime_heightfield_fixture::word(bytes, hf2 ? mime_hf2::version_offset : mime_sigdem::version_offset,
		99, sizeof(uint16_t), hf2);
	    GDAL_CHECK(write_fixture_text(filename, bytes));
	    GDAL_CHECK(hf2 || opened_by(filename.c_str(), sample.driver));
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, false));
	}
    }
    GDALClose(source);
    if (GDALGetDriverByName("BYN"))
	for (size_t sample_size : {size_t(2), size_t(4)})
	    for (bool little : {false, true}) {
		const std::string filename = std::string(temporary) + (little ? "/grid.byn" : "/grid.err");
		std::string bytes = mime_heightfield_fixture::byn(sample_size, little);
		if (!little) mime_heightfield_fixture::word(bytes, 40, 1, sizeof(uint16_t));
		GDAL_CHECK(write_fixture_text(filename, bytes) && opened_by(filename.c_str(), "BYN") && heightfield_samples(filename.c_str()));
		const std::string bits = std::to_string(sample_size * 8);
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("bit-depth", bits.c_str()),
		    BU_MIME_TRAIT_STRING("sample-format", "signed-integer"),
		    BU_MIME_TRAIT_STRING("byte-order", little ? "little-endian" : "big-endian"),
		    BU_MIME_TRAIT_STRING("byn.unit-scale", "true")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(filename.c_str(), mime_byn::type, &requested, true, ""));
		GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
		const bu_mime_trait incorrect[] = {BU_MIME_TRAIT_STRING("byte-order", little ? "big-endian" : "little-endian"),
		    BU_MIME_TRAIT_STRING("bit-depth", sample_size == 2 ? "32" : "16"), BU_MIME_TRAIT_STRING("byn.unit-scale", "false")};
		for (const auto &field : incorrect) {
		    const bu_mime_traits wrong = {&field, 1};
		    GDAL_CHECK(import_profile(filename.c_str(), mime_byn::type, &wrong, false));
		}
		const std::string neutral = std::string(temporary) + "/height.data";
		GDAL_CHECK(write_fixture_text(neutral, bytes));
		GDAL_CHECK(import_profile(neutral.c_str(), mime_byn::type, NULL, false, ""));
		bytes = mime_heightfield_fixture::byn(sample_size, little, false);
		GDAL_CHECK(write_fixture_text(filename, bytes) && opened_by(filename.c_str(), "BYN"));
		GDALDatasetH scaled = GDALOpen(filename.c_str(), GA_ReadOnly);
		GDAL_CHECK(scaled);
		int has_scale = FALSE;
		const double scale = GDALGetRasterScale(GDALGetRasterBand(scaled, 1), &has_scale);
		GDALClose(scaled);
		GDAL_CHECK(has_scale && NEAR_EQUAL(scale, 0.5, SMALL_FASTF));
		GDAL_CHECK(import_profile(filename.c_str(), mime_byn::type, NULL, false));
	    }
    if (GDALGetDriverByName("ISG")) {
	const std::string filename = std::string(temporary) + "/height.isg";
	for (const char *version : {"1.0", "1.01", "2.0"})
	    for (bool dms : {false, true}) {
		if (dms && !BU_STR_EQUAL(version, "2.0")) continue;
		const std::string bytes = mime_heightfield_fixture::isg(version, dms);
		GDAL_CHECK(write_fixture_text(filename, bytes) && opened_by(filename.c_str(), "ISG") && heightfield_samples(filename.c_str()));
		const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("isg.format-version", version),
		    BU_MIME_TRAIT_STRING("isg.data-format", "grid"), BU_MIME_TRAIT_STRING("isg.coordinate-units", dms ? "dms" : "deg"),
		    BU_MIME_TRAIT_STRING("encoding", dms ? "utf-8" : "ascii")};
		const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
		GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, &requested, true, ""));
		GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
		const bu_mime_trait incorrect[] = {BU_MIME_TRAIT_STRING("isg.format-version", "1.1"),
		    BU_MIME_TRAIT_STRING("isg.data-format", "sparse"), BU_MIME_TRAIT_STRING("isg.coordinate-type", "projected")};
		for (const auto &field : incorrect) {
		    const bu_mime_traits wrong = {&field, 1};
		    GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, &wrong, false));
		}
		const std::string neutral = std::string(temporary) + "/height.data";
		GDAL_CHECK(write_fixture_text(neutral, bytes));
		GDAL_CHECK(import_profile(neutral.c_str(), mime_isg::type, NULL, true, ""));
		GDAL_CHECK(write_fixture_text(filename, std::string("\xef\xbb\xbf") + bytes));
		GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, NULL, true, ""));
	    }
	std::string duplicate = mime_heightfield_fixture::isg("2.0");
	duplicate.insert(duplicate.find("end_of_head"), "ISG format = 2.0\n");
	GDAL_CHECK(write_fixture_text(filename, duplicate) && opened_by(filename.c_str(), "ISG"));
	GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, NULL, false));
	GDAL_CHECK(write_fixture_text(filename, mime_heightfield_fixture::isg("3.0")) && opened_by(filename.c_str(), "ISG"));
	GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, NULL, false));
	const std::pair<const char *, const char *> unsupported[] = {
	    {"data format : grid", "data format : sparse"}, {"coord type : geodetic", "coord type : projected"},
	    {"data ordering : N-to-S, W-to-E", "data ordering : S-to-N, W-to-E"}
	};
	for (const auto &field : unsupported) {
	    std::string bytes = mime_heightfield_fixture::isg("2.0");
	    const size_t at = bytes.find(field.first);
	    GDAL_CHECK(at != std::string::npos);
	    bytes.replace(at, strlen(field.first), field.second);
	    GDAL_CHECK(write_fixture_text(filename, bytes));
	    GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, NULL, false));
	}
	std::string padded = std::string("\xef\xbb\xbf") + mime_heightfield_fixture::isg("2.0") +
	    std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 32, ' ');
	GDAL_CHECK(write_fixture_text(filename, padded) && opened_by(filename.c_str(), "ISG"));
	const bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", "utf-8");
	const bu_mime_traits utf8 = {&encoding, 1};
	GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, &utf8, true));
	padded.back() = static_cast<char>(0xff);
	GDAL_CHECK(write_fixture_text(filename, padded) && opened_by(filename.c_str(), "ISG"));
	GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, &utf8, false));
	for (size_t width : {size_t(2), size_t(4)}) {
	    std::string wide(width, 0);
	    wide[0] = static_cast<char>(0xff);
	    wide[1] = static_cast<char>(0xfe);
	    for (unsigned char c : mime_heightfield_fixture::isg("2.0")) {
		wide.push_back(static_cast<char>(c));
		wide.append(width - 1, 0);
	    }
	    GDAL_CHECK(write_fixture_text(filename, wide));
	    GDAL_CHECK(import_profile(filename.c_str(), mime_isg::type, NULL, false));
	}
    }
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

/* The SDK's size_t tag writer declares sizeof(size_t) while serializing
 * a 32-bit value. Normalize only those fixture tags to the wire size. */
static bool
leveller_fixture_lengths(std::string &bytes)
{
    constexpr size_t header_size = 5, word_size = sizeof(uint32_t);
    size_t at = header_size;
    while (at < bytes.size()) {
	const size_t name_size = static_cast<unsigned char>(bytes[at++]);
	if (name_size > bytes.size() - at || bytes.size() - at - name_size < word_size) return false;
	const std::string name = bytes.substr(at, name_size);
	at += name_size;
	size_t length = mime_binary::little_u32(reinterpret_cast<const unsigned char *>(bytes.data() + at));
	if (name == "hf_w" || name == "hf_b" || name == "coordsys_wktl") {
	    if (length != word_size && length != sizeof(size_t)) return false;
	    length = word_size;
	    for (size_t i = 0; i < word_size; ++i) bytes[at + i] = static_cast<char>(length >> (8 * i));
	}
	at += word_size;
	if (length > bytes.size() - at) return false;
	at += length;
    }
    return at == bytes.size();
}

static int
leveller_profiles(const char *tiff)
{
    GDALDriverH driver = GDALGetDriverByName("Leveller");
    if (!driver) return 0;
    const char *type = "application/leveller";
    GDAL_CHECK(import_profile(tiff, type, NULL, false));
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    const std::string filename = std::string(temporary) + "/height.ter";
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    char **arguments = CSLTokenizeString("-of MEM -ot Float32");
    GDALTranslateOptions *translation = GDALTranslateOptionsNew(arguments, NULL);
    CSLDestroy(arguments);
    GDALDatasetH floating = translation ? GDALTranslate("", source, translation, NULL) : NULL;
    GDALTranslateOptionsFree(translation);
    GDALClose(source);
    GDAL_CHECK(floating);
    char **options = CSLSetNameValue(NULL, "MINUSERPIXELVALUE", "0");
    GDALDatasetH copy = GDALCreateCopy(driver, filename.c_str(), floating, FALSE, options, NULL, NULL);
    CSLDestroy(options);
    GDALClose(floating);
    GDAL_CHECK(copy);
    GDALClose(copy);
    std::ifstream input(filename, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    GDAL_CHECK(!input.bad() && bytes.size() > 5);
    input.close();
    GDAL_CHECK(leveller_fixture_lengths(bytes));
    for (unsigned char revision : {4, 5, 6, 7, 8, 9, 10, 11, 12, 99}) {
	bytes[4] = static_cast<char>(revision);
	GDAL_CHECK(write_fixture_text(filename, bytes));
	const bool supported = revision >= 7 && revision <= 12;
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, supported));
	if (!supported) continue;
	GDAL_CHECK(opened_by(filename.c_str(), "Leveller"));
	const std::string version = std::to_string(revision);
	const bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("version", version.c_str()),
	    BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "32"),
	    BU_MIME_TRAIT_STRING("sample-format", "ieee754"), BU_MIME_TRAIT_STRING("leveller.reference-kind", "geodetic")};
	const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
	GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true, ""));
	GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
	const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("sample-format", "signed-fixed-point"),
	    BU_MIME_TRAIT_STRING("leveller.reference-kind", "local"), BU_MIME_TRAIT_STRING("bit-depth", "16")};
	for (const auto &field : unsupported) {
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &wrong, false));
	}
    }
    bytes[4] = 7;
    const std::string neutral = std::string(temporary) + "/height.data";
    GDAL_CHECK(write_fixture_text(neutral, bytes));
    GDAL_CHECK(import_profile(neutral.c_str(), type, NULL, true, ""));
    /* A raster/local coordinate declaration is valid Leveller, but this
     * terrain path requires a geodetic reference for its warp step. */
    const size_t geo = bytes.find("csclass");
    GDAL_CHECK(geo != std::string::npos && bytes.size() - geo >= sizeof("csclass") - 1 + 2 * sizeof(uint32_t));
    const size_t value = geo + sizeof("csclass") - 1 + sizeof(uint32_t);
    for (size_t i = 0; i < sizeof(uint32_t); ++i) bytes[value + i] = 0;
    GDAL_CHECK(write_fixture_text(filename, bytes) && opened_by(filename.c_str(), "Leveller"));
    GDAL_CHECK(import_profile(filename.c_str(), type, NULL, false));
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static bool
lerc_available()
{
    GDALDriverH driver = GDALGetDriverByName("MRF");
    const char *creation = driver ? GDALGetMetadataItem(driver, GDAL_DMD_CREATIONOPTIONLIST, NULL) : NULL;
    return creation && strstr(creation, "<Value>LERC</Value>");
}

static bool
lerc_sample_decodes(const char *filename)
{
    GDALDatasetH dataset = GDALOpen(filename, GA_ReadOnly);
    if (!dataset) return false;
    constexpr int side = 2;
    unsigned short values[side * side];
    const bool valid = GDALRasterIO(GDALGetRasterBand(dataset, 1), GF_Read, 0, 0, side, side,
	values, side, side, GDT_UInt16, 0, 0) == CE_None &&
	values[0] == 1 && values[1] == 2 && values[2] == 3 && values[3] == 4;
    GDALClose(dataset);
    return valid;
}

static int
lerc_profiles(const char *tiff)
{
    if (!lerc_available()) return 0;
    const char *type = "image/vnd.esri.lerc";
    GDAL_CHECK(import_profile(tiff, type, NULL, false));
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    GDAL_CHECK(bu_file_directory(temporary));
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    const std::string directory = std::string(temporary) + "/";
    const struct {const char *options; const char *codec; const char *revision;} cases[] = {
	{"V1:1", "lerc1", "11"}, {"L2_VER=2", "lerc2", "2"},
	{"L2_VER=3", "lerc2", "3"}, {"L2_VER=4", "lerc2", "4"}
    };
    for (const auto &sample : cases) {
	const std::string base = directory + sample.codec + sample.revision;
	const std::string label = base + ".mrf", payload = base + ".lrc";
	char **options = CSLSetNameValue(NULL, "COMPRESS", "LERC");
	options = CSLSetNameValue(options, "OPTIONS", sample.options);
	GDALDatasetH copy = GDALCreateCopy(GDALGetDriverByName("MRF"), label.c_str(), source, FALSE, options, NULL, NULL);
	CSLDestroy(options);
	GDAL_CHECK(copy);
	GDALClose(copy);
	GDAL_CHECK(opened_by(payload.c_str(), "MRF") && lerc_sample_decodes(payload.c_str()));
	const bu_mime_trait declared[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	    BU_MIME_TRAIT_STRING("lerc.first-blob.codec", sample.codec),
	    BU_MIME_TRAIT_STRING("lerc.first-blob.version", sample.revision),
	    BU_MIME_TRAIT_STRING("lerc.blob-count", "1")};
	const bu_mime_traits requested = {declared, sizeof(declared) / sizeof(declared[0])};
	GDAL_CHECK(import_profile(payload.c_str(), type, &requested, true));
	GDAL_CHECK(import_profile(payload.c_str(), type, &requested, true, ""));
	GDAL_CHECK(import_profile(payload.c_str(), "application/vnd.nasa.mrf+xml", NULL, false));
	GDAL_CHECK(import_profile(label.c_str(), type, NULL, false));
	const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("version", "2"),
	    BU_MIME_TRAIT_STRING("lerc.first-blob.version", "99"), BU_MIME_TRAIT_STRING("lerc.blob-count", "2"),
	    BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("byte-order", "big-endian")};
	for (const auto &field : unsupported) {
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(payload.c_str(), type, &wrong, false));
	}
	const bu_mime_trait word[] = {BU_MIME_TRAIT_STRING("bit-depth", "16"),
	    BU_MIME_TRAIT_STRING("sample-format", "unsigned-integer"), BU_MIME_TRAIT_STRING("lerc.first-blob.data-type", "uint16")};
	const bu_mime_traits typed = {word, sizeof(word) / sizeof(word[0])};
	const bool legacy = BU_STR_EQUAL(sample.codec, "lerc1");
	GDAL_CHECK(import_profile(payload.c_str(), type, &typed, !legacy));
	if (legacy) {
	    /* Its image type 8 is CntZ, not the SDK's default Byte view. */
	    const bu_mime_trait byte = BU_MIME_TRAIT_STRING("bit-depth", "8");
	    const bu_mime_traits guessed = {&byte, 1};
	    GDAL_CHECK(import_profile(payload.c_str(), type, &guessed, false));
	}
	std::ifstream input(payload, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(input)), {});
	GDAL_CHECK(!input.bad() && !bytes.empty());
	input.close();
	const std::string neutral = base + ".data";
	GDAL_CHECK(write_fixture_text(neutral, bytes));
	GDAL_CHECK(import_profile(neutral.c_str(), type, &requested, true, ""));
	GDAL_CHECK(write_fixture_text(neutral, bytes + bytes));
	GDAL_CHECK(import_profile(neutral.c_str(), type, NULL, false));
	if (BU_STR_EQUAL(sample.revision, "2")) {
	    /* Version 2 adds byte-type Huffman coding. The uint16 fixture
	     * uses the version-1 layout and coding, without a checksum. */
	    bytes[6] = 1;
	    GDAL_CHECK(write_fixture_text(neutral, bytes) && opened_by(neutral.c_str(), "MRF") && lerc_sample_decodes(neutral.c_str()));
	    const bu_mime_trait revision = BU_MIME_TRAIT_STRING("lerc.first-blob.version", "1");
	    const bu_mime_traits oldest = {&revision, 1};
	    GDAL_CHECK(import_profile(neutral.c_str(), type, &oldest, true, ""));
	}
    }
    GDALClose(source);
    /* A noisy full-size tile keeps global facts beyond the prefix budget. */
    constexpr int page_size = 512;
    GDALDatasetH noisy = GDALCreate(GDALGetDriverByName("MEM"), "", page_size, page_size, 1, GDT_UInt16, NULL);
    GDAL_CHECK(noisy);
    std::vector<unsigned short> values(page_size * page_size);
    uint32_t random = 1;
    for (auto &value : values) {
	/* Fixed LCG constants make the fixture reproducible. */
	random = random * UINT32_C(1664525) + UINT32_C(1013904223);
	value = static_cast<unsigned short>(random >> 16);
    }
    GDAL_CHECK(GDALRasterIO(GDALGetRasterBand(noisy, 1), GF_Write, 0, 0, page_size, page_size,
	values.data(), page_size, page_size, GDT_UInt16, 0, 0) == CE_None);
    const std::string label = directory + "large.mrf", payload = directory + "large.lrc";
    char **options = CSLSetNameValue(NULL, "COMPRESS", "LERC");
    options = CSLSetNameValue(options, "OPTIONS", "L2_VER=4");
    GDALDatasetH copy = GDALCreateCopy(GDALGetDriverByName("MRF"), label.c_str(), noisy, FALSE, options, NULL, NULL);
    CSLDestroy(options);
    GDALClose(noisy);
    GDAL_CHECK(copy);
    GDALClose(copy);
    gcv_context context;
    gcv_context_init(&context);
    bu_mime_result *result = NULL;
    GDAL_CHECK(bu_mime_detect_file(gcv_mime_registry(&context), payload.c_str(), &result) != BU_MIME_ERROR);
    const bu_mime_traits *facts = NULL;
    for (size_t i = 0; i < result->count; ++i)
	if (BU_STR_EQUAL(result->candidates[i].name, type)) facts = &result->candidates[i].traits;
    GDAL_CHECK(facts && BU_STR_EQUAL(bu_mime_traits_get(facts, "lerc.first-blob.data-type"), "uint16") &&
	!bu_mime_traits_get(facts, "bit-depth") && !bu_mime_traits_get(facts, "lerc.blob-count"));
    bu_mime_result_free(result);
    gcv_context_destroy(&context);
    const bu_mime_trait bit_depth = BU_MIME_TRAIT_STRING("bit-depth", "16");
    const bu_mime_traits requested = {&bit_depth, 1};
    GDAL_CHECK(import_profile(payload.c_str(), type, &requested, true, ""));
    const bu_mime_trait incorrect = BU_MIME_TRAIT_STRING("bit-depth", "8");
    const bu_mime_traits wrong = {&incorrect, 1};
    GDAL_CHECK(import_profile(payload.c_str(), type, &wrong, false, ""));
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static int
mrf_tar_profiles(const char *tiff)
{
    GDALDriverH driver = GDALGetDriverByName("MRF");
    if (!driver) return 0;
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    GDAL_CHECK(bu_file_directory(temporary));
    const std::string directory = std::string(temporary) + "/";
    const std::string label = directory + "label.mrf";
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    double original_transform[6];
    std::vector<unsigned char> original_samples;
    GDAL_CHECK(read_fixture_grid(source, original_transform, original_samples));
    char **options = CSLSetNameValue(NULL, "COMPRESS", "DEFLATE");
    GDALDatasetH copy = GDALCreateCopy(driver, label.c_str(), source, FALSE, options, NULL, NULL);
    CSLDestroy(options);
    GDALClose(source);
    GDAL_CHECK(copy);
    GDALClose(copy);
    std::vector<std::pair<std::string, std::string>> entries;
    /* MRF deliberately omits tile heaps and indexes from GetFileList(). */
    for (const char *name : {"label.mrf", "label.idx", "label.pzp", "label.mrf.aux.xml"}) {
	const std::string filename = directory + name;
	if (BU_STR_EQUAL(name, "label.mrf.aux.xml") && !bu_file_exists(filename.c_str(), NULL)) continue;
	std::ifstream input(filename, std::ios::binary);
	GDAL_CHECK(input.is_open());
	std::string content((std::istreambuf_iterator<char>(input)), {});
	GDAL_CHECK(!input.bad());
	entries.emplace_back(filename.substr(directory.size()), content);
    }
    GDAL_CHECK(!entries.empty() && entries.front().first == "label.mrf");
    for (const auto &entry : entries) GDAL_CHECK(bu_file_delete((directory + entry.first).c_str()));
    const char *type = "application/x-tar";
    const bu_mime_trait declared[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("container", "tar"),
	BU_MIME_TRAIT_STRING("tar.first-member-type", "application/vnd.nasa.mrf+xml"),
	BU_MIME_TRAIT_STRING("mrf.tile-compression", "DEFLATE")};
    const bu_mime_traits requested = {declared, sizeof(declared) / sizeof(declared[0])};
    const std::string filename = directory + "package.tar";
    for (auto format : {mime_tar_fixture::format::posix, mime_tar_fixture::format::gnu, mime_tar_fixture::format::empty_prefix_padding}) {
	std::string archive;
	for (const auto &entry : entries) archive += mime_tar_fixture::entry(entry.first, entry.second, format);
	archive.append(2 * mime_tar_fixture::block_size, '\0');
	GDAL_CHECK(write_fixture_text(filename, archive) && opened_by(filename.c_str(), "MRF"));
	GDALDatasetH packaged = GDALOpen(filename.c_str(), GA_ReadOnly);
	double transform[6];
	std::vector<unsigned char> samples;
	GDAL_CHECK(packaged && read_fixture_grid(packaged, transform, samples) && samples == original_samples);
	GDALClose(packaged);
	for (size_t i = 0; i < sizeof(transform) / sizeof(transform[0]); ++i)
	    GDAL_CHECK(EQUAL(transform[i], original_transform[i]));
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, true));
	GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true));
	GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true, ""));
	GDAL_CHECK(import_profile(filename.c_str(), "application/vnd.nasa.mrf+xml", NULL, false));
	const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", "utf-8"),
	    BU_MIME_TRAIT_STRING("container", "zip"), BU_MIME_TRAIT_STRING("tar.first-member-type", "application/vnd.nasa.pds4+xml"),
	    BU_MIME_TRAIT_STRING("mrf.tile-compression", "PNG"), BU_MIME_TRAIT_STRING("text.code-unit-width", "8"),
	    BU_MIME_TRAIT_STRING("version", "1")};
	for (const auto &field : unsupported) {
	    const bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &wrong, false));
	}
	const std::string neutral = directory + "package.data";
	GDAL_CHECK(write_fixture_text(neutral, archive));
	/* The SDK constructs a /vsitar/ path requiring a .tar suffix. */
	GDAL_CHECK(!opened_by(neutral.c_str(), "MRF"));
	GDAL_CHECK(import_profile(neutral.c_str(), type, &requested, false, ""));
	GDAL_CHECK(bu_file_delete(neutral.c_str()));
	const std::string unrelated = mime_tar_fixture::entry("notes.txt", "unrelated", format);
	GDAL_CHECK(write_fixture_text(filename, unrelated + archive));
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, false));
	std::string nested = mime_tar_fixture::entry("subdir/label.mrf", entries.front().second, format);
	GDAL_CHECK(write_fixture_text(filename, nested));
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, false));
    }
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static int
stac_profiles(const char *tiff)
{
    using json = nlohmann::json;
    char temporary[MAXPATHLEN];
    FILE *file = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
    bu_mkdir(temporary);
    GDAL_CHECK(bu_file_directory(temporary));
    const char *type = "application/geo+json";
    const std::string filename = std::string(temporary) + "/raster.json";
    const json base = {{"type", "Feature"}, {"id", "height"}, {"geometry", nullptr}, {"links", json::array()},
	{"stac_extensions", {"https://stac-extensions.github.io/projection/v1.1.0/schema.json"}},
	{"properties", {{"datetime", "2026-01-01T00:00:00Z"}, {"proj:epsg", 32632},
	    {"proj:shape", {2, 2}}, {"proj:bbox", {500000, 4999980, 500020, 5000000}},
	    {"proj:transform", {10, 0, 500000, 0, -10, 5000000}}}},
	{"assets", {{"height", {{"href", tiff}, {"type", "image/tiff"}, {"roles", {"data"}}}}}}};
    for (bool tiled : {false, true}) {
	const char *driver = tiled ? "STACTA" : "STACIT";
	if (!GDALGetDriverByName(driver)) continue;
	for (const char *version : {"0.9.0", "1.0.0-beta.2", "1.0.0", "1.1.0"}) {
	    json document = base;
	    document["stac_version"] = version;
	    if (tiled) {
		/* OGC scale denominators use the standard 0.28 mm reference pixel. */
		constexpr double cell_metres = 10, reference_pixel_metres = 0.00028;
		const json matrix = {{"type", "TileMatrixType"}, {"identifier", "0"},
		    {"scaleDenominator", cell_metres / reference_pixel_metres}, {"topLeftCorner", {500000, 5000000}},
		    {"tileWidth", 2}, {"tileHeight", 2}, {"matrixWidth", 1}, {"matrixHeight", 1}};
		const json matrix_set = {{"type", "TileMatrixSetType"}, {"title", "Fixture"}, {"identifier", "fixture"},
		    {"supportedCRS", "http://www.opengis.net/def/crs/EPSG/0/32632"}, {"tileMatrix", json::array({matrix})}};
		const json limits = {{"min_tile_col", 0}, {"max_tile_col", 0}, {"min_tile_row", 0}, {"max_tile_row", 0}};
		const json link = {{"limits", {{"0", limits}}}};
		document["properties"] = {{"datetime", "2026-01-01T00:00:00Z"},
		    {"tiles:tile_matrix_sets", {{"fixture", matrix_set}}}, {"tiles:tile_matrix_links", {{"fixture", link}}}};
		document.erase("assets");
		document["asset_templates"] = {{"height", {{"href", tiff}, {"type", "image/tiff"}}}};
		document["stac_extensions"] = json::array({BU_STR_EQUAL(version, "0.9.0") || BU_STR_EQUAL(version, "1.0.0-beta.2") ?
		    "tiled-assets" : "https://stac-extensions.github.io/tiled-assets/v1.0.0/schema.json"});
	    }
	    GDAL_CHECK(write_fixture_text(filename, document.dump()) && opened_by(filename.c_str(), driver));
	    GDAL_CHECK(import_profile(filename.c_str(), type, NULL, true));
	    const char *versions[] = {version};
	    const bu_mime_trait declared = BU_MIME_TRAIT_SET("stac.versions", versions, 1);
	    const bu_mime_traits requested = {&declared, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true));
	    if (!BU_STR_EQUAL(version, "1.0.0")) continue;
	    GDAL_CHECK(import_profile(filename.c_str(), type, NULL, true, ""));
	    const char *extensions[] = {tiled ? "https://stac-extensions.github.io/tiled-assets/v1.0.0/schema.json" :
		"https://stac-extensions.github.io/projection/v1.1.0/schema.json"};
	    const bu_mime_trait extension = BU_MIME_TRAIT_SET("stac.declared-extensions", extensions, 1);
	    const bu_mime_traits declared_extensions = {&extension, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &declared_extensions, true));
	    const bu_mime_trait none = BU_MIME_TRAIT_SET("stac.declared-extensions", NULL, 0);
	    const bu_mime_traits empty_extensions = {&none, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), type, &empty_extensions, false));
	    const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("version", "1.0.0"),
		BU_MIME_TRAIT_STRING("bit-depth", "16"), BU_MIME_TRAIT_STRING("encoding", "binary"),
		BU_MIME_TRAIT_STRING("stac.object-kind", "item-collection")};
	    for (const auto &field : unsupported) {
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(filename.c_str(), type, &wrong, false));
	    }
	    GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
	    std::string duplicate = document.dump();
	    duplicate.insert(duplicate.size() - 1, ",\"stac_version\":\"1.0.0\"");
	    GDAL_CHECK(write_fixture_text(filename, duplicate) && opened_by(filename.c_str(), driver));
	    GDAL_CHECK(import_profile(filename.c_str(), type, NULL, false));
	    if (!tiled) {
		document["zz_metadata"] = std::string(BU_MIME_DEFAULT_SCAN_LIMIT, 'p');
		GDAL_CHECK(write_fixture_text(filename, document.dump()) && opened_by(filename.c_str(), driver));
		GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true, type));
		const char *other_versions[] = {"1.1.0"};
		const bu_mime_trait different = BU_MIME_TRAIT_SET("stac.versions", other_versions, 1);
		const bu_mime_traits contradicted = {&different, 1};
		GDAL_CHECK(import_profile(filename.c_str(), type, &contradicted, false));
	    }
	    document["stac_version"] = "9.0.0";
	    GDAL_CHECK(write_fixture_text(filename, document.dump()) && opened_by(filename.c_str(), driver));
	    GDAL_CHECK(import_profile(filename.c_str(), type, NULL, false));
	}
    }
    if (GDALGetDriverByName("STACIT")) {
	json first = base, second = base;
	first["stac_version"] = "1.0.0";
	second["id"] = "other";
	second["stac_version"] = "1.1.0";
	const json collection = {{"type", "FeatureCollection"}, {"features", json::array({first, second})}, {"links", json::array()}};
	GDAL_CHECK(write_fixture_text(filename, collection.dump()) && opened_by(filename.c_str(), "STACIT"));
	const char *versions[] = {"1.0.0", "1.1.0"};
	const bu_mime_trait declared[] = {BU_MIME_TRAIT_SET("stac.versions", versions, 2),
	    BU_MIME_TRAIT_STRING("stac.object-kind", "item-collection")};
	const bu_mime_traits requested = {declared, 2};
	GDAL_CHECK(import_profile(filename.c_str(), type, &requested, true, ""));
	const bu_mime_trait partial = BU_MIME_TRAIT_SET("stac.versions", versions, 1);
	const bu_mime_traits incomplete = {&partial, 1};
	GDAL_CHECK(import_profile(filename.c_str(), type, &incomplete, false));
	json future = collection;
	future["features"][1]["stac_version"] = "9.0.0";
	GDAL_CHECK(write_fixture_text(filename, future.dump()) && opened_by(filename.c_str(), "STACIT"));
	GDAL_CHECK(import_profile(filename.c_str(), type, NULL, false));
    }
    bu_dirclear(temporary);
    GDAL_CHECK(!bu_file_exists(temporary, NULL));
    return 0;
}

static bool
pnm_fixtures(const char *tiff, const char *pgm, const char *ppm)
{
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    if (!source) return false;
    bool valid = copy_fixture("PNM", pgm, source);
    constexpr int side = 2, bands = 3;
    GDALDatasetH rgb = GDALCreate(GDALGetDriverByName("MEM"), "", side, side, bands, GDT_Byte, NULL);
    double transform[6];
    valid = valid && rgb && GDALGetGeoTransform(source, transform) == CE_None &&
	GDALSetGeoTransform(rgb, transform) == CE_None && GDALSetProjection(rgb, GDALGetProjectionRef(source)) == CE_None;
    unsigned char samples[side * side] = {1, 2, 3, 4};
    for (int band = 1; valid && band <= bands; ++band)
	valid = GDALRasterIO(GDALGetRasterBand(rgb, band), GF_Write, 0, 0, side, side,
	    samples, side, side, GDT_Byte, 0, 0) == CE_None;
    valid = valid && copy_fixture("PNM", ppm, rgb);
    if (rgb) GDALClose(rgb);
    GDALClose(source);
    return valid;
}

static bool
pad_header(const char *filename, size_t count)
{
    std::ifstream input(filename, std::ios::binary);
    const std::vector<char> data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad() || data.size() < 3 || data[0] != 'P' || data[2] != '\n') return false;
    input.close();
    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    output.write(data.data(), 3);
    output.put('#');
    const std::string comment(count, 'p');
    output.write(comment.data(), comment.size());
    output.put('\n');
    output.write(data.data() + 3, data.size() - 3);
    output.close();
    return !output.fail();
}

static int
pnm_profiles(const char *tiff)
{
    char pgm[MAXPATHLEN], ppm[MAXPATHLEN];
    FILE *stream = bu_temp_file(pgm, sizeof(pgm));
    GDAL_CHECK(stream && !fclose(stream));
    stream = bu_temp_file(ppm, sizeof(ppm));
    GDAL_CHECK(stream && !fclose(stream));
    GDAL_CHECK(pnm_fixtures(tiff, pgm, ppm));
    for (bool gray : {true, false}) {
	const char *filename = gray ? pgm : ppm;
	const char *type = gray ? "image/x-portable-graymap" : "image/x-portable-pixmap";
	const char *other_type = gray ? "image/x-portable-pixmap" : "image/x-portable-graymap";
	bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	    BU_MIME_TRAIT_STRING(gray ? "pgm.maxval" : "ppm.maxval", gray ? "65535" : "255"),
	    BU_MIME_TRAIT_STRING("bit-depth", gray ? "16" : "8"), BU_MIME_TRAIT_STRING("byte-order", "big-endian")};
	bu_mime_traits profile = {fields, gray ? sizeof(fields) / sizeof(fields[0]) : sizeof(fields) / sizeof(fields[0]) - 1};
	GDAL_CHECK(import_profile(filename, type, NULL, true));
	GDAL_CHECK(import_profile(filename, type, &profile, true));
	GDAL_CHECK(import_profile(filename, type, &profile, true, ""));
	GDAL_CHECK(import_profile(filename, type, &profile, true, "pnm"));
	GDAL_CHECK(import_profile(filename, other_type, NULL, false));
	bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", "ascii"),
	    BU_MIME_TRAIT_STRING("bit-depth", gray ? "8" : "16"), BU_MIME_TRAIT_STRING("byte-order", "little-endian"),
	    BU_MIME_TRAIT_STRING(gray ? "pgm.maxval" : "ppm.maxval", "1"), BU_MIME_TRAIT_STRING("version", "unestablished")};
	for (auto &field : unsupported) {
	    bu_mime_traits wrong = {&field, 1};
	    GDAL_CHECK(import_profile(filename, type, &wrong, false));
	}
	// Stay inside this SDK's own fixed header window for the positive case.
	const size_t comment_size = 256;
	GDAL_CHECK(pad_header(filename, comment_size));
	GDAL_CHECK(import_profile(filename, type, &profile, true));
	GDAL_CHECK(pad_header(filename, BU_MIME_DEFAULT_SCAN_LIMIT));
	GDAL_CHECK(import_profile(filename, type, NULL, false));
    }
    for (const char *filename : {pgm, ppm}) {
	GDAL_CHECK(bu_file_delete(filename));
	for (const char *suffix : {".aux.xml", ".wld"}) {
	    const std::string sidecar = std::string(filename) + suffix;
	    if (bu_file_exists(sidecar.c_str(), NULL)) GDAL_CHECK(bu_file_delete(sidecar.c_str()));
	}
    }
    return 0;
}

static int
zarr_profiles(const std::string &store, const char *version)
{
    const char *type = "application/vnd.zarr";
    const bu_mime_trait directory_fields[] = {
	BU_MIME_TRAIT_STRING("version", version), BU_MIME_TRAIT_STRING("zarr.entrypoint", "directory")
    };
    const bu_mime_traits directory = {directory_fields, 2};
    GDAL_CHECK(import_profile(store.c_str(), type, &directory, true, "application/vnd+zarr"));
    const bu_mime_trait unsupported[] = {
	BU_MIME_TRAIT_STRING("version", BU_STR_EQUAL(version, "2") ? "3" : "2"),
	BU_MIME_TRAIT_STRING("zarr.entrypoint", "metadata"), BU_MIME_TRAIT_STRING("bit-depth", "8")
    };
    for (const auto &field : unsupported) {
	const bu_mime_traits wrong = {&field, 1};
	GDAL_CHECK(import_profile(store.c_str(), type, &wrong, false));
    }
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 12, 0)
    size_t entrypoints = 0;
    for (const char *key : {"zarr.json", ".zarray", ".zgroup", ".zmetadata"}) {
	const std::string path = store + "/" + key;
	if (!bu_file_exists(path.c_str(), NULL)) continue;
	++entrypoints;
	const bu_mime_trait fields[] = {
	    BU_MIME_TRAIT_STRING("version", version), BU_MIME_TRAIT_STRING("zarr.entrypoint", "metadata")
	};
	const bu_mime_traits metadata = {fields, 2};
	GDAL_CHECK(import_profile(path.c_str(), type, &metadata, true, ""));
	GDAL_CHECK(import_profile(path.c_str(), type, &directory, false));
	if (BU_STR_EQUAL(key, "zarr.json")) {
	    std::ifstream input(path, std::ios::binary);
	    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
	    GDAL_CHECK(!input.bad() && bytes.find_last_of('}') != std::string::npos);
	    input.close();
	    const auto write_metadata = [&path](const std::string &text) {
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output << text;
		output.close();
		return !output.fail();
	    };
	    std::string duplicate = bytes;
	    duplicate.insert(duplicate.find_last_of('}'), ",\"zarr_format\":3");
	    GDAL_CHECK(write_metadata(duplicate));
	    /* The SDK accepts repeated declarations; MIME observations cannot
	     * choose one occurrence as authoritative. */
	    GDALDatasetH opened = GDALOpen(store.c_str(), GA_ReadOnly);
	    GDAL_CHECK(opened);
	    GDALClose(opened);
	    GDAL_CHECK(import_profile(store.c_str(), type, NULL, false));
	    GDAL_CHECK(import_profile(path.c_str(), type, NULL, false));
	    GDAL_CHECK(write_metadata(bytes));
	}
    }
    GDAL_CHECK(entrypoints);
#endif
    if (BU_STR_EQUAL(version, "3")) {
	const std::string conflicting = store + "/.zgroup";
	GDAL_CHECK(!bu_file_exists(conflicting.c_str(), NULL));
	std::ofstream marker(conflicting, std::ios::binary);
	marker << "{\"zarr_format\":2}";
	marker.close();
	GDAL_CHECK(!marker.fail());
	GDALDatasetH opened = GDALOpen(store.c_str(), GA_ReadOnly);
	GDAL_CHECK(opened);
	GDALClose(opened);
	GDAL_CHECK(import_profile(store.c_str(), type, NULL, false));
	GDAL_CHECK(bu_file_delete(conflicting.c_str()));
    }
    return 0;
}

static int
curated_profiles()
{
    constexpr int side = 2;
    GDALDatasetH source = GDALCreate(GDALGetDriverByName("MEM"), "", side, side, 1, GDT_Byte, NULL);
    GDAL_CHECK(source);
    unsigned char samples[side * side] = {1, 2, 3, 4};
    double transform[] = {10.0, 0.01, 0.0, 50.0, 0.0, -0.01};
    OGRSpatialReference reference;
    char *wkt = NULL;
    GDAL_CHECK(reference.SetWellKnownGeogCS("WGS84") == OGRERR_NONE && reference.exportToWkt(&wkt) == OGRERR_NONE);
    const bool initialized = GDALSetGeoTransform(source, transform) == CE_None && GDALSetProjection(source, wkt) == CE_None &&
	GDALRasterIO(GDALGetRasterBand(source, 1), GF_Write, 0, 0, side, side, samples, side, side, GDT_Byte, 0, 0) == CE_None;
    CPLFree(wkt);
    GDAL_CHECK(initialized);
    using properties = std::vector<std::pair<const char *, const char *>>;
    const struct {
	const char *driver;
	const char *type;
	properties options;
	properties expected;
	GDALDataType data_type = GDT_Byte;
	const char *extension = NULL;
	bool automatic = true;
	int bands = 1;
    } cases[] = {
	{"Zarr", "application/vnd.zarr", {{"FORMAT", "ZARR_V2"}}, {}, GDT_Byte, ".zarr"},
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 8, 0)
	{"Zarr", "application/vnd.zarr", {{"FORMAT", "ZARR_V3"}}, {}, GDT_Byte, ".zarr"},
#endif
	{"VICAR", "application/vnd.nasa.vicar", {{"GEOREF_FORMAT", "GEOTIFF"}}, {{"vicar.header-layout", "lblsize"}}, GDT_Byte, ".vicar"},
	{"CALS", "application/cals-1840", {}, {{"cals.header-layout", "rtype"}, {"cals.raster-type", "1"}, {"encoding", "binary"}, {"bit-depth", "1"}, {"compression", "ccitt-group4"}}, GDT_Byte, ".ct1"},
	{"PCIDSK", "application/x-pcidsk", {}, {{"encoding", "binary"}}, GDT_Byte, ".pix"},
	{"ISIS3", "application/x-isis3", {}, {}, GDT_Byte, ".cub"},
	{"KRO", "application/x-kro", {}, {}, GDT_Byte, ".kro"},
	{"LCP", "application/x-lcp", {}, {}, GDT_Int16, ".lcp", true, 5},
	{"ENVI", "application/x-envi", {}, {}, GDT_Byte, "", false},
	{"ERS", "application/x-ers", {}, {}, GDT_Byte, ".ers"},
	{"RMF", "application/x-rmf", {}, {}, GDT_Byte, ".rsw"},
	{"PCRaster", "image/x-pcraster", {}, {}, GDT_Float32, ".map"},
	{"ILWIS", "image/ilwis", {}, {}, GDT_Byte, ".mpr"},
	{"RST", "application/x-rst", {}, {}, GDT_Byte, ".rst"},
	{"GTX", "application/x-gtx", {}, {}, GDT_Float32, ".gtx"},
	{"SAGA", "application/x-sdat", {}, {}, GDT_Byte, ".sdat"},
	{"DTED", "application/dted", {}, {{"encoding", "binary"}, {"dted.designator", "DTED0"}, {"dted.level", "0"}, {"bit-depth", "16"}, {"byte-order", "big-endian"}}},
	{"GXF", "application/x-gxf", {}, {{"encoding", "ascii"}, {"gxf.gtype", "0"}, {"gxf.value-encoding", "decimal"}}},
	{"GXF", "application/x-gxf", {{"GTYPE", "1"}}, {{"encoding", "ascii"}, {"gxf.gtype", "1"}, {"gxf.value-encoding", "base90"}}},
	{"GXF", "application/x-gxf", {{"GTYPE", "2"}}, {{"encoding", "ascii"}, {"gxf.gtype", "2"}, {"gxf.value-encoding", "base90"}}},
	{"ZMap", "application/x-zmap", {}, {{"encoding", "ascii"}, {"zmap.field-width", "20"}, {"zmap.values-per-line", "4"}}},
	{"GRASSASCIIGrid", "application/grass-ascii-grid", {}, {{"grass-grid.serialization", "text"}, {"encoding", "ascii"}, {"grass-grid.text-bom", "false"}}},
	{"XYZ", "application/x-ascii-xyz", {}, {}},
	{"XYZ", "application/x-ascii-xyz", {{"ADD_HEADER_LINE", "YES"}, {"COLUMN_SEPARATOR", ","}}, {}},
	{"GSAG", "image/vnd.goldensoftware.surfer-grid", {}, {{"surfer.layout", "classic"}, {"surfer.serialization", "text"}, {"encoding", "ascii"}}},
	{"GSBG", "image/vnd.goldensoftware.surfer-grid", {}, {{"surfer.layout", "classic"}, {"surfer.serialization", "binary"}, {"encoding", "binary"}, {"bit-depth", "32"}}},
	{"GS7BG", "image/vnd.goldensoftware.surfer-grid", {}, {{"surfer.layout", "tagged"}, {"surfer.serialization", "binary"}, {"surfer.header-version", "1"}, {"bit-depth", "64"}}},
	{"HFA", "application/x-erdas-hfa", {}, {}},
	{"HFA", "application/x-erdas-hfa", {{"COMPRESSED", "YES"}}, {}},
	{"AAIGrid", "application/x-ascii-grid", {}, {{"esri-grid.serialization", "text"}, {"encoding", "ascii"}, {"text.code-unit-width", "8"}, {"esri-grid.text-bom", "false"}}},
	{"BT", "application/x-bt", {}, {{"version", "1.3"}, {"bit-depth", "16"}, {"sample-format", "signed-integer"}}, GDT_Int16},
	{"BT", "application/x-bt", {}, {{"version", "1.3"}, {"bit-depth", "32"}, {"sample-format", "signed-integer"}}, GDT_Int32},
	{"BT", "application/x-bt", {}, {{"version", "1.3"}, {"bit-depth", "32"}, {"sample-format", "ieee754"}}, GDT_Float32},
	{"BMP", "image/bmp", {}, {{"bmp.header-size", "40"}, {"bit-depth", "8"}, {"bmp.compression-code", "0"}}},
	{"NITF", "application/vnd.nitf", {{"FHDR", "NITF02.10"}, {"ICORDS", "G"}},
	    {{"nitf.standard", "nitf"}, {"nitf.header-version", "02.10"}, {"version", "2.1"}}},
	{"NITF", "application/vnd.nitf", {{"FHDR", "NSIF01.00"}, {"ICORDS", "G"}},
	    {{"nitf.standard", "nsif"}, {"nitf.header-version", "01.00"}, {"version", "1.0"}}},
	{"NITF", "application/vnd.nitf", {{"FHDR", "NITF02.00"}, {"ICORDS", "G"}},
	    {{"nitf.standard", "nitf"}, {"nitf.header-version", "02.00"}, {"version", "2.0"}}},
	{"GPKG", "application/geopackage+sqlite3", {{"VERSION", "1.2"}},
	    {{"container", "sqlite3"}, {"geopackage.application-id", "GPKG"}, {"geopackage.user-version", "10200"}, {"version", "1.2"}}},
	{"GRIB", "application/grib", {}, {{"grib.first-edition", "2"}}}
    };
    gcv_context inspection;
    gcv_context_init(&inspection);
    for (const auto &sample : cases) {
	GDALDriverH driver = GDALGetDriverByName(sample.driver);
	if (!driver) continue;
	bool supported = true;
	for (const auto &option : sample.options)
	    if (BU_STR_EQUAL(sample.driver, "NITF") && BU_STR_EQUAL(option.first, "FHDR") && BU_STR_EQUAL(option.second, "NITF02.00")) {
		const char *creation = GDALGetMetadataItem(driver, GDAL_DMD_CREATIONOPTIONLIST, NULL);
		supported = creation && strstr(creation, option.second);
	    }
	if (!supported) continue;
	char temporary[MAXPATHLEN];
	FILE *file = bu_temp_file(temporary, sizeof(temporary));
	GDAL_CHECK(file && !fclose(file) && bu_file_delete(temporary));
	const char *extension = sample.extension ? sample.extension : BU_STR_EQUAL(sample.driver, "GPKG") ? ".gpkg" :
	    BU_STR_EQUAL(sample.driver, "XYZ") ? ".xyz" :
	    BU_STR_EQUAL(sample.driver, "AAIGrid") || BU_STR_EQUAL(sample.driver, "GRASSASCIIGrid") ? ".asc" : "";
	const std::string filename = std::string(temporary) + extension;
	char **options = NULL;
	for (const auto &option : sample.options) options = CSLSetNameValue(options, option.first, option.second);
	GDALDatasetH typed = NULL;
	if (sample.data_type != GDT_Byte || sample.bands != 1 || BU_STR_EQUAL(sample.driver, "CALS")) {
	    typed = GDALCreate(GDALGetDriverByName("MEM"), "", side, side, sample.bands, sample.data_type, NULL);
	    GDAL_CHECK(typed && GDALSetGeoTransform(typed, transform) == CE_None &&
		GDALSetProjection(typed, GDALGetProjectionRef(source)) == CE_None &&
		GDALRasterIO(GDALGetRasterBand(typed, 1), GF_Write, 0, 0, side, side, samples, side, side, GDT_Byte, 0, 0) == CE_None);
	}
	if (BU_STR_EQUAL(sample.driver, "CALS")) {
	    unsigned char bilevel[side * side] = {0, 1, 1, 0};
	    GDALRasterBandH band = GDALGetRasterBand(typed, 1);
	    GDAL_CHECK(GDALSetMetadataItem(band, "NBITS", "1", "IMAGE_STRUCTURE") == CE_None &&
		GDALRasterIO(band, GF_Write, 0, 0, side, side, bilevel, side, side, GDT_Byte, 0, 0) == CE_None);
	}
	const bool copied = copy_fixture(sample.driver, filename.c_str(), typed ? typed : source, options);
	if (typed) GDALClose(typed);
	CSLDestroy(options);
	GDAL_CHECK(copied);
	bu_mime_result *detected = NULL;
	const bool directory = bu_file_directory(filename.c_str());
	GDAL_CHECK((directory ? bu_mime_detect(gcv_mime_registry(&inspection), filename.c_str(), NULL, 0, 0, &detected) :
	    bu_mime_detect_file(gcv_mime_registry(&inspection), filename.c_str(), &detected)) != BU_MIME_ERROR);
	const bu_mime_traits *facts = NULL;
	for (size_t i = 0; i < detected->count; ++i)
	    if (BU_STR_EQUAL(detected->candidates[i].name, sample.type)) facts = &detected->candidates[i].traits;
	GDAL_CHECK(facts || !sample.automatic);
	GDAL_CHECK(!directory || (facts && !facts->count));
	for (const auto &property : sample.expected) {
	    const char *actual = bu_mime_traits_get(facts, property.first);
	    const bool matches = actual && BU_STR_EQUAL(actual, property.second);
	    if (!matches) bu_log("%s property %s: expected %s, got %s\n", sample.driver,
		property.first, property.second, actual ? actual : "unobserved");
	    GDAL_CHECK(matches);
	}
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true));
	if (sample.automatic) GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true, ""));
	GDAL_CHECK(import_profile(filename.c_str(), sample.type, facts, true, sample.type));
	const bool text = BU_STR_EQUAL(sample.driver, "AAIGrid") || BU_STR_EQUAL(sample.driver, "GRASSASCIIGrid") ||
	    BU_STR_EQUAL(sample.driver, "XYZ") || BU_STR_EQUAL(sample.driver, "GSAG") ||
	    BU_STR_EQUAL(sample.driver, "GXF") || BU_STR_EQUAL(sample.driver, "ZMap");
	const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", text ? "binary" : "ascii"),
	    BU_MIME_TRAIT_STRING("fixture.unestablished", "value"), BU_MIME_TRAIT_STRING("version", "unestablished")};
	for (const auto &field : unsupported) {
	    const bu_mime_traits invalid = {&field, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &invalid, false));
	}
	GDAL_CHECK(import_profile(filename.c_str(), "image/tiff", NULL, false));
	if (BU_STR_EQUAL(sample.driver, "Zarr"))
	    GDAL_CHECK(!zarr_profiles(filename, BU_STR_EQUAL(sample.options.front().second, "ZARR_V2") ? "2" : "3"));
	if (BU_STR_EQUAL(sample.driver, "VICAR")) {
	    const bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", "binary");
	    const bu_mime_traits binary = {&encoding, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &binary, true));
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &binary, true, sample.type));
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true, "img"));
	    GDAL_CHECK(facts && facts->count == 1 && !bu_mime_traits_get(facts, "encoding"));
	    const bu_mime_trait order = BU_MIME_TRAIT_STRING("byte-order", "little-endian");
	    const bu_mime_traits unobserved = {&order, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &unobserved, false));
	    const bu_mime_trait legacy = BU_MIME_TRAIT_STRING("vicar.header-layout", "njpl1i");
	    const bu_mime_traits other_layout = {&legacy, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &other_layout, false));
	    GDAL_CHECK(vicar_wrapper_identity(filename.c_str(), side));
	}
	if (BU_STR_EQUAL(sample.driver, "CALS")) {
	    for (const auto &property : properties{{"cals.header-layout", "dtype"}, {"cals.raster-type", "2"}, {"bit-depth", "8"}, {"compression", "none"}, {"byte-order", "little-endian"}}) {
		const bu_mime_trait field = BU_MIME_TRAIT_STRING(property.first, property.second);
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	    }
	    // The SDK's substring identification also accepts "rtype: 10".
	    // Full-header validation must enforce the declared Type 1 subset.
	    std::fstream header(filename, std::ios::in | std::ios::out | std::ios::binary);
	    constexpr std::streamoff type_offset = 6 * 128;
	    header.seekp(type_offset);
	    header.write("rtype: 10", strlen("rtype: 10"));
	    header.close();
	    GDAL_CHECK(!header.fail());
	    GDALDatasetH loose = GDALOpen(filename.c_str(), GA_ReadOnly);
	    GDAL_CHECK(loose && BU_STR_EQUAL(GDALGetDriverShortName(GDALGetDatasetDriver(loose)), "CALS"));
	    GDALClose(loose);
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, false));
	}
	if (BU_STR_EQUAL(sample.driver, "BMP"))
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, facts, true, "image/x-ms-bmp"));
	if (BU_STR_EQUAL(sample.driver, "GRIB")) GDAL_CHECK(!bu_mime_traits_get(facts, "version"));
	bu_mime_result_free(detected);
	if (BU_STR_EQUAL(sample.driver, "GXF") || BU_STR_EQUAL(sample.driver, "ZMap")) {
	    const bu_mime_trait word_size = BU_MIME_TRAIT_STRING("bit-depth", "32");
	    const bu_mime_traits wrong = {&word_size, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	}
	if (BU_STR_EQUAL(sample.driver, "GXF")) {
	    GDAL_CHECK(!gcv_select_filter(&inspection, GCV_FILTER_READ, "application/gxf", "GDAL Reader", NULL));
	    if (sample.options.empty()) {
		const std::string inline_file = filename + ".inline";
		char **creation = NULL;
		creation = CSLSetNameValue(creation, "GTYPE", "2");
		creation = CSLSetNameValue(creation, "INLINE_GTYPE", "YES");
		const bool written = write_gxf_fixture(inline_file.c_str(), source, creation);
		CSLDestroy(creation);
		GDAL_CHECK(written);
		GDALDatasetH decoded = GDALOpen(inline_file.c_str(), GA_ReadOnly);
		GDAL_CHECK(decoded);
		GDALClose(decoded);
		GDAL_CHECK(import_profile(inline_file.c_str(), sample.type, NULL, false));
		GDAL_CHECK(bu_file_delete(inline_file.c_str()));
	    }
	    const bu_mime_trait contrary = BU_MIME_TRAIT_STRING("gxf.value-encoding", sample.options.empty() ? "base90" : "decimal");
	    const bu_mime_traits wrong = {&contrary, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	}
	if (BU_STR_EQUAL(sample.driver, "XYZ")) {
	    const bu_mime_trait regular = BU_MIME_TRAIT_STRING("xyz.topology", "regular-grid");
	    const bu_mime_traits topology = {&regular, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &topology, true, "application/x-xyz"));
	    const bu_mime_trait irregular = BU_MIME_TRAIT_STRING("xyz.topology", "unstructured");
	    const bu_mime_traits wrong = {&irregular, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	    GDAL_CHECK(!gcv_select_filter(&inspection, GCV_FILTER_READ, "chemical/x-xyz", "GDAL Reader", NULL));
	}
	if (BU_STR_EQUAL(sample.driver, "GS7BG")) {
	    for (uint32_t version : {uint32_t(2), uint32_t(99)}) {
		std::fstream header(filename, std::ios::binary | std::ios::in | std::ios::out);
		header.seekp(8);
		for (size_t byte = 0; byte < sizeof(version); ++byte) header.put(static_cast<char>(version >> (8 * byte)));
		header.close();
		GDAL_CHECK(!header.fail());
		const auto number = std::to_string(version);
		const bu_mime_trait field = BU_MIME_TRAIT_STRING("surfer.header-version", number.c_str());
		const bu_mime_traits requested = {&field, 1};
		GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, version == 2));
	    }
	    std::fstream restored(filename, std::ios::binary | std::ios::in | std::ios::out);
	    restored.seekp(8);
	    restored.write("\1\0\0\0", 4);
	    restored.close();
	    GDAL_CHECK(!restored.fail());
	}
	if (BU_STR_EQUAL(sample.driver, "GSAG") || BU_STR_EQUAL(sample.driver, "GSBG") || BU_STR_EQUAL(sample.driver, "GS7BG")) {
	    const bool tagged = BU_STR_EQUAL(sample.driver, "GS7BG");
	    const bu_mime_trait contrary[] = {BU_MIME_TRAIT_STRING("surfer.layout", tagged ? "classic" : "tagged"),
		BU_MIME_TRAIT_STRING("surfer.serialization", text ? "binary" : "text")};
	    for (const auto &field : contrary) {
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	    }
	    const char *alias = text ? "application/x-gsag" : tagged ? "application/x-gs7bg" : "application/x-gsbg";
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true, alias));
	}
	if (BU_STR_EQUAL(sample.driver, "DTED")) {
	    const bu_mime_trait contrary[] = {BU_MIME_TRAIT_STRING("dted.level", "1"),
		BU_MIME_TRAIT_STRING("byte-order", "little-endian"), BU_MIME_TRAIT_STRING("bit-depth", "32")};
	    for (const auto &field : contrary) {
		const bu_mime_traits wrong = {&field, 1};
		GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	    }
	    constexpr std::streamoff level_offset = 80 + 59 + 4;
	    std::fstream header(filename, std::ios::binary | std::ios::in | std::ios::out);
	    header.seekp(level_offset);
	    header.put('9');
	    header.close();
	    GDAL_CHECK(!header.fail());
	    const bu_mime_trait level = BU_MIME_TRAIT_STRING("dted.level", "9");
	    const bu_mime_traits requested = {&level, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, true));
	}
	if (BU_STR_EQUAL(sample.driver, "BT")) {
	    const bu_mime_trait unsupported_layout[] = {BU_MIME_TRAIT_STRING("byte-order", "big-endian"),
		BU_MIME_TRAIT_STRING("bit-depth", "64"), BU_MIME_TRAIT_STRING("sample-format", "unsigned-integer")};
	    for (const auto &field : unsupported_layout) {
		const bu_mime_traits invalid = {&field, 1};
		GDAL_CHECK(import_profile(filename.c_str(), sample.type, &invalid, false));
	    }
	    std::fstream future(filename, std::ios::in | std::ios::out | std::ios::binary);
	    future.seekp(7);
	    future.write("9.9", 3);
	    future.close();
	    GDAL_CHECK(!future.fail());
	    const bu_mime_trait future_version = BU_MIME_TRAIT_STRING("version", "9.9");
	    const bu_mime_traits expected = {&future_version, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &expected, true));
	}
	if (BU_STR_EQUAL(sample.driver, "AAIGrid")) {
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, true, "application/arcgrid"));
	}
	if (BU_STR_EQUAL(sample.driver, "AAIGrid") || BU_STR_EQUAL(sample.driver, "GXF") || BU_STR_EQUAL(sample.driver, "ZMap")) {
	    std::ofstream tail(filename, std::ios::binary | std::ios::app);
	    tail << "\n#" << std::string(BU_MIME_DEFAULT_SCAN_LIMIT, ' ') << "caf\xc3\xa9\n";
	    tail.close();
	    GDAL_CHECK(!tail.fail() && import_profile(filename.c_str(), sample.type, NULL, true));
	    const bu_mime_trait charsets[] = {BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("encoding", "utf-8")};
	    for (size_t i = 0; i < sizeof(charsets) / sizeof(charsets[0]); ++i) {
		const bu_mime_traits requested = {charsets + i, 1};
		GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, i != 0));
	    }
	    std::ifstream source_bytes(filename, std::ios::binary);
	    const std::vector<char> bytes((std::istreambuf_iterator<char>(source_bytes)), std::istreambuf_iterator<char>());
	    GDAL_CHECK(!source_bytes.bad());
	    source_bytes.close();
	    for (bool wide : {false, true}) {
		std::ofstream marked(filename, std::ios::binary | std::ios::trunc);
		if (wide) marked.write("\xff\xfe", 2);
		else marked.write("\xef\xbb\xbf", 3);
		for (char byte : bytes) {marked.put(byte); if (wide) marked.put('\0');}
		marked.close();
		GDAL_CHECK(!marked.fail() && import_profile(filename.c_str(), sample.type, NULL, false));
	    }
	    std::ofstream restored(filename, std::ios::binary | std::ios::trunc);
	    restored.write(bytes.data(), bytes.size());
	    restored.close();
	    GDAL_CHECK(!restored.fail());
	}
	if (BU_STR_EQUAL(sample.driver, "RST")) {
	    const bu_mime_trait length = BU_MIME_TRAIT_STRING("raster.value-dimension", "length");
	    const bu_mime_traits length_profile = {&length, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &length_profile, true));
	    const bu_mime_trait angle = BU_MIME_TRAIT_STRING("raster.value-dimension", "angle");
	    const bu_mime_traits wrong = {&angle, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &wrong, false));
	    GDALDatasetH imperial = GDALOpen(filename.c_str(), GA_Update);
	    GDAL_CHECK(imperial && GDALSetRasterUnitType(GDALGetRasterBand(imperial, 1), "ft") == CE_None);
	    GDALClose(imperial);
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &length_profile, true));
	    GDALDatasetH unspecified = GDALOpen(filename.c_str(), GA_Update);
	    GDAL_CHECK(unspecified && GDALSetRasterUnitType(GDALGetRasterBand(unspecified, 1), "unspecified") == CE_None);
	    GDALClose(unspecified);
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, NULL, false));
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &length_profile, false));
	    GDALDatasetH restored = GDALOpen(filename.c_str(), GA_Update);
	    GDAL_CHECK(restored && GDALSetRasterUnitType(GDALGetRasterBand(restored, 1), "m") == CE_None);
	    GDALClose(restored);
	}
	if (BU_STR_EQUAL(sample.driver, "GPKG")) {
	    const std::string archive = filename + ".zip";
	    const std::string member = "/vsizip/" + archive + "/raster.gpkg";
	    GDAL_CHECK(CPLCopyFile(member.c_str(), filename.c_str()) == 0);
	    GDAL_CHECK(import_profile(archive.c_str(), sample.type, NULL, false));
	    GDAL_CHECK(bu_file_delete(archive.c_str()));
	}
	if (BU_STR_EQUAL(sample.driver, "GRIB")) {
	    std::ifstream input(filename, std::ios::binary);
	    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	    GDAL_CHECK(!input.bad());
	    input.close();
	    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
	    output << "ZCZC\r\r\nTEST00 TEST 010000\r\r\n";
	    output.write(bytes.data(), bytes.size());
	    output.close();
	    GDAL_CHECK(output.good());
	    const bu_mime_trait edition = BU_MIME_TRAIT_STRING("grib.first-edition", "2");
	    const bu_mime_traits requested = {&edition, 1};
	    GDAL_CHECK(import_profile(filename.c_str(), sample.type, &requested, true, "application/x-grib"));
	}
	if (directory) {
	    bu_dirclear(filename.c_str());
	    GDAL_CHECK(!bu_file_exists(filename.c_str(), NULL));
	} else GDAL_CHECK(GDALDeleteDataset(driver, filename.c_str()) == CE_None);
	for (const char *suffix : {".aux.xml", ".wld"}) {
	    const std::string sidecar = filename + suffix;
	    if (bu_file_exists(sidecar.c_str(), NULL)) GDAL_CHECK(bu_file_delete(sidecar.c_str()));
	}
    }
    gcv_context_destroy(&inspection);
    GDALClose(source);
    return 0;
}

static int
vrt_profile(const char *tiff)
{
    if (!GDALGetDriverByName("VRT")) return 0;
    char temporary[MAXPATHLEN];
    FILE *stream = bu_temp_file(temporary, sizeof(temporary));
    GDAL_CHECK(stream && !fclose(stream) && bu_file_delete(temporary));
    const std::string filename = std::string(temporary) + ".vrt";
    GDALDatasetH source = GDALOpen(tiff, GA_ReadOnly);
    GDAL_CHECK(source);
    const bool copied = copy_fixture("VRT", filename.c_str(), source);
    GDALClose(source);
    GDAL_CHECK(copied);
    GDAL_CHECK(import_profile(filename.c_str(), "application/vrt", NULL, true));
    GDAL_CHECK(import_profile(filename.c_str(), "application/vrt", NULL, true, ""));
    const bu_mime_trait version = BU_MIME_TRAIT_STRING("version", "unestablished");
    const bu_mime_traits wrong = {&version, 1};
    GDAL_CHECK(import_profile(filename.c_str(), "application/vrt", &wrong, false));
    GDAL_CHECK(GDALDeleteDataset(GDALGetDriverByName("VRT"), filename.c_str()) == CE_None);
    return 0;
}

static bool
readable_raster(GDALDriverH driver)
{
    if (!driver) return false;
    const char *raster = GDALGetMetadataItem(driver, GDAL_DCAP_RASTER, NULL);
    const char *open = GDALGetMetadataItem(driver, GDAL_DCAP_OPEN, NULL);
    return raster && open && CPLTestBool(raster) && CPLTestBool(open);
}

static int
inventory(void)
{
    gcv_context context;
    gcv_context_init(&context);
    std::set<std::string> expected;
    size_t readable = 0, omitted = 0;
    const struct {const char *driver; const char *type;} audited[] = {
        {"PCIDSK", "application/x-pcidsk"}, {"RST", "application/x-rst"},
        {"DTED", "application/dted"}, {"GXF", "application/x-gxf"}, {"ZMap", "application/x-zmap"},
        {"NOAA_B", mime_noaa_grid::type}, {"IRIS", mime_iris::type}, {"IRIS", mime_iris::ppi_type},
        {"IRIS", mime_iris::cappi_type}, {"PRF", mime_raster_label::photomod_label},
        {"ESRIC", mime_esric::xml_type}, {"ESRIC", mime_esric::json_type}, {"ESRIC", mime_esric::zip_type},
        {"RRASTER", mime_raster_label::rraster_label}, {"ROI_PAC", mime_raster_label::roipac_data},
        {"ISCE", mime_raster_label::isce_data}, {"EHdr", mime_raster_label::ehdr_data}, {"EHdr", mime_raster_label::bil_data},
        {"HF2", mime_hf2::type}, {"SIGDEM", mime_sigdem::type}, {"BYN", mime_byn::type}, {"ISG", mime_isg::type},
        {"Leveller", "application/leveller"}, {"PDS4", "application/vnd.nasa.pds4+xml"}, {"MRF", "application/vnd.nasa.mrf+xml"}, {"MRF", "application/x-tar"},
        {"STACTA", "application/geo+json"}, {"STACIT", "application/geo+json"},
        {"KMLSUPEROVERLAY", "application/vnd.google-earth.kml+xml"}, {"KMLSUPEROVERLAY", "application/vnd.google-earth.kmz"},
        {"Zarr", "application/vnd.zarr"}, {"VICAR", "application/vnd.nasa.vicar"}, {"CALS", "application/cals-1840"}, {"BMP", "image/bmp"}, {"NITF", "application/vnd.nitf"},
        {"GPKG", "application/geopackage+sqlite3"}, {"GRIB", "application/grib"},
        {"HFA", "application/x-erdas-hfa"}, {"AAIGrid", "application/x-ascii-grid"}, {"BT", "application/x-bt"}, {"GRASSASCIIGrid", "application/grass-ascii-grid"},
        {"XYZ", "application/x-ascii-xyz"}, {"GSAG", "image/vnd.goldensoftware.surfer-grid"},
        {"GSBG", "image/vnd.goldensoftware.surfer-grid"}, {"GS7BG", "image/vnd.goldensoftware.surfer-grid"}
    };
    for (int i = 0; i < GDALGetDriverCount(); ++i) {
	GDALDriverH driver = GDALGetDriver(i);
	if (!readable_raster(driver)) continue;
	++readable;
	if (BU_STR_EQUAL(GDALGetDriverShortName(driver), "CAD") || BU_STR_EQUAL(GDALGetDriverShortName(driver), "Terragen") ||
	    BU_STR_EQUAL(GDALGetDriverShortName(driver), "NSIDCbin") || BU_STR_EQUAL(GDALGetDriverShortName(driver), "SAFE") ||
	    BU_STR_EQUAL(GDALGetDriverShortName(driver), "SENTINEL2")) { ++omitted; continue; }
	if (BU_STR_EQUAL(GDALGetDriverShortName(driver), "PNM")) {
	    expected.emplace("image/x-portable-graymap");
	    expected.emplace("image/x-portable-pixmap");
	    continue;
	}
	bool mapped = false;
	for (const auto &binding : audited)
	    if (BU_STR_EQUAL(GDALGetDriverShortName(driver), binding.driver)) {
		expected.emplace(binding.type);
		mapped = true;
	    }
	if (mapped) continue;
	const char *published = gcv_gdal::identity(GDALGetDriverShortName(driver));
	if (published) {expected.emplace(published); continue;}
	const char *mime = GDALGetMetadataItem(driver, GDAL_DMD_MIMETYPE, NULL);
	const char *generic[] = {"application/octet-stream", "application/xml", "text/xml", "application/json", "text/plain"};
	bool specific = mime && mime[0];
	for (const char *name : generic) if (mime && BU_STR_EQUIV(mime, name)) specific = false;
	if (!specific) { ++omitted; continue; }
	bu_vls canonical = BU_VLS_INIT_ZERO;
	GDAL_CHECK(bu_mime_canonical(gcv_mime_registry(&context), mime, &canonical));
	expected.emplace(bu_vls_cstr(&canonical));
	bu_vls_free(&canonical);
    }
    if (lerc_available()) expected.emplace("image/vnd.esri.lerc");
    gcv_handler_info *info = NULL;
    size_t count = 0;
    GDAL_CHECK(gcv_list_handlers(&context, NULL, &info, &count));
    std::set<std::string> advertised;
    for (size_t i = 0; i < count; ++i)
	if (BU_STR_EQUAL(info[i].name, "GDAL Reader")) {
	    GDAL_CHECK(info[i].operation == GCV_FILTER_READ);
	    if (BU_STR_EQUAL(info[i].mime_type, "image/x-portable-graymap") || BU_STR_EQUAL(info[i].mime_type, "image/x-portable-pixmap"))
		GDAL_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "encoding"), "binary") &&
		    info[i].capability_flags & BU_MIME_DEFER_INPUT_VALIDATION);
	    if (BU_STR_EQUAL(info[i].mime_type, "image/vnd.esri.lerc"))
		GDAL_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "lerc.blob-count"), "1") &&
		    bu_mime_traits_get(&info[i].traits, "lerc.first-blob.codec") &&
		    bu_mime_traits_get(&info[i].traits, "lerc.first-blob.version") &&
		    info[i].capability_flags & BU_MIME_DEFER_INPUT_VALIDATION);
	    if (BU_STR_EQUAL(info[i].mime_type, "application/x-tar"))
		GDAL_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "encoding"), "binary") &&
		    BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "container"), "tar") &&
		    BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "tar.first-member-type"), "application/vnd.nasa.mrf+xml") &&
		    info[i].capability_flags & BU_MIME_DEFER_INPUT_VALIDATION);
	    if (BU_STR_EQUAL(info[i].mime_type, "application/vnd.zarr")) {
		const char *version = bu_mime_traits_get(&info[i].traits, "version");
		const char *entrypoint = bu_mime_traits_get(&info[i].traits, "zarr.entrypoint");
		GDAL_CHECK(version && (BU_STR_EQUAL(version, "2") || BU_STR_EQUAL(version, "3")) &&
		    entrypoint && (BU_STR_EQUAL(entrypoint, "directory") || BU_STR_EQUAL(entrypoint, "metadata")) &&
		    !bu_mime_traits_get(&info[i].traits, "encoding") &&
		    info[i].capability_flags & BU_MIME_DEFER_INPUT_VALIDATION);
	    }
	    if (BU_STR_EQUAL(info[i].mime_type, "application/vnd.nasa.vicar"))
		GDAL_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "vicar.header-layout"), "lblsize") &&
		    BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "encoding"), "binary") &&
		    info[i].capability_flags & BU_MIME_DEFER_INPUT_VALIDATION);
	    if (BU_STR_EQUAL(info[i].mime_type, "application/cals-1840"))
		GDAL_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "cals.header-layout"), "rtype") &&
		    BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "cals.raster-type"), "1") &&
		    BU_STR_EQUAL(bu_mime_traits_get(&info[i].traits, "compression"), "ccitt-group4") &&
		    info[i].capability_flags & BU_MIME_DEFER_INPUT_VALIDATION);
	    advertised.emplace(info[i].mime_type);
	}
    gcv_handler_list_free(info, count);
    GDAL_CHECK(advertised == expected);
    GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, "image/x-portable-anymap", "GDAL Reader", NULL));
    GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, "application/json", "GDAL Reader", NULL));
    GDAL_CHECK(!gcv_select_filter(&context, GCV_FILTER_READ, "image/vnd.dwg", "GDAL Reader", NULL));
    bu_log("GDAL MIME inventory: %zu drivers advertise raster open, %zu identities, %zu are unbound\n",
	readable, expected.size(), omitted);
    gcv_context_destroy(&context);
    return 0;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 2 || (argc == 2 && !BU_STR_EQUAL(argv[1], "heightfields") && !BU_STR_EQUAL(argv[1], "sidecars") && !BU_STR_EQUAL(argv[1], "science") && !BU_STR_EQUAL(argv[1], "products"))) {
	bu_log("Usage: %s [heightfields|sidecars|science|products]\n", argv[0]);
	return 1;
    }
    GDALAllRegister();
    GDAL_CHECK(!inventory());
    char tiff[MAXPATHLEN], png[MAXPATHLEN];
    FILE *stream = bu_temp_file(tiff, sizeof(tiff));
    GDAL_CHECK(stream && !fclose(stream));
    stream = bu_temp_file(png, sizeof(png));
    GDAL_CHECK(stream && !fclose(stream));
    GDAL_CHECK(fixture(tiff, png));
    if (argc == 2) {
	if (BU_STR_EQUAL(argv[1], "products")) GDAL_CHECK(!product_profiles(tiff));
	else if (BU_STR_EQUAL(argv[1], "sidecars")) GDAL_CHECK(!sidecar_profiles());
	else if (BU_STR_EQUAL(argv[1], "science")) GDAL_CHECK(!science_profiles(tiff));
	else GDAL_CHECK(!heightfield_profiles(tiff));
    } else {
	GDAL_CHECK(!label_profiles(tiff));
	GDAL_CHECK(!mrf_tar_profiles(tiff));
	GDAL_CHECK(!lerc_profiles(tiff));
	GDAL_CHECK(!leveller_profiles(tiff));
	GDAL_CHECK(!stac_profiles(tiff));
	GDAL_CHECK(!kml_profiles(tiff));
	for (const auto &binding : gcv_gdal::identity_bindings)
	    if (readable_raster(GDALGetDriverByName(binding.driver)))
		GDAL_CHECK(import_profile(tiff, binding.mime_type, NULL, false));
	const bu_mime_trait marker = BU_MIME_TRAIT_STRING("fixture.png.header", "present");
	const bu_mime_traits observed = {&marker, 1};
	GDAL_CHECK(import_profile(png, "image/png", &observed, true, NULL, &inspection_plugin));
	GDAL_CHECK(import_profile(png, "image/png", &observed, false));
	const bu_mime_trait absent = BU_MIME_TRAIT_STRING("fixture.png.header", "absent");
	const bu_mime_traits contradicted = {&absent, 1};
	GDAL_CHECK(import_profile(png, "image/png", &contradicted, false, NULL, &inspection_plugin));
	GDAL_CHECK(!curated_profiles());
	GDAL_CHECK(!vrt_profile(tiff));
	GDAL_CHECK(!pnm_profiles(tiff));
	GDAL_CHECK(import_profile(tiff, "image/tiff", NULL, true));
	bu_mime_traits empty = BU_MIME_TRAITS_INIT_ZERO;
	GDAL_CHECK(import_profile(tiff, "image/tiff", &empty, true));
	GDAL_CHECK(import_profile(png, "image/png", NULL, true));
	bu_mime_trait supported[] = {BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "16")};
	bu_mime_traits profile = {supported, sizeof(supported) / sizeof(supported[0])};
	GDAL_CHECK(import_profile(png, "image/png", &profile, true));
	bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("version", "unestablished"),
	    BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("bit-depth", "8")};
	for (auto &trait : unsupported) {
	    bu_mime_traits requested = {&trait, 1};
	    GDAL_CHECK(import_profile(tiff, "image/tiff", &requested, false));
	    GDAL_CHECK(import_profile(png, "image/png", &requested, false));
	}
	GDAL_CHECK(import_profile(tiff, "image/png", NULL, false));
	GDAL_CHECK(import_profile(png, "image/tiff", NULL, false));
    }
    GDAL_CHECK(GDALDeleteDataset(GDALGetDriverByName("PNG"), png) == CE_None);
    GDAL_CHECK(GDALDeleteDataset(GDALGetDriverByName("GTiff"), tiff) == CE_None);
    return 0;
}
