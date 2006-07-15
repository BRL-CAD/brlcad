/*                        T Y P E I N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */

/** @file typein.c
 *
 * This module contains functions which allow solid parameters to
 * be entered by keyboard.
 *
 * Functions -
 *	f_in		decides what solid needs to be entered and
 *			calls the appropriate solid parameter reader
 *	arb_in		reads ARB params from keyboard
 *	sph_in		reads sphere params from keyboard
 *	ell_in		reads params for all ELLs
 *	tor_in		gets params for torus from keyboard
 *	tgc_in		reads params for TGC from keyboard
 *	rcc_in		reads params for RCC from keyboard
 *	rec_in		reads params for REC from keyboard
 *	tec_in		reads params for TEC from keyboard
 *	trc_in		reads params for TRC from keyboard
 *	box_in		gets params for BOX and RAW from keyboard
 *	rpp_in		gets params for RPP from keyboard
 *	ars_in		gets ARS param from keyboard
 *	half_in		gets HALFSPACE params from keyboard
 *	rpc_in		reads right parabolic cylinder params from keyboard
 *	rhc_in		reads right hyperbolic cylinder params from keyboard
 *	epa_in		reads elliptical paraboloid params from keyboard
 *	ehy_in		reads elliptical hyperboloid params from keyboard
 *	eto_in		reads elliptical torus params from keyboard
 *	part_in		reads particle params from keyboard
 *	binunif_in	creates a binary object from a datafile
 *	extrude_in	reads extrude params from keyboard
 *	superell_in	reads params for all SUPERELLs
 *	metaball_in	reads params for a metaball grouping
 *	checkv		checks for zero vector from keyboard
 *
 * Authors -
 *	Charles M. Kennedy
 *	Keith A. Applin
 *	Michael J. Muuss
 *	Michael J. Markowski
 *	Erik Greenwald
 *
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "nurb.h"
#include "wdb.h"
#include "db.h"

#include "./ged.h"
#include "./mged_dm.h"
#include "./cmd.h"


void	aexists(char *name);

int		vals;		/* number of args for s_values[] */
char		**promp;	/* the prompt string */

char *p_half[] = {
	"Enter X, Y, Z of outward pointing normal vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter the distance from the origin: "
};

char *p_dsp_v4[] = {
	"Enter name of displacement-map file: ",
	"Enter width of displacement-map (number of values): ",
	"Enter length of displacement-map (number of values): ",
	"Normal Interpolation? 0=no 1=yes: ",
	"Cell size: ",
	"Unit elevation: "
};

char *p_dsp_v5[] = {
	"Take data from file or database binary object [f|o]:",
	"Enter name of file/object: ",
	"Enter width of displacement-map (number of values): ",
	"Enter length of displacement-map (number of values): ",
	"Normal Interpolation? 0=no 1=yes: ",
	"Cut direction [ad|lR|Lr] ",
	"Cell size: ",
	"Unit elevation: "
};

char *p_hf[] = {
	"Enter name of control file (or \"\" for none): ",
	"Enter name of data file (containing heights): ",
	"Enter 'cv' style format of data [h|n][s|u]c|s|i|l|d|8|16|32|64: ",
	"Enter number of values in 'x' direction: ",
	"Enter number of values in 'y' direction: ",
	"Enter '1' if data can be stored as 'short' in memory, or 0: ",
	"Enter factor to convert file data to mm: ",
	"Enter coordinates to position HF solid: ",
	"Enter Y coordinate: ",
	"Enter Z coordinate: ",
	"Enter direction vector for 'x' direction: ",
	"Enter Y coordinate: ",
	"Enter Z coordinate: ",
	"Enter direction vector for 'y' direction: ",
	"Enter Y coordinate: ",
	"Enter Z coordinate: ",
	"Enter length of HF in 'x' direction: ",
	"Enter width of HF in 'y' direction: ",
	"Enter scale factor for height (after conversion to mm): "
};

char *p_ebm[] = {
	"Enter name of bit-map file: ",
	"Enter width of bit-map (number of cells): ",
	"Enter height of bit-map (number of cells): ",
	"Enter extrusion distance: "
};

char *p_submodel[] = {
	"Enter name of treetop: ",
	"Enter space partitioning method: ",
	"Enter name of .g file (or \"\" for none): "
};

char *p_vol[] = {
	"Enter name of file containing voxel data: ",
	"Enter X, Y, Z dimensions of file (number of cells): ",
	"Enter Y dimension of file (number of cells): ",
	"Enter Z dimension of file (number of cells): ",
	"Enter lower threshold value: ",
	"Enter upper threshold value: ",
	"Enter X, Y, Z dimensions of a cell: ",
	"Enter Y dimension of a cell: ",
	"Enter Z dimension of a cell: ",
};

char *p_bot[] = {
	"Enter number of vertices: ",
	"Enter number of triangles: ",
	"Enter mode (1->surface, 2->solid, 3->plate): ",
	"Enter triangle orientation (1->unoriented, 2->counter-clockwise, 3->clockwise): ",
	"Enter X, Y, Z",
	"Enter Y",
	"Enter Z",
	"Enter three vertex numbers",
	"Enter second vertex number",
	"Enter third vertex number",
	"Enter face_mode (0->centered, 1->appended) and thickness",
	"Enter thickness"
};

char *p_arbn[] = {
	"Enter number of planes: ",
	"Enter coefficients",
	"Enter Y-coordinate of normal",
	"Enter Z-coordinate of normal",
	"Enter distance of plane along normal from origin"
};

char *p_pipe[] = {
	"Enter number of points: ",
	"Enter X, Y, Z, inner diameter, outer diameter, and bend radius for first point: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter inner diameter: ",
	"Enter outer diameter: ",
	"Enter bend radius: ",
	"Enter X, Y, Z, inner diameter, outer diameter, and bend radius",
	"Enter Y",
	"Enter Z",
	"Enter inner diameter",
	"Enter outer diameter",
	"Enter bend radius"
};

char *p_ars[] = {
	"Enter number of points per waterline, and number of waterlines: ",
	"Enter number of waterlines: ",
	"Enter X, Y, Z for First row point: ",
	"Enter Y for First row point: ",
	"Enter Z for First row point: ",
	"Enter X  Y  Z",
	"Enter Y",
	"Enter Z",
};

char *p_arb[] = {
	"Enter X, Y, Z for point 1: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 2: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 3: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 4: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 5: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 6: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 7: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z for point 8: ",
	"Enter Y: ",
	"Enter Z: "
};

char *p_sph[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius: "
};

char *p_ellg[] = {
	"Enter X, Y, Z of focus point 1: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of focus point 2: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter axis length L: "
};

char *p_ell1[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius of revolution: "
};

char *p_ell[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector C: ",
	"Enter Y: ",
	"Enter Z: "
};

char *p_tor[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of normal vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius 1: ",
	"Enter radius 2: "
};

char *p_rcc[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of height (H) vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius: "
};

char *p_tec[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of height (H) vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter ratio: "
};

char *p_rec[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of height (H) vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector B: ",
	"Enter Y: ",
	"Enter Z: "
};

char *p_trc[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of height (H) vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius of base: ",
	"Enter radius of top: "
};

char *p_tgc[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of height (H) vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter scalar c: ",
	"Enter scalar d: "
};

char *p_box[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector W: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector D: ",
	"Enter Y: ",
	"Enter Z: "
};

char *p_rpp[] = {
	"Enter XMIN, XMAX, YMIN, YMAX, ZMIN, ZMAX: ",
	"Enter XMAX: ",
	"Enter YMIN, YMAX, ZMIN, ZMAX: ",
	"Enter YMAX: ",
	"Enter ZMIN, ZMAX: ",
	"Enter ZMAX: "
};

char *p_orpp[] = {
	"Enter XMAX, YMAX, ZMAX: ",
	"Enter YMAX, ZMAX: ",
	"Enter ZMAX: "
};

char *p_rpc[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter rectangular half-width, r: "
};

char *p_part[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter v end radius: ",
	"Enter h end radius: "
};

char *p_rhc[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter rectangular half-width, r: ",
	"Enter apex-to-asymptotes distance, c: "
};

char *p_epa[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter magnitude of vector B: "
};

char *p_ehy[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter magnitude of vector B: ",
	"Enter apex-to-asymptotes distance, c: "
};

char *p_eto[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z, of normal vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius of revolution, r: ",
	"Enter X, Y, Z, of vector C: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter magnitude of elliptical semi-minor axis, d: "
};

char *p_binunif[] = {
	"Enter minor type (f,d,c,s,i,L,C,S,I, or L): ",
	"Enter name of file containing the data: ",
	"Enter number of values to read (-1 for entire file): "
};

char *p_extrude[] = {
	"Enter X, Y, Z of vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of H: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter name of sketch: ",
	"Enter K: ",
	NULL
};

char *p_grip[] = {
	"Enter X, Y, Z of center: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of normal: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter Magnitude: ",
	NULL
};

char *p_superell[] = {
	"Enter X, Y, Z of superellipse vertex: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector A: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector B: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter X, Y, Z of vector C: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter n, e of north-south and east-west power: ",
	"Enter e: "
};

char *p_metaball[] = {
	"Enter threshhold: ",
	"Enter number of points: ",
	"Enter X, Y, Z, field strength: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter field strength: ",
	"Enter X, Y, Z, field strength",
	"Enter Y",
	"Enter Z",
	"Enter field strength"
};

/*	F _ I N ( ) :  	decides which solid reader to call
 *			Used for manual entry of solids.
 */
