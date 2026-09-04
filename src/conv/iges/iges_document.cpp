/*               I G E S _ D O C U M E N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#include "iges_document.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace brlcad {
namespace iges {

class Parser {
public:
    static GlobalSection &global(Document &document) { return document.global_; }
    static std::vector<DirectoryEntry> &entities(Document &document) { return document.entities_; }
    static std::map<EntityId, size_t> &entity_index(Document &document) { return document.entity_index_; }
    static std::map<EntityId, ParameterList> &parameters(Document &document) { return document.parameters_; }
    static std::vector<Diagnostic> &diagnostics(Document &document) { return document.diagnostics_; }
    static void record_count(Document &document, size_t count) { document.record_count_ = count; }
};

namespace {

constexpr size_t RECORD_WIDTH = 80;
constexpr size_t DATA_WIDTH = 72;
constexpr size_t PARAMETER_DATA_WIDTH = 64;
constexpr size_t SECTION_COLUMN = 72;
constexpr size_t SEQUENCE_COLUMN = 73;
constexpr size_t DIRECTORY_FIELD_WIDTH = 8;

struct Record {
    std::string text;
    char section = '\0';
    int sequence = 0;
    size_t physical_record = 0;
};

struct ParseState {
    Document document;
    std::vector<Record> records;
};

std::string
trim(const std::string &value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
	return std::string();
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool
parse_integer(const std::string &input, int64_t &value)
{
    const std::string field = trim(input);
    if (field.empty())
	return false;

    const char *first = field.data();
    const char *last = first + field.size();
    const std::from_chars_result result = std::from_chars(first, last, value, 10);
    return result.ec == std::errc() && result.ptr == last;
}

void
add_diagnostic(Document &document, Severity severity, const char *code,
    const std::string &message, size_t record = 0, size_t column = 0,
    EntityId entity_id = EntityId(), int entity_type = 0)
{
    Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.entity_id = entity_id.value();
    diagnostic.entity_type = entity_type;
    diagnostic.location.record = record;
    diagnostic.location.column = column;
    Parser::diagnostics(document).push_back(std::move(diagnostic));
}

bool
has_record_trailer(const std::string &line)
{
    if (line.size() < 8)
	return false;
    const char section = line[line.size() - 8];
    if (section != 'S' && section != 'G' && section != 'D' &&
	section != 'P' && section != 'T')
	return false;
    const std::string sequence = trim(line.substr(line.size() - 7));
    return !sequence.empty() &&
	std::all_of(sequence.begin(), sequence.end(), [](unsigned char character) {
	    return character >= '0' && character <= '9';
	});
}

std::string
normalize_record(std::string line, size_t physical_record, Document &document)
{
    if (!line.empty() && line.back() == '\r')
	line.pop_back();

    if (line.size() != RECORD_WIDTH && has_record_trailer(line)) {
	const std::string trailer = line.substr(line.size() - 8);
	line.resize(line.size() - 8);
	if (line.size() > DATA_WIDTH) {
	    const std::string excess = line.substr(DATA_WIDTH);
	    const Severity severity = trim(excess).empty() ?
		Severity::Warning : Severity::Error;
	    add_diagnostic(document, severity, "record_data_too_long",
		trim(excess).empty() ?
		"ignored blank columns before the section trailer" :
		"record data exceeds 72 columns", physical_record, DATA_WIDTH + 1);
	    line.resize(DATA_WIDTH);
	}
	line.resize(DATA_WIDTH, ' ');
	line += trailer;
    } else {
	if (line.size() > RECORD_WIDTH) {
	    add_diagnostic(document, Severity::Error, "record_too_long",
		"physical record exceeds 80 columns", physical_record,
		RECORD_WIDTH + 1);
	    line.resize(RECORD_WIDTH);
	}
	line.resize(RECORD_WIDTH, ' ');
    }

    return line;
}

std::vector<std::string>
physical_records(const std::string &contents)
{
    std::vector<std::string> lines;
    if (contents.find('\n') == std::string::npos) {
	for (size_t offset = 0; offset < contents.size(); offset += RECORD_WIDTH)
	    lines.push_back(contents.substr(offset, RECORD_WIDTH));
	return lines;
    }

    size_t start = 0;
    while (start < contents.size()) {
	const size_t end = contents.find('\n', start);
	if (end == std::string::npos) {
	    lines.push_back(contents.substr(start));
	    break;
	}
	lines.push_back(contents.substr(start, end - start));
	start = end + 1;
    }
    return lines;
}

void
read_records(ParseState &state, const std::string &contents)
{
    const std::vector<std::string> lines = physical_records(contents);
    Parser::record_count(state.document, lines.size());
    state.records.reserve(lines.size());

    for (size_t index = 0; index < lines.size(); ++index) {
	Record record;
	record.physical_record = index + 1;
	record.text = normalize_record(lines[index], index + 1, state.document);
	record.section = record.text[SECTION_COLUMN];
	int64_t sequence = 0;
	if (!parse_integer(record.text.substr(SEQUENCE_COLUMN, 7), sequence) ||
		sequence <= 0 || sequence > std::numeric_limits<int>::max()) {
	    add_diagnostic(state.document, Severity::Error, "invalid_sequence",
		"record has an invalid section sequence number", index + 1,
		SEQUENCE_COLUMN + 1);
	} else {
	    record.sequence = static_cast<int>(sequence);
	}
	state.records.push_back(std::move(record));
    }
}

int
section_rank(char section)
{
    switch (section) {
	case 'S': return 0;
	case 'G': return 1;
	case 'D': return 2;
	case 'P': return 3;
	case 'T': return 4;
	default: return -1;
    }
}

void
validate_sections(ParseState &state)
{
    int previous_rank = -1;
    int previous_sequence[5] = {0, 0, 0, 0, 0};
    bool seen[5] = {false, false, false, false, false};

    for (const Record &record : state.records) {
	const int rank = section_rank(record.section);
	if (rank < 0) {
	    add_diagnostic(state.document, Severity::Error, "invalid_section",
		"record has an unknown section identifier", record.physical_record,
		SECTION_COLUMN + 1);
	    continue;
	}
	seen[rank] = true;
	if (rank < previous_rank) {
	    add_diagnostic(state.document, Severity::Error, "section_order",
		"IGES sections are out of order", record.physical_record,
		SECTION_COLUMN + 1);
	}
	previous_rank = std::max(previous_rank, rank);
	if (record.sequence != previous_sequence[rank] + 1) {
	    add_diagnostic(state.document, Severity::Error, "sequence_gap",
		"section sequence numbers are not contiguous", record.physical_record,
		SEQUENCE_COLUMN + 1);
	}
	previous_sequence[rank] = record.sequence;
    }

    for (int rank = 0; rank < 5; ++rank) {
	if (!seen[rank]) {
	    const char *names[] = {"Start", "Global", "Directory", "Parameter", "Terminate"};
	    add_diagnostic(state.document, Severity::Error, "missing_section",
		std::string("missing ") + names[rank] + " section");
	}
    }
}

bool
read_hollerith(const std::string &data, size_t &position, std::string &value)
{
    const size_t number_start = position;
    while (position < data.size() && data[position] >= '0' && data[position] <= '9')
	++position;
    if (position == number_start || position >= data.size() ||
	(data[position] != 'H' && data[position] != 'h')) {
	position = number_start;
	return false;
    }

    int64_t length = 0;
    if (!parse_integer(data.substr(number_start, position - number_start), length) ||
	length < 0 || static_cast<uint64_t>(length) > data.size() - position - 1) {
	position = number_start;
	return false;
    }

    ++position;
    value = data.substr(position, static_cast<size_t>(length));
    position += static_cast<size_t>(length);
    return true;
}

struct Tokenized {
    std::vector<Parameter> values;
    bool terminated = false;
};

bool
has_record_terminator(const std::string &data, char parameter_delimiter,
    char record_delimiter)
{
    size_t position = 0;
    while (position < data.size()) {
	while (position < data.size() &&
		(data[position] == ' ' || data[position] == '\t'))
	    ++position;
	const size_t token_start = position;
	while (position < data.size() && data[position] >= '0' &&
		data[position] <= '9')
	    ++position;
	if (position > token_start && position < data.size() &&
		(data[position] == 'H' || data[position] == 'h')) {
	    int64_t length = 0;
	    if (!parse_integer(data.substr(token_start, position - token_start), length) ||
		    length < 0)
		return false;
	    ++position;
	    if (static_cast<uint64_t>(length) > data.size() - position)
		return false;
	    position += static_cast<size_t>(length);
	} else {
	    position = token_start;
	    while (position < data.size() && data[position] != parameter_delimiter &&
		    data[position] != record_delimiter)
		++position;
	}
	while (position < data.size() &&
		(data[position] == ' ' || data[position] == '\t'))
	    ++position;
	if (position < data.size() && data[position] == record_delimiter)
	    return true;
	if (position >= data.size() || data[position] != parameter_delimiter)
	    return false;
	++position;
    }
    return false;
}

Tokenized
tokenize(const std::string &data, char parameter_delimiter, char record_delimiter,
    size_t first_record, size_t data_width, Document &document, EntityId owner)
{
    Tokenized result;
    size_t position = 0;
    while (position < data.size()) {
	Parameter parameter;
	parameter.location.record = first_record + position / data_width;
	parameter.location.column = position % data_width + 1;

	const size_t token_start = position;
	size_t probe = position;
	while (probe < data.size() && (data[probe] == ' ' || data[probe] == '\t'))
	    ++probe;
	std::string decoded;
	if (read_hollerith(data, probe, decoded)) {
	    parameter.raw = data.substr(position, probe - position);
	    position = probe;
	} else {
	    while (position < data.size() && data[position] != parameter_delimiter &&
		data[position] != record_delimiter)
		++position;
	    parameter.raw = data.substr(token_start, position - token_start);
	}
	result.values.push_back(std::move(parameter));

	while (position < data.size() && (data[position] == ' ' || data[position] == '\t'))
	    ++position;
	if (position >= data.size())
	    break;
	if (data[position] == record_delimiter) {
	    result.terminated = true;
	    ++position;
	    if (!trim(data.substr(position)).empty())
		add_diagnostic(document, Severity::Warning, "parameter_trailing_data",
		    "nonblank data follows the parameter record delimiter",
		    first_record + position / data_width, position % data_width + 1,
		    owner);
	    break;
	}
	if (data[position] != parameter_delimiter) {
	    add_diagnostic(document, Severity::Error, "parameter_separator",
		"parameter is not followed by the declared delimiter",
		first_record + position / data_width, position % data_width + 1,
		owner);
	    break;
	}
	++position;
	if (position < data.size() && data[position] == record_delimiter) {
	    Parameter omitted;
	    omitted.location.record = first_record + position / data_width;
	    omitted.location.column = position % data_width + 1;
	    result.values.push_back(std::move(omitted));
	}
    }
    return result;
}

std::string
section_data(const std::vector<Record> &records, char section, size_t width)
{
    std::string data;
    for (const Record &record : records) {
	if (record.section == section)
	    data.append(record.text, 0, width);
    }
    return data;
}

size_t
first_section_record(const std::vector<Record> &records, char section)
{
    for (const Record &record : records)
	if (record.section == section)
	    return record.physical_record;
    return 0;
}

void
parse_global(ParseState &state)
{
    const std::string data = section_data(state.records, 'G', DATA_WIDTH);
    if (data.empty())
	return;

    size_t position = 0;
    while (position < data.size() && data[position] == ' ')
	++position;
    std::string delimiter;
    if (read_hollerith(data, position, delimiter) && delimiter.size() == 1) {
	Parser::global(state.document).parameter_delimiter = delimiter[0];
	while (position < data.size() && data[position] == ' ')
	    ++position;
	if (position < data.size() && data[position] == delimiter[0])
	    ++position;
	else
	    add_diagnostic(state.document, Severity::Error, "global_delimiter",
		"global parameter delimiter declaration is malformed",
		first_section_record(state.records, 'G'), position + 1);

	size_t second = position;
	while (second < data.size() && data[second] == ' ')
	    ++second;
	std::string terminator;
	if (read_hollerith(data, second, terminator) && terminator.size() == 1)
	    Parser::global(state.document).record_delimiter = terminator[0];
	else
	    add_diagnostic(state.document, Severity::Error, "global_terminator",
		"global record delimiter declaration is malformed",
		first_section_record(state.records, 'G'), second + 1);
    }

    Tokenized parsed = tokenize(data, Parser::global(state.document).parameter_delimiter,
	Parser::global(state.document).record_delimiter,
	first_section_record(state.records, 'G'), DATA_WIDTH, state.document,
	EntityId());
    for (const Parameter &parameter : parsed.values) {
	std::string value;
	if (parameter.string(value))
	    Parser::global(state.document).parameters.push_back(value);
	else
	    Parser::global(state.document).parameters.push_back(trim(parameter.raw));
    }
    if (!parsed.terminated)
	add_diagnostic(state.document, Severity::Error, "global_unterminated",
	    "Global section has no declared record delimiter",
	    first_section_record(state.records, 'G'));
}

bool
directory_integer(const Record &record, size_t field, int &value,
    Document &document, EntityId entity_id)
{
    int64_t parsed = 0;
    const size_t offset = field * DIRECTORY_FIELD_WIDTH;
    const std::string raw = record.text.substr(offset, DIRECTORY_FIELD_WIDTH);
    if (trim(raw).empty()) {
	value = 0;
	return true;
    }
    if (!parse_integer(raw, parsed) || parsed < std::numeric_limits<int>::min() ||
	parsed > std::numeric_limits<int>::max()) {
	add_diagnostic(document, Severity::Error, "directory_integer",
	    "Directory field is not a valid integer", record.physical_record,
	    offset + 1, entity_id);
	return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

void
parse_directory(ParseState &state)
{
    std::vector<const Record *> directory_records;
    for (const Record &record : state.records)
	if (record.section == 'D')
	    directory_records.push_back(&record);

    if ((directory_records.size() & 1) != 0)
	add_diagnostic(state.document, Severity::Error, "directory_pair",
	    "Directory section contains an unmatched record",
	    directory_records.back()->physical_record);

    for (size_t index = 0; index + 1 < directory_records.size(); index += 2) {
	const Record &first = *directory_records[index];
	const Record &second = *directory_records[index + 1];
	const EntityId id(first.sequence);
	if (!id.valid() || second.sequence != first.sequence + 1) {
	    add_diagnostic(state.document, Severity::Error, "directory_sequence",
		"Directory entry must use consecutive odd/even sequence numbers",
		first.physical_record, SEQUENCE_COLUMN + 1, id);
	}

	DirectoryEntry entry;
	entry.id = id;
	entry.first_record = first.physical_record;
	directory_integer(first, 0, entry.type, state.document, id);
	directory_integer(first, 1, entry.parameter_record, state.document, id);
	directory_integer(first, 2, entry.structure, state.document, id);
	directory_integer(first, 3, entry.line_font, state.document, id);
	directory_integer(first, 4, entry.level, state.document, id);
	directory_integer(first, 5, entry.view, state.document, id);
	int transform = 0;
	directory_integer(first, 6, transform, state.document, id);
	entry.transform = EntityId(std::abs(static_cast<int64_t>(transform)));
	directory_integer(first, 7, entry.label_associativity, state.document, id);
	directory_integer(first, 8, entry.status, state.document, id);

	int repeated_type = 0;
	directory_integer(second, 0, repeated_type, state.document, id);
	if (repeated_type != entry.type)
	    add_diagnostic(state.document, Severity::Error, "directory_type_mismatch",
		"Directory entry type differs between its two records",
		second.physical_record, 1, id, entry.type);
	directory_integer(second, 1, entry.line_weight, state.document, id);
	directory_integer(second, 2, entry.color, state.document, id);
	directory_integer(second, 3, entry.parameter_line_count, state.document, id);
	directory_integer(second, 4, entry.form, state.document, id);
	entry.label = trim(second.text.substr(56, 8));
	directory_integer(second, 8, entry.subscript, state.document, id);

	if (entry.type <= 0)
	    add_diagnostic(state.document, Severity::Error, "directory_type",
		"Directory entry has no positive entity type", first.physical_record,
		1, id);
	if (entry.parameter_record <= 0)
	    add_diagnostic(state.document, Severity::Error, "parameter_reference",
		"Directory entry has an invalid first Parameter record",
		first.physical_record, DIRECTORY_FIELD_WIDTH + 1, id, entry.type);
	if (entry.parameter_line_count < 0)
	    add_diagnostic(state.document, Severity::Error, "parameter_count",
		"Directory entry has a negative Parameter record count",
		second.physical_record, 25, id, entry.type);

	if (Parser::entity_index(state.document).find(id) != Parser::entity_index(state.document).end()) {
	    add_diagnostic(state.document, Severity::Error, "duplicate_entity",
		"duplicate Directory entry sequence number", first.physical_record,
		SEQUENCE_COLUMN + 1, id, entry.type);
	    continue;
	}
	Parser::entity_index(state.document)[id] = Parser::entities(state.document).size();
	Parser::entities(state.document).push_back(std::move(entry));
    }
}

void
parse_parameters(ParseState &state)
{
    std::map<int, const Record *> by_sequence;
    for (const Record &record : state.records) {
	if (record.section != 'P')
	    continue;
	if (!by_sequence.emplace(record.sequence, &record).second)
	    add_diagnostic(state.document, Severity::Error, "duplicate_parameter_record",
		"duplicate Parameter section sequence number", record.physical_record,
		SEQUENCE_COLUMN + 1);
    }

    /* IGES 4-era writers sometimes left the DE parameter-line-count field
     * zero.  The P record delimiter is still authoritative, so infer the
     * bounded range once and retain a warning describing the repair. */
    for (DirectoryEntry &entry : Parser::entities(state.document)) {
	if (entry.parameter_line_count != 0 || entry.parameter_record <= 0)
	    continue;
	std::string data;
	int sequence = entry.parameter_record;
	while (true) {
	    const auto found = by_sequence.find(sequence);
	    if (found == by_sequence.end())
		break;
	    data.append(found->second->text, 0, PARAMETER_DATA_WIDTH);
	    ++entry.parameter_line_count;
	    if (has_record_terminator(data,
		    Parser::global(state.document).parameter_delimiter,
		    Parser::global(state.document).record_delimiter))
		break;
	    ++sequence;
	}
	if (entry.parameter_line_count > 0) {
	    add_diagnostic(state.document, Severity::Warning,
		"inferred_parameter_count",
		"inferred an omitted Parameter record count from the record delimiter",
		entry.first_record + 1, 25, entry.id, entry.type);
	} else {
	    add_diagnostic(state.document, Severity::Error,
		"missing_parameter_record",
		"Directory entry references a missing Parameter record",
		entry.first_record, DIRECTORY_FIELD_WIDTH + 1, entry.id, entry.type);
	}
    }

    std::map<int, EntityId> claimed;
    for (const DirectoryEntry &entry : Parser::entities(state.document)) {
	for (int offset = 0; offset < entry.parameter_line_count; ++offset) {
	    const int sequence = entry.parameter_record + offset;
	    const auto inserted = claimed.emplace(sequence, entry.id);
	    if (!inserted.second && !(inserted.first->second == entry.id))
		add_diagnostic(state.document, Severity::Error,
		    "overlapping_parameter_range",
		    "multiple Directory entries claim the same Parameter record",
		    entry.first_record, DIRECTORY_FIELD_WIDTH + 1, entry.id,
		    entry.type);
	}
    }

    for (const DirectoryEntry &entry : Parser::entities(state.document)) {
	std::string data;
	size_t first_record = 0;
	bool complete = true;
	for (int offset = 0; offset < entry.parameter_line_count; ++offset) {
	    const auto found = by_sequence.find(entry.parameter_record + offset);
	    if (found == by_sequence.end()) {
		add_diagnostic(state.document, Severity::Error, "missing_parameter_record",
		    "Directory entry references a missing Parameter record",
		    entry.first_record, DIRECTORY_FIELD_WIDTH + 1, entry.id, entry.type);
		complete = false;
		break;
	    }
	    const Record &record = *found->second;
	    if (!first_record)
		first_record = record.physical_record;
	    int64_t owner = 0;
	    if (!parse_integer(record.text.substr(64, 8), owner)) {
		add_diagnostic(state.document, Severity::Error, "parameter_owner",
		    "Parameter record has an invalid Directory-entry owner",
		    record.physical_record, 65, entry.id, entry.type);
		complete = false;
	    } else if (owner != entry.id.value()) {
		/* Several otherwise usable early IGES writers emitted a stale P
		 * owner while retaining an unambiguous DE parameter pointer. */
		add_diagnostic(state.document, Severity::Warning,
		    "parameter_owner_repaired",
		    "used the unambiguous Directory parameter range instead of a mismatched Parameter owner",
		    record.physical_record, 65, entry.id, entry.type);
	    }
	    data.append(record.text, 0, PARAMETER_DATA_WIDTH);
	}
	if (!complete || data.empty())
	    continue;

	Tokenized parsed = tokenize(data, Parser::global(state.document).parameter_delimiter,
	    Parser::global(state.document).record_delimiter, first_record,
	    PARAMETER_DATA_WIDTH, state.document, entry.id);
	ParameterList list;
	list.owner = entry.id;
	list.values = std::move(parsed.values);
	list.terminated = parsed.terminated;
	if (!list.terminated)
	    add_diagnostic(state.document, Severity::Error, "parameter_unterminated",
		"entity parameters have no declared record delimiter", first_record,
		1, entry.id, entry.type);
	if (!list.values.empty()) {
	    int64_t parameter_type = 0;
	    if (!list.values.front().integer(parameter_type) || parameter_type != entry.type)
		add_diagnostic(state.document, Severity::Error, "parameter_type_mismatch",
		    "Parameter entity type does not match its Directory entry",
		    list.values.front().location.record,
		    list.values.front().location.column, entry.id, entry.type);
	}
	Parser::parameters(state.document).emplace(entry.id, std::move(list));
    }
}

