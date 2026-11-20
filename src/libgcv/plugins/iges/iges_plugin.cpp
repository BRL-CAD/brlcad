/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_plugin.h"

#include <cmath>
#include <stdexcept>

void
validate_plugin_options(const struct gcv_opts &options)
{
    // IGES carries its own units.  Do not silently ignore a requested rescale.
    if (!std::isfinite(options.scale_factor) || !NEAR_EQUAL(options.scale_factor, 1.0, SMALL_FASTF))
	throw std::invalid_argument("IGES uses file-defined units; a scale override is not supported");
}

namespace {
const struct gcv_filter * const FILTERS[] = {&iges_reader, &iges_writer, nullptr};
const struct gcv_plugin PLUGIN = {FILTERS};
}

extern "C" COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void)
{
    return &PLUGIN;
}

static enum bu_mime_probe_status
iges_probe(const struct bu_mime_probe_input *input, const char *)
{
    const size_t record_width = 80;
    const size_t section_column = 72;
    const size_t sequence_column = section_column + 1;
    if (input->size < record_width)
	return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    if (input->data[section_column] != 'S')
	return BU_MIME_PROBE_NO_MATCH;
    for (size_t i = 0; i < section_column; ++i)
	if (input->data[i] < ' ' || input->data[i] > '~')
	    return BU_MIME_PROBE_NO_MATCH;
    bool digit = false;
    for (size_t i = sequence_column; i < record_width; ++i) {
	if (input->data[i] >= '0' && input->data[i] <= '9')
	    digit = true;
	else if (input->data[i] != ' ')
	    return BU_MIME_PROBE_NO_MATCH;
    }
    return digit ? BU_MIME_PROBE_HEURISTIC : BU_MIME_PROBE_NO_MATCH;
}

extern "C" COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
gcv_plugin_info_v2(void)
{
    static const struct bu_mime_trait ascii[] = {BU_MIME_TRAIT_STRING("encoding", "ascii")};
    static const struct bu_mime_trait output[] = {
	BU_MIME_TRAIT_STRING("encoding", "ascii"), BU_MIME_TRAIT_STRING("version", "5.3"),
	BU_MIME_TRAIT_STRING("iges.version-code", "11")
    };
    static const struct bu_mime_capability readers[] = {
	{"model/iges", ascii, sizeof(ascii) / sizeof(ascii[0]), 0, BU_MIME_DEFER_INPUT_VALIDATION}
    };
    static const struct bu_mime_capability writers[] = {
	BU_MIME_CAPABILITY("model/iges", output, sizeof(output) / sizeof(output[0]), 0)
    };
    static const struct gcv_handler handlers[] = {
	{&iges_reader_execution, readers, sizeof(readers) / sizeof(readers[0]), BU_MIME_DEFAULT_PRIORITY, iges_probe, NULL},
	{&iges_writer_execution, writers, sizeof(writers) / sizeof(writers[0]), BU_MIME_DEFAULT_PRIORITY, NULL, NULL},
    };
    static const struct gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2),
	NULL, 0, handlers, sizeof(handlers) / sizeof(handlers[0])};
    return &plugin;
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
