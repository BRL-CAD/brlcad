/*                     S T E P - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/step-g.cpp
 *
 * C++ main() for step-g converter.
 *
 */

#include "common.h"

#include <algorithm>
#include <iostream>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#ifndef _WIN32
#  include <sys/resource.h>
#endif

#include "bu/app.h"
#include "bu/time.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "bu/process.h"
#include "bu/vls.h"

//
// step-g related headers
//
#include <BRLCADWrapper.h>
#include <STEPString.h>
#include <STEPWrapper.h>
#include <STEPProgressReporter.h>

//
// include NIST step related headers
//
#include <sdai.h>
#include <STEPfile.h>
#include "Factory.h"
#include "schema.h"

namespace {

/* Match ap214-g's bounded automatic concurrency.  Exact BREP jobs can retain
 * substantial detached topology, so an unbounded hardware-concurrency default
 * is inappropriate on large hosts; users may still select another value with
 * -j. */
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

void
print_diagnostics(const STEPWrapper &step, bool verbose)
{
    if (verbose) {
	for (const brlcad::step::Diagnostic &diagnostic : step.Diagnostics()) {
	    std::cerr << severity_name(diagnostic.severity) << ": #"
		<< diagnostic.entity_id << ' ' << diagnostic.entity_type;
	    if (!diagnostic.attribute.empty())
		std::cerr << '.' << diagnostic.attribute;
	    std::cerr << ": " << diagnostic.message;
	    if (diagnostic.repeat_count > 1)
		std::cerr << " (count=" << diagnostic.repeat_count << ')';
	    std::cerr << std::endl;
	}
	return;
    }

    struct DiagnosticSummary {
	uint64_t count = 0;
	int64_t representative_entity_id = 0;
    };
    std::map<std::string, DiagnosticSummary> aggregated;
    for (const brlcad::step::Diagnostic &diagnostic : step.Diagnostics()) {
	std::string message = diagnostic.message;
	if (diagnostic.severity == brlcad::step::DiagnosticSeverity::Warning &&
		(message.find("source curve/surface separation exceeds declared tolerance ") == 0 ||
		 message.find("source edge geometry separation exceeds declared tolerance ") == 0))
	    message = "source edge/surface geometry exceeded the declared tolerance; "
		"adjusted affected OpenNURBS edge tolerances after dense validation";
	std::string key = std::string(severity_name(diagnostic.severity)) + ": " +
	    diagnostic.entity_type + (diagnostic.entity_type.empty() ? "" : ": ") +
	    message;
	DiagnosticSummary &summary = aggregated[key];
	if (!summary.representative_entity_id && diagnostic.entity_id > 0)
	    summary.representative_entity_id = diagnostic.entity_id;
	summary.count += diagnostic.repeat_count;
    }
    for (const auto &entry : aggregated) {
	std::cerr << entry.first;
	if (entry.second.representative_entity_id > 0)
	    std::cerr << " [representative=#"
		<< entry.second.representative_entity_id << ']';
	std::cerr << " (count=" << entry.second.count << ')' << std::endl;
    }
}

void
print_lazy_statistics(const STEPWrapper &step)
{
    if (!step.HasLazyIndex()) return;
    const brlcad::step::ImportStatistics &stats = step.Statistics();
    std::cerr << "STEP lazy cache: indexed=" << stats.lazy_indexed_instances
	<< ", peak-loaded=" << stats.lazy_loaded_instances
	<< ", current-loaded=" << stats.lazy_current_loaded_instances
	<< ", current-pinned=" << stats.lazy_pinned_instances
	<< ", materializations=" << stats.lazy_materializations
	<< ", evictions=" << stats.lazy_evictions;
    if (stats.lazy_cache_bytes_available)
	std::cerr << ", peak-source-bytes=" << stats.lazy_cache_byte_high_water;
    std::cerr << std::endl;
}

void
print_skipped_items(const STEPWrapper &step, bool verbose)
{
    const brlcad::step::ImportStatistics &stats = step.Statistics();
    const uint64_t total = static_cast<uint64_t>(stats.skipped_items.size()) +
	stats.skipped_items_omitted;
    if (!total)
	return;

    std::cerr << "Skipped STEP roots (" << total << "):";
    for (const brlcad::step::SkippedItem &item : stats.skipped_items)
	std::cerr << " #" << item.entity_id << '(' << item.entity_type << ')';
    if (stats.skipped_items_omitted)
	std::cerr << " ... " << stats.skipped_items_omitted << " omitted";
    std::cerr << std::endl;
    if (!verbose)
	return;
    for (const brlcad::step::SkippedItem &item : stats.skipped_items)
	std::cerr << "  #" << item.entity_id << ' ' << item.entity_type << ": "
	    << item.reason << std::endl;
}

void
write_count_map(std::ostream &out, const std::map<std::string, uint64_t> &values)
{
    out << '{';
    bool first = true;
    for (const auto &entry : values) {
	if (!first) out << ',';
	first = false;
	out << '"' << brlcad::step::json_escape(entry.first) << "\":" << entry.second;
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

void
write_report_diagnostic(std::ostream &out,
    const brlcad::step::Diagnostic &diagnostic, bool aggregated)
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
write_report(const std::string &path, const std::string &input,
    const std::string &output, const STEPWrapper &step, int exit_status)
{
    if (path.empty()) return true;
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    const brlcad::step::Document &document = step.Document();
    const brlcad::step::ImportStatistics &stats = step.Statistics();
    const brlcad::step::ImportOptions &options = step.ImportOptions();
    out << "{\n  \"format\":\"brlcad-step-import-report-v1\","
	<< "\n  \"input\":\"" << brlcad::step::json_escape(input) << "\","
	<< "\n  \"output\":\"" << brlcad::step::json_escape(output) << "\","
	<< "\n  \"exit_status\":" << exit_status
	<< ",\n  \"options\":{\"requested_jobs\":" << options.requested_jobs
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
	<< ",\"repair\":\"" << (options.repair == brlcad::step::RepairMode::Safe ?
	    "safe" : "none") << "\",\"exact\":" << (options.exact ? "true" : "false")
	<< ",\"strict\":" << (options.strict ? "true" : "false")
	<< ",\"dry_run\":" << (options.dry_run ? "true" : "false")
	<< ",\"selected_entity_ids\":[";
    bool first_selected = true;
    for (std::set<int64_t>::const_iterator selected =
	    options.selected_entity_ids.begin(); selected !=
	    options.selected_entity_ids.end(); ++selected) {
	if (!first_selected) out << ',';
	first_selected = false;
	out << *selected;
    }
    out << "]}"
	<< ",\n  \"coverage\":{\"entity_counts\":";
    write_count_map(out, document.entity_counts);
    out << ",\"unsupported_counts\":";
    write_count_map(out, document.unsupported_counts);
    out << ",\"products\":" << stats.products
	<< ",\"occurrences\":" << stats.occurrences
	<< ",\"geometry_attempted\":" << stats.geometry_attempted
	<< ",\"geometry_written\":" << stats.geometry_written
	<< ",\"geometry_skipped\":" << stats.geometry_skipped << '}'
	<< ",\n  \"skipped_items\":[";
    for (size_t i = 0; i < stats.skipped_items.size(); ++i) {
	if (i) out << ',';
	const brlcad::step::SkippedItem &item = stats.skipped_items[i];
	out << "{\"entity_id\":" << item.entity_id << ",\"entity_type\":\""
	    << brlcad::step::json_escape(item.entity_type) << "\",\"reason\":\""
	    << brlcad::step::json_escape(item.reason) << "\"}";
    }
    out << "]"
	<< ",\n  \"skipped_items_omitted\":" << stats.skipped_items_omitted
	<< ",\n  \"validation\":{\"invalid_breps\":" << stats.invalid_breps
	<< ",\"output_failures\":" << stats.output_failures
	<< ",\"repairs\":" << stats.repairs
	<< ",\"tolerance_mm\":" << stats.tolerance_mm << '}'
	<< ",\n  \"timings_us\":{\"load\":" << stats.load_time_us
	<< ",\"convert\":" << stats.convert_time_us << '}'
	<< ",\n  \"performance\":";
    write_performance_telemetry(out, stats);
    out
	<< ",\n  \"stepcode_cache\":{\"indexed_instances\":"
	<< stats.lazy_indexed_instances << ",\"high_water_instances\":"
	<< stats.lazy_loaded_instances << ",\"current_loaded_instances\":"
	<< stats.lazy_current_loaded_instances << ",\"pinned_instances\":"
	<< stats.lazy_pinned_instances << ",\"hits\":" << stats.lazy_cache_hits
	<< ",\"misses\":" << stats.lazy_cache_misses
	<< ",\"materializations\":"
	<< stats.lazy_materializations << ",\"evictions\":" << stats.lazy_evictions
	<< ",\"active_batches\":" << stats.lazy_active_batches
	<< ",\"data_sections\":" << stats.lazy_data_sections
	<< ",\"bytes\":";
    if (stats.lazy_cache_bytes_available) out << stats.lazy_cache_bytes;
    else out << "null";
    out << ",\"high_water_bytes\":";
    if (stats.lazy_cache_bytes_available) out << stats.lazy_cache_byte_high_water;
    else out << "null";
    out << '}' << ",\n  \"peak_rss_bytes\":" << stats.peak_rss_bytes
	<< ",\n  \"diagnostics\":[";
    const std::vector<brlcad::step::Diagnostic> &diagnostics = step.Diagnostics();
    if (options.verbose) {
	for (size_t i = 0; i < diagnostics.size(); ++i) {
	    if (i) out << ',';
	    write_report_diagnostic(out, diagnostics[i], false);
	}
    } else {
	/* Keep routine reports bounded on large assemblies.  Retain the first
	 * entity as a traceable representative, sum every occurrence, and let -v
	 * retain all entity-level records. */
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
    out << "]\n}\n";
    return static_cast<bool>(out);
}

}

void
usage(const struct bu_opt_desc *options)
{
    char *description = bu_opt_describe(options, NULL);
    bu_log("Usage: step-g [options] input.stp output.g\n"
	"       step-g [options] -o output.g input.stp\n"
	"       step-g -D [options] input.stp\n\nOptions:\n%s",
	description ? description : "");
    if (description) bu_free(description, "step-g option description");
}

struct OutputFile {
    char *filename;
    /* bu_opt's argument-less flag handler stores an int.  Keep flag storage
     * ABI-compatible with that handler rather than pointing it at a bool. */
    int overwrite;
};

struct TemporaryOutput {
    std::string path;
    bool keep = false;

    ~TemporaryOutput()
    {
	if (!keep && !path.empty()) bu_file_delete(path.c_str());
    }
};

static std::string
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

static int
parse_opt_O(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    int ret;
    OutputFile *ofile = (OutputFile *)set_var;

    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "-O");

    ofile->overwrite = true;
    if (ofile) {
	ret = bu_opt_str(error_msg, argc, argv, &ofile->filename);
	return ret;
    } else {
	return -1;
    }
}

static int
parse_entity_ids(struct bu_vls *error_msg, size_t argc, const char **argv, void *set_var)
{
    BU_OPT_CHECK_ARGV0(error_msg, argc, argv, "entity ID list");
    std::set<int64_t> *entity_ids = static_cast<std::set<int64_t> *>(set_var);
    if (!entity_ids) return -1;
    std::string error;
    if (!brlcad::step::parse_entity_id_list(argv[0], *entity_ids, &error)) {
	bu_vls_printf(error_msg, "%s\n", error.c_str());
	return -1;
    }
    return 1;
}

int
main(int argc, const char *argv[])
{
    int ret = 0;
    int64_t convert_started = 0;
    int64_t convert_elapsed = 0;

    bu_setprogname(argv[0]);
    std::signal(SIGINT, record_signal);
    std::signal(SIGTERM, record_signal);

    const int64_t load_started = bu_gettime();

    /*
     * You have to initialize the schema before you do anything else.
     * This initializes all of the registry information for the schema
     * you plan to use.  The SchemaInit() function is generated by
     * fedex_plus.  See the 'extern' stmt above.
     *
     * The registry is always going to be in memory.
     */

    // InstMgr instance_list;
    // Registry registry (SchemaInit);
    // STEPfile sfile (registry, instance_list);

    // process command line arguments
    static OutputFile ofile = {NULL, 0};
    static int verbose = 0;
    static int dry_run = 0;
    static int exact = 0;
    static int strict = 0;
    static int help = 0;
    static int jobs = default_geometry_jobs();
    static fastf_t absolute_tolerance = 0.0;
    static fastf_t budget_scale = 0.0;
    static fastf_t item_budget_seconds = 0.0;
    static fastf_t stall_timeout_seconds = 0.0;
    static int no_item_budget = 0;
    static char *repair_name = NULL;
    static char *report_name = NULL;
    static char *summary_log_file = (char *)NULL;
    static std::set<int64_t> selected_entity_ids;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"h", "help", "",       NULL,        &help,             "print help and exit"},
	{"?", "", "",           NULL,        &help,             ""},
	{"D", "dry-run", "",    NULL,        &dry_run,          "validate without writing a database"},
	{"v", "verbose", "",    NULL,        &verbose,          "report entity-level diagnostics"},
	{"", "exact", "",       NULL,        &exact,            "strictly enforce the declared model tolerance"},
	{"", "strict", "",      NULL,        &strict,           "reject partial output"},
	{"", "abs-tol", "MM",   bu_opt_fastf_t, &absolute_tolerance, "absolute output tolerance"},
	{"", "budget-scale", "FACTOR", bu_opt_fastf_t, &budget_scale,
	    "manual machine-speed budget scale (default: startup calibration)"},
	{"", "item-budget", "SECONDS", bu_opt_fastf_t, &item_budget_seconds,
	    "override the ordinary per-item CPU-work budget"},
	{"", "no-item-budget", "", NULL, &no_item_budget,
	    "disable CPU-work budgets; no-progress detection remains active"},
	{"", "stall-timeout", "SECONDS", bu_opt_fastf_t, &stall_timeout_seconds,
	    "override the no-progress cancellation interval"},
	{"", "repair", "MODE",  bu_opt_str,  &repair_name,      "none or safe"},
	{"", "report", "FILE",  bu_opt_str,  &report_name,      "structured JSON report"},
	{"f", "force", "",      NULL,        &ofile.overwrite,  "overwrite a positional or -o output"},
	{"O", "output-overwrite", "FILE", parse_opt_O, &ofile, "output file (overwrite)"},
	{"o", "output", "FILE", bu_opt_str,  &ofile.filename,   "output file"},
	{"S", "summary", "FILE", bu_opt_str, &summary_log_file, "legacy entity summary"},
	{"e", "entity", "ID[,ID...]", parse_entity_ids, &selected_entity_ids,
	    "convert only listed representation-item IDs (repeatable)"},
	{"j", "jobs", "N", bu_opt_int, &jobs,
	    "bounded geometry worker count (default: up to 8 CPUs)"},
	BU_OPT_DESC_NULL
    };