int
f_in(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	register struct directory *dp;
	char			*name;
	struct rt_db_internal	internal;
	char			*new_cmd[3], **menu;
	int			c;
	int			do_solid_edit = 0;
	int			dont_draw = 0;
	int			nvals, (*fn_in)();
	int			arb_in(char **cmd_argvs, struct rt_db_internal *intern), box_in(char **cmd_argvs, struct rt_db_internal *intern), ehy_in(char **cmd_argvs, struct rt_db_internal *intern), ell_in(char **cmd_argvs, struct rt_db_internal *intern),
				epa_in(char **cmd_argvs, struct rt_db_internal *intern), eto_in(char **cmd_argvs, struct rt_db_internal *intern), half_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name), rec_in(char **cmd_argvs, struct rt_db_internal *intern),
				rcc_in(char **cmd_argvs, struct rt_db_internal *intern), rhc_in(char **cmd_argvs, struct rt_db_internal *intern), rpc_in(char **cmd_argvs, struct rt_db_internal *intern), rpp_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name), orpp_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name),
				sph_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name), tec_in(char **cmd_argvs, struct rt_db_internal *intern), tgc_in(char **cmd_argvs, struct rt_db_internal *intern), tor_in(char **cmd_argvs, struct rt_db_internal *intern), ars_in(int argc, char **argv, struct rt_db_internal *intern, char **promp),
				trc_in(char **cmd_argvs, struct rt_db_internal *intern), ebm_in(char **cmd_argvs, struct rt_db_internal *intern), vol_in(char **cmd_argvs, struct rt_db_internal *intern), hf_in(char **cmd_argvs, struct rt_db_internal *intern), bot_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt),
				dsp_in_v4(char **cmd_argvs, struct rt_db_internal *intern),dsp_in_v5(char **cmd_argvs, struct rt_db_internal *intern), submodel_in(char **cmd_argvs, struct rt_db_internal *intern), part_in(char **cmd_argvs, struct rt_db_internal *intern), pipe_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt),
				binunif_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name), arbn_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt), extrude_in(char **cmd_argvs, struct rt_db_internal *intern), grip_in(char **cmd_argvs, struct rt_db_internal *intern), superell_in(char **cmd_argvs, struct rt_db_internal *intern),
				metaball_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt);

	CHECK_DBI_NULL;

	if(argc < 1){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help in");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Parse options. */
	bu_optind = 1;		/* re-init bu_getopt() */
	bu_opterr = 0;          /* suppress bu_getopt()'s error message */
	while( (c=bu_getopt(argc,argv,"sf")) != EOF )  {
		switch(c)  {
		case 's':
			do_solid_edit = 1;
			break;
		case 'f':
			dont_draw = 1;
			break;
		default:
		  {
		    struct bu_vls tmp_vls;

		    bu_vls_init(&tmp_vls);
		    bu_vls_printf(&tmp_vls, "in: option '%c' unknown\n", bu_optopt);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }

		  break;
		}
	}
	argc -= bu_optind-1;
	argv += bu_optind-1;

	vals = 0;

	/* Get the name of the solid to be created */
	if( argc < 2 )  {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter name of solid: ", (char *)NULL);
	  return TCL_ERROR;
	}
	if( db_lookup( dbip,  argv[1], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[1] );
	  return TCL_ERROR;
	}
	if( dbip->dbi_version <= 4 && (int)strlen(argv[1]) >= NAMESIZE )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "ERROR, v4 names are limited to %d characters\n", NAMESIZE-1);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  return TCL_ERROR;
	}
	/* Save the solid name */
	name = argv[1];

	/* Get the solid type to be created and make it */
	if( argc < 3 )  {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter solid type: ", (char *)NULL);
	  return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL( &internal );

	/*
	 * Decide which solid to make and get the rest of the args
	 * make name <half|arb[4-8]|sph|ell|ellg|ell1|tor|tgc|tec|
			rec|trc|rcc|box|raw|rpp|rpc|rhc|epa|ehy|eto|superell>
	 */
	if( strcmp( argv[2], "ebm" ) == 0 )  {
		nvals = 4;
		menu = p_ebm;
		fn_in = ebm_in;
	} else if( strcmp( argv[2], "arbn" ) == 0 ) {
		switch( arbn_in(argc, argv, &internal, &p_arbn[0]) ) {
		case CMD_BAD:
		  Tcl_AppendResult(interp, "ERROR, ARBN not made!\n",
				   (char *)NULL);
		  rt_db_free_internal( &internal, &rt_uniresource );
		  return TCL_ERROR;
		case CMD_MORE:
		  return TCL_ERROR;
		}
		goto do_new_update;
	} else if( strcmp( argv[2], "bot" ) == 0 ) {
		switch( bot_in(argc, argv, &internal, &p_bot[0]) ) {
		case CMD_BAD:
		  Tcl_AppendResult(interp, "ERROR, BOT not made!\n",
				   (char *)NULL);
		  rt_db_free_internal( &internal, &rt_uniresource );
		  return TCL_ERROR;
		case CMD_MORE:
		  return TCL_ERROR;
		}
		goto do_new_update;
	} else if( strcmp( argv[2], "submodel" ) == 0 )  {
		nvals = 3;
		menu = p_submodel;
		fn_in = submodel_in;
	} else if( strcmp( argv[2], "vol" ) == 0 )  {
		nvals = 9;
		menu = p_vol;
		fn_in = vol_in;
	} else if( strcmp( argv[2], "hf" ) == 0 )  {
		if (dbip->dbi_version <= 4) {
			nvals = 19;
			menu = p_hf;
			fn_in = hf_in;
			Tcl_AppendResult(interp, "in: the height field is deprecated. Use the dsp primitive.\n", (char *)NULL);
		} else {
			Tcl_AppendResult(interp, "in: the height field is deprecated and not supported by this command when using a new\nstyle database. Use the dsp primitive.\n", (char *)NULL);
			return TCL_ERROR;
		}
	} else if (strcmp(argv[2], "poly") == 0 ||
		   strcmp(argv[2], "pg") == 0) {
		Tcl_AppendResult(interp, "in: the polysolid is deprecated and not supported by this command.\nUse the bot primitive.\n", (char *)NULL);
		return TCL_ERROR;
	} else if( strcmp( argv[2], "dsp" ) == 0 )  {
		if (dbip->dbi_version <= 4) {
			nvals = 6;
			menu = p_dsp_v4;
			fn_in = dsp_in_v4;
		} else {
			nvals = 8;
			menu = p_dsp_v5;
			fn_in = dsp_in_v5;
		}

	} else if( strcmp( argv[2], "pipe" ) == 0 ) {
		switch( pipe_in(argc, argv, &internal, &p_pipe[0]) ) {
		case CMD_BAD:
		  Tcl_AppendResult(interp, "ERROR, pipe not made!\n", (char *)NULL);
		  rt_db_free_internal( &internal, &rt_uniresource );
		  return TCL_ERROR;
		case CMD_MORE:
		  return TCL_ERROR;
		}
		goto do_new_update;
	} else if( strcmp( argv[2], "metaball" ) == 0 ) {
		switch( metaball_in(argc, argv, &internal, &p_metaball[0]) ) {
		case CMD_BAD:
		  Tcl_AppendResult(interp, "ERROR, metaball not made!\n", (char *)NULL);
		  rt_db_free_internal( &internal, &rt_uniresource );
		  return TCL_ERROR;
		case CMD_MORE:
		  return TCL_ERROR;
		}
		goto do_new_update;
	} else if( strcmp( argv[2], "ars" ) == 0 )  {
		switch( ars_in(argc, argv, &internal, &p_ars[0]) ) {
		case CMD_BAD:
		  Tcl_AppendResult(interp, "ERROR, ars not made!\n", (char *)NULL);
		  rt_db_free_internal( &internal, &rt_uniresource );
		  return TCL_ERROR;
		case CMD_MORE:
		  return TCL_ERROR;
		}
		goto do_new_update;
	} else if( strcmp( argv[2], "half" ) == 0 )  {
		nvals = 3*1 + 1;
		menu = p_half;
		fn_in = half_in;
	} else if( strncmp( argv[2], "arb", 3 ) == 0 )  {
		int n = atoi(&argv[2][3]);

		if(n < 4 || 8 < n){
			Tcl_AppendResult(interp, "ERROR: \"", argv[2],
					 "\" not supported!\n", (char *)0);
			Tcl_AppendResult(interp, "supported arbs: arb4 arb5 arb6 arb7 arb8\n",
						 (char *)0);
			return TCL_ERROR;
		}

		nvals = 3*n;
		menu = p_arb;
		fn_in = arb_in;
	} else if( strcmp( argv[2], "sph" ) == 0 )  {
		nvals = 3*1 + 1;
		menu = p_sph;
		fn_in = sph_in;
	} else if( strcmp( argv[2], "ellg" ) == 0 )  {
		nvals = 3*2 + 1;
		menu = p_ellg;
		fn_in = ell_in;
	} else if( strcmp( argv[2], "ell" ) == 0 )  {
		nvals = 3*4;
		menu = p_ell;
		fn_in = ell_in;
	} else if( strcmp( argv[2], "ell1" ) == 0 )  {
		nvals = 3*2 + 1;
		menu = p_ell1;
		fn_in = ell_in;
	} else if( strcmp( argv[2], "tor" ) == 0 )  {
		nvals = 3*2 + 2;
		menu = p_tor;
		fn_in = tor_in;
	} else if( strcmp( argv[2], "tgc" ) == 0 ) {
		nvals = 3*4 + 2;
		menu = p_tgc;
		fn_in = tgc_in;
	} else if( strcmp( argv[2], "tec" ) == 0 )  {
		nvals = 3*4 + 1;
		menu = p_tec;
		fn_in = tec_in;
	} else if( strcmp( argv[2], "rec" ) == 0 )  {
		nvals = 3*4;
		menu = p_rec;
		fn_in = rec_in;
	} else if( strcmp( argv[2], "trc" ) == 0 )  {
		nvals = 3*2 + 2;
		menu = p_trc;
		fn_in = trc_in;
	} else if( strcmp( argv[2], "rcc" ) == 0 )  {
		nvals = 3*2 + 1;
		menu = p_rcc;
		fn_in = rcc_in;
	} else if( strcmp( argv[2], "box" ) == 0
		|| strcmp( argv[2], "raw" ) == 0 )  {
		nvals = 3*4;
		menu = p_box;
		fn_in = box_in;
	} else if( strcmp( argv[2], "rpp" ) == 0 )  {
		nvals = 3*2;
		menu = p_rpp;
		fn_in = rpp_in;
	} else if( strcmp( argv[2], "orpp" ) == 0 )  {
		nvals = 3*1;
		menu = p_orpp;
		fn_in = orpp_in;
	} else if( strcmp( argv[2], "rpc" ) == 0 )  {
		nvals = 3*3 + 1;
		menu = p_rpc;
		fn_in = rpc_in;
	} else if( strcmp( argv[2], "rhc" ) == 0 )  {
		nvals = 3*3 + 2;
		menu = p_rhc;
		fn_in = rhc_in;
	} else if( strcmp( argv[2], "epa" ) == 0 )  {
		nvals = 3*3 + 1;
		menu = p_epa;
		fn_in = epa_in;
	} else if( strcmp( argv[2], "ehy" ) == 0 )  {
		nvals = 3*3 + 2;
		menu = p_ehy;
		fn_in = ehy_in;
	} else if( strcmp( argv[2], "eto" ) == 0 )  {
		nvals = 3*3 + 2;
		menu = p_eto;
		fn_in = eto_in;
	} else if( strcmp( argv[2], "part" ) == 0 )  {
		nvals = 2*3 + 2;
		menu = p_part;
		fn_in = part_in;
	} else if( strcmp( argv[2], "binunif" ) == 0 ) {
		if (dbip->dbi_version <= 4) {
			Tcl_AppendResult(interp, "in: the binunif primitive is not supported by this command when using an old style database", (char *)NULL);
			return TCL_ERROR;
		} else {
			nvals = 3;
			menu = p_binunif;
			fn_in = binunif_in;
			do_solid_edit = 0;
			dont_draw = 1;
		}
	} else if (strcmp(argv[2], "extrude") == 0) {
		nvals = 4*3 + 2;
		menu = p_extrude;
		fn_in = extrude_in;
	} else if (strcmp(argv[2], "grip") == 0) {
		nvals = 2*3 + 1;
		menu = p_grip;
		fn_in = grip_in;
	} else if( strcmp( argv[2], "superell" ) == 0 )  {
		nvals = 3*4 + 2;
		menu = p_superell;
		fn_in = superell_in;
	} else if (strcmp(argv[2], "cline") == 0 ||
		   strcmp(argv[2], "grip") == 0 ||
		   strcmp(argv[2], "nmg") == 0 ||
		   strcmp(argv[2], "nurb") == 0 ||
		   strcmp(argv[2], "sketch") == 0 ||
		   strcmp(argv[2], "spline") == 0) {
		Tcl_AppendResult(interp, "in: the ", argv[2], " primitive is not supported by this command", (char *)NULL);
		return TCL_ERROR;
	} else {
	  Tcl_AppendResult(interp, "f_in:  ", argv[2], " is not a known primitive\n",
			   (char *)NULL);
	  return TCL_ERROR;
	}

	/* Read arguments */
	if( argc < 3+nvals )  {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, menu[argc-3], (char *)NULL);
	  return TCL_ERROR;
	}

	if (fn_in(argv, &internal, name) != 0)  {
	  Tcl_AppendResult(interp, "ERROR, ", argv[2], " not made!\n", (char *)NULL);
	  if( internal.idb_ptr ) {
		  /* a few input functions do not use the internal pointer
		   * only free it, if it has been used
		   */
		  rt_db_free_internal( &internal, &rt_uniresource );
	  }
	  return TCL_ERROR;
	}

