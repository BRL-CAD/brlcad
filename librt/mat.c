/*
 *			M A T . C
 *
 * 4 x 4 Matrix manipulation functions..............
 *
 *	mat_zero( &m )			Fill matrix m with zeros
 *	mat_idn( &m )			Fill matrix m with identity matrix
 *	mat_copy( &o, &i )		Copy matrix i to matrix o
 *	mat_mul( &o, &i1, &i2 )		Multiply i1 by i2 and store in o
 *	matXvec( &ov, &m, &iv )		Multiply m by vector iv, store in ov
 *	mat_inv( &om, &im )		Invert matrix im, store result in om
 *	mat_print( &title, &m )		Print matrix (with title) on stdout.
 *	mat_trn( &o, &i )		Transpose matrix i into matrix o
 *	mat_ae( &o, azimuth, elev)	Make rot matrix from azimuth+elevation
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
 *	U. S. Army Ballistic Research Laboratory
 *
 * $Revision$
 */

#include	<math.h>
#include	"vmath.h"

extern int	printf();


/*
 *			M A T _ Z E R O
 *
 * Fill in the matrix "m" with zeros.
 */
void
mat_zero( m )
register matp_t m;
{
	register int i = 0;

	/* Clear everything */
	for(; i<16; i++)
		*m++ = 0.0;
}


/*
 *			M A T _ I D N
 *
 * Fill in the matrix "m" with an identity matrix.
 */
void
mat_idn( m )
register matp_t m;
{
	/* Clear everything first */
	mat_zero( m );

	/* Set ones in the diagonal */
	m[0] = m[5] = m[10] = m[15] = 1.0;
}


/*
 *			M A T _ C O P Y
 *
 * Copy the matrix "im" into the matrix "om".
 */
void
mat_copy( om, im )
register matp_t om;
register matp_t im;
{
	register int i = 0;

	/* Copy all elements */
	for(; i<16; i++)
		*om++ = *im++;
}


/*
 *			M A T _ M U L
 *
 * Multiply matrix "im1" by "im2" and store the result in "om".
 * NOTE:  This is different from multiplying "im2" by "im1" (most
 * of the time!)
 * NOTE: "om" must not be the same as either of the inputs.
 */
void
mat_mul( om, im1, im2 )
register matp_t om;
register matp_t im1;
register matp_t im2;
{
	register int em1;		/* Element subscript for im1 */
	register int em2;		/* Element subscript for im2 */
	register int el = 0;		/* Element subscript for om */
	register int i;			/* For counting */

	/* For each element in the output matrix... */
	for(; el<16; el++) {

		om[el] = 0.0;		/* Start with zero in output */
		em1 = (el/4)*4;		/* Element at right of row in im1 */
		em2 = el%4;		/* Element at top of column in im2 */

		for(i=0; i<4; i++) {
			om[el] += im1[em1] * im2[em2];

			em1++;		/* Next row element in m1 */
			em2 += 4;	/* Next column element in m2 */
		}
	}
}


/*
 *			M A T X V E C
 *
 * Multiply the matrix "im" by the vector "iv" and store the result
 * in the vector "ov".  Note this is post-multiply.
 */
void
matXvec(ov, im, iv)
register vectp_t ov;
register matp_t im;
register vectp_t iv;
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
 *			M A T _ I N V
 *
 * The matrix pointed at by "im" is inverted and stored in the area
 * pointed at by "om".
 */
#define EPSILON	0.000001

/* 
 * Invert a 4-by-4 matrix using Algorithm 120 from ACM.
 * This is a modified Gauss-Jordan alogorithm
 * Note:  Inversion is done in place, with 3 work vectors
 */
void
mat_inv( output, input )
matp_t input;
register matp_t output;
{
	register int i, j;			/* Indices */
	static int k;				/* Indices */
	static int	z[4];			/* Temporary */
	static float	b[4];			/* Temporary */
	static float	c[4];			/* Temporary */

	mat_copy( output, input );	/* Duplicate */

	/* Initialization */
	for( j = 0; j < 4; j++ )
		z[j] = j;

	/* Main Loop */
	for( i = 0; i < 4; i++ )  {
		static float y;				/* local temporary */

		k = i;
		y = output[i*4+i];
		for( j = i+1; j < 4; j++ )  {
			static float w;			/* local temporary */

			w = output[i*4+j];
			if( fabs(w) > fabs(y) )  {
				k = j;
				y = w;
			}
		}

		if( fabs(y) < EPSILON )  {
			(void)printf("mat_inv:  error!\n");
			return;		/* ERROR */
		}
		y = 1.0 / y;

		for( j = 0; j < 4; j++ )  {
			static float temp;		/* Local */

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
			static int p;			/* Local temp */

			for( j = 0; j < 4; j++ )  {
				static float w;		/* Local temp */

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
 *			V T O H _ M O V E
 *
 * Takes a pointer to a [x,y,z] vector, and a pointer
 * to space for a homogeneous vector [x,y,z,w],
 * and builds [x,y,z,1].
 */
void
vtoh_move( h, v )
register float *h, *v;
{
	*h++ = *v++;
	*h++ = *v++;
	*h++ = *v;
	*h++ = 1.0;
}

/*
 *			H T O V _ M O V E
 *
 * Takes a pointer to [x,y,z,w], and converts it to
 * an ordinary vector [x/w, y/w, z/w].
 * Optimization for the case of w==1 is performed.
 */
void
htov_move( v, h )
register float *v, *h;
{
	static float inv;

	if( h[3] == 1.0 )  {
		*v++ = *h++;
		*v++ = *h++;
		*v   = *h;
	}  else  {
		if( h[3] == 0.0 )  {
			(void)printf("htov_move: divide by %f!\n", h[3]);
			return;
		}
		inv = 1.0 / h[3];
		*v++ = *h++ * inv;
		*v++ = *h++ * inv;
		*v   = *h   * inv;
	}
}

/*
 *			M A T _ P R I N T
 */
void
mat_print( title, m )
char *title;
mat_t m;
{
	register int i;

	printf("MATRIX %s:\n  ", title);
	for(i=0; i<16; i++)  {
		printf(" %8.3f", m[i]);
		if( (i&3) == 3 ) printf("\n  ");
	}
}

/*
 *			M A T _ T R N
 */
void
mat_trn( om, im )
register float *om;
register matp_t im;
{
	*om++ = im[0];
	*om++ = im[4];
	*om++ = im[8];
	*om++ = im[12];

	*om++ = im[1];
	*om++ = im[5];
	*om++ = im[9];
	*om++ = im[13];

	*om++ = im[2];
	*om++ = im[6];
	*om++ = im[10];
	*om++ = im[14];

	*om++ = im[3];
	*om++ = im[7];
	*om++ = im[11];
	*om++ = im[15];
}

/*
 *			M A T _ A E
 *
 *  Compute a 4x4 rotation matrix given Azimuth and Elevation.
 *  
 *  Azimuth is +X, Elevation is +Z, both in degrees.
 *
 *  Formula due to Doug Gwyn, BRL.
 */
void
mat_ae( m, azimuth, elev )
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
