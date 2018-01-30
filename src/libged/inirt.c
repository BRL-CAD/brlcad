/*                         I N I R T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2018 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/inirt.c
 *
 * Improved Natalie's Interactive Ray Tracer command.
 *
 */
/** @} */

#include "common.h"
#include "analyze.h"

#include "./qray.h"
#include "./ged_private.h"

/**
 * Run one or a series of NIRT commands. In interactive mode, the
 * NIRT state "n" may be passed in to allow for re-use of previously
 * initialized NIRT instances.
 */
int
ged_inirt(struct ged *gedp, int argc, const char *argv[], NIRT *n)
{
    int nr = 0;
    NIRT *ns = n;
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);
    if (!argv) return GED_ERROR;
    if (!ns && nirt_alloc(&ns)) return GED_ERROR;
    if (nirt_init(ns, gedp->ged_wdbp->dbip)) return GED_ERROR;

    nr = nirt_exec(ns, argv[1]);

    if (nr == -1) nirt_log(gedp->ged_result_str, ns, NIRT_ERR);
    nirt_log(gedp->ged_result_str, ns, NIRT_OUT);

    /* TODO - capture segments for drawing... */

    /* Reset textual outputs and vlists, but keep scene definition active */
    nirt_clear(ns, NIRT_ALL|NIRT_OBJS|NIRT_VIEW);

    if (!n || nr == 1) nirt_destroy(ns);
    return GED_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
