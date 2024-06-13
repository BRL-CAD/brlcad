/*                      C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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


#include "bn.h"
#include "rt/db4.h"
#include "pc.h"
#include "raytrace.h"


const struct bu_structparse rt_constraint_parse[] = {
    {"%d", 1, "ID", bu_offsetof(struct rt_constraint_internal, id), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "N", bu_offsetof(struct rt_constraint_internal, type), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "Ex", bu_offsetof(struct rt_constraint_internal, expression), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
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
	bu_free((void *)constraint, "constraint ifree");
    }
    ip->idb_ptr = ((void *)0);	/* sanity */
}

/**
 * Import a constraint from the database format to the internal format.
 */
int
rt_constraint_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *UNUSED(mat), const struct db_i *dbip)
{
    struct rt_constraint_internal* cip;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_CONSTRAINT;
    ip->idb_meth = &OBJ[ID_CONSTRAINT];
    BU_ALLOC(ip->idb_ptr, struct rt_constraint_internal);

    cip = (struct rt_constraint_internal *)ip->idb_ptr;
    cip->magic = RT_CONSTRAINT_MAGIC;
    BU_VLS_INIT(&cip->expression);
    cip->id = 0;
    cip->type = 0;

    bu_vls_strncpy(&str, (const char *)ep->ext_buf, ep->ext_nbytes);

    if (bu_struct_parse(&str, rt_constraint_parse, (char *)cip, NULL) < 0) {
	bu_vls_free(&str);
	bu_free((char *)cip, "rt_constraint_import5: cip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -2;
    }
    bu_vls_free(&str);

    return 0;
}


int
rt_constraint_export5(struct bu_external *ep, const struct rt_db_internal *ip, double UNUSED(local2mm), const struct db_i *UNUSED(dbip))
{
    struct rt_constraint_internal *cip;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type != ID_CONSTRAINT) bu_bomb("rt_constraint_export() type not ID_CONSTRAINT");
    cip = (struct rt_constraint_internal *) ip->idb_ptr;

    BU_EXTERNAL_INIT(ep);

    bu_vls_struct_print(&str, rt_constraint_parse, (char *)cip);

    ep->ext_nbytes = bu_vls_strlen(&str);
    if (ep->ext_nbytes == 0)
	return -1;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "constrnt external");
    bu_strlcpy((char *)ep->ext_buf, bu_vls_addr(&str), ep->ext_nbytes);

    bu_vls_free(&str);

    return 0;	/* OK */
}

/**
 * Make human-readable formatted presentation of this solid.
 * First line describes type of solid.
 * Additional lines are indented one tab, and give parameter values.
 */
int
rt_constraint_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double UNUSED(mm2local))
{
    struct rt_constraint_internal *cip = (struct rt_constraint_internal *)ip->idb_ptr;

    if (cip->magic != RT_CONSTRAINT_MAGIC) bu_bomb("rt_constraint_describe() bad magic");
    bu_vls_strcat(str, "constraint (CONSTRNT)\n");

    if (!verbose)
	return 0;

    bu_vls_printf(str, "\tid=%d\n\ttype=%d\n\texpression=%s\n",
		  cip->id,
		  cip->type,
		  bu_vls_addr(&cip->expression));

    return 0;
}

void
rt_constraint_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_constraint_internal* ip;

    intern->idb_type = ID_CONSTRAINT;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    BU_ALLOC(ip, struct rt_constraint_internal);
    intern->idb_ptr = (void *)ip;

    ip->magic = RT_CONSTRAINT_MAGIC;
    ip->id = 1;
    ip->type = 0;
    BU_VLS_INIT(&ip->expression);
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
