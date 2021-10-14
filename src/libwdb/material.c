/*                  M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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

/** @file libwdb/material.c
 *
 * @brief 
 *
 */

#include "common.h"

#include <stdlib.h>

#include "raytrace.h"
#include "wdb.h"


int
mk_material(struct rt_wdb *wdbp, const char *name) {
    struct rt_db_internal intern;
    struct rt_material_internal *material;

    RT_CK_WDB(wdbp);

    RT_DB_INTERNAL_INIT(&intern);

    /* Create a fresh new object for export */
    BU_ALLOC(material, struct rt_material_internal);
    material->magic = RT_MATERIAL_MAGIC;
    BU_VLS_INIT(&material->name);

    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_MATERIAL;
    intern.idb_ptr = (void *)material;
    intern.idb_meth = &OBJ[ID_MATERIAL];

    /* Add data */
    material->id = 4;
    bu_vls_strcpy(&material->name, "TEST");
    material->density = 123.456;

    /* The internal representation will be freed */
    return wdb_put_internal(wdbp, name, &intern, mk_conv2mm);
}