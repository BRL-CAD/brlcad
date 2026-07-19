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

#include <iostream>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include "bu/app.h"
#include "bu/time.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/opt.h"
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

    std::map<std::string, uint64_t> aggregated;
    for (const brlcad::step::Diagnostic &diagnostic : step.Diagnostics()) {
	std::string key = std::string(severity_name(diagnostic.severity)) + ": " +
	    diagnostic.entity_type + (diagnostic.entity_type.empty() ? "" : ": ") +
	    diagnostic.message;
	aggregated[key] += diagnostic.repeat_count;
    }
    for (const auto &entry : aggregated)
	std::cerr << entry.first << " (count=" << entry.second << ')' << std::endl;
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
    bool overwrite;
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
    int64_t elapsedtime;

    bu_setprogname(argv[0]);
    std::signal(SIGINT, record_signal);
    std::signal(SIGTERM, record_signal);

    elapsedtime = bu_gettime();

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
    static OutputFile ofile = {NULL, false};
    static bool verbose = false;
    static int dry_run = 0;
    static int help = 0;
    static int jobs = 1;
    static char *summary_log_file = (char *)NULL;
    static std::set<int64_t> selected_entity_ids;
    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"h", "help", "",       NULL,        &help,             "print help and exit"},
	{"?", "", "",           NULL,        &help,             ""},
	{"D", "dry-run", "",    NULL,        &dry_run,          "validate without writing a database"},
	{"v", "verbose", "",    NULL,        &verbose,          "report entity-level diagnostics"},
	{"f", "force", "",      NULL,        &ofile.overwrite,  "overwrite a positional or -o output"},
	{"O", "output-overwrite", "FILE", parse_opt_O, &ofile, "output file (overwrite)"},
	{"o", "output", "FILE", bu_opt_str,  &ofile.filename,   "output file"},
	{"S", "summary", "FILE", bu_opt_str, &summary_log_file, "legacy entity summary"},
	{"e", "entity", "ID[,ID...]", parse_entity_ids, &selected_entity_ids,
	    "convert only listed representation-item IDs (repeatable)"},
	{"j", "jobs", "N", bu_opt_int, &jobs, "bounded geometry worker count"},
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
    if (bu_vls_strlen(&parse_msgs) > 0 || jobs < 1) {
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
    import_options.verbose = verbose;
    import_options.requested_jobs = static_cast<unsigned int>(jobs);
    import_options.effective_jobs = static_cast<unsigned int>(jobs);
    import_options.selected_entity_ids = selected_entity_ids;
    step->SetImportOptions(import_options);

    /* load STEP file */
    if (step->load(iflnm)) {

	step->printLoadStatistics();

	elapsedtime = bu_gettime() - elapsedtime;
	{
	    struct bu_vls vls = BU_VLS_INIT_ZERO;
	    int seconds = elapsedtime / 1000000;
	    int minutes = seconds / 60;
	    int hours = minutes / 60;

	    minutes = minutes % 60;
	    seconds = seconds %60;

	    bu_vls_printf(&vls, "Load time: %02d:%02d:%02d\n", hours, minutes, seconds);
	    std::cerr << bu_vls_addr(&vls) << std::endl;
	    bu_vls_free(&vls);
	}
	elapsedtime = bu_gettime();

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
		} else if (stats.geometry_written == 0) {
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
		print_lazy_statistics(*step);
	    } else {
		std::cerr << "ERROR: unable to open temporary BRL-CAD output for [" << oflnm << "]" << std::endl;
		ret = 4;
	    }

	    delete dotg;
	}
    } else {
	ret = signal_exit_status() ? signal_exit_status() : 2;
    }
    progress_reporter.reset();
    delete step;

    elapsedtime = bu_gettime() - elapsedtime;
    {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	int seconds = elapsedtime / 1000000;
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
