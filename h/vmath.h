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
	/* In case <math.h> has not been included, define sqrt() here */
#	if __STDC__
		extern double sqrt(double);
#	else
#		if (defined(sgi) && defined(mips) && !defined(SGI4D_Rel2))
			/* What could SGI have been thinking of? */
			extern double sqrt(double);
#		else
			extern double sqrt();
#		endif
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
#define VSET(a,b,c,d)	(a)[X] = (b);\
			(a)[Y] = (c);\
			(a)[Z] = (d)

/* Set all elements of vector to same scalar value */
#define VSETALL(a,s)	(a)[X] = (a)[Y] = (a)[Z] = (s);

/* Transfer vector at `b' to vector at `a' */
#define VMOVE(a,b)	(a)[X] = (b)[X];\
			(a)[Y] = (b)[Y];\
			(a)[Z] = (b)[Z]

/* Transfer vector of length `n' at `b' to vector at `a' */
#define VMOVEN(a,b,n) \
	{ register int _vmove; \
	for(_vmove = 0; _vmove < (n); _vmove++) \
		(a)[_vmove] = (b)[_vmove]; \
	}

/* Reverse the direction of b and store it in a */
#define VREVERSE(a,b)	(a)[X] = -(b)[X]; \
			(a)[Y] = -(b)[Y]; \
			(a)[Z] = -(b)[Z];

/* Add vectors at `b' and `c', store result at `a' */
#ifdef VECTORIZE
#define VADD2(a,b,c) VADD2N(a,b,c, 3)
#else
#define VADD2(a,b,c)	(a)[X] = (b)[X] + (c)[X];\
			(a)[Y] = (b)[Y] + (c)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z]
#endif /* VECTORIZE */

/* Add vectors of length `n' at `b' and `c', store result at `a' */
#define VADD2N(a,b,c,n) \
	{ register int _vadd2; \
	for(_vadd2 = 0; _vadd2 < (n); _vadd2++) \
		(a)[_vadd2] = (b)[_vadd2] + (c)[_vadd2]; \
	}

/* Subtract vector at `c' from vector at `b', store result at `a' */
#ifdef VECTORIZE
#define VSUB2(a,b,c) 	VSUB2N(a,b,c, 3)
#else
#define VSUB2(a,b,c)	(a)[X] = (b)[X] - (c)[X];\
			(a)[Y] = (b)[Y] - (c)[Y];\
			(a)[Z] = (b)[Z] - (c)[Z]
#endif /* VECTORIZE */

/* Subtract `n' length vector at `c' from vector at `b', store result at `a' */
#define VSUB2N(a,b,c,n) \
	{ register int _vsub2; \
	for(_vsub2 = 0; _vsub2 < (n); _vsub2++) \
		(a)[_vsub2] = (b)[_vsub2] - (c)[_vsub2]; \
	}

/* Vectors:  A = B - C - D */
#ifdef VECTORIZE
#define VSUB3(a,b,c,d) VSUB3(a,b,c,d, 3)
#else
#define VSUB3(a,b,c,d)	(a)[X] = (b)[X] - (c)[X] - (d)[X];\
			(a)[Y] = (b)[Y] - (c)[Y] - (d)[Y];\
			(a)[Z] = (b)[Z] - (c)[Z] - (d)[Z]
#endif /* VECTORIZE */

/* Vectors:  A = B - C - D for vectors of length `n' */
#define VSUB3N(a,b,c,d,n) \
	{ register int _vsub3; \
	for(_vsub3 = 0; _vsub3 < (n); _vsub3++) \
		(a)[_vsub3] = (b)[_vsub3] - (c)[_vsub3] - (d)[_vsub3]; \
	}

/* Add 3 vectors at `b', `c', and `d', store result at `a' */
#ifdef VECTORIZE
#define VADD3(a,b,c,d) VADD3N(a,b,c,d, 3)
#else
#define VADD3(a,b,c,d)	(a)[X] = (b)[X] + (c)[X] + (d)[X];\
			(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z]
#endif /* VECTORIZE */

/* Add 3 vectors of length `n' at `b', `c', and `d', store result at `a' */
#define VADD3N(a,b,c,d,n) \
	{ register int _vadd3; \
	for(_vadd3 = 0; _vadd3 < (n); _vadd3++) \
		(a)[_vadd3] = (b)[_vadd3] + (c)[_vadd3] + (d)[_vadd3]; \
	}

