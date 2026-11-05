/*                    B R E P _ C O R P U S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libbrep/tests/brep_corpus.cpp
 *
 * Component-level CSG-to-B-Rep corpus evaluation and semantic validation.
 */

#include "common.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dbghelp.h>
#endif

#include "bu/app.h"
#include "bu/datetime.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "brep.h"
#include "brep/boolean.h"
#include "boolean_manifold.h"
#include "raytrace.h"
#include "rt/comb.h"
#include "wdb.h"


namespace {

constexpr size_t DEFAULT_SEMANTIC_RAYS = 4096;
constexpr int DEFAULT_COMPONENT_TIMEOUT_MS = 60000;
constexpr double PARTITION_ERROR_LIMIT = 0.01;
constexpr double CHORD_ERROR_LIMIT = 0.01;
constexpr double ESTIMATE_ERROR_LIMIT = 0.05;
constexpr double ENDPOINT_RELATIVE_TOLERANCE = 1.0e-7;
constexpr double ENDPOINT_MODEL_TOLERANCE_SCALE = 4.0;
constexpr double ENDPOINT_RETRY_MARGIN = 1.25;
constexpr double SEMANTIC_RETRY_TIME_MARGIN = 1.25;
constexpr size_t SEMANTIC_RETRY_MULTIPLIER = 4;
const char *CANDIDATE_OBJECT = "brep_corpus_candidate";


struct Options {
    std::string executable;
    std::string database;
    std::string database_directory;
    std::string component;
    std::string output = "brep_corpus.csv";
    std::string result_file;
    size_t rays = DEFAULT_SEMANTIC_RAYS;
    size_t max_rays = 0;
    size_t max_components = 0;
    int timeout_ms = DEFAULT_COMPONENT_TIMEOUT_MS;
    bool child = false;
    bool classification_self_test = false;
    bool manifold_shadow = false;
    bool require_semantic_pass = false;
    std::string required_failure_stage;
    std::string required_failure_reason;
    enum rt_brep_conversion_fidelity required_fidelity =
	RT_BREP_CONVERSION_FIDELITY_UNKNOWN;
};


struct Component {
    std::string name;
    bool region = false;
};


struct ComponentResult {
    std::string database;
    std::string component;
    bool region = false;
    std::string outcome = "harness_error";
    int conversion_status = BREP_BOOLEAN_INTERNAL_ERROR;
    std::string conversion_description;
    struct rt_brep_conversion_info conversion_info =
	RT_BREP_CONVERSION_INFO_INIT;
    double conversion_ms = 0.0;
    double semantic_ms = 0.0;
    double total_ms = 0.0;
    int faces = 0;
    bool valid = false;
    bool solid = false;
    std::string semantic_status = "unavailable";
    struct rt_crofton_comparison comparison = RT_CROFTON_COMPARISON_INIT;
    BooleanManifoldShadowMetrics manifold_metrics;
    std::string detail;
};


class LogSilencer {
public:
    explicit LogSilencer(const std::string &path)
	: stream(std::fopen(path.c_str(), "wb")), written(0)
    {
	bu_log_hook_save_all(&saved_hooks);
	bu_log_hook_delete_all();
	bu_log_add_hook(capture, this);
    }

    ~LogSilencer()
    {
	bu_log_hook_delete_all();
	bu_log_hook_restore_all(&saved_hooks);
	bu_hook_delete_all(&saved_hooks);
	if (stream)
	    std::fclose(stream);
    }

private:
    static int
    capture(void *data, void *message)
    {
	LogSilencer *capture = static_cast<LogSilencer *>(data);
	if (!capture || !capture->stream || !message ||
	    capture->written >= MAX_CAPTURE_BYTES)
	{
	    return 0;
	}
	const char *text = static_cast<const char *>(message);
	const size_t available = MAX_CAPTURE_BYTES - capture->written;
	const size_t length = std::min(std::strlen(text), available);
	if (length) {
	    capture->written += std::fwrite(text, 1, length, capture->stream);
	    std::fflush(capture->stream);
	}
	return 0;
    }

    /* DEBUG_BREP_BOOLEAN can emit more than one MiB before a late tree node
     * fails.  Preserve enough of the child log to retain the terminal
     * diagnostic while keeping corpus failures bounded. */
    static constexpr size_t MAX_CAPTURE_BYTES = 16 * 1024 * 1024;
    FILE *stream;
    size_t written;
    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
};


static void
usage(const char *program)
{
    std::fprintf(stderr,
	"Usage:\n"
	"  %s --database file.g [--component name] [options]\n"
	"  %s --database-dir share/db [options]\n\n"
	"Options:\n"
	"  --output file.csv          report path (default brep_corpus.csv)\n"
	"  --rays count               deterministic semantic rays per component\n"
	"  --max-rays count           retry borderline endpoint percentiles up to\n"
	"                             this many rays (default: no retry)\n"
	"  --timeout-ms milliseconds  child timeout per component\n"
	"  --max-components count     stop after this many components\n"
	"  --require-semantic-pass    return failure if any selected component\n"
	"                             does not produce an accepted valid B-Rep\n"
	"  --require-fidelity value   require exact or approximate conversion\n"
	"  --require-failure-stage s require the detailed Boolean failure stage\n"
	"  --require-failure-reason r require the detailed Boolean failure reason\n"
	"  --manifold-shadow          collect Manifold shadow-oracle metrics\n"
	"  --classification-self-test test result-retention policy only\n",
	program, program);
}


static bool
parse_size(const char *text, size_t &value)
{
    if (!text || !text[0] || text[0] == '-')
	return false;
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno || !end || end[0] != '\0' || parsed == 0 ||
	parsed > std::numeric_limits<size_t>::max())
    {
	return false;
    }
    value = static_cast<size_t>(parsed);
    return true;
}


static bool
parse_timeout(const char *text, int &value)
{
    size_t parsed = 0;
    if (!parse_size(text, parsed) ||
	parsed > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
	return false;
    }
    value = static_cast<int>(parsed);
    return true;
}


