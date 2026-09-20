/*                   G Q A _ A N A L Y Z E . C P P
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

#include "common.h"

#include "analyze/gqa.h"
#include "bu/log.h"
#include "ged.h"
#include "../ged_private.h"


static int
load_densities(struct analyze_densities **densities, char **source,
    const char *filename, void *data)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    return _ged_read_densities(densities, source, gedp, filename, 0) ==
        BRLCAD_OK ? ANALYZE_OK : ANALYZE_ERROR;
}


static void
report_progress(const char *message, void *UNUSED(data))
{
    bu_log("%s\n", message);
}


extern "C" int
ged_gqa_analyze(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    struct analyze_gqa_context context = {
        gedp->dbip, gedp->ged_result_str, load_densities, gedp,
        report_progress, NULL};
    return analyze_gqa(&context, argc, argv) == ANALYZE_OK ?
        BRLCAD_OK : BRLCAD_ERROR;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
