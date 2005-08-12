/*                           M A T . C
 * BRL-CAD
 *
 * Copyright (C) 1996-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbn */
/*@{*/

/** @file mat.c
 * 4 x 4 Matrix manipulation functions...
 *
 *  - bn_atan2()			Wrapper for library atan2()
 *  - (deprecated) bn_mat_zero( &m )		Fill matrix m with zeros
 *  - (deprecated) bn_mat_idn( &m )		Fill matrix m with identity matrix
 *  - (deprecated) bn_mat_copy( &o, &i )		Copy matrix i to matrix o
 *  - bn_mat_mul( &o, &i1, &i2 )	Multiply i1 by i2 and store in o
 *	- bn_mat_mul2( &i, &o )
 *	- bn_matXvec( &ov, &m, &iv )	Multiply m by vector iv, store in ov
 *	- bn_mat_inv( &om, &im )		Invert matrix im, store result in om
 *	- bn_mat_print( &title, &m )	Print matrix (with title) on stderr.
 *	- bn_mat_trn( &o, &i )		Transpose matrix i into matrix o
 *	- bn_mat_ae( &o, azimuth, elev)	Make rot matrix from azimuth+elevation
 *	- bn_ae_vec( &az, &el, v )	Find az/elev from dir vector
 *	- bn_aet_vec( &az, &el, &twist, v1, v2 ) Find az,el,twist from two vectors
 *	- bn_mat_angles( &o, alpha, beta, gama )	Make rot matrix from angles
 *	- bn_eigen2x2()			Eigen values and vectors
 *	- bn_mat_lookat			Make rot mat:  xform from D to -Z
 *	- bn_mat_fromto			Make rot mat:  xform from A to
 *	- bn_mat_arb_rot( &m, pt, dir, ang)	Make rot mat about axis (pt,dir), through ang
 *	- bn_mat_is_equal()		Is mat a equal to mat b?
 *
 *
 * Matrix array elements have the following positions in the matrix:
 *
 *				|  0  1  2  3 |		| 0 |
 *	  [ 0 1 2 3 ]		|  4  5  6  7 |		| 1 |
 *				|  8  9 10 11 |		| 2 |
 *				| 12 13 14 15 |		| 3 |
 *
 *
 *     preVector (vect_t)	 Matrix (mat_t)    postVector (vect_t)
 *
 *  Authors -
 *	Robert S. Miles
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

/*@}*/

