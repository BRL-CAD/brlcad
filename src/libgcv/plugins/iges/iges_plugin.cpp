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
