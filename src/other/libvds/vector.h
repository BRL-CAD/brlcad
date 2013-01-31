/*
 * File:	vector.h
 * Description:	Vector macros for the view-dependent simplification package.
 *
 * Copyright 1999 The University of Virginia.  All Rights Reserved.  Disclaimer
 * and copyright notice are given in full below and apply to this entire file.
 */
#ifndef _VECTOR_H
#define _VECTOR_H

#define VEC3_EQUAL(a,b) ((a)[0]==(b)[0]&&(a)[1]==(b)[1]&&(a)[2]==(b)[2])

#define VEC3_COPY(dst,src) ((dst)[0]=(src)[0], \
                            (dst)[1]=(src)[1], \
                            (dst)[2]=(src)[2])

#define VEC3_ADD(dst,a,b) 	{(dst)[0]=(a)[0]+(b)[0];\
                                 (dst)[1]=(a)[1]+(b)[1];\
                                 (dst)[2]=(a)[2]+(b)[2];}

                            
#define VEC3_SUBTRACT(dst,a,b) {(dst)[0]=(a)[0]-(b)[0];\
                                (dst)[1]=(a)[1]-(b)[1];\
                                (dst)[2]=(a)[2]-(b)[2];}

#define VEC3_SCALE(dst,scal,vec) {(dst)[0]=(vec)[0]*(scal);\
                                  (dst)[1]=(vec)[1]*(scal);\
                                  (dst)[2]=(vec)[2]*(scal);}

#define VEC3_AVERAGE(dst,a,b) {(dst)[0]=((a)[0]+(b)[0])/2.0;\
                               (dst)[1]=((a)[1]+(b)[1])/2.0;\
                               (dst)[2]=((a)[2]+(b)[2])/2.0;}
                                
#define VEC3_DOT(_v0, _v1) ((_v0)[0] * (_v1)[0] +    \
                            (_v0)[1] * (_v1)[1] +    \
                            (_v0)[2] * (_v1)[2])

#define VEC3_CROSS(dst,a,b) {(dst)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1];\
                             (dst)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2];\
                             (dst)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0];}

#define VEC3_LENGTH_SQUARED(v) ((v)[0]*(v)[0] + (v)[1]*(v)[1] + (v)[2]*(v)[2])

#define VEC3_LENGTH(v) (sqrt(VEC3_LENGTH_SQUARED(v)))

#define VEC3_DISTANCE_SQUARED(a,b) (((a)[0]-(b)[0])*((a)[0]-(b)[0]) +	\
				    ((a)[1]-(b)[1])*((a)[1]-(b)[1]) +	\
				    ((a)[2]-(b)[2])*((a)[2]-(b)[2]))

#define VEC3_DISTANCE(a,b) (sqrt(VEC3_DISTANCE_SQUARED((a),(b))))

#define VEC3_NORMALIZE(v) {static vdsFloat _n;				\
                           _n = 1.0/sqrt((v)[0]*(v)[0] +		\
				         (v)[1]*(v)[1] +		\
					 (v)[2]*(v)[2]);		\
			   (v)[0]*=_n;					\
			   (v)[1]*=_n;					\
			   (v)[2]*=_n;}

#define VEC3_FIND_MAX(dst,a,b) {(dst)[0]=(a)[0]>(b)[0]?(a)[0]:(b)[0];\
                                (dst)[1]=(a)[1]>(b)[1]?(a)[1]:(b)[1];\
                                (dst)[2]=(a)[2]>(b)[2]?(a)[2]:(b)[2];}

#define VEC3_FIND_MIN(dst,a,b) {(dst)[0]=(a)[0]<(b)[0]?(a)[0]:(b)[0];\
                                (dst)[1]=(a)[1]<(b)[1]?(a)[1]:(b)[1];\
                                (dst)[2]=(a)[2]<(b)[2]?(a)[2]:(b)[2];}

#define BYTE3_EQUAL(a,b) ((a)[0]==(b)[0]&&(a)[1]==(b)[1]&&(a)[2]==(b)[2])

#define BYTE3_COPY(dst,src) ((dst)[0]=(src)[0], \
                            (dst)[1]=(src)[1], \
                            (dst)[2]=(src)[2])
#endif /* _VECTOR_H */
