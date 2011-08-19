/*                         S I M U L A T E . C
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
/** @file libged/simulate.c
 *
 * The simulate command.
 *
 * Routines related to performing physics on passed objects only
 *
 * TODO:
 *   Insert Bullet code in the simphysics.cpp file and link to libs
 *   Add time option, frequency etc
 *
 * 
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged_private.h"


extern int single_step_sim(struct bu_vls *result_str, int argc, const char *argv[]);

/**
 * Duplicate existing objects : Unused for now as shape duplicated using copy command
 */
static int
duplicate_objects(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *from_dp;
    struct bu_external external;
    int i;
    
    for (i = 1; i < argc; i++) {
        
        GED_DB_LOOKUP(gedp, from_dp, argv[i], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
//        GED_CHECK_EXISTS(gedp, argv[i], LOOKUP_QUIET, GED_ERROR);

        if (db_get_external(&external, from_dp, gedp->ged_wdbp->dbip)) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	    return GED_ERROR;
        }

        if (wdb_export_external(gedp->ged_wdbp, &external, "TransformedResult",
			        from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	    bu_free_external(&external);
	    bu_vls_printf(gedp->ged_result_str,
		          "Failed to write new object (%s) to database - aborting!!\n",
		          argv[i]);
	    return GED_ERROR;
        }

        bu_free_external(&external);    
    }
    
    bu_vls_printf(gedp->ged_result_str, "Objects duplicated !\n");
  
    return GED_OK;
}


/**
 * How to use simulate.Blissfully simple interface, more options will be added soon
 */
static void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str, "Usage: simulate [-nft] <object>\n\n");
    bu_vls_printf(str, "-n <n> <x> <y> <z>\t- Specifies number of steps.\n");
    bu_vls_printf(str, "-f <n> <x> <y> <z>\t- Specifies frequency of update(eg 1/60 Hz)\n");
    bu_vls_printf(str, "-t <x> <y> <z>\t\t  - Specifies time for which to run(alternative to -n).\n");
    bu_vls_printf(str, "-v\t\t\t- Prints version info.\n");
    return;
}


/**
 * The physics simulation function
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *ndp;
    int i, rv;
    struct rt_db_internal intern;   //input and output solid
    static const char *transf_shape_name = "Transformed_Shape";
    static const char *usage = "object(s)";
    char *cmd_args[5], result_str[100];
    mat_t t;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Must be wanting help */
    if (argc == 1) {
	print_usage(gedp->ged_result_str);
	return GED_HELP;
    }

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }
    
    /* First duplicate the passed shape, so the original is untouched */
    //duplicate_objects(gedp, argc, argv);  //does not do deep object copy, so original primitives are attached:BAD
    //Check if the duplicate exists, and kill it if so
    if (db_lookup(gedp->ged_wdbp->dbip, transf_shape_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	cmd_args[0] = "sim_kill";
        cmd_args[1] = (char *)transf_shape_name;
        cmd_args[2] = (char *)0;
        if(ged_kill(gedp, 2, (const char **)cmd_args) != GED_OK){
            bu_vls_printf(gedp->ged_result_str, "%s: could not delete existing %s", argv[0], transf_shape_name);
	    return GED_ERROR;
	}
	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists, deleting it",  argv[0], transf_shape_name);	
    }
   
    /* Copy the shape, clone is required for combinations(full tree copy) : TODO */
    cmd_args[0] = "sim_copy";
    cmd_args[1] = (char *)argv[1];              //same as passed argv[1]/primitive
    cmd_args[2] = (char *)transf_shape_name;    //new primitive name
    cmd_args[3] = (char *)0;
    rv = ged_copy(gedp, 3, (const char **)cmd_args);
    if (rv != GED_OK)
	bu_vls_printf(gedp->ged_result_str, "%s: WARNING: Failure cloning \"%s\" to \"%s\"\n", argv[0],
	argv[1], transf_shape_name);    
       
    
    /* Setup translation matrix of 1m in X for testing */
    t[0]  = 1; t[1]  = 0; t[2]  = 0; t[3]  = 1000;
    t[4]  = 0; t[5]  = 1; t[6]  = 0; t[7]  = 0;
    t[8]  = 0; t[9]  = 0; t[10] = 1; t[11] = 0;
    t[12] = 0; t[13] = 0; t[14] = 0; t[15] = 1;
    
    /* Loop not correct as of now, only 1 primitive can be transformed, but no error as argc=2 */
    for (i = 1; i < argc; i++) {
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  transf_shape_name, LOOKUP_NOISY)) == RT_DIR_NULL){
            bu_vls_printf(gedp->ged_result_str, "%s: Error db_lookup(%s) failed ! \n", argv[0], 
            transf_shape_name);
            return GED_ERROR;
        }
        
        /* Get the internal representation of the object passed by the user */
        GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);
       
        bu_vls_printf(gedp->ged_result_str, "%s: Moving primitive : %s\n", argv[0], argv[i]);
       
        /* This is the cpp function with Bullet code, will be modified later to pass transformation 
           matrices to and fro */  
        rv = single_step_sim(gedp->ged_result_str, argc, argv);
        bu_vls_printf(gedp->ged_result_str, "%s: Single step : \n%s\n", argv[0], result_str);
          
        /* Do the matrix transform which will be output from single_step_sim() */
        if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){ 
           bu_vls_printf(gedp->ged_result_str, "%s: ERROR rt_matrix_transform failed !\n", argv[0]);
           return GED_ERROR;
        }
        
        /* Write the modified solid to the db so it can be redrawn at the new position & orientation by Mged */        
        if (rt_db_put_internal(ndp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
            bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
            return GED_ERROR;
        }
        
        bu_vls_printf(gedp->ged_result_str, "%s: Simulation result is in  primitive : %s\n", 
                                                                    argv[0], transf_shape_name); 
                                                     
        /* Draw the result : inserting it in argv[1] will cause it to be automatically drawn in cmd_wrapper */
 	argv[1] = (char *)transf_shape_name;
	argv[2] = (char *)0;
	
    }
   

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