do_new_update:
	/* The function may have already written via LIBWDB */
	if( internal.idb_ptr != NULL )  {
		if( (dp=db_diradd( dbip, name, -1L, 0, DIR_SOLID, (genptr_t)&internal.idb_type)) == DIR_NULL )  {
			rt_db_free_internal( &internal, &rt_uniresource );
			Tcl_AppendResult(interp, "Cannot add '", name, "' to directory\n", (char *)NULL );
			return TCL_ERROR;
		}
		if( rt_db_put_internal( dp, dbip, &internal, &rt_uniresource ) < 0 )
		{
			rt_db_free_internal( &internal, &rt_uniresource );
			TCL_WRITE_ERR_return;
		}
	}

	if( dont_draw )  return TCL_OK;

	/* draw the newly "made" solid */
	new_cmd[0] = "e";
	new_cmd[1] = name;
	new_cmd[2] = (char *)NULL;
	(void)cmd_draw( clientData, interp, 2, new_cmd );

	if( do_solid_edit )  {
		/* Also kick off solid edit mode */
		new_cmd[0] = "sed";
		new_cmd[1] = name;
		new_cmd[2] = (char *)NULL;
		(void)f_sed( clientData, interp, 2, new_cmd );
	}
	return TCL_OK;
}

int
binunif_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
	unsigned int minor_type;

	CHECK_DBI_NULL;

	intern->idb_ptr = NULL;

	if( strlen( cmd_argvs[3] ) != 1 ) {
		bu_log( "Unrecognized minor type (%s)\n", cmd_argvs[3] );
		return 1;
	}

	switch( *cmd_argvs[3] ) {
		case 'f':
			minor_type = DB5_MINORTYPE_BINU_FLOAT;
			break;
		case 'd':
			minor_type = DB5_MINORTYPE_BINU_DOUBLE;
			break;
		case 'c':
			minor_type = DB5_MINORTYPE_BINU_8BITINT;
			break;
		case 's':
			minor_type = DB5_MINORTYPE_BINU_16BITINT;
			break;
		case 'i':
			minor_type = DB5_MINORTYPE_BINU_32BITINT;
			break;
		case 'l':
			minor_type = DB5_MINORTYPE_BINU_64BITINT;
			break;
		case 'C':
			minor_type = DB5_MINORTYPE_BINU_8BITINT_U;
			break;
		case 'S':
			minor_type = DB5_MINORTYPE_BINU_16BITINT_U;
			break;
		case 'I':
			minor_type = DB5_MINORTYPE_BINU_32BITINT_U;
			break;
		case 'L':
			minor_type = DB5_MINORTYPE_BINU_64BITINT_U;
			break;
		default:
			bu_log( "Unrecognized minor type (%c)\n", *cmd_argvs[3] );
			return 1;
	}
	if( rt_mk_binunif( wdbp, name, cmd_argvs[4], minor_type, atol(cmd_argvs[5]) ) ) {
		bu_log( "Failed to create binary object %s from file %s\n",
			name, cmd_argvs[4] );
		return 1;
	}

	return 0;
}

/*			E B M _ I N
 *
 *	Read EBM solid from keyboard
 *
 */
int
ebm_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	struct rt_ebm_internal	*ebm;

	CHECK_DBI_NULL;

	BU_GETSTRUCT( ebm, rt_ebm_internal );
	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_EBM;
	intern->idb_meth = &rt_functab[ID_EBM];
	intern->idb_ptr = (genptr_t)ebm;
	ebm->magic = RT_EBM_INTERNAL_MAGIC;

	strcpy( ebm->file, cmd_argvs[3] );
	ebm->xdim = atoi( cmd_argvs[4] );
	ebm->ydim = atoi( cmd_argvs[5] );
	ebm->tallness = atof( cmd_argvs[6] ) * local2base;
	MAT_IDN( ebm->mat );

	return( 0 );
}

/*			S U B M O D E L _ I N
 *
 *	Read submodel from keyboard
 *
 */
int
submodel_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	struct rt_submodel_internal	*sip;

	CHECK_DBI_NULL;

	BU_GETSTRUCT( sip, rt_submodel_internal );
	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_SUBMODEL;
	intern->idb_meth = &rt_functab[ID_SUBMODEL];
	intern->idb_ptr = (genptr_t)sip;
	sip->magic = RT_SUBMODEL_INTERNAL_MAGIC;

	bu_vls_init( &sip->treetop );
	bu_vls_strcpy( &sip->treetop, cmd_argvs[3] );
	sip->meth = atoi( cmd_argvs[4] );
	bu_vls_init( &sip->file );
	bu_vls_strcpy( &sip->file, cmd_argvs[5] );

	return( 0 );
}

/*			D S P _ I N
 *
 *	Read DSP solid from keyboard
 */
int
dsp_in_v4 (char **cmd_argvs, struct rt_db_internal *intern)
{
	struct rt_dsp_internal	*dsp;

	BU_GETSTRUCT( dsp, rt_dsp_internal );
	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_DSP;
	intern->idb_meth = &rt_functab[ID_DSP];
	intern->idb_ptr = (genptr_t)dsp;
	dsp->magic = RT_DSP_INTERNAL_MAGIC;

	bu_vls_init( &dsp->dsp_name );
	bu_vls_strcpy( &dsp->dsp_name, cmd_argvs[3] );

	dsp->dsp_xcnt = atoi( cmd_argvs[4] );
	dsp->dsp_ycnt = atoi( cmd_argvs[5] );
	dsp->dsp_smooth = atoi( cmd_argvs[6] );
	MAT_IDN( dsp->dsp_stom );

	dsp->dsp_stom[0] = dsp->dsp_stom[5] =
		atof( cmd_argvs[7] ) * local2base;

	dsp->dsp_stom[10] = atof( cmd_argvs[8] ) * local2base;

	bn_mat_inv( dsp->dsp_mtos, dsp->dsp_stom );

	return( 0 );
}

extern void dsp_dump(struct rt_dsp_internal *dsp);

/*			D S P _ I N
 *
 *	Read DSP solid from keyboard
 */
