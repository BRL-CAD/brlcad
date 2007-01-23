/*                          A N I M . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup anim */
/** @{ */
/** @file anim.h
 *
 *  @author
 *	Carl J. Nuzman
 *
 * @par Source
 *      The U. S. Army Research Laboratory
 *@n      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * 	Be sure to include vmath.h before this file.
 */

#include "compat4.h"

#define ANIM_STEER_NEW	0
#define ANIM_STEER_END	1

#define DTOR    M_PI/180.0
#define RTOD	180.0/M_PI

#define VSUBUNIT(a,b,c) {VSUB2(a,b,c);\
                        VUNITIZE(a);}
#define FVSCAN(f,a)	fscanf(f,"%lf %lf %lf", (a),(a)+1,(a)+2)
#define FMATSCAN(f,m)	{FVSCAN(f,(m)); FVSCAN(f,(m)+4);\
			 FVSCAN(f,(m)+8); FVSCAN(f,(m)+12);}
#define VSCAN(a)	scanf("%lf %lf %lf", (a),(a)+1,(a)+2)
#define VPRINTS(t,a)	printf("%s %f %f %f ",t,(a)[0],(a)[1],(a)[2])
#define VPRINTN(t,a)	printf("%s %f %f %f\n",t,(a)[0],(a)[1],(a)[2])

#define MAT_MOVE(m,n)	MAT_COPY(m,n)

/***** 3x3 matrix format *****/

typedef fastf_t  mat3_t[9];

#define MAT3ZERO(m) 	{\
	int _j;	for(_j=0;_j<9;_j++) m[_j]=0.0;}

#define MAT3IDN(m)	{\
	int _j;	for(_j=0;_j<9;_j++) m[_j]=0.0;\
	m[0] = m[4] = m[8] = 1.0;}

#define MAT3MUL(o,a,b)	{\
	(o)[0] = (a)[0]*(b)[0] + (a)[1]*(b)[3] + (a)[2]*(b)[6];\
	(o)[1] = (a)[0]*(b)[1] + (a)[1]*(b)[4] + (a)[2]*(b)[7];\
	(o)[2] = (a)[0]*(b)[2] + (a)[1]*(b)[5] + (a)[2]*(b)[8];\
	(o)[3] = (a)[3]*(b)[0] + (a)[4]*(b)[3] + (a)[5]*(b)[6];\
	(o)[4] = (a)[3]*(b)[1] + (a)[4]*(b)[4] + (a)[5]*(b)[7];\
	(o)[5] = (a)[3]*(b)[2] + (a)[4]*(b)[5] + (a)[5]*(b)[8];\
	(o)[6] = (a)[6]*(b)[0] + (a)[7]*(b)[3] + (a)[8]*(b)[6];\
	(o)[7] = (a)[6]*(b)[1] + (a)[7]*(b)[4] + (a)[8]*(b)[7];\
	(o)[8] = (a)[6]*(b)[2] + (a)[7]*(b)[5] + (a)[8]*(b)[8];}

#define MAT3SUM(o,a,b)	{\
	int _j; for(_j=0;_j<9;_j++) (o)[_j]=(a)[_j]+(b)[_j];}

#define MAT3DIF(o,a,b)	{\
	int _j; for(_j=0;_j<9;_j++) (o)[_j]=(a)[_j]-(b)[_j];}

#define MAT3SCALE(o,a,s)	{\
	int _j; for(_j=0;_j<9;_j++) (o)[_j]=(a)[_j] * (s);}

#define MAT3MOVE(o,a)	{\
	int _j; for(_j=0;_j<9;_j++) (o)[_j] = (a)[_j];}

#define MAT3XVEC(u,m,v)	{\
	(u)[0] = (m)[0]*(v)[0] + (m)[1]*(v)[1] + (m)[2]*(v)[2];\
	(u)[1] = (m)[3]*(v)[0] + (m)[4]*(v)[1] + (m)[5]*(v)[2];\
	(u)[2] = (m)[6]*(v)[0] + (m)[7]*(v)[1] + (m)[8]*(v)[2];}

#define MAT3TO4(o,i)	{\
	(o)[0] = (i)[0];\
	(o)[1] = (i)[1];\
	(o)[2] = (i)[2];\
	(o)[4] = (i)[3];\
	(o)[5] = (i)[4];\
	(o)[6] = (i)[5];\
	(o)[8] = (i)[6];\
	(o)[9] = (i)[7];\
	(o)[10] = (i)[8];\
	(o)[3]=(o)[7]=(o)[11]=(o)[12]=(o)[13]=(o)[14]=0.0;\
	(o)[15]=1.0;}

#define MAT4TO3(o,i)	{\
	(o)[0] = (i)[0];\
	(o)[1] = (i)[1];\
	(o)[2] = (i)[2];\
	(o)[3] = (i)[4];\
	(o)[4] = (i)[5];\
	(o)[5] = (i)[6];\
	(o)[6] = (i)[8];\
	(o)[7] = (i)[9];\
	(o)[8] = (i)[10];}


/* tilde matrix: [M]a = v X a */
#define MAKE_TILDE(m,v)	{\
	MAT3ZERO(m);\
	m[1]= -v[2];	m[2]=v[1];	m[3]= v[2];\
	m[5]= -v[0];	m[6]= -v[1];	m[7]= v[0];}

/* a = Ix Iy Iz    b = Ixy Ixz Iyz*/
#define INERTIAL_MAT3(m,a,b)	{\
	(m)[0] =  (a)[0]; (m)[1] = -(b)[0]; (m)[2] = -(b)[1];\
	(m)[3] = -(b)[0]; (m)[4] =  (a)[1]; (m)[5] = -(b)[2];\
	(m)[6] = -(b)[1]; (m)[7] = -(b)[2]; (m)[8]=  (a)[2];}



/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

