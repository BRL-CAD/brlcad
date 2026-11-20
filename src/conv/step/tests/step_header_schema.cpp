/*              S T E P _ H E A D E R _ S C H E M A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPHeaderSchema.h"
#include "STEPString.h"

#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/mime_runtime.h"
#include "bu/str.h"

using brlcad::step::HeaderSchema;
using brlcad::step::STEPHeaderSchema;

static std::string
header(const std::string &schema)
{
    return "ISO-10303-21;\nHEADER;\nFILE_SCHEMA((" + schema + "));\nENDSEC;\nDATA;\n";
}

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    if (STEPHeaderSchema::inspect_header(header("'CONFIG_CONTROL_DESIGN'")).schema != HeaderSchema::AP203)
	return 1;
    if (STEPHeaderSchema::inspect_header(header(
	"'AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF'"
	)).schema != HeaderSchema::AP203e2)
	return 2;
    if (STEPHeaderSchema::inspect_header(header(
	"/* ignored */ 'AUTOMOTIVE_DESIGN { 1 0 10303 214 3 1 1 1 }'"
	)).schema != HeaderSchema::AP214)
	return 3;
    const brlcad::step::HeaderSchemaInfo qualified_cc2 =
	STEPHeaderSchema::inspect_header(header(
	    "'AUTOMOTIVE_DESIGN_CC2 { 1 2 10303 214 -1 1 5 1 }'"));
    if (!qualified_cc2.recognized || qualified_cc2.schema != HeaderSchema::AP214 ||
	!qualified_cc2.legacy_identifier)
	return 18;
    if (STEPHeaderSchema::inspect_header(header(
	"'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF'"
	)).schema != HeaderSchema::AP242)
	return 4;
    if (!STEPHeaderSchema::inspect_header(header(
	"'CONFIG_CONTROL_DESIGN','AUTOMOTIVE_DESIGN'"
	)).ambiguous)
	return 5;
    const brlcad::step::HeaderSchemaInfo companion =
	STEPHeaderSchema::inspect_header(header(
	    "'CONFIG_CONTROL_DESIGN','GEOMETRIC_VALIDATION_PROPERTIES_MIM',"
	    "'SHAPE_APPEARANCE_LAYER_MIM'"));
    if (!companion.recognized || companion.ambiguous ||
	companion.schema != HeaderSchema::AP203e2 ||
	!companion.legacy_identifier ||
	!companion.unrecognized_identifiers.empty())
	return 10;
    const brlcad::step::HeaderSchemaInfo plural_companion =
	STEPHeaderSchema::inspect_header(header(
	    "'shape_appearance_layers_mim','config_control_design'"));
    if (!plural_companion.recognized ||
	plural_companion.schema != HeaderSchema::AP203e2 ||
	!plural_companion.legacy_identifier)
	return 11;
    const brlcad::step::HeaderSchemaInfo standard_gvp =
	STEPHeaderSchema::inspect_header(header(
	    "'CONFIG_CONTROL_DESIGN',"
	    "'GEOMETRIC_VALIDATION_PROPERTY_REPRESENTATION_MIM'"));
    if (!standard_gvp.recognized ||
	standard_gvp.schema != HeaderSchema::AP203e2 ||
	!standard_gvp.legacy_identifier)
	return 12;
    const brlcad::step::HeaderSchemaInfo legacy_name =
	STEPHeaderSchema::inspect_header(header("'CCD_CLA_GVP_AST_ASD'"));
    if (!legacy_name.recognized || legacy_name.schema != HeaderSchema::AP203e2 ||
	!legacy_name.legacy_identifier)
	return 13;
    const brlcad::step::HeaderSchemaInfo legacy_short_name =
	STEPHeaderSchema::inspect_header(header("'CCD_CLA_GVP_AST'"));
    if (!legacy_short_name.recognized ||
	legacy_short_name.schema != HeaderSchema::AP203e2 ||
	!legacy_short_name.legacy_identifier)
	return 14;
    const brlcad::step::HeaderSchemaInfo interim_name =
	STEPHeaderSchema::inspect_header(header(
	    "'CONFIGURATION_CONTROL_3D_DESIGN_MIM_LF'"));
    if (!interim_name.recognized || interim_name.schema != HeaderSchema::AP203e2 ||
	!interim_name.legacy_identifier)
	return 15;
    const brlcad::step::HeaderSchemaInfo unknown_companion =
	STEPHeaderSchema::inspect_header(header(
	    "'CONFIG_CONTROL_DESIGN','ALIBRE_SCHEMA'"));
    if (!unknown_companion.recognized ||
	unknown_companion.schema != HeaderSchema::AP203 ||
	unknown_companion.unrecognized_identifiers.size() != 1)
	return 16;
    if (STEPHeaderSchema::inspect_header(header(
	"'SHAPE_APPEARANCE_LAYER_MIM'"
	)).recognized)
	return 17;
    if (STEPHeaderSchema::inspect_header(header("'../../step-schema-ap203'")).recognized)
	return 6;
    if (STEPHeaderSchema::inspect_header(
	"ISO-10303-21; HEADER; /* FILE_SCHEMA(('AUTOMOTIVE_DESIGN')); */ "
	"FILE_SCHEMA(('CONFIG_CONTROL_DESIGN')); ENDSEC; DATA;"
	).schema != HeaderSchema::AP203)
	return 7;
    const std::string quoted = "ISO-10303-21; HEADER; "
	"FILE_DESCRIPTION(('ENDSEC; FILE_SCHEMA((''AUTOMOTIVE_DESIGN''));'),'2;1'); "
	"FILE_SCHEMA(('CONFIG_CONTROL_DESIGN')); ENDSEC; DATA;";
    if (STEPHeaderSchema::inspect_header(quoted).schema != HeaderSchema::AP203)
	return 19;
    if (STEPHeaderSchema::inspect_header(
	"ISO-10303-21; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'); ENDSEC; DATA;"
	).recognized)
	return 20;
    if (STEPHeaderSchema::inspect_header("unrelated; HEADER; FILE_SCHEMA(('CONFIG_CONTROL_DESIGN')); ENDSEC;").recognized)
	return 21;
    const auto qualified_companion = STEPHeaderSchema::inspect_header(header(
	"'CONFIG_CONTROL_DESIGN','SHAPE_APPEARANCE_LAYER_MIM {1 2 3}'"));
    if (!qualified_companion.recognized || qualified_companion.schema != HeaderSchema::AP203e2 ||
	qualified_companion.edition != "2" || !qualified_companion.legacy_identifier ||
	!qualified_companion.unrecognized_identifiers.empty()) return 29;
    const char *oid_revisions[] = {"1", "3", "4", "5", "7"};
    for (size_t i = 0; i < sizeof(oid_revisions) / sizeof(oid_revisions[0]); ++i) {
	const std::string text = header("'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 " +
	    std::string(oid_revisions[i]) + " 1 4}'");
	const std::string edition = oid_revisions[i] == std::string("7") ? "4" : std::to_string(i + 1);
	const auto native = STEPHeaderSchema::inspect_header(text);
	if (!native.recognized || native.schema != HeaderSchema::AP242 || native.edition != edition) return 22;
	bu_mime_registry *registry = bu_mime_registry_create(0);
	bu_mime_result *observed = NULL;
	const bool match = registry && bu_mime_detect(registry, NULL, text.data(), text.size(), 1, &observed) == BU_MIME_MATCH &&
	    observed && observed->count && BU_STR_EQUAL(observed->candidates[0].name, "model/step") &&
	    BU_STR_EQUAL(bu_mime_traits_get(&observed->candidates[0].traits, "step.edition"), edition.c_str());
	bu_mime_result_free(observed);
	bu_mime_registry_destroy(registry);
	if (!match) return 23;
    }
    for (const std::string &identifier : {"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF",
	"AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 999 1 4}"}) {
	const auto info = STEPHeaderSchema::inspect_header(header("'" + identifier + "'"));
	if (!info.recognized || !info.edition.empty()) return 24;
    }
    const auto mixed = STEPHeaderSchema::inspect_header(header(
	"'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 1 1 4}',"
	"'AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF {1 0 10303 442 5 1 4}'"));
    if (mixed.recognized || !mixed.ambiguous) return 25;
    const auto literal = STEPHeaderSchema::inspect_header(header("'CONFIG_CONTROL_DESIGN','O''''Brien'"));
    if (!literal.recognized || literal.unrecognized_identifiers.size() != 1 ||
	literal.unrecognized_identifiers.front() != "O''Brien") return 26;
    char filename[MAXPATHLEN];
    FILE *file = bu_temp_file(filename, sizeof(filename));
    const std::string large = "ISO-10303-21; HEADER; /*" + std::string(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'p') +
	"*/ FILE_DESCRIPTION(('ENDSEC;'),'2;1'); FILE_SCHEMA(('CONFIG_CONTROL_DESIGN')); ENDSEC; DATA;";
    if (!file || fwrite(large.data(), 1, large.size(), file) != large.size() || fclose(file)) return 27;
    const auto full = STEPHeaderSchema::inspect_file(filename);
    if (!bu_file_delete(filename) || !full.recognized || full.schema != HeaderSchema::AP203 || full.identifiers.size() != 1) return 28;
    bu_mime_trait full_edition = BU_MIME_TRAIT_STRING("step.edition", "1");
    bu_mime_traits full_request = {&full_edition, 1};
    std::string mime_error;
    if (!STEPHeaderSchema::matches_mime(full, "model/step", &full_request, mime_error)) return 30;
    const std::string metadata = "Widget O'Brien \\ \xE2\x98\x83";
    const std::string encoded = brlcad::step::encode_string(metadata);
    if (encoded != "'Widget O''Brien \\X2\\005C\\X0\\ \\X2\\2603\\X0\\'")
	return 8;
    if (brlcad::step::decode_string(encoded) != metadata)
	return 9;
    return 0;
}