static bool
parse_options(int argc, char **argv, Options &options)
{
    options.executable = argv[0];
    for (int i = 1; i < argc; ++i) {
	const std::string argument(argv[i]);
	auto value = [&](std::string &destination) {
	    if (i + 1 >= argc)
		return false;
	    destination = argv[++i];
	    return true;
	};
	if (argument == "--database") {
	    if (!value(options.database))
		return false;
	} else if (argument == "--database-dir") {
	    if (!value(options.database_directory))
		return false;
	} else if (argument == "--component") {
	    if (!value(options.component))
		return false;
	} else if (argument == "--output") {
	    if (!value(options.output))
		return false;
	} else if (argument == "--result-file") {
	    if (!value(options.result_file))
		return false;
	} else if (argument == "--rays") {
	    if (i + 1 >= argc || !parse_size(argv[++i], options.rays))
		return false;
	} else if (argument == "--max-rays") {
	    if (i + 1 >= argc || !parse_size(argv[++i], options.max_rays))
		return false;
	} else if (argument == "--timeout-ms") {
	    if (i + 1 >= argc || !parse_timeout(argv[++i], options.timeout_ms))
		return false;
	} else if (argument == "--max-components") {
	    if (i + 1 >= argc ||
		!parse_size(argv[++i], options.max_components))
	    {
		return false;
	    }
	} else if (argument == "--child") {
	    options.child = true;
	} else if (argument == "--classification-self-test") {
	    options.classification_self_test = true;
	} else if (argument == "--manifold-shadow") {
	    options.manifold_shadow = true;
	} else if (argument == "--require-semantic-pass") {
	    options.require_semantic_pass = true;
	} else if (argument == "--require-fidelity") {
	    std::string fidelity;
	    if (!value(fidelity))
		return false;
	    if (fidelity == "exact") {
		options.required_fidelity = RT_BREP_CONVERSION_FIDELITY_EXACT;
	    } else if (fidelity == "approximate") {
		options.required_fidelity =
		    RT_BREP_CONVERSION_FIDELITY_APPROXIMATE;
	    } else {
		return false;
	    }
	} else if (argument == "--require-failure-stage") {
	    if (!value(options.required_failure_stage) ||
		options.required_failure_stage.empty())
	    {
		return false;
	    }
	} else if (argument == "--require-failure-reason") {
	    if (!value(options.required_failure_reason) ||
		options.required_failure_reason.empty())
	    {
		return false;
	    }
	} else if (argument == "--help" || argument == "-h") {
	    usage(argv[0]);
	    std::exit(0);
	} else {
	    return false;
	}
    }

    if (options.classification_self_test) {
	return argc == 2;
    }
    if (!options.max_rays)
	options.max_rays = options.rays;
    if (options.max_rays < options.rays)
	return false;
    if (options.child) {
	return !options.database.empty() && !options.component.empty() &&
	    !options.result_file.empty() && options.database_directory.empty();
    }
    return options.database.empty() != options.database_directory.empty();
}


static std::string
join_path(const std::string &directory, const std::string &name)
{
    if (directory.empty())
	return name;
    const char last = directory[directory.size() - 1];
    if (last == '/' || last == '\\')
	return directory + name;
    return directory + BU_DIR_SEPARATOR + name;
}


static std::string
csv_escape(const std::string &value)
{
    if (value.find_first_of(",\"\r\n") == std::string::npos)
	return value;
    std::string escaped("\"");
    for (char character : value) {
	if (character == '\"')
	    escaped += '\"';
	escaped += character;
    }
    escaped += '\"';
    return escaped;
}


static std::string
metric(double value)
{
    if (std::isnan(value))
	return "nan";
    if (std::isinf(value))
	return value < 0.0 ? "-inf" : "inf";
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}


static const char *
conversion_fidelity_name(enum rt_brep_conversion_fidelity fidelity)
{
    switch (fidelity) {
	case RT_BREP_CONVERSION_FIDELITY_EXACT:
	    return "exact";
	case RT_BREP_CONVERSION_FIDELITY_APPROXIMATE:
	    return "approximate";
	case RT_BREP_CONVERSION_FIDELITY_UNKNOWN:
	default:
	    return "unknown";
    }
}


static std::string
result_csv(const ComponentResult &result)
{
    std::ostringstream output;
    output << csv_escape(result.database) << ','
	<< csv_escape(result.component) << ','
	<< (result.region ? 1 : 0) << ','
	<< csv_escape(result.outcome) << ','
	<< result.conversion_status << ','
	<< csv_escape(result.conversion_description) << ','
	<< conversion_fidelity_name(result.conversion_info.fidelity) << ','
	<< metric(result.conversion_info.declared_relative_tolerance) << ','
	<< result.conversion_info.approximation_flags << ','
	<< result.conversion_info.regularization_attempts << ','
	<< result.conversion_info.whole_tree_regularization_attempts << ','
	<< result.conversion_info.repaired_arb_count << ','
	<< result.conversion_info.initial_status << ','
	<< rt_brep_failure_stage_description(
	    result.conversion_info.failure.stage) << ','
	<< rt_brep_failure_reason_description(
	    result.conversion_info.failure.reason) << ','
	<< result.conversion_info.failure.status << ','
	<< result.conversion_info.failure.operation << ','
	<< result.conversion_info.failure.regularized << ','
	<< metric(result.conversion_info.failure.regularization_scale) << ','
	<< result.conversion_info.failure.left_faces << ','
	<< result.conversion_info.failure.left_edges << ','
	<< result.conversion_info.failure.right_faces << ','
	<< result.conversion_info.failure.right_edges << ','
	<< result.conversion_info.failure.output_faces << ','
	<< result.conversion_info.failure.output_edges << ','
	<< result.conversion_info.failure.output_naked_edges << ','
	<< csv_escape(result.conversion_info.failure.tree_path) << ','
	<< csv_escape(result.conversion_info.failure.object_name) << ','
	<< csv_escape(result.conversion_info.failure.snapshot_name) << ','
	<< ON_BooleanFailureStageDescription(
	    result.conversion_info.failure.boolean_failure.stage) << ','
	<< ON_BooleanFailureReasonDescription(
	    result.conversion_info.failure.boolean_failure.reason) << ','
	<< result.conversion_info.failure.boolean_failure.face_index << ','
	<< result.conversion_info.failure.boolean_failure.peer_face_index << ','
	<< result.conversion_info.failure.boolean_failure.edge_index << ','
	<< result.conversion_info.failure.boolean_failure.peer_edge_index << ','
	<< result.conversion_info.failure.boolean_failure.intersection_candidates
	<< ','
	<< result.conversion_info.failure.boolean_failure.intersection_events
	<< ','
	<< result.conversion_info.failure.boolean_failure.projection_failures
	<< ','
	<< result.conversion_info.failure.boolean_failure.output_naked_edges
	<< ','
	<< result.conversion_info.failure.boolean_failure.output_nonmanifold_edges
	<< ','
	<< result.conversion_info.failure.boolean_failure.ambiguous_edge_matches
	<< ','
	<< metric(result.conversion_info.failure.boolean_failure.tolerance) << ','
	<< result.manifold_metrics.attempted_operations << ','
	<< result.manifold_metrics.classified_operations << ','
	<< result.manifold_metrics.abstained_operations << ','
	<< result.manifold_metrics.coplanar_operations << ','
	<< result.manifold_metrics.coplanar_classified_operations << ','
	<< result.manifold_metrics.oracle_decisions << ','
	<< result.manifold_metrics.decisions << ','
	<< result.manifold_metrics.region_abstentions << ','
	<< result.manifold_metrics.uncompared_decisions << ','
	<< result.manifold_metrics.coplanar_region_abstentions << ','
	<< result.manifold_metrics.uncertified_region_abstentions << ','
	<< result.manifold_metrics.selection_agreements << ','
	<< result.manifold_metrics.selection_disagreements << ','
	<< result.manifold_metrics.orientation_agreements << ','
	<< result.manifold_metrics.orientation_disagreements << ','
	<< result.manifold_metrics.certified_shared_boundaries << ','
	<< result.manifold_metrics.input_triangles << ','
	<< result.manifold_metrics.output_triangles << ','
	<< metric(result.conversion_ms) << ','
	<< metric(result.semantic_ms) << ','
	<< metric(result.total_ms) << ','
	<< result.faces << ','
	<< (result.valid ? 1 : 0) << ','
	<< (result.solid ? 1 : 0) << ','
	<< csv_escape(result.semantic_status) << ','
	<< result.comparison.ray_count << ','
	<< result.comparison.source_segment_count << ','
	<< result.comparison.candidate_segment_count << ','
	<< result.comparison.source_numerically_degenerate_segments << ','
	<< result.comparison.candidate_numerically_degenerate_segments << ','
	<< result.comparison.numerically_degenerate_rays << ','
	<< result.comparison.sub_tolerance_partition_mismatch_rays << ','
	<< result.comparison.material_partition_mismatch_rays << ','
	<< metric(result.comparison.source_max_numerically_degenerate_length) << ','
	<< metric(result.comparison.candidate_max_numerically_degenerate_length) << ','
	<< metric(result.comparison.maximum_partition_disagreement) << ','
	<< metric(result.comparison.maximum_normal_partition_disagreement) << ','
	<< metric(result.comparison.partition_mismatch_fraction) << ','
	<< metric(result.comparison.endpoint_error_p95) << ','
	<< metric(result.comparison.endpoint_error_p99) << ','
	<< metric(result.comparison.endpoint_error_max) << ','
	<< metric(result.comparison.source_chord) << ','
	<< metric(result.comparison.chord_disagreement) << ','
	<< metric(result.comparison.chord_error_fraction) << ','
	<< metric(result.comparison.surface_area_error_fraction) << ','
	<< metric(result.comparison.volume_error_fraction) << ','
	<< csv_escape(result.detail) << '\n';
    return output.str();
}