/* Add 4 vectors at `b', `c', `d', and `e', store result at `a' */
#ifdef VECTORIZE
#define VADD4(a,b,c,d,e) VADD4N(a,b,c,d,e, 3)
#else
#define VADD4(a,b,c,d,e) (a)[X] = (b)[X] + (c)[X] + (d)[X] + (e)[X];\
			(a)[Y] = (b)[Y] + (c)[Y] + (d)[Y] + (e)[Y];\
			(a)[Z] = (b)[Z] + (c)[Z] + (d)[Z] + (e)[Z]
#endif /* VECTORIZE */

/* Add 4 `n' length vectors at `b', `c', `d', and `e', store result at `a' */
#define VADD4N(a,b,c,d,e,n) \
	{ register int _vadd4; \
	for(_vadd4 = 0; _vadd4 < (n); _vadd4++) \
		(a)[_vadd4] = (b)[_vadd4] + (c)[_vadd4] + (d)[_vadd4] + (e)[_vadd4];\
	}

/* Scale vector at `b' by scalar `c', store result at `a' */
#ifdef VECTORIZE
#define VSCALE(a,b,c) VSCALEN(a,b,c, 3)
#else
#define VSCALE(a,b,c)	(a)[X] = (b)[X] * (c);\
			(a)[Y] = (b)[Y] * (c);\
			(a)[Z] = (b)[Z] * (c)
#endif /* VECTORIZE */

/* Scale vector of length `n' at `b' by scalar `c', store result at `a' */
#define VSCALEN(a,b,c,n) \
	{ register int _vscale; \
	for(_vscale = 0; _vscale < (n); _vscale++) \
		(a)[_vscale] = (b)[_vscale] * (c); \
	}

/* Normalize vector `a' to be a unit vector */
#ifdef VECTORIZE
#define VUNITIZE(a) \
	{ register double _f; register int _vunitize; \
	_f = MAGNITUDE(a); \
	if( _f < 1.0e-10 ) _f = 0.0; else _f = 1.0/_f; \
	for(_vunitize = 0; _vunitize < 3; _vunitize++) \
		(a)[_vunitize] *= _f; \
	}
#else
#define VUNITIZE(a)	{ register double _f; _f = MAGNITUDE(a); \
			if( _f < 1.0e-10 ) _f = 0.0; else _f = 1.0/_f; \
			(a)[X] *= _f; (a)[Y] *= _f; (a)[Z] *= _f; }
#endif /* VECTORIZE */

/*
 *  Find the sum of two points, and scale the result.
 *  Often used to find the midpoint.
 */
#ifdef VECTORIZE
#define VADD2SCALE( o, a, b, s )	VADD2SCALEN( o, a, b, s, 3 )
#else
#define VADD2SCALE( o, a, b, s )	o[X] = ((a)[X] + (b)[X]) * (s); \
					o[Y] = ((a)[Y] + (b)[Y]) * (s); \
					o[Z] = ((a)[Z] + (b)[Z]) * (s);
#endif

#define VADD2SCALEN( o, a, b, n ) \
	{ register int _vadd2scale; \
	for( _vadd2scale = 0; _vadd2scale < (n); _vadd2scale++ ) \
		(o)[_vadd2scale] = ((a)[_vadd2scale] + (b)[_vadd2scale]) * (s); \
	}

/*
 *  Find the difference between two points, and scale result.
 *  Often used to compute bounding sphere radius given rpp points.
 */
#ifdef VECTORIZE
#define VSUB2SCALE( o, a, b, s )	VSUB2SCALEN( o, a, b, s, 3 )
#else
#define VSUB2SCALE( o, a, b, s )	o[X] = ((a)[X] + (b)[X]) * (s); \
					o[Y] = ((a)[Y] + (b)[Y]) * (s); \
					o[Z] = ((a)[Z] + (b)[Z]) * (s);
#endif

#define VSUB2SCALEN( o, a, b, n ) \
	{ register int _vsub2scale; \
	for( _vsub2scale = 0; _vsub2scale < (n); _vsub2scale++ ) \
		(o)[_vsub2scale] = ((a)[_vsub2scale] + (b)[_vsub2scale]) * (s); \
	}


/*
 *  Combine together several vectors, scaled by a scalar
 */