    ++argv; --argc;
    argc = bu_opt_parse(&parse_msgs, argc, argv, options);

    if (bu_vls_strlen(&parse_msgs) > 0) {
	bu_log("%s\n", bu_vls_cstr(&parse_msgs));
    }
    if (help) {
	usage(options);
	bu_vls_free(&parse_msgs);
	return 0;
    }
    if (bu_vls_strlen(&parse_msgs) > 0 || jobs < 1 ||
	    absolute_tolerance < 0.0 || budget_scale < 0.0 ||
	    item_budget_seconds < 0.0 || stall_timeout_seconds < 0.0) {
	usage(options);
	bu_vls_free(&parse_msgs);
	return 2;
    }

    brlcad::step::RepairMode repair = brlcad::step::RepairMode::Safe;
    if (repair_name && BU_STR_EQUAL(repair_name, "none"))
	repair = brlcad::step::RepairMode::None;
    else if (repair_name && !BU_STR_EQUAL(repair_name, "safe")) {
	bu_log("ERROR: --repair must be 'none' or 'safe'\n");
	usage(options);
	bu_vls_free(&parse_msgs);
	return 2;
    }

    const bool option_output = !BU_STR_EMPTY(ofile.filename);
    if ((option_output && argc != 1) || (!option_output && argc != 2 && !(dry_run && argc == 1))) {
	bu_log("ERROR: specify one input and one output (or only an input with -D)\n");
	usage(options);
	bu_vls_free(&parse_msgs);
	return 2;
    }
    if (!option_output && argc == 2)
	ofile.filename = const_cast<char *>(argv[1]);
    if (dry_run && BU_STR_EMPTY(ofile.filename))
	ofile.filename = const_cast<char *>("(dry-run)");
    bu_vls_free(&parse_msgs);

