/*                             G D A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 Tom Browder
 * Copyright (c) 2017-2026 United States Government as represented by the U.S. Army
 * Research Laboratory.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
/** @file gdal.cpp
 *
 * Geospatial Data Abstraction Library plugin for libgcv.
 *
 */


#include "common.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/* GDAL headers */
#include "gdal.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "ogr_spatialref.h"
#include "vrtdataset.h"

/* GDAL uses EQUAL for case-insensitive strings in its own headers. */
#undef EQUAL
#include "vmath.h"

#include "bu/app.h"
#include "bu/env.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/units.h"
#include "raytrace.h"
#include "gcv/api.h"
#include "../mime_catalog.h"
#include "../mime_probe.h"
#include "../../../libbu/mime_pnm.h"
#include "../../../libbu/mime_bmp.h"
#include "../../../libbu/mime_cals.h"
#include "../../../libbu/mime_vicar.h"
#include "../../../libbu/mime_zarr.h"
#include "../../../libbu/mime_stac.h"
#include "../../../libbu/mime_xml.h"
#include "../../../libbu/mime_zip.h"
#include "../../../libbu/mime_tar.h"
#include "../../../libbu/mime_lerc.h"
#include "../../../libbu/mime_leveller.h"
#include "../../../libbu/mime_nitf.h"
#include "../../../libbu/mime_geopackage.h"
#include "../../../libbu/mime_grib.h"
#include "../../../libbu/mime_terrain.h"
#include "format_map.h"
#include "hf2_mime.h"
#include "raster_mime.h"
#include "photomod_mime.h"
#include "esric_mime.h"
#include "../../../libbu/mime_noaa_grid.h"
#include "../../../libbu/mime_nsidc.h"
#include "../../../libbu/mime_iris.h"
#include "../../../libbu/mime_sigdem.h"
#include "../../../libbu/mime_byn.h"
#include "../../../libbu/mime_isg.h"
#include "gcv/util.h"
#include "wdb.h"

// Function based on gdalinfo code
extern int gdal_ll(GDALDatasetH hDataset, double *lat_center, double *long_center);

struct gdal_read_options
{
    const char *proj;
    int center;
    int zone;
};

struct conversion_state {
    const struct gcv_opts *gcv_options;
    const struct gdal_read_options *ops;
    const char *input_file;
    struct rt_wdb *wdbp;

    /* GDAL state */
    GDALDatasetH hDataset;
};

static constexpr const char *gdal_kmz_type = "application/vnd.google-earth.kmz";

struct gdal_mime_profile {
    const char *driver;
    const char *type;
    std::vector<std::pair<std::string, std::string>> traits;
    std::vector<std::string> extensions;
    gcv_mime_catalog::set_constraints sets = {};
};