#ifdef VECTORIZE
#define VCOMB3(o, a,b, c,d, e,f)	VCOMB3N(o, a,b, c,d, e,f, 3)
#else
#define VCOMB3(o, a,b, c,d, e,f)	{\
	(o)[X] = (a) * (b)[X] + (c) * (d)[X] + (e) * (f)[X];\
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y];\
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z];}
#endif /* VECTORIZE */

#define VCOMB3N(o, a,b, c,d, e,f, n)	{\
	{ register int _vcomb3; \
	for(_vcomb3 = 0; _vcomb3 < (n); _vcomb3++) \
		(o)[_vcomb3] = (a) * (b)[_vcomb3] + (c) * (d)[_vcomb3] + (e) * (f)[_vcomb3]; \
	} }

#ifdef VECTORIZE
#define VCOMB2(o, a,b, c,d)	VCOMB2N(o, a,b, c,d, 3)
#else
#define VCOMB2(o, a,b, c,d)	{\
	(o)[X] = (a) * (b)[X] + (c) * (d)[X];\
	(o)[Y] = (a) * (b)[Y] + (c) * (d)[Y];\
	(o)[Z] = (a) * (b)[Z] + (c) * (d)[Z];}
#endif /* VECTORIZE */

#define VCOMB2N(o, a,b, c,d, n)	{\
	{ register int _vcomb2; \
	for(_vcomb2 = 0; _vcomb2 < (n); _vcomb2++) \
		(o)[_vcomb2] = (a) * (b)[_vcomb2] + (c) * (d)[_vcomb2]; \
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
	(a)[X] = (b)[X] + (c) * (d)[X] + (e) * (f)[X];\
	(a)[Y] = (b)[Y] + (c) * (d)[Y] + (e) * (f)[Y];\
	(a)[Z] = (b)[Z] + (c) * (d)[Z] + (e) * (f)[Z]
#endif /* VECTORIZE */

#define VJOIN2N(a,b,c,d,e,f,n)	\
	{ register int _vjoin2; \
	for(_vjoin2 = 0; _vjoin2 < (n); _vjoin2++) \
		(a)[_vjoin2] = (b)[_vjoin2] + (c) * (d)[_vjoin2] + (e) * (f)[_vjoin2]; \
	}

#ifdef VECTORIZE
#define VJOIN1(a,b,c,d)		VJOIN1N(a,b,c,d,3)
#else
#define VJOIN1(a,b,c,d) \
	(a)[X] = (b)[X] + (c) * (d)[X];\
	(a)[Y] = (b)[Y] + (c) * (d)[Y];\
	(a)[Z] = (b)[Z] + (c) * (d)[Z];
#endif /* VECTORIZE */

#define VJOIN1N(a,b,c,d,n) \
	{ register int _vjoin1; \
	for(_vjoin1 = 0; _vjoin1 < (n); _vjoin1++) \
		(a)[_vjoin1] = (b)[_vjoin1] + (c) * (d)[_vjoin1]; \
	}

/* Return scalar magnitude squared of vector at `a' */
#define MAGSQ(a)	( (a)[X]*(a)[X] + (a)[Y]*(a)[Y] + (a)[Z]*(a)[Z] )

/* Return scalar magnitude of vector at `a' */
#define MAGNITUDE(a)	sqrt( MAGSQ( a ) )

/* Store cross product of vectors at `b' and `c' in vector at `a' */
#define VCROSS(a,b,c)	(a)[X] = (b)[Y] * (c)[Z] - (b)[Z] * (c)[Y];\
			(a)[Y] = (b)[Z] * (c)[X] - (b)[X] * (c)[Z];\
			(a)[Z] = (b)[X] * (c)[Y] - (b)[Y] * (c)[X]

/* Compute dot product of vectors at `a' and `b' */
#define VDOT(a,b)	( (a)[X]*(b)[X] + (a)[Y]*(b)[Y] + (a)[Z]*(b)[Z] )

/* Print vector name and components on stdout */
#define VPRINT(a,b)	(void)fprintf(stderr,"%s (%g, %g, %g)\n", a, (b)[X], (b)[Y], (b)[Z])
#define HPRINT(a,b)	(void)fprintf(stderr,"%s (%g, %g, %g, %g)\n", a, (b)[X], (b)[Y], (b)[Z], (b)[3])

/* Vector element multiplication.  Really: diagonal matrix X vect */
#ifdef VECTORIZE
#define VELMUL(a,b,c) \
	{ register int _velmul; \
	for(_velmul = 0; _velmul < 3; _velmul++) \
		(a)[_velmul] = (b)[_velmul] * (c)[_velmul]; \
	}
#else
#define VELMUL(a,b,c) \
	(a)[X] = (b)[X] * (c)[X];\
	(a)[Y] = (b)[Y] * (c)[Y];\
	(a)[Z] = (b)[Z] * (c)[Z];
#endif /* VECTORIZE */

/* Apply the 3x3 part of a mat_t to a 3-tuple. */
#ifdef VECTORIZE
#define MAT3X3VEC(o,mat,vec) \
	{ register int _m3x3v; \
	for(_m3x3v = 0; _m3x3v < 3; _m3x3v++) \
		(o)[_m3x3v] = (mat)[4*_m3x3v+0]*(vec)[X] + \
			  (mat)[4*_m3x3v+1]*(vec)[Y] + \
			  (mat)[4*_m3x3v+2]*(vec)[Z]; \
	}
#else
#define MAT3X3VEC(o,mat,vec) \
	(o)[X] = (mat)[X]*(vec)[X]+(mat)[Y]*(vec)[Y] + (mat)[ 2]*(vec)[Z]; \
	(o)[Y] = (mat)[4]*(vec)[X]+(mat)[5]*(vec)[Y] + (mat)[ 6]*(vec)[Z]; \
	(o)[Z] = (mat)[8]*(vec)[X]+(mat)[9]*(vec)[Y] + (mat)[10]*(vec)[Z];
#endif /* VECTORIZE */

/* Multiply a 3-tuple by the 3x3 part of a mat_t. */
#ifdef VECTORIZE
#define VEC3X3MAT(o,i,m) \
	{ register int _v3x3m; \
	for(_v3x3m = 0; _v3x3m < 3; _v3x3m++) \
		(o)[_v3x3m] = (i)[X]*(m)[_v3x3m] + \
			(i)[Y]*(m)[_v3x3m+4] + \
			(i)[Z]*(m)[_v3x3m+8]; \
	}
#else
#define VEC3X3MAT(o,i,m) \
	(o)[X] = (i)[X]*(m)[X] + (i)[Y]*(m)[4] + (i)[Z]*(m)[8]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5] + (i)[Z]*(m)[9]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6] + (i)[Z]*(m)[10];
