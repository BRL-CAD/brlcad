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

extern int null_prep(),	null_print();
extern int tor_prep(),	tor_print();
extern int tgc_prep(),	tgc_print();
extern int ellg_prep(),	ellg_print();
extern int arb8_prep(),	arb8_print();
extern int ars_prep(),	ars_print();
extern int half_prep(),	half_print();

extern struct seg *null_shot();
extern struct seg *tor_shot();
extern struct seg *tgc_shot();
extern struct seg *ellg_shot();
extern struct seg *arb8_shot();
extern struct seg *ars_shot();
extern struct seg *half_shot();

struct functab functab[] = {
	null_prep,	null_shot,	null_print,	"ID_NULL",
	tor_prep,	tor_shot,	tor_print,	"ID_TOR",
	tgc_prep,	tgc_shot,	tgc_print,	"ID_TGC",
	ellg_prep,	ellg_shot,	ellg_print,	"ID_ELL",
	arb8_prep,	arb8_shot,	arb8_print,	"ID_ARB8",
	ars_prep,	ars_shot,	ars_print,	"ID_ARS",
	half_prep,	half_shot,	half_print,	"ID_HALF",
	null_prep,	null_shot,	null_print,	">ID_NULL"
};

extern struct partition *bool_regions();

int debug = DEBUG_OFF;

struct soltab *HeadSolid = SOLTAB_NULL;

struct seg *FreeSeg = SEG_NULL;		/* Head of freelist */

static struct seg *HeadSeg = SEG_NULL;

char usage[] = "Usage:  rt [-f[#]] [-x#] model.vg object [objects]\n";

/* Used for autosizing */
static fastf_t xbase, ybase, zbase;
static fastf_t deltas;

vect_t l0vec;		/* 0th light vector */
vect_t l1vec;		/* 1st light vector */
vect_t l2vec;		/* 2st light vector */

main(argc, argv)
int argc;
char **argv;
{
	register struct ray *rayp;
	register int xscreen, yscreen;
	static int npts;		/* # of points to shoot: x,y */
	static mat_t viewrot;
	static mat_t invview;
	static vect_t tempdir;
	static struct partition *PartHeadp, *pp;
	static fastf_t distsq;

	npts = 200;

	if( argc < 1 )  {
		printf(usage);
		exit(1);
	}

	argc--; argv++;
	while( argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case 'x':
			sscanf( &argv[0][2], "%x", &debug );
			printf("debug=x%x\n", debug);
			break;
		case 'f':
			/* "Fast" -- just a few pixels.  Or, arg's worth */
			npts = atoi( &argv[0][2] );
			if( npts < 2 || npts > 1024 )
				npts = 50;
			break;
		default:
			printf("Unknown option '%c' ignored\n", argv[0][1]);
			printf(usage);
			break;
		}
		argc--; argv++;
	}

	if( argc < 2 )  {
		printf(usage);
		exit(2);
	}
	/* Build directory of GED database */
	dir_build( argv[0] );
	argc--; argv++;

	if( !(debug&DEBUG_QUICKIE) )  {
		/* Prepare the Ikonas display */
		ikopen();
		load_map(1);
		ikclear();
	}

	/* Load the desired portion of the model */
	while( argc > 0 )  {
		get_tree(argv[0]);
		argc--; argv++;
	}

	if( HeadSolid == 0 )  bomb("No solids");

	/* Determine a view */
	GETSTRUCT(rayp, ray);

	mat_idn( invview );
/**	mat_angles( invview, 290.0, 0.0, 310.0 ); */
	mat_angles( invview, 295.0, 0.0, 235.0 );
/**	mat_ae( invview, 360.-35.0, 360.-25.0 ); * */
/**	mat_ae( invview, -35.0, -25.0 ); * */
/**	mat_ae( invview, 180.0+35.0, 180.0+25.0 ); ** */
	mat_trn( viewrot, invview );		/* inverse */
	mat_print( "View Rotation", viewrot );

	if( !(debug&DEBUG_QUICKIE) )  {
		autosize( viewrot, npts );
		VSET( tempdir, 0, 0, -1 );
	} else {
		xbase = -3;
		ybase = -3;
		zbase = -10;
		deltas = 1;
		npts = 8;
		VSET( tempdir, 0, 0, 1 );
	}
	MAT3XVEC( rayp->r_dir, viewrot, tempdir );
