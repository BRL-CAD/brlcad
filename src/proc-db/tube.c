/*                          T U B E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
/** @file tube.c
 *
 *  Program to generate a gun-tube as a procedural spline.
 *  The tube's core lies on the X axis.
 *
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "nurb.h"
#include "raytrace.h"
#include "wdb.h"

mat_t	identity;
double	degtorad = 0.0174532925199433;
double	inches2mm = 25.4;

void	build_spline(char *name, int npts, double radius), read_pos(FILE *fp), build_cyl(char *cname, int npts, double radius), xfinddir(fastf_t *dir, double x, fastf_t *loc);
int	read_frame( FILE *fp );

#define N_CIRCLE_KNOTS	12
fastf_t	circle_knots[N_CIRCLE_KNOTS] = {
	0,	0,	0,
	1,	1,
	2,	2,
	3,	3,
	4,	4,	4
};

#define IRT2	0.70710678	/* 1/sqrt(2) */
#define NCOLS	9
/* When scaling, multiply only XYZ, not W */
fastf_t polyline[NCOLS*4] = {
	0,	1,	0,	1,
	0,	IRT2,	IRT2,	IRT2,
	0,	0,	1,	1,
	0,	-IRT2,	IRT2,	IRT2,
	0,	-1,	0,	1,
	0,	-IRT2,	-IRT2,	IRT2,
	0,	0,	-1,	1,
	0,	IRT2,	-IRT2,	IRT2,
	0,	1,	0,	1
};

/*
 * X displacement table for Kathy's gun tube center of masses, in mm,
 * with X=0 at rear of projectile (on diagram, junction between m1 & m2)
 *  This table lists x positions of centers of mass m1..m12, which
 *  will be used as the end-points of the cylinders
 */
double	dxtab[] = {
	-555,		/* breach rear */
	-280.5,		/* m1 */
	341,
	700+316,
	700+650+246.4,
	700+650+650+301.8,				/* m5 */
	700+650+650+600+238.7,
	700+650+650+600+500+178.9,
	700+650+650+600+500+400+173.4,			/* m8 */
	700+650+650+600+500+400+350+173.3,
	700+650+650+600+500+400+350+350+148.7,
	700+650+650+600+500+400+350+350+300+147.4,
	700+650+650+600+500+400+350+350+300+300+119,	/* m12 */
	700+650+650+600+500+400+350+350+300+300+238,	/* muzzle end */
	0,
};

double	projectile_pos;
point_t	sample[1024];
int	nsamples;

double	iradius, oradius;
double	length;
double	spacing;

int	nframes = 10;
double	delta_t = 0.02;		/* ms/step */
FILE	*pos_fp;
double	cur_time;

struct rt_wdb	*outfp;

