/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef GCV_IGES_PLUGIN_H
#define GCV_IGES_PLUGIN_H

#include "common.h"
#include "gcv/api.h"

void validate_plugin_options(const struct gcv_opts &options);

extern const struct gcv_filter iges_reader;
extern const struct gcv_filter iges_writer;

#endif

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