VPRINT("Direction", rayp->r_dir);
	/* Sanity check */
	distsq = MAGSQ(rayp->r_dir) - 1.0;
	if( !NEAR_ZERO(distsq) )
		printf("ERROR: |r_dir|**2 - 1 = %f != 0\n", distsq);

	VSET( tempdir, 	xbase, ybase, zbase );
	MAT3XVEC( rayp->r_pt, viewrot, tempdir );
VPRINT("Starting point", tempdir );

	/* Determine the Light location(s) in model space, xlate to view */
	/* 0:  Blue, at left edge, 1/2 high */
	tempdir[0] = 2 * (xbase);
	tempdir[1] = (2/2) * (ybase);
	tempdir[2] = 2 * (zbase + npts*deltas);
VPRINT("Light0 Pos", tempdir);
	MAT3XVEC( l0vec, viewrot, tempdir );
	VUNITIZE(l0vec);
VPRINT("Light0 Vec", l0vec);

	/* 1: Red, at right edge, 1/2 high */
	tempdir[0] = 2 * (xbase + npts*deltas);
	tempdir[1] = (2/2) * (ybase);
	tempdir[2] = 2 * (zbase + npts*deltas);
VPRINT("Light1 Pos", tempdir);
	MAT3XVEC( l1vec, viewrot, tempdir );
	VUNITIZE(l1vec);
VPRINT("Light1 Vec", l1vec);

	/* 2:  Green, behind, and overhead */
	tempdir[0] = 2 * (xbase + (npts/2)*deltas);
	tempdir[1] = 2 * (ybase + npts*deltas);
	tempdir[2] = 2 * (zbase + (npts/2)*deltas);
VPRINT("Light2 Pos", tempdir);
	MAT3XVEC( l2vec, viewrot, tempdir );
	VUNITIZE(l2vec);
VPRINT("Light2 Vec", l2vec);

	for( yscreen = npts-1; yscreen >= 0; yscreen--)  {
		for( xscreen = 0; xscreen < npts; xscreen++)  {
			VSET( tempdir,
				xbase + xscreen * deltas,
				ybase + (npts-yscreen-1) * deltas,
				zbase +  2*npts*deltas );
			MAT3XVEC( rayp->r_pt, viewrot, tempdir );

			shootray( rayp );
			/* Implicit return of HeadSeg chain */

			if( HeadSeg == SEG_NULL )  {
				wbackground( xscreen, yscreen );
				continue;
			}

			/*
			 *  All intersections of the ray with the model have
			 *  been computed.  Evaluate the boolean functions.
			 */
			PartHeadp = bool_regions( HeadSeg );
			if( PartHeadp->pt_forw == PartHeadp )  {
				wbackground( xscreen, yscreen );
				continue;
			}

			/*
			 * Hand final partitioned intersection list
			 * to application.
			 */
			viewit( PartHeadp, rayp, xscreen, yscreen );

			/*
			 * Processing of this ray is complete.
			 * Release resources.
			 *
			 * Free up Seg memory.
			 */
			while( HeadSeg != 0 )  {
				register struct seg *hsp;	/* XXX */

				hsp = HeadSeg->seg_next;
				FREE_SEG( HeadSeg );
				HeadSeg = hsp;
			}
			/* Free up partition list */
			for( pp = PartHeadp->pt_forw; pp != PartHeadp;  )  {
				register struct partition *newpp;
				newpp = pp;
				pp = pp->pt_forw;
				FREE_PART(newpp);
			}
		}
	}
}