    if (!ofile.overwrite && !dry_run) {
	/* check our inputs/outputs */
	if (bu_file_exists(ofile.filename, NULL)) {
	    bu_exit(2, "ERROR: refusing to overwrite existing output file:"
		        "\"%s\". Please remove the existing file, change the "
			"output file name, or use the '-O' option to force an"
			" overwrite.", ofile.filename);
	}
    }
    if (!bu_file_exists(argv[0], NULL) && !BU_STR_EQUAL(argv[0], "-")) {
	bu_exit(2, "ERROR: unable to read input \"%s\" STEP file", argv[0]);
    }

    std::string iflnm = argv[0];
    std::string oflnm = ofile.filename;

    TemporaryOutput temporary_output;
    if (!dry_run) temporary_output.path = temporary_output_name(oflnm);
    std::string working_output = dry_run ? oflnm : temporary_output.path;

    STEPWrapper *step = new STEPWrapper();
    std::unique_ptr<STEPProgressReporter> progress_reporter(new STEPProgressReporter(*step));

    step->dry_run = dry_run;
    step->summary_log_file= summary_log_file;

    step->Verbose(verbose);
    step->SetCancellationCallback([]() { return caught_signal != 0; });
    brlcad::step::ImportOptions import_options;
    import_options.dry_run = dry_run != 0;
    import_options.absolute_tolerance_mm = absolute_tolerance;
    import_options.budget_scale = budget_scale;
    import_options.item_budget_milliseconds =
	seconds_to_milliseconds(item_budget_seconds);
    import_options.stall_timeout_milliseconds =
	seconds_to_milliseconds(stall_timeout_seconds);
    import_options.disable_item_budgets = no_item_budget != 0;
    import_options.repair = repair;
    import_options.exact = exact != 0;
    import_options.strict = strict != 0;
    import_options.verbose = verbose;
    import_options.requested_jobs = static_cast<unsigned int>(jobs);
    import_options.effective_jobs = static_cast<unsigned int>(jobs);
    import_options.selected_entity_ids = selected_entity_ids;
    step->SetImportOptions(import_options);