int
dsp_in_v5 (char **cmd_argvs, struct rt_db_internal *intern)
{
	struct rt_dsp_internal	*dsp;

	BU_GETSTRUCT( dsp, rt_dsp_internal );
	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_DSP;
	intern->idb_meth = &rt_functab[ID_DSP];
	intern->idb_ptr = (genptr_t)dsp;
	dsp->magic = RT_DSP_INTERNAL_MAGIC;

	if (*cmd_argvs[3] == 'f' || *cmd_argvs[3] == 'F')
		dsp->dsp_datasrc = RT_DSP_SRC_FILE;
	else if (*cmd_argvs[3] == 'O' || *cmd_argvs[3] == 'o')
		dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
	else
		return -1;

	bu_vls_init( &dsp->dsp_name );
	bu_vls_strcpy( &dsp->dsp_name, cmd_argvs[4] );

	dsp->dsp_xcnt = atoi( cmd_argvs[5] );
	dsp->dsp_ycnt = atoi( cmd_argvs[6] );
	dsp->dsp_smooth = atoi( cmd_argvs[7] );
	switch ( *cmd_argvs[8] ) {
	case 'a':	/* adaptive */
	case 'A':
	    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;
	    break;
	case 'l':	/* lower left to upper right */
	    dsp->dsp_cuttype = DSP_CUT_DIR_llUR;
	    break;
	case 'L':	/* Upper Left to lower right */
	    dsp->dsp_cuttype = DSP_CUT_DIR_ULlr;
	    break;
	default:
	    bu_log("Error: dsp_cuttype:\"%s\"\n", cmd_argvs[8]);
	    return -1;
	    break;
	}

	MAT_IDN( dsp->dsp_stom );

	dsp->dsp_stom[0] = dsp->dsp_stom[5] =
		atof( cmd_argvs[9] ) * local2base;

	dsp->dsp_stom[10] = atof( cmd_argvs[10] ) * local2base;

	bn_mat_inv( dsp->dsp_mtos, dsp->dsp_stom );

	return( 0 );
}



/*			H F _ I N
 *
 *	Read HF solid from keyboard
 *
 */
int
hf_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	struct rt_hf_internal	*hf;

	CHECK_DBI_NULL;

	BU_GETSTRUCT( hf, rt_hf_internal );
	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_HF;
	intern->idb_meth = &rt_functab[ID_HF];
	intern->idb_ptr = (genptr_t)hf;
	hf->magic = RT_HF_INTERNAL_MAGIC;

	strcpy( hf->cfile, cmd_argvs[3] );
	strcpy( hf->dfile, cmd_argvs[4] );
	strncpy( hf->fmt, cmd_argvs[5], 7 );
	hf->fmt[7] = '\0';
	hf->w = atoi( cmd_argvs[6] );
	hf->n = atoi( cmd_argvs[7] );
	hf->shorts = atoi( cmd_argvs[8] );
	hf->file2mm = atof( cmd_argvs[9] );
	hf->v[0] = atof( cmd_argvs[10] ) * local2base;
	hf->v[1] = atof( cmd_argvs[11] ) * local2base;
	hf->v[2] = atof( cmd_argvs[12] ) * local2base;
	hf->x[0] = atof( cmd_argvs[13] );
	hf->x[1] = atof( cmd_argvs[14] );
	hf->x[2] = atof( cmd_argvs[15] );
	hf->y[0] = atof( cmd_argvs[16] );
	hf->y[1] = atof( cmd_argvs[17] );
	hf->y[2] = atof( cmd_argvs[18] );
	hf->xlen = atof( cmd_argvs[19] ) * local2base;
	hf->ylen = atof( cmd_argvs[20] ) * local2base;
	hf->zscale = atof( cmd_argvs[21] );

	if( hf->w < 2 || hf->n < 2 )
	{
		Tcl_AppendResult(interp, "ERROR: length or width of fta file is too small\n", (char *)NULL );
		return( 1 );
	}

	if( hf->xlen <= 0 || hf->ylen <= 0 )
	{
		Tcl_AppendResult(interp, "ERROR: length and width of HF solid must be greater than 0\n", (char *)NULL );
		return( 1 );
	}

	/* XXXX should check for orthogonality of 'x' and 'y' vectors */

	if( !(hf->mp = bu_open_mapped_file( hf->dfile, "hf" )) )
	{
		Tcl_AppendResult(interp, "ERROR: cannot open data file\n", (char *)NULL );
		hf->mp = (struct bu_mapped_file *)NULL;
		return( 1 );
	}

	return( 0 );
}

/*			V O L _ I N
 *
 *	Read VOL solid from keyboard
 *
 */
int
vol_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	struct rt_vol_internal	*vol;

	CHECK_DBI_NULL;

	BU_GETSTRUCT( vol, rt_vol_internal );
	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_VOL;
	intern->idb_meth = &rt_functab[ID_VOL];
	intern->idb_ptr = (genptr_t)vol;
	vol->magic = RT_VOL_INTERNAL_MAGIC;

	strcpy( vol->file, cmd_argvs[3] );
	vol->xdim = atoi( cmd_argvs[4] );
	vol->ydim = atoi( cmd_argvs[5] );
	vol->zdim = atoi( cmd_argvs[6] );
	vol->lo = atoi( cmd_argvs[7] );
	vol->hi = atoi( cmd_argvs[8] );
	vol->cellsize[0] = atof( cmd_argvs[9] ) * local2base;
	vol->cellsize[1] = atof( cmd_argvs[10] ) * local2base;
	vol->cellsize[2] = atof( cmd_argvs[11] ) * local2base;
	MAT_IDN( vol->mat );

	return( 0 );
}

/*
 *			B O T _ I N
 */
int
bot_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt)
{
	int i;
	int num_verts, num_faces;
	int mode, orientation;
	int arg_count;
	struct rt_bot_internal *bot;

	CHECK_DBI_NULL;

	if( argc < 7 ) {
		Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
		return CMD_MORE;
	}

	num_verts = atoi( argv[3] );
	if( num_verts < 3 )
	{
		Tcl_AppendResult(interp, "Invalid number of vertices (must be at least 3)\n", (char *)NULL);
		return CMD_BAD;
	}

	num_faces = atoi( argv[4] );
	if( num_faces < 1 )
	{
		Tcl_AppendResult(interp, "Invalid number of triangles (must be at least 1)\n", (char *)NULL);
		return CMD_BAD;
	}

	mode = atoi( argv[5] );
	if( mode < 1 || mode > 3 )
	{
		Tcl_AppendResult(interp, "Invalid mode (must be 1, 2, or 3)\n", (char *)NULL );
		return CMD_BAD;
	}

	orientation = atoi( argv[6] );
	if( orientation < 1 || orientation > 3 )
	{
		Tcl_AppendResult(interp, "Invalid orientation (must be 1, 2, or 3)\n", (char *)NULL );
		return CMD_BAD;
	}

	arg_count = argc - 7;
	if( arg_count < num_verts*3 )
	{
		struct bu_vls tmp_vls;

		bu_vls_init( &tmp_vls );
		bu_vls_printf( &tmp_vls, "%s for vertex %d : ", prompt[4+arg_count%3], arg_count/3 );

		Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		return CMD_MORE;
	}

	arg_count = argc - 7 - num_verts*3;
	if( arg_count < num_faces*3 )
	{
		struct bu_vls tmp_vls;

		bu_vls_init( &tmp_vls );
		bu_vls_printf( &tmp_vls, "%s for triangle %d : ", prompt[7+arg_count%3], arg_count/3 );

		Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		return CMD_MORE;
	}

	if( mode == RT_BOT_PLATE )
	{
		arg_count = argc - 7 - num_verts*3 - num_faces*3;
		if( arg_count < num_faces*2 )
		{
			struct bu_vls tmp_vls;

			bu_vls_init( &tmp_vls );
			bu_vls_printf( &tmp_vls, "%s for face %d : ", prompt[10+arg_count%2], arg_count/2 );

			Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);

			return CMD_MORE;
		}
	}

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_BOT;
	intern->idb_meth = &rt_functab[ID_BOT];
	bot = (struct rt_bot_internal *)bu_calloc( 1, sizeof( struct rt_bot_internal ), "rt_bot_internal" );
	intern->idb_ptr = (genptr_t)bot;
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->num_vertices = num_verts;
	bot->num_faces = num_faces;
	bot->mode = mode;
	bot->orientation = orientation;
	bot->faces = (int *)bu_calloc( bot->num_faces * 3, sizeof( int ), "bot faces" );
	bot->vertices = (fastf_t *)bu_calloc( bot->num_vertices * 3, sizeof( fastf_t ), "bot vertices" );
	bot->thickness = (fastf_t *)NULL;
	bot->face_mode = (struct bu_bitv *)NULL;

	for( i=0 ; i<num_verts ; i++ )
	{
		bot->vertices[i*3] = atof( argv[7+i*3] ) * local2base;
		bot->vertices[i*3+1] = atof( argv[8+i*3] ) * local2base;
		bot->vertices[i*3+2] = atof( argv[9+i*3] ) * local2base;
	}

	arg_count = 7 + num_verts*3;
	for( i=0 ; i<num_faces ; i++ )
	{
		bot->faces[i*3] = atoi( argv[arg_count + i*3] );
		bot->faces[i*3+1] = atoi( argv[arg_count + i*3 + 1] );
		bot->faces[i*3+2] = atoi( argv[arg_count + i*3 + 2] );
	}

	if( mode == RT_BOT_PLATE )
	{
		arg_count = 7 + num_verts*3 + num_faces*3;
		bot->thickness = (fastf_t *)bu_calloc( num_faces, sizeof( fastf_t ), "bot thickness" );
		bot->face_mode = bu_bitv_new( num_faces );
		bu_bitv_clear( bot->face_mode );
		for( i=0 ; i<num_faces ; i++ )
		{
			int j;

			j = atoi( argv[arg_count + i*2] );
			if( j == 1 )
				BU_BITSET( bot->face_mode, i );
			else if( j != 0 )
			{
				Tcl_AppendResult(interp, "Invalid face mode (must be 0 or 1)\n", (char *)NULL );
				return CMD_BAD;
			}
			bot->thickness[i] = atof( argv[arg_count + i*2 + 1] ) * local2base;
		}
	}

	return CMD_OK;
}

/*
 *			A R B N _ I N
 */