int
main(int argc, char **argv)
{
	int	frame;
	char	name[128];
	char	gname[128];
	vect_t	normal;
	struct wmember head, ghead;
	matp_t	matp;
	mat_t	xlate;
	mat_t	rot1, rot2, rot3;
	vect_t	from, to;
	vect_t	offset;

	BU_LIST_INIT( &head.l );
	BU_LIST_INIT( &ghead.l );
	rt_uniresource.re_magic = RESOURCE_MAGIC;

	outfp = wdb_fopen("tube.g");
	if( (pos_fp = fopen( "pos.dat", "r" )) == NULL )
		perror( "pos.dat" );	/* Just warn */

	mk_id( outfp, "Procedural Gun Tube with Projectile" );

	VSET( normal, 0, -1, 0 );
	mk_half( outfp, "cut", normal, 0.0 );
	VSET( normal, 0, 1, 0 );
	mk_half( outfp, "bg.s", normal, -1000.0 );
	(void)mk_addmember( "bg.s", &head.l, NULL, WMOP_UNION );	/* temp use of "head" */
	mk_lcomb( outfp, "bg.r", &head, 1,
		"texture", "file=movie128bw.pix w=128",
		(unsigned char *)0, 0 );

#ifdef never
	/* Numbers for a 105-mm M68 gun */
	oradius = 5 * inches2mm / 2;		/* 5" outer diameter */
	iradius = 4.134 * inches2mm / 2;	/* 5" inner (land) diameter */
#else
	/* Numbers invented to match 125-mm KE (Erline) round */
	iradius = 125.0/2;
	oradius = iradius + (5-4.134) * inches2mm / 2;		/* 5" outer diameter */
#endif
	fprintf(stderr,"inner radius=%gmm, outer radius=%gmm\n", iradius, oradius);

	length = 187 * inches2mm;
#ifdef never
	spacing = 100;			/* mm per sample */
	nsamples = ceil(length/spacing);
	fprintf(stderr,"length=%gmm, spacing=%gmm\n", length, spacing);
	fprintf(stderr,"nframes=%d\n", nframes);
#endif

	for( frame=0;; frame++ )  {
		cur_time = frame * delta_t;
#ifdef never
		/* Generate some dummy sample data */
		if( frame < 16 ) break;
		for( i=0; i<nsamples; i++ )  {
			sample[i][X] = i * spacing;
			sample[i][Y] = 0;
			sample[i][Z] = 4 * oradius * sin(
				((double)i*i)/nsamples * 2 * 3.14159265358979323 +
				frame * 3.141592 * 2 / 8 );
		}
		projectile_pos = ((double)frame)/nframes *
			(sample[nsamples-1][X] - sample[0][X]); /* length */
#else
		if( read_frame(stdin) < 0 )  break;
		if( pos_fp != NULL )  read_pos(pos_fp);
#endif

#define build_spline build_cyl
		sprintf( name, "tube%do", frame);
		build_spline( name, nsamples, oradius );
		(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );

		sprintf( name, "tube%di", frame);
		build_spline( name, nsamples, iradius );
		mk_addmember( name, &head.l, NULL, WMOP_SUBTRACT );

		mk_addmember( "cut", &head.l, NULL, WMOP_SUBTRACT );

		sprintf( name, "tube%d", frame);
		mk_lcomb( outfp, name, &head, 1,
			"plastic", "",
			(unsigned char *)0, 0 );

		/*  Place the tube region and the ammo together.
		 *  The origin of the ammo is expected to be the center
		 *  of the rearmost plate.
		 */
		mk_addmember( name, &ghead.l, NULL, WMOP_UNION );
		matp = mk_addmember( "ke", &ghead.l, NULL, WMOP_UNION )->wm_mat;

		VSET( from, 0, -1, 0 );
		VSET( to, 1, 0, 0 );		/* to X axis */
		bn_mat_fromto( rot1, from, to );

		VSET( from, 1, 0, 0 );
		/* Projectile is 480mm long -- use center pt, not end */
		xfinddir( to, projectile_pos + 480.0/2, offset );
		bn_mat_fromto( rot2, from, to );

		MAT_IDN( xlate );
		MAT_DELTAS( xlate, offset[X], offset[Y], offset[Z] );
		bn_mat_mul( rot3, rot2, rot1 );
		bn_mat_mul( matp, xlate, rot3 );

		(void)mk_addmember( "light.r", &ghead.l, NULL, WMOP_UNION );
		(void)mk_addmember( "bg.r", &ghead.l, NULL, WMOP_UNION );

		sprintf( gname, "g%d", frame);
		mk_lcomb( outfp, gname, &ghead, 0,
			(char *)0, "", (unsigned char *)0, 0 );

		fprintf( stderr, "%d, ", frame );  fflush(stderr);
	}
	wdb_close(outfp);
	fflush(stderr);
	system("cat ke.g");	/* XXX need library routine */
	exit(0);
}
#undef build_spline

