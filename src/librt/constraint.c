/*                      C O N S T R A I N T . C
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
/** @addtogroup librt */
/** @{ */
/** @file librt/constraint.c
 *
 * Various functions associated with constraint object database I/O
 *
 * Todo: Remove * resp from the param list ?? by modification of
 * table.c and rt_functab struct ?
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "bn.h"
#include "db.h"
#include "pc.h"
#include "raytrace.h"


const struct bu_structparse rt_constraint_parse[] = {
    {"%d", 1, "ID", bu_offsetof(struct rt_constraint_internal, id), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "N", bu_offsetof(struct rt_constraint_internal, type), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "Ex", bu_offsetof(struct rt_constraint_internal, expression), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * R T _ C O N S T R A I N T _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of
 * constraint object.
 */
void
rt_constraint_ifree(struct rt_db_internal *ip)
{
    register struct rt_constraint_internal *constraint;

    RT_CK_DB_INTERNAL(ip);
    constraint = (struct rt_constraint_internal *)ip->idb_ptr;

    if (constraint) {
	constraint->magic = 0;			/* sanity */
	if (BU_VLS_IS_INITIALIZED(&constraint->expression))
	    bu_vls_free(&constraint->expression);
	else
	    bu_log("Freeing bogus constraint, VLS not initialized\n");

	bu_free((genptr_t)constraint, "constraint ifree");
    }
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ C O N S T R A I N T _ E X P O R T 5
 *
 */
int
rt_constraint_export5(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double UNUSED(local2mm),
    const struct db_i *dbip,
    struct resource *resp)
{
    struct rt_constraint_internal *cip;
    struct rt_constraint_internal constraint;
    struct bu_vls str;

    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);
    if (resp) RT_CK_RESOURCE(resp);

    if (ip->idb_type != ID_CONSTRAINT) bu_bomb("rt_constraint_export() type not ID_CONSTRAINT");
    cip = (struct rt_constraint_internal *) ip->idb_ptr;
    /*RT_CONSTRAINT_CK_MAGIC(cip);*/
    constraint = *cip;

    BU_EXTERNAL_INIT(ep);

    bu_vls_init(&str);
    bu_vls_struct_print(&str, rt_constraint_parse, (char *) &constraint);

    ep->ext_nbytes = bu_vls_strlen(&str);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "constrnt external");
    bu_strlcpy((char *)ep->ext_buf, bu_vls_addr(&str), ep->ext_nbytes);

    bu_vls_free(&str);

    return 0;	/* OK */
}

/**
 * R T _ C O N S T R A I N T _ I M P O R T 5
 *
 */
int
rt_constraint_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t UNUSED(mat), const struct db_i *dbip, struct resource *resp)
{
    RT_CK_DB_INTERNAL(ip);
    BU_CK_EXTERNAL(ep);
    RT_CK_DBI(dbip);
    if (resp) RT_CK_RESOURCE(resp);

    return 0; /* OK */
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