void
validate_references(ParseState &state)
{
    for (const DirectoryEntry &entry : Parser::entities(state.document)) {
	if (!entry.transform.empty()) {
	    const DirectoryEntry *transform = state.document.entity(entry.transform);
	    if (!transform) {
		add_diagnostic(state.document, Severity::Error, "missing_transform",
		    "Directory entry references a missing transformation",
		    entry.first_record, 49, entry.id, entry.type);
	    } else if (transform->type != 124 && transform->type != 700) {
		add_diagnostic(state.document, Severity::Error, "invalid_transform",
		    "Directory transformation reference does not identify a matrix",
		    entry.first_record, 49, entry.id, entry.type);
	    }
	}
    }
}

} /* namespace */

bool
Parameter::empty() const
{
    return trim(raw).empty();
}

bool
Parameter::integer(int64_t &value) const
{
    return parse_integer(raw, value);
}

bool
Parameter::real(double &value) const
{
    std::string field = trim(raw);
    if (field.empty())
	return false;
    std::replace(field.begin(), field.end(), 'D', 'E');
    std::replace(field.begin(), field.end(), 'd', 'e');
    char *end = nullptr;
    errno = 0;
    value = std::strtod(field.c_str(), &end);
    return errno != ERANGE && end == field.c_str() + field.size() &&
	std::isfinite(value);
}

