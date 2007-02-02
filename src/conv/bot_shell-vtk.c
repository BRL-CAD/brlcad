/*                 B O T _ S H E L L - V T K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file bot_shell-vtk.c
 *	This program uses raytracing to determine which triangles of the specified objects
 *	are external, and includes them in an output VTK polydata file.
 *
 *	Each triangle gets one ray shot at it along its normal from outside the model bounding box.
 *	If that triangle appears as the first object hit along the ray, then that triangle is
 *	added to the output list. Only triangles from BOT primitives are considered as candidates
 *	for inclusion in the VTK data. Non-BOT primitives that get hit along the ray may
 *	"hide" BOT triangles, but they will not be represented in the output.
 *
 *	The "-m" option specifies that triangles that are first or last on each ray should
 *	be included in the output. This can improve performance, but can also degrade performance
 *	considerabley. With the "-m" option, "onehit" processing is turned off.
 *
 *	If a "-g" option is provided, then rays are shot from a uniform grid from three orthogonal
 *	directions and the first and last triangles hit are included in the output (implies "-m").
 *
 *	If a "-n" option is provided, then vertex normals will be included in the VTK output data.
 *	Note that this will not provide any additional information unless the BOT primitives
 *	in the model have vertex normals. This can significantly increase the size of the VTK
 *	output file.
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#else
#  if defined(HAVE_SYS_UNISTD_H)
#    include <sys/unistd.h>
#  endif
#endif

/* interface headers */
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"

/* local headers */
#include "../librt/plane.h"
#include "../librt/bot.h"


static int debug=0;
static int use_normals=0;
static double cell_size=0.0;
static char *output_file;
static FILE *fd_out;
static struct rt_i *rtip;
static struct bn_tol tol;
static struct vert_root *verts;
static long *faces=NULL;
static long max_faces=0;
static long num_faces=0;
#define FACES_BLOCK	512

static char *usage="Usage:\n\
	%s [-m] [-n] [-d debug_level] [-g cell_size] -o vtk_polydata_output_file database.g object1 object2...\n";

/* routine to replace default overlap handler.
 * overlaps are irrelevant to this application
 */
static int
a_overlap( ap, pp, reg1, reg2, pheadp )
register struct application     *ap;
register struct partition       *pp;
struct region                   *reg1;
struct region                   *reg2;
struct partition                *pheadp;
{
	return( 1 );
}


static int
miss( ap )
register struct application *ap;
{
	return(0);
}

static void
Add_face( int face[3] )
{
	long i;

	if( debug) {
		bu_log( "Adding face %d %d %d\n", V3ARGS( face ) );
		for( i=0 ; i<3 ; i++ ) {
			bu_log( "\t( %g %g %g )\n", V3ARGS( &verts->the_array[ face[i]*3 ] ) );
		}
	}

	for( i=0 ; i<num_faces*3 ; i+=3 ) {
		if( faces[i] == face[0] ) {
			if( faces[i+1] == face[1] && faces[i+2] == face[2] ) {
				if( debug ) {
					bu_log( "Duplicate face ignored\n" );
				}
				return;
			}
		}
	}

	if( num_faces >= max_faces ) {
		max_faces += FACES_BLOCK;
		faces = (long *)bu_realloc( (genptr_t)faces, max_faces*3*sizeof(long), "faces array" );
	}

	VMOVE( &faces[num_faces*3], face );
	num_faces++;
}

