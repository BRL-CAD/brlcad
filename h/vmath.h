/*
 *			V M A T H . H
 *
 *  This header file defines many commonly used 3D vector math macros.
 *
 *  Note that while many people in the computer graphics field use
 *  post-multiplication with row vectors (ie, vector * matrix * matrix ...)
 *  the BRL CAD system uses the more traditional representation of 
 *  column vectors (ie, ... matrix * matrix * vector).  (The matrices
 *  in these two representations are the transposes of each other). Therefore,
 *  when transforming a vector by a matrix, pre-multiplication is used, ie:
 *
 *		view_vec = model2view_mat * model_vec
 *
 *  Furthermore, additional transformations are multiplied on the left, ie:
 *
 *		vec'  =  T1 * vec
 *		vec'' =  T2 * T1 * vec  =  T2 * vec'
 *
 *  The most notable implication of this is the location of the
 *  "delta" (translation) values in the matrix, ie:
 *
 *        x'     ( R0   R1   R2   Dx )      x
 *        y' =   ( R4   R5   R6   Dy )   *  y
 *        z'     ( R8   R9   R10  Dz )      z
 *        w'     (  0    0    0   1/s)      w
 *
 *  Note -
 *	vect_t objects are 3-tuples
 *	hvect_t objects are 4-tuples
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Distribution Status -
 *	This file is public domain, distribution unlimited.
 *
 *  $Header$
 */

#ifndef VMATH_H
#define VMATH_H seen

#ifndef sqrt
#	ifdef __STDC__
		extern double sqrt(double x);
#	else
		extern double sqrt();
#	endif
#endif

#define NEAR_ZERO(val,epsilon)	( ((val) > -epsilon) && ((val) < epsilon) )

/*
 * Types for matrixes and vectors.
 */
typedef	fastf_t	mat_t[4*4];
typedef	fastf_t	*matp_t;

#define ELEMENTS_PER_VECT	3	/* # of fastf_t's per vect_t */
#define ELEMENTS_PER_PT         3
#define HVECT_LEN		4	/* # of fastf_t's per hvect_t */
#define HPT_LEN			4

typedef	fastf_t	vect_t[ELEMENTS_PER_VECT];
typedef	fastf_t	*vectp_t;

typedef fastf_t	point_t[ELEMENTS_PER_PT];
typedef fastf_t	*pointp_t;

typedef fastf_t hvect_t[HVECT_LEN];
typedef fastf_t hpoint_t[HPT_LEN];

/* Element names in homogeneous vector (4-tuple) */
#define	X	0
#define	Y	1
#define Z	2
#define H	3

/* Locations of deltas in 4x4 Homogenous Transform matrix */
#define MDX	3
#define MDY	7
#define MDZ	11
#define MAT_DELTAS(m,x,y,z)	(m)[MDX] = x; \
				(m)[MDY] = y; \
				(m)[MDZ] = z;

/* Set vector at `a' to have coordinates `b', `c', `d' */
#define VSET(a,b,c,d)	(a)[0] = (b);\
			(a)[1] = (c);\
			(a)[2] = (d)

/* Set all elements of vector to same scalar value */
#define VSETALL(a,s)	(a)[0] = (a)[1] = (a)[2] = (s);

/* Transfer vector at `b' to vector at `a' */
#define VMOVE(a,b)	(a)[0] = (b)[0];\
			(a)[1] = (b)[1];\
			(a)[2] = (b)[2]

/* Transfer vector of length `n' at `b' to vector at `a' */
#define VMOVEN(a,b,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i]; \
	}

/* Reverse the direction of b and store it in a */
#define VREVERSE(a,b)	(a)[0] = -(b)[0]; \
			(a)[1] = -(b)[1]; \
			(a)[2] = -(b)[2];

/* Add vectors at `b' and `c', store result at `a' */
#ifdef VECTORIZE
#define VADD2(a,b,c) VADD2N(a,b,c, 3)
#else
#define VADD2(a,b,c)	(a)[0] = (b)[0] + (c)[0];\
			(a)[1] = (b)[1] + (c)[1];\
			(a)[2] = (b)[2] + (c)[2]
#endif VECTORIZE