static bool
write_text(const std::string &path, const std::string &text)
{
    FILE *stream = std::fopen(path.c_str(), "wb");
    if (!stream)
	return false;
    const bool written = std::fwrite(text.data(), 1, text.size(), stream) ==
	text.size();
    return std::fclose(stream) == 0 && written;
}


static std::string
read_text(const std::string &path)
{
    FILE *stream = std::fopen(path.c_str(), "rb");
    if (!stream)
	return std::string();
    std::string text;
    char buffer[4096];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), stream)) > 0)
	text.append(buffer, count);
    std::fclose(stream);
    return text;
}


static std::string
diagnostic_path(const Options &options, const char *suffix)
{
    return options.result_file + suffix;
}


static void
record_stage(const Options &options, const char *stage)
{
    if (!options.result_file.empty())
	(void)write_text(diagnostic_path(options, ".stage"), stage);
}


#ifdef _WIN32
static std::string windows_crash_report_path;


static void
write_windows_stack_frame(FILE *stream, HANDLE process, DWORD64 address,
    unsigned int frame_index)
{
    MEMORY_BASIC_INFORMATION memory = {};
    char module_path[MAX_PATH] = {};
    DWORD64 module_base = 0;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &memory,
	    sizeof(memory)) == sizeof(memory))
    {
	module_base = reinterpret_cast<DWORD64>(memory.AllocationBase);
	(void)GetModuleFileNameA(reinterpret_cast<HMODULE>(memory.AllocationBase),
	    module_path, MAX_PATH);
    }
    char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
    SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);
    DWORD line_displacement = 0;
    if (SymFromAddr(process, address, &displacement, symbol)) {
	if (SymGetLineFromAddr64(process, address, &line_displacement, &line)) {
	    std::fprintf(stream, "  #%u 0x%llx %s+0x%llx (%s:%lu) [%s+0x%llx]\n",
		frame_index, static_cast<unsigned long long>(address),
		symbol->Name, static_cast<unsigned long long>(displacement),
		line.FileName, static_cast<unsigned long>(line.LineNumber),
		module_path, static_cast<unsigned long long>(address - module_base));
	} else {
	    std::fprintf(stream, "  #%u 0x%llx %s+0x%llx [%s+0x%llx]\n", frame_index,
		static_cast<unsigned long long>(address), symbol->Name,
		static_cast<unsigned long long>(displacement), module_path,
		static_cast<unsigned long long>(address - module_base));
	}
    } else {
	std::fprintf(stream, "  #%u 0x%llx [%s+0x%llx]\n", frame_index,
	    static_cast<unsigned long long>(address), module_path,
	    static_cast<unsigned long long>(address - module_base));
    }
}


static LONG WINAPI
corpus_exception_filter(EXCEPTION_POINTERS *exception)
{
    FILE *stream = windows_crash_report_path.empty() ? NULL :
	std::fopen(windows_crash_report_path.c_str(), "wb");
    if (!stream || !exception || !exception->ExceptionRecord ||
	!exception->ContextRecord)
    {
	if (stream)
	    std::fclose(stream);
	return EXCEPTION_EXECUTE_HANDLER;
    }

    std::fprintf(stream, "Windows exception 0x%08lx at 0x%p\n",
	static_cast<unsigned long>(exception->ExceptionRecord->ExceptionCode),
	exception->ExceptionRecord->ExceptionAddress);
    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
	exception->ExceptionRecord->NumberParameters >= 2)
    {
	const ULONG_PTR operation = exception->ExceptionRecord->ExceptionInformation[0];
	const char *operation_name = operation == 0 ? "read" :
	    (operation == 1 ? "write" : (operation == 8 ? "execute" : "access"));
	std::fprintf(stream, "Access violation attempting to %s 0x%llx\n",
	    operation_name, static_cast<unsigned long long>(
		exception->ExceptionRecord->ExceptionInformation[1]));
    }

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(process, NULL, TRUE)) {
	std::fprintf(stream, "SymInitialize failed with %lu\n",
	    static_cast<unsigned long>(GetLastError()));
	std::fclose(stream);
	return EXCEPTION_EXECUTE_HANDLER;
    }

    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame = {};
    DWORD machine = 0;