static int
hit( struct application *ap, struct partition *part, struct seg *seg )
{
	struct partition *p;
	int surfno;
	struct soltab *stp;
	double x, y, z, nx, ny, nz;
	int face[3];
	int i;
	struct tri_specific *tri;
	struct bot_specific *bot;


	if( debug ) {
		bu_log( "got a hit\n" );
	}

	/* get the first hit */
	p = part->pt_forw;
	surfno = p->pt_inhit->hit_surfno;
	stp = p->pt_inseg->seg_stp;
	if( stp->st_id != ID_BOT ) {
		if( debug ) {
			bu_log( "hit a non-BOT primitive (ignoring)\n" );
		}
		if( ap->a_onehit != 0 ) {
			return 0;
		}
	} else {
		bot = (struct bot_specific *)stp->st_specific;
		if( bot->bot_facearray ) {
			tri = bot->bot_facearray[surfno];
		} else {
			i = bot->bot_ntri - 1;
			tri = bot->bot_facelist;
			while( i != surfno ) {
				i--;
				tri = tri->tri_forw;
			}
		}
		if( debug ) {
			bu_log( "\thit at (%g %g %g) on %s surfno = %d\n",
				V3ARGS( p->pt_inhit->hit_point ), stp->st_dp->d_namep, surfno );
		}


		/* get the first vertex */
		x = tri->tri_A[X];
		y = tri->tri_A[Y];
		z = tri->tri_A[Z];
		if( tri->tri_normals ) {
			nx = tri->tri_normals[X];
			ny = tri->tri_normals[Y];
			nz = tri->tri_normals[Z];
		} else {
			nx = tri->tri_N[X];
			ny = tri->tri_N[Y];
			nz = tri->tri_N[Z];
		}

		/* add this vertex to the vertex tree */
		if( use_normals ) {
			face[0] = Add_vert_and_norm( x, y, z, nx, ny, nz, verts, tol.dist_sq );
		} else {
			face[0] = Add_vert( x, y, z, verts, tol.dist_sq );
		}
		if( debug ) {
			bu_log( "\tvertex %d = ( %g %g %g ), norm = (%g %g %g )\n",
				face[0], x, y, z, nx, ny, nz );
		}

		/* get the second vertex */
		x = tri->tri_A[X] + tri->tri_BA[X];
		y = tri->tri_A[Y] + tri->tri_BA[Y];
		z = tri->tri_A[Z] + tri->tri_BA[Z];
		if( tri->tri_normals ) {
			nx = tri->tri_normals[X+3];
			ny = tri->tri_normals[Y+3];
			nz = tri->tri_normals[Z+3];
		} else {
			nx = tri->tri_N[X];
			ny = tri->tri_N[Y];
			nz = tri->tri_N[Z];
		}

		/* add this vertex to the vertex tree */
		if( use_normals ) {
			face[1] = Add_vert_and_norm( x, y, z, nx, ny, nz, verts, tol.dist_sq );
		} else {
			face[1] = Add_vert( x, y, z, verts, tol.dist_sq );
		}
		if( debug ) {
			bu_log( "\tvertex %d = ( %g %g %g ), norm = (%g %g %g )\n",
				face[1], x, y, z, nx, ny, nz );
		}

		/* get the third vertex */
		x = tri->tri_A[X] + tri->tri_CA[X];
		y = tri->tri_A[Y] + tri->tri_CA[Y];
		z = tri->tri_A[Z] + tri->tri_CA[Z];
		if( tri->tri_normals ) {
			nx = tri->tri_normals[X+6];
			ny = tri->tri_normals[Y+6];
			nz = tri->tri_normals[Z+6];
		} else {
			nx = tri->tri_N[X];
			ny = tri->tri_N[Y];
			nz = tri->tri_N[Z];
		}

		/* add this vertex to the vertex tree */
		if( use_normals ) {
			face[2] = Add_vert_and_norm( x, y, z, nx, ny, nz, verts, tol.dist_sq );
		} else {
			face[2] = Add_vert( x, y, z, verts, tol.dist_sq );
		}
		if( debug ) {
			bu_log( "\tvertex %d = ( %g %g %g ), norm = (%g %g %g )\n",
				face[2], x, y, z, nx, ny, nz );
		}

		/* add this face to our list (Add_face checks for duplicates) */
		Add_face( face );
	}


	if( ap->a_onehit != 0 ) {
		return 1;
	}

	/* get the last hit */
	p = part->pt_back;
	if( p == part->pt_forw ) {
		return 1;
	}
	surfno = p->pt_outhit->hit_surfno;
	stp = p->pt_outseg->seg_stp;
	if( stp->st_id != ID_BOT ) {
		if( debug ) {
			bu_log( "hit a non-BOT primitive (ignoring)\n" );
		}
		return 0;
	}
	bot = (struct bot_specific *)stp->st_specific;
	if( bot->bot_facearray ) {
		tri = bot->bot_facearray[surfno];
	} else {
		i = bot->bot_ntri - 1;
		tri = bot->bot_facelist;
		while( i != surfno ) {
			i--;
			tri = tri->tri_forw;
		}
	}
	if( debug ) {
		bu_log( "\thit at (%g %g %g) on %s surfno = %d\n",
			V3ARGS( p->pt_inhit->hit_point ), stp->st_dp->d_namep, surfno );
	}


	/* get the first vertex */
	x = tri->tri_A[X];
	y = tri->tri_A[Y];
	z = tri->tri_A[Z];
	if( tri->tri_normals ) {
		nx = tri->tri_normals[X];
		ny = tri->tri_normals[Y];
		nz = tri->tri_normals[Z];
	} else {
		nx = tri->tri_N[X];
		ny = tri->tri_N[Y];
		nz = tri->tri_N[Z];
	}

	/* add this vertex to the vertex tree */
	if( use_normals ) {
		face[0] = Add_vert_and_norm( x, y, z, nx, ny, nz, verts, tol.dist_sq );
	} else {
		face[0] = Add_vert( x, y, z, verts, tol.dist_sq );
	}
	if( debug ) {
		bu_log( "\tvertex %d = ( %g %g %g ), norm = (%g %g %g )\n",
			face[0], x, y, z, nx, ny, nz );
	}

	/* get the second vertex */
	x = tri->tri_A[X] + tri->tri_BA[X];
	y = tri->tri_A[Y] + tri->tri_BA[Y];
	z = tri->tri_A[Z] + tri->tri_BA[Z];
	if( tri->tri_normals ) {
		nx = tri->tri_normals[X+3];
		ny = tri->tri_normals[Y+3];
		nz = tri->tri_normals[Z+3];
	} else {
		nx = tri->tri_N[X];
		ny = tri->tri_N[Y];
		nz = tri->tri_N[Z];
	}

	/* add this vertex to the vertex tree */
	if( use_normals ) {
		face[1] = Add_vert_and_norm( x, y, z, nx, ny, nz, verts, tol.dist_sq );
	} else {
		face[1] = Add_vert( x, y, z, verts, tol.dist_sq );
	}
	if( debug ) {
		bu_log( "\tvertex %d = ( %g %g %g ), norm = (%g %g %g )\n",
			face[1], x, y, z, nx, ny, nz );
	}

	/* get the first vertex */
	x = tri->tri_A[X] + tri->tri_CA[X];
	y = tri->tri_A[Y] + tri->tri_CA[Y];
	z = tri->tri_A[Z] + tri->tri_CA[Z];
	if( tri->tri_normals ) {
		nx = tri->tri_normals[X+6];
		ny = tri->tri_normals[Y+6];
		nz = tri->tri_normals[Z+6];
	} else {
		nx = tri->tri_N[X];
		ny = tri->tri_N[Y];
		nz = tri->tri_N[Z];
	}

	/* add this vertex to the vertex tree */
	if( use_normals ) {
		face[2] = Add_vert_and_norm( x, y, z, nx, ny, nz, verts, tol.dist_sq );
	} else {
		face[2] = Add_vert( x, y, z, verts, tol.dist_sq );
	}
	if( debug ) {
		bu_log( "\tvertex %d = ( %g %g %g ), norm = (%g %g %g )\n",
			face[2], x, y, z, nx, ny, nz );
	}

	Add_face( face );

	return 1;
}

