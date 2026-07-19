/*                     A P 2 1 4 - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "AP214Adapter.h"
#include "BRLCADWrapper.h"
#include "STEPString.h"
#include "STEPWrapper.h"
#include "STEPProgressReporter.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "bu/process.h"
#include "bu/time.h"
#include "bu/vls.h"

#include <cerrno>
#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>

#ifndef _WIN32
#  include <sys/resource.h>
#endif

namespace {

volatile std::sig_atomic_t caught_signal = 0;

void
record_signal(int signal_number)
{
    caught_signal = signal_number;
}

int
signal_exit_status()
{
    return caught_signal ? 128 + static_cast<int>(caught_signal) : 0;
}

struct OutputFile {
    char *filename = NULL;
    bool overwrite = false;
};

class TemporaryPath {
public:
    TemporaryPath() : active(false) {}
    explicit TemporaryPath(const std::string &p) : path(p), active(true) {}
    ~TemporaryPath() {
	if (active && !path.empty()) bu_file_delete(path.c_str());
    }
    void reset(const std::string &p) {
	if (active && !path.empty()) bu_file_delete(path.c_str());
	path = p;
	active = true;
    }
    void release() { active = false; }
    const std::string &name() const { return path; }

private:
    std::string path;
    bool active;
};

class BoundedConversionLog : public std::streambuf {
public:
    BoundedConversionLog(std::streambuf *destination, bool passthrough)
	: output(destination), verbose(passthrough), line_count(0), overflow_lines(0)
    {
    }

    void finish()
    {
	std::lock_guard<std::mutex> guard(lock);
	if (!verbose && !line.empty()) record_line();
    }

    void report() const
    {
	std::lock_guard<std::mutex> guard(lock);
	if (verbose || line_count == 0 || !output) return;
	std::ostringstream message;
	message << "suppressed " << line_count << " converter detail line(s) in "
	    << unique_lines.size() << " bounded group(s)";
	if (overflow_lines) message << " (" << overflow_lines << " beyond the group limit)";
	message << "; use -v for details\n";
	const std::string text = message.str();
	output->sputn(text.data(), static_cast<std::streamsize>(text.size()));
    }

protected:
    int_type overflow(int_type character) override
    {
	std::lock_guard<std::mutex> guard(lock);
	if (traits_type::eq_int_type(character, traits_type::eof())) return traits_type::not_eof(character);
	record_char(traits_type::to_char_type(character));
	return character;
    }

    std::streamsize xsputn(const char *text, std::streamsize count) override
    {
	std::lock_guard<std::mutex> guard(lock);
	if (verbose) return output ? output->sputn(text, count) : count;
	for (std::streamsize i = 0; i < count; ++i) record_char(text[i]);
	return count;
    }

    int sync() override
    {
	std::lock_guard<std::mutex> guard(lock);
	return verbose && output ? output->pubsync() : 0;
    }

private:
    void record_char(char c)
    {
	if (verbose) {
	    if (output) output->sputc(c);
	    return;
	}
	if (c == '\n')
	    record_line();
	else if (line.size() < 4096)
	    line.push_back(c);
    }

    void record_line()
    {
	++line_count;
	if (unique_lines.size() < 128)
	    unique_lines.insert(line);
	else if (unique_lines.find(line) == unique_lines.end())
	    ++overflow_lines;
	line.clear();
    }

    std::streambuf *output;
    bool verbose;
    uint64_t line_count;
    uint64_t overflow_lines;
    std::string line;
    std::set<std::string> unique_lines;
    mutable std::mutex lock;
};

class ScopedStreamBuffer {
public:
    ScopedStreamBuffer(std::ostream &s, std::streambuf *replacement)
	: stream(s), original(s.rdbuf(replacement))
    {
    }
    ~ScopedStreamBuffer() { stream.rdbuf(original); }

private:
    std::ostream &stream;
    std::streambuf *original;
};

int
parse_output_overwrite(struct bu_vls *message, size_t argc, const char **argv, void *set_var)
{
    OutputFile *output = static_cast<OutputFile *>(set_var);
    BU_OPT_CHECK_ARGV0(message, argc, argv, "-O");
    if (!output) return -1;
    output->overwrite = true;
    return bu_opt_str(message, argc, argv, &output->filename);
}

int
parse_entity_ids(struct bu_vls *message, size_t argc, const char **argv, void *set_var)
{
    BU_OPT_CHECK_ARGV0(message, argc, argv, "entity ID list");
    std::set<int64_t> *entity_ids = static_cast<std::set<int64_t> *>(set_var);
    if (!entity_ids) return -1;
    std::string error;
    if (!brlcad::step::parse_entity_id_list(argv[0], *entity_ids, &error)) {
	bu_vls_printf(message, "%s\n", error.c_str());
	return -1;
    }
    return 1;
}

void
usage(const char *program)
{
    std::cerr << "Usage: " << program << " [options] -o output.g input.stp\n"
	<< "       " << program << " [options] -O output.g input.stp\n"
	<< "       " << program << " [options] -D input.stp\n\n"
	<< "  -D                  validate without writing a database\n"
	<< "  -v                  report entity-level diagnostics\n"
	<< "  -S FILE             write the legacy entity summary\n"
	<< "  -e, --entity IDS    convert only listed representation-item IDs (repeatable)\n"
	<< "  -j, --jobs N        bounded geometry worker count\n"
	<< "      --abs-tol MM    override output-space tolerance\n"
	<< "      --repair MODE   none or safe (default safe)\n"
	<< "      --strict        reject a partial import\n"
	<< "      --report FILE   write a structured JSON report\n";
}

std::string
temporary_output_name(const std::string &output)
{
    std::ostringstream base;
    base << output << ".ap214-g.tmp." << bu_pid();
    std::string candidate = base.str();
    unsigned int suffix = 0;
    while (bu_file_exists(candidate.c_str(), NULL))
	candidate = base.str() + "." + std::to_string(++suffix);
    return candidate;
}

bool
copy_standard_input(std::string &path)
{
    char buffer[MAXPATHLEN] = {0};
    FILE *temporary = bu_temp_file(buffer, sizeof(buffer));
    if (!temporary) return false;

    char bytes[16384];
    bool ok = true;
    while (!std::feof(stdin)) {
	if (caught_signal) {
	    ok = false;
	    break;
	}
	size_t count = std::fread(bytes, 1, sizeof(bytes), stdin);
	if (count && std::fwrite(bytes, 1, count, temporary) != count) {
	    ok = false;
	    break;
	}
	if (std::ferror(stdin)) {
	    ok = false;
	    break;
	}
    }
    if (std::fclose(temporary) != 0) ok = false;
    path = buffer;
    if (!ok) bu_file_delete(path.c_str());
    return ok;
}

const char *
severity_name(brlcad::step::DiagnosticSeverity severity)
{
    switch (severity) {
	case brlcad::step::DiagnosticSeverity::Information: return "information";
	case brlcad::step::DiagnosticSeverity::Warning: return "warning";
	case brlcad::step::DiagnosticSeverity::Error: return "error";
	case brlcad::step::DiagnosticSeverity::Fatal: return "fatal";
    }
    return "unknown";
}

size_t
peak_rss_bytes()
{
#ifndef _WIN32
    struct rusage usage_info;
    if (getrusage(RUSAGE_SELF, &usage_info) != 0) return 0;
#  ifdef __APPLE__
    return static_cast<size_t>(usage_info.ru_maxrss);
#  else
    return static_cast<size_t>(usage_info.ru_maxrss) * 1024U;
#  endif
#else
    return 0;
#endif
}

void
write_string_array(std::ostream &out, const std::vector<std::string> &values)
{
    out << '[';
    for (size_t i = 0; i < values.size(); ++i) {
	if (i) out << ',';
	out << '"' << brlcad::step::json_escape(values[i]) << '"';
    }
    out << ']';
}

void
write_count_map(std::ostream &out, const std::map<std::string, uint64_t> &values)
{
    out << '{';
    bool first = true;
    for (const auto &value : values) {
	if (!first) out << ',';
	first = false;
	out << '"' << brlcad::step::json_escape(value.first) << "\":" << value.second;
    }
    out << '}';
}

std::string
property_key(const brlcad::step::MetadataProperty &property)
{
    std::string key = property.name.empty() ? property.value_type : property.name;
    std::string result;
    for (unsigned char c : key) {
	if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
	else if (!result.empty() && result.back() != '_') result.push_back('_');
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    return result.empty() ? std::string("property_step") + std::to_string(property.entity_id) : result;
}

std::string
property_values(const brlcad::step::MetadataProperty &property)
{
    if (!property.text.empty()) return property.text;
    std::ostringstream out;
    out << std::setprecision(17);
    for (size_t i = 0; i < property.values.size(); ++i) {
	if (i) out << ' ';
	out << property.values[i];
    }
    return out.str();
}

void
write_property(std::ostream &out, const brlcad::step::MetadataProperty &property)
{
    out << "{\"entity_id\":" << property.entity_id
	<< ",\"representation_id\":" << property.representation_id
	<< ",\"item_entity_id\":" << property.item_entity_id
	<< ",\"category\":\"" << brlcad::step::json_escape(property.category)
	<< "\",\"name\":\"" << brlcad::step::json_escape(property.name)
	<< "\",\"description\":\"" << brlcad::step::json_escape(property.description)
	<< "\",\"value_type\":\"" << brlcad::step::json_escape(property.value_type)
	<< "\",\"units\":\"" << brlcad::step::json_escape(property.units)
	<< "\",\"text\":\"" << brlcad::step::json_escape(property.text)
	<< "\",\"values\":[";
    for (size_t i = 0; i < property.values.size(); ++i) {
	if (i) out << ',';
	out << std::setprecision(17) << property.values[i];
    }
    out << "]}";
}

void
write_product_metadata(std::ostream &out, const brlcad::step::Document &document)
{
    out << '[';
    bool first_product = true;
    for (const auto &entry : document.products) {
	const brlcad::step::Product &product = entry.second;
	if (product.materials.empty() && product.validation_properties.empty()) continue;
	if (!first_product) out << ',';
	first_product = false;
	out << "{\"entity_id\":" << product.entity_id << ",\"materials\":[";
	for (size_t i = 0; i < product.materials.size(); ++i) {
	    if (i) out << ',';
	    const brlcad::step::Material &material = product.materials[i];
	    out << "{\"usage_entity_id\":" << material.usage_entity_id
		<< ",\"definition_entity_id\":" << material.definition_entity_id
		<< ",\"product_entity_id\":" << material.product_entity_id
		<< ",\"identifier\":\"" << brlcad::step::json_escape(material.identifier)
		<< "\",\"name\":\"" << brlcad::step::json_escape(material.name)
		<< "\",\"description\":\"" << brlcad::step::json_escape(material.description)
		<< "\",\"properties\":[";
	    for (size_t j = 0; j < material.properties.size(); ++j) {
		if (j) out << ',';
		write_property(out, material.properties[j]);
	    }
	    out << "]}";
	}
	out << "],\"validation_properties\":[";
	for (size_t i = 0; i < product.validation_properties.size(); ++i) {
	    if (i) out << ',';
	    write_property(out, product.validation_properties[i]);
	}
	out << "]}";
    }
    out << ']';
}

void
write_report_diagnostic(std::ostream &out, const brlcad::step::Diagnostic &diagnostic,
    bool aggregated)
{
    out << "{\"severity\":\"" << severity_name(diagnostic.severity)
	<< "\",\"entity_id\":" << diagnostic.entity_id
	<< ",\"entity_type\":\"" << brlcad::step::json_escape(diagnostic.entity_type)
	<< "\",\"file_offset\":" << diagnostic.file_offset
	<< ",\"line\":" << diagnostic.line
	<< ",\"attribute\":\"" << brlcad::step::json_escape(diagnostic.attribute)
	<< "\",\"message\":\"" << brlcad::step::json_escape(diagnostic.message)
	<< "\",\"count\":" << diagnostic.repeat_count
	<< ",\"aggregated\":" << (aggregated ? "true" : "false") << '}';
}

bool
write_report(const std::string &path, const std::string &input, const std::string &output,
    const brlcad::step::AP214SchemaInfo &schema, const STEPWrapper *wrapper, int exit_status)
{
    if (path.empty()) return true;
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out << "{\n  \"format\":\"brlcad-ap214-import-report-v1\","
	<< "\n  \"input\":\"" << brlcad::step::json_escape(input) << "\","
	<< "\n  \"output\":\"" << brlcad::step::json_escape(output) << "\","
	<< "\n  \"exit_status\":" << exit_status << ','
	<< "\n  \"schemas\":";
    write_string_array(out, schema.identifiers);
    out << ",\n  \"legacy_automotive_design_cc2\":" << (schema.legacy_cc2 ? "true" : "false");

    if (wrapper) {
	const brlcad::step::Document &document = wrapper->Document();
	const brlcad::step::ImportStatistics &stats = wrapper->Statistics();
	const brlcad::step::ImportOptions &options = wrapper->ImportOptions();
	out << ",\n  \"options\":{\"requested_jobs\":" << options.requested_jobs
	    << ",\"effective_jobs\":" << options.effective_jobs
	    << ",\"repair\":\"" << (options.repair == brlcad::step::RepairMode::Safe ? "safe" : "none")
	    << "\",\"strict\":" << (options.strict ? "true" : "false")
	    << ",\"dry_run\":" << (options.dry_run ? "true" : "false")
	    << ",\"selected_entity_ids\":[";
	bool first_selected = true;
	for (std::set<int64_t>::const_iterator selected = options.selected_entity_ids.begin();
	     selected != options.selected_entity_ids.end(); ++selected) {
	    if (!first_selected) out << ',';
	    first_selected = false;
	    out << *selected;
	}
	out << "]}";
	out << ",\n  \"coverage\":{\"entity_counts\":";
	write_count_map(out, document.entity_counts);
	out << ",\"unsupported_counts\":";
	write_count_map(out, document.unsupported_counts);
	std::map<std::string, uint64_t> representation_counts;
	for (const auto &entry : document.representations)
	    ++representation_counts[entry.second.type];
	out << ",\"representation_counts\":";
	write_count_map(out, representation_counts);
	out << ",\"products\":" << stats.products
	    << ",\"occurrences\":" << stats.occurrences
	    << ",\"styles_extracted\":" << stats.styles_extracted
	    << ",\"styles_applied\":" << stats.styles_applied
	    << ",\"layers_extracted\":" << stats.layers_extracted
	    << ",\"materials_extracted\":" << stats.materials_extracted
	    << ",\"properties_extracted\":" << stats.properties_extracted
	    << ",\"selected_entities_matched\":" << stats.selected_entity_ids_encountered.size()
	    << ",\"geometry_attempted\":" << stats.geometry_attempted
	    << ",\"geometry_written\":" << stats.geometry_written
	    << ",\"geometry_skipped\":" << stats.geometry_skipped << '}'
	    << ",\n  \"skipped_items\":[";
	for (size_t i = 0; i < stats.skipped_items.size(); ++i) {
	    if (i) out << ',';
	    const brlcad::step::SkippedItem &item = stats.skipped_items[i];
	    out << "{\"entity_id\":" << item.entity_id
		<< ",\"entity_type\":\"" << brlcad::step::json_escape(item.entity_type)
		<< "\",\"reason\":\"" << brlcad::step::json_escape(item.reason) << "\"}";
	}
	out << "]"
	    << ",\n  \"skipped_items_omitted\":" << stats.skipped_items_omitted
	    << ",\n  \"validation\":{\"invalid_breps\":" << stats.invalid_breps
	    << ",\"output_failures\":" << stats.output_failures
	    << ",\"repairs\":" << stats.repairs
	    << ",\"tolerance_mm\":" << stats.tolerance_mm << '}'
	    << ",\n  \"timings_us\":{\"load\":" << stats.load_time_us
	    << ",\"convert\":" << stats.convert_time_us << '}'
	    << ",\n  \"stepcode_cache\":{\"indexed_instances\":" << stats.lazy_indexed_instances
	    << ",\"loaded_instances\":" << stats.lazy_current_loaded_instances
	    << ",\"high_water_instances\":" << stats.lazy_loaded_instances
	    << ",\"pinned_instances\":" << stats.lazy_pinned_instances
	    << ",\"hits\":" << stats.lazy_cache_hits
	    << ",\"misses\":" << stats.lazy_cache_misses
	    << ",\"materializations\":" << stats.lazy_materializations
	    << ",\"evictions\":" << stats.lazy_evictions
	    << ",\"active_batches\":" << stats.lazy_active_batches
	    << ",\"data_sections\":" << stats.lazy_data_sections
	    << ",\"bytes\":";
	if (stats.lazy_cache_bytes_available)
	    out << stats.lazy_cache_bytes;
	else
	    out << "null";
	out << ",\"high_water_bytes\":";
	if (stats.lazy_cache_bytes_available)
	    out << stats.lazy_cache_byte_high_water;
	else
	    out << "null";
	out << '}'
	    << ",\n  \"product_metadata\":";
	write_product_metadata(out, document);
	out << ",\n  \"peak_rss_bytes\":" << stats.peak_rss_bytes
	    << ",\n  \"diagnostics\":[";
	const std::vector<brlcad::step::Diagnostic> &diagnostics = wrapper->Diagnostics();
	if (options.verbose) {
	    for (size_t i = 0; i < diagnostics.size(); ++i) {
		if (i) out << ',';
		write_report_diagnostic(out, diagnostics[i], false);
	    }
	} else {
	    /* Keep the default report bounded on large assemblies.  The key omits
	     * entity location deliberately, grouping by diagnostic type while the
	     * summed count still records every occurrence exactly.  -v retains the
	     * complete entity-level records above. */
	    std::map<std::string, brlcad::step::Diagnostic> aggregated;
	    for (const brlcad::step::Diagnostic &diagnostic : diagnostics) {
		std::string key = std::to_string(static_cast<int>(diagnostic.severity));
		key.push_back('\0');
		key += diagnostic.entity_type;
		key.push_back('\0');
		key += diagnostic.attribute;
		key.push_back('\0');
		key += diagnostic.message;
		std::map<std::string, brlcad::step::Diagnostic>::iterator found =
		    aggregated.find(key);
		if (found == aggregated.end()) {
		    brlcad::step::Diagnostic summary = diagnostic;
		    summary.entity_id = 0;
		    summary.file_offset = 0;
		    summary.line = 0;
		    aggregated.insert(std::make_pair(key, summary));
		} else {
		    found->second.repeat_count += diagnostic.repeat_count;
		}
	    }
	    bool first = true;
	    for (const auto &entry : aggregated) {
		if (!first) out << ',';
		first = false;
		write_report_diagnostic(out, entry.second, true);
	    }
	}
	out << ']';
    }
    if (!schema.error.empty())
	out << ",\n  \"schema_error\":\"" << brlcad::step::json_escape(schema.error) << '"';
    out << "\n}\n";
    return static_cast<bool>(out);
}

