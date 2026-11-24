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
#include "STEPReport.h"
#include "STEPString.h"
#include "STEPWrapper.h"
#include "STEPProgressReporter.h"
#include "step_plugin.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/datetime.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/interrupt.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "bu/process.h"
#include "bu/vls.h"

#include <cerrno>
#include <algorithm>
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

namespace {

/* Exact STEP geometry conversion is memory-heavier than ordinary libbu
 * parallel loops.  Use the available CPUs by default, but cap automatic
 * concurrency at the eight-worker configuration exercised by the importer
 * memory gates.  -j remains an explicit override, including -j 1 for fully
 * serial diagnostic runs. */
constexpr size_t kMaximumAutomaticGeometryJobs = 8;

int
default_geometry_jobs()
{
    return static_cast<int>(std::max<size_t>(1,
	std::min(bu_avail_cpus(), kMaximumAutomaticGeometryJobs)));
}

uint64_t
seconds_to_milliseconds(fastf_t seconds)
{
    if (!(seconds > 0.0)) return 0;
    const long double milliseconds = static_cast<long double>(seconds) * 1000.0L;
    return milliseconds >= static_cast<long double>(UINT64_MAX) ? UINT64_MAX :
	static_cast<uint64_t>(milliseconds + 0.5L);
}

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
parse_entity_ids(struct bu_vls *message, const char *arg, void *set_var)
{
    std::set<int64_t> **entity_ptr = static_cast<std::set<int64_t> **>(set_var);
    std::set<int64_t> validation_ids;
    std::set<int64_t> *entity_ids = entity_ptr ? *entity_ptr : &validation_ids;
    if (!entity_ids) return -1;
    std::string error;
    if (!arg || !brlcad::step::parse_entity_id_list(arg, *entity_ids, &error)) {
	if (message) bu_vls_printf(message, "%s\n", error.c_str());
	return -1;
    }
    return 0;
}

struct ap214_args {
    int help = 0, dry_run = 0, verbose = 0, exact = 0, strict = 0;
    int reject_invalid = 0, skip_open_shells = 0, no_item_budget = 0, jobs = 1;
    const char *output_overwrite = NULL, *output = NULL, *summary = NULL;
    const char *repair = NULL, *report = NULL;
    fastf_t absolute_tolerance = 0.0, budget_scale = 0.0;
    fastf_t item_budget = 0.0, stall_timeout = 0.0;
    std::set<int64_t> *entities = NULL;
};
static const struct bu_arg_value_keyword ap214_repair_values[] = {
    {"none", NULL, "Disable repair"}, {"safe", NULL, "Use safe repair"}, {NULL, NULL, NULL}
};
static const struct bu_arg_option ap214_options[] = {
    BU_ARG_FLAG("h", "help", struct ap214_args, help, "Show help"),
    BU_ARG_FLAG("D", "dry-run", struct ap214_args, dry_run, "Validate without writing"),
    BU_ARG_FLAG("v", "verbose", struct ap214_args, verbose, "Verbose diagnostics"),
    BU_ARG_VALUE("O", "output-overwrite", struct ap214_args, output_overwrite, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Output file and overwrite it"),
    BU_ARG_VALUE("o", "output", struct ap214_args, output, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Output file"),
    BU_ARG_VALUE("S", "summary", struct ap214_args, summary, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Per-instance conversion status CSV"),
    BU_ARG_CUSTOM("e", "entity", struct ap214_args, entities, parse_entity_ids, "ID[,ID...]", "Representation-item entity IDs"),
    BU_ARG_INTEGER_RANGE("j", "jobs", struct ap214_args, jobs, 1, 1, 0, 0, "N", "Geometry workers (default: up to 8 CPUs)"),
    BU_ARG_NUMBER_RANGE(NULL, "abs-tol", struct ap214_args, absolute_tolerance, 1, 0.0, 0, 0.0, "MM", "Absolute tolerance"),
    BU_ARG_NUMBER_RANGE(NULL, "budget-scale", struct ap214_args, budget_scale, 1, 0.0, 0, 0.0, "FACTOR", "Manual machine-speed budget scale"),
    BU_ARG_NUMBER_RANGE(NULL, "item-budget", struct ap214_args, item_budget, 1, 0.0, 0, 0.0, "SECONDS", "Ordinary per-item CPU-work budget"),
    BU_ARG_FLAG(NULL, "no-item-budget", struct ap214_args, no_item_budget, "Disable CPU-work budgets; retain no-progress detection"),
    BU_ARG_NUMBER_RANGE(NULL, "stall-timeout", struct ap214_args, stall_timeout, 1, 0.0, 0, 0.0, "SECONDS", "No-progress cancellation interval"),
    BU_ARG_KEYWORD_VALUES(NULL, "repair", struct ap214_args, repair, "MODE", "Repair mode (default: safe)", ap214_repair_values),
    BU_ARG_FLAG(NULL, "exact", struct ap214_args, exact, "Strictly enforce declared tolerance"),
    BU_ARG_FLAG(NULL, "strict", struct ap214_args, strict, "Reject partial output"),
    BU_ARG_FLAG(NULL, "reject-invalid-objs", struct ap214_args, reject_invalid, "Reject B-reps which fail validation"),
    BU_ARG_FLAG(NULL, "skip-open-shells", struct ap214_args, skip_open_shells, "Exclude standalone open-shell surface geometry"),
    BU_ARG_VALUE(NULL, "report", struct ap214_args, report, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "JSON report"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand ap214_operands[] = {
    BU_ARG_OPERAND("input.stp", BU_ARG_VALUE_FILE, 1, 1, "Input STEP AP214 file", NULL), BU_ARG_OPERAND_NULL
};
static const char * const ap214_outputs[] = {"output", "output-overwrite", NULL};
static const struct bu_arg_constraint ap214_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS, ap214_outputs, 0, 1, "select at most one output option"), BU_ARG_CONSTRAINT_NULL
};
static const struct bu_arg_schema ap214_schema = BU_ARG_SCHEMA(
    "ap214-g", "Import STEP AP214 geometry", ap214_options, ap214_operands,
    BU_ARG_PARSE_INTERSPERSED, BU_ARG_SCHEMA_META_HELP(NULL, ap214_constraints, NULL, NULL, NULL));

void
usage(const char *program)
{
    char *help = bu_arg_schema_help(&ap214_schema, program);
    if (help) {
	std::cerr << help;
	bu_free(help, "AP214 import help");
    }
    std::cerr << "\nModes:\n"
	<< "  -o FILE   write a new output database\n"
	<< "  -O FILE   overwrite the output database\n"
	<< "  -D        validate without writing a database\n";
}

std::string
temporary_output_name(const std::string &output)
{
    std::ostringstream base;
    base << output << ".step-g.tmp." << bu_pid();
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
    const ssize_t peak = bu_mem(BU_MEM_PROCESS_PEAK_RESIDENT, NULL);
    return peak < 0 ? 0 : static_cast<size_t>(peak);
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

void
write_performance_telemetry(std::ostream &out,
    const brlcad::step::ImportStatistics &stats)
{
    out << "{\"calibration\":{\"ran\":"
	<< (stats.budget_calibration_ran ? "true" : "false")
	<< ",\"valid\":" << (stats.budget_calibration_valid ? "true" : "false")
	<< ",\"queries\":" << stats.budget_calibration_queries
	<< ",\"elapsed_us\":" << stats.budget_calibration_microseconds
	<< ",\"parallel_workers\":"
	<< stats.budget_calibration_parallel_workers
	<< ",\"scalar_queries_per_second\":"
	<< stats.budget_calibration_scalar_queries_per_second
	<< ",\"parallel_queries_per_second\":"
	<< stats.budget_calibration_parallel_queries_per_second
	<< ",\"parallel_cpu_queries_per_second\":"
	<< stats.budget_calibration_parallel_cpu_queries_per_second
	<< "},\"stages\":{";
    bool first = true;
    for (const auto &entry : stats.stage_timings) {
	if (!first) out << ',';
	first = false;
	out << '\"' << brlcad::step::json_escape(entry.first) << "\":{"
	    << "\"calls\":" << entry.second.calls
	    << ",\"total_us\":" << entry.second.total_us
	    << ",\"maximum_us\":" << entry.second.maximum_us
	    << ",\"maximum_entity_id\":" << entry.second.maximum_entity_id << '}';
    }
    std::vector<brlcad::step::ItemTiming> slow = stats.slow_item_timings;
    std::sort(slow.begin(), slow.end(), [](const brlcad::step::ItemTiming &left,
	const brlcad::step::ItemTiming &right) {
	if (left.entity_id != right.entity_id) return left.entity_id < right.entity_id;
	if (left.stage != right.stage) return left.stage < right.stage;
	return left.elapsed_us < right.elapsed_us;
    });
    out << "},\"slow_items\":[";
    for (size_t i = 0; i < slow.size(); ++i) {
	if (i) out << ',';
	out << "{\"entity_id\":" << slow[i].entity_id
	    << ",\"entity_type\":\"" << brlcad::step::json_escape(slow[i].entity_type)
	    << "\",\"stage\":\"" << brlcad::step::json_escape(slow[i].stage)
	    << "\",\"elapsed_us\":" << slow[i].elapsed_us
	    << ",\"faces\":" << slow[i].faces << ",\"edges\":"
	    << slow[i].edges << ",\"trims\":" << slow[i].trims << '}';
    }
    out << "],\"slow_items_omitted\":" << stats.slow_item_timings_omitted
	<< ",\"pullback\":{\"closest_point_queries\":"
	<< stats.pullback_closest_point_queries
	<< ",\"surfaces_prepared\":" << stats.pullback_surfaces_prepared
	<< ",\"surface_cache_hits\":" << stats.pullback_surface_cache_hits
	<< ",\"span_boxes_built\":" << stats.pullback_span_boxes_built
	<< ",\"span_boxes_tested\":" << stats.pullback_span_boxes_tested
	<< ",\"primary_search_successes\":"
	<< stats.pullback_primary_search_successes
	<< ",\"continuity_seed_searches\":"
	<< stats.pullback_continuity_seed_searches
	<< ",\"continuity_seed_successes\":"
	<< stats.pullback_continuity_seed_successes
	<< ",\"continuity_seed_failures\":"
	<< stats.pullback_continuity_seed_failures
	<< ",\"continuity_seed_finite_candidates\":"
	<< stats.pullback_continuity_seed_finite_candidates
	<< ",\"continuity_seed_iterations\":"
	<< stats.pullback_continuity_seed_iterations
	<< ",\"continuity_seed_line_searches\":"
	<< stats.pullback_continuity_seed_line_searches
	<< ",\"maximum_continuity_seed_iterations\":"
	<< stats.pullback_maximum_continuity_seed_iterations
	<< ",\"maximum_continuity_seed_line_searches\":"
	<< stats.pullback_maximum_continuity_seed_line_searches
	<< ",\"multiseed_fallbacks\":" << stats.pullback_multiseed_fallbacks
	<< ",\"multiseed_successes\":" << stats.pullback_multiseed_successes
	<< ",\"multiseed_failures\":" << stats.pullback_multiseed_failures
	<< ",\"fallback_calls_with_finite_primary\":"
	<< stats.pullback_fallback_calls_with_finite_primary
	<< ",\"fallback_samples_evaluated\":"
	<< stats.pullback_fallback_samples_evaluated
	<< ",\"fallback_seed_refinements\":"
	<< stats.pullback_fallback_seed_refinements
	<< ",\"fallback_refinement_improvements\":"
	<< stats.pullback_fallback_refinement_improvements
	<< ",\"fallback_late_seed_improvements\":"
	<< stats.pullback_fallback_late_seed_improvements
	<< ",\"maximum_winning_seed_index\":"
	<< stats.pullback_maximum_winning_seed_index
	<< ",\"subdivision_nodes\":" << stats.pullback_subdivision_nodes
	<< ",\"maximum_subdivision_nodes\":"
	<< stats.pullback_maximum_subdivision_nodes
	<< ",\"preparation_us\":" << stats.pullback_preparation_us
	<< ",\"primary_search_us\":" << stats.pullback_primary_search_us
	<< ",\"continuity_seed_us\":" << stats.pullback_continuity_seed_us
	<< ",\"multiseed_us\":" << stats.pullback_multiseed_us
	<< ",\"fallback_primary_improvement_total\":"
	<< stats.pullback_fallback_primary_improvement_total
	<< ",\"fallback_primary_improvement_maximum\":"
	<< stats.pullback_fallback_primary_improvement_maximum
	<< ",\"fallback_refinement_improvement_total\":"
	<< stats.pullback_fallback_refinement_improvement_total
	<< ",\"fallback_refinement_improvement_maximum\":"
	<< stats.pullback_fallback_refinement_improvement_maximum << "}}";
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
	    << ",\"requested_budget_scale\":" << options.budget_scale
	    << ",\"effective_budget_scale\":" << options.effective_budget_scale
	    << ",\"item_cpu_budgets_enabled\":"
	    << (options.disable_item_budgets ? "false" : "true")
	    << ",\"effective_item_cpu_budget_ms\":"
	    << options.effective_item_budget_milliseconds
	    << ",\"item_budget_clock\":\"critical_path_cpu\""
	    << ",\"effective_stall_timeout_ms\":"
	    << options.effective_stall_timeout_milliseconds
	    << ",\"repair\":\"" << (options.repair == brlcad::step::RepairMode::Safe ? "safe" : "none")
	    << "\",\"exact\":" << (options.exact ? "true" : "false")
	    << ",\"requested_invalid_brep_policy\":\""
	    << (options.requested_invalid_brep_policy ==
		brlcad::step::InvalidBrepPolicy::Preserve ? "preserve" : "reject")
	    << "\",\"effective_invalid_brep_policy\":\""
	    << (options.effective_invalid_brep_policy ==
		brlcad::step::InvalidBrepPolicy::Preserve ? "preserve" : "reject") << '"'
	    << ",\"strict\":" << (options.strict ? "true" : "false")
	    << ",\"dry_run\":" << (options.dry_run ? "true" : "false")
	    << ",\"skip_open_shells\":" <<
		(options.skip_open_shells ? "true" : "false")
	    << ",\"selected_entity_ids\":[";
	bool first_selected = true;
	for (std::set<int64_t>::const_iterator selected = options.selected_entity_ids.begin();
	     selected != options.selected_entity_ids.end(); ++selected) {
	    if (!first_selected) out << ',';
	    first_selected = false;
	    out << *selected;
	}
	out << "]}";
	out << ",\n  \"coverage\":{\"entity_counts_complete\":"
	    << (document.entity_counts_complete ? "true" : "false")
	    << ",\"entity_counts\":";
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
	    << ",\"pmi_semantic_records\":" << stats.pmi_semantic_records
	    << ",\"pmi_presentation_records\":" << stats.pmi_presentation_records
	    << ",\"pmi_association_records\":" << stats.pmi_association_records
	    << ",\"pmi_dependency_records\":" << stats.pmi_dependency_records
	<< ",\"pmi_native_datums\":" << stats.pmi_native_datums
	<< ",\"pmi_native_annotations\":" << stats.pmi_native_annotations
	<< ",\"pmi_invalid_records\":" << stats.pmi_invalid_records
	    << ",\"materials_extracted\":" << stats.materials_extracted
	    << ",\"properties_extracted\":" << stats.properties_extracted
	    << ",\"properties_invalid\":" << stats.properties_invalid
	    << ",\"selected_entities_matched\":" << stats.selected_entity_ids_encountered.size()
	    << ",\"geometry_attempted\":" << stats.geometry_attempted
	    << ",\"geometry_written\":" << stats.geometry_written
	    << ",\"geometry_skipped\":" << stats.geometry_skipped
	    << ",\"outcome\":\"" << brlcad::step::json_escape(stats.outcome) << '"'
	    << ",\"geometry_filtered\":" << stats.geometry_filtered << "}"
	    << ",\n  \"representation_coverage\":[";
	bool first_coverage = true;
	for (std::map<int64_t, brlcad::step::RepresentationCoverage>::const_iterator entry =
		document.representation_coverage.begin();
	     entry != document.representation_coverage.end(); ++entry) {
	    const brlcad::step::RepresentationCoverage &coverage = entry->second;
	    if (!first_coverage) out << ',';
	    first_coverage = false;
	    out << "{\"entity_id\":" << coverage.entity_id
		<< ",\"product_id\":" << coverage.product_id
		<< ",\"type\":\"" << brlcad::step::json_escape(coverage.type)
		<< "\",\"status\":\""
		<< brlcad::step::RepresentationCoverageStatusName(coverage.status)
		<< "\",\"reason\":\"" << brlcad::step::json_escape(coverage.reason)
		<< "\",\"handled_items\":" << coverage.handled_items
		<< ",\"preserved_invalid_items\":"
		<< coverage.preserved_invalid_items
		<< ",\"filtered_items\":" << coverage.filtered_items
		<< ",\"intentionally_non_geometric_items\":"
		<< coverage.intentionally_non_geometric_items
		<< ",\"omitted_items\":" << coverage.omitted_items
		<< ",\"items\":[";
	    for (size_t i = 0; i < coverage.items.size(); ++i) {
		if (i) out << ',';
		const brlcad::step::RepresentationItemCoverage &item = coverage.items[i];
		out << "{\"entity_id\":" << item.entity_id
		    << ",\"type\":\"" << brlcad::step::json_escape(item.type)
		    << "\",\"status\":\""
		    << brlcad::step::RepresentationCoverageStatusName(item.status)
		    << "\",\"reason\":\"" << brlcad::step::json_escape(item.reason)
		    << "\"}";
	    }
	    out << "]}";
	}
	out << ']' << ",\n  ";
	brlcad::step::write_assembly_report_json(out, document);
	out << ",\n  ";
	brlcad::step::write_configuration_report_json(out, document);
	out
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
	    << ",\"invalid_breps_written\":" << stats.invalid_breps_written
	    << ",\"invalid_breps_rejected\":" << stats.invalid_breps_rejected
	    << ",\"output_failures\":" << stats.output_failures
	    << ",\"repairs\":" << stats.repairs
	    << ",\"inferred_curves\":" << stats.inferred_curves
	    << ",\"tolerance_mm\":" << stats.tolerance_mm << '}'
	    << ",\n  ";
	brlcad::step::write_inferred_curve_report_json(out,
	    wrapper->InferredCurves());
	out
	    << ",\n  \"timings_us\":{\"load\":" << stats.load_time_us
	    << ",\"index\":" << std::max<int64_t>(0,
		stats.load_time_us - stats.inventory_time_us)
	    << ",\"inventory\":" << stats.inventory_time_us
	    << ",\"convert\":" << stats.convert_time_us << '}'
	    << ",\n  \"performance\":";
	write_performance_telemetry(out, stats);
	out
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
	out << '}' << ",\n  ";
	brlcad::step::write_product_metadata_report_json(out, document);
	out << ",\n  ";
	brlcad::step::write_pmi_report_json(out, document);
	out << ",\n  \"peak_rss_bytes\":" << stats.peak_rss_bytes
	    << ",\n  \"diagnostics\":[";
	const std::vector<brlcad::step::Diagnostic> &diagnostics = wrapper->Diagnostics();
	if (options.verbose) {
	    for (size_t i = 0; i < diagnostics.size(); ++i) {
		if (i) out << ',';
		write_report_diagnostic(out, diagnostics[i], false);
	    }
	} else {
	    /* Keep the default report bounded on large assemblies.  Group by
	     * diagnostic type, retain the first entity as a traceable
	     * representative, and sum every occurrence exactly.  -v retains the
	     * complete entity-level records above. */
	    std::map<std::string, brlcad::step::Diagnostic> aggregated;
	    for (const brlcad::step::Diagnostic &diagnostic : diagnostics) {
		std::string summary_message = diagnostic.message;
		if (diagnostic.severity == brlcad::step::DiagnosticSeverity::Warning &&
			(summary_message.find("source curve/surface separation exceeds declared tolerance ") == 0 ||
			 summary_message.find("source edge geometry separation exceeds declared tolerance ") == 0))
		    summary_message = "source edge/surface geometry exceeded the declared tolerance; "
			"adjusted affected OpenNURBS edge tolerances after dense validation";
		std::string key = std::to_string(static_cast<int>(diagnostic.severity));
		key.push_back('\0');
		key += diagnostic.entity_type;
		key.push_back('\0');
		key += diagnostic.attribute;
		key.push_back('\0');
		key += summary_message;
		std::map<std::string, brlcad::step::Diagnostic>::iterator found =
		    aggregated.find(key);
		if (found == aggregated.end()) {
		    brlcad::step::Diagnostic summary = diagnostic;
		    summary.message = summary_message;
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

    struct DiagnosticSummary {
	uint64_t count = 0;
	int64_t representative_entity_id = 0;
    };
    std::map<std::string, DiagnosticSummary> aggregated;
    for (const auto &diagnostic : wrapper.Diagnostics()) {
	std::string message = diagnostic.message;
	if (diagnostic.severity == brlcad::step::DiagnosticSeverity::Warning &&
		(message.find("source curve/surface separation exceeds declared tolerance ") == 0 ||
		 message.find("source edge geometry separation exceeds declared tolerance ") == 0))
	    message = "source edge/surface geometry exceeded the declared tolerance; "
		"adjusted affected OpenNURBS edge tolerances after dense validation";
	std::string key = std::string(severity_name(diagnostic.severity)) + ": " +
	    diagnostic.entity_type + (diagnostic.entity_type.empty() ? "" : ": ") + message;
	DiagnosticSummary &summary = aggregated[key];
	if (!summary.representative_entity_id && diagnostic.entity_id > 0)
	    summary.representative_entity_id = diagnostic.entity_id;
	summary.count += diagnostic.repeat_count;
    }
    for (const auto &message : aggregated) {
	if (message.second.representative_entity_id > 0)
	    bu_log("%s [representative=#%lld] (count=%llu)\n", message.first.c_str(),
		static_cast<long long>(message.second.representative_entity_id),
		static_cast<unsigned long long>(message.second.count));
	else
	    bu_log("%s (count=%llu)\n", message.first.c_str(),
		static_cast<unsigned long long>(message.second.count));
    }
}

} // namespace

extern "C" int
step_ap214_import_cli(int argc, const char *argv[])
{
    if (argc < 1 || !argv || !argv[0]) return 2;
    caught_signal = 0;
    bu_setprogname(argv[0]);
    std::signal(SIGINT, record_signal);
    std::signal(SIGTERM, record_signal);

    const char *program = argv[0];
    ++argv;
    --argc;
    std::set<int64_t> selected_entity_ids;
    ap214_args args;
    args.jobs = default_geometry_jobs();
    args.entities = &selected_entity_ids;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const int operand_index = bu_arg_schema_parse_complete(&ap214_schema, &args,
	&messages, argc, argv);
    if (operand_index < 0) {
	if (bu_vls_strlen(&messages)) bu_log("%s\n", bu_vls_cstr(&messages));
	bu_vls_free(&messages);
	usage(program);
	return 2;
    }
    bu_vls_free(&messages);
    argc -= operand_index;
    argv += operand_index;
    if (args.help) {
	usage(program);
	return 0;
    }
    const char *output_filename = args.output_overwrite ? args.output_overwrite : args.output;
    if (!args.dry_run && BU_STR_EMPTY(output_filename)) {
	usage(program);
	return 2;
    }
    const int dry_run = args.dry_run;
    const int verbose = args.verbose;
    const int exact = args.exact;
    const int strict = args.strict;
    const int reject_invalid_objs = args.reject_invalid;
    const int skip_open_shells = args.skip_open_shells;
    const int no_item_budget = args.no_item_budget;
    const int jobs = args.jobs;
    const int overwrite = args.output_overwrite != NULL;
    const fastf_t absolute_tolerance = args.absolute_tolerance;
    const fastf_t budget_scale = args.budget_scale;
    const fastf_t item_budget_seconds = args.item_budget;
    const fastf_t stall_timeout_seconds = args.stall_timeout;
    const char *repair_name = args.repair;
    const char *report_name = args.report;
    char *summary_name = const_cast<char *>(args.summary);
    const std::string final_output = output_filename ? output_filename : "";

    brlcad::step::RepairMode repair = brlcad::step::RepairMode::Safe;
    if (repair_name && BU_STR_EQUAL(repair_name, "none")) repair = brlcad::step::RepairMode::None;
    else if (repair_name && !BU_STR_EQUAL(repair_name, "safe")) {
	bu_log("ERROR: --repair must be 'none' or 'safe'\n");
	return 2;
    }
    if (!overwrite && !dry_run && bu_file_exists(final_output.c_str(), NULL)) {
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
    import_options.budget_scale = budget_scale;
    import_options.item_budget_milliseconds =
	seconds_to_milliseconds(item_budget_seconds);
    import_options.stall_timeout_milliseconds =
	seconds_to_milliseconds(stall_timeout_seconds);
    import_options.disable_item_budgets = no_item_budget != 0;
    import_options.repair = repair;
    import_options.requested_invalid_brep_policy = reject_invalid_objs ?
	brlcad::step::InvalidBrepPolicy::Reject :
	brlcad::step::InvalidBrepPolicy::Preserve;
    import_options.effective_invalid_brep_policy =
	(strict || reject_invalid_objs) ? brlcad::step::InvalidBrepPolicy::Reject :
	brlcad::step::InvalidBrepPolicy::Preserve;
    import_options.exact = exact != 0;
    import_options.strict = strict != 0;
    import_options.verbose = verbose != 0;
    import_options.dry_run = dry_run != 0;
    import_options.skip_open_shells = skip_open_shells != 0;
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
    wrapper.printLoadStatistics();

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
		if (!property.valid) continue;
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
	    if (!property.valid) continue;
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
    } else if (wrapper.Statistics().outcome == "empty") {
	status = STEP_PLUGIN_STATUS_EMPTY;
    } else if (wrapper.Statistics().geometry_written == 0 ||
	    wrapper.Statistics().outcome == "failed") {
	status = 3;
    } else if (wrapper.Statistics().outcome == "partial") {
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