int 
arbn_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt)
{
	struct rt_arbn_internal *arbn;
	int num_planes=0;
	int i;

	CHECK_DBI_NULL;

	if( argc < 4 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
	  return CMD_MORE;
	}

	num_planes = atoi( argv[3] );

	if( argc < num_planes * 4 + 4 ) {
		struct bu_vls tmp_vls;

		bu_vls_init( &tmp_vls );
		bu_vls_printf( &tmp_vls, "%s for plane %d : ", prompt[(argc-4)%4 + 1], 1+(argc-4)/4 );

		Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		return CMD_MORE;
	}

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_ARBN;
	intern->idb_meth = &rt_functab[ID_ARBN];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof( struct rt_arbn_internal ),
					       "rt_arbn_internal" );
	arbn = (struct rt_arbn_internal *)intern->idb_ptr;
	arbn->magic = RT_ARBN_INTERNAL_MAGIC;
	arbn->neqn = num_planes;
	arbn->eqn = (plane_t *)bu_calloc( arbn->neqn, sizeof( plane_t ), "arbn planes" );

	/* Normal is unscaled, should have unit length; d is scaled */
	for( i=0 ; i<arbn->neqn ; i++ ) {
		arbn->eqn[i][0] = atof( argv[4+i*4] );
		arbn->eqn[i][1] = atof( argv[4+i*4+1] );
		arbn->eqn[i][2] = atof( argv[4+i*4+2] );
		arbn->eqn[i][3] = atof( argv[4+i*4+3] ) * local2base;
	}

	return CMD_OK;
}

/*
 *			P I P E _ I N
 */
int
pipe_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt)
{
	register struct rt_pipe_internal *pipe;
	int i,num_points;

	CHECK_DBI_NULL;

	if( argc < 4 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
	  return CMD_MORE;
	}

	num_points = atoi( argv[3] );
	if( num_points < 2 )
	{
		Tcl_AppendResult(interp, "Invalid number of points (must be at least 2)\n", (char *)NULL);
		return CMD_BAD;
	}

	if( argc < 10 )
	{
		Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
		return CMD_MORE;
	}

	if( argc < 4 + num_points*6 )
	{
		struct bu_vls tmp_vls;

		bu_vls_init( &tmp_vls );
		bu_vls_printf( &tmp_vls, "%s for point %d : ", prompt[7+(argc-10)%6], 1+(argc-4)/6 );

		Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		return CMD_MORE;
	}

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_PIPE;
	intern->idb_meth = &rt_functab[ID_PIPE];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof( struct rt_pipe_internal ), "rt_pipe_internal" );
	pipe = (struct rt_pipe_internal *)intern->idb_ptr;
	pipe->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
	BU_LIST_INIT( &pipe->pipe_segs_head );
	for( i=4 ; i<argc ; i+= 6 )
	{
		struct wdb_pipept *pipept;

		pipept = (struct wdb_pipept *)bu_malloc( sizeof( struct wdb_pipept ), "wdb_pipept" );
		pipept->pp_coord[0] = atof( argv[i] ) * local2base;
		pipept->pp_coord[1] = atof( argv[i+1] ) * local2base;
		pipept->pp_coord[2] = atof( argv[i+2] ) * local2base;
		pipept->pp_id = atof( argv[i+3] ) * local2base;
		pipept->pp_od = atof( argv[i+4] ) * local2base;
		pipept->pp_bendradius = atof( argv[i+5] ) * local2base;

		BU_LIST_INSERT( &pipe->pipe_segs_head, &pipept->l );
	}

	if( rt_pipe_ck(  &pipe->pipe_segs_head ) )
	{
		Tcl_AppendResult(interp, "Illegal pipe, solid not made!!\n", (char *)NULL );
		return CMD_BAD;
	}

	return CMD_OK;
}

/*
 *			A R S _ I N
 */
int
ars_in(int argc, char **argv, struct rt_db_internal *intern, char **promp)
{
	register struct rt_ars_internal	*arip;
	register int			i;
	int			total_points;
	int			cv;	/* current curve (waterline) # */
	int			axis;	/* current fastf_t in waterline */
	int			ncurves_minus_one;
	int num_pts = 0;
	int num_curves = 0;
	int vals_present, total_vals_needed;
	struct bu_vls tmp_vls;

	CHECK_DBI_NULL;

	vals_present = argc - 3;

	if (vals_present > 0) {
	    num_pts = atoi(argv[3]);
	    if (num_pts < 3 ) {
		Tcl_AppendResult(interp,
				 "points per waterline must be >= 3\n",
				 (char *)NULL);
		intern->idb_meth = &rt_functab[ID_ARS];
		return CMD_BAD;
	    }
	}

	if (vals_present > 1) {
	    num_curves = atoi(argv[4]);
	    if (num_curves < 3) {
		Tcl_AppendResult(interp, "points per waterline must be >= 3\n",
				 (char *)NULL);
		intern->idb_meth = &rt_functab[ID_ARS];
		return CMD_BAD;
	    }
	}

	if (vals_present < 5) {
	    /* for #rows, #pts/row & first point,
	     * pre-formatted prompts exist
	     */
	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   promp[vals_present], (char *)NULL);
	  return CMD_MORE;
	}

	total_vals_needed = 2 +		/* #rows, #pts/row */
	    (ELEMENTS_PER_PT * 2) +	/* the first point, and very last */
	    (num_pts * ELEMENTS_PER_PT * (num_curves-2)); /* the curves */

	if (vals_present < (total_vals_needed - ELEMENTS_PER_PT)) {
	    /* if we're looking for points on the curves, and not
	     * the last point which makes up the last curve, we
	     * have to format up a prompt string
	     */
	    bu_vls_init(&tmp_vls);

	    switch ((vals_present-2) % 3) {
	    case 0:
		bu_vls_printf(&tmp_vls, "%s for Waterline %d, Point %d : ",
			      promp[5],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts );
		break;
	    case 1:
		bu_vls_printf(&tmp_vls, "%s for Waterline %d, Point %d : ",
			      promp[6],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts );
		break;
	    case 2:
		bu_vls_printf(&tmp_vls, "%s for Waterline %d, Point %d : ",
			      promp[7],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts );
		break;
	    }

	    Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls),
			     (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    return CMD_MORE;
	} else if (vals_present < total_vals_needed) {
	    /* we're looking for the last point which is used for all points
	     * on the last curve
	     */
	    bu_vls_init(&tmp_vls);


	    switch ((vals_present-2) % 3) {
	    case 0:
		bu_vls_printf(&tmp_vls, "%s for pt of last Waterline : ",
			      promp[5],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts );
		break;
	    case 1:
		bu_vls_printf(&tmp_vls, "%s for pt of last Waterline : ",
			      promp[6],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts );
		break;
	    case 2:
		bu_vls_printf(&tmp_vls, "%s for pt of last Waterline : ",
			      promp[7],
			      1+(argc-8)/3/num_pts,
			      ((argc-8)/3)%num_pts );
		break;
	    }


	    Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls),
			     (char *)NULL);
	    bu_vls_free(&tmp_vls);

	    return CMD_MORE;
	}

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_ARS;
	intern->idb_meth = &rt_functab[ID_ARS];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ars_internal), "rt_ars_internal");
	arip = (struct rt_ars_internal *)intern->idb_ptr;
	arip->magic = RT_ARS_INTERNAL_MAGIC;
	arip->pts_per_curve = num_pts;
	arip->ncurves = num_curves;
	ncurves_minus_one = arip->ncurves - 1;
	total_points = arip->ncurves * arip->pts_per_curve;

	arip->curves = (fastf_t **)bu_malloc(
		(arip->ncurves+1) * sizeof(fastf_t **), "ars curve ptrs" );
	for( i=0; i < arip->ncurves+1; i++ )  {
		/* Leave room for first point to be repeated */
		arip->curves[i] = (fastf_t *)bu_malloc(
		    (arip->pts_per_curve+1) * sizeof(point_t),
		    "ars curve" );
	}

	/* fill in the point of the first row */
	arip->curves[0][0] = atof(argv[5]) * local2base;
	arip->curves[0][1] = atof(argv[6]) * local2base;
	arip->curves[0][2] = atof(argv[7]) * local2base;

	/* The first point is duplicated across the first curve */
	for (i=1 ; i < arip->pts_per_curve ; ++i) {
		VMOVE( arip->curves[0]+3*i, arip->curves[0] );
	}

	cv = 1;
	axis = 0;
	/* scan each of the other points we've already got */
	for (i=8 ; i < argc && i < total_points * ELEMENTS_PER_PT ; ++i) {
		arip->curves[cv][axis] = atof(argv[i]) * local2base;
		if (++axis >= arip->pts_per_curve * ELEMENTS_PER_PT) {
			axis = 0;
			cv++;
		}
	}

	/* The first point is duplicated across the last curve */
	for (i=1 ; i < arip->pts_per_curve ; ++i) {
		VMOVE( arip->curves[ncurves_minus_one]+3*i,
			arip->curves[ncurves_minus_one] );
	}

	return CMD_OK;
}

/*   H A L F _ I N ( ) :   	reads halfspace parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
half_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
	vect_t norm;
	double d;

	CHECK_DBI_NULL;

	intern->idb_ptr = NULL;

	norm[X] = atof(cmd_argvs[3+0]);
	norm[Y] = atof(cmd_argvs[3+1]);
	norm[Z] = atof(cmd_argvs[3+2]);
	d = atof(cmd_argvs[3+3]) * local2base;

	if (MAGNITUDE(norm) < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, normal vector is too small!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	VUNITIZE( norm );
	if( mk_half( wdbp, name, norm, d ) < 0 )
		return 1;	/* failure */
	return 0;	/* success */
}

