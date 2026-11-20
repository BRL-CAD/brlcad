/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "brep.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/ptbl.h"
#include "gcv/api.h"
#include "rt/geom.h"
#include "rt/primitives/datum.h"
#include "wdb.h"

#include "iges_convert.h"
#include "iges_runtime.h"

namespace {
using namespace brlcad::iges;
namespace fs = std::filesystem;

void
require(bool condition, const std::string &message)
{
    if (!condition)
	throw std::runtime_error(message);
}

class Context {
public:
    Context() { gcv_context_init(&value); }
    ~Context() { gcv_context_destroy(&value); }
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    int execute(enum gcv_filter_type type, const fs::path &path,
	const std::vector<const char *> &arguments = {}, const struct gcv_opts *options = nullptr,
	const struct bu_mime_traits *traits = nullptr)
    {
	const auto *filter = gcv_select_filter(&value, type, "model/iges", nullptr, nullptr);
	if (filter)
	    return gcv_execute_variant(&value, filter, "model/iges", traits, options,
		arguments.size(), arguments.data(), path.string().c_str());
	throw std::runtime_error("IGES reader or writer plugin was not loaded");
    }

    struct gcv_context value;
};

void
read_object(Context &context, const char *name, int type, Internal &internal)
{
    const auto *entry = db_lookup(context.value.dbip, name, LOOKUP_QUIET);
    require(entry && rt_db_get_internal(&internal.value, entry, context.value.dbip, nullptr) >= 0 &&
	internal.value.idb_type == type, std::string("missing or incorrect object: ") + name);
}

const struct bu_mime_trait profile_values[] = {
    BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("version", "5.3"),
    BU_MIME_TRAIT_STRING("iges.version-code", "11")
};
const struct bu_mime_traits profile = {profile_values, sizeof(profile_values) / sizeof(profile_values[0])};

std::string
read_file(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "cannot open IGES test output");
    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    require(!input.bad(), "cannot read IGES test output");
    return contents;
}

void
check_profile(const Document &document)
{
    constexpr size_t version_parameter = 23;
    require(document.valid() && document.global().parameters.size() >= version_parameter &&
	document.global().parameters[version_parameter - 1] == "11", "output does not declare IGES 5.3");
}