void
print_diagnostics(const STEPWrapper &wrapper, bool verbose)
{
    if (verbose) {
	for (const auto &diagnostic : wrapper.Diagnostics()) {
	    bu_log("%s: #%lld %s%s%s (count=%llu)\n", severity_name(diagnostic.severity),
		static_cast<long long>(diagnostic.entity_id), diagnostic.entity_type.c_str(),
		diagnostic.entity_type.empty() ? "" : ": ", diagnostic.message.c_str(),
		static_cast<unsigned long long>(diagnostic.repeat_count));
	}
	return;
    }

    std::map<std::string, uint64_t> aggregated;
    for (const auto &diagnostic : wrapper.Diagnostics()) {
	std::string key = std::string(severity_name(diagnostic.severity)) + ": " +
	    diagnostic.entity_type + (diagnostic.entity_type.empty() ? "" : ": ") + diagnostic.message;
	aggregated[key] += diagnostic.repeat_count;
    }
    for (const auto &message : aggregated)
	bu_log("%s (count=%llu)\n", message.first.c_str(), static_cast<unsigned long long>(message.second));
}

} // namespace

int
main(int argc, const char *argv[])
{
    bu_setprogname(argv[0]);
    std::signal(SIGINT, record_signal);
    std::signal(SIGTERM, record_signal);

    static OutputFile output;
    static int dry_run = 0;
    static int verbose = 0;
    static int strict = 0;
    static int help = 0;
    static int jobs = 1;
    static fastf_t absolute_tolerance = 0.0;
    static char *repair_name = NULL;
    static char *report_name = NULL;
    static char *summary_name = NULL;
    static std::set<int64_t> selected_entity_ids;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"h", "help", "", NULL, &help, "show help"},
	{"D", "", "", NULL, &dry_run, "dry run"},
	{"v", "", "", NULL, &verbose, "verbose diagnostics"},
	{"O", "", "FILE", parse_output_overwrite, &output, "output file (overwrite)"},
	{"o", "", "FILE", bu_opt_str, &output.filename, "output file"},
	{"S", "", "FILE", bu_opt_str, &summary_name, "legacy summary file"},
	{"e", "entity", "ID[,ID...]", parse_entity_ids, &selected_entity_ids,
	    "representation-item entity IDs"},
	{"j", "jobs", "N", bu_opt_int, &jobs, "geometry workers"},
	{"", "abs-tol", "MM", bu_opt_fastf_t, &absolute_tolerance, "absolute tolerance"},
	{"", "repair", "MODE", bu_opt_str, &repair_name, "none or safe"},
	{"", "strict", "", NULL, &strict, "reject partial output"},
	{"", "report", "FILE", bu_opt_str, &report_name, "JSON report"},
	BU_OPT_DESC_NULL
    };

    const char *program = argv[0];
    ++argv;
    --argc;
    argc = bu_opt_parse(&messages, argc, argv, options);
    if (bu_vls_strlen(&messages)) bu_log("%s\n", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    if (help) {
	usage(program);
	return 0;
    }
    if (argc != 1 || (!dry_run && BU_STR_EMPTY(output.filename)) || jobs < 1 || absolute_tolerance < 0.0) {
	usage(program);
	return 2;
    }

    const std::string final_output = output.filename ? output.filename : "";

    brlcad::step::RepairMode repair = brlcad::step::RepairMode::Safe;
    if (repair_name && BU_STR_EQUAL(repair_name, "none")) repair = brlcad::step::RepairMode::None;
    else if (repair_name && !BU_STR_EQUAL(repair_name, "safe")) {
	bu_log("ERROR: --repair must be 'none' or 'safe'\n");
	return 2;
    }
    if (!output.overwrite && !dry_run && bu_file_exists(final_output.c_str(), NULL)) {
	bu_log("ERROR: refusing to overwrite '%s'; use -O to replace it\n", final_output.c_str());
	return 2;
    }

    const std::string requested_input = argv[0];
    std::string input = requested_input;
    TemporaryPath input_temporary;
    if (requested_input == "-") {
	if (!copy_standard_input(input)) {
	    bu_log("ERROR: unable to spool standard input\n");
	    return signal_exit_status() ? signal_exit_status() : 2;
	}
	input_temporary.reset(input);
    } else if (!bu_file_exists(input.c_str(), NULL)) {
	bu_log("ERROR: unable to read input '%s'\n", input.c_str());
	return 2;
    }

    const brlcad::step::AP214SchemaInfo schema = brlcad::step::AP214Adapter::inspect_file(input);
    const std::string report = report_name ? report_name : "";
    if (!schema.accepted) {
	bu_log("ERROR: %s\n", schema.error.c_str());
	write_report(report, requested_input, final_output, schema, NULL, 2);
	return 2;
    }
    if (schema.legacy_cc2)
	bu_log("WARNING: accepting legacy AUTOMOTIVE_DESIGN_CC2 schema identifier\n");

    STEPWrapper wrapper;
    STEPProgressReporter progress_reporter(wrapper);
    wrapper.dry_run = dry_run;
    wrapper.summary_log_file = summary_name;
    wrapper.Verbose(verbose != 0);
    wrapper.Document().schema_identifiers = schema.identifiers;
    brlcad::step::ImportOptions import_options;
    import_options.requested_jobs = static_cast<unsigned int>(jobs);
    import_options.effective_jobs = static_cast<unsigned int>(jobs);
    import_options.absolute_tolerance_mm = absolute_tolerance;
    import_options.repair = repair;
    import_options.strict = strict != 0;
    import_options.verbose = verbose != 0;
    import_options.dry_run = dry_run != 0;
    import_options.selected_entity_ids = selected_entity_ids;
    wrapper.SetImportOptions(import_options);
    wrapper.SetCancellationCallback([]() { return caught_signal != 0; });
    if (schema.legacy_cc2)
	wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, 0, "FILE_SCHEMA",
	    std::string(), "legacy AUTOMOTIVE_DESIGN_CC2 identifier accepted");

    int64_t started = bu_gettime();
    if (!wrapper.load(input)) {
	wrapper.Statistics().load_time_us = bu_gettime() - started;
	wrapper.Statistics().peak_rss_bytes = peak_rss_bytes();
	print_diagnostics(wrapper, verbose != 0);
	const int status = signal_exit_status() ? signal_exit_status() : 2;
	write_report(report, requested_input, final_output, schema, &wrapper, status);
	return status;
    }
    wrapper.Statistics().load_time_us = bu_gettime() - started;

    std::string output_path = final_output;
    TemporaryPath output_temporary;
    if (!dry_run) {
	output_path = temporary_output_name(final_output);
	output_temporary.reset(output_path);
    }

    BRLCADWrapper database;
    database.dry_run = dry_run;
    if (!database.OpenFile(output_path)) {
	wrapper.Statistics().peak_rss_bytes = peak_rss_bytes();
	write_report(report, requested_input, final_output, schema, &wrapper, 4);
	return 4;
    }

    started = bu_gettime();
    std::streambuf *original_error_stream = std::cerr.rdbuf();
    BoundedConversionLog conversion_log(original_error_stream, verbose != 0);
    bool converted = false;
    {
	ScopedStreamBuffer redirect(std::cerr, &conversion_log);
	converted = wrapper.convert(&database);
    }
    conversion_log.finish();
    conversion_log.report();
    wrapper.Statistics().convert_time_us = bu_gettime() - started;
    for (const auto &entry : wrapper.Document().products) {
	const brlcad::step::Product &product = entry.second;
	if (product.output_name.empty()) continue;
	database.SetAttribute(product.output_name, "step:source_id", std::to_string(product.entity_id));
	if (!product.original_name.empty())
	    database.SetAttribute(product.output_name, "step:original_name", product.original_name);
	if (!product.identifier.empty())
	    database.SetAttribute(product.output_name, "step:product_id", product.identifier);
	if (!product.description.empty())
	    database.SetAttribute(product.output_name, "step:description", product.description);
	if (!product.revision.empty())
	    database.SetAttribute(product.output_name, "step:revision", product.revision);
	if (!product.revision_description.empty())
	    database.SetAttribute(product.output_name, "step:revision_description", product.revision_description);
	if (!product.definition_identifier.empty())
	    database.SetAttribute(product.output_name, "step:definition_id", product.definition_identifier);
	if (!product.definition_description.empty())
	    database.SetAttribute(product.output_name, "step:definition_description", product.definition_description);
	for (size_t material_index = 0; material_index < product.materials.size(); ++material_index) {
	    const brlcad::step::Material &material = product.materials[material_index];
	    const std::string indexed = "step:material:" + std::to_string(material_index + 1) + ':';
	    database.SetAttribute(product.output_name, indexed + "usage_id", std::to_string(material.usage_entity_id));
	    database.SetAttribute(product.output_name, indexed + "definition_id", std::to_string(material.definition_entity_id));
	    database.SetAttribute(product.output_name, indexed + "source_id", std::to_string(material.product_entity_id));
	    if (!material.identifier.empty()) database.SetAttribute(product.output_name, indexed + "id", material.identifier);
	    if (!material.name.empty()) database.SetAttribute(product.output_name, indexed + "name", material.name);
	    if (!material.description.empty()) database.SetAttribute(product.output_name, indexed + "description", material.description);
	    if (material_index == 0) {
		database.SetAttribute(product.output_name, "step:material_usage_id", std::to_string(material.usage_entity_id));
		database.SetAttribute(product.output_name, "step:material_source_id", std::to_string(material.product_entity_id));
		if (!material.identifier.empty()) database.SetAttribute(product.output_name, "step:material_id", material.identifier);
		if (!material.name.empty()) database.SetAttribute(product.output_name, "step:material_name", material.name);
		if (!material.description.empty()) database.SetAttribute(product.output_name, "step:material_description", material.description);
	    }
	    for (const brlcad::step::MetadataProperty &property : material.properties) {
		const std::string key = property_key(property);
		const std::string value = property_values(property);
		if (!value.empty()) database.SetAttribute(product.output_name, indexed + key, value);
		if (!property.units.empty()) database.SetAttribute(product.output_name, indexed + key + "_units", property.units);
		if (material_index == 0 && key.find("density") != std::string::npos) {
		    database.SetAttribute(product.output_name, "step:density", value);
		    if (!property.units.empty()) database.SetAttribute(product.output_name, "step:density_units", property.units);
		}
	    }
	}
	std::set<std::string> validation_keys;
	for (const brlcad::step::MetadataProperty &property : product.validation_properties) {
	    std::string key = property_key(property);
	    if (!validation_keys.insert(key).second)
		key += "_step" + std::to_string(property.entity_id);
	    const std::string value = property_values(property);
	    if (!value.empty()) database.SetAttribute(product.output_name, "step:validation:" + key, value);
	    if (!property.units.empty()) database.SetAttribute(product.output_name,
		"step:validation:" + key + "_units", property.units);
	}
    }
    database.Close();
    wrapper.Statistics().peak_rss_bytes = peak_rss_bytes();

    int status = 0;
    if (signal_exit_status()) {
	status = signal_exit_status();
    } else if (wrapper.Statistics().output_failures || (!converted && !strict)) {
	status = 4;
    } else if (wrapper.Statistics().geometry_written == 0 || (strict && wrapper.Statistics().geometry_skipped)) {
	status = 3;
    } else if (wrapper.Statistics().geometry_skipped) {
	status = 1;
    }

    if (!dry_run && (status == 0 || status == 1)) {
	if (std::rename(output_path.c_str(), final_output.c_str()) != 0) {
	    bu_log("ERROR: cannot replace '%s': %s\n", final_output.c_str(), std::strerror(errno));
	    status = 4;
	} else {
	    output_temporary.release();
	}
    }

    print_diagnostics(wrapper, verbose != 0);
    if (!write_report(report, requested_input, final_output, schema, &wrapper, status)) {
	bu_log("ERROR: unable to write report '%s'\n", report.c_str());
	if (status == 0) status = 4;
    }
    return status;
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
