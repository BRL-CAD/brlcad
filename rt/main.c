/*
 *			R T
 *
 * Ray Tracing program
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	March 27, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "ray.h"
#include "debug.h"

extern int null_prep(),	null_shot(),	null_print();
extern int tor_prep(),	tor_shot(),	tor_print();
extern int tgc_prep(),	tgc_shot(),	tgc_print();
extern int ellg_prep(),	ellg_shot(),	ellg_print();
extern int arb8_prep(),	arb8_shot(),	arb8_print();
extern int ars_prep(),	ars_shot(),	ars_print();
extern int half_prep(),	half_shot(),	half_print();

struct functab functab[] = {
	null_prep,	null_shot,	null_print,	/* ID_NULL */
	tor_prep,	tor_shot,	tor_print,	/* ID_TOR */
	tgc_prep,	tgc_shot,	tgc_print,	/* ID_TGC */
	ellg_prep,	ellg_shot,	ellg_print,	/* ID_ELL */
	arb8_prep,	arb8_shot,	arb8_print,	/* ID_ARB8 */
	ars_prep,	ars_shot,	ars_print,	/* ID_ARS */
	half_prep,	half_shot,	half_print,	/* ID_HALF */
	null_prep,	null_shot,	null_print,	/* ID_NULL */
};

int debug = DEBUG_OFF;

struct soltab *HeadSolid = 0;
struct seg *HeadSeg = 0;

char usage[] = "Usage:  rt [-d#] model.vg object [objects]";

main(argc, argv)
int argc;
char **argv;
{
	static struct ray *rayp;
	static float xbase, ybase, zbase;
	static float dx, dy, dz;
	static int xscreen, yscreen;
	static int xpts, ypts;
	static mat_t viewrot;
	static vect_t tempdir;
	static float *fp;

	if( argc < 1 )  {
		printf("%s\n", usage);
		exit(1);
	}

	argc--; argv++;
	while( argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case 'd':
			sscanf( &argv[0][2], "%x", &debug );
			printf("debug=x%x\n", debug);
			break;
		default:
			printf("Unknown option '%c' ignored\n", argv[0][1]);
			printf("%s\n", usage);
			break;
		}
		argc--; argv++;
	}
	/* Fetching database name, etc, here later */

	ikopen();
	load_map(1);
	ikclear();

	/* Load the desired portion of the model */
	get_tree("noname");
	if( HeadSolid == 0 )  bomb("No solids");

	/* Determine a view */
	GETSTRUCT(rayp, ray);

	ae_mat( viewrot, -35.0, -25.0 );
	mat_print( "View Rotation", viewrot );

	if( !(debug&DEBUG_QUICKIE) )  {
		xbase = -5.0; dx =  0.025;
		ybase =  6.0; dy = -0.025;
		zbase = 10.0;
		xpts = 400;
		ypts = 400;
		VSET( tempdir, 0, 0, -1 );
	} else {
		xbase = -3; dx = 1;
		ybase = -3; dy = 1;
		zbase = -10;
		xpts = 8;
		ypts = 8;
		VSET( tempdir, 0, 0, 1 );
	}
	fp = &rayp->r_dir[0];
	MAT3XVEC( fp, viewrot, tempdir );

	for( yscreen = 0; yscreen < ypts; yscreen++)  {
		for( xscreen = 0; xscreen < xpts; xscreen++)  {
			VSET( tempdir,
				xbase + xscreen * dx,
				ybase + yscreen * dy,
				zbase);
			MAT3XVEC( rayp->r_pt, viewrot, tempdir );

			shootray( rayp );
			/* Implicit return of HeadSeg chain */

			if( HeadSeg == 0 )  {
				wbackground( xscreen, yscreen );
				continue;
			}

			/*
			 *  All intersections of the ray with the model have
			 *  been computed.
			 */
			/* HeadSeg = closegaps( HeadSeg ) */

			/*
			 *  Hand final intersection list to application
			 */
			viewit( HeadSeg, rayp, xscreen, yscreen );

			/* Free up Seg memory.  Use freelist, later */
			while( HeadSeg != 0 )  {
				register struct seg *hsp;	/* XXX */

				hsp = HeadSeg->seg_next;
				free( HeadSeg );
				HeadSeg = hsp;
			}
		}
	}
}

shootray( rayp )
register struct ray *rayp;
{
	register struct soltab *stp;
	static float f;
	static vect_t diff;	/* diff between shot base & solid center */
	register float distsq;	/* distance**2 */

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", rayp->r_pt);
		VPRINT("Ray Direction", rayp->r_dir);
	}
	f = MAGSQ(rayp->r_dir) - 1.0;
	if( !NEAR_ZERO(f) )
		printf("ERROR: |r_dir|**2 - 1 = %f != 0\n", f);

	HeadSeg = 0;	/* Should check, actually */

	/* For now, shoot at all solids in model */
	for( stp=HeadSolid; stp != 0; stp=stp->st_forw ) {
		if(debug&DEBUG_ALLRAYS)
			printf("Shooting at %s -- ", stp->st_name);

		/* Consider bounding sphere */
		VSUB2( diff, stp->st_center, rayp->r_pt );
		distsq = VDOT(rayp->r_dir, diff);
		distsq = MAGSQ(diff) - distsq*distsq;
		if( distsq < 0.0 )  {
			printf("ERROR in %s:  dist**2 = %f -- skipped\n",
				stp->st_name, distsq );
			continue;
		}
		if( distsq > stp->st_radsq )  {
			if(debug&DEBUG_ALLRAYS)  printf("(Not close)\n");
			continue;
		}

		functab[stp->st_id].ft_shot( stp, rayp );
		/* ret == 0 for HIT */
		/* Adds results to HeadSeg chain, if it hit */
	}
}

bomb(str)
char *str;
{
	fprintf(stderr, "\nrt: %s\nFATAL ERROR\n", str);
	exit(12);
}

/*
 *  Compute a 4x4 rotation matrix given Azimuth and Elevation.
 *  
 *  Azimuth is +X, Elevation is +Z, both in degrees.
 *
 *  Formula due to Doug Gwyn, BRL.
 */
ae_mat( m, azimuth, elev )
register matp_t m;
float azimuth;
float elev;
{
	static float sin_az, sin_el;
	static float cos_az, cos_el;
	extern double sin(), cos();
	static double degtorad = 0.0174532925;

	azimuth *= degtorad;
	elev *= degtorad;

	sin_az = sin(azimuth);
	cos_az = cos(azimuth);
	sin_el = sin(elev);
	cos_el = cos(elev);

	m[0] = cos_el * cos_az;
	m[1] = -sin_az;
	m[2] = -sin_el * cos_az;
	m[3] = 0;

	m[4] = cos_el * sin_az;
	m[5] = cos_az;
	m[6] = -sin_el * sin_az;
	m[7] = 0;

	m[8] = sin_el;
	m[9] = 0;
	m[10] = cos_el;
	m[11] = 0;

	m[12] = m[13] = m[14] = 0;
	m[15] = 1.0;
}