/*   A R B _ I N ( ) :   	reads arb parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
arb_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i, j, n;
	struct rt_arb_internal	*aip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_ARB8;
	intern->idb_meth = &rt_functab[ID_ARB8];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal),
		"rt_arb_internal" );
	aip = (struct rt_arb_internal *)intern->idb_ptr;
	aip->magic = RT_ARB_INTERNAL_MAGIC;

	n = atoi(&cmd_argvs[2][3]);	/* get # from "arb#" */
	for (j = 0; j < n; j++)
	  for (i = 0; i < ELEMENTS_PER_PT; i++)
	    aip->pt[j][i] = atof(cmd_argvs[3+i+3*j]) * local2base;

	if (!strcmp("arb4", cmd_argvs[2])) {
		VMOVE( aip->pt[7], aip->pt[3] );
		VMOVE( aip->pt[6], aip->pt[3] );
		VMOVE( aip->pt[5], aip->pt[3] );
		VMOVE( aip->pt[4], aip->pt[3] );
		VMOVE( aip->pt[3], aip->pt[0] );
	} else if (!strcmp("arb5", cmd_argvs[2])) {
		VMOVE( aip->pt[7], aip->pt[4] );
		VMOVE( aip->pt[6], aip->pt[4] );
		VMOVE( aip->pt[5], aip->pt[4] );
	} else if (!strcmp("arb6", cmd_argvs[2])) {
		VMOVE( aip->pt[7], aip->pt[5] );
		VMOVE( aip->pt[6], aip->pt[5] );
		VMOVE( aip->pt[5], aip->pt[4] );
	} else if (!strcmp("arb7", cmd_argvs[2])) {
		VMOVE( aip->pt[7], aip->pt[4] );
	}

	return(0);	/* success */
}

/*   S P H _ I N ( ) :   	reads sph parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
sph_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
	point_t			center;
	fastf_t			r;
	int			i;

	CHECK_DBI_NULL;

	intern->idb_ptr = NULL;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		center[i] = atof(cmd_argvs[3+i]) * local2base;
	}
	r = atof(cmd_argvs[6]) * local2base;

	if (r < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, radius must be greater than zero!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	if( mk_sph( wdbp, name, center, r ) < 0 )
		return 1;	/* failure */
	return 0;	/* success */
}

/*   E L L _ I N ( ) :   	reads ell parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
ell_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	fastf_t			len, mag_b, r_rev, vals[12];
	int			i, n;
	struct rt_ell_internal	*eip;

	CHECK_DBI_NULL;

	n = 12;				/* ELL has twelve params */
	if (cmd_argvs[2][3] != '\0')	/* ELLG and ELL1 have seven */
		n = 7;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_ELL;
	intern->idb_meth = &rt_functab[ID_ELL];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal),
		"rt_ell_internal" );
	eip = (struct rt_ell_internal *)intern->idb_ptr;
	eip->magic = RT_ELL_INTERNAL_MAGIC;

	/* convert typed in args to reals */
	for (i = 0; i < n; i++) {
		vals[i] = atof(cmd_argvs[3+i]) * local2base;
	}

	if (!strcmp("ell", cmd_argvs[2])) {	/* everything's ok */
		/* V, A, B, C */
		VMOVE( eip->v, &vals[0] );
		VMOVE( eip->a, &vals[3] );
		VMOVE( eip->b, &vals[6] );
		VMOVE( eip->c, &vals[9] );
		return(0);
	}

	if (!strcmp("ellg", cmd_argvs[2])) {
		/* V, f1, f2, len */
		/* convert ELLG format into ELL1 format */
		len = vals[6];
		/* V is halfway between the foci */
		VADD2( eip->v, &vals[0], &vals[3] );
		VSCALE( eip->v, eip->v, 0.5);
		VSUB2( eip->b, &vals[3], &vals[0] );
		mag_b = MAGNITUDE( eip->b );
		if ( NEAR_ZERO( mag_b, RT_LEN_TOL )) {
		  Tcl_AppendResult(interp, "ERROR, foci are coincident!\n", (char *)NULL);
		  return(1);
		}
		/* calculate A vector */
		VSCALE( eip->a, eip->b, .5*len/mag_b );
		/* calculate radius of revolution (for ELL1 format) */
		r_rev = sqrt( MAGSQ( eip->a ) - (mag_b*.5)*(mag_b*.5) );
	} else if (!strcmp("ell1", cmd_argvs[2])) {
		/* V, A, r */
		VMOVE( eip->v, &vals[0] );
		VMOVE( eip->a, &vals[3] );
		r_rev = vals[6];
	} else {
		r_rev = 0;
	}

	/* convert ELL1 format into ELLG format */
	/* calculate B vector */
	bn_vec_ortho( eip->b, eip->a );
	VUNITIZE( eip->b );
	VSCALE( eip->b, eip->b, r_rev);

	/* calculate C vector */
	VCROSS( eip->c, eip->a, eip->b );
	VUNITIZE( eip->c );
	VSCALE( eip->c, eip->c, r_rev );
	return(0);	/* success */
}

/*   T O R _ I N ( ) :   	reads tor parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
tor_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_tor_internal	*tip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_TOR;
	intern->idb_meth = &rt_functab[ID_TOR];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tor_internal),
		"rt_tor_internal" );
	tip = (struct rt_tor_internal *)intern->idb_ptr;
	tip->magic = RT_TOR_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		tip->v[i] = atof(cmd_argvs[3+i]) * local2base;
		tip->h[i] = atof(cmd_argvs[6+i]) * local2base;
	}
	tip->r_a = atof(cmd_argvs[9]) * local2base;
	tip->r_h = atof(cmd_argvs[10]) * local2base;
	/* Check for radius 2 >= radius 1 */
	if( tip->r_a <= tip->r_h )  {
	  Tcl_AppendResult(interp, "ERROR, radius 2 >= radius 1 ....\n", (char *)NULL);
	  return(1);	/* failure */
	}

	if (MAGNITUDE( tip->h ) < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, normal must be greater than zero!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	return(0);	/* success */
}

/*   T G C _ I N ( ) :   	reads tgc parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
tgc_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	fastf_t			r1, r2;
	int			i;
	struct rt_tgc_internal	*tip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_TGC;
	intern->idb_meth = &rt_functab[ID_TGC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal),
		"rt_tgc_internal" );
	tip = (struct rt_tgc_internal *)intern->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		tip->v[i] = atof(cmd_argvs[3+i]) * local2base;
		tip->h[i] = atof(cmd_argvs[6+i]) * local2base;
		tip->a[i] = atof(cmd_argvs[9+i]) * local2base;
		tip->b[i] = atof(cmd_argvs[12+i]) * local2base;
	}
	r1 = atof(cmd_argvs[15]) * local2base;
	r2 = atof(cmd_argvs[16]) * local2base;

	if (MAGNITUDE(tip->h) < RT_LEN_TOL
		|| MAGNITUDE(tip->a) < RT_LEN_TOL
		|| MAGNITUDE(tip->b) < RT_LEN_TOL
		|| r1 < RT_LEN_TOL || r2 < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, all dimensions must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	/* calculate C */
	VMOVE( tip->c, tip->a );
	VUNITIZE( tip->c );
	VSCALE( tip->c, tip->c, r1);

	/* calculate D */
	VMOVE( tip->d, tip->b );
	VUNITIZE( tip->d );
	VSCALE( tip->d, tip->d, r2);

	return(0);	/* success */
}

/*   R C C _ I N ( ) :   	reads rcc parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
rcc_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	fastf_t			r;
	int			i;
	struct rt_tgc_internal	*tip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_TGC;
	intern->idb_meth = &rt_functab[ID_TGC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal),
		"rt_tgc_internal" );
	tip = (struct rt_tgc_internal *)intern->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		tip->v[i] = atof(cmd_argvs[3+i]) * local2base;
		tip->h[i] = atof(cmd_argvs[6+i]) * local2base;
	}
	r = atof(cmd_argvs[9]) * local2base;

	if (MAGNITUDE(tip->h) < RT_LEN_TOL || r < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, all dimensions must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	bn_vec_ortho( tip->a, tip->h );
	VUNITIZE( tip->a );
	VCROSS( tip->b, tip->h, tip->a );
	VUNITIZE( tip->b );

	VSCALE( tip->a, tip->a, r );
	VSCALE( tip->b, tip->b, r );
	VMOVE( tip->c, tip->a );
	VMOVE( tip->d, tip->b );

	return(0);	/* success */
}

/*   T E C _ I N ( ) :   	reads tec parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
tec_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	fastf_t			ratio;
	int			i;
	struct rt_tgc_internal	*tip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_TGC;
	intern->idb_meth = &rt_functab[ID_TGC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal),
		"rt_tgc_internal" );
	tip = (struct rt_tgc_internal *)intern->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		tip->v[i] = atof(cmd_argvs[3+i]) * local2base;
		tip->h[i] = atof(cmd_argvs[6+i]) * local2base;
		tip->a[i] = atof(cmd_argvs[9+i]) * local2base;
		tip->b[i] = atof(cmd_argvs[12+i]) * local2base;
	}
	ratio = atof(cmd_argvs[15]);
	if (MAGNITUDE(tip->h) < RT_LEN_TOL
		|| MAGNITUDE(tip->a) < RT_LEN_TOL
		|| MAGNITUDE(tip->b) < RT_LEN_TOL
		|| ratio < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, all dimensions must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	VSCALE( tip->c, tip->a, 1./ratio );	/* C vector */
	VSCALE( tip->d, tip->b, 1./ratio );	/* D vector */

	return(0);	/* success */
}

/*   R E C _ I N ( ) :   	reads rec parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
rec_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_tgc_internal	*tip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_TGC;
	intern->idb_meth = &rt_functab[ID_TGC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal),
		"rt_tgc_internal" );
	tip = (struct rt_tgc_internal *)intern->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		tip->v[i] = atof(cmd_argvs[3+i]) * local2base;
		tip->h[i] = atof(cmd_argvs[6+i]) * local2base;
		tip->a[i] = atof(cmd_argvs[9+i]) * local2base;
		tip->b[i] = atof(cmd_argvs[12+i]) * local2base;
	}

	if (MAGNITUDE(tip->h) < RT_LEN_TOL
		|| MAGNITUDE(tip->a) < RT_LEN_TOL
		|| MAGNITUDE(tip->b) < RT_LEN_TOL ) {
	  Tcl_AppendResult(interp, "ERROR, all dimensions must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	VMOVE( tip->c, tip->a );		/* C vector */
	VMOVE( tip->d, tip->b );		/* D vector */

	return(0);	/* success */
}