#  if defined(_M_X64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rsp;
    frame.AddrStack.Offset = context.Rsp;
#  elif defined(_M_IX86)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#  elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context.Pc;
    frame.AddrFrame.Offset = context.Fp;
    frame.AddrStack.Offset = context.Sp;
#  endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    if (machine != 0) {
	DWORD64 previous_address = 0;
	for (unsigned int frame_index = 0; frame_index < 64; ++frame_index) {
	    if (!frame.AddrPC.Offset || frame.AddrPC.Offset == previous_address)
		break;
	    write_windows_stack_frame(stream, process, frame.AddrPC.Offset,
		frame_index);
	    previous_address = frame.AddrPC.Offset;
	    if (!StackWalk64(machine, process, GetCurrentThread(), &frame,
		    &context, NULL, SymFunctionTableAccess64,
		    SymGetModuleBase64, NULL))
	    {
		break;
	    }
	}
    }
    SymCleanup(process);
    std::fclose(stream);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif


static bool
append_file(FILE *output, const std::string &path)
{
    FILE *input = std::fopen(path.c_str(), "rb");
    if (!input)
	return false;
    char buffer[4096];
    bool success = true;
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), input)) > 0) {
	if (std::fwrite(buffer, 1, count, output) != count) {
	    success = false;
	    break;
	}
    }
    if (std::ferror(input))
	success = false;
    std::fclose(input);
    return success;
}


static bool
enumerate_components(const std::string &database,
    std::vector<Component> &components, std::string &detail)
{
    struct db_i *dbip = db_open(database.c_str(), DB_OPEN_READONLY);
    if (!dbip) {
	detail = "unable to open database";
	return false;
    }
    if (db_dirbuild(dbip) < 0) {
	detail = "unable to build database directory";
	db_close(dbip);
	return false;
    }
    db_update_nref(dbip);

    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, dbip) {
	if (!(entry->d_flags & RT_DIR_COMB) ||
	    (entry->d_flags & RT_DIR_HIDDEN))
	{
	    continue;
	}
	Component component;
	component.name = entry->d_namep;
	component.region = (entry->d_flags & RT_DIR_REGION) != 0;
	components.push_back(component);
    } FOR_ALL_DIRECTORY_END;
    db_close(dbip);
    std::sort(components.begin(), components.end(),
	[](const Component &left, const Component &right) {
	    return left.name < right.name;
	});
    return true;
}


static struct rt_i *
prepare_object(struct db_i *dbip, const char *name)
{
    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return NULL;
    if (rt_gettree(rtip, name) != 0) {
	rt_i_destroy(rtip);
	return NULL;
    }
    rt_prep_parallel(rtip, 1);
    if (!rtip->stats.nsolids || !rtip->stats.nregions) {
	rt_i_destroy(rtip);
	return NULL;
    }
    return rtip;
}


static bool
prepared_bounds(point_t minimum, point_t maximum, struct rt_i *rtip)
{
    VSETALL(minimum, MAX_FASTF);
    VSETALL(maximum, -MAX_FASTF);
    struct soltab *solid;
    RT_VISIT_ALL_SOLTABS_START(solid, rtip) {
	VMIN(minimum, solid->st_min);
	VMAX(maximum, solid->st_max);
    } RT_VISIT_ALL_SOLTABS_END;
    return minimum[X] < MAX_FASTF;
}


static void
delete_conversion(ON_Brep *converted, ON_Brep *owner)
{
    if (converted && converted != owner)
	delete converted;
    delete owner;
}


static bool
semantic_non_endpoint_metrics_pass(
    const struct rt_crofton_comparison &comparison)
{
    return comparison.ray_count > 0 &&
	comparison.partition_mismatch_fraction <= PARTITION_ERROR_LIMIT &&
	comparison.chord_error_fraction <= CHORD_ERROR_LIMIT &&
	comparison.surface_area_error_fraction <= ESTIMATE_ERROR_LIMIT &&
	comparison.volume_error_fraction <= ESTIMATE_ERROR_LIMIT;
}


static bool
semantic_passes(const struct rt_crofton_comparison &comparison,
    double endpoint_tolerance)
{
    return semantic_non_endpoint_metrics_pass(comparison) &&
	comparison.endpoint_error_p95 <= endpoint_tolerance &&
	comparison.endpoint_error_p99 <= 2.0 * endpoint_tolerance;
}


static bool
borderline_endpoint_percentile_failure(
    const struct rt_crofton_comparison &comparison,
    double endpoint_tolerance)
{
    const bool endpoint_percentiles_pass =
	comparison.endpoint_error_p95 <= endpoint_tolerance &&
	comparison.endpoint_error_p99 <= 2.0 * endpoint_tolerance;
    return semantic_non_endpoint_metrics_pass(comparison) &&
	!endpoint_percentiles_pass &&
	comparison.endpoint_error_p95 <=
	    ENDPOINT_RETRY_MARGIN * endpoint_tolerance &&
	comparison.endpoint_error_p99 <=
	    ENDPOINT_RETRY_MARGIN * 2.0 * endpoint_tolerance;
}


static size_t
next_semantic_ray_count(size_t current, size_t maximum)
{
    if (current >= maximum)
	return current;
    if (current > std::numeric_limits<size_t>::max() /
	    SEMANTIC_RETRY_MULTIPLIER)
    {
	return maximum;
    }
    return std::min(maximum, current * SEMANTIC_RETRY_MULTIPLIER);
}


static bool
thin_partition_divergence(const struct rt_crofton_comparison &comparison,
    double endpoint_tolerance)
{
    return comparison.ray_count > 0 &&
	comparison.partition_mismatch_rays > 0 &&
	comparison.sub_tolerance_partition_mismatch_rays ==
	    comparison.partition_mismatch_rays &&
	comparison.material_partition_mismatch_rays == 0 &&
	comparison.endpoint_error_p95 <= endpoint_tolerance &&
	comparison.endpoint_error_p99 <= 2.0 * endpoint_tolerance &&
	comparison.chord_error_fraction <= CHORD_ERROR_LIMIT &&
	comparison.volume_error_fraction <= ESTIMATE_ERROR_LIMIT;
}