/* Add vectors of length `n' at `b' and `c', store result at `a' */
#define VADD2N(a,b,c,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] + (c)[_i]; \
	}

/* Subtract vector at `c' from vector at `b', store result at `a' */
#ifdef VECTORIZE
#define VSUB2(a,b,c) 	VSUB2N(a,b,c, 3)
#else
#define VSUB2(a,b,c)	(a)[0] = (b)[0] - (c)[0];\
			(a)[1] = (b)[1] - (c)[1];\
			(a)[2] = (b)[2] - (c)[2]
#endif VECTORIZE

/* Subtract `n' length vector at `c' from vector at `b', store result at `a' */
#define VSUB2N(a,b,c,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] - (c)[_i]; \
	}

/* Vectors:  A = B - C - D */
#ifdef VECTORIZE
#define VSUB3(a,b,c,d) VSUB3(a,b,c,d, 3)
#else
#define VSUB3(a,b,c,d)	(a)[0] = (b)[0] - (c)[0] - (d)[0];\
			(a)[1] = (b)[1] - (c)[1] - (d)[1];\
			(a)[2] = (b)[2] - (c)[2] - (d)[2]
#endif VECTORIZE

/* Vectors:  A = B - C - D for vectors of length `n' */
#define VSUB3N(a,b,c,d,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] - (c)[_i] - (d)[_i]; \
	}

/* Add 3 vectors at `b', `c', and `d', store result at `a' */
#ifdef VECTORIZE
#define VADD3(a,b,c,d) VADD3N(a,b,c,d, 3)
#else
#define VADD3(a,b,c,d)	(a)[0] = (b)[0] + (c)[0] + (d)[0];\
			(a)[1] = (b)[1] + (c)[1] + (d)[1];\
			(a)[2] = (b)[2] + (c)[2] + (d)[2]
#endif VECTORIZE

/* Add 3 vectors of length `n' at `b', `c', and `d', store result at `a' */
#define VADD3N(a,b,c,d,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] + (c)[_i] + (d)[_i]; \
	}

/* Add 4 vectors at `b', `c', `d', and `e', store result at `a' */
#ifdef VECTORIZE
#define VADD4(a,b,c,d,e) VADD4N(a,b,c,d,e, 3)
#else
#define VADD4(a,b,c,d,e) (a)[0] = (b)[0] + (c)[0] + (d)[0] + (e)[0];\
			(a)[1] = (b)[1] + (c)[1] + (d)[1] + (e)[1];\
			(a)[2] = (b)[2] + (c)[2] + (d)[2] + (e)[2]
#endif VECTORIZE

/* Add 4 `n' length vectors at `b', `c', `d', and `e', store result at `a' */
#define VADD4N(a,b,c,d,e,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] + (c)[_i] + (d)[_i] + (e)[_i];\
	}

/* Scale vector at `b' by scalar `c', store result at `a' */
#ifdef VECTORIZE
#define VSCALE(a,b,c) VSCALEN(a,b,c, 3)
#else
#define VSCALE(a,b,c)	(a)[0] = (b)[0] * (c);\
			(a)[1] = (b)[1] * (c);\
			(a)[2] = (b)[2] * (c)
#endif VECTORIZE

/* Scale vector of length `n' at `b' by scalar `c', store result at `a' */
#define VSCALEN(a,b,c,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] * (c); \
	}

/* Normalize vector `a' to be a unit vector */
#ifdef VECTORIZE
#define VUNITIZE(a) \
	{ register double f; register int _i; \
	f = MAGNITUDE(a); \
	if( f < 1.0e-10 ) f = 0.0; else f = 1.0/f; \
	for(_i = 0; _i < 3; _i++) \
		(a)[_i] *= f; \
	}
#else
#define VUNITIZE(a)	{ register double f; f = MAGNITUDE(a); \
			if( f < 1.0e-10 ) f = 0.0; else f = 1.0/f; \
			(a)[0] *= f; (a)[1] *= f; (a)[2] *= f; }
#endif VECTORIZE

