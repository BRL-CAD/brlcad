/*
 *  			T Y P E I N
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
 *	checkv		checks for zero vector from keyboard
 *
 * Authors -
 *	Charles M. Kennedy
 *	Keith A. Applin
 *	Michael J. Muuss
 *	Michael J. Markowski
 *
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "./mged_dm.h"

void	aexists();

int		vals;		/* number of args for s_values[] */
char		**promp;	/* the prompt string */

char *p_half[] = {
	"Enter X, Y, Z of outward pointing normal vector: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter the distance from the origin: "
};

char *p_dsp[] = {
	"Enter name of displacement-map file: ",
	"Enter width of displacement-map (number of values): ",
	"Enter length of displacement-map (number of values): ",
	"Normal Interpolation? 0=no 1=yes default=1: "
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

char *p_ars[] = {
	"Enter number of points per waterline, and number of waterlines: ",
	"Enter number of waterlines: ",
	"Enter X, Y, Z for First row point: ",
	"Enter Y: ",
	"Enter Z: ",
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

char *p_ell[] = {
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

char *p_ellg[] = {
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
	"Enter X, Y, Z, of vector C: ",
	"Enter Y: ",
	"Enter Z: ",
	"Enter radius of revolution, r: ",
	"Enter elliptical semi-minor axis, d: "
};

/*	F _ I N ( ) :  	decides which solid reader to call
 *			Used for manual entry of solids.
 */
int
f_in(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	register struct directory *dp;
	char			name[NAMESIZE+2];
	struct rt_db_internal	internal;
	struct bu_external	external;
	char			*new_cmd[3], **menu;
	int			ngran;		/* number of db granules */
	int			id;
	int			c;
	int			do_solid_edit = 0;
	int			dont_draw = 0;
	int			nvals, (*fn_in)();
	int			arb_in(), box_in(), ehy_in(), ell_in(),
				epa_in(), eto_in(), half_in(), rec_in(),
				rcc_in(), rhc_in(), rpc_in(), rpp_in(),
				sph_in(), tec_in(), tgc_in(), tor_in(),
				trc_in(), ebm_in(), vol_in(), hf_in(),
				dsp_in();

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(argc < 1 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help in");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Parse options. */
	optind = 1;		/* re-init getopt() */
	while( (c=getopt(argc,argv,"sf")) != EOF )  {
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
		    bu_vls_printf(&tmp_vls, "in: option '%c' unknown\n", c);
		    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		    bu_vls_free(&tmp_vls);
		  }

		  break;
		}
	}
	argc -= optind-1;
	argv += optind-1;

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
	if( (int)strlen(argv[1]) >= NAMESIZE )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "ERROR, names are limited to %d characters\n", NAMESIZE-1);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  return TCL_ERROR;
	}
	/* Save the solid name since argv[] might get bashed */
	strcpy( name, argv[1] );

	/* Get the solid type to be created and make it */
	if( argc < 3 )  {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter solid type: ", (char *)NULL);
	  return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL( &internal );

	/*
	 * Decide which solid to make and get the rest of the args
	 * make name <half|arb[4-8]|sph|ell|ellg|ell1|tor|tgc|tec|
			rec|trc|rcc|box|raw|rpp|rpc|rhc|epa|ehy|eto>
	 */
	if( strcmp( argv[2], "ebm" ) == 0 )  {
		nvals = 4;
		menu = p_ebm;
		fn_in = ebm_in;
	} else if( strcmp( argv[2], "vol" ) == 0 )  {
		nvals = 9;
		menu = p_vol;
		fn_in = vol_in;
	} else if( strcmp( argv[2], "hf" ) == 0 )  {
		nvals = 19;
		menu = p_hf;
		fn_in = hf_in;
	} else if( strcmp( argv[2], "dsp" ) == 0 )  {
		nvals = 4;
		menu = p_dsp;
		fn_in = dsp_in;
	} else if( strcmp( argv[2], "ars" ) == 0 )  {
		switch( ars_in(argc, argv, &internal, &p_ars[0]) ) {
		case CMD_BAD:
		  Tcl_AppendResult(interp, "ERROR, ars not made!\n", (char *)NULL);
		  if(internal.idb_type) rt_functab[internal.idb_type].
					  ft_ifree( &internal );
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
		nvals = 3*atoi(&argv[2][3]);
		menu = p_arb;
		fn_in = arb_in;
	} else if( strcmp( argv[2], "sph" ) == 0 )  {
		nvals = 3*1 + 1;
		menu = p_sph;
		fn_in = sph_in;
	} else if( strcmp( argv[2], "ell" ) == 0 )  {
		nvals = 3*2 + 1;
		menu = p_ell;
		fn_in = ell_in;
	} else if( strcmp( argv[2], "ellg" ) == 0 )  {
		nvals = 3*4;
		menu = p_ellg;
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
		nvals = 6*1;
		menu = p_rpp;
		fn_in = rpp_in;
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

	if (fn_in(argv, &internal, menu) != 0)  {
	  Tcl_AppendResult(interp, "ERROR, ", argv[2], " not made!\n", (char *)NULL);
	  if(internal.idb_type) rt_functab[internal.idb_type].
				  ft_ifree( &internal );
	  return TCL_ERROR;
	}

do_new_update:
	if( (dp=db_diradd( dbip, name, -1L, 0, DIR_SOLID)) == DIR_NULL )
	{
		rt_db_free_internal( &internal );
		Tcl_AppendResult(interp, "Cannot add '", name, "' to directory\n", (char *)NULL );
		return TCL_ERROR;
	}
	if( rt_db_put_internal( dp, dbip, &internal ) < 0 )
	{
		rt_db_free_internal( &internal );
		TCL_WRITE_ERR_return;
	}

	if( dont_draw )  return TCL_OK;

	/* draw the newly "made" solid */
	new_cmd[0] = "e";
	new_cmd[1] = name;
	new_cmd[2] = (char *)NULL;
	(void)f_edit( clientData, interp, 2, new_cmd );

	if( do_solid_edit )  {
		/* Also kick off solid edit mode */
		new_cmd[0] = "sed";
		new_cmd[1] = name;
		new_cmd[2] = (char *)NULL;
		(void)f_sed( clientData, interp, 2, new_cmd );
	}
	return TCL_OK;
}

/*			E B M _ I N
 *
 *	Read EBM solid from keyboard
 *
 */
int
ebm_in( cmd_argvs, intern )
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	struct rt_ebm_internal	*ebm;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	BU_GETSTRUCT( ebm, rt_ebm_internal );
	intern->idb_type = ID_EBM;
	intern->idb_ptr = (genptr_t)ebm;
	ebm->magic = RT_EBM_INTERNAL_MAGIC;

	strcpy( ebm->file, cmd_argvs[3] );
	ebm->xdim = atoi( cmd_argvs[4] );
	ebm->ydim = atoi( cmd_argvs[5] );
	ebm->tallness = atof( cmd_argvs[6] ) * local2base;
	bn_mat_idn( ebm->mat );

	return( 0 );
}

/*			D S P _ I N
 *
 *	Read DSP solid from keyboard
 */
int
dsp_in ( cmd_argvs, intern )
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	struct rt_dsp_internal	*dsp;

	BU_GETSTRUCT( dsp, rt_dsp_internal );
	intern->idb_type = ID_DSP;
	intern->idb_ptr = (genptr_t)dsp;
	dsp->magic = RT_DSP_INTERNAL_MAGIC;

	strcpy( dsp->dsp_file, cmd_argvs[3] );
	dsp->dsp_xcnt = atoi( cmd_argvs[4] );
	dsp->dsp_ycnt = atoi( cmd_argvs[5] );
	dsp->dsp_smooth = atoi( cmd_argvs[6] );
	bn_mat_idn( dsp->dsp_mtos );
	bn_mat_idn( dsp->dsp_stom );

	return( 0 );
}



/*			H F _ I N
 *
 *	Read HF solid from keyboard
 *
 */
int
hf_in( cmd_argvs, intern )
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	struct rt_hf_internal	*hf;
	vect_t work;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	BU_GETSTRUCT( hf, rt_hf_internal );
	intern->idb_type = ID_HF;
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
vol_in( cmd_argvs, intern )
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	struct rt_vol_internal	*vol;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	BU_GETSTRUCT( vol, rt_vol_internal );
	intern->idb_type = ID_VOL;
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
	bn_mat_idn( vol->mat );

	return( 0 );
}

