/*                 S T E P _ P L U G I N _ A P I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Test the STEP-specific libbu plugin ABI without a schema binding.
 */

#include "common.h"

#include "step_plugin.h"
#include "STEPHeaderSchema.h"
#include "StepPluginHost.h"
#include "StepMime.h"

#include <stdexcept>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/mime_runtime.h"

static int
successful_command(const struct step_plugin_request *, struct step_plugin_result *result)
{
    step_plugin_result_message(result, STEP_PLUGIN_STATUS_OK, "ok");
    return STEP_PLUGIN_STATUS_OK;
}

static int
throwing_command(const struct step_plugin_request *, struct step_plugin_result *)
{
    throw std::runtime_error("caught test exception");
}

static bool
schema_selection()
{
    const char *revisions[] = {"1", "3", "4", "5"};
    for (size_t i = 0; i < sizeof(revisions) / sizeof(revisions[0]); ++i) {
	const std::string text = "ISO-10303-21; HEADER; FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF "
	    "{1 0 10303 442 " + std::string(revisions[i]) + " 1 4}')); ENDSEC; DATA;";
	const auto header = brlcad::step::STEPHeaderSchema::inspect_header(text);
	const std::vector<std::string> expected = {"ap242e" + std::to_string(i + 1)};
	std::string error;
	for (const char *choice : {"auto", "ap242"})
	    if (brlcad::step::schema_plugin_candidates(header, choice, error) != expected || !error.empty()) return false;
	if (brlcad::step::schema_plugin_candidates(header, expected.front(), error) != expected || !error.empty()) return false;
	const std::string wrong = i == 0 ? "ap242e4" : "ap242e1";
	if (brlcad::step::schema_plugin_candidates(header, wrong, error) != std::vector<std::string>{wrong} || !error.empty()) return false;
    }
    const auto unknown = brlcad::step::STEPHeaderSchema::inspect_header(
	"ISO-10303-21; HEADER; FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF')); ENDSEC; DATA;");
    std::string error;
    const std::vector<std::string> fallback = {"ap242e4", "ap242e3", "ap242e2", "ap242e1"};
    if (brlcad::step::schema_plugin_candidates(unknown, "auto", error) != fallback || !error.empty()) return false;
    if (brlcad::step::schema_plugin_candidates(unknown, "ap242e2", error) != std::vector<std::string>{"ap242e2"} || !error.empty()) return false;
    if (!brlcad::step::schema_plugin_candidates(unknown, "ap203", error).empty() || error.empty()) return false;
    return bu_plugin_loaded_modules_count() == 0;
}

#define MIME_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("STEP MIME assertion failed at line %d: %s\n", __LINE__, #condition); \
	return false; \
    } \
} while (0)

