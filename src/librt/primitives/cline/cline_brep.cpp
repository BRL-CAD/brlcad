/*                    C L I N E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file cline_brep.cpp
 *
 * Convert a FASTGEN4 CLINE element to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

extern "C" {
    int rt_cline_to_pipe(struct rt_pipe_internal *pipe, const struct rt_db_internal *ip);
    void rt_pipe_brep(ON_Brep **b, struct rt_db_internal *ip, const struct bn_tol *tol);
}

extern "C" void
rt_cline_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_cline_internal *cip;
    struct rt_pipe_internal *pip;

    RT_CK_DB_INTERNAL(ip);
    cip = (struct rt_cline_internal *)ip->idb_ptr;
    RT_CLINE_CK_MAGIC(cip);

    BU_ALLOC(pip, struct rt_pipe_internal);
    rt_cline_to_pipe(pip, ip);

    struct rt_db_internal tmp_internal;
    RT_DB_INTERNAL_INIT(&tmp_internal);
    tmp_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    tmp_internal.idb_ptr = (void *)pip;
    tmp_internal.idb_minor_type = ID_PIPE;
    tmp_internal.idb_meth = &OBJ[ID_PIPE];
    rt_pipe_brep(b, &tmp_internal, tol);

    bu_free(pip, "pipe internal");
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