    /* load STEP file */
    if (step->load(iflnm)) {

	step->printLoadStatistics();

	const int64_t load_elapsed = bu_gettime() - load_started;
	step->Statistics().load_time_us = load_elapsed;
	{
	    struct bu_vls vls = BU_VLS_INIT_ZERO;
	    int seconds = load_elapsed / 1000000;
	    int minutes = seconds / 60;
	    int hours = minutes / 60;

	    minutes = minutes % 60;
	    seconds = seconds %60;

	    bu_vls_printf(&vls, "Load time: %02d:%02d:%02d\n", hours, minutes, seconds);
	    std::cerr << bu_vls_addr(&vls) << std::endl;
	    bu_vls_free(&vls);
	}
	convert_started = bu_gettime();

	BRLCADWrapper *dotg  = new BRLCADWrapper();
	if (!dotg) {
	    std::cerr << "ERROR: unable to create BRL-CAD instance" << std::endl;
	    ret = 3;
	} else {

	    dotg->dry_run = dry_run;
	    std::cerr << "Writing output file [" << oflnm << "] ...";
	    if (dotg->OpenFile(working_output)) {
		const bool converted = step->convert(dotg);
		dotg->Close();
		const brlcad::step::ImportStatistics &stats = step->Statistics();
		if (signal_exit_status()) {
		    ret = signal_exit_status();
		} else if (stats.output_failures || (!converted && stats.geometry_written > 0)) {
		    ret = 4;
		} else if (stats.geometry_written == 0 ||
			(strict && stats.geometry_skipped)) {
		    ret = 3;
		} else if (stats.geometry_skipped) {
		    ret = 1;
		}
		if (!dry_run && (ret == 0 || ret == 1)) {
		    if (std::rename(working_output.c_str(), oflnm.c_str()) != 0) {
			std::cerr << "ERROR: cannot replace output file [" << oflnm
			    << "]: " << std::strerror(errno) << std::endl;
			ret = 4;
		    } else {
			temporary_output.keep = true;
		    }
		}
		if (ret == 3)
		    std::cerr << "no usable geometry was produced" << std::endl;
		else if (ret == 4)
		    std::cerr << "output failed" << std::endl;
		else if (ret == 0 || ret == 1)
		    std::cerr << "done!" << std::endl;
		else
		    std::cerr << "conversion interrupted" << std::endl;
		print_diagnostics(*step, verbose);
		print_skipped_items(*step, verbose);
		print_lazy_statistics(*step);
	    } else {
		std::cerr << "ERROR: unable to open temporary BRL-CAD output for [" << oflnm << "]" << std::endl;
		ret = 4;
	    }

	    delete dotg;
	}
    } else {
	step->Statistics().load_time_us = bu_gettime() - load_started;
	ret = signal_exit_status() ? signal_exit_status() : 2;
    }
    if (convert_started)
	convert_elapsed = bu_gettime() - convert_started;
    step->Statistics().convert_time_us = convert_elapsed;
    step->Statistics().peak_rss_bytes = peak_rss_bytes();
    progress_reporter.reset();
    const std::string report_path = report_name ? report_name : "";
    if (!write_report(report_path, iflnm, oflnm, *step, ret)) {
	std::cerr << "ERROR: unable to write report [" << report_path << "]"
	    << std::endl;
	ret = 4;
    }
    delete step;

    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	int seconds = convert_elapsed / 1000000;
	int minutes = seconds / 60;
	int hours = minutes / 60;

	minutes = minutes % 60;
	seconds = seconds %60;

	bu_vls_printf(&vls, "Convert time: %02d:%02d:%02d\n", hours, minutes, seconds);
	std::cerr << bu_vls_addr(&vls) << std::endl;
	bu_vls_free(&vls);
    }

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