void
build_spline(char *name, int npts, double radius)
{
	struct face_g_snurb *bp;
	register int i;
	int	nv;
	int	cur_kv;
	fastf_t	*meshp;
	register int col;
	vect_t	point;

	/*
	 *  This spline will look like a cylinder.
	 *  In the mesh, the circular cross section will be presented
	 *  across the first row by filling in the 9 (NCOLS) columns.
	 *
	 *  The U direction is across the first row,
	 *  and has NCOLS+order[U] positions, 12 in this instance.
	 *  The V direction is down the first column,
	 *  and has NROWS+order[V] positions.
	 */
	bp = rt_nurb_new_snurb( 3,	4,		/* u,v order */
		N_CIRCLE_KNOTS,	npts+6,		/* u,v knot vector size */
		npts+2,		NCOLS,		/* nrows, ncols */
		RT_NURB_MAKE_PT_TYPE(4,2,1),
		&rt_uniresource);

	/*  Build the U knots */
	for( i=0; i<N_CIRCLE_KNOTS; i++ )
		bp->u.knots[i] = circle_knots[i];

	/* Build the V knots */
	cur_kv = 0;		/* current knot value */
	nv = 0;			/* current knot subscript */
	for( i=0; i<4; i++ )
		bp->v.knots[nv++] = cur_kv;
	cur_kv++;
	for( i=4; i<(npts+4-2); i++ )
		bp->v.knots[nv++] = cur_kv++;
	for( i=0; i<4; i++ )
		bp->v.knots[nv++] = cur_kv;

	/*
	 *  The control mesh is stored in row-major order,
	 *  which works out well for us, as a row is one
	 *  circular slice through the tube.  So we just
	 *  have to write down the slices, one after another.
	 *  The first and last "slice" are the center points that
	 *  create the end caps.
	 */
	meshp = bp->ctl_points;

	/* Row 0 */
	for( col=0; col<9; col++ )  {
		*meshp++ = sample[0][X];
		*meshp++ = sample[0][Y];
		*meshp++ = sample[0][Z];
		*meshp++ = 1;
	}

	/* Rows 1..npts */
	for( i=0; i<npts; i++ )  {
		/* row = i; */
		VMOVE( point, sample[i] );
		for( col=0; col<9; col++ )  {
			register fastf_t h;

			h = polyline[col*4+H];
			*meshp++ = polyline[col*4+X]*radius + point[X]*h;
			*meshp++ = polyline[col*4+Y]*radius + point[Y]*h;
			*meshp++ = polyline[col*4+Z]*radius + point[Z]*h;
			*meshp++ = h;
		}
	}

	/* Row npts+1 */
	for( col=0; col<9; col++ )  {
		*meshp++ = sample[npts-1][X];
		*meshp++ = sample[npts-1][Y];
		*meshp++ = sample[npts-1][Z];
		*meshp++ = 1;
	}

	{
		struct face_g_snurb *surfp[2];
		surfp[0] = bp;
		surfp[1] = NULL;
		mk_bspline( outfp, name, surfp );
	}

	rt_nurb_free_snurb( bp, &rt_uniresource );
}

/* Returns -1 if done, 0 if something to draw */
int
read_frame( FILE *fp )
{
	char	buf[256];
	int	i;
	static float	last_read_time = -5;
	double	dx = 0.0;

	if( feof(fp) )
		return(-1);

#ifdef never
	/* Phils format */
	for( nsamples=0;;nsamples++)  {
		if( fgets( buf, sizeof(buf), fp ) == NULL )  return(-1);
		if( buf[0] == '\0' || buf[0] == '\n' )
			/* Blank line, marks break in implicit connection */
			fprintf(stderr,"implicit break unimplemented\n");
			continue;
		}
		if( buf[0] == '=' )  {
			/* End of frame */
			break;
		}
		i = sscanf( buf, "%f %f %f",
			&sample[nsamples][X],
			&sample[nsamples][Y],
			&sample[nsamples][Z] );
		if( i != 3 )  {
			fprintf(stderr, "input line didn't have 3 numbers: %s\n", buf);
			break;
		}
		/* Phil's numbers are in meters, not mm */
		sample[nsamples][X] *= 1000;
		sample[nsamples][Y] *= 1000;
		sample[nsamples][Z] *= 1000;
	}