bool
Parameter::string(std::string &value) const
{
    size_t position = 0;
    while (position < raw.size() && (raw[position] == ' ' || raw[position] == '\t'))
	++position;
    if (read_hollerith(raw, position, value))
	return trim(raw.substr(position)).empty();
    value = trim(raw);
    return true;
}

bool
Parameter::entity(EntityId &value) const
{
    int64_t parsed = 0;
    if (!integer(parsed))
	return false;
    parsed = std::abs(parsed);
    EntityId candidate(parsed);
    if (!candidate.empty() && !candidate.valid())
	return false;
    value = candidate;
    return true;
}

Document
Document::parse_file(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
	Document result;
	result.source_name_ = path;
	add_diagnostic(result, Severity::Fatal, "input_open",
	    "unable to open IGES input file");
	return result;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
	Document result;
	result.source_name_ = path;
	add_diagnostic(result, Severity::Fatal, "input_read",
	    "unable to read complete IGES input file");
	return result;
    }
    return parse_buffer(contents.str(), path);
}

Document
Document::parse_buffer(const std::string &contents, const std::string &source_name)
{
    ParseState state;
    state.document.source_name_ = source_name;
    if (contents.empty()) {
	add_diagnostic(state.document, Severity::Fatal, "empty_input",
	    "IGES input is empty");
	return std::move(state.document);
    }

    read_records(state, contents);
    validate_sections(state);
    parse_global(state);
    parse_directory(state);
    parse_parameters(state);
    validate_references(state);
    return std::move(state.document);
}

bool
Document::valid() const
{
    return std::none_of(diagnostics_.begin(), diagnostics_.end(),
	[](const Diagnostic &diagnostic) {
	    return diagnostic.severity == Severity::Error ||
		diagnostic.severity == Severity::Fatal;
	});
}

const DirectoryEntry *
Document::entity(EntityId id) const
{
    const auto found = entity_index_.find(id);
    return found == entity_index_.end() ? nullptr : &entities_[found->second];
}

const ParameterList *
Document::parameters(EntityId id) const
{
    const auto found = parameters_.find(id);
    return found == parameters_.end() ? nullptr : &found->second;
}

std::vector<const DirectoryEntry *>
Document::find(int entity_type) const
{
    std::vector<const DirectoryEntry *> result;
    for (const DirectoryEntry &entry : entities_)
	if (entry.type == entity_type)
	    result.push_back(&entry);
    return result;
}

} /* namespace iges */
} /* namespace brlcad */

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