/*   T R C _ I N ( ) :   	reads trc parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
trc_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	fastf_t			r1, r2;
	int			i;
	struct rt_tgc_internal	*tip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_TGC;
	intern->idb_meth = &rt_functab[ID_TGC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_tgc_internal),
		"rt_tgc_internal" );
	tip = (struct rt_tgc_internal *)intern->idb_ptr;
	tip->magic = RT_TGC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		tip->v[i] = atof(cmd_argvs[3+i]) * local2base;
		tip->h[i] = atof(cmd_argvs[6+i]) * local2base;
	}
	r1 = atof(cmd_argvs[9]) * local2base;
	r2 = atof(cmd_argvs[10]) * local2base;

	if (MAGNITUDE(tip->h) < RT_LEN_TOL
		|| r1 < RT_LEN_TOL || r2 < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, all dimensions must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	bn_vec_ortho( tip->a, tip->h );
	VUNITIZE( tip->a );
	VCROSS( tip->b, tip->h, tip->a );
	VUNITIZE( tip->b );
	VMOVE( tip->c, tip->a );
	VMOVE( tip->d, tip->b );

	VSCALE( tip->a, tip->a, r1 );
	VSCALE( tip->b, tip->b, r1 );
	VSCALE( tip->c, tip->c, r2 );
	VSCALE( tip->d, tip->d, r2 );

	return(0);	/* success */
}

/*   B O X _ I N ( ) :   	reads box parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
box_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_arb_internal	*aip;
	vect_t			Dpth, Hgt, Vrtx, Wdth;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_ARB8;
	intern->idb_meth = &rt_functab[ID_ARB8];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal),
		"rt_arb_internal" );
	aip = (struct rt_arb_internal *)intern->idb_ptr;
	aip->magic = RT_ARB_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		Vrtx[i] = atof(cmd_argvs[3+i]) * local2base;
		Hgt[i] = atof(cmd_argvs[6+i]) * local2base;
		Wdth[i] = atof(cmd_argvs[9+i]) * local2base;
		Dpth[i] = atof(cmd_argvs[12+i]) * local2base;
	}

	if (MAGNITUDE(Dpth) < RT_LEN_TOL || MAGNITUDE(Hgt) < RT_LEN_TOL
		|| MAGNITUDE(Wdth) < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, all dimensions must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	if (!strcmp("box", cmd_argvs[2])) {
		VMOVE( aip->pt[0], Vrtx );
		VADD2( aip->pt[1], Vrtx, Wdth );
		VADD3( aip->pt[2], Vrtx, Wdth, Hgt );
		VADD2( aip->pt[3], Vrtx, Hgt );
		VADD2( aip->pt[4], Vrtx, Dpth );
		VADD3( aip->pt[5], Vrtx, Dpth, Wdth );
		VADD4( aip->pt[6], Vrtx, Dpth, Wdth, Hgt );
		VADD3( aip->pt[7], Vrtx, Dpth, Hgt );
	} else { /* "raw" */
		VADD2( aip->pt[0], Vrtx, Wdth );
		VADD2( aip->pt[1], Vrtx, Hgt );
		VADD2( aip->pt[2], aip->pt[1], Dpth );
		VADD2( aip->pt[3], aip->pt[0], Dpth );
		VMOVE( aip->pt[4], Vrtx );
		VMOVE( aip->pt[5], Vrtx );
		VADD2( aip->pt[6], Vrtx, Dpth );
		VMOVE( aip->pt[7], aip->pt[6] );
	}

	return(0);	/* success */
}

/*   R P P _ I N ( ) :   	reads rpp parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */ 
int
rpp_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
	point_t		min, max;
	char *p;
	int i;
	char errbuf[128];

	CHECK_DBI_NULL;

	intern->idb_ptr = NULL;

	min[X] = atof(cmd_argvs[3+0]) * local2base;
	max[X] = atof(cmd_argvs[3+1]) * local2base;
	min[Y] = atof(cmd_argvs[3+2]) * local2base;
	max[Y] = atof(cmd_argvs[3+3]) * local2base;
	min[Z] = atof(cmd_argvs[3+4]) * local2base;
	max[Z] = atof(cmd_argvs[3+5]) * local2base;

	if (min[X] >= max[X]) {
	    sprintf(errbuf, "ERROR, XMIN:(%lg) greater than XMAX:(%lg) !\n", min[X], max[X]);
	    Tcl_AppendResult(interp, errbuf, (char *)NULL);
	  return(1);	/* failure */
	}
	if (min[Y] >= max[Y]) {
	    sprintf(errbuf, "ERROR, YMIN:(%lg) greater than YMAX:(%lg) !\n", min[Y], max[Y]);
	  Tcl_AppendResult(interp, errbuf, (char *)NULL);
	  return(1);	/* failure */
	}
	if (min[Z] >= max[Z]) {
	    sprintf(errbuf, "ERROR, ZMIN:(%lg) greater than ZMAX:(%lg)!\n", min[Z], max[Z]);
	    Tcl_AppendResult(interp, errbuf, (char *)NULL);
	    return(1);	/* failure */
	}

	if( mk_rpp( wdbp, name, min, max ) < 0 )
		return 1;
	return 0;	/* success */
}

/*
 *			O R P P _ I N ( )
 *
 * Reads origin-min rpp (box) parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
orpp_in(char **cmd_argvs, struct rt_db_internal *intern, const char *name)
{
	point_t		min, max;

	CHECK_DBI_NULL;

	intern->idb_ptr = NULL;

	VSETALL( min, 0 );
	max[X] = atof(cmd_argvs[3+0]) * local2base;
	max[Y] = atof(cmd_argvs[3+1]) * local2base;
	max[Z] = atof(cmd_argvs[3+2]) * local2base;

	if (min[X] >= max[X]) {
	  Tcl_AppendResult(interp, "ERROR, XMIN greater than XMAX!\n", (char *)NULL);
	  return(1);	/* failure */
	}
	if (min[Y] >= max[Y]) {
	  Tcl_AppendResult(interp, "ERROR, YMIN greater than YMAX!\n", (char *)NULL);
	  return(1);	/* failure */
	}
	if (min[Z] >= max[Z]) {
	  Tcl_AppendResult(interp, "ERROR, ZMIN greater than ZMAX!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	if( mk_rpp( wdbp, name, min, max ) < 0 )
		return 1;
	return 0;	/* success */
}

/*   P A R T _ I N ( ) :	reads particle parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
part_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_part_internal *part_ip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_PARTICLE;
	intern->idb_meth = &rt_functab[ID_PARTICLE];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_part_internal),
		"rt_part_internal" );
	part_ip = (struct rt_part_internal *)intern->idb_ptr;
	part_ip->part_magic = RT_PART_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		part_ip->part_V[i] = atof(cmd_argvs[3+i]) * local2base;
		part_ip->part_H[i] = atof(cmd_argvs[6+i]) * local2base;
	}
	part_ip->part_vrad = atof(cmd_argvs[9]) * local2base;
	part_ip->part_hrad = atof(cmd_argvs[10]) * local2base;

	if (MAGNITUDE(part_ip->part_H) < RT_LEN_TOL
		|| part_ip->part_vrad <= RT_LEN_TOL
		|| part_ip->part_hrad <= RT_LEN_TOL) {
	  Tcl_AppendResult(interp,
			   "ERROR, height, v radius and h radius must be greater than zero!\n",
			   (char *)NULL);
	  return(1);    /* failure */
	}

	return(0);      /* success */
}

/*   R P C _ I N ( ) :   	reads rpc parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
rpc_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_rpc_internal	*rip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_RPC;
	intern->idb_meth = &rt_functab[ID_RPC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_rpc_internal),
		"rt_rpc_internal" );
	rip = (struct rt_rpc_internal *)intern->idb_ptr;
	rip->rpc_magic = RT_RPC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		rip->rpc_V[i] = atof(cmd_argvs[3+i]) * local2base;
		rip->rpc_H[i] = atof(cmd_argvs[6+i]) * local2base;
		rip->rpc_B[i] = atof(cmd_argvs[9+i]) * local2base;
	}
	rip->rpc_r = atof(cmd_argvs[12]) * local2base;

	if (MAGNITUDE(rip->rpc_H) < RT_LEN_TOL
		|| MAGNITUDE(rip->rpc_B) < RT_LEN_TOL
		|| rip->rpc_r <= RT_LEN_TOL) {
	  Tcl_AppendResult(interp,
			   "ERROR, height, breadth, and width must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	return(0);	/* success */
}

/*   R H C _ I N ( ) :   	reads rhc parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
rhc_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_rhc_internal	*rip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_RHC;
	intern->idb_meth = &rt_functab[ID_RHC];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_rhc_internal),
		"rt_rhc_internal" );
	rip = (struct rt_rhc_internal *)intern->idb_ptr;
	rip->rhc_magic = RT_RHC_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		rip->rhc_V[i] = atof(cmd_argvs[3+i]) * local2base;
		rip->rhc_H[i] = atof(cmd_argvs[6+i]) * local2base;
		rip->rhc_B[i] = atof(cmd_argvs[9+i]) * local2base;
	}
	rip->rhc_r = atof(cmd_argvs[12]) * local2base;
	rip->rhc_c = atof(cmd_argvs[13]) * local2base;

	if (MAGNITUDE(rip->rhc_H) < RT_LEN_TOL
		|| MAGNITUDE(rip->rhc_B) < RT_LEN_TOL
		|| rip->rhc_r <= RT_LEN_TOL || rip->rhc_c <= RT_LEN_TOL) {
	  Tcl_AppendResult(interp,
			   "ERROR, height, breadth, and width must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	return(0);	/* success */
}

