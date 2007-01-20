/*                         P L R O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @file plrot.c
 *
 *  Rotate, Translate, and Scale a Unixplot file.
 *
 *  Authors -
 *	Phillip Dykstra
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

#include <stdlib.h> /* atof() */
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "plot3.h"
#include "bn.h"

#define	UPPER_CASE(c)	((c)-32)
#define	COPY(n)	{fread(cbuf,1,n,fp); fwrite(cbuf,1,n,stdout);}
#define	SKIP(n)	{fread(cbuf,1,n,fp);}
#define LEN 265

#define	putsi(s) {putchar(s); putchar((s)>>8);}

double	getdouble(FILE *fp);

char	cbuf[48];		/* COPY and SKIP macro buffer */

mat_t	rmat;			/* rotation matrix to be applied */
double	scale;			/* scale factor to be applied */

int	verbose;

int	space_set;		/* current space has been explicitly set */
point_t	space_min, space_max;	/* Current space */

int	forced_space;		/* space is being forced */
point_t	forced_space_min, forced_space_max;	/* forced space */

int	rpp;			/* flag: compute new center values */
int	Mflag;			/* flag: don't rebound space rpp */

void	dofile(FILE *fp);
void	copy_string(FILE *fp);
void	two_coord_out(FILE *fp, fastf_t *m);
void	three_coord_out(FILE *fp, fastf_t *m);
void	two_dcoord_out(FILE *fp, fastf_t *m);
void	three_dcoord_out(FILE *fp, fastf_t *m);

static char usage[] = "\
Usage: plrot [options] [file1 ... fileN] > file.plot\n\
   -x# -y# -z#    Rotation about axis in degrees\n\
   -X# -Y# -Z#    Translation along axis\n\
   -s#            Scale factor\n\
   -a# -e#        Azimuth/Elevation from front view\n\
                  (usually first, in this order, implies -M)\n\
   -g             MGED front view to display coordinates (usually last)\n\
   -M             Autoscale space command like RT model RPP\n\
   -m#            Takes a 4X4 matrix as an argument\n\
   -v             Verbose\n\
   -S#            Space: takes a quoted string of six floats\n";

/*
 *			M O D E L _ R P P
 *
 *  Process a space command.
 *  Behavior depends on setting of several flags.
 *
 *  Implicit Returns -
 *	In all cases, sets space_min and space_max.
 */
int
model_rpp(const fastf_t *min, const fastf_t *max)
{

	if( space_set )  {
		fprintf(stderr, "plrot:  additional SPACE command ignored\n");
		fprintf(stderr, "got: space (%g, %g, %g) (%g, %g, %g)\n",
			V3ARGS(min), V3ARGS(max) );
		fprintf(stderr, "still using: space (%g, %g, %g) (%g, %g, %g)\n",
			V3ARGS(space_min), V3ARGS(space_max) );
		return 0;
	}

	if( rpp )  {
		point_t	rot_center;		/* center of rotation */
		mat_t	xlate;
		mat_t	resize;
		mat_t	t1, t2;

		VADD2SCALE( rot_center, min, max, 0.5 );

		/* Create the matrix which encodes this */
		MAT_IDN( xlate );
		MAT_DELTAS( xlate, -rot_center[X], -rot_center[Y], -rot_center[Z] );
		MAT_IDN( resize );
		resize[15] = 1/scale;
		bn_mat_mul( t1, resize, xlate );
		bn_mat_mul( t2, rmat, t1 );
		MAT_COPY( rmat, t2 );
		if( verbose )  {
			bn_mat_print("rmat", rmat);
		}

		if( Mflag )  {
			/*  Don't rebound, just expand size of space
			 *  around center point.
			 *  Has advantage of the output space() not being
			 *  affected by changes in rotation,
			 *  which may be significant for animation scripts.
			 */
			vect_t	diag;
			double	v;

			VSUB2( diag, max, min );
			v = MAGNITUDE(diag)*0.5 + 0.5;
			VSET( space_min, -v, -v, -v );
			VSET( space_max,  v,  v,  v );
		} else {
			/* re-bound the space() rpp with a tighter one
			 * after rotating & scaling it.
			 */
			bn_rotate_bbox( space_min, space_max, rmat, min, max );
		}
		space_set = 1;
	} else {
		VMOVE( space_min, min );
		VMOVE( space_max, max );
		space_set = 1;
	}

	if( forced_space )  {
		/* Put forced space back */
		VMOVE( space_min, forced_space_min );
		VMOVE( space_max, forced_space_max );
		space_set = 1;
	}

	if( verbose )  {
		fprintf(stderr, "got: space (%g, %g, %g) (%g, %g, %g)\n",
			V3ARGS(min), V3ARGS(max) );
		fprintf(stderr, "put: space (%g, %g, %g) (%g, %g, %g)\n",
			V3ARGS(space_min), V3ARGS(space_max) );
	}

	return( 1 );
}


