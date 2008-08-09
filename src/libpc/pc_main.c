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
 * 			PC_INIT_PCSET
 * initiates a pc_set object by calling the appropriate bu_list init macros
 *
 */
void
pc_init_pcset(struct pc_pc_set * pcsp)
{
    BU_GETSTRUCT(pcsp->ps,pc_param);
    BU_LIST_INIT(&(pcsp->ps->l));
    BU_GETSTRUCT(pcsp->cs,pc_constrnt);
    BU_LIST_INIT(&(pcsp->cs->l));
}

/**
 * 			PC_GETPARAMETER
 * allocates memory for the parameter if parameter storage accoring to
 * the integer argument
 *
 * 	0 : data storage by expression
 * 	anything else : data storage by structure
 *
 */
void
pc_getparameter(struct pc_param ** p, int n)
{
    BU_GETSTRUCT(*p,pc_param);
    bu_vls_init(&((*p)->name));
    //if (n == 0)
	bu_vls_init(&((*p)->data.expression));
}

/**
 * 			PC_PUSHPARAMETER
 * pushes a given parameter expression into the parameter
 * list in the pc set
 *
 */
void
pc_pushparameter(struct pc_pc_set * pcsp, const char * str)
{
    struct pc_param * par;
    pc_getparameter(&par,0);
    bu_vls_strcat(&(par->name), str);
    par->ctype = byexpression;
    PC_PCSET_PUSHP(pcsp, par);
}

/**
 * 			PC_GETCONSTRAINT
 * allocates memory for the parameter if parameter storage accoring to
 * the integer argument
 *
 * 	0 : data storage by expression
 * 	anything else : data storage by structure
 *
 */
void
pc_getconstraint(struct pc_constrnt ** c, int n)
{
    BU_GETSTRUCT(*c,pc_constrnt);
    bu_vls_init(&((*c)->name));
    /*if (n == 0) */
    bu_vls_init(&((*c)->expression));
}

/**
 * 			PC_PUSHCONSTRAINT
 * pushes a given constraint expression into the constraint list
 * in the pc set
 *
 */
void
pc_pushconstraint(struct pc_pc_set * pcsp, const char * str)
{
    struct pc_constrnt * con;
    pc_getconstraint(&con,0);
    bu_vls_strcat(&(con->name), str);
    PC_PCSET_PUSHC(pcsp, con);
}

/**
 * 			PC_FREE_PCSET
 * frees the parameter&constraint set (pc_pc_set) pointed to by the
 * pointer taken as an argument. It also frees the corresponding allocation
 * for bu_vls structs used for the storage of name and expression of
 * pc_param and pc_constrnt structures
 *
 */
void
pc_free_pcset(struct pc_pc_set * pcs) 
{
    struct pc_param * par;
    struct pc_constrnt * con;
    while (BU_LIST_WHILE(par,pc_param,&(pcs->ps->l))) { 
        bu_vls_free(&(par->name));
        bu_vls_free(&(par->data.expression));
        BU_LIST_DEQUEUE(&(par->l));
	bu_free(par, "free parameter");
    }
    bu_free(pcs->ps, "free parameter");
    while (BU_LIST_WHILE(con,pc_constrnt,&(pcs->cs->l))) {
        bu_vls_free(&(con->name));
        bu_vls_free(&(con->expression));
        BU_LIST_DEQUEUE(&(con->l));
        bu_free(con, "free constraint");
    }
    bu_free(pcs->cs, "free constraint");
}

/**
 * 			PC_MK_CONSTRAINT
 *
 * Given the appropriate parameters, makes the non-geometric
 * constraint object and writes it to the database using
 * wdb_put_internal.  Only supported on database version 5 or above
 *
 *
 * Methodology for the new object type definition followed:
 *
 *      define pc_constraint_internal --- parameters for solid
 *      define rt_xxx_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 *      code import/export/describe/print/ifree/prep/
 *
 *      pc_constraint_internal defined in pc.h
 *      edit magic.h to add RT_CONSTRAINT_MAGIC
 *      edit table.c:
 *              RT_DECLARE_INTERFACE()
 *              struct rt_functab entry
 *      edit raytrace.h to make ID_CONSTRAINT, increment ID_MAXIMUM
 *      !! edit db_scan.c to add the new solid to db_scan()
 *      edit Makefile.am to add pc_constraint.c to compile
 *
 *      Then:
 *      pc_mk_constraint defined here ( shift to wdb?? )
 *      !! go to src/conv and edit g2asc.c and asc2g.c to support the new solid
 *      !! go to src/librt and edit tcl.c to add the new solid to
 *              rt_solid_type_lookup[]
 *              also add the interface table and to rt_id_solid() in table.c
 *      !! go to src/mged and create the edit support
 */
int
pc_mk_constraint(
    struct rt_wdb       *wdbp,
    const char          *constraintname,
    int                 append_ok)      /* 0 = obj must not exit */
{
    struct rt_db_internal       intern;
    struct rt_constraint_internal *constraint;
    int new_constraint;

    RT_CK_WDB(wdbp);

    RT_INIT_DB_INTERNAL(&intern);

    if (append_ok &&
	wdb_import( wdbp, &intern, constraintname, (matp_t)NULL ) >= 0) {
	/* We retrieved an existing object, append to it */
	constraint = (struct rt_constraint_internal *)intern.idb_ptr;
	/*PC_CK_CONSTRAINT( constraint );*/

	new_constraint = 0;
    } else {
	/* Create a fresh new object for export */
	BU_GETSTRUCT( constraint, rt_constraint_internal );
	constraint->magic = RT_CONSTRAINT_MAGIC;
	/* bu_vls_init( &constraint->shader );*/

	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_CONSTRAINT;
	intern.idb_ptr = (genptr_t)constraint;
	intern.idb_meth = &rt_functab[ID_CONSTRAINT];

	new_constraint = 1;
    }

    /* Don't change these things when appending to existing
     * constraint.
     */
    if (new_constraint) {
	constraint->id=1432;
	constraint->type=323;
    }

    /* The internal representation will be freed */
    return wdb_put_internal( wdbp, constraintname, &intern, mk_conv2mm );
    /*return wdb_export(wdbp,constraintname,(genptr_t) constraint,ID_CONSTRAINT, mk_conv2mm);*/
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