#ifndef lint
static const char bn_RCSmat[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif 

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

const mat_t	bn_mat_identity = {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
};

void
bn_mat_print_guts(const char	*title,
		  const mat_t	m,
		  char		*obuf)
{
	register int	i;
	register char	*cp;

	sprintf(obuf, "MATRIX %s:\n  ", title);
	cp = obuf+strlen(obuf);
	if (!m) {
		strcat(obuf, "(Identity)");
	} else {
		for (i=0; i<16; i++)  {
			sprintf(cp, " %8.3f", m[i]);
			cp += strlen(cp);
			if (i == 15) {
				break;
			} else if((i&3) == 3) {
				*cp++ = '\n';
				*cp++ = ' ';
				*cp++ = ' ';
			}
		}
		*cp++ = '\0';
	}
}

/*
 *			B N _ M A T _ P R I N T
 */
void
bn_mat_print(const char		*title,
	     const mat_t	m)
{
	char		obuf[1024];	/* sprintf may be non-PARALLEL */

	bn_mat_print_guts(title, m, obuf);
	bu_log("%s\n", obuf);
}

/*
 *			B N _ A T A N 2
 *
 *  A wrapper for the system atan2().  On the Silicon Graphics,
 *  and perhaps on others, x==0 incorrectly returns infinity.
 */
double
bn_atan2(double y, double x)
{
	if( x > -1.0e-20 && x < 1.0e-20 )  {
		/* X is equal to zero, check Y */
		if( y < -1.0e-20 )  return( -3.14159265358979323/2 );
		if( y >  1.0e-20 )  return(  3.14159265358979323/2 );
		return(0.0);
	}
	return( atan2( y, x ) );
}

#if 0  /********* Deprecated for macros that call memcpy() *********/

/*
 *			B N _ M A T _ Z E R O
 *
 * Fill in the matrix "m" with zeros.
 */
void
bn_mat_zero( m )
mat_t	m;
{
	register int i = 0;
	register matp_t mp = m;

	bu_log("libbn/mat.c:  bn_mat_zero() is deprecated, use MAT_ZERO()\n");
	/* Clear everything */
	for(; i<16; i++)
		*mp++ = 0.0;
}



/*
 *			B N _ M A T _ I D N
 *
 * Fill in the matrix "m" with an identity matrix.
 */
void
bn_mat_idn( m )
register mat_t	m;
{
	bu_log("libbn/mat.c:  bn_mat_idn() is deprecated, use MAT_IDN()\n");
	memcpy(m, bn_mat_identity, sizeof(m));
}

/*
 *			B N _ M A T _ C O P Y
 * Copy the matrix
 */
void
bn_mat_copy( dest, src )
register mat_t		dest;
register const mat_t	src;
{
	register int i;

	/* Copy all elements */
#	include "noalias.h"
	bu_log("libbn/mat.c:  bn_mat_copy() is deprecated, use MAT_COPY()\n");
	for( i=15; i>=0; i--)
		dest[i] = src[i];
}

#endif	/******************* deprecated *******************/

/*
 *			B N _ M A T _ M U L
 *
 * Multiply matrix "a" by "b" and store the result in "o".
 * NOTE:  This is different from multiplying "b" by "a"
 * (most of the time!)
 * NOTE: "o" must not be the same as either of the inputs.
 */
void
bn_mat_mul(register mat_t o, register const mat_t a, register const mat_t b)
{
	o[ 0] = a[ 0]*b[ 0] + a[ 1]*b[ 4] + a[ 2]*b[ 8] + a[ 3]*b[12];
	o[ 1] = a[ 0]*b[ 1] + a[ 1]*b[ 5] + a[ 2]*b[ 9] + a[ 3]*b[13];
	o[ 2] = a[ 0]*b[ 2] + a[ 1]*b[ 6] + a[ 2]*b[10] + a[ 3]*b[14];
	o[ 3] = a[ 0]*b[ 3] + a[ 1]*b[ 7] + a[ 2]*b[11] + a[ 3]*b[15];

	o[ 4] = a[ 4]*b[ 0] + a[ 5]*b[ 4] + a[ 6]*b[ 8] + a[ 7]*b[12];
	o[ 5] = a[ 4]*b[ 1] + a[ 5]*b[ 5] + a[ 6]*b[ 9] + a[ 7]*b[13];
	o[ 6] = a[ 4]*b[ 2] + a[ 5]*b[ 6] + a[ 6]*b[10] + a[ 7]*b[14];
	o[ 7] = a[ 4]*b[ 3] + a[ 5]*b[ 7] + a[ 6]*b[11] + a[ 7]*b[15];

	o[ 8] = a[ 8]*b[ 0] + a[ 9]*b[ 4] + a[10]*b[ 8] + a[11]*b[12];
	o[ 9] = a[ 8]*b[ 1] + a[ 9]*b[ 5] + a[10]*b[ 9] + a[11]*b[13];
	o[10] = a[ 8]*b[ 2] + a[ 9]*b[ 6] + a[10]*b[10] + a[11]*b[14];
	o[11] = a[ 8]*b[ 3] + a[ 9]*b[ 7] + a[10]*b[11] + a[11]*b[15];

	o[12] = a[12]*b[ 0] + a[13]*b[ 4] + a[14]*b[ 8] + a[15]*b[12];
	o[13] = a[12]*b[ 1] + a[13]*b[ 5] + a[14]*b[ 9] + a[15]*b[13];
	o[14] = a[12]*b[ 2] + a[13]*b[ 6] + a[14]*b[10] + a[15]*b[14];
	o[15] = a[12]*b[ 3] + a[13]*b[ 7] + a[14]*b[11] + a[15]*b[15];
}

/*
 *			B N _ M A T _ M U L 2
 *
 *  o = i * o
 *
 *  A convenience wrapper for bn_mat_mul() to update a matrix in place.
 *  The arugment ordering is confusing either way.
 */
void
bn_mat_mul2(register const mat_t i, register mat_t o)
{
	mat_t	temp;

	bn_mat_mul( temp, i, o );
	MAT_COPY( o, temp );
}

/*
 *			B N _ M A T _ M U L 3
 *
 *  o = a * b * c
 *
 *  The output matrix may be the same as 'b' or 'c', but may not be 'a'.
 */
void
bn_mat_mul3(mat_t o, const mat_t a, const mat_t b, const mat_t c)
{
	mat_t	t;

	bn_mat_mul( t, b, c );
	bn_mat_mul( o, a, t );
}

/*
 *			B N _ M A T _ M U L 4
 *
 *  o = a * b * c * d
 *
 *  The output matrix may be the same as any input matrix.
 */
void
bn_mat_mul4(mat_t ao, const mat_t a, const mat_t b, const mat_t c, const mat_t d)
{
	mat_t	t, u;

	bn_mat_mul( u, c, d );
	bn_mat_mul( t, a, b );
	bn_mat_mul( ao, t, u );
}

/*
 *			B N _ M A T X V E C
 *
 * Multiply the matrix "im" by the vector "iv" and store the result
 * in the vector "ov".  Note this is post-multiply, and
 * operates on 4-tuples.  Use MAT4X3VEC() to operate on 3-tuples.
 */
void
bn_matXvec(register vect_t ov, register const mat_t im, register const vect_t iv)
{
	register int eo = 0;		/* Position in output vector */
	register int em = 0;		/* Position in input matrix */
	register int ei;		/* Position in input vector */

	/* For each element in the output array... */
	for(; eo<4; eo++) {

		ov[eo] = 0;		/* Start with zero in output */

		for(ei=0; ei<4; ei++)
			ov[eo] += im[em++] * iv[ei];
	}
}


/*
 *			B N _ M A T _ I N V
 *
 * The matrix pointed at by "im" is inverted and stored in the area
 * pointed at by "om".
 */
/* 
 * Invert a 4-by-4 matrix using Algorithm 120 from ACM.
 * This is a modified Gauss-Jordan alogorithm
 * Note:  Inversion is done in place, with 3 work vectors
 */
void
bn_mat_inv(register mat_t output, const mat_t input)
{
	register int i, j;			/* Indices */
	LOCAL int k;				/* Indices */
	LOCAL int	z[4];			/* Temporary */
	LOCAL fastf_t	b[4];			/* Temporary */
	LOCAL fastf_t	c[4];			/* Temporary */
	
	MAT_COPY( output, input );	/* Duplicate */

        /* Initialization */
	for( j = 0; j < 4; j++ )
		z[j] = j;

	/* Main Loop */
	for( i = 0; i < 4; i++ )  {
		FAST fastf_t y;				/* local temporary */

		k = i;
		y = output[i*4+i];
		for( j = i+1; j < 4; j++ )  {
			FAST fastf_t w;			/* local temporary */

			w = output[i*4+j];
			if( fabs(w) > fabs(y) )  {
				k = j;
				y = w;
			}
		}

		if( fabs(y) < SQRT_SMALL_FASTF )  {
			bu_log("bn_mat_inv:  error! fabs(y)=%g\n", fabs(y));
			bn_mat_print("singular matrix", input);
			bu_bomb("bn_mat_inv: singular matrix\n");
			/* NOTREACHED */
		}
		y = 1.0 / y;

		for( j = 0; j < 4; j++ )  {
			FAST fastf_t temp;		/* Local */

			c[j] = output[j*4+k];
			output[j*4+k] = output[j*4+i];
			output[j*4+i] = - c[j] * y;
			temp = output[i*4+j] * y;
			b[j] = temp;
			output[i*4+j] = temp;
		}

		output[i*4+i] = y;
		j = z[i];
		z[i] = z[k];
		z[k] = j;
		for( k = 0; k < 4; k++ )  {
			if( k == i )  continue;
			for( j = 0; j < 4; j++ )  {
				if( j == i )  continue;
				output[k*4+j] = output[k*4+j] - b[j] * c[k];
			}
		}
	}

	/*  Second Loop */
	for( i = 0; i < 4; i++ )  {
		while( (k = z[i]) != i )  {
			LOCAL int p;			/* Local temp */

			for( j = 0; j < 4; j++ )  {
				FAST fastf_t w;		/* Local temp */

				w = output[i*4+j];
				output[i*4+j] = output[k*4+j];
				output[k*4+j] = w;
			}
			p = z[i];
			z[i] = z[k];
			z[k] = p;
		}
	}
}

/*
 *			B N _ V T O H _ M O V E
 *
 * Takes a pointer to a [x,y,z] vector, and a pointer
 * to space for a homogeneous vector [x,y,z,w],
 * and builds [x,y,z,1].
void
bn_vtoh_move(register vert_t h2, register const vert_t v2)
{
	h2[X] = v2[X];
	h2[Y] = v2[Y];
	h2[Z] = v2[Z];
	h2[W] = 1.0;
}
 */

/*
 *			B N _ H T O V _ M O V E
 *
 * Takes a pointer to [x,y,z,w], and converts it to
 * an ordinary vector [x/w, y/w, z/w].
 * Optimization for the case of w==1 is performed.
 */
void
bn_htov_move(register vect_t v, register const vect_t h)
{
	register fastf_t inv;

	if( h[3] == 1.0 )  {
		v[X] = h[X];
		v[Y] = h[Y];
		v[Z] = h[Z];
	}  else  {
		if( h[W] == SMALL_FASTF )  {
			bu_log("bn_htov_move: divide by %f!\n", h[W]);
			return;
		}
		inv = 1.0 / h[W];
		v[X] = h[X] * inv;
		v[Y] = h[Y] * inv;
		v[Z] = h[Z] * inv;
	}
}


/*
 *			B N _ M A T _ T R N
 */
void
bn_mat_trn(mat_t om, register const mat_t im)
{
	register matp_t op = om;

	*op++ = im[0];
	*op++ = im[4];
	*op++ = im[8];
	*op++ = im[12];

	*op++ = im[1];
	*op++ = im[5];
	*op++ = im[9];
	*op++ = im[13];

	*op++ = im[2];
	*op++ = im[6];
	*op++ = im[10];
	*op++ = im[14];

	*op++ = im[3];
	*op++ = im[7];
	*op++ = im[11];
	*op++ = im[15];
}

/*
 *			B N _ M A T _ A E
 *
 *  Compute a 4x4 rotation matrix given Azimuth and Elevation.
 *  
 *  Azimuth is +X, Elevation is +Z, both in degrees.
 *
 *  Formula due to Doug Gwyn, BRL.
 */
void
bn_mat_ae(register fastf_t *m, double azimuth, double elev)
{
	LOCAL double sin_az, sin_el;
	LOCAL double cos_az, cos_el;

	azimuth *= bn_degtorad;
	elev *= bn_degtorad;

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

/*
 *			B N _ A E _ V E C
 *
 *  Find the azimuth and elevation angles that correspond to the
 *  direction (not including twist) given by a direction vector.
 */
void
bn_ae_vec(fastf_t *azp, fastf_t *elp, const vect_t v)
{
	register fastf_t	az;

	if( (az = bn_atan2( v[Y], v[X] ) * bn_radtodeg) < 0 )  {
		*azp = 360 + az;
	} else if( az >= 360 ) {
		*azp = az - 360;
	} else {
		*azp = az;
	}
	*elp = bn_atan2( v[Z], hypot( v[X], v[Y] ) ) * bn_radtodeg;
}

/*			B N _ A E T _ V E C
 *
 * Find the azimuth, elevation, and twist from two vectors.
 * Vec_ae is in the direction of view (+z in mged view)
 * and vec_twist points to the viewers right (+x in mged view).
 * Accuracy (degrees) is used to stabilze flutter between
 * equivalent extremes of atan2(), and to snap twist to zero
 * when elevation is near +/- 90
 */
void
bn_aet_vec(fastf_t *az, fastf_t *el, fastf_t *twist, fastf_t *vec_ae, fastf_t *vec_twist, fastf_t accuracy)
{
	vect_t zero_twist,ninety_twist;
	vect_t z_dir;

	/* Get az and el as usual */
	bn_ae_vec( az , el , vec_ae );

	/* stabilize fluctuation bewteen 0 and 360
	 * change azimuth near 360 to 0 */
	if( NEAR_ZERO( *az - 360.0 , accuracy ) )
		*az = 0.0;

	/* if elevation is +/-90 set twist to zero and calculate azimuth */
	if( NEAR_ZERO( *el - 90.0 , accuracy ) || NEAR_ZERO( *el + 90.0 , accuracy ) )
	{
		*twist = 0.0;
		*az = bn_atan2( -vec_twist[X] , vec_twist[Y] ) * bn_radtodeg;
	}
	else
	{
		/* Calculate twist from vec_twist */
		VSET( z_dir , 0 , 0 , 1 );
		VCROSS( zero_twist , z_dir , vec_ae );
		VUNITIZE( zero_twist );
		VCROSS( ninety_twist , vec_ae , zero_twist );
		VUNITIZE( ninety_twist );

		*twist = bn_atan2( VDOT( vec_twist , ninety_twist ) , VDOT( vec_twist , zero_twist ) ) * bn_radtodeg;

		/* stabilize flutter between +/- 180 */
		if( NEAR_ZERO( *twist + 180.0 , accuracy ) )
			*twist = 180.0;
	}
}


/*
 *			B N _ M A T _ A N G L E S
 *
 * This routine builds a Homogeneous rotation matrix, given
 * alpha, beta, and gamma as angles of rotation, in degrees.
 *
 * Alpha is angle of rotation about the X axis, and is done third.
 * Beta is angle of rotation about the Y axis, and is done second.
 * Gamma is angle of rotation about Z axis, and is done first.
 */
void
bn_mat_angles(register fastf_t *mat, double alpha_in, double beta_in, double ggamma_in)
{
	LOCAL double alpha, beta, ggamma;
	LOCAL double calpha, cbeta, cgamma;
	LOCAL double salpha, sbeta, sgamma;

	if( alpha_in == 0.0 && beta_in == 0.0 && ggamma_in == 0.0 )  {
		MAT_IDN( mat );
		return;
	}

	alpha = alpha_in * bn_degtorad;
	beta = beta_in * bn_degtorad;
	ggamma = ggamma_in * bn_degtorad;

	calpha = cos( alpha );
	cbeta = cos( beta );
	cgamma = cos( ggamma );

	/* sine of "180*bn_degtorad" will not be exactly zero
	 * and will result in errors when some codes try to
	 * convert this back to azimuth and elevation.
	 * do_frame() uses this technique!!!
	 */
	if( alpha_in == 180.0 )
		salpha = 0.0;
	else
		salpha = sin( alpha );

	if( beta_in == 180.0 )
		sbeta = 0.0;
	else
		sbeta = sin( beta );

	if( ggamma_in == 180.0 )
		sgamma = 0.0;
	else
		sgamma = sin( ggamma );

	mat[0] = cbeta * cgamma;
	mat[1] = -cbeta * sgamma;
	mat[2] = sbeta;
	mat[3] = 0.0;

	mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
	mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
	mat[6] = -salpha * cbeta;
	mat[7] = 0.0;

	mat[8] = salpha * sgamma - calpha * sbeta * cgamma;
	mat[9] = salpha * cgamma + calpha * sbeta * sgamma;
	mat[10] = calpha * cbeta;
	mat[11] = 0.0;
	mat[12] = mat[13] = mat[14] = 0.0;
	mat[15] = 1.0;
}

/*
 *			B N _ M A T _ A N G L E S _ R A D
 *
 * This routine builds a Homogeneous rotation matrix, given
 * alpha, beta, and gamma as angles of rotation, in radians.
 *
 * Alpha is angle of rotation about the X axis, and is done third.
 * Beta is angle of rotation about the Y axis, and is done second.
 * Gamma is angle of rotation about Z axis, and is done first.
 */
void
bn_mat_angles_rad(register mat_t	mat,
		  double		alpha,
		  double		beta,
		  double		ggamma)
{
	LOCAL double calpha, cbeta, cgamma;
	LOCAL double salpha, sbeta, sgamma;

	if (alpha == 0.0 && beta == 0.0 && ggamma == 0.0) {
		MAT_IDN( mat );
		return;
	}

	calpha = cos( alpha );
	cbeta = cos( beta );
	cgamma = cos( ggamma );

	salpha = sin( alpha );
	sbeta = sin( beta );
	sgamma = sin( ggamma );

	mat[0] = cbeta * cgamma;
	mat[1] = -cbeta * sgamma;
	mat[2] = sbeta;
	mat[3] = 0.0;

	mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
	mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
	mat[6] = -salpha * cbeta;
	mat[7] = 0.0;

	mat[8] = salpha * sgamma - calpha * sbeta * cgamma;
	mat[9] = salpha * cgamma + calpha * sbeta * sgamma;
	mat[10] = calpha * cbeta;
	mat[11] = 0.0;
	mat[12] = mat[13] = mat[14] = 0.0;
	mat[15] = 1.0;
}

/*
 *			B N _ E I G E N 2 X 2
 *
 *  Find the eigenvalues and eigenvectors of a
 *  symmetric 2x2 matrix.
 *	( a b )
 *	( b c )
 *
 *  The eigenvalue with the smallest absolute value is
 *  returned in val1, with its eigenvector in vec1.
 */
void
bn_eigen2x2(fastf_t *val1, fastf_t *val2, fastf_t *vec1, fastf_t *vec2, fastf_t a, fastf_t b, fastf_t c)
{
	fastf_t	d, root;
	fastf_t	v1, v2;

	d = 0.5 * (c - a);

	/* Check for diagonal matrix */
	if( NEAR_ZERO(b, 1.0e-10) ) {
		/* smaller mag first */
		if( fabs(c) < fabs(a) ) {
			*val1 = c;
			VSET( vec1, 0.0, 1.0, 0.0 );
			*val2 = a;
			VSET( vec2, -1.0, 0.0, 0.0 );
		} else {
			*val1 = a;
			VSET( vec1, 1.0, 0.0, 0.0 );
			*val2 = c;
			VSET( vec2, 0.0, 1.0, 0.0 );
		}
		return;
	}

	root = sqrt( d*d + b*b );
	v1 = 0.5 * (c + a) - root;
	v2 = 0.5 * (c + a) + root;

	/* smaller mag first */
	if( fabs(v1) < fabs(v2) ) {
		*val1 = v1;
		*val2 = v2;
		VSET( vec1, b, d - root, 0.0 );
	} else {
		*val1 = v2;
		*val2 = v1;
		VSET( vec1, root - d, b, 0.0 );
	}
	VUNITIZE( vec1 );
	VSET( vec2, -vec1[Y], vec1[X], 0.0 );	/* vec1 X vec2 = +Z */
}

/*
 *			B N _ V E C _ P E R P
 *
 *  Given a vector, create another vector which is perpendicular to it.
 *  The output vector will have unit length only if the input vector did.
 */
void
bn_vec_perp(vect_t new, const vect_t old)
{
	register int i;
	LOCAL vect_t another;	/* Another vector, different */

	i = X;
	if( fabs(old[Y])<fabs(old[i]) )  i=Y;
	if( fabs(old[Z])<fabs(old[i]) )  i=Z;
	VSETALL( another, 0 );
	another[i] = 1.0;
	if( old[X] == 0 && old[Y] == 0 && old[Z] == 0 )  {
		VMOVE( new, another );
	} else {
		VCROSS( new, another, old );
	}
}

/*
 *			B N _ M A T _ F R O M T O
 *
 *  Given two vectors, compute a rotation matrix that will transform
 *  space by the angle between the two.  There are many
 *  candidate matricies.
 *
 *  The input 'from' and 'to' vectors need not be unit length.
 *  MAT4X3VEC( to, m, from ) is the identity that is created.
 */
void
bn_mat_fromto(mat_t m, const vect_t from, const vect_t to)
{
	vect_t	test_to;
	vect_t	unit_from, unit_to;
	fastf_t	dot;

	/*
	 *  The method used here is from Graphics Gems, A. Glasner, ed.
	 *  page 531, "The Use of Coordinate Frames in Computer Graphics",
	 *  by Ken Turkowski, Example 6.
	 */
	mat_t	Q, Qt;
	mat_t	R;
	mat_t	A;
	mat_t	temp;
	vect_t	N, M;
	vect_t	w_prime;		/* image of "to" ("w") in Qt */

	VMOVE( unit_from, from );
	VUNITIZE( unit_from );		/* aka "v" */
	VMOVE( unit_to, to );
	VUNITIZE( unit_to );		/* aka "w" */

	/*  If from and to are the same or opposite, special handling
	 *  is needed, because the cross product isn't defined.
	 *  asin(0.00001) = 0.0005729 degrees (1/2000 degree)
	 */
	dot = VDOT(unit_from, unit_to);
	if( dot > 1.0-0.00001 )  {
		/* dot == 1, return identity matrix */
		MAT_IDN(m);
		return;
	}
	if( dot < -1.0+0.00001 )  {
		/* dot == -1, select random perpendicular N vector */
		bn_vec_perp( N, unit_from );
	} else {
		VCROSS( N, unit_from, unit_to );
		VUNITIZE( N );			/* should be unnecessary */
	}
	VCROSS( M, N, unit_from );
	VUNITIZE( M );			/* should be unnecessary */

	/* Almost everything here is done with pre-multiplys:  vector * mat */
	MAT_IDN( Q );
	VMOVE( &Q[0], unit_from );
	VMOVE( &Q[4], M );
	VMOVE( &Q[8], N );
	bn_mat_trn( Qt, Q );

	/* w_prime = w * Qt */
	MAT4X3VEC( w_prime, Q, unit_to );	/* post-multiply by transpose */

	MAT_IDN( R );
	VMOVE( &R[0], w_prime );
	VSET( &R[4], -w_prime[Y], w_prime[X], w_prime[Z] );
	VSET( &R[8], 0, 0, 1 );		/* is unnecessary */

	bn_mat_mul( temp, R, Q );
	bn_mat_mul( A, Qt, temp );
	bn_mat_trn( m, A );		/* back to post-multiply style */

	/* Verify that it worked */
	MAT4X3VEC( test_to, m, unit_from );
	dot = VDOT( unit_to, test_to );
	if( dot < 0.98 || dot > 1.02 )  {
		bu_log("bn_mat_fromto() ERROR!  from (%g,%g,%g) to (%g,%g,%g) went to (%g,%g,%g), dot=%g?\n",
			V3ARGS(from),
			V3ARGS(to),
			V3ARGS( test_to ), dot );
	}
}

/*
 *			B N _ M A T _ X R O T
 *
 *  Given the sin and cos of an X rotation angle, produce the rotation matrix.
 */
void
bn_mat_xrot(fastf_t *m, double sinx, double cosx)
{
	m[0] = 1.0;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;

	m[4] = 0.0;
	m[5] = cosx;
	m[6] = -sinx;
	m[7] = 0.0;

	m[8] = 0.0;
	m[9] = sinx;
	m[10] = cosx;
	m[11] = 0.0;

	m[12] = m[13] = m[14] = 0.0;
	m[15] = 1.0;
}

/*
 *			B N _ M A T _ Y R O T
 *
 *  Given the sin and cos of a Y rotation angle, produce the rotation matrix.
 */
void
bn_mat_yrot(fastf_t *m, double siny, double cosy)
{
	m[0] = cosy;
	m[1] = 0.0;
	m[2] = -siny;
	m[3] = 0.0;

	m[4] = 0.0;
	m[5] = 1.0;
	m[6] = 0.0;
	m[7] = 0.0;

	m[8] = siny;
	m[9] = 0.0;
	m[10] = cosy;

	m[11] = 0.0;
	m[12] = m[13] = m[14] = 0.0;
	m[15] = 1.0;
}

/*
 *			B N _ M A T _ Z R O T
 *
 *  Given the sin and cos of a Z rotation angle, produce the rotation matrix.
 */
void
bn_mat_zrot(fastf_t *m, double sinz, double cosz)
{
	m[0] = cosz;
	m[1] = -sinz;
	m[2] = 0.0;
	m[3] = 0.0;

	m[4] = sinz;
	m[5] = cosz;
	m[6] = 0.0;
	m[7] = 0.0;

	m[8] = 0.0;
	m[9] = 0.0;
	m[10] = 1.0;
	m[11] = 0.0;

	m[12] = m[13] = m[14] = 0.0;
	m[15] = 1.0;
}


/*
 *			B N _ M A T _ L O O K A T
 *
 *  Given a direction vector D of unit length,
 *  product a matrix which rotates that vector D onto the -Z axis.
 *  This matrix will be suitable for use as a "model2view" matrix.
 *
 *  XXX This routine will fail if the vector is already more or less aligned
 *  with the Z axis.
 *
 *  This is done in several steps.
 *	1)  Rotate D about Z to match +X axis.  Azimuth adjustment.
 *	2)  Rotate D about Y to match -Y axis.  Elevation adjustment.
 *	3)  Rotate D about Z to make projection of X axis again point
 *	    in the +X direction.  Twist adjustment.
 *	4)  Optionally, flip sign on Y axis if original Z becomes inverted.
 *	    This can be nice for static frames, but is astonishing when
 *	    used in animation.
 */
void
bn_mat_lookat(mat_t rot, const vect_t dir, int yflip)
{
	mat_t	first;
	mat_t	second;
	mat_t	prod12;
	mat_t	third;
	vect_t	x;
	vect_t	z;
	vect_t	t1;
	fastf_t	hypot_xy;
	vect_t	xproj;
	vect_t	zproj;

	/* First, rotate D around Z axis to match +X axis (azimuth) */
	hypot_xy = hypot( dir[X], dir[Y] );
	bn_mat_zrot( first, -dir[Y] / hypot_xy, dir[X] / hypot_xy );

	/* Next, rotate D around Y axis to match -Z axis (elevation) */
	bn_mat_yrot( second, -hypot_xy, -dir[Z] );
	bn_mat_mul( prod12, second, first );

	/* Produce twist correction, by re-orienting projection of X axis */
	VSET( x, 1, 0, 0 );
	MAT4X3VEC( xproj, prod12, x );
	hypot_xy = hypot( xproj[X], xproj[Y] );
	if( hypot_xy < 1.0e-10 )  {
		bu_log("Warning: bn_mat_lookat:  unable to twist correct, hypot=%g\n", hypot_xy);
		VPRINT( "xproj", xproj );
		MAT_COPY( rot, prod12 );
		return;
	}
	bn_mat_zrot( third, -xproj[Y] / hypot_xy, xproj[X] / hypot_xy );
	bn_mat_mul( rot, third, prod12 );

	if( yflip )  {
		VSET( z, 0, 0, 1 );
		MAT4X3VEC( zproj, rot, z );
		/* If original Z inverts sign, flip sign on resulting Y */
		if( zproj[Y] < 0.0 )  {
			MAT_COPY( prod12, rot );
			MAT_IDN( third );
			third[5] = -1;
			bn_mat_mul( rot, third, prod12 );
		}
	}

	/* Check the final results */
	MAT4X3VEC( t1, rot, dir );
	if( t1[Z] > -0.98 )  {
		bu_log("Error:  bn_mat_lookat final= (%g, %g, %g)\n", t1[X], t1[Y], t1[Z] );
	}
}

/*
 *			B N _ V E C _ O R T H O
 *
 *  Given a vector, create another vector which is perpendicular to it,
 *  and with unit length.  This algorithm taken from Gift's arvec.f;
 *  a faster algorithm may be possible.
 */
void
bn_vec_ortho(register vect_t out, register const vect_t in)
{
	register int j, k;
	FAST fastf_t	f;
	register int i;

	if( NEAR_ZERO(in[X], 0.0001) && NEAR_ZERO(in[Y], 0.0001) &&
	    NEAR_ZERO(in[Z], 0.0001) )  {
		VSETALL( out, 0 );
		VPRINT("bn_vec_ortho: zero-length input", in);
		return;
	}

	/* Find component closest to zero */
	f = fabs(in[X]);
	i = X;
	j = Y;
	k = Z;
	if( fabs(in[Y]) < f )  {
		f = fabs(in[Y]);
		i = Y;
		j = Z;
		k = X;
	}
	if( fabs(in[Z]) < f )  {
		i = Z;
		j = X;
		k = Y;
	}
	f = hypot( in[j], in[k] );
	if( NEAR_ZERO( f, SMALL ) ) {
		VPRINT("bn_vec_ortho: zero hypot on", in);
		VSETALL( out, 0 );
		return;
	}
	f = 1.0/f;
	out[i] = 0.0;
	out[j] = -in[k]*f;
	out[k] =  in[j]*f;
}


/*
 *			B N _ M A T _ S C A L E _ A B O U T _ P T
 *
 *  Build a matrix to scale uniformly around a given point.
 *
 *  Returns -
 *	-1	if scale is too small.
 *	 0	if OK.
 */
int
bn_mat_scale_about_pt(mat_t mat, const point_t pt, const double scale)
{
	mat_t	xlate;
	mat_t	s;
	mat_t	tmp;

	MAT_IDN( xlate );
	MAT_DELTAS_VEC_NEG( xlate, pt );

	MAT_IDN( s );
	if( NEAR_ZERO( scale, SMALL ) )  {
		MAT_ZERO( mat );
		return -1;			/* ERROR */
	}
	s[15] = 1/scale;

	bn_mat_mul( tmp, s, xlate );

	MAT_DELTAS_VEC( xlate, pt );
	bn_mat_mul( mat, xlate, tmp );
	return 0;				/* OK */
}

/*
 *			B N _ M A T _ X F O R M _ A B O U T _ P T
 *
 *  Build a matrix to apply arbitary 4x4 transformation around a given point.
 */
void
bn_mat_xform_about_pt(mat_t mat, const mat_t xform, const point_t pt)
{
	mat_t	xlate;
	mat_t	tmp;

	MAT_IDN( xlate );
	MAT_DELTAS_VEC_NEG( xlate, pt );

	bn_mat_mul( tmp, xform, xlate );

	MAT_DELTAS_VEC( xlate, pt );
	bn_mat_mul( mat, xlate, tmp );
}

/*
 *			B N _ M A T _ I S _ E Q U A L
 *
 *  Returns -
 *	0	When matrices are not equal
 *	1	When matricies are equal
 */
int
bn_mat_is_equal(const mat_t a, const mat_t b, const struct bn_tol *tol)
{
	register int i;
	register double f;
	register double tdist, tperp;

	BN_CK_TOL(tol);

	tdist = tol->dist;
	tperp = tol->perp;

	/*
	 * First, check that the translation part of the matrix (dist) are
	 * within the distance tolerance.
	 * Because most instancing involves translation and no rotation,
	 * doing this first should detect most non-equal cases rapidly.
	 */
	for (i=3; i<12; i+=4) {
		f = a[i] - b[i];
		if ( !NEAR_ZERO(f, tdist)) return 0;
	}

	/*
	 * Check that the rotation part of the matrix (cosines) are within
	 * the perpendicular tolerance.
	 */
	for (i = 0; i < 16; i+=4) {
		f = a[i] - b[i];
		if ( !NEAR_ZERO(f, tperp)) return 0;
		f = a[i+1] - b[i+1];
		if ( !NEAR_ZERO(f, tperp)) return 0;
		f = a[i+2] - b[i+2];
		if ( !NEAR_ZERO(f, tperp)) return 0;
	}
	/*
	 * Check that the scale part of the matrix (ratio) is within the
	 * perpendicular tolerance.  There is no ratio tolerance so we use
	 * the tighter of dist or perp.
	 */
	f = a[15] - b[15];
	if ( !NEAR_ZERO(f, tperp)) return 0;

	return 1;
}


/*
 *			B N _ M A T _ I S _ I D E N T I T Y
 *
 *  This routine is intended for detecting identity matricies read in
 *  from ascii or binary files, where the numbers are pure ones or zeros.
 *  This routine is *not* intended for tolerance-based "near-zero"
 *  comparisons; as such, it shouldn't be used on matrices which are
 *  the result of calculation.
 *
 *  Returns -
 *	0	non-identity
 *	1	a perfect identity matrix
 */
int
bn_mat_is_identity(const mat_t m)
{
	return (! memcmp(m, bn_mat_identity, sizeof(mat_t)));
}

/*	B N _ M A T _ A R B _ R O T
 *
 * Construct a transformation matrix for rotation about an arbitrary axis
 *
 *	The axis is defined by a point (pt) and a unit direction vector (dir).
 *	The angle of rotation is "ang"
 */
void
bn_mat_arb_rot(mat_t m, const point_t pt, const vect_t dir, const fastf_t ang)
{
	mat_t tran1,tran2,rot;
	double cos_ang, sin_ang, one_m_cosang;
	double n1_sq, n2_sq, n3_sq;
	double n1_n2, n1_n3, n2_n3;

	if( ang == 0.0 )
	{
		MAT_IDN( m );
		return;
	}

	MAT_IDN( tran1 );
	MAT_IDN( tran2 );

	/* construct translation matrix to pt */
	tran1[MDX] = (-pt[X]);
	tran1[MDY] = (-pt[Y]);
	tran1[MDZ] = (-pt[Z]);

	/* construct translation back from pt */
	tran2[MDX] = pt[X];
	tran2[MDY] = pt[Y];
	tran2[MDZ] = pt[Z];

	/* construct rotation matrix */
	cos_ang = cos( ang );
	sin_ang = sin( ang );
	one_m_cosang = 1.0 - cos_ang;
	n1_sq = dir[X]*dir[X];
	n2_sq = dir[Y]*dir[Y];
	n3_sq = dir[Z]*dir[Z];
	n1_n2 = dir[X]*dir[Y];
	n1_n3 = dir[X]*dir[Z];
	n2_n3 = dir[Y]*dir[Z];

	MAT_IDN( rot );
	rot[0] = n1_sq + (1.0 - n1_sq)*cos_ang;
	rot[1] = n1_n2 * one_m_cosang - dir[Z]*sin_ang;
	rot[2] = n1_n3 * one_m_cosang + dir[Y]*sin_ang;

	rot[4] = n1_n2 * one_m_cosang + dir[Z]*sin_ang;
	rot[5] = n2_sq + (1.0 - n2_sq)*cos_ang;
	rot[6] = n2_n3 * one_m_cosang - dir[X]*sin_ang;

	rot[8] = n1_n3 * one_m_cosang - dir[Y]*sin_ang;
	rot[9] = n2_n3 * one_m_cosang + dir[X]*sin_ang;
	rot[10] = n3_sq + (1.0 - n3_sq) * cos_ang;

	bn_mat_mul( m, rot, tran1 );
	bn_mat_mul2( tran2, m );
}


/*
 *			B N _ M A T _ D U P
 *
 *  Return a pointer to a copy of the matrix in dynamically allocated memory.
 */
matp_t
bn_mat_dup(const mat_t in)
{
	matp_t	out;

	out = (matp_t) bu_malloc( sizeof(mat_t), "bn_mat_dup" );
	bcopy( (const char *)in, (char *)out, sizeof(mat_t) );
	return out;
}

/*
 *			B N _ M A T _ C K
 *
 *  Check to ensure that a rotation matrix preserves axis perpendicularily.
 *  Note that not all matricies are rotation matricies.
 *
 *  Returns -
 *	-1	FAIL
 *	 0	OK
 */
int
bn_mat_ck(const char *title, const mat_t m)
{
	vect_t	A, B, C;
	fastf_t	fx, fy, fz;

	if( !m )  return 0;		/* implies identity matrix */

	/*
	 * Validate that matrix preserves perpendicularity of axis
	 * by checking that A.B == 0, B.C == 0, A.C == 0
	 * XXX these vectors should just be grabbed out of the matrix
	 */
#if 0
	MAT4X3VEC( A, m, xaxis );
	MAT4X3VEC( B, m, yaxis );
	MAT4X3VEC( C, m, zaxis );
#else
	VMOVE( A, &m[0] );
	VMOVE( B, &m[4] );
	VMOVE( C, &m[8] );
#endif
	fx = VDOT( A, B );
	fy = VDOT( B, C );
	fz = VDOT( A, C );
	if( ! NEAR_ZERO(fx, 0.0001) ||
	    ! NEAR_ZERO(fy, 0.0001) ||
	    ! NEAR_ZERO(fz, 0.0001) ||
	    NEAR_ZERO( m[15], VDIVIDE_TOL )
	)  {
		bu_log("bn_mat_ck(%s):  bad matrix, does not preserve axis perpendicularity.\n  X.Y=%g, Y.Z=%g, X.Z=%g, s=%g\n",
			title, fx, fy, fz, m[15] );
		bn_mat_print("bn_mat_ck() bad matrix", m);

		if( bu_debug & (BU_DEBUG_MATH | BU_DEBUG_COREDUMP) )  {
			bu_debug |= BU_DEBUG_COREDUMP;
			bu_bomb("bn_mat_ck() bad matrix\n");
		}
	    	return -1;	/* FAIL */
	}
	return 0;		/* OK */
}

/*
 *		B N _ M A T _ D E T 3
 *
 *	Calculates the determinant of the 3X3 "rotation"
 *	part of the passed matrix
 */
fastf_t
bn_mat_det3(const mat_t m)
{
	FAST fastf_t sum;

	sum = m[0] * ( m[5]*m[10] - m[6]*m[9] )
	     -m[1] * ( m[4]*m[10] - m[6]*m[8] )
	     +m[2] * ( m[4]*m[9] - m[5]*m[8] );

	return( sum );
}


/*
 *		B N _ M A T _ D E T E R M I N A N T
 *
 *	Calculates the determinant of the 4X4 matrix
 */
fastf_t
bn_mat_determinant(const mat_t m)
{
	fastf_t det[4];
	fastf_t sum;

	det[0] = m[5] * (m[10]*m[15] - m[11]*m[14])
		-m[6] * (m[ 9]*m[15] - m[11]*m[13])
		+m[7] * (m[ 9]*m[14] - m[10]*m[13]);

	det[1] = m[4] * (m[10]*m[15] - m[11]*m[14])
		-m[6] * (m[ 8]*m[15] - m[11]*m[12])
		+m[7] * (m[ 8]*m[14] - m[10]*m[12]);

	det[2] = m[4] * (m[ 9]*m[15] - m[11]*m[13])
		-m[5] * (m[ 8]*m[15] - m[11]*m[12])
		+m[7] * (m[ 8]*m[13] - m[ 9]*m[12]);

	det[3] = m[4] * (m[ 9]*m[14] - m[10]*m[13])
		-m[5] * (m[ 8]*m[14] - m[10]*m[12])
		+m[6] * (m[ 8]*m[13] - m[ 9]*m[12]);

	sum = m[0]*det[0] - m[1]*det[1] + m[2]*det[2] - m[3]*det[3];

	return( sum );

}

int
bn_mat_is_non_unif(const mat_t m)
{
    double mag[3];

    mag[0] = MAGSQ(m);
    mag[1] = MAGSQ(&m[4]);
    mag[2] = MAGSQ(&m[8]);

    if (fabs(1.0 - (mag[1]/mag[0])) > .0005 ||
	fabs(1.0 - (mag[2]/mag[0])) > .0005) {

	return 1;
    }
    
    if (m[12] != 0.0 || m[13] != 0.0 || m[14] != 0.0)
	return 2;

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