/*   E P A _ I N ( ) :   	reads epa parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
epa_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_epa_internal	*rip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_EPA;
	intern->idb_meth = &rt_functab[ID_EPA];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_epa_internal),
		"rt_epa_internal" );
	rip = (struct rt_epa_internal *)intern->idb_ptr;
	rip->epa_magic = RT_EPA_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		rip->epa_V[i] = atof(cmd_argvs[3+i]) * local2base;
		rip->epa_H[i] = atof(cmd_argvs[6+i]) * local2base;
		rip->epa_Au[i] = atof(cmd_argvs[9+i]) * local2base;
	}
	rip->epa_r1 = MAGNITUDE(rip->epa_Au);
	rip->epa_r2 = atof(cmd_argvs[12]) * local2base;
	VUNITIZE(rip->epa_Au);

	if (MAGNITUDE(rip->epa_H) < RT_LEN_TOL
		|| rip->epa_r1 <= RT_LEN_TOL || rip->epa_r2 <= RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, height and axes must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	if (rip->epa_r2 > rip->epa_r1) {
	  Tcl_AppendResult(interp, "ERROR, |A| must be greater than |B|!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	return(0);	/* success */
}

/*   E H Y _ I N ( ) :   	reads ehy parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
ehy_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_ehy_internal	*rip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_EHY;
	intern->idb_meth = &rt_functab[ID_EHY];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ehy_internal),
		"rt_ehy_internal" );
	rip = (struct rt_ehy_internal *)intern->idb_ptr;
	rip->ehy_magic = RT_EHY_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		rip->ehy_V[i] = atof(cmd_argvs[3+i]) * local2base;
		rip->ehy_H[i] = atof(cmd_argvs[6+i]) * local2base;
		rip->ehy_Au[i] = atof(cmd_argvs[9+i]) * local2base;
	}
	rip->ehy_r1 = MAGNITUDE(rip->ehy_Au);
	rip->ehy_r2 = atof(cmd_argvs[12]) * local2base;
	rip->ehy_c = atof(cmd_argvs[13]) * local2base;
	VUNITIZE(rip->ehy_Au);

	if (MAGNITUDE(rip->ehy_H) < RT_LEN_TOL
		|| rip->ehy_r1 <= RT_LEN_TOL || rip->ehy_r2 <= RT_LEN_TOL
		|| rip->ehy_c <= RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, height, axes, and distance to asymptotes must be greater than zero!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	if (rip->ehy_r2 > rip->ehy_r1) {
	  Tcl_AppendResult(interp, "ERROR, |A| must be greater than |B|!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	return(0);	/* success */
}

/*   E T O _ I N ( ) :   	reads eto parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
eto_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_eto_internal	*eip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_ETO;
	intern->idb_meth = &rt_functab[ID_ETO];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_eto_internal),
		"rt_eto_internal" );
	eip = (struct rt_eto_internal *)intern->idb_ptr;
	eip->eto_magic = RT_ETO_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		eip->eto_V[i] = atof(cmd_argvs[3+i]) * local2base;
		eip->eto_N[i] = atof(cmd_argvs[6+i]) * local2base;
		eip->eto_C[i] = atof(cmd_argvs[10+i]) * local2base;
	}
	eip->eto_r = atof(cmd_argvs[9]) * local2base;
	eip->eto_rd = atof(cmd_argvs[13]) * local2base;

	if (MAGNITUDE(eip->eto_N) < RT_LEN_TOL
		|| MAGNITUDE(eip->eto_C) < RT_LEN_TOL
		|| eip->eto_r <= RT_LEN_TOL || eip->eto_rd <= RT_LEN_TOL) {
	  Tcl_AppendResult(interp,
			   "ERROR, normal, axes, and radii must be greater than zero!\n",
			   (char *)NULL);
	  return(1);	/* failure */
	}

	if (eip->eto_rd > MAGNITUDE(eip->eto_C)) {
	  Tcl_AppendResult(interp, "ERROR, |C| must be greater than |D|!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	return(0);	/* success */
}

/*   E X T R U D E _ I N ( ) :   	reads extrude parameters from keyboard
 *					returns 0 if successful read
 *					1 if unsuccessful read
 */
int
extrude_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_extrude_internal	*eip;
	struct rt_db_internal		tmp_ip;
	struct directory		*dp;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_EXTRUDE;
	intern->idb_meth = &rt_functab[ID_EXTRUDE];
	intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_extrude_internal),
					      "rt_extrude_internal");
	eip = (struct rt_extrude_internal *)intern->idb_ptr;
	eip->magic = RT_EXTRUDE_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		eip->V[i] = atof(cmd_argvs[3+i]) * local2base;
		eip->h[i] = atof(cmd_argvs[6+i]) * local2base;
		eip->u_vec[i] = atof(cmd_argvs[9+i]) * local2base;
		eip->v_vec[i] = atof(cmd_argvs[12+i]) * local2base;
	}
	eip->sketch_name = bu_strdup(cmd_argvs[15]);
	eip->keypoint = atoi(cmd_argvs[16]);

	if ((dp=db_lookup(dbip, eip->sketch_name, LOOKUP_NOISY)) == DIR_NULL) {
		Tcl_AppendResult(interp, "Cannot find sketch (", eip->sketch_name,
				 ") for extrusion (", cmd_argvs[1], ")\n", (char *)NULL);
		eip->skt = (struct rt_sketch_internal *)NULL;
		return 1;
	}

	if (rt_db_get_internal(&tmp_ip, dp, dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
		Tcl_AppendResult(interp, "Cannot import sketch (", eip->sketch_name,
				 ") for extrusion (", cmd_argvs[1], ")\n", (char *)NULL);
		eip->skt = (struct rt_sketch_internal *)NULL;
		return 1;
	} else
		eip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;

	return 0;	/* success */
}

/*   G R I P _ I N ( ) :   	reads grip parameters from keyboard
 *				returns 0 if successful read
 *				1 if unsuccessful read
 */
int
grip_in(char **cmd_argvs, struct rt_db_internal *intern)
{
	int			i;
	struct rt_grip_internal	*gip;

	CHECK_DBI_NULL;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_GRIP;
	intern->idb_meth = &rt_functab[ID_GRIP];
	intern->idb_ptr = (genptr_t)bu_malloc(sizeof(struct rt_grip_internal),
					      "rt_grip_internal");
	gip = (struct rt_grip_internal *)intern->idb_ptr;
	gip->magic = RT_GRIP_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		gip->center[i] = atof(cmd_argvs[3+i]) * local2base;
		gip->normal[i] = atof(cmd_argvs[6+i]) * local2base;
	}

	gip->mag = atof(cmd_argvs[9]);

	return 0;
}

/*   S U P E R E L L _ I N ( ) :   	reads superell parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
superell_in(char *cmd_argvs[], struct rt_db_internal *intern)
{
	fastf_t			vals[14];
	int			i, n;
	struct rt_superell_internal	*eip;

	CHECK_DBI_NULL;

	n = 14;				/* SUPERELL has 12 (same as ELL) + 2 (for <n,e>) params */

	intern->idb_type = ID_SUPERELL;
	intern->idb_meth = &rt_functab[ID_SUPERELL];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_superell_internal),
		"rt_superell_internal" );
	eip = (struct rt_superell_internal *)intern->idb_ptr;
	eip->magic = RT_SUPERELL_INTERNAL_MAGIC;

	/* convert typed in args to reals and convert to local units */
	for (i = 0; i < n - 2; i++) {
		vals[i] = atof(cmd_argvs[3+i]) * local2base;
	}
	vals[12] = atof(cmd_argvs[3 + 12]);
	vals[13] = atof(cmd_argvs[3 + 13]);

	/* V, A, B, C */
	VMOVE( eip->v, &vals[0] );
	VMOVE( eip->a, &vals[3] );
	VMOVE( eip->b, &vals[6] );
	VMOVE( eip->c, &vals[9] );
	eip->n = vals[12];
	eip->e = vals[13];
	return(0);
}

/*
 *			M E T A B A L L _ I N
 */
int
metaball_in(int argc, char **argv, struct rt_db_internal *intern, char **prompt)
{
	struct rt_metaball_internal *metaball;
	int i,num_points;
	fastf_t threshhold = -1.0;

	CHECK_DBI_NULL;

	if( argc < 4 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
	  return CMD_MORE;
	}

	threshhold = atof( argv[3] );
	if(threshhold < 0.0) {
		Tcl_AppendResult(interp, "Threshhold may not be negative.\n",(char *)NULL);
		return CMD_BAD;
	}

	if( argc < 5 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
	  return CMD_MORE;
	}

	num_points = atoi( argv[4] );
	if( num_points < 1 )
	{
		Tcl_AppendResult(interp, "Invalid number of points (must be at least 1)\n", (char *)NULL);
		return CMD_BAD;
	}

	if( argc < 9 )
	{
		Tcl_AppendResult(interp, MORE_ARGS_STR, prompt[argc-3], (char *)NULL);
		return CMD_MORE;
	}

	if( argc < 5 + num_points*4 )
	{
		struct bu_vls tmp_vls;

		bu_vls_init( &tmp_vls );
		bu_vls_printf( &tmp_vls, "%s for point %d : ", prompt[6+(argc-9)%4], 1+(argc-5)/4 );
		Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		return CMD_MORE;
	}


	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_METABALL;
	intern->idb_meth = &rt_functab[ID_METABALL];
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof( struct rt_metaball_internal ), "rt_metaball_internal" );
	metaball = (struct rt_metaball_internal *)intern->idb_ptr;
	metaball->magic = RT_METABALL_INTERNAL_MAGIC;
	BU_LIST_INIT( &metaball->metaball_pt_head );

	for( i=5 ; i<argc ; i+= 4 )
	{
		struct wdb_metaballpt *metaballpt;

		metaballpt = (struct wdb_metaballpt *)bu_malloc( sizeof( struct wdb_metaballpt ), "wdb_metaballpt" );
		metaballpt->coord[0] = atof( argv[i] ) * local2base;
		metaballpt->coord[1] = atof( argv[i+1] ) * local2base;
		metaballpt->coord[2] = atof( argv[i+2] ) * local2base;
		metaballpt->fldstr = atof( argv[i+3] ) * local2base;

		BU_LIST_INSERT( &metaball->metaball_pt_head, &metaballpt->l );
	}

	return CMD_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
