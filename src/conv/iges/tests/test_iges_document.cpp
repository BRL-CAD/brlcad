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

bool
test_field_boundaries()
{
    using namespace brlcad::iges;
    constexpr size_t GLOBAL_WIDTH = 72;
    constexpr size_t PARAMETER_WIDTH = 64;
    enum class Kind { Real, Integer, String, Empty, InvalidReal, InvalidInteger };
    struct Case { std::string field; Kind kind; double number; std::string text; };
    const std::vector<Case> cases = {
	{"120", Kind::Integer, 120, {}}, {"-120", Kind::Integer, -120, {}},
	{"+120", Kind::Integer, 120, {}}, {" 120 ", Kind::Integer, 120, {}},
	{"-1.25", Kind::Real, -1.25, {}}, {"+1.2D+02", Kind::Real, 120, {}},
	{"1.2d+02", Kind::Real, 120, {}}, {"", Kind::Empty, 0, {}},
	{"   ", Kind::Empty, 0, {}}, {"5Hhello", Kind::String, 0, "hello"},
	{"8Ha,^;!xyz", Kind::String, 0, "a,^;!xyz"},
	{"15H20260905.000000", Kind::String, 0, "20260905.000000"},
	{"13H260905.000000", Kind::String, 0, "260905.000000"},
	{"1junk", Kind::InvalidReal, 0, {}}, {"NaN", Kind::InvalidReal, 0, {}},
	{"Inf", Kind::InvalidReal, 0, {}}, {"1e9999", Kind::InvalidReal, 0, {}},
	{"1e-9999", Kind::InvalidReal, 0, {}}, {"1junk", Kind::InvalidInteger, 0, {}},
	{std::string(1024, '1'), Kind::InvalidInteger, 0, {}}
    };
    for (bool custom : {false, true}) {
	const char separator = custom ? '^' : ',';
	const char terminator = custom ? '!' : ';';
	const std::string delimiters = std::string("1H") + separator + separator + "1H" + terminator;
	for (bool global : {false, true}) {
	    const size_t width = global ? GLOBAL_WIDTH : PARAMETER_WIDTH;
	    for (size_t offset = 0; offset <= width; ++offset) {
		for (const auto &test : cases) {
		    const std::string values = std::string(offset, ' ') + test.field + separator + '7' + terminator;
		    const std::string global_data = delimiters + (global ? separator + values : std::string(1, terminator));
		    const std::string parameter_data = "110" + std::string(1, separator) +
			(global ? std::string("0") + terminator : values);
		    const size_t parameter_lines = (parameter_data.size() + PARAMETER_WIDTH - 1) / PARAMETER_WIDTH;
		    std::string input = record("field boundary test", 'S', 1) + '\n';
		    for (size_t start = 0; start < global_data.size(); start += GLOBAL_WIDTH)
			input += record(global_data.substr(start, GLOBAL_WIDTH), 'G', start / GLOBAL_WIDTH + 1) + '\n';
		    input += directory_first(110, 1, 1) + '\n' + directory_second(110, parameter_lines, 2) + '\n';
		    for (size_t start = 0; start < parameter_data.size(); start += PARAMETER_WIDTH)
			input += parameter_record(parameter_data.substr(start, PARAMETER_WIDTH), 1, start / PARAMETER_WIDTH + 1) + '\n';
		    input += record("", 'T', 1) + '\n';
		    const auto document = Document::parse_buffer(input);
		    if (!expect(document.valid(), "field boundary fixture failed to parse"))
			return false;
		    Parameter value, following;
		    if (global) {
			if (!expect(document.global().parameters.size() == 4, "global fields were lost"))
			    return false;
			value.raw = document.global().parameters[2];
			following.raw = document.global().parameters[3];
		    } else {
			const auto *parameters = document.parameters(EntityId(1));
			if (!expect(parameters && parameters->values.size() == 3, "parameter fields were lost"))
			    return false;
			value = parameters->values[1];
			following = parameters->values[2];
		    }
		    double number = 0.0;
		    int64_t integer = 0;
		    std::string text;
		    bool correct = false;
		    switch (test.kind) {
			case Kind::Real: correct = value.real(number) && NEAR_EQUAL(number, test.number, SMALL_FASTF); break;
			case Kind::Integer: correct = value.integer(integer) && integer == static_cast<int64_t>(test.number); break;
			case Kind::String: correct = value.string(text) && text == test.text; break;
			case Kind::Empty: correct = value.empty(); break;
			case Kind::InvalidReal: correct = !value.real(number); break;
			case Kind::InvalidInteger: correct = !value.integer(integer); break;
		    }
		    if (!expect(correct, "field value changed at a physical record boundary") ||
			!expect(following.integer(integer) && integer == 7, "following field was consumed"))
			return false;
		}
	    }
	}
    }
    return true;
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
    passed = test_field_boundaries() && passed;
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
