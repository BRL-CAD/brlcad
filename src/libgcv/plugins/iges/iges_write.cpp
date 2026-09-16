/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>

#include "brep.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"

#include "iges_export.h"
#include "iges_plugin.h"

namespace {
using namespace brlcad::iges;

struct IgesWriteOptions {
    int faceted = 0;
    int trimmed = 0;
    int nurbs = 0;
    int flatten = 0;
};

void
iges_create_options(struct bu_opt_desc **descriptions, void **options_data)
{
    auto options = std::make_unique<IgesWriteOptions>();
    struct bu_opt_desc descriptors[] = {
	{"", "faceted", "", nullptr, &options->faceted, "tessellate regions to faceted B-Reps"},
	{"", "trimmed-surfaces", "", nullptr, &options->trimmed, "tessellate regions to trimmed surfaces"},
	{"", "nurbs", "", nullptr, &options->nurbs, "write NMG facets using NURBS geometry"},
	{"", "flatten-brep", "", nullptr, &options->flatten, "write exact B-Reps as grouped trimmed surfaces"},
	BU_OPT_DESC_NULL
    };
    *descriptions = static_cast<struct bu_opt_desc *>(bu_malloc(sizeof(descriptors), "IGES writer options"));
    std::copy_n(descriptors, sizeof(descriptors) / sizeof(descriptors[0]), *descriptions);
    *options_data = options.release();
}

void
iges_free_options(void *options_data)
{
    delete static_cast<IgesWriteOptions *>(options_data);
}

int
write_document(struct db_i *database, const struct gcv_opts &gcv_options,
    const IgesWriteOptions &writer_options, const char *destination)
{
    validate_plugin_options(gcv_options);
    if (writer_options.faceted && writer_options.trimmed)
	throw std::invalid_argument("faceted and trimmed-surface output are mutually exclusive");
    ExportOptions options;
    options.mode = writer_options.faceted ? ExportOptions::Mode::Faceted :
	writer_options.trimmed ? ExportOptions::Mode::Trimmed : ExportOptions::Mode::Csg;
    options.flatten_brep = writer_options.flatten != 0;
    options.nurbs_facets = writer_options.nurbs != 0;
    options.verbose = gcv_options.verbosity_level != 0;
    options.tolerance = gcv_options.calculational_tolerance;
    options.tessellation = gcv_options.tessellation_tolerance;
    validate_export_options(options);
    std::unique_ptr<ProgressReporter> progress;
    if (options.verbose) {
	progress = std::make_unique<ProgressReporter>();
	options.progress = [&](const char *stage, const char *activity, size_t completed, size_t total) {
	    progress->update(stage, activity, completed, total);
	};
	if (gcv_options.max_cpus > 1)
	    bu_log("IGES: serializing conversion to protect NMG recovery and record ordering\n");
    }
    const std::string source = database->dbi_filename ? database->dbi_filename : "";
    validate_paths(!source.empty() && std::filesystem::is_regular_file(source) ? source : "", {destination});
    std::vector<std::string> roots;
    for (size_t index = 0; index < gcv_options.num_objects; ++index)
	roots.emplace_back(gcv_options.object_names[index]);
    ON::Begin();
    Writer writer(options, std::set<std::string>(roots.begin(), roots.end()));
    const bool wrote = export_objects(writer, database, roots);
    StagedFile staged(destination);
    File output(std::fopen(staged.path().c_str(), "wb"));
    if (!output)
	throw std::runtime_error("cannot create IGES output");
    writer.finish(output.get(), source, destination);
    close_file(output);
    staged.publish();
    if (options.verbose)
	writer.print_statistics();
    return wrote && !writer.omissions() ? 1 : 0;
}

int
iges_write(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *options_data, const char *destination)
{
    if (!context || !context->dbip || !gcv_options || !options_data || !destination)
	return 0;
    try {
	return write_document(context->dbip, *gcv_options,
	    *static_cast<const IgesWriteOptions *>(options_data), destination);
    } catch (const std::exception &error) {
	bu_log("IGES writer: %s\n", error.what());
    } catch (...) {
	bu_log("IGES writer: unexpected conversion failure\n");
    }
    return 0;
}
} // namespace

const struct gcv_filter iges_writer = {
    "IGES Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_IGES, nullptr,
    iges_create_options, iges_free_options, iges_write
};

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