/* Combine together several vectors, scaled by a scalar */
#ifdef VECTORIZE
#define VCOMB3(o, a,b, c,d, e,f)	VCOMB3N(o, a,b, c,d, e,f, 3)
#else
#define VCOMB3(o, a,b, c,d, e,f)	{\
	(o)[X] = (a) * (b)[X] + (c) * (d)[X] + (e) * (f)[X];\
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y];\
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z];}
#endif VECTORIZE

#define VCOMB3N(o, a,b, c,d, e,f, n)	{\
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(o)[_i] = (a) * (b)[_i] + (c) * (d)[_i] + (e) * (f)[_i]; \
	} }

#ifdef VECTORIZE
#define VCOMB2(o, a,b, c,d)	VCOMB2N(o, a,b, c,d, 3)
#else
#define VCOMB2(o, a,b, c,d)	{\
	(o)[X] = (a) * (b)[X] + (c) * (d)[X];\
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y];\
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z];}
#endif VECTORIZE

#define VCOMB2N(o, a,b, c,d, n)	{\
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(o)[_i] = (a) * (b)[_i] + (c) * (d)[_i]; \
	} }

/* Compose vector at `a' of:
 *	Vector at `b' plus
 *	scalar `c' times vector at `d' plus
 *	scalar `e' times vector at `f'
 */
#ifdef VECTORIZE
#define VJOIN2(a,b,c,d,e,f)	VJOIN2N(a,b,c,d,e,f,3)
#else
#define VJOIN2(a,b,c,d,e,f)	\
	(a)[0] = (b)[0] + (c) * (d)[0] + (e) * (f)[0];\
	(a)[1] = (b)[1] + (c) * (d)[1] + (e) * (f)[1];\
	(a)[2] = (b)[2] + (c) * (d)[2] + (e) * (f)[2]
#endif VECTORIZE

#define VJOIN2N(a,b,c,d,e,f,n)	\
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] + (c) * (d)[_i] + (e) * (f)[_i]; \
	}

#ifdef VECTORIZE
#define VJOIN1(a,b,c,d)		VJOIN1N(a,b,c,d,3)
#else
#define VJOIN1(a,b,c,d) \
	(a)[0] = (b)[0] + (c) * (d)[0];\
	(a)[1] = (b)[1] + (c) * (d)[1];\
	(a)[2] = (b)[2] + (c) * (d)[2];
#endif VECTORIZE

#define VJOIN1N(a,b,c,d,n) \
	{ register int _i; \
	for(_i = 0; _i < n; _i++) \
		(a)[_i] = (b)[_i] + (c) * (d)[_i]; \
	}

/* Return scalar magnitude squared of vector at `a' */
#define MAGSQ(a)	( (a)[0]*(a)[0] + (a)[1]*(a)[1] + (a)[2]*(a)[2] )

/* Return scalar magnitude of vector at `a' */
#define MAGNITUDE(a)	sqrt( MAGSQ( a ) )

/* Store cross product of vectors at `b' and `c' in vector at `a' */
#define VCROSS(a,b,c)	(a)[0] = (b)[1] * (c)[2] - (b)[2] * (c)[1];\
			(a)[1] = (b)[2] * (c)[0] - (b)[0] * (c)[2];\
			(a)[2] = (b)[0] * (c)[1] - (b)[1] * (c)[0]

/* Compute dot product of vectors at `a' and `b' */
#define VDOT(a,b)	( (a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2] )

/* Print vector name and components on stdout */
#define VPRINT(a,b)	(void)fprintf(stderr,"%s (%g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2])
#define HPRINT(a,b)	(void)fprintf(stderr,"%s (%g, %g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2], (b)[3])

/* Vector element multiplication.  Really: diagonal matrix X vect */
#ifdef VECTORIZE
#define VELMUL(a,b,c) \
	{ register int _i; \
	for(_i = 0; _i < 3; _i++) \
		(a)[_i] = (b)[_i] * (c)[_i]; \
	}
#else
#define VELMUL(a,b,c) \
	(a)[0] = (b)[0] * (c)[0];\
	(a)[1] = (b)[1] * (c)[1];\
	(a)[2] = (b)[2] * (c)[2];
#endif VECTORIZE

