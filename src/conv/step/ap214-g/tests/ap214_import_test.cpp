/*               A P 2 1 4 _ I M P O R T _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP214Adapter.h"
#include "STEPString.h"

#include <iostream>
#include <set>
#include <string>

namespace {

int failures = 0;

void
expect(bool condition, const char *message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

std::string
part21_header(const std::string &schema)
{
    return "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('fixture'),'2;1');\n"
	"FILE_NAME('fixture.stp','2026-01-01T00:00:00',(''),(''),'','','');\n"
	"FILE_SCHEMA((" + schema + "));\nENDSEC;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n";
}

} // namespace

int
main()
{
    using brlcad::step::AP214Adapter;
    using brlcad::step::AP214SchemaInfo;

    AP214SchemaInfo qualified = AP214Adapter::inspect_header(
	part21_header("'AUTOMOTIVE_DESIGN { 1 0 10303 214 3 1 1 }'"));
    expect(qualified.accepted, "AP214 edition-qualified AUTOMOTIVE_DESIGN is accepted");
    expect(!qualified.legacy_cc2, "edition-qualified schema is not marked legacy");

    AP214SchemaInfo legacy = AP214Adapter::inspect_header(part21_header("'AUTOMOTIVE_DESIGN_CC2'"));
    expect(legacy.accepted, "legacy AUTOMOTIVE_DESIGN_CC2 is accepted");
    expect(legacy.legacy_cc2, "legacy AUTOMOTIVE_DESIGN_CC2 is reported");

    AP214SchemaInfo ap203 = AP214Adapter::inspect_header(part21_header("'CONFIG_CONTROL_DESIGN'"));
    expect(!ap203.accepted, "AP203 schema is rejected");
    expect(ap203.suggested_converter == "step-g", "AP203 rejection suggests step-g");

    AP214SchemaInfo comments = AP214Adapter::inspect_header(
	"ISO-10303-21; HEADER; /* FILE_SCHEMA(('BAD')); */ FILE_SCHEMA(("
	"'AUXILIARY_SCHEMA', 'AUTOMOTIVE_DESIGN')); ENDSEC;");
    expect(comments.accepted, "comments do not interfere with schema inspection");
    expect(comments.identifiers.size() == 2, "all FILE_SCHEMA identifiers are retained");

    AP214SchemaInfo malformed = AP214Adapter::inspect_header("ISO-10303-21; HEADER; ENDSEC;");
    expect(!malformed.accepted && !malformed.error.empty(), "missing FILE_SCHEMA is diagnosed");

    expect(brlcad::step::decode_string("'Dante''s'" ) == "Dante's",
	"doubled apostrophes are decoded");
    expect(brlcad::step::decode_string("'\\X2\\03A900E9\\X0\\'") == std::string("\xCE\xA9\xC3\xA9"),
	"X2 Unicode escapes are decoded as UTF-8");
    expect(brlcad::step::decode_string("'\\X4\\0001F680\\X0\\'") == std::string("\xF0\x9F\x9A\x80"),
	"X4 Unicode escapes are decoded as UTF-8");
    expect(brlcad::step::sanitize_name("'Crème brûlée / Ω'") == "Creme_brulee_u3A9",
	"decoded names are stable and BRL-CAD-safe");
    expect(brlcad::step::json_escape("a\n\"b") == "a\\n\\\"b", "JSON strings are escaped");

    std::set<int64_t> entity_ids;
    std::string entity_error;
    expect(brlcad::step::parse_entity_id_list("#200, 207", entity_ids, &entity_error) &&
	entity_ids.size() == 2 && entity_ids.count(200) && entity_ids.count(207),
	"comma-separated entity selections accept optional hash prefixes");
    expect(brlcad::step::parse_entity_id_list("207,263", entity_ids, &entity_error) &&
	entity_ids.size() == 3 && entity_ids.count(263),
	"repeated entity selections accumulate and deduplicate identifiers");
    const std::set<int64_t> before_invalid = entity_ids;
    expect(!brlcad::step::parse_entity_id_list("300,bad", entity_ids, &entity_error) &&
	entity_ids == before_invalid && !entity_error.empty(),
	"an invalid entity list is rejected without partially changing the selection");

    if (failures)
	std::cerr << failures << " AP214 importer unit test(s) failed\n";
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 */