int
getshort(FILE *fp)
{
	register long	v, w;

	v = getc(fp);
	v |= (getc(fp)<<8);	/* order is important! */

	/* worry about sign extension - sigh */
	if( v <= 0x7FFF )  return(v);
	w = -1;
	w &= ~0x7FFF;
	return( w | v );
}



int
get_args(int argc, register char **argv)
{
	register int c;
	mat_t	tmp, m;
	int	i, num;
	double	mtmp[16];

	MAT_IDN( rmat );
	scale = 1.0;

	while ( (c = bu_getopt( argc, argv, "S:m:vMga:e:x:y:z:X:Y:Z:s:" )) != EOF )  {
		switch( c )  {
		case 'M':
			/* take model RPP from space() command */
			rpp++;
			Mflag = 1;		/* don't rebound */
			break;
		case 'g':
			bn_mat_angles( tmp, -90.0, 0.0, -90.0 );
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 'a':
			bn_mat_angles( tmp, 0.0, 0.0, -atof(bu_optarg) );
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			rpp++;
			break;
		case 'e':
			bn_mat_angles( tmp, 0.0, -atof(bu_optarg), 0.0 );
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			rpp++;
			break;
		case 'x':
			bn_mat_angles( tmp, atof(bu_optarg), 0.0, 0.0 );
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 'y':
			bn_mat_angles( tmp, 0.0, atof(bu_optarg), 0.0 );
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 'z':
			bn_mat_angles( tmp, 0.0, 0.0, atof(bu_optarg) );
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 'm':
			num = sscanf(&bu_optarg[0], "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
				&mtmp[0], &mtmp[1], &mtmp[2], &mtmp[3],
				&mtmp[4], &mtmp[5], &mtmp[6], &mtmp[7],
				&mtmp[8], &mtmp[9], &mtmp[10], &mtmp[11],
				&mtmp[12], &mtmp[13], &mtmp[14], &mtmp[15]);

			if( num != 16)  {
				fprintf(stderr, "Num of arguments to -m only %d, should be 16\n", num);
				exit(1);
			}

			/* Now copy the array of doubles into mat. */
			for(i=0; i < 16; i++ )  {
				rmat[i] = mtmp[i];
			}
			break;
		case 'X':
			MAT_IDN( tmp );
			tmp[MDX] = atof(bu_optarg);
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 'Y':
			MAT_IDN( tmp );
			tmp[MDY] = atof(bu_optarg);
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 'Z':
			MAT_IDN( tmp );
			tmp[MDZ] = atof(bu_optarg);
			MAT_COPY( m, rmat );
			bn_mat_mul( rmat, tmp, m );
			break;
		case 's':
			scale *= atof(bu_optarg);
			/*
			 *  If rpp flag has already been set, defer
			 *  application of scale until after the
			 *  xlate to space-center, in model_rpp().
			 *  Otherwise, do it here, in the sequence
			 *  seen in the arg list.
			 */
			if( !rpp )  {
				MAT_IDN( tmp );
				tmp[15] = 1/scale;
				MAT_COPY( m, rmat );
				bn_mat_mul( rmat, tmp, m );
				scale = 1.0;
			}
			break;
		case 'v':
			verbose++;
			break;
		case 'S':
			num = sscanf(bu_optarg, "%lf %lf %lf %lf %lf %lf",
				&mtmp[0], &mtmp[1], &mtmp[2],
				&mtmp[3], &mtmp[4], &mtmp[5]);
			VSET(forced_space_min, mtmp[0], mtmp[1], mtmp[2]);
			VSET(forced_space_max, mtmp[3], mtmp[4], mtmp[5]);

			/* Write it now, in case input does not have one */
			pdv_3space( stdout, forced_space_min, forced_space_max );
			forced_space = 1;
			break;
		default:		/* '?' */
			return(0);	/* Bad */
		}
	}


	if( isatty(fileno(stdout))
	  || (isatty(fileno(stdin)) && (bu_optind >= argc)) )  {
		return(0);	/* Bad */
	}

	return(1);		/* OK */
}