void
test_profiles(const fs::path &fixtures, const fs::path &outputs)
{
    const auto source = fixtures / "native-csg.igs";
    Context preserved;
    auto *database = wdb_dbopen(preserved.value.dbip, RT_WDB_TYPE_DB_INMEM);
    const point_t center = {1, 2, 3};
    require(database && mk_id_units(database, "Before IGES profile validation", "cm") == 0 &&
	mk_sph(database, "kept.s", center, 1) == 0, "cannot prepare IGES profile sentinel");
    const fastf_t original_units = preserved.value.dbip->dbi_local2base;
    const auto unchanged = [&]() {
	require(std::string(preserved.value.dbip->dbi_title) == "Before IGES profile validation" &&
	    EQUAL(preserved.value.dbip->dbi_local2base, original_units) &&
	    check_output(preserved.value.dbip).objects == 1 &&
	    db_lookup(preserved.value.dbip, "kept.s", LOOKUP_QUIET), "rejected IGES profile changed the database");
    };
    for (const auto &property : std::vector<bu_mime_trait>{
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("version", "5.2"),
	BU_MIME_TRAIT_STRING("version", "7.45.0"), BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion")}) {
	const struct bu_mime_traits request = {&property, 1};
	require(preserved.execute(GCV_FILTER_READ, source, {}, nullptr, &request) == 0,
	    "incorrect or unestablished IGES assertion was accepted");
	unchanged();
    }
    const std::string original = read_file(source);
    const size_t code = original.find(",11,0,");
    require(code != std::string::npos && original.find(",11,0,", code + 1) == std::string::npos,
	"IGES rejection fixture has no unique version code");
    size_t index = 0;
    for (const char *replacement : {"12", "  ", " 2"}) {
	/* These are rejection fixtures, not evidence of another edition's support. */
	std::string changed = original;
	changed.replace(code + 1, 2, replacement);
	const fs::path path = outputs / ("plugin-profile-" + std::to_string(index++) + ".igs");
	std::ofstream output(path, std::ios::binary);
	output << changed;
	output.close();
	require(output.good() && Document::parse_file(path.string()).valid(), "cannot prepare IGES profile fixture");
	require(preserved.execute(GCV_FILTER_READ, path, {}, nullptr, &profile) == 0,
	    "unknown or incompatible IGES declaration satisfied a 5.3 request");
	unchanged();
    }
    Context imported;
    const auto *reader = gcv_select_filter(&imported.value, GCV_FILTER_READ, "application/iges", "IGES Reader", nullptr);
    require(reader && reader == gcv_select_filter(&imported.value, GCV_FILTER_READ, "model/iges", "IGES Reader", nullptr),
	"obsolete IGES name did not resolve to the canonical reader");
    require(gcv_execute_variant(&imported.value, reader, "application/iges", &profile, nullptr,
	0, nullptr, source.string().c_str()) == 1, "IGES 5.3 profile import failed");
    Internal sphere;
    read_object(imported, "sphere.0", ID_ELL, sphere);
    require(NEAR_EQUAL(MAGNITUDE(static_cast<const struct rt_ell_internal *>(sphere.value.idb_ptr)->a),
	10.0, SMALL_FASTF), "profile-aware IGES import changed sphere geometry");
}

void
test_imports(const fs::path &fixtures)
{
    Context empty;
    require(check_output(empty.value.dbip).objects == 0,
	"in-memory database metadata was counted as geometry");
    for (const char *fixture : {"native-csg.igs", "mixed-assembly.igs", "mixed-subfigure.igs",
	"native-wedge.igs", "native-extrude.igs", "native-revolve.igs",
	"numeric-boundary.igs", "hollerith-boundary.igs", "name-collision.igs"}) {
	Context context;
	require(context.execute(GCV_FILTER_READ, fixtures / fixture, {"--strict", "--breps-only"}) == 1,
	    std::string("strict plugin import failed: ") + fixture);
	const auto statistics = check_output(context.value.dbip);
	require(statistics.objects > 0 && statistics.unresolved == 0,
	    std::string("incomplete plugin import: ") + fixture);
    }
    Context sphere_context;
    require(sphere_context.execute(GCV_FILTER_READ, fixtures / "native-csg.igs") == 1,
	"default native CSG import failed");
    Internal sphere;
    read_object(sphere_context, "sphere.0", ID_ELL, sphere);
    require(NEAR_EQUAL(MAGNITUDE(static_cast<const struct rt_ell_internal *>(sphere.value.idb_ptr)->a),
	10.0, SMALL_FASTF), "native sphere radius changed");

    for (bool strict : {false, true}) {
	Context context;
	require(context.execute(GCV_FILTER_READ, fixtures / "bad-property.igs",
	    strict ? std::vector<const char *>{"--strict"} : std::vector<const char *>{}) == (strict ? 0 : 1),
	    "plugin did not preserve strict versus best-effort policy");
    }
    for (unsigned int rejected : {0u, static_cast<unsigned int>(GCV_LOSS_GEOMETRY),
	    static_cast<unsigned int>(GCV_LOSS_METADATA)}) {
	Context context;
	struct gcv_opts options;
	gcv_opts_default(&options);
	options.reject_losses = rejected;
	struct gcv_conversion_result result = GCV_CONVERSION_RESULT_INIT_ZERO;
	const auto *reader = gcv_select_filter(&context.value, GCV_FILTER_READ, "model/iges", nullptr, nullptr);
	const auto path = (fixtures / "bad-property.igs").string();
	const auto status = gcv_execute_result(&context.value, reader, "model/iges", nullptr,
	    &options, 0, nullptr, path.c_str(), &result);
	require(status == (rejected == GCV_LOSS_METADATA ? GCV_CONVERSION_FAILED : GCV_CONVERSION_PARTIAL),
	    "optional IGES metadata repair was classified as geometry loss");
	require(result.loss_count == 1 && result.losses[0].kind == GCV_LOSS_METADATA &&
	    std::string(result.losses[0].feature) == "iges.metadata-repairs",
	    "optional IGES metadata repair was not reported");
	gcv_conversion_result_free(&result);
    }
    for (const auto &arguments : std::vector<std::vector<const char *>>{
	{"--drawings-only", "--breps-only"}, {"--relative-tolerance", "-1"},
	{"--relative-tolerance", "nan"}, {"--repair", "invalid"},
	{"--strict", "--max-repair-tolerance", "1"}}) {
	Context context;
	require(context.execute(GCV_FILTER_READ, fixtures / "native-csg.igs", arguments) == 0,
	    "invalid reader options were accepted");
	require(context.execute(GCV_FILTER_READ, fixtures / "native-csg.igs") == 1,
	    "rejected options corrupted context or reader defaults");
    }
}

void
test_datums(const fs::path &fixtures)
{
    const std::vector<std::vector<const char *>> modes = {
	{}, {"--drawings-only"}, {"--drawings-only", "--3d-drawings"}
    };
    for (size_t mode = 0; mode < modes.size(); ++mode) {
	Context context;
	require(context.execute(GCV_FILTER_READ, fixtures / "datum.igs", modes[mode]) == 1,
	    "datum plugin import failed");
	Internal internal;
	read_object(context, "POINT.datum", ID_DATUM, internal);
	const auto *datum = static_cast<const struct rt_datum_internal *>(internal.value.idb_ptr);
	require(NEAR_EQUAL(datum->pnt[2], mode == 1 ? 0.0 : 3.0, SMALL_FASTF),
	    "drawing projection policy changed datum coordinates");
    }
}

void
test_exports(const fs::path &fixtures, const fs::path &outputs)
{
    Context source;
    require(source.execute(GCV_FILTER_READ, fixtures / "native-csg.igs") == 1,
	"could not prepare export source");
    const fs::path native = outputs / "plugin-native.igs";
    require(source.execute(GCV_FILTER_WRITE, native, {}, nullptr, &profile) == 1, "native CSG plugin export failed");
    const auto document = Document::parse_file(native.string());
    check_profile(document);
    require(document.valid() && std::any_of(document.entities().begin(), document.entities().end(),
	[](const DirectoryEntry &entry) { return entry.type == 158 || entry.type == 168; }),
	"native sphere exported without CSG");
    Context roundtrip;
    require(roundtrip.execute(GCV_FILTER_READ, native, {"--strict"}) == 1,
	"native plugin round trip failed");
    Internal sphere;
    read_object(roundtrip, "sphere_0", ID_ELL, sphere);
    require(NEAR_EQUAL(MAGNITUDE(static_cast<const struct rt_ell_internal *>(sphere.value.idb_ptr)->a),
	10.0, SMALL_FASTF), "plugin round trip changed sphere radius");

    Context box;
    auto *writer = wdb_dbopen(box.value.dbip, RT_WDB_TYPE_DB_INMEM);
    const point_t minimum = {0, 0, 0}, maximum = {10, 20, 30};
    require(writer && mk_rpp(writer, "box.s", minimum, maximum) == 0 &&
	mk_comb1(writer, "box.r", "box.s", 1) == 0,
	"could not prepare faceted export source");
    const std::vector<std::vector<const char *>> modes = {
	{"--faceted"}, {"--trimmed-surfaces"}, {"--faceted", "--nurbs"}
    };
    for (size_t mode = 0; mode < modes.size(); ++mode) {
	const fs::path path = outputs / ("plugin-faceted-" + std::to_string(mode) + ".igs");
	require(box.execute(GCV_FILTER_WRITE, path, modes[mode], nullptr, &profile) == 1,
	    "faceted plugin export failed");
	check_profile(Document::parse_file(path.string()));
	Context imported;
	require(imported.execute(GCV_FILTER_READ, path, {"--breps-only", "--strict"}) == 1 &&
	    check_output(imported.value.dbip).unresolved == 0, "faceted plugin round trip failed");
    }

    Context mixed;
    require(mixed.execute(GCV_FILTER_READ, fixtures / "mixed-subfigure.igs", {"--strict"}) == 1,
	"could not prepare mixed CSG/B-Rep export source");
    for (bool flatten : {false, true}) {
	const fs::path path = outputs / (flatten ? "plugin-flat.igs" : "plugin-brep.igs");
	require(mixed.execute(GCV_FILTER_WRITE, path,
	    flatten ? std::vector<const char *>{"--flatten-brep"} : std::vector<const char *>{}) == 1,
	    "mixed CSG/B-Rep plugin export failed");
	Context imported;
	require(imported.execute(GCV_FILTER_READ, path, {"--breps-only", "--strict"}) == 1,
	    "mixed CSG/B-Rep plugin round trip failed");
	const auto statistics = check_output(imported.value.dbip);
	require(statistics.objects == 2 && statistics.unresolved == 0,
	    "mixed CSG/B-Rep plugin round trip: objects=" + std::to_string(statistics.objects) +
	    ", unresolved references=" + std::to_string(statistics.unresolved));
    }
    const std::string before = read_file(native);
    for (const auto &property : std::vector<bu_mime_trait>{
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("version", "5.2"),
	BU_MIME_TRAIT_STRING("iges.version-code", "10"), BU_MIME_TRAIT_STRING("iges.representation", "csg")}) {
	const struct bu_mime_traits request = {&property, 1};
	require(source.execute(GCV_FILTER_WRITE, native, {}, nullptr, &request) == 0 && read_file(native) == before,
	    "unsupported IGES output profile replaced the destination");
    }
    require(source.execute(GCV_FILTER_WRITE, native, {"--faceted", "--trimmed-surfaces"}) == 0 &&
	Document::parse_file(native.string()).valid(), "invalid output options replaced existing output");
    struct gcv_opts options;
    gcv_opts_default(&options);
    const char *selected = "missing";
    options.num_objects = 1;
    options.object_names = &selected;
    require(source.execute(GCV_FILTER_WRITE, outputs / "plugin-missing.igs", {}, &options) == 0,
	"missing export selection was reported as successful");
    gcv_opts_default(&options);
    options.scale_factor = 2.0;
    Context scaled;
    require(scaled.execute(GCV_FILTER_READ, fixtures / "native-csg.igs", {}, &options) == 0 &&
	source.execute(GCV_FILTER_WRITE, outputs / "plugin-scaled.igs", {}, &options) == 0,
	"unsupported scale override was silently ignored");
}
} // namespace

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 3)
	return 1;
    try {
	test_imports(argv[1]);
	test_datums(argv[1]);
	test_profiles(argv[1], argv[2]);
	test_exports(argv[1], argv[2]);
	return 0;
    } catch (const std::exception &error) {
	bu_log("IGES plugin test: %s\n", error.what());
	return 1;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