#endif /* VECTORIZE */

/* Apply the 3x3 part of a mat_t to a 2-tuple (Z part=0). */
#ifdef VECTORIZE
#define MAT3X2VEC(o,mat,vec) \
	{ register int _m3x2v; \
	for(_m3x2v = 0; _m3x2v < 3; _m3x2v++) \
		(o)[_m3x2v] = (mat)[4*_m3x2v]*(vec)[X] + \
			(mat)[4*_m3x2v+1]*(vec)[Y]; \
	}
#else
#define MAT3X2VEC(o,mat,vec) \
	(o)[X] = (mat)[0]*(vec)[X] + (mat)[Y]*(vec)[Y]; \
	(o)[Y] = (mat)[4]*(vec)[X] + (mat)[5]*(vec)[Y]; \
	(o)[Z] = (mat)[8]*(vec)[X] + (mat)[9]*(vec)[Y];
#endif /* VECTORIZE */

/* Multiply a 2-tuple (Z=0) by the 3x3 part of a mat_t. */
#ifdef VECTORIZE
#define VEC2X3MAT(o,i,m) \
	{ register int _v2x3m; \
	for(_v2x3m = 0; _v2x3m < 3; _v2x3m++) \
		(o)[_v2x3m] = (i)[X]*(m)[_v2x3m] + (i)[Y]*(m)[2*_v2x3m]; \
	}
#else
#define VEC2X3MAT(o,i,m) \
	(o)[X] = (i)[X]*(m)[0] + (i)[Y]*(m)[4]; \
	(o)[Y] = (i)[X]*(m)[1] + (i)[Y]*(m)[5]; \
	(o)[Z] = (i)[X]*(m)[2] + (i)[Y]*(m)[6];
#endif /* VECTORIZE */