static bool
thin_chord_divergence(const struct rt_crofton_comparison &comparison,
    double endpoint_tolerance)
{
    /* A tolerance-scale displacement of either boundary of a very thin
     * volume can exceed the relative chord limit even though the two ray
     * paths report identical material partitions.  Retain that result only
     * when the aggregate absolute disagreement is bounded by the two-endpoint
     * tolerance allowance across all observed source segments and the
     * aggregate estimates remain within their ordinary limits. */
    const double maximum_bounded_chord_error = 2.0 * endpoint_tolerance *
	static_cast<double>(comparison.source_segment_count);
    return comparison.ray_count > 0 && comparison.source_segment_count > 0 &&
	comparison.source_segment_count == comparison.candidate_segment_count &&
	comparison.source_hit_rays == comparison.candidate_hit_rays &&
	comparison.partition_mismatch_rays == 0 &&
	comparison.material_partition_mismatch_rays == 0 &&
	comparison.endpoint_error_p95 <= endpoint_tolerance &&
	comparison.endpoint_error_p99 <= 2.0 * endpoint_tolerance &&
	comparison.chord_error_fraction > CHORD_ERROR_LIMIT &&
	comparison.chord_disagreement <= maximum_bounded_chord_error &&
	comparison.surface_area_error_fraction <= ESTIMATE_ERROR_LIMIT &&
	comparison.volume_error_fraction <= ESTIMATE_ERROR_LIMIT;
}


static bool
accepted_semantics(const ComponentResult &result)
{
    return (result.semantic_status == "pass" &&
	(result.outcome == "success" ||
	 result.outcome == "approximate_success")) ||
	(result.valid && result.solid &&
	 result.semantic_status == "retained_with_note" &&
	 (result.outcome == "thin_partition_divergence" ||
	  result.outcome == "thin_chord_divergence"));
}


static int
run_classification_self_test()
{
    struct rt_crofton_comparison comparison = RT_CROFTON_COMPARISON_INIT;
    comparison.ray_count = 4096;
    comparison.partition_mismatch_rays = 64;
    comparison.sub_tolerance_partition_mismatch_rays = 64;
    comparison.material_partition_mismatch_rays = 0;
    comparison.endpoint_error_p95 = 0.25 * BN_TOL_DIST;
    comparison.endpoint_error_p99 = 0.5 * BN_TOL_DIST;
    comparison.chord_error_fraction = 0.001;
    comparison.volume_error_fraction = 0.001;
    if (!thin_partition_divergence(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "sub-tolerance partition divergence was rejected\n");
	return 1;
    }

    ComponentResult retained;
    retained.valid = true;
    retained.solid = true;
    retained.outcome = "thin_partition_divergence";
    retained.semantic_status = "retained_with_note";
    if (!accepted_semantics(retained)) {
	std::fprintf(stderr, "valid solid thin divergence was not retained\n");
	return 1;
    }

    comparison.material_partition_mismatch_rays = 1;
    if (thin_partition_divergence(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "material partition mismatch was retained\n");
	return 1;
    }
    retained.solid = false;
    if (accepted_semantics(retained)) {
	std::fprintf(stderr, "nonsolid thin divergence was retained\n");
	return 1;
    }
    retained.solid = true;
    retained.valid = false;
    if (accepted_semantics(retained)) {
	std::fprintf(stderr, "invalid thin divergence was retained\n");
	return 1;
    }

    const struct rt_crofton_comparison empty_comparison =
	RT_CROFTON_COMPARISON_INIT;
    comparison = empty_comparison;
    comparison.ray_count = 4096;
    comparison.source_hit_rays = 64;
    comparison.candidate_hit_rays = 64;
    comparison.source_segment_count = 64;
    comparison.candidate_segment_count = 64;
    comparison.endpoint_error_p95 = 0.25 * BN_TOL_DIST;
    comparison.endpoint_error_p99 = 0.5 * BN_TOL_DIST;
    comparison.source_chord = 0.32;
    comparison.chord_disagreement = 0.0064;
    comparison.chord_error_fraction = 0.02;
    comparison.volume_error_fraction = 0.02;
    if (!thin_chord_divergence(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "tolerance-bounded thin chord divergence was rejected\n");
	return 1;
    }
    retained.valid = true;
    retained.outcome = "thin_chord_divergence";
    if (!accepted_semantics(retained)) {
	std::fprintf(stderr, "valid solid thin chord divergence was not retained\n");
	return 1;
    }
    comparison.chord_disagreement = 0.25;
    if (thin_chord_divergence(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "unbounded chord disagreement was retained\n");
	return 1;
    }

    comparison = empty_comparison;
    comparison.ray_count = 16384;
    comparison.endpoint_error_p95 = 1.01 * BN_TOL_DIST;
    comparison.endpoint_error_p99 = 1.5 * BN_TOL_DIST;
    if (!borderline_endpoint_percentile_failure(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "borderline endpoint percentile was not retried\n");
	return 1;
    }
    comparison.endpoint_error_p95 = 1.26 * BN_TOL_DIST;
    if (borderline_endpoint_percentile_failure(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "material endpoint error was treated as borderline\n");
	return 1;
    }
    comparison.endpoint_error_p95 = 0.5 * BN_TOL_DIST;
    comparison.endpoint_error_p99 = 1.5 * BN_TOL_DIST;
    if (borderline_endpoint_percentile_failure(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "passing endpoint percentiles requested a retry\n");
	return 1;
    }
    comparison.endpoint_error_p99 = 2.01 * BN_TOL_DIST;
    comparison.volume_error_fraction = ESTIMATE_ERROR_LIMIT + 0.001;
    if (borderline_endpoint_percentile_failure(comparison, BN_TOL_DIST)) {
	std::fprintf(stderr, "non-endpoint semantic failure requested a retry\n");
	return 1;
    }
    if (next_semantic_ray_count(16384, 262144) != 65536 ||
	next_semantic_ray_count(65536, 262144) != 262144 ||
	next_semantic_ray_count(262144, 262144) != 262144)
    {
	std::fprintf(stderr, "adaptive semantic ray progression is invalid\n");
	return 1;
    }
    return 0;
}