int
main( argc, argv )
int argc;
char *argv[];
{
	char idbuf[132];
	struct application ap;
	int dir;
	int c;
	long i;
	int database_index;

	if( debug ) {
		bu_debug = BU_DEBUG_COREDUMP;
	}

	bu_setlinebuf( stderr );

	/* These need to be improved */
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-5;
	tol.para = 1 - tol.perp;

	/* Get command line arguments. */
	bzero( &ap, sizeof( struct application ) );
	ap.a_onehit = 1;
	while( (c=bu_getopt( argc, argv, "nmd:g:o:")) != EOF)
	{
		switch( c )
		{
			case 'd':	/* debug level */
				debug = atoi( bu_optarg );
				break;
			case 'm':	/* use first and last hits */
				ap.a_onehit = 0;
				break;
			case 'g':	/* cell size */
				cell_size = atof( bu_optarg );
				if( cell_size < tol.dist ) {
					bu_log( "Cell size too small!!! (%g)\n", cell_size );
					exit( 1 );
				}
				break;
			case 'o':	/* VTK polydata output file */
				output_file = bu_optarg;
				break;
			case 'n':	/* include normals in the VTK data */
				use_normals = 1;
				break;
		}
	}

	if (bu_optind+1 >= argc)
	{
		bu_log( usage, argv[0] );
		exit( 1 );
	}

	if( output_file )
	{
		if( (fd_out=fopen( output_file, "w" )) == NULL )
		{
			bu_log( "Cannot open output file (%s)\n", output_file );
			perror( argv[0] );
			exit( 1 );
		}
	}
	else
		bu_bomb( "Output file must be specified!!!\n" );

	/* Open BRL-CAD database */
	database_index = bu_optind;
	if ((rtip=rt_dirbuild(argv[bu_optind], idbuf, sizeof(idbuf))) == RTI_NULL )
	{
		bu_log( "rt_durbuild FAILED on %s\n", argv[bu_optind] );
		exit(1);
	}

	rtip->rti_space_partition = RT_PART_NUBSPT;

	ap.a_rt_i = rtip;
	ap.a_hit = hit;
	ap.a_miss = miss;
	ap.a_overlap = a_overlap;
	ap.a_logoverlap = rt_silent_logoverlap;

	while( ++bu_optind < argc )
	{
		if( rt_gettree( rtip, argv[bu_optind] ) < 0 )
			bu_log( "rt_gettree failed on %s\n", argv[bu_optind] );
	}

	rt_prep( rtip );

	/* create vertex tree */
	if( use_normals ) {
		verts = create_vert_tree_w_norms();
	} else {
		verts = create_vert_tree();
	}

	if( cell_size != 0.0 ) {
		/* do a grid of shots */

		ap.a_onehit = 0;
		for( dir=X ; dir<=Z ; dir++ ) {
			int grid_dir1, grid_dir2;

			if( debug ) {
				bu_log( "************** Process direction %d\n", dir );
			}
			grid_dir1 = X;
			if( grid_dir1 == dir ) {
				grid_dir1++;
			}
			grid_dir2 = grid_dir1 + 1;
			if( grid_dir2 == dir ) {
				grid_dir2++;
			}
			VSETALL( ap.a_ray.r_dir, 0.0 );
			ap.a_ray.r_dir[dir] = 1.0;

			/* back off a smidge, just to be safe */
			ap.a_ray.r_pt[dir] = rtip->mdl_min[dir] - 5.0;

			/* move just inside the bounding box */
			ap.a_ray.r_pt[grid_dir1] = rtip->mdl_min[grid_dir1] + tol.dist;

			/* now fire a grid of rays spaced at "cell_size" distance */
			while( ap.a_ray.r_pt[grid_dir1] <= rtip->mdl_max[grid_dir1] ) {
				ap.a_ray.r_pt[grid_dir2] = rtip->mdl_min[grid_dir2] + tol.dist;
				while( ap.a_ray.r_pt[grid_dir2] <= rtip->mdl_max[grid_dir2] ) {

					/* shoot a ray */
					if( debug ) {
						bu_log( "Shooting a ray from (%g %g %g) in direction (%g %g %g)\n",
							V3ARGS( ap.a_ray.r_pt ) , V3ARGS( ap.a_ray.r_dir ) );
					}
					(void)rt_shootray( &ap );
					ap.a_ray.r_pt[grid_dir2] += cell_size;
				}
				ap.a_ray.r_pt[grid_dir1] += cell_size;
			}
		}
	} else {
		struct soltab *stp;
		struct bot_specific *bot;
		struct tri_specific *tri;
		vect_t inv_dir;

		/* shoot at every triangle */
		for( i=0 ; i<rtip->nsolids ; i++ ) {
			stp = rtip->rti_Solids[i];
			if( stp->st_id != ID_BOT ) {
				continue;
			}

			bot = (struct bot_specific *)stp->st_specific;
			tri = bot->bot_facelist;
			while( tri ) {
				point_t p2, p3, sum;

				VADD2( p2, tri->tri_A, tri->tri_BA );
				VADD2( p3, tri->tri_A, tri->tri_CA );
				VSETALL( sum, 0.0 );
				VADD2( sum, sum, tri->tri_A );
				VADD2( sum, sum, p2 );
				VADD2( sum, sum, p3 );
				VSCALE( ap.a_ray.r_pt, sum, 1.0/3.0 );
				VREVERSE( ap.a_ray.r_dir, tri->tri_N );
				VINVDIR( inv_dir, ap.a_ray.r_dir );
				if( rt_in_rpp( &ap.a_ray, inv_dir, rtip->mdl_min, rtip->mdl_max ) == 0 ) {
					tri = tri->tri_forw;
					continue;
				}
				VJOIN1( ap.a_ray.r_pt, ap.a_ray.r_pt, (ap.a_ray.r_min - 1000.0), ap.a_ray.r_dir );

				/* shoot a ray */
				if( debug ) {
					point_t B, C;

					VADD2( B, tri->tri_A, tri->tri_BA );
					VADD2( C, tri->tri_A, tri->tri_CA );
					bu_log( "Shooting a ray from (%g %g %g) in direction (%g %g %g)\n",
						V3ARGS( ap.a_ray.r_pt ) , V3ARGS( ap.a_ray.r_dir ) );
					bu_log( "\tAt triangle ( %g %g %g ) ( %g %g %g ) ( %g %g %g )\n",
						 V3ARGS( tri->tri_A ), V3ARGS( B ) , V3ARGS( C ) );
				}
				(void)rt_shootray( &ap );
				tri = tri->tri_forw;
			}
		}
}

	/* now write out the results */
	if( debug ) {
		bu_log( "Writing output (%ld vertices and %d faces)\n", verts->curr_vert, num_faces );
	}
	fprintf( fd_out, "# vtk DataFile Version 1.0\n" );
	fprintf( fd_out, "%s", argv[database_index] );
	database_index++;
	while( database_index < argc ) {
		fprintf( fd_out, " %s", argv[database_index] );
		database_index++;
	}
	fprintf( fd_out, "\nASCII\n\nDATASET POLYDATA\n" );
	fprintf( fd_out, "POINTS %ld float\n", verts->curr_vert );
	for( i=0 ; i<verts->curr_vert ; i++ ) {
		if( use_normals ) {
			fprintf( fd_out, "%g %g %g\n", V3ARGS( &verts->the_array[i*6] ) );
		} else {
			fprintf( fd_out, "%g %g %g\n", V3ARGS( &verts->the_array[i*3] ) );
		}
	}
	fprintf( fd_out, "POLYGONS %ld %ld\n", num_faces, num_faces*4 );
	for( i=0 ; i<num_faces ; i++ ) {
		fprintf( fd_out, "3 %ld %ld %ld\n", V3ARGS( &faces[i*3] ) );
	}
	if( use_normals ) {
		fprintf( fd_out, "POINT_DATA %ld\n", verts->curr_vert );
		fprintf( fd_out, "NORMALS default float\n" );
		for( i=0 ; i<verts->curr_vert ; i++ ) {
			fprintf( fd_out, "%g %g %g\n", V3ARGS( &verts->the_array[i*6+3] ) );
		}
	}

	return 0;
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