#else
	/* Kurt's / Kathy's format, in inches */
	if( cur_time <= 0 )  {
		/* Really should use Y and Z initial conditions, too */
		for( nsamples=0; nsamples < (sizeof(dxtab)/sizeof(dxtab[0])); nsamples++ )  {
			sample[nsamples][X] = dxtab[nsamples];
			sample[nsamples][Y] = sample[nsamples][Z] = 0;
		}
		return(0);		/* OK */
	}
	if( last_read_time > cur_time )
		return(0);		/* OK, reuse last step's data */
	/* Ferret out next time marker */
	while(1)  {
		if( fgets( buf, sizeof(buf), fp ) == NULL )  {
			fprintf(stderr,"EOF?\n");
			return(-1);
		}
		if( strncmp(buf, "TIME", strlen("TIME")) != 0 )  continue;
		if( sscanf(buf, "TIME %f", &last_read_time ) < 1 )  {
			fprintf(stderr, "bad TIME\n");
			return(-1);
		}
		break;
	}

	for( nsamples=0;;nsamples++)  {
		int	nmass;
		float	kx, ky, kz;

		buf[0] = '\0';
		if( fgets( buf, sizeof(buf), fp ) == NULL )  return(-1);
		/* center of mass #, +X, +Z, -Y (chg of coordinates) */
		if( buf[0] == '\0' || buf[0] == '\n' )
			break;		/* stop at a blank line */
		i = sscanf( buf, "%d %f %f %f",
			&nmass, &kx, &ky, &kz );
		if( i != 4 )  {
			fprintf( stderr, "input line in error: %s\n", buf );
			return(-1);
		}
		if( nmass-1 != nsamples )  {
			fprintf( stderr, "nmass %d / nsamples %d mismatch\n",
				nmass, nsamples );
			return(-1);
		}
#define EXAGERATION	(4 * oradius)
		/* scale = EXAGERATIONmm / MAX_DEVIATIONmm */
		/* Deviations used here manually derived */
		dx = kx * inches2mm * EXAGERATION / (0.95 * inches2mm);
		sample[nsamples][X] = dx + dxtab[nsamples];
		sample[nsamples][Y] = kz * inches2mm *
			EXAGERATION / (0.00002 * inches2mm) /5;
		sample[nsamples][Z] = -ky * inches2mm *
			EXAGERATION / (0.02 * inches2mm);
	}
	/* Extrapolate data for the right side -- end of muzzle */
	sample[nsamples][X] = dxtab[nsamples] + dx;	/* reuse last displacement */
	sample[nsamples][Y] = sample[nsamples-1][Y] * 2 - sample[nsamples-2][Y];
	sample[nsamples][Z] = sample[nsamples-1][Z] * 2 - sample[nsamples-2][Z];
	nsamples++;
#endif
	if( nsamples <= 4 )  {
		fprintf(stderr,"insufficient samples\n");
		return(-1);
	}
	return(0);			/* OK */
}

void
read_pos(FILE *fp)
{
	static float	last_read_time = -5;
	static float	pos = 0;

	/* Skip over needless intermediate time steps */
	while( last_read_time < cur_time )  {
		if( feof(fp) )
			break;
		fscanf( fp, "%f %f", &last_read_time, &pos );
		/* HACK:  tmax[kathy]=6.155ms, tmax[kurt]=9.17 */
		/* we just read a Kurt number, make it a Kathy number */
		last_read_time = last_read_time / 9.17 * 6.155;
	}

	/* Kurt's data is in inches */
	projectile_pos = pos * inches2mm;
}

void
build_cyl(char *cname, int npts, double radius)
{
	register int i;
	vect_t	v, h, a, b;
	char	name[32];
	struct wmember head;

	BU_LIST_INIT( &head.l );

	for( i=0; i<npts-1; i++ )  {
		VMOVE( v, sample[i] );
		VSUB2( h, sample[i+1], v );
		VSET( a, 0, radius, 0 );
		VSET( b, 0, 0, radius );

		sprintf( name, "%s%d", cname, i );
		mk_tgc( outfp, name, v, h, a, b, a, b );
		(void)mk_addmember( name, &head.l, NULL, WMOP_UNION );
	}
	mk_lfcomb( outfp, cname, &head, 0 );
}

/*
 * Find which section a given X value is in, and indicate what
 * direction the tube is headed in then.
 */
void
xfinddir(fastf_t *dir, double x, fastf_t *loc)
{
	register int i;
	fastf_t	ratio;

	for( i=0; i<nsamples-1; i++ )  {
		if( x < sample[i][X] )
			break;
		if( x >= sample[i+1][X] )
			continue;
		goto out;
	}
	fprintf(stderr, "xfinddir: x=%g is past last segment, using final direction\n", x);
	i = nsamples-2;
out:
	VSUB2( dir, sample[i+1], sample[i] );
	ratio = (x-sample[i][X]) / (sample[i+1][X]-sample[i][X]);
	VJOIN1( loc, sample[i], ratio, dir );

	VUNITIZE( dir );
	return;
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
