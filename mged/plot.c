/*
 *  			P L O T . C
 *  
 *	Provide UNIX-plot output of the current view.
 *
 *  Authors -
 *  	Michael John Muuss	(This version)
 *	Douglas A. Gwyn		(3-D UNIX Plot routines)
 *  	Gary S. Moss		(Original gedplot program)
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


#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "plot3.h"
#include "./solid.h"
#include "./dm.h"

/*
 *  			F _ P L O T
 *
 *  plot file [opts]
 *  potential options might include:
 *	grid, 3d w/color, |filter, infinite Z
 */
void
f_plot( argc, argv )
int	argc;
char	**argv;
{
	register struct solid *sp;
	register struct vlist *vp;
	register FILE *fp;
	static vect_t clipmin, clipmax;
	static vect_t last;		/* last drawn point */
	static vect_t fin;
	static vect_t start;
	int Three_D;			/* 0=2-D -vs- 1=3-D */
	int Z_clip;			/* Z clipping */
	int Dashing;			/* linetype is dashed */
	int floating;			/* 3-D floating point plot */
	char **argv;
	char buf[128];

	if( not_state( ST_VIEW, "UNIX Plot of view" ) )
		return;

	/* Process any options */
	Three_D = 1;				/* 3-D w/color, by default */
	Z_clip = 0;				/* NO Z clipping, by default*/
	floating = 0;
	argv = &argv[1];
	while( argv[0] != (char *)0 && argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case 'f':
			floating = 1;
			break;
		case '3':
			Three_D = 1;
			break;
		case '2':
			Three_D = 0;		/* 2-D, for portability */
			break;
		case 'g':
			/* do grid */
			(void)printf("grid unimplemented\n");
			break;
		case 'z':
		case 'Z':
			/* Enable Z clipping */
			(void)printf("Clipped in Z to viewing cube\n");
			Z_clip = 1;
			break;
		default:
			(void)printf("bad PLOT option %s\n", argv[0] );
			break;
		}
		argv++;
	}
	if( argv[0] == (char *)0 )  {
		printf("no filename or filter specified\n");
		return;
	}
	if( argv[0][0] == '|' )  {
		strncpy( buf, &argv[0][1], sizeof(buf) );
		while( (++argv)[0] != (char *)0 )  {
			strncat( buf, " ", sizeof(buf) );
			strncat( buf, argv[0], sizeof(buf) );
		}
		if( (fp = popen( buf, "w" ) ) == NULL )  {
			perror( buf );
			return;
		}
		(void)printf("piped to %s\n", buf );
	}  else  {
		if( (fp = fopen( argv[0], "w" )) == NULL )  {
			perror( argv[0] );
			return;
		}
		(void)printf("plot stored in %s\n", argv[0] );
	}

	color_soltab();		/* apply colors to the solid table */

	if( floating )  {
		pd_3space( fp,
			-toViewcenter[MDX] - Viewscale,
			-toViewcenter[MDY] - Viewscale,
			-toViewcenter[MDZ] - Viewscale,
			-toViewcenter[MDX] + Viewscale,
			-toViewcenter[MDY] + Viewscale,
			-toViewcenter[MDZ] + Viewscale );
		Dashing = 0;
		pl_linmod( fp, "solid" );
		FOR_ALL_SOLIDS( sp )  {
			/* Could check for differences from last color */
			pl_color( fp,
				sp->s_color[0],
				sp->s_color[1],
				sp->s_color[2] );
			if( Dashing != sp->s_soldash )  {
				if( sp->s_soldash )
					pl_linmod( fp, "dotdashed");
				else
					pl_linmod( fp, "solid");
				Dashing = sp->s_soldash;
			}
			for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
				if( vp->vl_draw )
					pdv_3cont( fp, vp->vl_pnt );
				else
					pdv_3move( fp, vp->vl_pnt );
			}
		}
		goto out;
	}

	/*
	 *  Integer output version, either 2-D or 3-D.
	 *  Viewing region is from -1.0 to +1.0
	 *  which is mapped to integer space -2048 to +2048 for plotting.
	 *  Compute the clipping bounds of the screen in view space.
	 */
	clipmin[X] = -1.0;
	clipmax[X] =  1.0;
	clipmin[Y] = -1.0;
	clipmax[Y] =  1.0;
	if( Z_clip )  {
		clipmin[Z] = -1.0;
		clipmax[Z] =  1.0;
	} else {
		clipmin[Z] = -1.0e20;
		clipmax[Z] =  1.0e20;
	}

	if( Three_D )
		pl_3space( fp, -2048, -2048, -2048, 2048, 2048, 2048 );
	else
		pl_space( fp, -2048, -2048, 2048, 2048 );
	pl_erase( fp );
	Dashing = 0;
	pl_linmod( fp, "solid");
	FOR_ALL_SOLIDS( sp )  {
		if( Dashing != sp->s_soldash )  {
			if( sp->s_soldash )
				pl_linmod( fp, "dotdashed");
			else
				pl_linmod( fp, "solid");
			Dashing = sp->s_soldash;
		}
		for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
			if( vp->vl_draw == 0 )  {
				/* Move, not draw */
				MAT4X3PNT( last, model2view, vp->vl_pnt );
				continue;
			}
			/* draw */
			MAT4X3PNT( fin, model2view, vp->vl_pnt );
			VMOVE( start, last );
			VMOVE( last, fin );
			if(
				vclip( start, fin, clipmin, clipmax ) == 0
			)  continue;

			if( Three_D )  {
				/* Could check for differences from last color */
				pl_color( fp,
					sp->s_color[0],
					sp->s_color[1],
					sp->s_color[2] );
				pl_3line( fp,
					(int)( start[X] * 2047 ),
					(int)( start[Y] * 2047 ),
					(int)( start[Z] * 2047 ),
					(int)( fin[X] * 2047 ),
					(int)( fin[Y] * 2047 ),
					(int)( fin[Z] * 2047 ) );
			}  else
				pl_line( fp,
					(int)( start[0] * 2047 ),
					(int)( start[1] * 2047 ),
					(int)( fin[0] * 2047 ),
					(int)( fin[1] * 2047 ) );
		}
	}