static ComponentResult
evaluate_component(const Options &options)
{
    ComponentResult result;
    result.database = options.database;
    result.component = options.component;
    const int64_t total_start = bu_gettime();
    LogSilencer silence(diagnostic_path(options, ".log"));

    record_stage(options, "opening database");
    struct db_i *dbip = db_open(options.database.c_str(), DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	result.detail = "unable to open database or build its directory";
	if (dbip)
	    db_close(dbip);
	return result;
    }
    db_update_nref(dbip);
    struct directory *entry = db_lookup(dbip, options.component.c_str(),
	LOOKUP_QUIET);
    if (!entry || !(entry->d_flags & RT_DIR_COMB)) {
	result.detail = "component is missing or is not a combination";
	db_close(dbip);
	return result;
    }
    result.region = (entry->d_flags & RT_DIR_REGION) != 0;

    record_stage(options, "loading combination");
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, entry, dbip, bn_mat_identity) < 0) {
	result.detail = "unable to read combination internal";
	db_close(dbip);
	return result;
    }

    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    const int64_t conversion_start = bu_gettime();
    record_stage(options, "converting combination to B-Rep");
    if (options.manifold_shadow)
	bu_setenv("LIBBREP_BOOLEAN_MANIFOLD_SHADOW", "1", 1);
    boolean_manifold_shadow_metrics_reset();
    result.conversion_status = rt_comb_brep_status_info(&converted, &internal,
	&tolerance, dbip, &result.conversion_info);
    result.manifold_metrics = boolean_manifold_shadow_metrics();
    result.conversion_ms = static_cast<double>(bu_gettime() -
	conversion_start) / 1000.0;
    rt_db_free_internal(&internal);
    result.conversion_description = ON_BooleanStatusDescription(
	result.conversion_status);

    if (result.conversion_status == BREP_BOOLEAN_EMPTY) {
	result.outcome = "empty";
	result.semantic_status = "empty";
	delete_conversion(converted, owner);
	result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
	db_close(dbip);
	return result;
    }
    if (result.conversion_status != BREP_BOOLEAN_SUCCESS || !converted) {
	result.outcome = "conversion_failure";
	delete_conversion(converted, owner);
	result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
	db_close(dbip);
	return result;
    }

    record_stage(options, "validating converted B-Rep");
    result.faces = converted->m_F.Count();
    result.valid = converted->IsValid();
    result.solid = result.valid && converted->IsSolid();
    if (!result.valid || !result.solid) {
	result.outcome = "invalid_brep";
	result.semantic_status = "not_run";
	delete_conversion(converted, owner);
	result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
	db_close(dbip);
	return result;
    }

    record_stage(options, "preparing source CSG for semantic comparison");
    struct rt_i *source = prepare_object(dbip, options.component.c_str());
    if (!source) {
	result.outcome = "semantic_unavailable";
	result.detail = "source CSG is not raytraceable as a region hierarchy";
	delete_conversion(converted, owner);
	result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
	db_close(dbip);
	return result;
    }

    record_stage(options, "serializing candidate B-Rep");
    struct db_i *candidate_db = db_create_inmem();
    struct rt_wdb *candidate_writer = candidate_db ?
	wdb_dbopen(candidate_db, RT_WDB_TYPE_DB_INMEM) : NULL;
    if (!candidate_writer || mk_brep(candidate_writer, CANDIDATE_OBJECT,
	    converted) < 0)
    {
	result.outcome = "semantic_unavailable";
	result.detail = "unable to serialize candidate B-Rep";
	if (candidate_db)
	    db_close(candidate_db);
	rt_i_destroy(source);
	delete_conversion(converted, owner);
	result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
	db_close(dbip);
	return result;
    }
    db_update_nref(candidate_db);
    record_stage(options, "preparing candidate B-Rep for semantic comparison");
    struct rt_i *candidate = prepare_object(candidate_db, CANDIDATE_OBJECT);
    if (!candidate) {
	result.outcome = "semantic_unavailable";
	result.detail = "serialized candidate B-Rep is not raytraceable";
	db_close(candidate_db);
	rt_i_destroy(source);
	delete_conversion(converted, owner);
	result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
	db_close(dbip);
	return result;
    }

    point_t source_min;
    point_t source_max;
    double endpoint_tolerance = ENDPOINT_MODEL_TOLERANCE_SCALE * BN_TOL_DIST;
    if (prepared_bounds(source_min, source_max, source)) {
	const double model_diagonal = DIST_PNT_PNT(source_min, source_max);
	endpoint_tolerance = std::max(endpoint_tolerance,
	    ENDPOINT_RELATIVE_TOLERANCE * model_diagonal);
	if (result.conversion_info.fidelity ==
		RT_BREP_CONVERSION_FIDELITY_APPROXIMATE &&
	    result.conversion_info.declared_relative_tolerance > 0.0 &&
	    std::isfinite(
		result.conversion_info.declared_relative_tolerance))
	{
	    endpoint_tolerance = std::max(endpoint_tolerance,
		result.conversion_info.declared_relative_tolerance *
		model_diagonal);
	}
    }
    struct rt_crofton_params parameters = {};
    parameters.n_rays = options.rays;
    const int64_t semantic_start = bu_gettime();
    record_stage(options, "comparing source and candidate semantics");
    int semantic_result = rt_crofton_compare(&result.comparison,
	source, candidate, &parameters, 0, NULL, NULL);
    int64_t comparison_end = bu_gettime();
    double last_comparison_ms = static_cast<double>(comparison_end -
	semantic_start) / 1000.0;
    result.semantic_ms = last_comparison_ms;
    size_t adaptive_retry_count = 0;
    std::string adaptive_detail;
    while (semantic_result >= 0 && parameters.n_rays < options.max_rays &&
	borderline_endpoint_percentile_failure(result.comparison,
	    endpoint_tolerance))
    {
	const size_t retry_rays = next_semantic_ray_count(parameters.n_rays,
	    options.max_rays);
	if (retry_rays <= parameters.n_rays)
	    break;
	const double elapsed_ms = static_cast<double>(bu_gettime() - total_start) /
	    1000.0;
	const double timeout_reserve_ms = std::max(250.0,
	    0.05 * static_cast<double>(options.timeout_ms));
	const double estimated_retry_ms = std::max(1.0, last_comparison_ms) *
	    (static_cast<double>(retry_rays) /
	     static_cast<double>(parameters.n_rays)) *
	    SEMANTIC_RETRY_TIME_MARGIN;
	if (elapsed_ms + estimated_retry_ms + timeout_reserve_ms >=
		static_cast<double>(options.timeout_ms))
	{
	    adaptive_detail = "borderline endpoint percentile retry skipped to "
		"preserve the component timeout budget";
	    break;
	}

	parameters.n_rays = retry_rays;
	record_stage(options,
	    "retrying borderline endpoint percentiles with more rays");
	struct rt_crofton_comparison retry_comparison =
	    RT_CROFTON_COMPARISON_INIT;
	const int64_t retry_start = bu_gettime();
	const int retry_status = rt_crofton_compare(&retry_comparison, source,
	    candidate, &parameters, 0, NULL, NULL);
	comparison_end = bu_gettime();
	last_comparison_ms = static_cast<double>(comparison_end - retry_start) /
	    1000.0;
	result.semantic_ms += last_comparison_ms;
	if (retry_status < 0) {
	    adaptive_detail = "adaptive semantic retry failed; retained the last "
		"complete comparison";
	    break;
	}
	result.comparison = retry_comparison;
	++adaptive_retry_count;
    }
    rt_i_destroy(candidate);
    db_close(candidate_db);
    rt_i_destroy(source);
    delete_conversion(converted, owner);

    if (semantic_result < 0) {
	result.outcome = "semantic_unavailable";
	result.detail = "deterministic Crofton comparison failed";
    } else if (semantic_passes(result.comparison, endpoint_tolerance)) {
	result.outcome = result.conversion_info.fidelity ==
	    RT_BREP_CONVERSION_FIDELITY_APPROXIMATE ?
	    "approximate_success" : "success";
	result.semantic_status = "pass";
    } else if (thin_partition_divergence(result.comparison,
	    endpoint_tolerance))
    {
	result.outcome = "thin_partition_divergence";
	result.semantic_status = "retained_with_note";
	result.detail = "valid solid retained; partition-count disagreement is "
	    "confined to tolerance-scale shotline partitions and may reflect "
	    "raytrace partition culling rather than incorrect B-Rep volume";
    } else if (thin_chord_divergence(result.comparison, endpoint_tolerance)) {
	result.outcome = "thin_chord_divergence";
	result.semantic_status = "retained_with_note";
	result.detail = "valid solid retained; matching shotline partitions have "
	    "tolerance-bounded endpoint displacement whose relative chord error "
	    "is amplified by thin material";
    } else {
	result.outcome = "semantic_mismatch";
	result.semantic_status = "fail";
    }
    if (adaptive_retry_count) {
	std::ostringstream note;
	note << "adaptive semantic validation increased the deterministic sample "
	    << "from " << options.rays << " to " << result.comparison.ray_count
	    << " rays";
	if (!adaptive_detail.empty())
	    note << "; " << adaptive_detail;
	adaptive_detail = note.str();
    }
    if (!adaptive_detail.empty()) {
	if (!result.detail.empty())
	    result.detail += "; ";
	result.detail += adaptive_detail;
    }
    if (result.conversion_info.repaired_arb_count) {
	if (!result.detail.empty())
	    result.detail += "; ";
	result.detail += std::to_string(
	    result.conversion_info.repaired_arb_count) +
	    " near-planar ARB input(s) repaired within modeling tolerance";
    }
    if (result.conversion_info.whole_tree_regularization_attempts) {
	if (!result.detail.empty())
	    result.detail += "; ";
	result.detail += "whole-region regularization attempts=" +
	    std::to_string(
		result.conversion_info.whole_tree_regularization_attempts);
    }
    result.total_ms = static_cast<double>(bu_gettime() - total_start) / 1000.0;
    db_close(dbip);
    return result;
}


