#ifndef _UTIL_MATH_H
#define _UTIL_MATH_H

#include <math.h>
#include "tie.h"
#include "util_rand.h"

#define MATH_PI		3.14159265f
#define MATH_2PI	6.28318530f
#define	MATH_RAD2DEG	57.2957795f
#define	MATH_DEG2RAD	0.0174532925f
#define	MATH_INV180	0.0055555556f



#define MATH_VEC_MAG(_a, _b) { \
	_a = sqrt(_b.v[0]*_b.v[0] + _b.v[1]*_b.v[1] + _b.v[2]*_b.v[2]); }

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define MATH_VEC_TRANSFORM(_a, _b, _c) { \
	tfloat	w; \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]) + _c[12]; \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]) + _c[13]; \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]) + _c[14]; \
	w = (_b.v[0] * _c[3]) + (_b.v[1] * _c[7]) + (_b.v[2] * _c[11]) + _c[15]; \
        w = w ? 1/w : 1.0; \
        _a.v[0] *= w; _a.v[1] *= w; _a.v[2] *= w; }

/* _a is transformed vertex, _b is input vertex, _c is 4x4 transformation matrix */
#define MATH_VEC_TRANSFORM_ROTATE(_a, _b, _c) { \
	_a.v[0] = (_b.v[0] * _c[0]) + (_b.v[1] * _c[4]) + (_b.v[2] * _c[8]); \
	_a.v[1] = (_b.v[0] * _c[1]) + (_b.v[1] * _c[5]) + (_b.v[2] * _c[9]); \
	_a.v[2] = (_b.v[0] * _c[2]) + (_b.v[1] * _c[6]) + (_b.v[2] * _c[10]); }

/* _a is reflected ray, _b is incident ray, _c is normal */
#define	MATH_VEC_REFLECT(_a, _b, _c) { \
	tfloat _d; \
	MATH_VEC_DOT(_d, _b, _c); \
	MATH_VEC_MUL_SCALAR(_a, _c, 2.0*_d); \
	MATH_VEC_SUB(_a, _b, _a); \
	MATH_VEC_UNITIZE(_a); }
	

extern	void	math_mat_ident(tfloat *M, int S);						/* Identity Matrix */
extern	void	math_mat_mult(tfloat *A, int Ar, int Ac, tfloat *B, int Br, int Bc, tfloat *C);	/* Multiply 2 Matrices */
extern	void	math_mat_invert(tfloat *D, tfloat *M, int S);					/* Invert */

#endif
