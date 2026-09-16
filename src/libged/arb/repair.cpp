/*                     R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/opt.h"
#include "rt/geom.h"
#include "rt/db_internal.h"
#include "wdb.h"

#include "ged_arb.h"
#include "../ged_private.h"

extern "C" int
_arb_cmd_repair(void *bs, int argc, const char **argv)
{
    struct _ged_arb_info *gb = (struct _ged_arb_info *)bs;
    struct ged *gedp = gb->gedp;
    (void)gedp;

    int ret = ged_repair(gb->gedp, argc, argv);
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
