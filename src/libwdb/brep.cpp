/*                          B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 1987-2011 United States Government as represented by
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
/** @file brep.cpp
 *
 * Library for writing BREP objects into
 * MGED databases from arbitrary procedures.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "wdb.h"


/*
 * M K _ B R E P
 *
 * Create a brep in the geometry file.
 */
int
mk_brep(struct rt_wdb* file, const char* name, ON_Brep* brep)
{
    struct rt_brep_internal* bi;

    BU_ASSERT(brep != NULL);
    BU_GETSTRUCT(bi, rt_brep_internal);
    bi->magic = RT_BREP_INTERNAL_MAGIC;
    bi->brep = new ON_Brep(*brep); /* copy the users' brep */
    if (!bi->brep) {
	bu_log("mk_brep: Unable to copy BREP\n");
    }
    return wdb_export(file, name, (genptr_t)bi, ID_BREP, mk_conv2mm);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