static int
run_child(const Options &options)
{
    ComponentResult result;
#ifdef _WIN32
    windows_crash_report_path = diagnostic_path(options, ".crash");
    LPTOP_LEVEL_EXCEPTION_FILTER previous_filter =
	SetUnhandledExceptionFilter(corpus_exception_filter);
#endif
    try {
	result = evaluate_component(options);
    } catch (const std::exception &error) {
	result.database = options.database;
	result.component = options.component;
	result.detail = error.what();
    } catch (...) {
	result.database = options.database;
	result.component = options.component;
	result.detail = "unexpected non-standard exception";
    }
#ifdef _WIN32
    SetUnhandledExceptionFilter(previous_filter);
#endif
    record_stage(options, "writing component result");
    if (!write_text(options.result_file, result_csv(result)))
	return 3;
    /* A thin divergence is not a measured semantic pass, but it is an
     * accepted result: both ray paths may legitimately cull tolerance-scale
     * partitions, and the valid B-Rep may preserve the literal CSG volume. */
    if ((options.require_semantic_pass && !accepted_semantics(result)) ||
	(options.required_fidelity != RT_BREP_CONVERSION_FIDELITY_UNKNOWN &&
	 result.conversion_info.fidelity != options.required_fidelity) ||
	(!options.required_failure_stage.empty() &&
	 options.required_failure_stage != ON_BooleanFailureStageDescription(
	     result.conversion_info.failure.boolean_failure.stage)) ||
	(!options.required_failure_reason.empty() &&
	 options.required_failure_reason != ON_BooleanFailureReasonDescription(
	     result.conversion_info.failure.boolean_failure.reason)))
    {
	return 2;
    }
    return 0;
}


static ComponentResult
process_failure_result(const std::string &database,
    const Component &component, const char *outcome, int process_status)
{
    ComponentResult result;
    result.database = database;
    result.component = component.name;
    result.region = component.region;
    result.outcome = outcome;
    result.conversion_status = BREP_BOOLEAN_INTERNAL_ERROR;
    result.conversion_description = "component subprocess did not complete";
    result.detail = "process status " + std::to_string(process_status);
    return result;
}


