/*
 *			R T
 *
 *  Demonstration Ray Tracing main program, using RT library.
 *  Invoked by MGED for quick pictures.
 *  Is linked with each of three "back ends" (view.c, viewpp.c, viewray.c)
 *  to produce three executable programs:  rt, rtpp, rtray.
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
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"

extern char usage[];

extern double atof();

/***** Variables shared with viewing model *** */
double AmbientIntensity = 0.3;	/* Ambient light intensity */
double azimuth, elevation;
int lightmodel;		/* Select lighting model */
mat_t view2model;
mat_t model2view;
/***** end of sharing with viewing model *****/

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct application ap;
	static vect_t temp;
	static int matflag = 0;		/* read matrix from stdin */
	static double utime;
	char *title_file, *title_obj;	/* name of file and first object */
	int	perspective=0;	/* perspective view -vs- parallel view */
	float	zoomout=1;	/* >0 zoom out factor, 0..1 zoom in factor */
	vect_t	dx_model;	/* view delta-X as model-space vector */
	vect_t	dy_model;	/* view delta-Y as model-space vector */
	point_t	eye_model;	/* model-space location of eye */
	point_t	viewbase_model;	/* model-space location of viewplane corner */
	static int outfd;	/* fd of optional pixel output file */
	static int npts;	/* # of points to shoot: x,y */

	npts = 512;
	azimuth = -35.0;			/* GIFT defaults */
	elevation = -25.0;

	if( argc < 1 )  {
		fprintf(stderr, usage);
		exit(1);
	}

	argc--; argv++;
	while( argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case 'M':
			matflag = 1;
			break;
		case 'A':
			AmbientIntensity = atof( &argv[0][2] );
			break;
		case 'x':
			sscanf( &argv[0][2], "%x", &debug );
			fprintf(stderr,"debug=x%x\n", debug);
			break;
		case 'f':
			/* "Fast" -- just a few pixels.  Or, arg's worth */
			npts = atoi( &argv[0][2] );
			if( npts < 2 || npts > 1024 )  {
				npts = 50;
			}
			break;
		case 'a':
			/* Set azimuth */
			azimuth = atof( &argv[0][2] );
			matflag = 0;
			break;
		case 'e':
			/* Set elevation */
			elevation = atof( &argv[0][2] );
			matflag = 0;
			break;
		case 'l':
			/* Select lighting model # */
			lightmodel = atoi( &argv[0][2] );
			break;
		case 'o':
			/* Output pixel file name */
			if( (outfd = creat( argv[1], 0444 )) <= 0 )  {
				perror( argv[1] );
				exit(10);
			}
			argc--; argv++;
			break;
		case 'p':
			perspective = 1;
			if( argv[0][2] != '\0' )
				zoomout = atof( &argv[0][2] );
			if( zoomout <= 0 )  zoomout = 1;
			break;
		default:
			fprintf(stderr,"rt:  Option '%c' unknown\n", argv[0][1]);
			fprintf(stderr, usage);
			break;
		}
		argc--; argv++;
	}

	if( argc < 2 )  {
		fprintf(stderr, usage);
		exit(2);
	}

	title_file = argv[0];
	title_obj = argv[1];

	prep_timer();		/* Start timing preparations */

	/* Build directory of GED database */
	if( dir_build( argv[0], 1 ) < 0 )  {
		fprintf(stderr,"rt:  dir_build failure\n");
		exit(2);
	}
	argc--; argv++;

	(void)pr_timer("DB TOC");
	prep_timer();

	/* Load the desired portion of the model */
	while( argc > 0 )  {
		(void)get_tree(argv[0]);
		argc--; argv++;
	}
	(void)pr_timer("DB WALK");
	prep_timer();

	/* Allow library to prepare itself */
	rt_prep();

	/* initialize application */
	view_init( &ap, title_file, title_obj, npts, outfd );

	(void)pr_timer("PREP");

	if( HeadSolid == SOLTAB_NULL )  {
		fprintf(stderr,"rt: No solids remain after prep.\n");
		exit(3);
	}
	fprintf(stderr,"shooting at %d solids in %d regions\n",
		nsolids, nregions );

	fprintf(stderr,"model X(%f,%f), Y(%f,%f), Z(%f,%f)\n",
		mdl_min[X], mdl_max[X],
		mdl_min[Y], mdl_max[Y],
		mdl_min[Z], mdl_max[Z] );

