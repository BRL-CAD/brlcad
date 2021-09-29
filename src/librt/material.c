/*                      M A T E R I A L . C
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
/** @addtogroup librt */
/** @{ */
/** @file librt/material.c
 *
 * Various functions associated with material object database I/O
 *
 * Todo: 
 *
 */

#include "common.h"

#include <stdio.h>


#include "bn.h"
#include "rt/db4.h"
#include "pc.h"
#include "raytrace.h"


static const struct bu_structparse rt_material_parse[] = {
    {"%d", 1, "ID", bu_offsetof(struct rt_material_internal, id), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "Ex", bu_offsetof(struct rt_material_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "D", bu_offsetof(struct rt_material_internal, density), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * Free the storage associated with the rt_db_internal version of
 * material object.
 */
void
rt_material_ifree(struct rt_db_internal *ip)
{
    register struct rt_material_internal *material;

    RT_CK_DB_INTERNAL(ip);
    material = (struct rt_material_internal *)ip->idb_ptr;

    if (material) {
	material->magic = 0;			/* sanity */
	bu_vls_free(&material->name);
	bu_free((void *)material, "material ifree");
    }
    ip->idb_ptr = ((void *)0);	/* sanity */
}


/**
 * Import a material from the database format to the internal format.
 */
int
rt_material_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *UNUSED(mat), const struct db_i *UNUSED(dbip))
{
    struct rt_material_internal *material;
    unsigned char *ptr;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_MATERIAL;
    ip->idb_meth = &OBJ[ID_MATERIAL];
    BU_ALLOC(ip->idb_ptr, struct rt_material_internal);

    material = (struct rt_material_internal *)ip->idb_ptr;
    BU_VLS_INIT(&material->name);
    material->magic = RT_MATERIAL_MAGIC;

    ptr = ep->ext_buf;

    bu_vls_init(&material->name);
    bu_vls_strncpy(&material->name, (char *)ptr,
           ep->ext_nbytes - (ptr - (unsigned char *)ep->ext_buf));

    return 0;			/* OK */
}


/**
 * Export a material from the internal format to the database format.
 */
int
rt_material_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *UNUSED(dbip))
{
    struct rt_material_internal *cip;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_MATERIAL) bu_bomb("rt_material_export() type not ID_MATERIAL");
    cip = (struct rt_material_internal *) ip->idb_ptr;

    BU_EXTERNAL_INIT(ep);

    bu_vls_struct_print(&str, rt_material_parse, (char *)cip);

    ep->ext_nbytes = bu_vls_strlen(&str);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "material external");
    bu_strlcpy((char *)ep->ext_buf, bu_vls_addr(&str), ep->ext_nbytes);

    bu_vls_free(&str);

    return 0;	/* OK */
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