static bool
mime_assertions()
{
    using brlcad::step::STEPHeaderSchema;
    const auto inspect = [](const std::string &identifiers) {
	return STEPHeaderSchema::inspect_header("ISO-10303-21; HEADER; FILE_SCHEMA((" + identifiers + ")); ENDSEC; DATA;");
    };
    const char *ap242 = "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF";
    const auto known = inspect("'" + std::string(ap242) + " {1 0 10303 442 1 1 4}'");
    std::string error;
    bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("step.ap", "242"), BU_MIME_TRAIT_STRING("step.edition", "1")};
    bu_mime_traits required = {fields, sizeof(fields) / sizeof(fields[0])};
    for (const char *format : {"model/step", "MODEL/STEP", "step", "STP"})
	MIME_CHECK(STEPHeaderSchema::matches_mime(known, format, &required, error) && error.empty());
    MIME_CHECK(STEPHeaderSchema::matches_mime(known, NULL, &required, error));
    MIME_CHECK(brlcad::step::schema_plugin_candidates(known, "ap242e4", error) == std::vector<std::string>{"ap242e4"});
    MIME_CHECK(known.edition == "1");
    MIME_CHECK(!STEPHeaderSchema::matches_mime(known, "image/png", NULL, error) && !error.empty());
    MIME_CHECK(!STEPHeaderSchema::matches_mime(known, "unknown-format", NULL, error) && !error.empty());
    bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("step.ap", "203"), BU_MIME_TRAIT_STRING("step.edition", "4"),
	BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("version", "1"),
	BU_MIME_TRAIT_STRING("step.oid", "unestablished")};
    for (auto &field : unsupported) {
	const bu_mime_traits wrong = {&field, 1};
	MIME_CHECK(!STEPHeaderSchema::matches_mime(known, NULL, &wrong, error) && !error.empty());
    }
    const char *protocols[] = {"242", "203"};
    bu_mime_trait protocol = BU_MIME_TRAIT_SET("step.application-protocols", protocols, 1);
    bu_mime_traits requested = {&protocol, 1};
    MIME_CHECK(STEPHeaderSchema::matches_mime(known, NULL, &requested, error));
    protocol.member_count = sizeof(protocols) / sizeof(protocols[0]);
    MIME_CHECK(!STEPHeaderSchema::matches_mime(known, NULL, &requested, error));
    const auto bare = inspect("'" + std::string(ap242) + "'");
    const auto future = inspect("'" + std::string(ap242) + " {1 0 10303 442 999 1 4}'");
    MIME_CHECK(!STEPHeaderSchema::matches_mime(bare, NULL, &required, error));
    MIME_CHECK(!STEPHeaderSchema::matches_mime(future, NULL, &required, error));
    bu_mime_trait empty = BU_MIME_TRAIT_SET("step.oids", NULL, 0);
    requested = {&empty, 1};
    MIME_CHECK(STEPHeaderSchema::matches_mime(bare, NULL, &requested, error));
    MIME_CHECK(!STEPHeaderSchema::matches_mime(known, NULL, &requested, error));
    empty.key = "step.profiles";
    MIME_CHECK(!STEPHeaderSchema::matches_mime(bare, NULL, &requested, error));
    bu_mime_trait generic = BU_MIME_TRAIT_STRING("p21.schema", ap242);
    requested = {&generic, 1};
    MIME_CHECK(STEPHeaderSchema::matches_mime(known, "application/p21", &requested, error));
    MIME_CHECK(!STEPHeaderSchema::matches_mime(known, "model/step", &requested, error));
    const auto companion = inspect("'CONFIG_CONTROL_DESIGN','ALIBRE_SCHEMA'");
    MIME_CHECK(companion.recognized);
    MIME_CHECK(!STEPHeaderSchema::matches_mime(companion, "model/step", NULL, error));
    const char *schemas[] = {"CONFIG_CONTROL_DESIGN", "ALIBRE_SCHEMA"};
    bu_mime_trait schema_set = BU_MIME_TRAIT_SET("p21.schemas", schemas, sizeof(schemas) / sizeof(schemas[0]));
    requested = {&schema_set, 1};
    MIME_CHECK(STEPHeaderSchema::matches_mime(companion, "application/p21", &requested, error));
    MIME_CHECK(bu_plugin_loaded_modules_count() == 0);
    return true;
}