do_more:
	if( !matflag )  {
		vect_t view_min;		/* view position of mdl_min */
		vect_t view_max;		/* view position of mdl_max */
		mat_t Viewrot, toViewcenter;
		fastf_t f;
		fastf_t viewsize;

		/*
		 * Unrotated view is TOP.
		 * Rotation of 270,0,270 takes us to a front view.
		 * Standard GIFT view is -35 azimuth, -25 elevation off front.
		 */
		mat_idn( Viewrot );
		mat_angles( Viewrot, 270.0-elevation, 0.0, 270.0+azimuth );
		fprintf(stderr,"Viewing %f azimuth, %f elevation off of front view\n",
			azimuth, elevation);

		viewbounds( view_min, view_max, Viewrot );
		viewsize = (view_max[X]-view_min[X]);
		f = (view_max[Y]-view_min[Y]);
		if( f > viewsize )  viewsize = f;
		f = (view_max[Z]-view_min[Z]);
		if( f > viewsize )  viewsize = f;

		mat_idn( toViewcenter );
		toViewcenter[MDX] = -(view_max[X]+view_min[X])/2;
		toViewcenter[MDY] = -(view_max[Y]+view_min[Y])/2;
		toViewcenter[MDZ] = -(view_max[Z]+view_min[Z])/2;

		mat_mul( model2view, Viewrot, toViewcenter );
		model2view[15] = 0.5*viewsize;	/* Viewscale */
	}  else  {
		register int i;

		/* Visible part is from -1 to +1 in view space */
		for( i=0; i < 16; i++ )
			if( scanf( "%f", &model2view[i] ) != 1 )
				exit(0);
	}
	mat_inv( view2model, model2view );

	fprintf(stderr,"Deltas=%f (model units between rays)\n",
		model2view[15]*2/npts );

	VSET( temp, 0, 0, -1 );
	MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
	VUNITIZE( ap.a_ray.r_dir );

	/* Chop -1.0..+1.0 range into npts parts */
	VSET( temp, 2.0/npts, 0, 0 );
	MAT4X3VEC( dx_model, view2model, temp );
	VSET( temp, 0, 2.0/npts, 0 );
	MAT4X3VEC( dy_model, view2model, temp );

	VSET( temp, 0, 0, zoomout );
	MAT4X3PNT( eye_model, view2model, temp );	/* perspective only */

	/* "Upper left" corner of viewing plane */
	if( perspective )  {
		VSET( temp, -1, -1, 0 );	/* viewing plane */
	}  else  {
		VSET( temp, -1, -1, 1 );	/* eye plane */
	}
	MAT4X3PNT( viewbase_model, view2model, temp );

	/* initialize lighting */
	view_2init( &ap );

	fflush(stdout);
	fflush(stderr);
	prep_timer();	/* start timing actual run */

	for( ap.a_y = npts-1; ap.a_y >= 0; ap.a_y--)  {
		for( ap.a_x = 0; ap.a_x < npts; ap.a_x++)  {
			VJOIN2( ap.a_ray.r_pt, viewbase_model,
				ap.a_x, dx_model, 
				(npts-ap.a_y-1), dy_model );
			if( perspective )  {
				VSUB2( ap.a_ray.r_dir,
					ap.a_ray.r_pt, eye_model );
				VUNITIZE( ap.a_ray.r_dir );
				VMOVE( ap.a_ray.r_pt, eye_model );
			}

			ap.a_level = 0;		/* recursion level */
			shootray( &ap );
			view_pixel( &ap );
		}
		view_eol( &ap );	/* End of scan line */
	}
	view_end( &ap );		/* End of application */

	/*
	 *  All done.  Display run statistics.
	 */
	utime = pr_timer("SHOT");
	fprintf(stderr,"ft_shot(): %ld = %ld hits + %ld miss\n",
		nshots, nhits, nmiss );
	fprintf(stderr,"pruned:  %ld model RPP, %ld sub-tree RPP, %ld solid RPP\n",
		nmiss_model, nmiss_tree, nmiss_solid );
	fprintf(stderr,"pruning efficiency %.1f%%\n",
		((double)nhits*100.0)/nshots );
	fprintf(stderr,"%d output rays in %f sec = %f rays/sec\n",
		npts*npts, utime, (double)(npts*npts/utime) );

	if( matflag )  goto do_more;
	return(0);
}