/* Apply a 4x4 matrix to a 3-tuple which is an absolute Point in space */
#ifdef VECTORIZE
#define MAT4X3PNT(o,m,i) \
	{ register double f; \
	register int _i_m4x3p, _j_m4x3p; \
	f = 0.0; \
	for(_j_m4x3p = 0; _j_m4x3p < 3; _j_m4x3p++)  \
		f += (m)[_j_m4x3p+12] * (i)[_j_m4x3p]; \
	f = 1.0/(f + (m)[15]); \
	for(_i_m4x3p = 0; _i_m4x3p < 3; _i_m4x3p++) \
		(o)[_i_m4x3p] = 0.0; \
	for(_i_m4x3p = 0; _i_m4x3p < 3; _i_m4x3p++)  { \
		for(_j_m4x3p = 0; _j_m4x3p < 3; _j_m4x3p++) \
			(o)[_i_m4x3p] += (m)[_j_m4x3p+4*_i_m4x3p] * (i)[_j_m4x3p]; \
	} \
	for(_i_m4x3p = 0; _i_m4x3p < 3; _i_m4x3p++)  { \
		(o)[_i_m4x3p] = ((o)[_i_m4x3p] + (m)[4*_i_m4x3p+3]) * f; \
	} }
#else
#define MAT4X3PNT(o,m,i) \
	{ register double f; \
	f = 1.0/((m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]);\
	(o)[X]=((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[3]) * f;\
	(o)[Y]=((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[7]) * f;\
	(o)[Z]=((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11])* f;}
#endif /* VECTORIZE */

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
	{ register int _i_m4x4p, _j_m4x4p; \
	for(_i_m4x4p = 0; _i_m4x4p < 4; _i_m4x4p++) \
		(o)[_i_m4x4p] = 0.0; \
	for(_i_m4x4p = 0; _i_m4x4p < 4; _i_m4x4p++) \
		for(_j_m4x4p = 0; _j_m4x4p < 4; _j_m4x4p++) \
			(o)[_i_m4x4p] += (m)[_j_m4x4p+4*_i_m4x4p] * (i)[_j_m4x4p]; \
	}
#else
#define MAT4X4PNT(o,m,i) \
	(o)[X]=(m)[ 0]*(i)[X] + (m)[ 1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[ 3]*(i)[H];\
	(o)[Y]=(m)[ 4]*(i)[X] + (m)[ 5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[ 7]*(i)[H];\
	(o)[Z]=(m)[ 8]*(i)[X] + (m)[ 9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11]*(i)[H];\
	(o)[H]=(m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]*(i)[H];
#endif /* VECTORIZE */

/* Apply a 4x4 matrix to a 3-tuple which is a relative Vector in space */
#ifdef VECTORIZE
#define MAT4X3VEC(o,m,i) \
	{ register double f; \
	register int _i_m4x3v, _j_m4x3v; \
	f = 1.0/((m)[15]); \
	for(_i_m4x3v = 0; _i_m4x3v < 3; _i_m4x3v++) \
		(o)[_i_m4x3v] = 0.0; \
	for(_i_m4x3v = 0; _i_m4x3v < 3; _i_m4x3v++) { \
		for(_j_m4x3v = 0; _j_m4x3v < 3; _j_m4x3v++) \
			(o)[_i_m4x3v] += (m)[_j_m4x3v+4*_i_m4x3v] * (i)[_j_m4x3v]; \
	} \
	for(_i_m4x3v = 0; _i_m4x3v < 3; _i_m4x3v++) { \
		(o)[_i_m4x3v] *= f; \
	} }
#else
#define MAT4X3VEC(o,m,i) \
	{ register double f;	f = 1.0/((m)[15]);\
	(o)[X] = ((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z]) * f; \
	(o)[Y] = ((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z]) * f; \
	(o)[Z] = ((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z]) * f; }
#endif /* VECTORIZE */

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
	{ register int _hdivide; \
	for(_hdivide = 0; _hdivide < 3; _hdivide++) \
		(a)[_hdivide] = (b)[_hdivide] / (b)[H]; \
	}
#else
#define HDIVIDE(a,b)  \
	(a)[X] = (b)[X] / (b)[H];\
	(a)[Y] = (b)[Y] / (b)[H];\
	(a)[Z] = (b)[Z] / (b)[H];
#endif /* VECTORIZE */

#endif /* VMATH_H */