/* Apply the 3x3 part of a mat_t to a 3-tuple. */
#ifdef VECTORIZE
#define MAT3X3VEC(o,mat,vec) \
	{ register int _i; \
	for(_i = 0; _i < 3; _i++) \
		(o)[_i] = (mat)[4*_i+0]*(vec)[0] + \
			  (mat)[4*_i+1]*(vec)[1] + \
			  (mat)[4*_i+2]*(vec)[2]; \
	}
#else
#define MAT3X3VEC(o,mat,vec) \
	(o)[0] = (mat)[0]*(vec)[0]+(mat)[1]*(vec)[1] + (mat)[ 2]*(vec)[2]; \
	(o)[1] = (mat)[4]*(vec)[0]+(mat)[5]*(vec)[1] + (mat)[ 6]*(vec)[2]; \
	(o)[2] = (mat)[8]*(vec)[0]+(mat)[9]*(vec)[1] + (mat)[10]*(vec)[2];
#endif VECTORIZE

/* Multiply a 3-tuple by the 3x3 part of a mat_t. */
#ifdef VECTORIZE
#define VEC3X3MAT(o,i,m) \
	{ register int _i; \
	for(_i = 0; _i < 3; _i++) \
		(o)[_i] = (i)[X]*(m)[_i] + (i)[Y]*(m)[_i+4] + (i)[Z]*(m)[_i+8]; \
	}
#else
#define VEC3X3MAT(o,i,m) \
	(o)[X] = (i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10];
#endif VECTORIZE

/* Apply the 3x3 part of a mat_t to a 2-tuple (Z part=0). */
#ifdef VECTORIZE
#define MAT3X2VEC(o,mat,vec) \
	{ register int _i; \
	for(_i = 0; _i < 3; _i++) \
		(o)[_i] = (mat)[4*_i]*(vec)[0] + (mat)[4*_i+1]*(vec)[1]; \
	}
#else
#define MAT3X2VEC(o,mat,vec) \
	(o)[0] = (mat)[0]*(vec)[0] + (mat)[1]*(vec)[1]; \
	(o)[1] = (mat)[4]*(vec)[0] + (mat)[5]*(vec)[1]; \
	(o)[2] = (mat)[8]*(vec)[0] + (mat)[9]*(vec)[1];
#endif VECTORIZE

/* Multiply a 2-tuple (Z=0) by the 3x3 part of a mat_t. */
#ifdef VECTORIZE
#define VEC2X3MAT(o,i,m) \
	{ register int _i; \
	for(_i = 0; _i < 3; _i++) \
		(o)[_i] = (i)[X]*(m)[_i] + (i)[Y]*(m)[2*_i]; \
	}
#else
#define VEC2X3MAT(o,i,m) \
	(o)[X] = (i)[X]*(m)[0] + (i)[Y]*(m)[4]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6];
#endif VECTORIZE

/* Apply a 4x4 matrix to a 3-tuple which is an absolute Point in space */
#ifdef VECTORIZE
#define MAT4X3PNT(o,m,i) \
	{ register double f; \
	register int _i, _j; \
	f = 0.0; \
	for(_j = 0; _j < 3; _j++)  \
		f += (m)[_j+12] * (i)[_j]; \
	f = 1.0/(f + (m)[15]); \
	for(_i = 0; _i < 3; _i++) \
		(o)[_i] = 0.0; \
	for(_i = 0; _i < 3; _i++)  { \
		for(_j = 0; _j < 3; _j++) \
			(o)[_i] += (m)[_j+4*_i] * (i)[_j]; \
	} \
	for(_i = 0; _i < 3; _i++)  { \
		(o)[_i] = ((o)[_i] + (m)[4*_i+3]) * f; \
	} }