/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	FILE	*fp=NULL;

	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1);
	}

	if( verbose )  {
		bn_mat_print("rmat", rmat);
	}

	if( bu_optind < argc ) {
		while( bu_optind < argc ) {
			if( fp != NULL && fp != stdin )
				fclose( fp );
			if( strcmp(argv[bu_optind], "-") == 0 )
				fp = stdin;
			else if( (fp = fopen(argv[bu_optind],"r")) == NULL ) {
				fprintf( stderr, "plrot: can't open \"%s\"\n", argv[bu_optind] );
				continue;
			}
			dofile( fp );
			bu_optind++;
		}
	} else {
		dofile( stdin );
	}
	return 0;
}

/*
 *			D O F I L E
 */
void
dofile(FILE *fp)
{
	register int	c;

	while( (c = getc(fp)) != EOF ) {
		switch( c ) {
		/* One of a kind functions */
		case 'e':
		case 'F':
			putchar( c );
			break;
		case 'f':
		case 't':
			putchar( c );
			copy_string( fp );
			break;
		case 'C':
			putchar( c );
			COPY(3);
			break;
		case 'c':	/* map x, y? */
			putchar( c );
			COPY(6);
			break;
		case 'a':	/* map points? */
			putchar( c );
			COPY(12);
			break;
		case 'p':
		case 'm':
		case 'n':
			/* Two coordinates in, three out. Change command. */
			putchar( UPPER_CASE(c) );
			two_coord_out( fp, rmat );
			break;
		case 'l':
			putchar( 'L' );
			two_coord_out( fp, rmat );
			two_coord_out( fp, rmat );
			break;
		case 'P':
		case 'M':
		case 'N':
			putchar( c );
			three_coord_out( fp, rmat );
			break;
		case 'L':
			putchar( c );
			three_coord_out( fp, rmat );
			three_coord_out( fp, rmat );
			break;
		case 's':
			{
				/*  2-D integer SPACE command.
				 *  This is the only AT&T compatible space
				 *  command;  be certain to only output
				 *  with pl_space(), to ensure that output
				 *  file is AT&T style if input was.
				 */
				long	minx, miny, maxx, maxy;
				point_t	min, max;

				minx = getshort(fp);
				miny = getshort(fp);
				maxx = getshort(fp);
				maxy = getshort(fp);

				VSET( min, minx, miny, -1 );
				VSET( max, maxx, maxy, -1 );
				model_rpp( min, max );

				minx = (long)floor( space_min[X] );
				miny = (long)floor( space_min[Y] );
				maxx = (long)ceil( space_max[X] );
				maxy = (long)ceil( space_max[Y] );
				if( minx < -32768 )  minx = -32768;
				if( miny < -32768 )  miny = -32768;
				if( maxx > 32767 )  maxx = 32767;
				if( maxy > 32767 )  maxy = 32767;

				pl_space( stdout, minx, miny, maxx, maxy );
			}
			break;
		case 'S':
			{
				/* BRL-extended 3-D integer SPACE command */
				point_t	min, max;

				min[X] = getshort(fp);
				min[Y] = getshort(fp);
				min[Z] = getshort(fp);
				max[X] = getshort(fp);
				max[Y] = getshort(fp);
				max[Z] = getshort(fp);

				model_rpp( min, max );

				pdv_3space( stdout, space_min, space_max );
			}
			break;
		/* 2D and 3D IEEE */
		case 'w':
			{
				/* BRL 2-D floating point SPACE command */
				point_t	min, max;

				min[X] = getdouble(fp);
				min[Y] = getdouble(fp);
				min[Z] = -1.0;
				max[X] = getdouble(fp);
				max[Y] = getdouble(fp);
				max[Z] = 1.0;

				model_rpp( min, max );

				pdv_3space( stdout, space_min, space_max );
			}
			break;
		case 'W':
			{
				/* BRL 3-D floating point SPACE command */
				point_t	min, max;

				min[X] = getdouble(fp);
				min[Y] = getdouble(fp);
				min[Z] = getdouble(fp);
				max[X] = getdouble(fp);
				max[Y] = getdouble(fp);
				max[Z] = getdouble(fp);

				model_rpp( min, max );

				pdv_3space( stdout, space_min, space_max );
			}
			break;
		case 'i':
			putchar(c);
			COPY(3*8);
			break;
		case 'r':
			putchar(c);
			COPY(6*8);
			break;
		case 'x':
		case 'o':
		case 'q':
			/* Two coordinates in, three out.  Change command. */
			putchar( UPPER_CASE(c) );
			two_dcoord_out( fp, rmat );
			break;
		case 'v':
			/* Two coordinates in, three out.  Change command. */
			putchar( 'V' );
			two_dcoord_out( fp, rmat );
			two_dcoord_out( fp, rmat );
			break;
		case 'X':
		case 'O':
		case 'Q':
			putchar( c );
			three_dcoord_out( fp, rmat );
			break;
		case 'V':
			putchar( c );
			three_dcoord_out( fp, rmat );
			three_dcoord_out( fp, rmat );
			break;
		default:
			fprintf( stderr, "plrot: unrecognized command '%c' (0x%x)\n",
				(isascii(c) && isprint(c)) ? c : '?',
				c );
			fprintf( stderr, "plrot: ftell = %ld\n", ftell(fp) );
			putchar( c );
			break;
		}
	}
}

