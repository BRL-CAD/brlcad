/*
 *			O V E R L A Y . C
 *
 * Functions -
 *	f_overlay		Read a UNIX-Plot file as an overlay
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"

#include "./sedit.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./mged_dm.h"

/* Usage:  overlay file.plot [name] */
int
f_overlay(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	char		*name;
	FILE		*fp;
	int		ret;
	struct rt_vlblock	*vbp;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( argc == 2 )
		name = "_PLOT_OVERLAY_";
	else
		name = argv[2];

	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return TCL_ERROR;
	}

	vbp = rt_vlblock_init();
	ret = rt_uplot_to_vlist( vbp, fp, Viewscale * 0.01 );
	fclose(fp);
	if( ret < 0 )  {
		rt_vlblock_free(vbp);
		return TCL_ERROR;
	}

	cvt_vlblock_to_solids( vbp, name, 0 );

	rt_vlblock_free(vbp);
	dmaflag = 1;
	return TCL_OK;
}

/* Usage:  labelvert solid(s) */
int
f_labelvert(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int	i;
	struct rt_vlblock	*vbp;
	struct directory	*dp;
	mat_t			mat;
	fastf_t			scale;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	vbp = rt_vlblock_init();
	mat_idn(mat);
	mat_inv( mat, Viewrot );
	scale = VIEWSIZE / 100;		/* divide by # chars/screen */

	for( i=1; i<argc; i++ )  {
		struct solid	*s;
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL )
			continue;
		/* Find uses of this solid in the solid table */
		FOR_ALL_SOLIDS(s)  {
			int	j;
			for( j = s->s_last; j >= 0; j-- )  {
				if( s->s_path[j] == dp )  {
					rt_label_vlist_verts( vbp, &s->s_vlist, mat, scale, base2local );
					break;
				}
			}
		}
	}

	cvt_vlblock_to_solids( vbp, "_LABELVERT_", 0 );

	rt_vlblock_free(vbp);
	dmaflag = 1;
	return TCL_OK;
}
