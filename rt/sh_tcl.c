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
}