/*
 *			C O P Y _ S T R I N G
 */
void
copy_string(FILE *fp)
{
	int	c;

	while( (c = putchar(getc(fp))) != '\n' && c != EOF )
		;
}


/******* Coordinate Transforms / Output *******/

void
two_coord_out(FILE *fp, fastf_t *m)
{
	point_t	p1;
	short	p2[3];

	p1[0] = getshort(fp);	/* get X */
	p1[1] = getshort(fp);	/* and Y */
	p1[2] = 0;		/* no Z  */

	MAT4X3PNT( p2, m, p1 );

	putsi(p2[0]);		/* put X */
	putsi(p2[1]);		/* and Y */
	putsi(p2[2]);		/* and Z */
}

void
three_coord_out(FILE *fp, fastf_t *m)
{
	point_t	p1;
	short	p2[3];

	p1[0] = getshort(fp);	/* get X */
	p1[1] = getshort(fp);	/* and Y */
	p1[2] = getshort(fp);	/* and Z */

	MAT4X3PNT( p2, m, p1 );

	putsi(p2[0]);		/* put X */
	putsi(p2[1]);		/* and Y */
	putsi(p2[2]);		/* and Z */
}

void
two_dcoord_out(FILE *fp, fastf_t *m)
{
	unsigned char	buf[2*8];
	double	p1[3];
	double	p2[3];

	fread( buf, 1, 2*8, fp );
	ntohd( (unsigned char *)p1, buf, 2 );
	p1[2] = 0;		/* no Z  */

	MAT4X3PNT( p2, m, p1 );

	htond( buf, (unsigned char *)p2, 3 );
	fwrite( buf, 1, 3*8, stdout );
}

void
three_dcoord_out(FILE *fp, fastf_t *m)
{
	unsigned char	buf[3*8];
	double	p1[3];
	double	p2[3];

	fread( buf, 1, 3*8, fp );
	ntohd( (unsigned char *)p1, buf, 3 );

	MAT4X3PNT( p2, m, p1 );

	htond( buf, (unsigned char *)p2, 3 );
	fwrite( buf, 1, 3*8, stdout );
}


double
getdouble(FILE *fp)
{
	double	d;
	unsigned char	buf[8];
	fread( buf, 8, 1, fp );
	ntohd( (unsigned char *)&d, buf, 1 );
	return( d );
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
