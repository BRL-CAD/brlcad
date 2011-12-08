/*                  C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @addtogroup pcdbio */
/** @{ */
/** @file libwdb/constraint.c
 *
 * @brief External C Routines of Parametrics and Constraints API
 *
 */

#include "common.h"

#include <stdlib.h>

#include "raytrace.h"
#include "wdb.h"


int
mk_constraint(struct rt_wdb *wdbp, const char *name, const char *UNUSED(expr))
{
    struct rt_db_internal intern;
    struct rt_constraint_internal *constraint;

    RT_CK_WDB(wdbp);

    RT_DB_INTERNAL_INIT(&intern);

    /* Create a fresh new object for export */
    BU_GET(constraint, struct rt_constraint_internal);
    constraint->magic = RT_CONSTRAINT_MAGIC;

    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_CONSTRAINT;
    intern.idb_ptr = (genptr_t)constraint;
    intern.idb_meth = &rt_functab[ID_CONSTRAINT];

    /* Add data */
    constraint->id=1432;
    constraint->type=323;

    /* The internal representation will be freed */
    return wdb_put_internal(wdbp, name, &intern, mk_conv2mm);
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
