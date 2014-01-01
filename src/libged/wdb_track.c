/*                         T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file libged/track.c
 *
 * Adds "tracks" to the data file given the required info
 *
 * Acknowledgements:
 * Modifications by Bob Parker (SURVICE Engineering):
 * *- adapt for use in LIBRT's database object
 * *- removed prompting for input
 * *- removed signal catching
 * *- added basename parameter
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

#include "./ged_private.h"

int
wdb_track_cmd(void *data,
	      int argc,
	      const char *argv[])
{
    struct bu_vls log_str = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = (struct rt_wdb *)data;
    int retval;

    if (argc != 15) {
	bu_log("ERROR: expecting 15 arguments\n");
	return TCL_ERROR;
    }

    retval = _ged_track(&log_str, wdbp, argv);

    if (bu_vls_strlen(&log_str) > 0) {
	bu_log("%s", bu_vls_addr(&log_str));
    }

    switch(retval) {
	case GED_OK:
	    return TCL_OK;
	case GED_ERROR:
	    return TCL_ERROR;
    }

    /* This should never happen */
    return TCL_ERROR;
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
