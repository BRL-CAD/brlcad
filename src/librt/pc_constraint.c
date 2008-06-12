/*                   	P C _ C O N S T R A I N T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @addtogroup libpc */
/** @{ */
/** @file pc_main.c
 *
 *  Various functions associated with constraint objet input output 
 *  using the database
 *
 *  Todo: Remove * resp from the param list ?? by modification of table.c
 *	  and rt_functab struct ?
 *
 *
 *@author	Dawn Thomas
 *	
 *
 */
#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "bn.h"
#include "db.h"
#include "wdb.h"
#include "pc.h"
#include "raytrace.h"

const struct bu_structparse pc_constraint_parse[] = {
    {"%d",      1, "ID",        bu_offsetof(struct pc_constraint_internal, id) ,BU_STRUCTPARSE_FUNC_NULL },
    {"%d",      1, "N",         bu_offsetof(struct pc_constraint_internal, type),BU_STRUCTPARSE_FUNC_NULL },
    {"",        0, (char *)0,   0,                      BU_STRUCTPARSE_FUNC_NULL }
};


/**
 *			P C _ C O N S T R A I N T _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of constraint object.
 *  TODO: Rename to rt_constraint_ifree??
 */
void
pc_constraint_ifree( struct rt_db_internal *ip, struct resource *resp)
{
    register struct pc_constraint_internal	*constraint;

    RT_CK_DB_INTERNAL(ip);
    constraint = (struct pc_constraint_internal *)ip->idb_ptr;
    
    if (constraint) {
	constraint->magic = 0;			/* sanity */
	bu_free( (genptr_t)constraint, "constraint ifree" );
    }
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}



/**
 * P C _ C O N S T R A I N T  _ E X P O R T
 *
 *  Todo: Check for version 4, Rename to rt_constraint_export??
 */
int
pc_constraint_export(
    struct bu_external		*ep,
    const struct rt_db_internal	*ip,
    double			local2mm,
    const struct db_i		*dbip,
    struct resource		*resp)
{
    struct pc_constraint_internal	*cip;
    struct pc_constraint_internal	constraint;
    struct bu_vls			str;

    RT_CK_DB_INTERNAL(ip);

    if ( ip->idb_type != ID_CONSTRAINT ) bu_bomb("pc_constraint_export() type not ID_CONSTRAINT");
    cip = (struct pc_constraint_internal *) ip->idb_ptr;
    /*PC_CONSTRAINT_CK_MAGIC(cip);*/
    constraint = *cip;

    BU_INIT_EXTERNAL(ep);
/*    BU_CK_EXTERNAL(ep);*/
    
    bu_vls_init(&str);
    bu_vls_struct_print(&str, pc_constraint_parse, (char *) &constraint);

    ep->ext_nbytes = bu_vls_strlen(&str);
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes,"constrnt external");
    bu_strlcpy(ep->ext_buf,bu_vls_addr(&str),ep->ext_nbytes);
   
    bu_vls_free(&str);
    return 0;	/* OK */
}

/**
 * P C _ C O N S T R A I N T _ I M P O R T
 *
 *  Todo: Implement, Check for version 4, Rename to rt_constraint_import??
 *
 */
int
pc_constraint_import(
    struct rt_db_internal	*ip,
    const struct bu_external *ep,
    const mat_t		mat,
    const struct db_i	*dbip,
    struct resource		*resp,
    const int		minor_type)
{
    return 0; /* OK */
}


/**
 * P C _ C H E C K _ P A R A M S
 *
 *@brief
 *  Given a parametrics-constraints set object, check parameters checks the
 *  various parameters within the set with respect to the constraints using 
 *  libpc subroutines
 *@return	0	OK
 *@return	-1	FAIL
 */
int
pc_check_params(const struct pc_pc_set * ps) {

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
