/*                   	P C _ M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file pc_main.c
 *
 * @brief External C Routines of Parametrics and Constraints API
 *
 * @author Dawn Thomas
 *
 *
 */

#include "common.h"

#include <stdlib.h>

#include "raytrace.h"
#include "wdb.h"
#include "pc.h"

/**
 * 			PC_MK_CONSTRAINT
 *
 * Given the appropriate parameters, makes the non-geometric
 * constraint object and writes it to the database using
 * wdb_put_internal.  Only supported on database version 5 or above
 *
 */
int
pc_mk_constraint(
    struct rt_wdb       *wdbp,
    const char          *name,
    const char 		*expr )
{
    struct rt_db_internal       intern;
    struct rt_constraint_internal *constraint;

    RT_CK_WDB(wdbp);

    RT_INIT_DB_INTERNAL(&intern);
    /* Create a fresh new object for export */
    BU_GETSTRUCT( constraint, rt_constraint_internal );
    constraint->magic = RT_CONSTRAINT_MAGIC;
    /* bu_vls_init( &constraint->shader );*/
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_CONSTRAINT;
    intern.idb_ptr = (genptr_t)constraint;
    intern.idb_meth = &rt_functab[ID_CONSTRAINT];

     /* Add data */
     constraint->id=1432;
     constraint->type=323;

    /* The internal representation will be freed */
    return wdb_put_internal( wdbp, name, &intern, mk_conv2mm );
    /*return wdb_export(wdbp,name,(genptr_t) constraint,ID_CONSTRAINT, mk_conv2mm);*/
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