out:
	if( argv[1][0] == '|' )
		(void)pclose( fp );
	else
		(void)fclose( fp );
}

void
f_area( argc, argv )
int	argc;
char	**argv;
{
	register struct solid *sp;
	register struct vlist *vp;
	static vect_t last;
	static vect_t fin;
	char buf[128];
	FILE *fp;

	if( not_state( ST_VIEW, "Presented Area Calculation" ) )
		return;

	FOR_ALL_SOLIDS( sp )  {
		if( !sp->s_Eflag && sp->s_soldash != 0 )  {
			printf("everything in view must be 'E'ed\n");
			return;
		}
	}

	/* Create pipes to boundp | parea */
	if( argc == 2 )  {
		sprintf( buf, "boundp -t %s | parea", argv[1] );
		(void)printf("Tolerance is %s\n", argv[1] );
	}  else  {
		double tol = VIEWSIZE * 0.001;
		sprintf( buf, "boundp -t %e | parea", tol );
		(void)printf("Auto-tolerance of 0.1%% is %e\n", tol );
	}

	if( (fp = popen( buf, "w" )) == NULL )  {
		perror( buf );
		return;
	}

	/*
	 * Write out rotated but unclipped, untranslated,
	 * and unscaled vectors
	 */
	FOR_ALL_SOLIDS( sp )  {
		for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
			if( vp->vl_draw == 0 )  {
				/* Move, not draw */
				MAT4X3VEC( last, Viewrot, vp->vl_pnt );
				continue;
			}
			/* draw.  */
			MAT4X3VEC( fin, Viewrot, vp->vl_pnt );

			fprintf(fp, "%.9e %.9e %.9e %.9e\n",
				last[X] * base2local,
				last[Y] * base2local,
				fin[X] * base2local,
				fin[Y] * base2local );

			VMOVE( last, fin );
		}
	}
	(void)printf("Presented area from this viewpoint, square %s:\n",
		local_unit[localunit] );
	pclose( fp );
}