shootray( rayp )
register struct ray *rayp;
{
	register struct soltab *stp;
	static vect_t diff;	/* diff between shot base & solid center */
	FAST fastf_t distsq;	/* distance**2 */

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", rayp->r_pt);
		VPRINT("Ray Direction", rayp->r_dir);
	}

	HeadSeg = SEG_NULL;	/* Should check, actually */

	/* For now, shoot at all solids in model */
	for( stp=HeadSolid; stp != 0; stp=stp->st_forw ) {
		register struct seg *newseg;		/* XXX */

		if(debug&DEBUG_ALLRAYS)
			printf("Shooting at %s -- ", stp->st_name);

		/* Consider bounding sphere */
		VSUB2( diff, stp->st_center, rayp->r_pt );
		distsq = VDOT(rayp->r_dir, diff);
		if( (distsq=(MAGSQ(diff) - distsq*distsq)) > stp->st_radsq ) {
			if(debug&DEBUG_ALLRAYS)  printf("(Not close)\n");
			continue;
		}
		if( distsq < -EPSILON )  {
			printf("ERROR in %s:  dist**2 = %f -- skipped\n",
				stp->st_name, distsq );
			continue;
		}

		newseg = functab[stp->st_id].ft_shot( stp, rayp );
		if( newseg == SEG_NULL )
			continue;

		/* First, some checking */
		if( newseg->seg_in.hit_dist > newseg->seg_out.hit_dist )  {
			struct hit temp;	/* XXX */
			printf("ERROR %s %s: in/out reversal (%f,%f)\n",
				functab[stp->st_id].ft_name,
				newseg->seg_stp->st_name,
				newseg->seg_in.hit_dist,
				newseg->seg_out.hit_dist );
			temp = newseg->seg_in;		/* struct copy */
			newseg->seg_in = newseg->seg_out; /* struct copy */
			newseg->seg_out = temp;		/* struct copy */
		}
		/* Add to list */
		newseg->seg_next = HeadSeg;
		HeadSeg = newseg;
	}
}

autosize( rot, npts )
matp_t rot;
int npts;
{
	register struct soltab *stp;
	static float xmin, xmax;
	static float ymin, ymax;
	static float zmin, zmax;
	static vect_t xlated;
	static mat_t invrot;

	mat_trn( invrot, rot );		/* Inverse rotation matrix */

	/* init maxima and minima */
	xmax = ymax = zmax = -100000000.0;
	xmin = ymin = zmin =  100000000.0;

	for( stp=HeadSolid; stp != 0; stp=stp->st_forw ) {
		FAST fastf_t rad;

		rad = sqrt(stp->st_radsq);
		MAT3XVEC( xlated, invrot, stp->st_center );
#define MIN(v,t) {FAST fastf_t rt=(t); if(rt<v) v = rt;}
#define MAX(v,t) {FAST fastf_t rt=(t); if(rt>v) v = rt;}
		MIN( xmin, xlated[0]-rad );
		MAX( xmax, xlated[0]+rad );
		MIN( ymin, xlated[1]-rad );
		MAX( ymax, xlated[1]+rad );
		MIN( zmin, xlated[2]-rad );
		MAX( zmax, xlated[2]+rad );
	}

	/* Provide a slight border */
	xmin -= xmin * 0.03;
	ymin -= ymin * 0.03;
	zmin -= zmin * 0.03;
	xmax *= 1.03;
	ymax *= 1.03;
	zmax *= 1.03;

	xbase = xmin;
	ybase = ymin;
	zbase = zmin;

	deltas = (xmax-xmin)/npts;
	MAX( deltas, (ymax-ymin)/npts );
	printf("X(%f,%f), Y(%f,%f), Z(%f,%f)\nDeltas=%f\n",
		xmin, xmax, ymin, ymax, zmin, zmax, deltas );
}

bomb(str)
char *str;
{
	fflush(stdout);
	fprintf(stderr,"\nrt: %s.  FATAL ERROR.\n", str);
	exit(12);
}

pr_seg(segp)
register struct seg *segp;
{
	printf("SEG at %.8x:  flag=%x (%f,%f) solid=%s\n",
		segp, segp->seg_flag,
		segp->seg_in.hit_dist,
		segp->seg_out.hit_dist,
		segp->seg_stp->st_name );
}