static const std::vector<gdal_mime_profile> &
gdal_audited_profiles()
{
    static const gcv_mime_catalog::set_constraints stac_versions = {
	{"stac.versions", {"0.9.0", "1.0.0-beta.2", "1.0.0", "1.1.0"}}
    };
    const auto isg_traits = [](const char *version, const char *units) -> std::vector<std::pair<std::string, std::string>> {
	return {{"isg.format-version", version}, {"text.code-unit-width", "8"}, {"isg.data-format", "grid"},
	    {"isg.data-ordering", "N-to-S, W-to-E"}, {"isg.coordinate-type", "geodetic"}, {"isg.coordinate-units", units}};
    };
    static const std::vector<gdal_mime_profile> profiles = [isg_traits]() {
	std::vector<gdal_mime_profile> entries = {
	    {"HF2", mime_hf2::type, {{"encoding", "binary"}, {"version", "0"}, {"compression", "none"}}, {"hf2"}},
	    {"HF2", mime_hf2::type, {{"encoding", "binary"}, {"version", "0"}, {"compression", "gzip"}}, {"hfz"}},
	    {"SIGDEM", mime_sigdem::type, {{"encoding", "binary"}, {"version", "1"}, {"compression", "none"}}, {"sigdem"}},
	    {"BYN", mime_byn::type, {{"encoding", "binary"}, {"byn.unit-scale", "true"}}, {"byn", "err"}},
	    {"ISG", mime_isg::type, isg_traits("1.0", "deg"), {"isg"}},
	    {"ISG", mime_isg::type, isg_traits("1.01", "deg"), {"isg"}},
	    {"ISG", mime_isg::type, isg_traits("2.0", "deg"), {"isg"}},
	    {"ISG", mime_isg::type, isg_traits("2.0", "dms"), {"isg"}},
	    {"RST", "application/x-rst", {{"raster.value-dimension", "length"}}, {}},
	    {"PCIDSK", mime_terrain::pcidsk_type, {{"encoding", "binary"}}, {}},
	    {"DTED", mime_terrain::dted_type, {{"encoding", "binary"}, {"byte-order", "big-endian"}, {"bit-depth", "16"}, {"sample-format", "signed-integer"}}, {}},
	    {"GXF", mime_terrain::gxf_type, {{"gxf.serialization", "text"}, {"text.code-unit-width", "8"}, {"gxf.text-bom", "false"}, {"gxf.inline-header-values", "false"}, {"gxf.value-encoding", "decimal"}}, {}},
	    {"GXF", mime_terrain::gxf_type, {{"gxf.serialization", "text"}, {"text.code-unit-width", "8"}, {"gxf.text-bom", "false"}, {"gxf.inline-header-values", "false"}, {"gxf.value-encoding", "base90"}}, {}},
	    {"ZMap", mime_terrain::zmap_type, {{"zmap.serialization", "text"}, {"text.code-unit-width", "8"}, {"zmap.text-bom", "false"}}, {}},
	    {"GRASSASCIIGrid", mime_terrain::grass_type, {{"grass-grid.serialization", "text"}, {"text.code-unit-width", "8"}, {"grass-grid.text-bom", "false"}}, {"asc"}},
	    {"XYZ", mime_terrain::xyz_type, {{"xyz.serialization", "text"}, {"text.code-unit-width", "8"}, {"xyz.text-bom", "false"}, {"xyz.topology", "regular-grid"}}, {}},
	    {"GSAG", mime_terrain::surfer_type, {{"surfer.layout", "classic"}, {"surfer.serialization", "text"}, {"text.code-unit-width", "8"}, {"surfer.text-bom", "false"}}, {}},
	    {"GSBG", mime_terrain::surfer_type, {{"surfer.layout", "classic"}, {"surfer.serialization", "binary"}, {"encoding", "binary"}, {"byte-order", "little-endian"}, {"bit-depth", "32"}, {"sample-format", "ieee754"}}, {}},
	    {"GS7BG", mime_terrain::surfer_type, {{"surfer.layout", "tagged"}, {"surfer.serialization", "binary"}, {"encoding", "binary"}, {"byte-order", "little-endian"}, {"bit-depth", "64"}, {"sample-format", "ieee754"}, {"surfer.header-version", "1"}}, {}},
	    {"GS7BG", mime_terrain::surfer_type, {{"surfer.layout", "tagged"}, {"surfer.serialization", "binary"}, {"encoding", "binary"}, {"byte-order", "little-endian"}, {"bit-depth", "64"}, {"sample-format", "ieee754"}, {"surfer.header-version", "2"}}, {}},
	    {"HFA", mime_terrain::hfa_type, {{"encoding", "binary"}}, {}},
	    {"BT", mime_terrain::bt_type, {{"encoding", "binary"}, {"byte-order", "little-endian"}, {"bit-depth", "16"}, {"sample-format", "signed-integer"}}, {}},
	    {"BT", mime_terrain::bt_type, {{"encoding", "binary"}, {"byte-order", "little-endian"}, {"bit-depth", "32"}, {"sample-format", "signed-integer"}}, {}},
	    {"BT", mime_terrain::bt_type, {{"encoding", "binary"}, {"byte-order", "little-endian"}, {"bit-depth", "32"}, {"sample-format", "ieee754"}}, {}},
	    {"AAIGrid", mime_terrain::ascii_grid_type, {{"esri-grid.serialization", "text"}, {"text.code-unit-width", "8"}, {"esri-grid.text-bom", "false"}}, {}},
	    {"BMP", "image/bmp", {{"encoding", "binary"}}, {}},
	    {"Leveller", mime_leveller::type, {{"encoding", "binary"}, {"version", "7"}, {"leveller.reference-kind", "geodetic"}}, {"ter"}},
	    {"Leveller", mime_leveller::type, {{"encoding", "binary"}, {"version", "8"}, {"leveller.reference-kind", "geodetic"}}, {"ter"}},
	    {"Leveller", mime_leveller::type, {{"encoding", "binary"}, {"version", "9"}, {"leveller.reference-kind", "geodetic"}}, {"ter"}},
	    {"Leveller", mime_leveller::type, {{"encoding", "binary"}, {"version", "10"}, {"leveller.reference-kind", "geodetic"}}, {"ter"}},
	    {"Leveller", mime_leveller::type, {{"encoding", "binary"}, {"version", "11"}, {"leveller.reference-kind", "geodetic"}}, {"ter"}},
	    {"Leveller", mime_leveller::type, {{"encoding", "binary"}, {"version", "12"}, {"leveller.reference-kind", "geodetic"}}, {"ter"}},
	    {"PDS4", mime_xml::pds4_type, {{"container", "none"}, {"text.code-unit-width", "8"}}, {"xml"}},
	    {"MRF", mime_lerc::type, {{"encoding", "binary"}, {"lerc.blob-count", "1"}, {"lerc.first-blob.codec", "lerc1"}, {"lerc.first-blob.version", "11"}}, {"lrc", "lerc"}},
	    {"MRF", mime_lerc::type, {{"encoding", "binary"}, {"lerc.blob-count", "1"}, {"lerc.first-blob.codec", "lerc2"}, {"lerc.first-blob.version", "1"}}, {"lrc", "lerc"}},
	    {"MRF", mime_lerc::type, {{"encoding", "binary"}, {"lerc.blob-count", "1"}, {"lerc.first-blob.codec", "lerc2"}, {"lerc.first-blob.version", "2"}}, {"lrc", "lerc"}},
	    {"MRF", mime_lerc::type, {{"encoding", "binary"}, {"lerc.blob-count", "1"}, {"lerc.first-blob.codec", "lerc2"}, {"lerc.first-blob.version", "3"}}, {"lrc", "lerc"}},
	    {"MRF", mime_lerc::type, {{"encoding", "binary"}, {"lerc.blob-count", "1"}, {"lerc.first-blob.codec", "lerc2"}, {"lerc.first-blob.version", "4"}}, {"lrc", "lerc"}},
	    {"MRF", mime_tar::type, {{"container", "tar"}, {"encoding", "binary"}, {"tar.first-member-type", mime_xml::mrf_type}}, {"tar"}},
	    {"MRF", mime_xml::mrf_type, {{"container", "none"}, {"text.code-unit-width", "8"}}, {"mrf"}},
	    {"STACIT", mime_stac::type, {{"stac.object-kind", "item"}, {"stac.items-have-assets", "true"}}, {"json", "geojson"}, stac_versions},
	    {"STACIT", mime_stac::type, {{"stac.object-kind", "item-collection"}, {"stac.items-have-assets", "true"}}, {"json", "geojson"}, stac_versions},
	    {"STACTA", mime_stac::type, {{"stac.object-kind", "item"}, {"stac.asset-templates", "present"}}, {"json"}, stac_versions},
	    {"KMLSUPEROVERLAY", mime_xml::kml_type, {{"container", "none"}, {"text.code-unit-width", "8"}}, {"kml"}},
	    {"KMLSUPEROVERLAY", gdal_kmz_type, {{"container", "zip"}, {"encoding", "binary"}}, {"kmz"}},
	    {"ESRIC", mime_esric::xml_type, {{"container", "none"}, {"text.code-unit-width", "8"}, {"esri-cache.storage-format", mime_esric::compact_v2}}, {"xml"}},
	    {"ESRIC", mime_esric::json_type, {{"container", "none"}, {"text.code-unit-width", "8"}, {"esri-cache.storage-format", mime_esric::compact_v2}}, {"json"}},
	    {"ESRIC", mime_esric::zip_type, {{"container", "zip"}, {"encoding", "binary"}, {"esri-cache.storage-format", mime_esric::compact_v2}}, {"tpkx"}},
	    {"Zarr", mime_zarr::type, {{"version", "2"}, {"zarr.entrypoint", "directory"}}, {"zarr"}},
    #if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 8, 0)
	    /* Earlier SDKs implement an incompatible draft of the V3 format. */
	    {"Zarr", mime_zarr::type, {{"version", "3"}, {"zarr.entrypoint", "directory"}}, {"zarr"}},
    #endif
    #if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3, 12, 0)
	    /* Direct metadata entrypoints were added after directory support. */
	    {"Zarr", mime_zarr::type, {{"version", "2"}, {"zarr.entrypoint", "metadata"}}, {"zarr"}},
	    {"Zarr", mime_zarr::type, {{"version", "3"}, {"zarr.entrypoint", "metadata"}}, {"zarr"}},
    #endif
	    {"VICAR", mime_vicar::type, {{"vicar.header-layout", "lblsize"}, {"encoding", "binary"}}, {"img", "vicar"}},
	    {"CALS", mime_cals::type, {{"cals.header-layout", "rtype"}, {"cals.raster-type", "1"}, {"encoding", "binary"}, {"bit-depth", "1"}, {"compression", "ccitt-group4"}}, {}},
	    {"NITF", "application/vnd.nitf", {{"encoding", "binary"}, {"nitf.standard", "nitf"}, {"version", "1.1"}}, {}},
	    {"NITF", "application/vnd.nitf", {{"encoding", "binary"}, {"nitf.standard", "nitf"}, {"version", "2.0"}}, {}},
	    {"NITF", "application/vnd.nitf", {{"encoding", "binary"}, {"nitf.standard", "nitf"}, {"version", "2.1"}}, {}},
	    {"NITF", "application/vnd.nitf", {{"encoding", "binary"}, {"nitf.standard", "nsif"}, {"version", "1.0"}}, {}},
	    {"GPKG", "application/geopackage+sqlite3", {{"encoding", "binary"}, {"container", "sqlite3"}}, {"gpkg"}},
	    {"GRIB", "application/grib", {{"encoding", "binary"}, {"grib.first-edition", "1"}}, {}},
	    {"GRIB", "application/grib", {{"encoding", "binary"}, {"grib.first-edition", "2"}}, {}},
	    {"PNM", "image/x-portable-graymap", {{"encoding", "binary"}}, {"pgm", "pnm"}},
	    {"PNM", "image/x-portable-pixmap", {{"encoding", "binary"}}, {"ppm", "pnm"}}
	};
	for (const char *datatype : {"LOG1S", "INT1S", "INT2S", "INT4S", "INT1U", "INT2U", "INT4U", "FLT4S", "FLT8S"})
	    entries.push_back({"RRASTER", mime_raster_label::rraster_label,
		{{"raster.entrypoint", "label"}, {"text.code-unit-width", "8"}, {"rraster.datatype", datatype}}, {"grd"}});
	for (const char *datatype : {"BYTE", "CHAR", "SHORT", "INT", "LONG", "FLOAT", "DOUBLE"})
	    entries.push_back({"ISCE", mime_raster_label::isce_data,
		{{"raster.entrypoint", "payload"}, {"encoding", "binary"}, {"raster.label-code-unit-width", "8"}, {"isce.datatype", datatype}}, {}});
	for (const char *kind : {"dem", "flg", "amp", "cor", "hgt", "unw", "msk", "trans"})
	    entries.push_back({"ROI_PAC", mime_raster_label::roipac_data,
		{{"raster.entrypoint", "payload"}, {"encoding", "binary"}, {"raster.label-code-unit-width", "8"},
		 {"roi-pac.filename-kind", kind}, {"roi-pac.unit-transform", "true"}}, {}});
	for (const char *layout : {"BIL", "BIP", "BSQ"})
	    entries.push_back({"EHdr", mime_raster_label::ehdr_data,
		{{"raster.entrypoint", "payload"}, {"encoding", "binary"}, {"raster.label-code-unit-width", "8"}, {"ehdr.interleave", layout}}, {"bip", "bsq"}});
	entries.push_back({"EHdr", mime_raster_label::bil_data,
	    {{"raster.entrypoint", "payload"}, {"encoding", "binary"}, {"raster.label-code-unit-width", "8"}, {"ehdr.interleave", "BIL"}}, {"bil"}});
	for (const char *kind : {"0", "1", "2"})
	    entries.push_back({"NOAA_B", mime_noaa_grid::type,
		{{"encoding", "binary"}, {"noaa-grid.layout", "b-records"}, {"noaa-grid.kind", kind},
		 {"noaa-grid.steps-within-one-degree", "true"}}, {"b"}});
	for (unsigned int product : {1u, 3u, 5u, 7u, 8u, 10u, 16u})
	    for (unsigned int quantity : {1u, 2u, 3u, 8u, 9u, 32u, 33u, 35u, 37u}) {
		const char *view = product == 1 ? mime_iris::ppi_type : product == 3 ? mime_iris::cappi_type : NULL;
		for (const char *type : {mime_iris::type, view}) {
		    if (!type) continue;
		    std::vector<std::pair<std::string, std::string>> traits = {
			{"encoding", "binary"}, {"byte-order", "little-endian"}, {"iris.product-header-version", "8"},
			{"iris.configuration-version", "6"}, {"iris.product-code", std::to_string(product)},
			{"iris.quantity-code", std::to_string(quantity)}, {"iris.payload-layout", "plain-cartesian"}
		    };
		    entries.push_back({"IRIS", type, traits, BU_STR_EQUAL(type, mime_iris::ppi_type) ? std::vector<std::string>{"ppi"} :
			BU_STR_EQUAL(type, mime_iris::cappi_type) ? std::vector<std::string>{"cappi"} : std::vector<std::string>{}});
		}
	    }
	for (const auto &channel : {std::make_pair("U", "1"), std::make_pair("U", "2"), std::make_pair("U", "4"),
	    std::make_pair("F", "4"), std::make_pair("F", "8")})
	    entries.push_back({"PRF", mime_raster_label::photomod_label,
		{{"raster.entrypoint", "label"}, {"text.code-unit-width", "8"}, {"photomod.channel-type", channel.first},
		 {"photomod.channel-bytes", channel.second}, {"photomod.unit-offset", "true"}}, {"prf", "x-dem"}});
	return entries;
    }();
    return profiles;
}