static int
run_parent(const Options &options)
{
    std::vector<std::string> databases;
    if (!options.database.empty()) {
	databases.push_back(options.database);
    } else {
	char **names = NULL;
	const size_t count = bu_file_list(options.database_directory.c_str(),
	    "*.g", &names);
	for (size_t i = 0; i < count; ++i) {
	    databases.push_back(join_path(options.database_directory, names[i]));
	    bu_free(names[i], "database filename");
	}
	bu_free(names, "database filenames");
    }
    if (databases.empty()) {
	std::fprintf(stderr, "No geometry databases were found.\n");
	return 1;
    }

    FILE *report = std::fopen(options.output.c_str(), "wb");
    if (!report) {
	std::fprintf(stderr, "Unable to open report %s.\n", options.output.c_str());
	return 1;
    }
    std::fprintf(report,
	"database,component,is_region,outcome,conversion_status,"
	"conversion_description,conversion_fidelity,"
	"declared_relative_tolerance,approximation_flags,"
	"regularization_attempts,whole_tree_regularization_attempts,"
	"repaired_arb_count,initial_conversion_status,"
	"failure_stage,failure_reason,failure_status,failure_operation,"
	"failure_regularized,failure_regularization_scale,"
	"failure_left_faces,failure_left_edges,failure_right_faces,"
	"failure_right_edges,failure_output_faces,failure_output_edges,"
	"failure_output_naked_edges,failure_tree_path,failure_object,"
	"failure_snapshot,"
	"boolean_failure_stage,boolean_failure_reason,boolean_failure_face,"
	"boolean_failure_peer_face,boolean_failure_edge,"
	"boolean_failure_peer_edge,boolean_failure_intersection_candidates,"
	"boolean_failure_intersection_events,"
	"boolean_failure_projection_failures,"
	"boolean_failure_output_naked_edges,"
	"boolean_failure_output_nonmanifold_edges,"
	"boolean_failure_ambiguous_edge_matches,boolean_failure_tolerance,"
	"manifold_attempted_operations,"
	"manifold_classified_operations,manifold_abstained_operations,"
	"manifold_coplanar_operations,manifold_coplanar_classified_operations,"
	"manifold_oracle_decisions,manifold_decisions,"
	"manifold_region_abstentions,manifold_uncompared_decisions,"
	"manifold_coplanar_region_abstentions,"
	"manifold_uncertified_region_abstentions,manifold_selection_agreements,"
	"manifold_selection_disagreements,manifold_orientation_agreements,"
	"manifold_orientation_disagreements,"
	"manifold_certified_shared_boundaries,"
	"manifold_input_triangles,manifold_output_triangles,"
	"conversion_ms,semantic_ms,total_ms,faces,valid,"
	"solid,semantic_status,ray_count,source_segment_count,"
	"candidate_segment_count,source_numerically_degenerate_segments,"
	"candidate_numerically_degenerate_segments,numerically_degenerate_rays,"
	"sub_tolerance_partition_mismatch_rays,"
	"material_partition_mismatch_rays,"
	"source_max_numerically_degenerate_length,"
	"candidate_max_numerically_degenerate_length,"
	"maximum_partition_disagreement,"
	"maximum_normal_partition_disagreement,"
	"partition_mismatch_fraction,"
	"endpoint_error_p95,endpoint_error_p99,endpoint_error_max,"
	"source_chord,chord_disagreement,chord_error_fraction,"
	"surface_area_error_fraction,volume_error_fraction,detail\n");

    const std::string temporary = options.output + ".component.tmp";
    const std::string stage_file = temporary + ".stage";
    const std::string crash_file = temporary + ".crash";
    const std::string log_file = temporary + ".log";
    size_t processed = 0;
    size_t process_failures = 0;
    for (const std::string &database : databases) {
	std::vector<Component> components;
	std::string detail;
	if (!enumerate_components(database, components, detail)) {
	    Component component;
	    ComponentResult failure = process_failure_result(database,
		component, "database_failure", -1);
	    failure.detail = detail;
	    const std::string row = result_csv(failure);
	    std::fwrite(row.data(), 1, row.size(), report);
	    ++process_failures;
	    continue;
	}
	if (!options.component.empty()) {
	    components.erase(std::remove_if(components.begin(), components.end(),
		[&options](const Component &component) {
		    return component.name != options.component;
		}), components.end());
	    if (components.empty()) {
		std::fprintf(stderr, "%s has no combination named %s.\n",
		    database.c_str(), options.component.c_str());
		++process_failures;
		continue;
	    }
	}

	for (const Component &component : components) {
	    if (options.max_components && processed >= options.max_components)
		break;
	    ++processed;
	    if (bu_file_exists(temporary.c_str(), NULL))
		bu_file_delete(temporary.c_str());
	    if (bu_file_exists(stage_file.c_str(), NULL))
		bu_file_delete(stage_file.c_str());
	    if (bu_file_exists(crash_file.c_str(), NULL))
		bu_file_delete(crash_file.c_str());
	    if (bu_file_exists(log_file.c_str(), NULL))
		bu_file_delete(log_file.c_str());

	    const std::string rays = std::to_string(options.rays);
	    const std::string max_rays = std::to_string(options.max_rays);
	    std::vector<const char *> child_arguments = {
		options.executable.c_str(), "--child", "--database",
		database.c_str(), "--component", component.name.c_str(),
		"--result-file", temporary.c_str(), "--rays", rays.c_str(),
		"--max-rays", max_rays.c_str()
	    };
	    if (options.require_semantic_pass)
		child_arguments.push_back("--require-semantic-pass");
	    if (options.manifold_shadow)
		child_arguments.push_back("--manifold-shadow");
	    if (options.required_fidelity !=
		    RT_BREP_CONVERSION_FIDELITY_UNKNOWN)
	    {
		child_arguments.push_back("--require-fidelity");
		child_arguments.push_back(conversion_fidelity_name(
		    options.required_fidelity));
	    }
	    if (!options.required_failure_stage.empty()) {
		child_arguments.push_back("--require-failure-stage");
		child_arguments.push_back(options.required_failure_stage.c_str());
	    }
	    if (!options.required_failure_reason.empty()) {
		child_arguments.push_back("--require-failure-reason");
		child_arguments.push_back(options.required_failure_reason.c_str());
	    }
	    child_arguments.push_back(NULL);

	    struct bu_process *process = NULL;
	    bu_process_create(&process, child_arguments.data(),
		BU_PROCESS_HIDE_WINDOW | BU_PROCESS_OUT_EQ_ERR);
	    int status = process ? bu_process_wait_n(&process,
		options.timeout_ms) : -1;
	    const bool have_result = bu_file_exists(temporary.c_str(), NULL);
	    if (have_result && !append_file(report, temporary)) {
		status = -1;
	    }
	    if (!have_result) {
		const char *outcome = status == ERROR_PROCESS_ABORTED ?
		    "timeout" : "process_failure";
		ComponentResult failure = process_failure_result(database,
		    component, outcome, status);
		const std::string stage = read_text(stage_file);
		const std::string crash = read_text(crash_file);
		const std::string log = read_text(log_file);
		if (!stage.empty())
		    failure.detail += "; last stage: " + stage;
		if (!crash.empty())
		    failure.detail += "; " + crash;
		if (!log.empty())
		    failure.detail += "; captured log: " + log;
		const std::string row = result_csv(failure);
		std::fwrite(row.data(), 1, row.size(), report);
	    }
	    if (status != 0)
		++process_failures;
	    if (have_result)
		bu_file_delete(temporary.c_str());
	    if (bu_file_exists(stage_file.c_str(), NULL))
		bu_file_delete(stage_file.c_str());
	    if (bu_file_exists(crash_file.c_str(), NULL))
		bu_file_delete(crash_file.c_str());
	    if (bu_file_exists(log_file.c_str(), NULL))
		bu_file_delete(log_file.c_str());
	    std::fflush(report);
	}
	if (options.max_components && processed >= options.max_components)
	    break;
    }

    std::fclose(report);
    if (bu_file_exists(temporary.c_str(), NULL))
	bu_file_delete(temporary.c_str());
    if (bu_file_exists(stage_file.c_str(), NULL))
	bu_file_delete(stage_file.c_str());
    if (bu_file_exists(crash_file.c_str(), NULL))
	bu_file_delete(crash_file.c_str());
    if (bu_file_exists(log_file.c_str(), NULL))
	bu_file_delete(log_file.c_str());
    std::printf("B-Rep corpus report: %zu components, %zu process failures, %s\n",
	processed, process_failures, options.output.c_str());
    return process_failures ? 1 : 0;
}

} // namespace


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    Options options;
    if (!parse_options(argc, argv, options)) {
	usage(argv[0]);
	return 1;
    }
    if (options.classification_self_test) {
	return run_classification_self_test();
    }
    return options.child ? run_child(options) : run_parent(options);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