static bool
mime_output_selection()
{
    using namespace brlcad::step;
    std::string error;
    const struct {
	const char *schema;
	const char *ap;
	const char *edition;
    } profiles[] = {
	{"ap203", "203", "1"}, {"ap203e2", "203", "2"}, {"ap214", "214", "3"},
	{"ap242e1", "242", "1"}, {"ap242e2", "242", "2"},
	{"ap242e3", "242", "3"}, {"ap242e4", "242", "4"}
    };
    for (const auto &profile : profiles) {
	const auto *declaration = output_schema_profile(profile.schema);
	MIME_CHECK(declaration);
	const auto header = STEPHeaderSchema::inspect_header(
	    "ISO-10303-21; HEADER; FILE_SCHEMA((" + std::string(declaration->file_schema) + ")); ENDSEC; DATA;");
	MIME_CHECK(header.recognized && header.edition == profile.edition);
	for (const char *type : {"model/step", "application/p21"}) {
	    MimeTraits emitted;
	    MIME_CHECK(append_output_schema_traits(profile.schema, type, emitted.value));
	    MIME_CHECK(STEPHeaderSchema::matches_mime(header, type, &emitted.value, error));
	    const auto selected = select_schema_output(type, &emitted.value, "", "ap203", error);
	    if (schema_plugin_available(profile.schema)) {
		MIME_CHECK(selected == profile.schema && error.empty());
		MIME_CHECK(select_schema_output(type, &emitted.value, profile.schema, "ap203", error) == profile.schema);
	    } else {
		MIME_CHECK(selected.empty() && !error.empty());
	    }
	}
	bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("step.ap", profile.ap),
	    BU_MIME_TRAIT_STRING("step.edition", profile.edition)};
	const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
	const auto selected = select_schema_output("STP", &requested, "", "ap203", error);
	MIME_CHECK(schema_plugin_available(profile.schema) ? selected == profile.schema && error.empty() : selected.empty());
    }
    for (const auto &schema : available_schema_plugins())
	MIME_CHECK(select_schema_output(NULL, NULL, "auto", schema, error) == schema && error.empty());
    MIME_CHECK(select_schema_output("image/png", NULL, "", "ap203", error).empty() && !error.empty());
    MIME_CHECK(select_schema_output("unknown-format", NULL, "", "ap203", error).empty());
    MIME_CHECK(select_schema_output(NULL, NULL, "unknown-schema", "ap203", error).empty());
    bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", "ascii"),
	BU_MIME_TRAIT_STRING("version", "1"), BU_MIME_TRAIT_STRING("step.ap", "999"),
	BU_MIME_TRAIT_STRING("step.oid", "1.0.10303.442.7.1.4")};
    for (auto &field : unsupported) {
	const bu_mime_traits requested = {&field, 1};
	MIME_CHECK(select_schema_output(NULL, &requested, "", "ap203", error).empty() && !error.empty());
    }
    bu_mime_trait edition = BU_MIME_TRAIT_STRING("step.edition", "4");
    bu_mime_traits requested = {&edition, 1};
    MIME_CHECK(select_schema_output(NULL, &requested, "ap203", "ap203", error).empty() && !error.empty());
    if (schema_plugin_available("ap242e4")) {
	MIME_CHECK(select_schema_output(NULL, &requested, "ap242", "ap203", error) == "ap242e4");
	MIME_CHECK(select_schema_output(NULL, &requested, "", "ap203", error) == "ap242e4");
    }
    edition.value = "2";
    if (schema_plugin_available("ap203e2") && schema_plugin_available("ap242e2"))
	MIME_CHECK(select_schema_output(NULL, &requested, "", "ap203", error).empty() && error.find("ambiguous") != std::string::npos);
    bu_mime_trait empty = BU_MIME_TRAIT_SET("step.application-protocols", NULL, 0);
    requested = {&empty, 1};
    MIME_CHECK(select_schema_output(NULL, &requested, "", "ap203", error).empty());
    const char *protocols[] = {"203", "242"};
    bu_mime_trait both = BU_MIME_TRAIT_SET("step.application-protocols", protocols, sizeof(protocols) / sizeof(protocols[0]));
    requested = {&both, 1};
    MIME_CHECK(select_schema_output(NULL, &requested, "", "ap203", error).empty());
    const char *unexpected[] = {"ALIBRE_SCHEMA"};
    bu_mime_trait schema = BU_MIME_TRAIT_SET("p21.schemas", unexpected, 1);
    requested = {&schema, 1};
    MIME_CHECK(select_schema_output("application/p21", &requested, "", "ap203", error).empty());
    MIME_CHECK(bu_plugin_loaded_modules_count() == 0);
    return true;
}

