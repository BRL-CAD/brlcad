/*
 *			S H _ T C L . C
 *
 *  Tcl interfaces to RT material & shader routines.
 *
 *  These routines are not for casual command-line use;
 *  as a result, the Tcl name for the function should be exactly
 *  the same as the C name for the underlying function.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif


#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "externs.h"

#define RT_CK_DBI_TCL(_p)	BU_CKMAG_TCL(interp,_p,DBI_MAGIC,"struct db_i")
#define RT_CK_RTI_TCL(_p)	BU_CKMAG_TCL(interp,_p, RTI_MAGIC, "struct rt_i")

/*
 *			S H _ D I R E C T C H A N G E _ R G B
 *
 *  Go poke the rgb values of a region, on the fly.
 *  This does not update the inmemory database,
 *  so any changes will vanish on next re-prep unless other measures
 *  are taken.
 */
sh_directchange_rgb( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_i	*rtip;
	struct region	*regp;
	struct directory *dp;
	float		r,g,b;
	char		buf[64];

	if( argc != 6 )  {
		Tcl_AppendResult(interp, "Usage: sh_directchange_rgb $rtip comb r g b\n", NULL);
		return TCL_ERROR;
	}

	r = atoi(argv[3+0]) / 255.;
	g = atoi(argv[3+1]) / 255.;
	b = atoi(argv[3+2]) / 255.;

	rtip = (struct rt_i *)atoi(argv[1]);
	RT_CK_RTI_TCL(rtip);

	if( rtip->needprep )  {
		Tcl_AppendResult(interp, "rt_prep() hasn't been called yet, error.\n", NULL);
		return TCL_ERROR;
	}

	if( (dp = db_lookup( rtip->rti_dbip, argv[2], LOOKUP_NOISY)) == DIR_NULL )  {
		Tcl_AppendResult(interp, argv[2], ": not found\n", NULL);
		return TCL_ERROR;
	}

	/* Find all region names which match /comb/ pattern */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		if( dp->d_flags & DIR_REGION )  {
			/* name will occur at end of region string w/leading slash */
		} else {
			/* name will occur anywhere, bracked by slashes */
		}

		/* XXX quick hack */
		if( strstr( regp->reg_name, argv[2] ) == NULL )  continue;

		/* Modify the region's color */
bu_log("sh_directchange_rgb() changing %s\n", regp->reg_name);
		VSET( regp->reg_mater.ma_color, r, g, b );

		/* Update the shader */
		mlib_free(regp);
		if( mlib_setup( regp, rtip ) != 1 )  {
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
sh_directchange_shader( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_i	*rtip;
	struct region	*regp;
	struct directory *dp;
	struct bu_vls	shader;
	char		buf[64];

	if( argc != 6 )  {
		Tcl_AppendResult(interp, "Usage: sh_directchange_shader $rtip comb shader_arg(s)\n", NULL);
		return TCL_ERROR;
	}

	rtip = (struct rt_i *)atoi(argv[1]);
	RT_CK_RTI_TCL(rtip);

	if( rtip->needprep )  {
		Tcl_AppendResult(interp, "rt_prep() hasn't been called yet, error.\n", NULL);
		return TCL_ERROR;
	}

	if( (dp = db_lookup( rtip->rti_dbip, argv[2], LOOKUP_NOISY)) == DIR_NULL )  {
		Tcl_AppendResult(interp, argv[2], ": not found\n", NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&shader);
	bu_vls_from_argv(&shader, argc-3, argv+3);

	/* Find all region names which match /comb/ pattern */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		if( dp->d_flags & DIR_REGION )  {
			/* name will occur at end of region string w/leading slash */
		} else {
			/* name will occur anywhere, bracked by slashes */
		}

		/* XXX quick hack */
		if( strstr( regp->reg_name, argv[2] ) == NULL )  continue;

		/* Modify the region's shader string */
bu_log("sh_directchange_shader() changing %s\n", regp->reg_name);
		if( regp->reg_mater.ma_shader )
			bu_free( (genptr_t)regp->reg_mater.ma_shader, "reg_mater.ma_shader");
		regp->reg_mater.ma_shader = bu_vls_strdup(&shader);

		/* Update the shader */
		mlib_free(regp);
		if( mlib_setup( regp, rtip ) != 1 )  {
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
sh_opt( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_i	*rtip;
	struct region	*regp;
	struct directory *dp;
	float		r,g,b;
	char		buf[64];

	if( argc < 2 )  {
		Tcl_AppendResult(interp, "Usage: sh_opt command_line_option(s)\n", NULL);
		return TCL_ERROR;
	}
	if( get_args( argc, argv ) <= 0 )
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
sh_tcl_setup(interp)
Tcl_Interp *interp;
{
	(void)Tcl_CreateCommand(interp, "sh_directchange_rgb", sh_directchange_rgb,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "sh_directchange_shader", sh_directchange_shader,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "sh_opt", sh_opt,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
}
