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
 * 
 */

#include "common.h"

#ifdef HAVE_BULLET

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "../ged_private.h"


extern int run_simulation(struct bu_vls *result_str, mat_t t, int argc, const char *argv[]);

/**
 * How to use simulate.Blissfully simple interface, more options will be added soon
 */
static void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str, "Usage: simulate <steps> <object>\n\n");
    bu_vls_printf(str, "Currently this command aligns <object> to the final position of a falling sphere\n");
    bu_vls_printf(str, "after <steps> number of simulation steps.\n");
    bu_vls_printf(str, "-n <n> <x> <y> <z>\t- Specifies number of steps.\n");
    bu_vls_printf(str, "-f <n> <x> <y> <z>\t- Specifies frequency of update(eg 1/60 Hz).\n");
    bu_vls_printf(str, "-t <x> <y> <z>\t\t  - Specifies time for which to run(alternative to -n).\n");
    return;
}


/**
 * The physics simulation function
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *ndp;
    int rv;
    struct rt_db_internal intern;
    static const char *transf_shape_name = "Transformed_Shape";
    char *cmd_args[5];
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

    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s <steps> <object>", argv[0]);
        return GED_ERROR;
    }
    
    /* First duplicate the passed shape, so the original is untouched */
    /* Check if the duplicate already exists, and kill it if so(maybe of a different shape) */
    if (db_lookup(gedp->ged_wdbp->dbip, transf_shape_name, LOOKUP_QUIET) != RT_DIR_NULL) {
    	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists, deleting it",  argv[0], transf_shape_name);
    	cmd_args[0] = "kill";
        cmd_args[1] = (char *)transf_shape_name;
        cmd_args[2] = (char *)0;

        if(ged_kill(gedp, 2, (const char **)cmd_args) != GED_OK){
            bu_vls_printf(gedp->ged_result_str, "%s: could not delete existing %s", argv[0], transf_shape_name);
            return GED_ERROR;
        }
    }
   
    /* Copy the shape, clone is required for combinations(full tree copy) : TODO */
    cmd_args[0] = "copy";
    cmd_args[1] = (char *)argv[2];
    cmd_args[2] = (char *)transf_shape_name;
    cmd_args[3] = (char *)0;
    rv = ged_copy(gedp, 3, (const char **)cmd_args);
    if (rv != GED_OK){
        bu_vls_printf(gedp->ged_result_str, "%s: could not copy \"%s\" to \"%s\"\n", argv[0], argv[1],
        		transf_shape_name);
        return GED_ERROR;
    }
    
    /* Get the existing transform of the object : TODO */
    
    /* The shape which will reflect the new position is now ready. so the simulation can be run.
     * run_simulation() is the C++ function with Bullet code. It will be modified later to pass
     * transformation matrices to and fro . Error check to be added below : TODO
     */
    rv = run_simulation(gedp->ged_result_str, t, argc, argv);

    /* Lookup the target shape to which the result of run_simulation() will be applied */
    if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  transf_shape_name, LOOKUP_NOISY)) == RT_DIR_NULL){
        bu_vls_printf(gedp->ged_result_str, "%s: Error db_lookup(%s) failed ! \n", argv[0],
        transf_shape_name);
        return GED_ERROR;
    }
        
	/* Get the internal representation of the same shape */
	GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	/* Drop any existing transformations on it */
	/* TODO */

	/* Apply the transform output from run_simulation() on Transformed_Shape
	 * This transform must be treated as an absolute world transform and
	 * not an increment. Currently rt_matrix_transform() adds to the existing transform
	 */
	if (rt_matrix_transform(&intern, t, &intern, 0, gedp->ged_wdbp->dbip, &rt_uniresource) < 0){
	   bu_vls_printf(gedp->ged_result_str, "%s: ERROR rt_matrix_transform failed !\n", argv[0]);
	   return GED_ERROR;
	}

	/* Write the modified solid to the db so it can be redrawn at the new position & orientation by Mged */
	if (rt_db_put_internal(ndp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
		return GED_ERROR;
	}

	bu_vls_printf(gedp->ged_result_str, "%s: The simulation result is in primitive : %s\n",
																argv[0], transf_shape_name);
                                                     
    /* Draw the result : inserting it in argv[1] will cause it to be automatically drawn in the cmd_wrapper */
    argv[1] = (char *)transf_shape_name;
    argv[2] = (char *)0;

    return GED_OK;
}


#else

#include "../ged_private.h"

/**
 * Dummy simulation function in case no physics library found
 */
int
ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* Initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%s : This command is disabled due to absence of a physics library",
    		argv[0]);
	return GED_ERROR;
}

#endif






/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