static bool
gdal_audited_profile_matches(const char *type, const bu_mime_traits &observed)
{
    for (const auto &profile : gdal_audited_profiles()) {
	if (!BU_STR_EQUAL(type, profile.type)) continue;
	bu_mime_traits constraints = BU_MIME_TRAITS_INIT_ZERO;
	bool valid = true;
	for (const auto &property : profile.traits)
	    valid = valid && bu_mime_traits_add(&constraints, property.first.c_str(), property.second.c_str());
	for (const auto &set : profile.sets) {
	    std::vector<const char *> members;
	    for (const auto &member : set.second) members.push_back(member.c_str());
	    valid = valid && bu_mime_traits_add_set(&constraints, set.first.c_str(), members.data(), members.size());
	}
	const bu_mime_capability supported = BU_MIME_CAPABILITY(type, constraints.items, constraints.count, BU_MIME_READ);
	const bool matches = valid && bu_mime_capability_match(&supported, &observed, BU_MIME_INPUT);
	bu_mime_traits_free(&constraints);
	if (matches) return true;
    }
    return false;
}

static bool
gdal_requested_property(const bu_mime_traits *requested, const char *key)
{
    return bu_mime_traits_get(requested, key) || bu_mime_traits_get_set(requested, key, NULL, NULL);
}

/* Preserve namespace scope while reading the few owned label declarations.
 * Removing prefixes would equate extension metadata with the format fields. */
static bool
gdal_xml_values(const CPLXMLNode *nodes, const std::vector<std::string> &path, size_t depth,
    const std::string &uri, const std::map<std::string, std::string> &inherited,
    size_t &sections, std::vector<std::string> &values)
{
    for (const CPLXMLNode *node = nodes; node; node = node->psNext) {
	if (node->eType != CXT_Element) continue;
	auto namespaces = inherited;
	for (const CPLXMLNode *attribute = node->psChild; attribute; attribute = attribute->psNext) {
	    if (attribute->eType != CXT_Attribute || !attribute->psChild || attribute->psChild->eType != CXT_Text) continue;
	    const std::string name = attribute->pszValue;
	    if (name == "xmlns") namespaces[""] = attribute->psChild->pszValue;
	    else if (name.compare(0, sizeof("xmlns:") - 1, "xmlns:") == 0)
		namespaces[name.substr(sizeof("xmlns:") - 1)] = attribute->psChild->pszValue;
	}
	const std::string qualified = node->pszValue;
	const size_t colon = qualified.find(':');
	const std::string local = colon == std::string::npos ? qualified : qualified.substr(colon + 1);
	const std::string prefix = colon == std::string::npos ? "" : qualified.substr(0, colon);
	const auto binding = namespaces.find(prefix);
	if (local != path[depth] || (!prefix.empty() && binding == namespaces.end()) ||
	    (binding == namespaces.end() ? !uri.empty() : binding->second != uri)) continue;
	if (depth + 2 == path.size()) ++sections;
	if (depth + 1 < path.size()) {
	    if (!gdal_xml_values(node->psChild, path, depth + 1, uri, namespaces, sections, values)) return false;
	} else {
	    std::string text;
	    for (const CPLXMLNode *value = node->psChild; value; value = value->psNext) {
		if (value->eType == CXT_Element) return false;
		if (value->eType == CXT_Text) text += value->pszValue;
	    }
	    text = mime_detail::trim(text);
	    if (text.empty()) return false;
	    values.push_back(text);
	}
    }
    return true;
}

static bool
gdal_label_value(const char *filename, const mime_xml::profile &profile, const char *section,
    const char *field, std::string &value, const char *implicit = NULL)
{
    CPLXMLNode *document = CPLParseXMLFile(filename);
    if (!document) return false;
    const std::vector<std::string> path = {profile.local, section, field};
    std::vector<std::string> values;
    size_t sections = 0;
    const bool parsed = gdal_xml_values(document, path, 0, profile.uri, {}, sections, values);
    /* Apply a format default only after establishing one owned section
     * with no explicit declaration. Duplicate sections remain ambiguous. */
    if (parsed && sections == 1 && values.empty() && implicit) values.emplace_back(implicit);
    const bool valid = parsed && sections == 1 && values.size() == 1;
    if (valid) value = values.front();
    CPLDestroyXMLNode(document);
    return valid;
}

