/*                        S H _ T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
/** @file rt/sh_tcl.c
 *
 *  Tcl interfaces to RT material & shader routines.
 *
 *  These routines are not for casual command-line use;
 *  as a result, the Tcl name for the function should be exactly
 *  the same as the C name for the underlying function.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "shadefuncs.h"


extern struct mfuncs	*mfHead;	/* view.c */
extern int get_args(int argc, const char *argv[]); /* opt.c */


/*
 *			S H _ D I R E C T C H A N G E _ R G B
 *
 *  Go poke the rgb values of a region, on the fly.
 *  This does not update the inmemory database,
 *  so any changes will vanish on next re-prep unless other measures
 *  are taken.
 */
int
sh_directchange_rgb(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_i	*rtip;
    struct region	*regp;
    struct directory *dp;
    float		r, g, b;

    if ( argc != 6 )  {
	Tcl_AppendResult(interp, "Usage: sh_directchange_rgb $rtip comb r g b\n", NULL);
	return TCL_ERROR;
    }

    r = atoi(argv[3+0]) / 255.;
    g = atoi(argv[3+1]) / 255.;
    b = atoi(argv[3+2]) / 255.;

    rtip = (struct rt_i *)atol(argv[1]);
    RT_CK_RTI(rtip);

    if ( rtip->needprep )  {
	Tcl_AppendResult(interp, "rt_prep() hasn't been called yet, error.\n", NULL);
	return TCL_ERROR;
    }

    if ( (dp = db_lookup( rtip->rti_dbip, argv[2], LOOKUP_NOISY)) == RT_DIR_NULL )  {
	Tcl_AppendResult(interp, argv[2], ": not found\n", NULL);
	return TCL_ERROR;
    }

    /* Find all region names which match /comb/ pattern */
    for ( BU_LIST_FOR( regp, region, &rtip->HeadRegion ) )  {
	if ( dp->d_flags & RT_DIR_REGION )  {
	    /* name will occur at end of region string w/leading slash */
	} else {
	    /* name will occur anywhere, bracked by slashes */
	}

	/* XXX quick hack */
	if ( strstr( regp->reg_name, argv[2] ) == NULL )  continue;

	/* Modify the region's color */
	bu_log("sh_directchange_rgb() changing %s\n", regp->reg_name);
	VSET( regp->reg_mater.ma_color, r, g, b );

	/* Update the shader */
	mlib_free(regp);
	if ( mlib_setup( &mfHead, regp, rtip ) != 1 )  {
	    Tcl_AppendResult(interp, regp->reg_name, ": mlib_setup() failure\n", NULL);
	}
    }

    return TCL_OK;
}


/*
 *			S H _ D I R E C T C H A N G E _ S H A D E R
 *
 *  Go poke the rgb values of a region, on the fly.
 *  This does not update the inmemory database,
 *  so any changes will vanish on next re-prep unless other measures
 *  are taken.
 */
int
sh_directchange_shader(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    struct rt_i	*rtip;
    struct region	*regp;
    struct directory *dp;
    struct bu_vls	shader;

    if ( argc < 4 )  {
	Tcl_AppendResult(interp, "Usage: sh_directchange_shader $rtip comb shader_arg(s)\n", NULL);
	return TCL_ERROR;
    }

    rtip = (struct rt_i *)atol(argv[1]);
    RT_CK_RTI(rtip);

    if ( rtip->needprep )  {
	Tcl_AppendResult(interp, "rt_prep() hasn't been called yet, error.\n", NULL);
	return TCL_ERROR;
    }

    if ( (dp = db_lookup( rtip->rti_dbip, argv[2], LOOKUP_NOISY)) == RT_DIR_NULL )  {
	Tcl_AppendResult(interp, argv[2], ": not found\n", NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&shader);
    bu_vls_from_argv(&shader, argc-3, argv+3);
    bu_vls_trimspace(&shader);

    /* Find all region names which match /comb/ pattern */
    for ( BU_LIST_FOR( regp, region, &rtip->HeadRegion ) )  {
	if ( dp->d_flags & RT_DIR_REGION )  {
	    /* name will occur at end of region string w/leading slash */
	} else {
	    /* name will occur anywhere, bracked by slashes */
	}

	/* XXX quick hack */
	if ( strstr( regp->reg_name, argv[2] ) == NULL )  continue;

	/* Modify the region's shader string */
	bu_log("sh_directchange_shader() changing %s\n", regp->reg_name);
	if ( regp->reg_mater.ma_shader )
	    bu_free( (genptr_t)regp->reg_mater.ma_shader, "reg_mater.ma_shader");
	regp->reg_mater.ma_shader = bu_vls_strdup(&shader);

	/* Update the shader */
	mlib_free(regp);
	if ( mlib_setup( &mfHead, regp, rtip ) != 1 )  {
	    Tcl_AppendResult(interp, regp->reg_name, ": mlib_setup() failure\n", NULL);
	}
    }

    bu_vls_free(&shader);
    return TCL_OK;
}

/*
 *			S H _ O P T
 *
 *  Process RT-style command-line options.
 */
int
sh_opt(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    if ( argc < 2 )  {
	Tcl_AppendResult(interp, "Usage: sh_opt command_line_option(s)\n", NULL);
	return TCL_ERROR;
    }
    if ( get_args( argc, argv ) <= 0 )
	return TCL_ERROR;
    return TCL_OK;
}

/*
 *			S H _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to RT material/shader routines to
 *  the list of commands known by the given interpreter.
 */
void
sh_tcl_setup(Tcl_Interp *interp)
{
    (void)Tcl_CreateCommand(interp, "sh_directchange_rgb", sh_directchange_rgb,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    (void)Tcl_CreateCommand(interp, "sh_directchange_shader", sh_directchange_shader,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
    (void)Tcl_CreateCommand(interp, "sh_opt", sh_opt,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