/*
 *			A R S _ I N
 */
int
ars_in( argc, argv, intern, promp )
int			argc;
char			**argv;
struct rt_db_internal	*intern;
char			*promp[];
{
	register struct rt_ars_internal	*arip;
	register int			i;
	int			total_points;
	int			cv;	/* current curve (waterline) # */
	int			axis;	/* current fastf_t in waterline */
	int			ncurves_minus_one;
	int num_pts, num_curves;


	if(dbip == DBI_NULL)
	  return TCL_OK;

	if( argc < 5 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, promp[argc-3], (char *)NULL);
	  return CMD_MORE;
	}

	num_pts = atoi(argv[3]);
	num_curves = atoi(argv[4]);

	if (num_pts < 3 || num_curves < 3 ) {
	  Tcl_AppendResult(interp, "Invalid number of lines or pts_per_curve\n", (char *)NULL);
	  return CMD_BAD;
	}

	if( argc < 8 ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, promp[argc-3], (char *)NULL);
	  return CMD_MORE;
	}

#if 0
	if( argc < 8+((num_curves-2)*num_pts*3) ) {
		bu_log("%s for Waterline %d, Point %d : ",
			promp[5+(argc-8)%3], 1+(argc-8)/3/num_pts, ((argc-8)/3)%
			num_pts );
		return CMD_MORE;
	}

	if( argc < 8+((num_curves-2)*num_pts*3+3)) {
		bu_log("%s for point of last waterline : ",
			promp[5+(argc-8)%3]);
		return CMD_MORE;
	}
