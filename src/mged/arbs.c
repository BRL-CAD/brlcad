/*                          A R B S . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./mged.h"
#include "./mged_dm.h"
#include "./cmd.h"

char **promp;

char *p_arb3pt[] = {
    "Enter X, Y, Z for point 1: ",
    "Enter Y, Z: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 2: ",
    "Enter Y, Z: ",
    "Enter Z: ",
    "Enter X, Y, Z for point 3: ",
    "Enter Y, Z: ",
    "Enter Z: "
};

char *p_rfin[] = {
    "Enter X, Y, Z of the known point: ",
    "Enter Y, Z: ",
    "Enter Z: "
};


/* F _ R F A R B () :	finds arb8 given.....
 *
 * 1. one point
 * 2. 2 coordinates of 3 other points
 * 3. rot and fallback angles
 * 4. thickness
 */
int
f_rfarb(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int solve[3];
    fastf_t pt[3][2];
    fastf_t thick, rota, fba;
    vect_t norm;
    fastf_t ndotv;
    struct rt_db_internal internal;
    struct rt_arb_internal *aip;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if (argc < 1 || 27 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help rfarb");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* get the arb name */
    if (argc < 2) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter name for this arb: ", (char *)NULL);
	return TCL_ERROR;
    }
    if (db_lookup(dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	Tcl_AppendResult(interp, argv[1], ":  already exists\n", (char *)NULL);
	return TCL_ERROR;
    }


    /* read the known point */
    promp = &p_rfin[0];
    if (argc < 5) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, promp[argc-2], (char *)NULL);
	return TCL_ERROR;
    }

    if (argc < 6) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter ROTATION angle (deg): ", (char *)NULL);
	return TCL_ERROR;
    }

    rota = atof(argv[5]) * DEG2RAD;

    if (argc < 7) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter FALL BACK angle (deg): ", (char *)NULL);
	return TCL_ERROR;
    }

    fba = atof(argv[6]) * DEG2RAD;

    /* calculate plane defined by these angles */
    norm[0] = cos(fba) * cos(rota);
    norm[1] = cos(fba) * sin(rota);
    norm[2] = sin(fba);

    for (i=0; i<3; i++) {
	if (argc < 8+3*i) {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "POINT %d...\n", i+2);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), MORE_ARGS_STR,
			     "Enter coordinate to solve for (x, y, or z): ", (char *)NULL);
	    return TCL_ERROR;
	}

	switch (argv[7+3*i][0]) {
	    case 'x':
		if (ZERO(norm[0])) {
		    Tcl_AppendResult(interp, "X not unique in this face\n", (char *)NULL);
		    return TCL_ERROR;
		}
		solve[i] = X;

		if (argc < 7+3*i+2) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR,
				     "Enter the Y, Z coordinate values: ", (char *)NULL);
		    return TCL_ERROR;
		}
		if (argc < 7+3*i+3) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR,
				     "Enter the Z coordinate value: ", (char *)NULL);
		    return TCL_ERROR;
		}
		pt[i][0] = atof(argv[7+3*i+1]) * local2base;
		pt[i][1] = atof(argv[7+3*i+2]) * local2base;
		break;

	    case 'y':
		if (ZERO(norm[1])) {
		    Tcl_AppendResult(interp, "Y not unique in this face\n", (char *)NULL);
		    return TCL_ERROR;
		}
		solve[i] = Y;

		if (argc < 7+3*i+2) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR,
				     "Enter the X, Z coordinate values: ", (char *)NULL);
		    return TCL_ERROR;
		}
		if (argc < 7+3*i+3) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR,
				     "Enter the Z coordinate value: ", (char *)NULL);
		    return TCL_ERROR;
		}
		pt[i][0] = atof(argv[7+3*i+1]) * local2base;
		pt[i][1] = atof(argv[7+3*i+2]) * local2base;
		break;

	    case 'z':
		if (ZERO(norm[2])) {
		    Tcl_AppendResult(interp, "Z not unique in this face\n", (char *)NULL);
		    return TCL_ERROR;
		}
		solve[i] = Z;

		if (argc < 7+3*i+2) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR,
				     "Enter the X, Y coordinate values: ", (char *)NULL);
		    return TCL_ERROR;
		}
		if (argc < 7+3*i+3) {
		    Tcl_AppendResult(interp, MORE_ARGS_STR,
				     "Enter the Y coordinate value: ", (char *)NULL);
		    return TCL_ERROR;
		}
		pt[i][0] = atof(argv[7+3*i+1]) * local2base;
		pt[i][1] = atof(argv[7+3*i+2]) * local2base;
		break;

	    default:
		Tcl_AppendResult(interp, "coordinate must be x, y, or z\n", (char *)NULL);
		return TCL_ERROR;
	}
    }

    if (argc < 8+3*3) {
	Tcl_AppendResult(interp, MORE_ARGS_STR,
			 "Enter thickness for this arb: ", (char *)NULL);
	return TCL_ERROR;
    }
    thick = atof(argv[7+3*3]);
    if (ZERO(thick)) {
	Tcl_AppendResult(interp, "thickness = 0.0\n", (char *)NULL);
	return TCL_ERROR;
    }
    thick *= local2base;

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_ARB8;
    internal.idb_meth = &rt_functab[ID_ARB8];
    internal.idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_arb_internal), "rt_arb_internal");
    aip = (struct rt_arb_internal *)internal.idb_ptr;
    aip->magic = RT_ARB_INTERNAL_MAGIC;

    for (i=0; i<8; i++) {
	VSET(aip->pt[i], 0.0, 0.0, 0.0);
    }

    VSET(aip->pt[0], atof(argv[2])*local2base, atof(argv[3])*local2base, atof(argv[4])*local2base);

    ndotv = VDOT(aip->pt[0], norm);

    /* calculate the unknown coordinate for points 2, 3, 4 */
    for (i=0; i<3; i++) {
	int j;
	j = i+1;

	switch (solve[i]) {
	    case X:
		aip->pt[j][Y] = pt[i][0];
		aip->pt[j][Z] = pt[i][1];
		aip->pt[j][X] = (ndotv
				 - norm[1] * aip->pt[j][Y]
				 - norm[2] * aip->pt[j][Z])
		    / norm[0];
		break;
	    case Y:
		aip->pt[j][X] = pt[i][0];
		aip->pt[j][Z] = pt[i][1];
		aip->pt[j][Y] = (ndotv
				 - norm[0] * aip->pt[j][X]
				 - norm[2] * aip->pt[j][Z])
		    / norm[1];
		break;
	    case Z:
		aip->pt[j][X] = pt[i][0];
		aip->pt[j][Y] = pt[i][1];
		aip->pt[j][Z] = (ndotv
				 - norm[0] * aip->pt[j][X]
				 - norm[1] * aip->pt[j][Y])
		    / norm[2];
		break;

	    default:
		return TCL_ERROR;
	}
    }

    /* calculate the remaining 4 vertices */
    for (i=0; i<4; i++) {
	VJOIN1(aip->pt[i+4], aip->pt[i], thick, norm);
    }

    /* no interuprts */
    (void)signal(SIGINT, SIG_IGN);

    if ((dp = db_diradd(dbip, argv[1], -1L, 0, RT_DIR_SOLID, (genptr_t)&internal.idb_type)) == RT_DIR_NULL) {
	Tcl_AppendResult(interp, "Cannot add ", argv[1], " to the directory\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &internal, &rt_uniresource) < 0) {
	rt_db_free_internal(&internal);
	TCL_WRITE_ERR_return;
    }

    {
	const char *av[3];

	av[0] = "e";
	av[1] = argv[1]; /* depends on solid name being in argv[1] */
	av[2] = NULL;

	/* draw the "made" solid */
	return cmd_draw(clientData, interp, 2, av);
    }
}


/* ------------------------------ new way ------------------------------ */
/* rt_arb_get_cgtype(), rt_arb_std_type(), and rt_arb_centroid() */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
