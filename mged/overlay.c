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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"

#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./sedit.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

/* Usage:  overlay file.plot [name] */
int
cmd_overlay(ClientData	clientData,
	    Tcl_Interp	*interp,
	    int		argc,
	    char	**argv)
{
	int		ret;
	struct bu_vls	char_size;
	int		ac;
	char		*av[5];

	CHECK_DBI_NULL;

	ac = argc + 1;
	bu_vls_init(&char_size);
	bu_vls_printf(&char_size, "%lf", view_state->vs_vop->vo_scale * 0.01);
	av[0] = argv[0];		/* command name */
	av[1] = argv[1];		/* plotfile name */
	av[2] = bu_vls_addr(&char_size);
	if (argc == 3) {
		av[3] = argv[2];	/* name */
		av[4] = (char *)0;
	} else
		av[3] = (char *)0;

	if ((ret = dgo_overlay_cmd(dgop, interp, ac, av)) == TCL_OK)
		update_views = 1;

	bu_vls_free(&char_size);
	return ret;
}

/* Usage:  labelvert solid(s) */
int
f_labelvert(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	int	i;
	struct rt_vlblock	*vbp;
	struct directory	*dp;
	mat_t			mat;
	fastf_t			scale;

	CHECK_DBI_NULL;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help labelvert");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	vbp = rt_vlblock_init();
	MAT_IDN(mat);
	bn_mat_inv(mat, view_state->vs_vop->vo_rotation);
	scale = view_state->vs_vop->vo_size / 100;		/* divide by # chars/screen */

	for( i=1; i<argc; i++ )  {
		struct solid	*s;
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL )
			continue;
		/* Find uses of this solid in the solid table */
		FOR_ALL_SOLIDS(s, &dgop->dgo_headSolid)  {
			if( db_full_path_search( &s->s_fullpath, dp ) )  {
				rt_label_vlist_verts( vbp, &s->s_vlist, mat, scale, base2local );
			}
		}
	}

	cvt_vlblock_to_solids( vbp, "_LABELVERT_", 0 );

	rt_vlblock_free(vbp);
	update_views = 1;
	return TCL_OK;
}