static bool
mime_discovery()
{
    using namespace brlcad::step;
    const auto source_traits = [](const char *declaration, MimeTraits &traits) {
	const auto header = STEPHeaderSchema::inspect_header("ISO-10303-21; HEADER; FILE_SCHEMA((" +
	    std::string(declaration) + ")); ENDSEC; DATA;");
	mime_p21::profile source;
	source.identifiers = header.source_identifiers;
	source.complete = header.recognized;
	return source.append_step_traits(traits.value);
    };
    MimeTraits ap203, ap203e2, ap214, ap242, ap242e1, ap242e4;
    MIME_CHECK(source_traits("'CONFIG_CONTROL_DESIGN'", ap203));
    MIME_CHECK(source_traits("'CONFIG_CONTROL_DESIGN','SHAPE_APPEARANCE_LAYER_MIM'", ap203e2));
    MIME_CHECK(source_traits("'AUTOMOTIVE_DESIGN'", ap214));
    MIME_CHECK(source_traits("'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF'", ap242));
    MIME_CHECK(source_traits("'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 1 1 4}'", ap242e1));
    MIME_CHECK(source_traits("'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 5 1 4}'", ap242e4));
    for (const auto &schema : available_schema_plugins()) {
	bu_mime_capability input = {};
	MIME_CHECK(schema_mime_capability(schema, "model/step", BU_MIME_READ, input));
	MIME_CHECK(input.operations == BU_MIME_READ && input.flags == BU_MIME_DEFER_INPUT_VALIDATION);
	const bu_mime_traits declared = {input.traits, input.trait_count};
	MIME_CHECK(!bu_mime_traits_get(&declared, "step.oid") && !bu_mime_traits_get(&declared, "encoding"));
	if (schema.compare(0, 5, "ap242") == 0) {
	    MIME_CHECK(!bu_mime_traits_get(&declared, "step.edition"));
	    for (const auto *source : {&ap242, &ap242e1, &ap242e4})
		MIME_CHECK(bu_mime_capability_match(&input, &source->value, BU_MIME_INPUT));
	    MIME_CHECK(!bu_mime_capability_match(&input, &ap203.value, BU_MIME_INPUT));
	} else if (schema == "ap203" || schema == "ap203e2") {
	    const auto &supported = schema == "ap203" ? ap203 : ap203e2;
	    const auto &other = schema == "ap203" ? ap203e2 : ap203;
	    MIME_CHECK(bu_mime_capability_match(&input, &supported.value, BU_MIME_INPUT));
	    MIME_CHECK(!bu_mime_capability_match(&input, &other.value, BU_MIME_INPUT));
	} else if (schema == "ap214") {
	    MIME_CHECK(!bu_mime_traits_get(&declared, "step.edition"));
	    MIME_CHECK(bu_mime_capability_match(&input, &ap214.value, BU_MIME_INPUT));
	    MIME_CHECK(!bu_mime_capability_match(&input, &ap242.value, BU_MIME_INPUT));
	}
	bu_mime_capability_free(&input);
	for (const char *type : {"model/step", "application/p21"}) {
	    bu_mime_capability output = {};
	    MIME_CHECK(schema_mime_capability(schema, type, BU_MIME_WRITE, output));
	    MimeTraits emitted;
	    MIME_CHECK(append_output_schema_traits(schema, type, emitted.value));
	    MIME_CHECK(bu_mime_capability_match(&output, &emitted.value, BU_MIME_OUTPUT));
	    MIME_CHECK(output.operations == BU_MIME_WRITE && !output.flags);
	    bu_mime_capability_free(&output);
	}
	MIME_CHECK(!schema_mime_capability(schema, "application/p21", BU_MIME_READ, input));
	MIME_CHECK(!input.mime_type && !input.trait_count);
    }
    bu_mime_capability invalid = {};
    MIME_CHECK(!schema_mime_capability("unknown-schema", "model/step", BU_MIME_READ, invalid));
    MIME_CHECK(!schema_mime_capability("ap203", NULL, BU_MIME_READ, invalid));
    MIME_CHECK(!schema_mime_capability("ap203", "model/step", BU_MIME_METADATA, invalid));
    MIME_CHECK(!invalid.mime_type && !invalid.trait_count);
    MIME_CHECK(bu_plugin_loaded_modules_count() == 0);
    return true;
}

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    if (bu_plugin_init() != 0 ||
        bu_plugin_cmd_register("step.test.success", successful_command) != 0 ||
        bu_plugin_cmd_register("step.test.throw", throwing_command) != 0)
        return 1;

    char diagnostic[128] = {0};
    struct step_plugin_result result = {
        STEP_PLUGIN_RESULT_VERSION, sizeof(struct step_plugin_result), 0, 0,
        0, 0, diagnostic, sizeof(diagnostic), 0
    };
    struct step_import_options options = {};
    options.api_version = STEP_PLUGIN_IMPORT_OPTIONS_VERSION;
    options.struct_size = sizeof(options);
    struct step_plugin_request request = {};
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;
    request.struct_size = sizeof(request);
    request.operation = STEP_OPERATION_IMPORT;
    request.input_path = "input.step";
    request.output_path = "output.g";
    request.schema = "ap203";
    request.import_options = &options;

    if (step_plugin_host_call("step.test.success", &request, &result) != 0)
        return 2;

    request.api_version++;
    if (step_plugin_host_call("step.test.success", &request, &result) != STEP_PLUGIN_STATUS_ABI)
        return 3;
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;

    if (step_plugin_host_call("step.test.throw", &request, &result) != STEP_PLUGIN_STATUS_EXCEPTION)
        return 4;
    if (!strstr(diagnostic, "caught test exception"))
        return 5;

    if (!schema_selection()) return 7;
    if (!mime_assertions()) return 8;
    if (!mime_output_selection()) return 9;
    if (!mime_discovery()) return 10;

    result.struct_size--;
    if (step_plugin_host_call("step.test.success", &request, &result) != STEP_PLUGIN_STATUS_ABI)
        return 6;
    return 0;
}
