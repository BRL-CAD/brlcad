/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2008 United States Government as represented by
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
/** @file tcl.c
 *
 * Tcl interfaces to LIBGED routines.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "dg.h"

/* private headers */
#include "brlcad_version.h"


/**
 * G E D _ I N I T
 *
 * Allows LIBGED to be dynamically loade to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 * "load /usr/brlcad/lib/libbn.so"
 * "load /usr/brlcad/lib/librt.so"
 * "load /usr/brlcad/lib/libged.so"
 */
int
Ged_Init(Tcl_Interp *interp)
{

    /* initialize database objects */
    Wdb_Init(interp);
    
    /* initialize drawable geometry objects */
    Dgo_Init(interp);

    /* initialize view objects */
    Vo_Init(interp);
    
    Tcl_PkgProvide(interp,  "Ged", brlcad_version());

    return TCL_OK;
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