#else
	if( argc < 5+3*(num_curves-1)*num_pts ) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "%s for Waterline %d, Point %d : ",
			promp[5+(argc-8)%3], 1+(argc-8)/3/num_pts, ((argc-8)/3)%num_pts );

	  Tcl_AppendResult(interp, MORE_ARGS_STR, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);

	  return CMD_MORE;
	}

	if( argc < 5+3*num_curves*num_pts ) {
	  Tcl_AppendResult(interp, MORE_ARGS_STR, promp[5+(argc-8)%3],
			   " for point of last waterline : ", (char *)NULL);
	  return CMD_MORE;
	}
#endif

	intern->idb_type = ID_ARS;
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
half_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_half_internal	*hip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_HALF;
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_half_internal),
		"rt_half_internal" );
	hip = (struct rt_half_internal *)intern->idb_ptr;
	hip->magic = RT_HALF_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PLANE; i++) {
		hip->eqn[i] = atof(cmd_argvs[3+i]) * local2base;
	}
	VUNITIZE( hip->eqn );
	
	if (MAGNITUDE(hip->eqn) < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, normal vector is too small!\n", (char *)NULL);
	  return(1);	/* failure */
	}
	
	return(0);	/* success */
}

/*   A R B _ I N ( ) :   	reads arb parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
arb_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i, j, n;
	struct rt_arb_internal	*aip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_ARB8;
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
sph_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			r;
	int			i;
	struct rt_ell_internal	*sip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_ELL;
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal),
		"rt_ell_internal" );
	sip = (struct rt_ell_internal *)intern->idb_ptr;
	sip->magic = RT_ELL_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		sip->v[i] = atof(cmd_argvs[3+i]) * local2base;
	}
	r = atof(cmd_argvs[6]) * local2base;
	VSET( sip->a, r, 0., 0. );
	VSET( sip->b, 0., r, 0. );
	VSET( sip->c, 0., 0., r );
	
	if (r < RT_LEN_TOL) {
	  Tcl_AppendResult(interp, "ERROR, radius must be greater than zero!\n", (char *)NULL);
	  return(1);	/* failure */
	}
	
	return(0);	/* success */
}