static bool
gdal_stac_input_matches(struct gcv_context *context, const char *path, const bu_mime_traits *requested)
{
    std::ifstream stream(path, std::ios::binary);
    const auto profile = mime_stac::inspect(stream);
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool valid = stream.is_open() && !stream.bad() && profile.identified && profile.consistent &&
	!profile.stac_kind.empty() && profile.append_traits(observed);
    if (valid && gdal_requested_property(requested, "text.ascii-only")) {
	stream.clear();
	stream.seekg(0);
	const bool rewound = static_cast<bool>(stream);
	mime_unicode::text_source source(stream);
	valid = rewound && !source.finish_encoding().empty() && !stream.bad() &&
	    bu_mime_traits_add(&observed, "text.ascii-only", source.ascii ? "true" : "false");
    }
    valid = valid && gdal_audited_profile_matches(mime_stac::type, observed) &&
	gcv_probe_file_traits_match(context, path, mime_stac::type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static bool
gdal_raster_sidecar_matches(struct gcv_context *context, const char *source, const char *type,
    GDALDatasetH dataset, const bu_mime_traits *requested)
{
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool inspected = false;
    if (BU_STR_EQUAL(type, mime_raster_label::isce_data)) {
	const auto profile = gcv_gdal_raster::isce(dataset, source);
	const std::string scheme = mime_detail::upper(profile.value("SCHEME"));
	const std::string order = mime_detail::upper(profile.value("BYTE_ORDER"));
	inspected = profile.known && mime_raster_label::layout(scheme) &&
	    (order.empty() || order == "L" || order == "B") && profile.append_traits(observed);
    } else {
	const auto profile = gcv_gdal_raster::label(dataset, source);
	if (profile.header_known) {
	    if (BU_STR_EQUAL(type, mime_raster_label::rraster_label) && profile.kind == mime_raster_label::family::rraster) {
		const std::string bands = mime_raster_label::integer(profile.value("DATA.NBANDS", "1"));
		const std::string order = mime_detail::upper(profile.value("DATA.BYTEORDER"));
		const bool charset = (!gdal_requested_property(requested, "encoding") &&
		    !gdal_requested_property(requested, "text.ascii-only")) || profile.repertoire_known;
		inspected = charset && !bands.empty() && bands != "0" && (bands == "1" || mime_raster_label::layout(profile.interleave())) &&
		    (order.empty() || order == "BIG" || order == "LITTLE") && profile.append_traits(observed);
	    } else if ((BU_STR_EQUAL(type, mime_raster_label::ehdr_data) || BU_STR_EQUAL(type, mime_raster_label::bil_data)) &&
		profile.kind == mime_raster_label::family::ehdr) {
		const std::string bits = mime_raster_label::integer(profile.value("NBITS"));
		const std::string pixel = mime_detail::upper(profile.value("PIXELTYPE"));
		const std::string order = mime_detail::upper(profile.value("BYTEORDER"));
		const std::set<std::string> widths = {"1", "2", "3", "4", "5", "6", "7", "8", "16", "32"};
		const bool width_ok = profile.value("NBITS").empty() || widths.count(bits);
		const bool pixel_ok = pixel.empty() || (pixel == "FLOAT" && (bits.empty() || bits == "32")) ||
		    (pixel == "SIGNEDBYTE" && (bits.empty() || bits == "8")) ||
		    (pixel == "SIGNEDINT" && (bits.empty() || bits == "8" || bits == "16" || bits == "32")) || pixel == "UNSIGNEDINT";
		inspected = mime_raster_label::layout(profile.interleave()) && width_ok && pixel_ok &&
		    (order.empty() || order == "I" || order == "M" || order == "LSBFIRST" || order == "MSBFIRST") &&
		    (!BU_STR_EQUAL(type, mime_raster_label::bil_data) || profile.interleave() == "BIL") && profile.append_traits(observed, true);
	    } else if (BU_STR_EQUAL(type, mime_raster_label::roipac_data) && profile.kind == mime_raster_label::family::roipac) {
		bu_vls extension = BU_VLS_INIT_ZERO;
		inspected = bu_path_component(&extension, source, BU_PATH_EXT) && profile.append_traits(observed, true) &&
		    bu_mime_traits_add(&observed, "roi-pac.filename-kind", bu_vls_cstr(&extension));
		bu_vls_free(&extension);
	    }
	}
    }
    const bool valid = inspected && gdal_audited_profile_matches(type, observed) &&
	gcv_probe_file_traits_match(context, source, type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static mime_iris::profile
gdal_iris_profile(const char *filename)
{
    std::ifstream input(filename, std::ios::binary);
    if (!input.is_open()) return {};
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    if (length < 0) return {};
    input.seekg(0);
    unsigned char prefix[mime_iris::header_size];
    input.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
    if (input.bad()) return {};
    const bu_mime_probe_input source = {prefix, static_cast<size_t>(input.gcount()), input.eof(), static_cast<uint64_t>(length), 1};
    return mime_iris::inspect(source);
}

static bool
gdal_scientific_input_matches(struct gcv_context *context, const char *filename, const char *type, const bu_mime_traits *requested)
{
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool inspected = false;
    if (BU_STR_EQUAL(type, mime_raster_label::photomod_label)) {
	const auto profile = gcv_photomod::inspect(filename);
	const bool charset = (!gdal_requested_property(requested, "encoding") &&
	    !gdal_requested_property(requested, "text.ascii-only")) || profile.repertoire_known;
	inspected = charset && profile.append_traits(observed);
    } else {
	const auto profile = gdal_iris_profile(filename);
	const bool view = BU_STR_EQUAL(type, mime_iris::type) ||
	    (profile.product_view() && BU_STR_EQUAL(type, profile.product_view()));
	inspected = view && profile.append_traits(observed);
    }
    const bool valid = inspected && gdal_audited_profile_matches(type, observed) &&
	gcv_probe_file_traits_match(context, filename, type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static bool
gdal_heightfield_input_matches(struct gcv_context *context, const char *path, const char *type, const bu_mime_traits *requested)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return false;
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool inspected = false;
    if (BU_STR_EQUAL(type, mime_hf2::type)) {
	const auto profile = gcv_hf2::inspect(stream, true);
	inspected = !profile.error && profile.header.revision_known && profile.append_traits(observed);
    } else if (BU_STR_EQUAL(type, mime_isg::type)) {
	const bool repertoire = bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only");
	const auto profile = mime_isg::inspect(stream, true, repertoire);
	inspected = profile.header_known && (!repertoire || profile.repertoire_known) && profile.append_traits(observed);
    } else {
	constexpr size_t prefix_size = mime_sigdem::header_size;
	static_assert(prefix_size >= mime_byn::header_size && prefix_size >= mime_noaa_grid::header_size,
	    "heightfield prefix must include the BYN and NOAA headers");
	unsigned char prefix[prefix_size];
	stream.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
	const size_t size = static_cast<size_t>(stream.gcount());
	if (BU_STR_EQUAL(type, mime_sigdem::type)) inspected = mime_sigdem::inspect(prefix, size, stream.eof()).append_traits(observed);
	else if (BU_STR_EQUAL(type, mime_byn::type)) inspected = mime_byn::inspect(prefix, size).append_traits(observed);
	else if (BU_STR_EQUAL(type, mime_noaa_grid::type)) inspected = mime_noaa_grid::inspect(prefix, size).append_traits(observed);
    }
    const bool valid = inspected && !stream.bad() && gdal_audited_profile_matches(type, observed) &&
	gcv_probe_file_traits_match(context, path, type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static bu_mime_probe_status
gdal_probe(const bu_mime_probe_input *input, const char *type)
{
    if (!BU_STR_EQUAL(type, mime_hf2::type)) return BU_MIME_PROBE_NO_MATCH;
    const auto profile = gcv_hf2::inspect(*input);
    if (profile.error) return BU_MIME_PROBE_ERROR;
    if (profile.header.identified) return BU_MIME_PROBE_SIGNATURE;
    return profile.header.incomplete ? BU_MIME_PROBE_INCOMPLETE : BU_MIME_PROBE_NO_MATCH;
}

static int
gdal_inspect(const bu_mime_probe_input *input, const char *type, bu_mime_traits *traits)
{
    if (!BU_STR_EQUAL(type, mime_hf2::type)) return 1;
    const auto profile = gcv_hf2::inspect(*input);
    return !profile.error && (!profile.header.identified || profile.append_traits(*traits));
}

static bool
gdal_leveller_input_matches(struct gcv_context *context, const char *path, GDALDatasetH dataset, const bu_mime_traits *requested)
{
    std::ifstream stream(path, std::ios::binary);
    unsigned char prefix[mime_leveller::header_size];
    stream.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
    const auto profile = mime_leveller::inspect(prefix, static_cast<size_t>(stream.gcount()), stream.eof());
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    OGRSpatialReference reference(GDALGetProjectionRef(dataset));
    const char *kind = reference.IsGeographic() || reference.IsProjected() ? "geodetic" : reference.IsLocal() ? "local" : "unspecified";
    const bool valid = stream.is_open() && !stream.bad() && profile.identified && profile.revision_known &&
	profile.append_traits(observed) && bu_mime_traits_add(&observed, "leveller.reference-kind", kind) &&
	gdal_audited_profile_matches(mime_leveller::type, observed) &&
	gcv_probe_file_traits_match(context, path, mime_leveller::type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static bool
gdal_lerc_input_matches(struct gcv_context *context, const char *path, const bu_mime_traits *requested)
{
    std::ifstream stream(path, std::ios::binary);
    const auto profile = mime_lerc::inspect(stream);
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = stream.is_open() && !stream.bad() && profile.header_known && profile.single_blob &&
	profile.append_traits(observed) && gdal_audited_profile_matches(mime_lerc::type, observed) &&
	gcv_probe_file_traits_match(context, path, mime_lerc::type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static mime_tar::profile
gdal_tar_profile(const char *path)
{
    std::ifstream source(path, std::ios::binary);
    std::vector<unsigned char> prefix(BU_MIME_DEFAULT_SCAN_LIMIT);
    source.read(reinterpret_cast<char *>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
    if (source.bad()) return {};
    return mime_tar::inspect(prefix.data(), static_cast<size_t>(source.gcount()), source.eof());
}

static bool
gdal_tar_input_matches(struct gcv_context *context, const char *path, const bu_mime_traits *requested)
{
    const auto profile = gdal_tar_profile(path);
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool valid = profile.identified && profile.member_type == mime_xml::mrf_type && profile.append_traits(observed);
    if (valid && gdal_requested_property(requested, "mrf.tile-compression")) {
	/* The SDK opens this same root member. Its XML facts belong to the
	 * member; only tile compression is published on the package. */
	const std::string member = std::string("/vsitar/") + path + "/" + profile.member_name;
	mime_xml::profile label;
	label.local = "MRF_META";
	std::string compression;
	valid = gdal_label_value(member.c_str(), label, "Raster", "Compression", compression, "PNG");
	compression = mime_detail::upper(compression);
	valid = valid && bu_mime_traits_add(&observed, "mrf.tile-compression", compression.c_str());
    }
    valid = valid && gdal_audited_profile_matches(mime_tar::type, observed) &&
	gcv_probe_file_traits_match(context, path, mime_tar::type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

/* Several SDK drivers open both an XML entrypoint and other containers.
 * Validate source facts independently of the chosen driver and its payload. */
static bool
gdal_document_input_matches(struct gcv_context *context, const char *path, const char *type, const bu_mime_traits *requested)
{
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool valid;
    if (BU_STR_EQUAL(type, gdal_kmz_type)) {
	valid = mime_zip::end_record(path) && bu_mime_traits_add(&observed, "container", "zip") &&
	    bu_mime_traits_add(&observed, "encoding", "binary");
    } else {
	std::ifstream stream(path, std::ios::binary);
	auto profile = mime_xml::inspect(stream);
	valid = stream.is_open() && !stream.bad() && profile.type() && BU_STR_EQUAL(profile.type(), type);
	if (valid && BU_STR_EQUAL(type, mime_xml::kml_type) && !profile.version.empty()) {
	    /* KML 2.3 declares an instance version while sharing the 2.2
	     * namespace. Preserve future declarations without advertising
	     * this overlay reader for a future schema revision. */
	    constexpr const char *first_revision = "2.1", *next_revision = "2.4";
	    int first = 0, next = 0;
	    valid = bu_mime_version_compare(profile.version.c_str(), first_revision, &first) &&
		bu_mime_version_compare(profile.version.c_str(), next_revision, &next) && first >= 0 && next < 0;
	}
	const bool charset = requested && (bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only"));
	if (valid) {
	    stream.clear();
	    stream.seekg(0);
	    valid = static_cast<bool>(stream);
	    if (charset) {
		const auto text = mime_xml::inspect_text(stream, profile);
		valid = valid && !text.encoding.empty() && !stream.bad() && text.append_traits(profile, observed);
	    } else valid = valid && profile.append_traits(observed);
	    valid = valid && bu_mime_traits_add(&observed, "container", "none");
	    if (valid && BU_STR_EQUAL(type, mime_xml::pds4_type) && gdal_requested_property(requested, "pds4.information-model-version")) {
		std::string model;
		valid = gdal_label_value(path, profile, "Identification_Area", "information_model_version", model) &&
		    bu_mime_traits_add(&observed, "pds4.information-model-version", model.c_str());
	    }
	    if (valid && BU_STR_EQUAL(type, mime_xml::mrf_type) && gdal_requested_property(requested, "mrf.tile-compression")) {
		std::string compression;
		valid = gdal_label_value(path, profile, "Raster", "Compression", compression, "PNG");
		compression = mime_detail::upper(compression);
		valid = valid && bu_mime_traits_add(&observed, "mrf.tile-compression", compression.c_str());
	    }
	}
    }
    valid = valid && gdal_audited_profile_matches(type, observed) &&
	gcv_probe_file_traits_match(context, path, type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static bool
gdal_zarr_metadata(const std::string &path, const char *name, mime_zarr::profile &profile)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    profile = mime_zarr::inspect(stream);
    if (stream.bad() || !profile.identified || !profile.consistent) return false;
    if (BU_STR_EQUAL(name, "zarr.json"))
	return profile.version == "3" && (profile.node_type == "array" || profile.node_type == "group");
    if (BU_STR_EQUAL(name, ".zmetadata")) return profile.consolidated_format == "1" && profile.version == "2";
    if (profile.version != "2" || !profile.consolidated_format.empty()) return false;
    profile.node_type = BU_STR_EQUAL(name, ".zarray") ? "array" : "group";
    return true;
}

static bool
gdal_zarr_input_matches(struct gcv_context *context, const char *path, const bu_mime_traits *requested)
{
    const bool directory = bu_file_directory(path);
    bu_vls parent = BU_VLS_INIT_ZERO, basename = BU_VLS_INIT_ZERO;
    bool components = true;
    if (directory) bu_vls_strcpy(&parent, path);
    else components = bu_path_component(&parent, path, BU_PATH_DIRNAME) &&
	bu_path_component(&basename, path, BU_PATH_BASENAME);
    std::string root = bu_vls_cstr(&parent), name = bu_vls_cstr(&basename);
    bu_vls_free(&parent);
    bu_vls_free(&basename);
    if (!components) return false;
    const char *keys[] = {"zarr.json", ".zarray", ".zgroup", ".zmetadata"};
    bool known_name = directory;
    for (const char *key : keys) if (name == key) known_name = true;
    if (!known_name) return false;
    mime_zarr::profile source;
    for (const char *key : keys) {
	const std::string filename = root + "/" + key;
	if (!bu_file_exists(filename.c_str(), NULL)) continue;
	mime_zarr::profile metadata;
	if (!gdal_zarr_metadata(filename, key, metadata)) return false;
	if (source.identified && (source.version != metadata.version ||
	    (!source.node_type.empty() && !metadata.node_type.empty() && source.node_type != metadata.node_type))) return false;
	if (!source.identified) source = metadata;
	else {
	    if (source.node_type.empty()) source.node_type = metadata.node_type;
	    if (!metadata.consolidated_format.empty()) source.consolidated_format = metadata.consolidated_format;
	}
    }
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = source.append_traits(observed, directory ? "directory" : "metadata") &&
	gdal_audited_profile_matches(mime_zarr::type, observed) &&
	(directory ? gcv_probe_traits_match(mime_zarr::type, &observed, requested) :
	    gcv_probe_file_traits_match(context, path, mime_zarr::type, &observed, requested));
    bu_mime_traits_free(&observed);
    return valid;
}

static bool
gdal_audited_input_matches(struct gcv_context *context, const char *path, const char *type, const bu_mime_traits *requested)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bool inspected = false;
    const bool charset = requested && (bu_mime_traits_get(requested, "encoding") || bu_mime_traits_get(requested, "text.ascii-only"));
    if (BU_STR_EQUAL(type, mime_terrain::dted_type)) {
	inspected = mime_terrain::dted(stream, true).append_traits(observed);
    } else if (BU_STR_EQUAL(type, mime_terrain::gxf_type)) {
	inspected = mime_terrain::gxf(stream, true, charset).append_traits(observed);
    } else if (BU_STR_EQUAL(type, mime_terrain::zmap_type)) {
	inspected = mime_terrain::zmap(stream, true, charset).append_traits(observed);
    } else if (BU_STR_EQUAL(type, mime_terrain::ascii_grid_type) || BU_STR_EQUAL(type, mime_terrain::grass_type) || BU_STR_EQUAL(type, mime_terrain::xyz_type)) {
	mime_unicode::text_source source(stream);
	mime_terrain::profile profile;
	profile.type = type;
	profile.regular_grid = BU_STR_EQUAL(type, mime_terrain::xyz_type);
	const char *family = BU_STR_EQUAL(type, mime_terrain::grass_type) ? "grass-grid" : profile.regular_grid ? "xyz" : "esri-grid";
	profile.observe_text(source, charset, family);
	inspected = profile.append_traits(observed);
    } else if (BU_STR_EQUAL(type, mime_terrain::hfa_type) || BU_STR_EQUAL(type, mime_terrain::bt_type) || BU_STR_EQUAL(type, mime_terrain::surfer_type)) {
	unsigned char prefix[mime_terrain::bt_header_size];
	stream.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
	const size_t size = static_cast<size_t>(stream.gcount());
	const bu_mime_probe_input input = {prefix, size, stream.eof(), 0, 0};
	auto profile = mime_terrain::binary(input);
	if (!profile.type && BU_STR_EQUAL(type, mime_terrain::surfer_type)) {
	    stream.clear();
	    stream.seekg(0);
	    profile = mime_terrain::surfer_text(stream, true, charset);
	}
	inspected = profile.type && BU_STR_EQUAL(profile.type, type) && profile.append_traits(observed);
    } else if (BU_STR_EQUAL(type, mime_vicar::type)) {
	/* Opening through VICAR can also succeed for a PDS wrapper. The MIME
	 * identity describes the source representation, not the chosen driver. */
	inspected = mime_vicar::inspect(stream, true).append_traits(observed) &&
	    bu_mime_traits_add(&observed, "encoding", "binary");
    } else if (BU_STR_EQUAL(type, mime_cals::type)) {
	unsigned char prefix[mime_cals::header_size];
	stream.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
	const auto profile = mime_cals::inspect(prefix, static_cast<size_t>(stream.gcount()));
	inspected = profile.type_one() && profile.append_traits(observed);
    } else if (BU_STR_EQUAL(type, "application/grib")) {
	const auto profile = mime_grib::inspect(stream);
	inspected = profile.header_known && profile.append_traits(observed);
    } else {
	constexpr size_t prefix_size = mime_bmp::largest_header_size;
	static_assert(prefix_size >= mime_nitf::header_size && prefix_size >= mime_geopackage::header_size,
	    "fixed raster prefix must include the complete identifying fields");
	unsigned char prefix[prefix_size];
	stream.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
	const size_t size = static_cast<size_t>(stream.gcount());
	if (BU_STR_EQUAL(type, "image/bmp")) inspected = mime_bmp::inspect(prefix, size).append_traits(observed);
	else if (BU_STR_EQUAL(type, "application/vnd.nitf")) inspected = mime_nitf::inspect(prefix, size).append_traits(observed);
	else if (BU_STR_EQUAL(type, "application/geopackage+sqlite3")) inspected = mime_geopackage::inspect(prefix, size).append_traits(observed);
    }
    if ((bu_mime_traits_get(requested, "encoding") && !bu_mime_traits_get(&observed, "encoding")) ||
	(bu_mime_traits_get(requested, "text.ascii-only") && !bu_mime_traits_get(&observed, "text.ascii-only")))
	inspected = false;
    const bool valid = inspected && !stream.bad() && gdal_audited_profile_matches(type, observed) &&
	gcv_probe_file_traits_match(context, path, type, &observed, requested);
    bu_mime_traits_free(&observed);
    return valid;
}

static std::string
gdal_format_identity(GDALDriverH driver)
{
    if (!driver) return {};
    for (const auto &profile : gdal_audited_profiles())
	if (BU_STR_EQUAL(GDALGetDriverShortName(driver), profile.driver)) return profile.type;
    const char *published = gcv_gdal::identity(GDALGetDriverShortName(driver));
    if (published) return published;
    /* Driver identity cannot come from extensions: Terragen and Leveller
     * both use .ter, but have unrelated headers and sample representations. */
    const char *mime = GDALGetMetadataItem(driver, GDAL_DMD_MIMETYPE, NULL);
    if (!mime || !mime[0] || BU_STR_EQUIV(mime, "application/octet-stream") ||
	BU_STR_EQUIV(mime, "application/xml") || BU_STR_EQUIV(mime, "text/xml") ||
	BU_STR_EQUIV(mime, "application/json") || BU_STR_EQUIV(mime, "text/plain"))
	return {};
    return mime;
}

static std::string
gdal_dataset_identity(GDALDatasetH dataset, const char *source_path, const char *requested)
{
    GDALDriverH driver = GDALGetDatasetDriver(dataset);
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "ESRIC")) return gcv_esric::identity(source_path);
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "IRIS") && requested && !BU_STR_EQUAL(requested, mime_iris::type)) {
	const auto profile = gdal_iris_profile(source_path);
	if (profile.product_view() && BU_STR_EQUAL(requested, profile.product_view())) return profile.product_view();
    }
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "EHdr") &&
	requested && BU_STR_EQUAL(requested, mime_raster_label::bil_data)) {
	const auto profile = gcv_gdal_raster::label(dataset, source_path);
	/* BIL is a valid published view of this independently confirmed
	 * source subset, not a canonical alias for the whole family. */
	if (profile.kind == mime_raster_label::family::ehdr && profile.header_known && profile.interleave() == "BIL")
	    return mime_raster_label::bil_data;
    }
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "VICAR")) {
	/* This SDK can use its VICAR reader for an embedded image while the
	 * source remains a PDS-labelled file. */
	constexpr char identifier[] = "PDS_VERSION_ID";
	char prefix[sizeof(identifier) - 1];
	std::ifstream source(source_path, std::ios::binary);
	source.read(prefix, sizeof(prefix));
	if (!source.bad() && source.gcount() == sizeof(prefix) && !memcmp(prefix, identifier, sizeof(prefix))) {
	    source >> std::ws;
	    if (source.peek() == '=') return "application/x-pds";
	}
    }
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "MRF")) {
	std::ifstream source(source_path, std::ios::binary);
	if (mime_lerc::inspect(source).identified) return mime_lerc::type;
	return gdal_tar_profile(source_path).identified ? mime_tar::type : mime_xml::mrf_type;
    }
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "KMLSUPEROVERLAY"))
	return mime_zip::end_record(source_path) ? gdal_kmz_type : mime_xml::kml_type;
    if (driver && BU_STR_EQUAL(GDALGetDriverShortName(driver), "PNM")) {
	/* This driver accepts P5/P6 only. Its generic MIME metadata names
	 * the family rather than the actual source representation. */
	const int bands = GDALGetRasterCount(dataset);
	return bands == 1 ? "image/x-portable-graymap" : bands == 3 ? "image/x-portable-pixmap" : "";
    }
    return gdal_format_identity(driver);
}

static bool
gdal_input_profile_matches(struct gcv_context *context, const char *source_path, const char *type, GDALDatasetH dataset)
{
    const struct bu_mime_traits *requested = gcv_target_request_traits(context);
    if (BU_STR_EQUAL(type, mime_esric::xml_type) || BU_STR_EQUAL(type, mime_esric::json_type) || BU_STR_EQUAL(type, mime_esric::zip_type)) {
	bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
	const bool valid = gcv_esric::inspect(source_path, type, observed, requested) && gdal_audited_profile_matches(type, observed) &&
	    gcv_probe_file_traits_match(context, source_path, type, &observed, requested);
	bu_mime_traits_free(&observed);
	return valid;
    }
    if (BU_STR_EQUAL(type, mime_raster_label::photomod_label) || BU_STR_EQUAL(type, mime_iris::type) ||
	BU_STR_EQUAL(type, mime_iris::ppi_type) || BU_STR_EQUAL(type, mime_iris::cappi_type))
	return gdal_scientific_input_matches(context, source_path, type, requested);
    if (BU_STR_EQUAL(type, mime_raster_label::rraster_label) || BU_STR_EQUAL(type, mime_raster_label::roipac_data) ||
	BU_STR_EQUAL(type, mime_raster_label::isce_data) || BU_STR_EQUAL(type, mime_raster_label::ehdr_data) || BU_STR_EQUAL(type, mime_raster_label::bil_data))
	return gdal_raster_sidecar_matches(context, source_path, type, dataset, requested);
    if (BU_STR_EQUAL(type, mime_hf2::type) || BU_STR_EQUAL(type, mime_sigdem::type) ||
	BU_STR_EQUAL(type, mime_byn::type) || BU_STR_EQUAL(type, mime_isg::type) || BU_STR_EQUAL(type, mime_noaa_grid::type))
	return gdal_heightfield_input_matches(context, source_path, type, requested);
    if (BU_STR_EQUAL(type, mime_leveller::type)) return gdal_leveller_input_matches(context, source_path, dataset, requested);
    if (BU_STR_EQUAL(type, mime_lerc::type)) return gdal_lerc_input_matches(context, source_path, requested);
    if (BU_STR_EQUAL(type, mime_tar::type)) return gdal_tar_input_matches(context, source_path, requested);
    if (BU_STR_EQUAL(type, mime_stac::type)) return gdal_stac_input_matches(context, source_path, requested);
    if (BU_STR_EQUAL(type, mime_xml::pds4_type) || BU_STR_EQUAL(type, mime_xml::mrf_type) ||
	BU_STR_EQUAL(type, mime_xml::kml_type) || BU_STR_EQUAL(type, gdal_kmz_type))
	return gdal_document_input_matches(context, source_path, type, requested);
    if (BU_STR_EQUAL(type, mime_zarr::type)) return gdal_zarr_input_matches(context, source_path, requested);
    if (BU_STR_EQUAL(type, "image/x-portable-graymap") || BU_STR_EQUAL(type, "image/x-portable-pixmap")) {
	FILE *stream = fopen(source_path, "rb");
	if (!stream) return false;
	mime_pnm_input input = {stream, NULL, NULL, 0};
	mime_pnm_header header;
	bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
	bool valid = mime_pnm_header_read(&input, &header) && header.complete && mime_pnm_binary(&header) &&
	    BU_STR_EQUAL(mime_pnm_type(&header), type) && mime_pnm_append_traits(&header, &observed) &&
	    gcv_probe_file_traits_match(context, source_path, type, &observed, requested) && !ferror(stream);
	bu_mime_traits_free(&observed);
	if (fclose(stream)) valid = false;
	return valid;
    }
    for (const char *audited : {mime_vicar::type, mime_cals::type, "image/bmp", "application/vnd.nitf", "application/geopackage+sqlite3", "application/grib", mime_terrain::hfa_type, mime_terrain::bt_type, mime_terrain::ascii_grid_type, mime_terrain::grass_type, mime_terrain::xyz_type, mime_terrain::surfer_type, mime_terrain::dted_type, mime_terrain::gxf_type, mime_terrain::zmap_type})
	if (BU_STR_EQUAL(type, audited)) return gdal_audited_input_matches(context, source_path, type, requested);
    const bool idrisi = BU_STR_EQUAL(type, "application/x-rst");
    if (idrisi) {
	if (GDALGetRasterCount(dataset) < 1) return false;
	const char *unit = GDALGetRasterUnitType(GDALGetRasterBand(dataset, 1));
	const double scale = unit ? bu_units_conversion(unit) : 0;
	if (!std::isfinite(scale) || scale <= 0) return false;
    }
    if (!requested || !requested->count) return true;
    struct bu_mime_result *result = NULL;
    const bool inspected = gcv_detect_file(context, source_path, &result, NULL) != BU_MIME_ERROR;
    const struct bu_mime_traits *observed = NULL;
    for (size_t i = 0; inspected && result && i < result->count; ++i)
	if (BU_STR_EQUAL(result->candidates[i].name, type)) observed = &result->candidates[i].traits;
    /* SDK acceptance establishes the driver identity, not arbitrary caller
     * assertions. Only independently inspected source facts establish them. */
    bu_mime_traits confirmed = BU_MIME_TRAITS_INIT_ZERO;
    const bool matches = inspected && bu_mime_traits_copy(&confirmed, observed) &&
	(!idrisi || bu_mime_traits_add(&confirmed, "raster.value-dimension", "length")) &&
	gcv_probe_traits_match(type, &confirmed, requested);
    bu_mime_traits_free(&confirmed);
    bu_mime_result_free(result);
    return matches;
}

static void
gdal_state_init(struct conversion_state *gs)
{
    if(!gs) return;
    gs->gcv_options = NULL;
    gs->ops = NULL;
    gs->input_file = NULL;
    gs->wdbp = RT_WDB_NULL;

    gs->hDataset = NULL;
}

static int
get_dataset_info(GDALDatasetH hDataset)
{
    char *gdal_info = GDALInfo(hDataset, NULL);
    if (gdal_info)
	bu_log("%s", gdal_info);
    CPLFree(gdal_info);
    return 0;
}

static int
gdal_can_read(const char *data)
{
    GDALDatasetH hDataset;
    GDALAllRegister();

    if (!data)
	return 0;

    if (!bu_file_exists(data,NULL))
	return 0;

    hDataset = GDALOpenEx(data, GDAL_OF_READONLY | GDAL_OF_RASTER , NULL, NULL, NULL);

    if (!hDataset)
	return 0;

    GDALClose(hDataset);

    return 1;
}

static void
gdal_elev_minmax(GDALDatasetH ds)
{
    int bmin, bmax = 0;
    double mm[2];
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    mm[0] = GDALGetRasterMinimum(band, &bmin);
    mm[1] = GDALGetRasterMaximum(band, &bmin);
    if (!bmin || !bmax)
	GDALComputeRasterMinMax(band, TRUE, mm);
    bu_log("Elevation Minimum/Maximum: %f, %f\n", mm[0], mm[1]);
}

/* Get the UTM zone of the GDAL dataset - see
 * https://gis.stackexchange.com/questions/241696/how-to-convert-from-lat-lon-to-utm-with-gdaltransform
 * and the linked posts in the answers for more info. */
static int
gdal_utm_zone(struct conversion_state *state)
{
    int zone = INT_MAX;
    double longitude = 0.0;
    (void)gdal_ll(state->hDataset, NULL, &longitude);
    if (longitude > -180.0 && longitude < 180.0) {
	zone = floor((longitude + 180) / 6) + 1;
	bu_log("zone: %d\n", zone);
    }
    return zone;
}


/* Get corresponding EPSG number of the UTM zone - see
 * https://gis.stackexchange.com/questions/241696/how-to-convert-from-lat-lon-to-utm-with-gdaltransform
 * and the linked posts in the answers for more info. */
static int
gdal_utm_epsg(struct conversion_state *state, int zone)
{
    int epsg = INT_MAX;
    double latitude = 0.0;
    if (gdal_ll(state->hDataset, &latitude, NULL)) {
	epsg = (latitude > 0) ? 32600 + zone : 32700 + zone;
    }
    bu_log("epsg: %d\n", epsg);
    return epsg;
}

static int
gdal_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
	const void *options_data, const char *source_path)
{
    size_t count;
    unsigned int xsize = 0;
    unsigned int ysize = 0;
    struct conversion_state *state;
    struct bu_vls name_root = BU_VLS_INIT_ZERO;
    struct bu_vls name_data = BU_VLS_INIT_ZERO;
    struct bu_vls name_dsp = BU_VLS_INIT_ZERO;
    BU_GET(state, struct conversion_state);
    gdal_state_init(state);
    state->gcv_options = gcv_options;
    state->ops = (struct gdal_read_options *)options_data;
    state->input_file = source_path;

    /* If the environment is identifying a PROJ_LIB directory use it,
     * else set it to the correct one for BRL-CAD */
    if (!getenv("PROJ_LIB")) {
	const char *proj_dir = bu_dir(NULL, 0, BU_DIR_DATA, "proj", NULL);
	if (bu_setenv("PROJ_LIB", proj_dir, 1) == 0)
	    bu_log("Setting PROJ_LIB to %s\n", proj_dir);
	else
	    bu_log("Unable to set PROJ_LIB\n");
    }

    GDALAllRegister();

    const char *requested = gcv_target_mime(context);
    std::string fallback_path;
    if (requested && BU_STR_EQUAL(requested, mime_hf2::type)) {
	std::ifstream source(source_path, std::ios::binary);
	const auto profile = gcv_hf2::inspect(source, true);
	/* HF2's own open callback only unwraps gzip for recognized suffixes.
	 * Use its existing virtual I/O path when content establishes HFZ. */
	if (source.is_open() && !source.bad() && !profile.error && profile.compressed && profile.header.identified)
	    fallback_path = std::string("/vsigzip/") + source_path;
    }
    if (requested && BU_STR_EQUAL(requested, mime_esric::zip_type) && mime_zip::end_record(source_path))
	fallback_path = gcv_esric::member_path(source_path);
    constexpr unsigned int open_flags = GDAL_OF_READONLY | GDAL_OF_RASTER;
    /* Keep the SDK's ordinary path, including its sidecar lookup, whenever
     * it can open the original filename. */
    state->hDataset = GDALOpenEx(source_path, open_flags | (fallback_path.empty() ? GDAL_OF_VERBOSE_ERROR : 0), NULL, NULL, NULL);
    if (!state->hDataset && !fallback_path.empty())
	state->hDataset = GDALOpenEx(fallback_path.c_str(), open_flags | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
    if (!state->hDataset) {
	bu_log("GDAL Reader: error opening input file %s\n", source_path);
	BU_PUT(state, struct conversion_state);
	return 0;
    }
    if (requested) {
	std::string identity = gdal_dataset_identity(state->hDataset, source_path, requested);
	struct bu_vls canonical = BU_VLS_INIT_ZERO;
	int matches = !identity.empty() && bu_mime_canonical(gcv_mime_registry(context), identity.c_str(), &canonical) &&
	    BU_STR_EQUAL(bu_vls_cstr(&canonical), requested) &&
	    gdal_input_profile_matches(context, source_path, requested, state->hDataset);
	bu_vls_free(&canonical);
	if (!matches) {
	    bu_log("GDAL input format %s does not satisfy selected MIME type/profile %s\n",
		identity.empty() ? "unknown" : identity.c_str(), requested);
	    GDALClose(state->hDataset);
	    BU_PUT(state, struct conversion_state);
	    return 0;
	}
    }
    state->wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    (void)get_dataset_info(state->hDataset);
    gdal_elev_minmax(state->hDataset);

    /* Stash the original Spatial Reference System as a PROJ.4 string
     * for later assignment
     */
    char *orig_proj4_str = NULL;
    OGRSpatialReference iSRS(GDALGetProjectionRef(state->hDataset));
    iSRS.exportToProj4(&orig_proj4_str);

    /* Use the information in the data set to deduce the EPSG number
     * corresponding to the correct UTM projection zone, define a
     * spatial reference, and generate the "Well Known Text" to use as
     * the argument to the warping function*/
    GDALDatasetH hOutDS;
    int zone = (state->ops->zone == INT_MAX) ? gdal_utm_zone(state) : state->ops->zone;
    char *dunit = NULL;
    const char *dunit_default = "m";
    struct bu_vls new_proj4_str = BU_VLS_INIT_ZERO;
    if (zone != INT_MAX) {
	int epsg = gdal_utm_epsg(state, zone);
	bu_vls_sprintf(&new_proj4_str, "+init=epsg:%d +zone=%d +proj=utm +no_defs", epsg, zone);
	OGRSpatialReference oSRS;
	oSRS.importFromProj4(bu_vls_addr(&new_proj4_str));
	char *dst_Wkt = NULL;
	oSRS.exportToWkt(&dst_Wkt);

	/* Perform the UTM data warp.  At this point the data is not
	 * yet in the form needed by the DSP primitive */
	hOutDS = GDALAutoCreateWarpedVRT(state->hDataset, NULL, dst_Wkt, GRA_CubicSpline, 0.0, NULL);
	CPLFree(dst_Wkt);

	if (hOutDS) {
	    dunit = bu_strdup(GDALGetRasterUnitType(((GDALDataset *)hOutDS)->GetRasterBand(1)));
	    bu_log("\nTransformed dataset info:\n");
	    (void)get_dataset_info(hOutDS);
	    gdal_elev_minmax(hOutDS);
	}
    } else {
	hOutDS = state->hDataset;
	dunit = bu_strdup(GDALGetRasterUnitType(((GDALDataset *)hOutDS)->GetRasterBand(1)));
    }
    if (!dunit || strlen(dunit) == 0)
	dunit = (char *)dunit_default;

    /* Do the translate step (a.l.a gdal_translate) that puts the data
     * in a form we can use */
    char *img_opts[3];
    img_opts[0] = bu_strdup("-of");
    img_opts[1] = bu_strdup("MEM");
    img_opts[2] = NULL;
    GDALTranslateOptions *gdalt_opts = GDALTranslateOptionsNew(img_opts, NULL);
    GDALDatasetH flatDS = GDALTranslate("", hOutDS, gdalt_opts, NULL);
    GDALTranslateOptionsFree(gdalt_opts);
    if (hOutDS != state->hDataset)
	GDALClose(hOutDS);
    for(int i = 0; i < 3; i++) {
	bu_free(img_opts[i], "imgopt");
    }

    /* Stash the flat Spatial Reference System as a PROJ.4 string for
     * later assignment */
    char *flat_proj4_str = NULL;
    OGRSpatialReference fSRS(GDALGetProjectionRef(flatDS));
    fSRS.exportToProj4(&flat_proj4_str);

    bu_log("\nFinalized dataset info:\n");
    (void)get_dataset_info(flatDS);
    gdal_elev_minmax(flatDS);

    /* Read the data into something a DSP can process - note that y is
     * backwards for DSPs compared to GDAL */
    unsigned short *uint16_array = NULL;
    uint16_t *scanline = NULL;
    GDALRasterBandH band = GDALGetRasterBand(flatDS, 1);
    xsize = GDALGetRasterBandXSize(band);
    ysize = GDALGetRasterBandYSize(band);
    count = xsize * ysize; /* TODO - check for overflow here... */
    uint16_array = (unsigned short *)bu_calloc(count, sizeof(unsigned short), "unsigned short array");
    for(unsigned int i = 0; i < ysize; i++) {
	if (GDALRasterIO(band, GF_Read, 0, ysize-i-1, xsize, 1, &(uint16_array[i*xsize]), xsize, 1, GDT_UInt16, 0, 0) == CPLE_None) {
	    continue;
	} else {
	    bu_log("GDAL read error for band scanline %d\n", i);
	}
    }

    /* Collect info for DSP */
    double px = 0.0; double py = 0.0;
    double cx = 0.0; double cy = 0.0;
    double fGeoT[6];
    if (GDALGetGeoTransform(flatDS, fGeoT) == CE_None) {
	cx = (fGeoT[0] + fGeoT[1] * GDALGetRasterXSize(flatDS)/2.0 + fGeoT[2] * GDALGetRasterYSize(flatDS)/2.0) - fGeoT[0];
	cy = (fGeoT[3] + fGeoT[4] * GDALGetRasterXSize(flatDS)/2.0 + fGeoT[5] * GDALGetRasterYSize(flatDS)/2.0) - fGeoT[3];
	px = fGeoT[0];
	/* y is backwards for DSPs compared to GDAL */
	py = fGeoT[3] + 2 * cy;
    }

    /* We're in business - come up with some names for the objects */
    if (!bu_path_component(&name_root, source_path, BU_PATH_BASENAME_EXTLESS)) {
	bu_vls_sprintf(&name_root, "dsp");
    }
    bu_vls_sprintf(&name_data, "%s.data", bu_vls_addr(&name_root));
    bu_vls_sprintf(&name_dsp, "%s.s", bu_vls_addr(&name_root));

    /* Got it - write the binary object to the .g file */
    mk_binunif(state->wdbp, bu_vls_addr(&name_data), (void *)uint16_array, WDB_BINUNIF_UINT16, count);

    /* Done reading - close out inputs. */
    CPLFree(scanline);
    GDALClose(flatDS);
    GDALClose(state->hDataset);

    /* TODO: if we're going to BoT (3-space mesh, will depend on the
     * transform requested) we will need different logic... */

    /* Write out the dsp.  Since we're using a data object, instead of
     * a file, do the setup by hand. */
    struct rt_dsp_internal *dsp;
    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic = RT_DSP_INTERNAL_MAGIC;
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, bu_vls_addr(&name_data));
    dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
    dsp->dsp_xcnt = xsize;
    dsp->dsp_ycnt = ysize;
    dsp->dsp_smooth = 1;
    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;
    MAT_IDN(dsp->dsp_stom);
    dsp->dsp_stom[0] = dsp->dsp_stom[5] = fGeoT[1] * bu_units_conversion(dunit);
    dsp->dsp_stom[10] = bu_units_conversion(dunit);
    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    if (state->ops->center) {
	dsp->dsp_stom[3] = -cx * bu_units_conversion(dunit);
	/* y is backwards for DSPs compared to GDAL */
	dsp->dsp_stom[7] = cy * bu_units_conversion(dunit);
    } else {
	dsp->dsp_stom[3] = px * bu_units_conversion(dunit);
	dsp->dsp_stom[7] = py * bu_units_conversion(dunit);
    }

    wdb_export(state->wdbp, bu_vls_addr(&name_dsp), (void *)dsp, ID_DSP, 1);

    /* Write out the original and current Spatial Reference Systems
     * and other dimensional information to attributes on the dsp */
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    struct directory *dp = db_lookup(context->dbip, bu_vls_addr(&name_dsp), LOOKUP_QUIET);
    if (dp != RT_DIR_NULL && !db5_get_attributes(context->dbip, &avs, dp)) {
	struct bu_vls tstr = BU_VLS_INIT_ZERO;
	(void)bu_avs_add(&avs, "s_srs" , orig_proj4_str);
	(void)bu_avs_add(&avs, "t_srs" , bu_vls_addr(&new_proj4_str));
	(void)bu_avs_add(&avs, "f_srs" , flat_proj4_str);
	bu_vls_sprintf(&tstr, "%f %s", fabs(2*cx), dunit);
	(void)bu_avs_add(&avs, "x_length" , bu_vls_addr(&tstr));
	bu_vls_sprintf(&tstr, "%f %s", fabs(2*cy), dunit);
	(void)bu_avs_add(&avs, "y_length" , bu_vls_addr(&tstr));
	if (state->ops->center) {
	    bu_vls_sprintf(&tstr, "%f %s", px + cx, dunit);
	    (void)bu_avs_add(&avs, "x_offset" , bu_vls_addr(&tstr));
	    bu_vls_sprintf(&tstr, "%f %s", py - cy, dunit);
	    (void)bu_avs_add(&avs, "y_offset" , bu_vls_addr(&tstr));
	}
	bu_vls_free(&tstr);
	(void)db5_update_attributes(dp, &avs, context->dbip);
    }

    if (dunit != dunit_default)
	bu_free(dunit, "free dunit");
    if (orig_proj4_str)
	CPLFree(orig_proj4_str);
    if (flat_proj4_str)
	CPLFree(flat_proj4_str);
    bu_vls_free(&new_proj4_str);

    bu_vls_free(&name_root);
    bu_vls_free(&name_data);
    bu_vls_free(&name_dsp);

    return 1;
}

static void
gdal_read_create_opts(struct bu_opt_desc **odesc, void **dest_options_data)
{
    struct gdal_read_options *odata;

    BU_ALLOC(odata, struct gdal_read_options);
    *dest_options_data = odata;
    *odesc = (struct bu_opt_desc *)bu_malloc(4*sizeof(struct bu_opt_desc), "gdal option descriptions");

    odata->center = 0;
    odata->proj = NULL;
    odata->zone = INT_MAX;

    BU_OPT((*odesc)[0], "c", "center", NULL, NULL, &(odata->center), "Center the dsp terrain at global (0,0)");
    BU_OPT((*odesc)[1], "",  "utm-zone", "zone", &bu_opt_int, &(odata->zone), "When performing UTM project use the specified zone.");
    BU_OPT((*odesc)[2], "", "projection", "proj", &bu_opt_str, &(odata->proj), "Projection to apply to the terrain data before exporting to a .g file");
    BU_OPT_NULL((*odesc)[3]);
}

static void
gdal_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

extern "C"
{
    struct gcv_filter gcv_conv_gdal_read =
    {"GDAL Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_GDAL, gdal_can_read, gdal_read_create_opts, gdal_read_free_opts, gdal_read};

    static const struct gcv_filter * const filters[] = {&gcv_conv_gdal_read, NULL};
    const struct gcv_plugin gcv_plugin_info_s = { filters };
    COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info(){return &gcv_plugin_info_s;}

    COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
    gcv_plugin_info_v2()
    {
	static const gcv_mime_catalog catalog = []() {
	    gcv_mime_catalog result;
	    GDALAllRegister();
	    for (int i = 0; i < GDALGetDriverCount(); ++i) {
		GDALDriverH driver = GDALGetDriver(i);
		/* CAD fails in this SDK's metadata-copy loop; Terragen fails in
		 * the current terrain path. NSIDC concentration scales and masks
		 * are not interpreted by this adapter. Keep these exclusions
		 * independent of MIME metadata an SDK may later supply. SAFE needs
		 * GCP support and Sentinel-2 needs subdataset selection; the current
		 * affine terrain reader implements neither. */
		if (BU_STR_EQUAL(GDALGetDriverShortName(driver), "CAD") ||
		    BU_STR_EQUAL(GDALGetDriverShortName(driver), "Terragen") ||
		    BU_STR_EQUAL(GDALGetDriverShortName(driver), "NSIDCbin") ||
		    BU_STR_EQUAL(GDALGetDriverShortName(driver), "SAFE") ||
		    BU_STR_EQUAL(GDALGetDriverShortName(driver), "SENTINEL2")) continue;
		const char *raster = GDALGetMetadataItem(driver, GDAL_DCAP_RASTER, NULL);
		const char *open = GDALGetMetadataItem(driver, GDAL_DCAP_OPEN, NULL);
		if (!raster || !open || !CPLTestBool(raster) || !CPLTestBool(open))
		    continue;
		const char *extensions = GDALGetMetadataItem(driver, GDAL_DMD_EXTENSIONS, NULL);
		bool audited = false;
		for (const auto &profile : gdal_audited_profiles())
		    if (BU_STR_EQUAL(GDALGetDriverShortName(driver), profile.driver)) {
			const char *creation = GDALGetMetadataItem(driver, GDAL_DMD_CREATIONOPTIONLIST, NULL);
			if (BU_STR_EQUAL(profile.type, mime_lerc::type) && (!creation || !strstr(creation, "<Value>LERC</Value>"))) continue;
			result.add(profile.type, GDALGetDriverLongName(driver),
			    profile.extensions.empty() ? gcv_mime_catalog::split(extensions) : profile.extensions,
			    true, false, profile.traits, BU_MIME_DEFER_INPUT_VALIDATION, profile.sets);
			audited = true;
		    }
		if (audited) continue;
		std::string name = gdal_format_identity(driver);
		if (name.empty())
		    continue;
		result.add(name, GDALGetDriverLongName(driver), gcv_mime_catalog::split(extensions), true, false);
	    }
	    result.finish(&gcv_conv_gdal_read, NULL, gdal_probe, gdal_inspect);
	    return result;
	}();
	return catalog.plugin();
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
