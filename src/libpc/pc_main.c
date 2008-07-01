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
/** @addtogroup libpc */
/** @{ */
/** @file pc_main.c
 *
 * TODO: clean up header includes
 *
 * @author Dawn Thomas
 *
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "pc.h"


/**
 * P C _  W R I T E _ P A R A M E T E R _ S E T
 *
 * Takes a parameter set as input and writes it onto the directory
 * pointed by dp in the database pointed to by fp.
 *
 * No Checking done presently.
 *
 * TODO: Remove: Redundant after rt_*_params() functions
 */
int
pc_write_param_set(struct pc_param_set ps, struct directory * dp,struct db_i * dbip ) {
    register int i,r;
    struct bu_attribute_value_set avs;
    struct bu_vls v,w;
    bu_avs_init_empty(&avs);
    bu_vls_init(&v);
    bu_vls_init(&w);

    for (i=ps.len-1;i>=0;i--) {

	bu_vls_printf(&v,"%g",ps.p[i].step);
	bu_vls_strcat(&w,ps.p[i].name);
	bu_vls_strcat(&w,"_step");
	bu_avs_add(&avs,bu_vls_addr(&w),bu_vls_addr(&v));
	bu_vls_free(&v);
	bu_vls_free(&w);

	bu_vls_printf(&v,"%g",ps.p[i].max);
	bu_vls_strcat(&w,ps.p[i].name);
	bu_vls_strcat(&w,"_max");
	bu_avs_add(&avs,bu_vls_addr(&w),bu_vls_addr(&v));
	bu_vls_free(&v);
	bu_vls_free(&w);

	bu_vls_printf(&v,"%g",ps.p[i].min);
	bu_vls_strcat(&w,ps.p[i].name);
	bu_vls_strcat(&w,"_min");
	bu_avs_add(&avs,bu_vls_addr(&w),bu_vls_addr(&v));
	bu_vls_free(&v);
	bu_vls_free(&w);

	bu_vls_printf(&v,"%g",ps.p[i].value);
	bu_vls_strcat(&w,ps.p[i].name);
	bu_vls_strcat(&w,"_value");
	bu_avs_add(&avs,bu_vls_addr(&w),bu_vls_addr(&v));
	bu_vls_free(&v);
	bu_vls_free(&w);

	bu_vls_printf(&v,"%d",ps.p[i].parametrized);
	bu_vls_strcat(&w,ps.p[i].name);
	bu_vls_strcat(&w,"_parametrized");
	bu_avs_add(&avs,bu_vls_addr(&w),bu_vls_addr(&v));
	bu_vls_free(&v);
	bu_vls_free(&w);

	/*bu_vls_strcat(&w,"name");*/
	bu_vls_strcat(&v,ps.p[i].name);
	bu_vls_printf(&w,"name_%d",i);
	bu_avs_add_nonunique(&avs,bu_vls_addr(&w),bu_vls_addr(&v));
	//db5_export_attributes(&ext,&avs);

	bu_vls_free(&w);
	bu_vls_free(&v);
    }
    db5_update_attributes(dp,&avs,dbip);
    bu_avs_free(&avs);
    return 0;
}


/**
 * P C _  G E N E R A T E _ P A R A M E T E R S
 *
 * Given a directory pointer, pc_generate_parameters generates the set
 * of all parameters that the particular geometry element could have.
 * All the parametrized flags corresponding to each parameter are
 * still set to 0. The user will have to use pc_parametrize to make
 * any parameter non-explicit
 *
 * TODO: Redundant after rt_*_params definition
 */
int
pc_generate_parameters( struct pc_param_set * psp, struct directory * dp,struct db_i * dbip) {
    register int id;
    struct rt_db_internal ip;
    rt_init_resource(&rt_uniresource,0,NULL);
    id = rt_db_get_internal(&ip,dp,dbip,NULL,&rt_uniresource);
    return 0;
}


/**
 * P C _ P A R A M E T R I Z E
 *
 * Taking parameter_table, parameter name, parameter value and
 * directory pointer as inputs pc_parametrize sets the parametrized
 * flag of the corresponding parameter and assigns the valueand writes
 * it onto the directory pointed by dp in the database pointed to by
 * fp.
 *
 * No Checking done presently.
 *
 * TODO: Remove if redundant after implementation of rt_*_params
 */
int pc_parametrize() {
}


/**
 * P C _ D E P A R A M E T R I Z E
 *
 * Scans the parameter_set for the named parameter , sets parametrized
 * flag to 0 thereby implying that the value corresponding to it is
 * datum / explicit
 *
 * TODO: Remove if redundant after implementation of rt_*_params
 */
int pc_deparametrize() {
}
/**
 * P C _ M K _ C O N S T R A I N T
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