/*   E L L _ I N ( ) :   	reads ell parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
ell_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			len, mag_b, r_rev, vals[12];
	int			i, n;
	struct rt_ell_internal	*eip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	n = 7;				/* ELL and ELL1 have seven params */
	if (cmd_argvs[2][3] == 'g')	/* ELLG has twelve */
		n = 12;

	intern->idb_type = ID_ELL;
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_ell_internal),
		"rt_ell_internal" );
	eip = (struct rt_ell_internal *)intern->idb_ptr;
	eip->magic = RT_ELL_INTERNAL_MAGIC;

	/* convert typed in args to reals */
	for (i = 0; i < n; i++) {
		vals[i] = atof(cmd_argvs[3+i]) * local2base;
	}

	if (!strcmp("ellg", cmd_argvs[2])) {	/* everything's ok */
		/* V, A, B, C */
		VMOVE( eip->v, &vals[0] );
		VMOVE( eip->a, &vals[3] );
		VMOVE( eip->b, &vals[6] );
		VMOVE( eip->c, &vals[9] );
		return(0);
	}
	
	if (!strcmp("ell", cmd_argvs[2])) {
		/* V, f1, f2, len */
		/* convert ELL format into ELL1 format */
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
tor_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_tor_internal	*tip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_TOR;
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
tgc_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			r1, r2;
	int			i;
	struct rt_tgc_internal	*tip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_TGC;
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
rcc_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			r;
	int			i;
	struct rt_tgc_internal	*tip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_TGC;
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
tec_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			ratio;
	int			i;
	struct rt_tgc_internal	*tip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_TGC;
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
rec_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_tgc_internal	*tip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_TGC;
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
trc_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			r1, r2;
	int			i;
	struct rt_tgc_internal	*tip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_TGC;
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
box_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_arb_internal	*aip;
	vect_t			Dpth, Hgt, Vrtx, Wdth;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_ARB8;
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
rpp_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	fastf_t			xmin, xmax, ymin, ymax, zmin, zmax;
	struct rt_arb_internal	*aip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_ARB8;
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_arb_internal),
		"rt_arb_internal" );
	aip = (struct rt_arb_internal *)intern->idb_ptr;
	aip->magic = RT_ARB_INTERNAL_MAGIC;

	xmin = atof(cmd_argvs[3+0]) * local2base;
	xmax = atof(cmd_argvs[3+1]) * local2base;
	ymin = atof(cmd_argvs[3+2]) * local2base;
	ymax = atof(cmd_argvs[3+3]) * local2base;
	zmin = atof(cmd_argvs[3+4]) * local2base;
	zmax = atof(cmd_argvs[3+5]) * local2base;

	if (xmin >= xmax) {
	  Tcl_AppendResult(interp, "ERROR, XMIN greater than XMAX!\n", (char *)NULL);
	  return(1);	/* failure */
	}
	if (ymin >= ymax) {
	  Tcl_AppendResult(interp, "ERROR, YMIN greater than YMAX!\n", (char *)NULL);
	  return(1);	/* failure */
	}
	if (zmin >= zmax) {
	  Tcl_AppendResult(interp, "ERROR, ZMIN greater than ZMAX!\n", (char *)NULL);
	  return(1);	/* failure */
	}

	VSET( aip->pt[0], xmax, ymin, zmin );
	VSET( aip->pt[1], xmax, ymax, zmin );
	VSET( aip->pt[2], xmax, ymax, zmax );
	VSET( aip->pt[3], xmax, ymin, zmax );
	VSET( aip->pt[4], xmin, ymin, zmin );
	VSET( aip->pt[5], xmin, ymax, zmin );
	VSET( aip->pt[6], xmin, ymax, zmax );
	VSET( aip->pt[7], xmin, ymin, zmax );

	return(0);	/* success */
}

/*   R P C _ I N ( ) :   	reads rpc parameters from keyboard
 *				returns 0 if successful read
 *					1 if unsuccessful read
 */
int
rpc_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_rpc_internal	*rip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_RPC;
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
rhc_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_rhc_internal	*rip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_RHC;
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
epa_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_epa_internal	*rip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_EPA;
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
ehy_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_ehy_internal	*rip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_EHY;
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
eto_in(cmd_argvs, intern)
char			*cmd_argvs[];
struct rt_db_internal	*intern;
{
	int			i;
	struct rt_eto_internal	*eip;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	intern->idb_type = ID_ETO;
	intern->idb_ptr = (genptr_t)bu_malloc( sizeof(struct rt_eto_internal),
		"rt_eto_internal" );
	eip = (struct rt_eto_internal *)intern->idb_ptr;
	eip->eto_magic = RT_ETO_INTERNAL_MAGIC;

	for (i = 0; i < ELEMENTS_PER_PT; i++) {
		eip->eto_V[i] = atof(cmd_argvs[3+i]) * local2base;
		eip->eto_N[i] = atof(cmd_argvs[6+i]) * local2base;
		eip->eto_C[i] = atof(cmd_argvs[9+i]) * local2base;
	}
	eip->eto_r = atof(cmd_argvs[12]) * local2base;
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