#else
#define MAT4X3PNT(o,m,i) \
	{ register double f; \
	f = 1.0/((m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]);\
	(o)[X]=((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[3]) * f;\
	(o)[Y]=((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[7]) * f;\
	(o)[Z]=((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11])* f;}
#endif VECTORIZE

/* Multiply an Absolute 3-Point by a full 4x4 matrix. */
#define PNT3X4MAT(o,i,m) \
	{ register double f; \
	f = 1.0/((i)[X]*(m)[3] + (i)[Y]*(m)[7] + (i)[Z]*(m)[11] + (m)[15]);\
	(o)[X]=((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8] + (m)[12]) * f;\
	(o)[Y]=((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9] + (m)[13]) * f;\
	(o)[Z]=((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10] + (m)[14])* f;}

/* Multiply an Absolute hvect_t 4-Point by a full 4x4 matrix. */
#ifdef VECTORIZE
#define MAT4X4PNT(o,m,i) \
	{ register int _i, _j; \
	for(_i = 0; _i < 4; _i++) (o)[_i] = 0.0; \
	for(_i = 0; _i < 4; _i++) \
		for(_j = 0; _j < 4; _j++) \
			(o)[_i] += (m)[_j+4*_i] * (i)[_j]; \
	}
#else
#define MAT4X4PNT(o,m,i) \
	(o)[X]=(m)[ 0]*(i)[X] + (m)[ 1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[ 3]*(i)[H];\
	(o)[Y]=(m)[ 4]*(i)[X] + (m)[ 5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[ 7]*(i)[H];\
	(o)[Z]=(m)[ 8]*(i)[X] + (m)[ 9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11]*(i)[H];\
	(o)[H]=(m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]*(i)[H];
#endif VECTORIZE

/* Apply a 4x4 matrix to a 3-tuple which is a relative Vector in space */
#ifdef VECTORIZE
#define MAT4X3VEC(o,m,i) \
	{ register double f; \
	register int _i, _j; \
	f = 1.0/((m)[15]); \
	for(_i = 0; _i < 3; _i++) \
		(o)[_i] = 0.0; \
	for(_i = 0; _i < 3; _i++) { \
		for(_j = 0; _j < 3; _j++) \
			(o)[_i] += (m)[_j+4*_i] * (i)[_j]; \
	} \
	for(_i = 0; _i < 3; _i++) { \
		(o)[_i] *= f; \
	} }
#else
#define MAT4X3VEC(o,m,i) \
	{ register double f;	f = 1.0/((m)[15]);\
	(o)[X] = ((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z]) * f; \
	(o)[Y] = ((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z]) * f; \
	(o)[Z] = ((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z]) * f; }
#endif VECTORIZE

/* Multiply a Relative 3-Vector by most of a 4x4 matrix */
#define VEC3X4MAT(o,i,m) \
	{ register double f; 	f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]) * f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]) * f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10]) * f; }

/* Multiply a Relative 2-Vector by most of a 4x4 matrix */
#define VEC2X4MAT(o,i,m) \
	{ register double f; 	f = 1.0/((m)[15]); \
	(o)[X] = ((i)[X]*(m)[0] + (i)[Y]*(m)[4]) * f; \
	(o)[Y] = ((i)[X]*(m)[1] + (i)[Y]*(m)[5]) * f; \
	(o)[Z] = ((i)[X]*(m)[2] + (i)[Y]*(m)[6]) * f; }

/* Compare two vectors for EXACT equality.  Use carefully. */
#define VEQUAL(a,b)	((a)[X]==(b)[X] && (a)[Y]==(b)[Y] && (a)[Z]==(b)[Z])

/* Macros to update min and max X,Y,Z values to contain a point */
#define V_MIN(r,s)	if( (r) > (s) ) r = (s)
#define V_MAX(r,s)	if( (r) < (s) ) r = (s)
#define VMIN(r,s)	{ V_MIN(r[X],s[X]); V_MIN(r[Y],s[Y]); V_MIN(r[Z],s[Z]); }
#define VMAX(r,s)	{ V_MAX(r[X],s[X]); V_MAX(r[Y],s[Y]); V_MAX(r[Z],s[Z]); }
#define VMINMAX( min, max, pt )	{ VMIN( min, pt ); VMAX( max, pt ); }

/* Divide out homogeneous parameter from hvect_t, creating vect_t */
#ifdef VECTORIZE
#define HDIVIDE(a,b)  \
	{ register int _i; \
	for(_i = 0; _i < 3; _i++) \
		(a)[_i] = (b)[_i] / (b)[H]; \
	}
#else
#define HDIVIDE(a,b)  \
	(a)[X] = (b)[X] / (b)[H];\
	(a)[Y] = (b)[Y] / (b)[H];\
	(a)[Z] = (b)[Z] / (b)[H];
#endif VECTORIZE

#endif VMATH_H
