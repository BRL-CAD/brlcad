/*                       E D I T 2 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/edit2.cpp
 *
 * Testing command for experimenting with editing routines.
 *
 * Among other things, we want the edit command to be aware of
 * the GED selection state, if we have a default one set and
 * the edit command doesn't explicitly specify an object or objects
 * to operate on.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"

extern "C" int
ged_edit2_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    // We know we're the edit command
    argc--;argv++;

    // Clear results buffer
    bu_vls_trunc(gedp->ged_result_str, 0);

    // Until we get some logic in here, report we got edit2
    bu_vls_printf(gedp->ged_result_str, "called edit2\n");

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

