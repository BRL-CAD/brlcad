/*             T E S T _ I G E S _ D O C U M E N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "vmath.h"

#include "../iges_document.h"

#include "bu/app.h"

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

std::string
field(int64_t value)
{
    std::ostringstream output;
    output << std::setw(8) << value;
    return output.str();
}

std::string
record(const std::string &data, char section, int sequence)
{
    std::ostringstream trailer;
    trailer << section << std::setw(7) << sequence;
    std::string result = data.substr(0, 72);
    result.resize(72, ' ');
    result += trailer.str();
    return result;
}

std::string
directory_first(int type, int parameter, int sequence, int transform = 0)
{
    std::string data;
    data += field(type);
    data += field(parameter);
    data += field(0);
    data += field(0);
    data += field(0);
    data += field(0);
    data += field(transform);
    data += field(0);
    data += field(0);
    return record(data, 'D', sequence);
}

std::string
directory_second(int type, int parameter_lines, int sequence,
    const std::string &label = "ENTITY")
{
    std::string data;
    data += field(type);
    data += field(0);
    data += field(0);
    data += field(parameter_lines);
    data += field(0);
    data += field(0);
    data += field(0);
    std::ostringstream label_field;
    label_field << std::left << std::setw(8) << label.substr(0, 8);
    data += label_field.str();
    data += field(0);
    return record(data, 'D', sequence);
}

std::string
parameter_record(const std::string &data, int owner, int sequence)
{
    std::string result = data.substr(0, 64);
    result.resize(64, ' ');
    result += field(owner);
    std::ostringstream trailer;
    trailer << 'P' << std::setw(7) << sequence;
    result += trailer.str();
    return result;
}

std::string
sample(char parameter_delimiter = ',', char record_delimiter = ';',
    int parameter_owner = 1)
{
    std::string global = "1H";
    global.push_back(parameter_delimiter);
    global.push_back(parameter_delimiter);
    global += "1H";
    global.push_back(record_delimiter);
    global.push_back(record_delimiter);

    std::string parameters = "110";
    parameters.push_back(parameter_delimiter);
    parameters += "1.25D+2";
    parameters.push_back(parameter_delimiter);
    parameters += "8Habc";
    parameters.push_back(parameter_delimiter);
    parameters.push_back(record_delimiter);
    parameters += "xyz";
    parameters.push_back(record_delimiter);

    std::string result;
    result += record("parser test", 'S', 1) + "\n";
    result += record(global, 'G', 1) + "\n";
    result += directory_first(110, 1, 1) + "\n";
    result += directory_second(110, 1, 2) + "\n";
    result += parameter_record(parameters, parameter_owner, 1) + "\n";
    result += record("", 'T', 1) + "\n";
    return result;
}

bool
expect(bool condition, const char *message)
{
    if (!condition)
	std::fprintf(stderr, "%s\n", message);
    return condition;
}

bool
has_code(const brlcad::iges::Document &document, const std::string &code)
{
    for (const brlcad::iges::Diagnostic &diagnostic : document.diagnostics())
	if (diagnostic.code == code)
	    return true;
    return false;
}

bool
test_valid_document()
{
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample(), "memory.iges");
    if (!expect(document.valid(), "valid document was rejected") ||
	!expect(document.entities().size() == 1, "entity count is wrong") ||
	!expect(document.record_count() == 6, "record count is wrong"))
	return false;

    const brlcad::iges::EntityId id(1);
    const brlcad::iges::DirectoryEntry *entry = document.entity(id);
    const brlcad::iges::ParameterList *parameters = document.parameters(id);
    if (!expect(entry && entry->type == 110, "line directory entry was not parsed") ||
	!expect(parameters && parameters->values.size() == 3,
	    "line parameters were not tokenized"))
	return false;

    double coordinate = 0.0;
    std::string text;
    return expect(parameters->values[1].real(coordinate) && EQUAL(coordinate, 125.0),
	"D exponent was not parsed") &&
	expect(parameters->values[2].string(text) && text == "abc,;xyz",
	    "Hollerith string containing delimiters was not parsed");
}

bool
test_custom_delimiters()
{
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample('^', '!'));
    return expect(document.valid(), "custom delimiters were rejected") &&
	expect(document.global().parameter_delimiter == '^',
	    "custom parameter delimiter was not retained") &&
	expect(document.global().record_delimiter == '!',
	    "custom record delimiter was not retained");
}

bool
test_fixed_records()
{
    std::string input = sample();
    std::string fixed;
    for (char character : input)
	if (character != '\n')
	    fixed.push_back(character);
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(fixed);
    return expect(document.valid(), "fixed-width stream was rejected") &&
	expect(document.record_count() == 6, "fixed-width stream record count is wrong");
}

bool
test_repaired_owner()
{
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(sample(',', ';', 3));
    return expect(document.valid(), "unambiguous parameter owner was not repaired") &&
	expect(has_code(document, "parameter_owner_repaired"),
	    "parameter owner repair diagnostic is missing");
}

bool
test_inferred_parameter_count()
{
    std::string input = sample();
    const size_t second_directory = input.find("     110       0       0       1");
    if (second_directory == std::string::npos)
	return expect(false, "test setup could not locate Parameter count");
    input.replace(second_directory + 24, 8, field(0));
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(input);
    return expect(document.valid(), "omitted Parameter count was not inferred") &&
	expect(has_code(document, "inferred_parameter_count"),
	    "inferred Parameter count diagnostic is missing");
}

bool
test_blank_overrun()
{
    std::string input = sample();
    const size_t first_newline = input.find('\n');
    if (first_newline == std::string::npos)
	return expect(false, "test setup could not locate first record");
    input.insert(first_newline - 8, 1, ' ');
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(input);
    return expect(document.valid(), "blank record overrun was not repaired") &&
	expect(has_code(document, "record_data_too_long"),
	    "blank record repair diagnostic is missing");
}

bool
test_missing_section()
{
    std::string input = sample();
    const size_t parameter = input.find("       1P");
    if (parameter == std::string::npos)
	return expect(false, "test setup could not locate Parameter record");
    const size_t line_start = input.rfind('\n', parameter);
    const size_t line_end = input.find('\n', parameter);
    input.erase(line_start + 1, line_end - line_start);
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_buffer(input);
    return expect(!document.valid(), "missing Parameter section was accepted") &&
	expect(has_code(document, "missing_section"),
	    "missing section diagnostic is absent");
}

} /* namespace */

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 1) {
	bool passed = true;
	for (int i = 1; i < argc; ++i) {
	    const brlcad::iges::Document document =
		brlcad::iges::Document::parse_file(argv[i]);
	    for (const brlcad::iges::Diagnostic &diagnostic : document.diagnostics())
		std::fprintf(stderr, "%s:%zu:%zu: %s: %s\n", argv[i],
		    diagnostic.location.record, diagnostic.location.column,
		    diagnostic.code.c_str(), diagnostic.message.c_str());
	    passed = document.valid() && passed;
	}
	return passed ? 0 : 1;
    }

    bool passed = true;
    passed = test_valid_document() && passed;
    passed = test_custom_delimiters() && passed;
    passed = test_fixed_records() && passed;
    passed = test_repaired_owner() && passed;
    passed = test_inferred_parameter_count() && passed;
    passed = test_blank_overrun() && passed;
    passed = test_missing_section() && passed;
    return passed ? 0 : 1;
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
