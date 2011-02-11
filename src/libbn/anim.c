/*                          A N I M . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "anim.h"


#define NORMAL 0
#define ERROR1 1
#define ERROR2 2


void
anim_v_permute(mat_t m)
{
    int i;
    fastf_t store;

    for (i=0; i<9; i+=4) {
	store = m[i];
	m[i] = -m[i+1];
	m[i+1] = m[i+2];
	m[i+2] = -store;
    }
}


void
anim_v_unpermute(mat_t m)
{
    int i;
    fastf_t store;

    for (i=0; i<9; i+=4) {
	store = m[i+2];
	m[i+2] = m[i+1];
	m[i+1] = -m[i];
	m[i] = -store;
    }
}


void
anim_tran(mat_t m)
{
    int i;
    fastf_t store;

    int src[6] = { 1, 2, 3, 6, 7, 11 };
    int dst[6] = { 4, 8, 12, 9, 13, 14};

    for (i=0; i<6; i++) {
	store = m[dst[i]];
	m[dst[i]] = m[src[i]];
	m[src[i]] = store;
    }
}


/***************************************
 *ANIM_MAT2* - Conversions from matrices
 ***************************************/

int
anim_mat2zyx(const mat_t viewrot, vect_t angle)
{
    int i, return_value, id_x, id_z;
    fastf_t sin_x, sin_z, cos_x, cos_z, big_x, big_z;
    static fastf_t previous[3];

    if (ZERO(viewrot[1]) && ZERO(viewrot[0])) {
	return_value = ERROR1;
	angle[0] = 0.0;
	angle[2] = atan2(viewrot[4], viewrot[5]);
    } else {
	return_value = NORMAL;
	angle[2] = atan2(-viewrot[1], viewrot[0]);
	angle[0] = atan2(-viewrot[6], viewrot[10]);
    }

    sin_x = sin(angle[0]);
    sin_z = sin(angle[2]);
    cos_x = cos(angle[0]);
    cos_z = cos(angle[2]);

    /* in principle, we can use the sin_x or cos_x with sin_z or cos_z
     * to figure out angle[1], as long as they are non-zero. To avoid
     * ill-conditioning effects, we choose the two that are greatest
     * in absolute value
     */

    id_z  = (fabs(sin_z) > fabs(cos_z)) ? 1 : 0;
    big_z = id_z ? sin_z : cos_z;
    id_x  = (fabs(sin_x) > fabs(cos_x)) ? 1 : 0;
    big_x = id_x ? sin_x : cos_x;

    if (fabs(big_x*big_z) < VDIVIDE_TOL) {
	/* this should be impossible*/
	/* unable to calculate pitch*/
	return ERROR2;
    } else if (id_x && (!id_z))
	angle[1]=atan2((viewrot[4] - cos_x*sin_z)/(sin_x*cos_z), -viewrot[6]/sin_x);
    else if ((!id_x) && (!id_z))
	angle[1]=atan2((-viewrot[8] + sin_x*sin_z)/(cos_x*cos_z), viewrot[0]/cos_z);
    else if (id_x && id_z)
	angle[1]=atan2((-viewrot[5] + cos_x*cos_z)/(sin_x*sin_z), -viewrot[1]/sin_z);
    else if ((!id_x) && id_z)
	angle[1]=atan2((viewrot[9] - sin_x*cos_z)/(cos_x*sin_z), viewrot[10]/cos_x);


    /* assume the smallest possible arc-length from frame to frame */
    for (i=0; i<3; i++) {
	while ((angle[i] - previous[i]) > M_PI)
	    angle[i] -= (2.0*M_PI);
	while ((previous[i] - angle[i]) > M_PI)
	    angle[i] += (2.0*M_PI);
	previous[i] = angle[i];
    }

    return return_value;
}


int
anim_mat2ypr(mat_t viewrot, vect_t angle)
{
    int i, return_value, id_y, id_r;
    fastf_t sin_y, sin_r, cos_y, cos_r, big_y, big_r;
    static fastf_t prev_angle[3];

    if (ZERO(viewrot[9]) && ZERO(viewrot[10])) {
	return_value = ERROR1;
	angle[2] = 0.0;
	angle[0] = atan2(-viewrot[1], viewrot[5]);
    } else {
	return_value = NORMAL;
	angle[0] = atan2(viewrot[4], viewrot[0]);
	angle[2] = atan2(viewrot[9], viewrot[10]);
    }

    sin_y = sin(angle[0]);
    sin_r = sin(angle[2]);
    cos_y = cos(angle[0]);
    cos_r = cos(angle[2]);

    /* in principle, we can use sin_y or cos_y with sin_r or cos_r to
     * figure out angle[1], as long as they are non-zero. To avoid
     * ill-conditioning effects, we choose the two that are greatest
     * in absolute value
     */

    id_y  = (fabs(sin_y) > fabs(cos_y)) ? 1 : 0;
    big_y = id_y ? sin_y : cos_y;
    id_r  = (fabs(sin_r) > fabs(cos_r)) ? 1 : 0;
    big_r = id_r ? sin_r : cos_r;

    if (fabs(big_y*big_r) < VDIVIDE_TOL) {
	/* this should not happen */
	/* unable to calculate pitch*/
	return ERROR2;
    } else if ((!id_y) && id_r)
	angle[1] = atan2(-(viewrot[1]+sin_y*cos_r)/(cos_y*sin_r), viewrot[9]/sin_r);
    else if (id_y && (!id_r))
	angle[1] = atan2(-(viewrot[6]+cos_y*sin_r)/(sin_y*cos_r), viewrot[10]/cos_r);
    else if (id_y && id_r)
	angle[1] = atan2(-(viewrot[5]-cos_y*cos_r)/(sin_y*sin_r), viewrot[4]/sin_y);
    else if ((!id_y) && (!id_r))
	angle[1] = atan2(-(viewrot[2]-sin_y*sin_r)/(cos_y*cos_r), viewrot[0]/cos_y);

    /* assume the smallest possible arc-length from frame to frame */
    for (i=0; i<3; i++) {
	while ((angle[i] - prev_angle[i]) > M_PI)
	    angle[i] -= (2.0*M_PI);
	while ((prev_angle[i] - angle[i]) > M_PI)
	    angle[i] += (2.0*M_PI);
	prev_angle[i] = angle[i];
    }

    return return_value;
}


int
anim_mat2quat(quat_t quat, const mat_t viewrot)
{
    int i;
    fastf_t qdiff[4], square, mag1, mag2;
    static fastf_t prev_quat[4];

    square = 0.25 * (1 + viewrot[0] + viewrot[5] + viewrot[10]);
    if (!ZERO(square)) {
	quat[W] = sqrt(square);
	quat[X] = 0.25 * (viewrot[9] - viewrot[6])/ quat[W];
	quat[Y] = 0.25 * (viewrot[2] - viewrot[8])/ quat[W];
	quat[Z] = 0.25 * (viewrot[4] - viewrot[1])/ quat[W];
    } else {
	quat[W] = 0.0;
	square = -0.5 * (viewrot[5] + viewrot[10]);
	if (!ZERO(square)) {
	    quat[X] = sqrt(square);
	    quat[Y] = 0.5 * viewrot[4] / quat[X];
	    quat[Z] = 0.5 * viewrot[8] / quat[X];
	} else {
	    quat[X] = 0.0;
	    square = 0.5 * (1 - viewrot[10]);
	    if (!ZERO(square)) {
		quat[Y] = sqrt(square);
		quat[Z] = 0.5 * viewrot[9]/ quat[Y];
	    } else {
		quat[Y] = 0.0;
		quat[Z] = 1.0;
	    }
	}
    }

    /* quaternions on opposite sides of a four-dimensional sphere are
       equivalent. Take the quaternion closest to the previous one */

    for (i=0; i<4; i++)
	qdiff[i] = prev_quat[i] - quat[i];
    mag1 = QMAGSQ(qdiff);
    for (i=0; i<4; i++)
	qdiff[i] = prev_quat[i] + quat[i];
    mag2 = QMAGSQ(qdiff);

    for (i=0; i<4; i++) {
	if (mag1 > mag2)  /* inverse of quat would be closer */
	    quat[i] = -quat[i];
	prev_quat[i] = quat[i];
    }

    return 1;
}


/***************************************
 *ANIM_*2MAT - Conversions to matrices
 ***************************************/

void
anim_ypr2mat(mat_t m, const vect_t a)
{
    fastf_t cos_y, cos_p, cos_r, sin_y, sin_p, sin_r;

    cos_y = cos(a[0]);
    cos_p = cos(a[1]);
    cos_r = cos(a[2]);
    sin_y = sin(a[0]);
    sin_p = sin(a[1]);
    sin_r = sin(a[2]);

    m[0] =	 cos_y*cos_p;
    m[1] =	 -cos_y*sin_p*sin_r-sin_y*cos_r;
    m[2] =	 -cos_y*sin_p*cos_r+sin_y*sin_r;
    m[3] =	0;
    m[4] = 	sin_y*cos_p;
    m[5] =	-sin_y*sin_p*sin_r+cos_y*cos_r;
    m[6] =	-sin_y*sin_p*cos_r-cos_y*sin_r;
    m[7] =	0;
    m[8]= 	sin_p;
    m[9] = 	cos_p*sin_r;
    m[10] = cos_p*cos_r;
    m[11] =	0.0;
    m[12] =	0.0;
    m[13] =	0.0;
    m[14] =	0.0;
    m[15] =	1.0;
}


void
anim_ypr2vmat(mat_t m, const vect_t a)
{
    fastf_t cos_y, cos_p, cos_r, sin_y, sin_p, sin_r;

    cos_y = cos(a[0]);
    cos_p = cos(a[1]);
    cos_r = cos(a[2]);
    sin_y = sin(a[0]);
    sin_p = sin(a[1]);
    sin_r = sin(a[2]);

    m[0] =    -cos_y*sin_p*sin_r-sin_y*cos_r;
    m[1] =    -sin_y*sin_p*sin_r+cos_y*cos_r;
    m[2] =     cos_p*sin_r;
    m[3] =     0;
    m[4] =    -cos_y*sin_p*cos_r+sin_y*sin_r;
    m[5] =    -sin_y*sin_p*cos_r-cos_y*sin_r;
    m[6] =     cos_p*cos_r;
    m[7] =     0;
    m[8] =     cos_y*cos_p;
    m[9] =     sin_y*cos_p;
    m[10] =    sin_p;
    m[11] =	   0.0;
    m[12] =	   0.0;
    m[13] =	   0.0;
    m[14] =	   0.0;
    m[15] =	   1.0;
}


void
anim_y_p_r2mat(mat_t m, double y, double p, double r)
{
    fastf_t cos_y = cos(y);
    fastf_t sin_y = sin(y);
    fastf_t cos_p = cos(p);
    fastf_t sin_p = sin(p);
    fastf_t cos_r = cos(r);
    fastf_t sin_r = sin(r);

    m[0] = cos_y*cos_p;
    m[1] = -cos_y*sin_p*sin_r-sin_y*cos_r;
    m[2] = -cos_y*sin_p*cos_r+sin_y*sin_r;
    m[4] = sin_y*cos_p;
    m[5] = -sin_y*sin_p*sin_r+cos_y*cos_r;
    m[6] = -sin_y*sin_p*cos_r-cos_y*sin_r;
    m[8]= sin_p;
    m[9] = cos_p*sin_r;
    m[10] = cos_p*cos_r;
    m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
    m[15]=1;
}


void
anim_dy_p_r2mat(mat_t m, double y, double p, double r)
{
    fastf_t radian_yaw = y*(M_PI*0.0055555555556);
    fastf_t radian_pitch = p*(M_PI*0.0055555555556);
    fastf_t radian_roll = r*(M_PI*0.0055555555556);

    fastf_t cos_y = cos(radian_yaw);
    fastf_t sin_y = sin(radian_yaw);
    fastf_t cos_p = cos(radian_pitch);
    fastf_t sin_p = sin(radian_pitch);
    fastf_t cos_r = cos(radian_roll);
    fastf_t sin_r = sin(radian_roll);

    m[0] = cos_y*cos_p;
    m[1] = -cos_y*sin_p*sin_r-sin_y*cos_r;
    m[2] = -cos_y*sin_p*cos_r+sin_y*sin_r;
    m[4] = sin_y*cos_p;
    m[5] = -sin_y*sin_p*sin_r+cos_y*cos_r;
    m[6] = -sin_y*sin_p*cos_r-cos_y*sin_r;
    m[8]= sin_p;
    m[9] = cos_p*sin_r;
    m[10] = cos_p*cos_r;
    m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
    m[15]=1;
}


void
anim_dy_p_r2vmat(mat_t m, double yaw, double pch, double rll)
{

    float ryaw = yaw*(M_PI*0.0055555555556);
    float rpch = pch*(M_PI*0.0055555555556);
    float rrll = rll*(M_PI*0.0055555555556);

    float cos_y = cos(ryaw);
    float sin_y = sin(ryaw);
    float cos_p = cos(rpch);
    float sin_p = sin(rpch);
    float cos_r = cos(rrll);
    float sin_r = sin(rrll);

    m[0] = -cos_y*sin_p*sin_r-sin_y*cos_r;
    m[1] = -sin_y*sin_p*sin_r+cos_y*cos_r;
    m[2] = cos_p*sin_r;
    m[4] = -cos_y*sin_p*cos_r+sin_y*sin_r;
    m[5] = -sin_y*sin_p*cos_r-cos_y*sin_r;
    m[6] = cos_p*cos_r;
    m[8] = cos_y*cos_p;
    m[9] = sin_y*cos_p;
    m[10]= sin_p;
    m[3]=m[7]=m[11]=0;
    m[12]=m[13]=m[14]=0;
    m[15]=1;

}


void
anim_x_y_z2mat(mat_t m, double x, double y, double z)
{
    fastf_t cosx = cos(x);
    fastf_t sinx = sin(x);
    fastf_t cosy = cos(y);
    fastf_t siny = sin(y);
    fastf_t cosz = cos(z);
    fastf_t sinz = sin(z);

    m[0] = cosz*cosy;
    m[1] = cosz*siny*sinx-sinz*cosx;
    m[2] = cosz*siny*cosx+sinz*sinx;
    m[4] = sinz*cosy;
    m[5] = sinz*siny*sinx+cosz*cosx;
    m[6] = sinz*siny*cosx-cosz*sinx;
    m[8] = -siny;
    m[9] = cosy*sinx;
    m[10] = cosy*cosx;
    m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
    m[15]=1;
}


void
anim_dx_y_z2mat(mat_t m, double x, double y, double z)
{
    fastf_t cosx, cosy, cosz, sinx, siny, sinz;

    x *= (M_PI*0.0055555555556);
    y *= (M_PI*0.0055555555556);
    z *= (M_PI*0.0055555555556);

    cosx = cos(x);
    sinx = sin(x);
    cosy = cos(y);
    siny = sin(y);
    cosz = cos(z);
    sinz = sin(z);

    m[0] = cosz*cosy;
    m[1] = cosz*siny*sinx-sinz*cosx;
    m[2] = cosz*siny*cosx+sinz*sinx;
    m[4] = sinz*cosy;
    m[5] = sinz*siny*sinx+cosz*cosx;
    m[6] = sinz*siny*cosx-cosz*sinx;
    m[8] = -siny;
    m[9] = cosy*sinx;
    m[10] = cosy*cosx;
    m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0.0;
    m[15]=1.0;
}


void
anim_zyx2mat(mat_t m, const vect_t a)
{
    fastf_t cosX, cosY, cosZ, sinX, sinY, sinZ;

    cosX = cos(a[0]);
    cosY = cos(a[1]);
    cosZ = cos(a[2]);
    sinX = sin(a[0]);
    sinY = sin(a[1]);
    sinZ = sin(a[2]);

    m[0] =     cosY*cosZ;
    m[1] =    -cosY*sinZ;
    m[2] =     sinY;
    m[3] =     0;
    m[4] =     cosX*sinZ + sinX*sinY*cosZ;
    m[5] =     cosX*cosZ - sinX*sinY*sinZ;
    m[6] =    -sinX*cosY;
    m[7] =     0;
    m[8] =     sinX*sinZ - cosX*sinY*cosZ;
    m[9] =     sinX*cosZ + cosX*sinY*sinZ;
    m[10] =    cosX*cosY;
    m[11] =	   0.0;
    m[12] =	   0.0;
    m[13] =	   0.0;
    m[14] =	   0.0;
    m[15] =	   1.0;

}


void
anim_z_y_x2mat(mat_t m, double x, double y, double z)
{
    fastf_t cosx = cos(x);
    fastf_t sinx = sin(x);
    fastf_t cosy = cos(y);
    fastf_t siny = sin(y);
    fastf_t cosz = cos(z);
    fastf_t sinz = sin(z);

    m[0] =  cosy*cosz;
    m[1] = -cosy*sinz;
    m[2] =  siny;
    m[4] =  cosx*sinz + sinx*siny*cosz;
    m[5] =  cosx*cosz - sinx*siny*sinz;
    m[6] = -sinx*cosy;
    m[8] =  sinx*sinz - cosx*siny*cosz;
    m[9] =  sinx*cosz + cosx*siny*sinz;
    m[10]=  cosx*cosy;
    m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0.0;
    m[15]=1.0;
}


void
anim_dz_y_x2mat(mat_t m, double x, double y, double z)
{
    fastf_t cosx, cosy, cosz, sinx, siny, sinz;

    x *= (M_PI*0.0055555555556);
    y *= (M_PI*0.0055555555556);
    z *= (M_PI*0.0055555555556);

    cosx = cos(x);
    sinx = sin(x);
    cosy = cos(y);
    siny = sin(y);
    cosz = cos(z);
    sinz = sin(z);

    m[0] =  cosy*cosz;
    m[1] = -cosy*sinz;
    m[2] =  siny;
    m[4] =  cosx*sinz + sinx*siny*cosz;
    m[5] =  cosx*cosz - sinx*siny*sinz;
    m[6] = -sinx*cosy;
    m[8] =  sinx*sinz - cosx*siny*cosz;
    m[9] =  sinx*cosz + cosx*siny*sinz;
    m[10]=  cosx*cosy;
    m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
    m[15]=1;
}


void
anim_quat2mat(mat_t m, const quat_t qq)
{
    fastf_t two_q[4];
    quat_t q;

    QMOVE(q, qq);
    QUNITIZE(q);

    VADD2N(two_q, q, q, 4);

    m[0] 	= 1.0 - two_q[Y]*q[Y] - two_q[Z]*q[Z];
    m[1]	= two_q[X]*q[Y] - two_q[W]*q[Z];
    m[2]	= two_q[X]*q[Z] + two_q[W]*q[Y];
    m[3]	= 0.0;
    m[4]	= two_q[X]*q[Y] + two_q[W]*q[Z];
    m[5] 	= 1.0 - two_q[X]*q[X] - two_q[Z]*q[Z];
    m[6]	= two_q[Y]*q[Z] - two_q[W]*q[X];
    m[7]	= 0.0;
    m[8]	= two_q[X]*q[Z] - two_q[W]*q[Y];
    m[9]	= two_q[Y]*q[Z] + two_q[W]*q[X];
    m[10] 	= 1.0 - two_q[X]*q[X] - two_q[Y]*q[Y];
    m[11]	= 0.0;
    m[12]	= 0.0;
    m[13]	= 0.0;
    m[14]	= 0.0;
    m[15]	= 1.0;
}


void
anim_dir2mat(mat_t m, const vect_t d, const vect_t d2b)
{
    fastf_t hypotenuse, sign;
    vect_t d2;

    VMOVE(d2, d2b);
    sign = 1.0;
    hypotenuse = sqrt(d[0]*d[0]+d[1]*d[1]);
    if (hypotenuse < VDIVIDE_TOL) {
	/* vertical direction - use d2 to determine roll */
	hypotenuse = sqrt(d2[0]*d2[0]+d2[1]*d2[1]);
	if (hypotenuse < VDIVIDE_TOL) {
	    /* use x-axis as default*/
	    VSET(d2, 1, 0, 0);
	    hypotenuse = 1;
	}
	if (d[2] < 0)
	    sign = -1.0;
	m[1] = -d2[1]/hypotenuse;
	m[5] = d2[0]/hypotenuse;
	m[2] = -sign * d2[0]/hypotenuse;
	m[6] = -sign * d2[1]/hypotenuse;
	m[8] = sign;
	m[0]=m[4]=m[9]=m[10]=0.0;
    } else {
	/* normal - no roll*/
	m[0] = d[0];
	m[1] = -d[1]/hypotenuse;
	m[2] = -d[0]*d[2]/hypotenuse;
	m[4] = d[1];
	m[5] = d[0]/hypotenuse;
	m[6] = -d[1]*d[2]/hypotenuse;
	m[8] = d[2];
	m[9] = 0.0;
	m[10] = hypotenuse;
    }
    m[3]=m[7]=m[11]=0.0;
    m[12]=m[13]=m[14]=0.0;
    m[15]=1.0;

}


void
anim_dirn2mat(mat_t m, const vect_t dx2, const vect_t dn)
{
    vect_t temp;
    fastf_t hyp, sign, inv, mag;
    vect_t dx;

    VMOVE(dx, dx2);
    sign = 1.0;
    mag = MAGNITUDE(dx);
    if (mag < VDIVIDE_TOL) {
	bu_log("anim_dirn2mat: Need non-zero vector");
	return;
    }
    inv = 1.0/mag;
    dx[0] *= inv;
    dx[1] *= inv;
    dx[2] *= inv;
    hyp = sqrt(dx[0]*dx[0]+dx[1]*dx[1]);
    if (hyp < VDIVIDE_TOL) {
	/* vertical - special handling */
	sign = (dx[2] < 0) ? -1.0 : 1.0;
	VSET(temp, dn[0], dn[1], 0.0);
	mag = MAGNITUDE(temp);
	if (mag < VDIVIDE_TOL) {
	    /* use default */
	    VSET(temp, 0.0, 1.0, 0.0);
	    mag = 1.0;
	} else {
	    inv = 1.0/mag;
	    temp[0] *= inv;
	    temp[1] *= inv;
	}
	m[0] = 0.0;
	m[4] = 0.0;
	m[8] = sign;
	m[1] = temp[0];
	m[5] = temp[1];
	m[9] = 0.0;
	m[2] = -sign*temp[1];
	m[6] = sign*temp[0];
	m[10] = 0.0;
	m[3]=m[7]=m[11]=0.0;
	m[12]=m[13]=m[14]=0.0;
	m[15]=1.0;
	return;
    }

    /*else normal*/
    m[0] = dx[0];
    m[1] = -dx[1]/hyp;
    m[2] = -dx[0]*dx[2]/hyp;
    m[4] = dx[1];
    m[5] = dx[0]/hyp;
    m[6] = -dx[1]*dx[2]/hyp;
    m[8] = dx[2];
    m[9] = 0.0;
    m[10] = hyp;
    m[3]=m[7]=m[11]=0.0;
    m[12]=m[13]=m[14]=0.0;
    m[15]=1.0;

}


#define ASM_EMPTY 0
#define ASM_FIRST 1
#define ASM_FULL 2

int
anim_steer_mat(mat_t mat, vect_t point, int end)
{
    static vect_t p1, p2, p3;
    vect_t dir = VINIT_ZERO;
    static vect_t norm;
    static int state = ASM_EMPTY;

    VMOVE(p1, p2);
    VMOVE(p2, p3);
    VMOVE(p3, point);

    switch (state) {
	case ASM_EMPTY:
	    if (end) {
		state = ASM_EMPTY;
	    } else {
		state = ASM_FIRST;
		/* "don't print yet */
	    }
	    return 0;
	case ASM_FIRST:
	    if (end) {
		/* only one point specified, use default direction */
		VSET(dir, 1.0, 0.0, 0.0);
		VSET(norm, 0.0, 1.0, 0.0);
		state = ASM_EMPTY;
	    } else {
		VSUBUNIT(dir, p3, p2);
		VSET(norm, 0.0, 1.0, 0.0);
		state = ASM_FULL;
	    }
	    break;
	case ASM_FULL:
	    if (end) {
		VSUBUNIT(dir, p2, p1);
		state = ASM_EMPTY;
	    } else {
		VSUBUNIT(dir, p3, p1);
		state = ASM_FULL;
	    }
    }

    /* go for it */
    anim_dirn2mat(mat, dir, norm); /* create basic rotation matrix */
    VSET(norm, mat[1], mat[5], 0.0); /* save for next time */
    VMOVE(point, p2); /* for main's purposes, the current point is p2 */
    return 1; /* return signal go ahead and print */

}


/***************************************
 * Other animation routines
 ***************************************/


void
anim_add_trans(mat_t m, const vect_t post, const vect_t pre)
{
    int i;
    for (i=0; i<3; i++)
	m[3+i*4] += m[i*4]*pre[0] + m[1+i*4]*pre[1]+m[2+i*4]*pre[2] + post[i];

}


void
anim_rotatez(fastf_t a, vect_t d)
{
    fastf_t temp[3];
    fastf_t cos_y = cos(a);
    fastf_t sin_y = sin(a);
    temp[0] = d[0]*cos_y - d[1]*sin_y;
    temp[1] = d[0]*sin_y + d[1]*cos_y;
    d[0]=temp[0];
    d[1]=temp[1];
}


void
anim_mat_print(FILE *fp, const mat_t m, int s_colon)
{
    bu_flog(fp, "%.10g %.10g %.10g %.10g\n", m[0], m[1], m[2], m[3]);
    bu_flog(fp, "%.10g %.10g %.10g %.10g\n", m[4], m[5], m[6], m[7]);
    bu_flog(fp, "%.10g %.10g %.10g %.10g\n", m[8], m[9], m[10], m[11]);
    bu_flog(fp, "%.10g %.10g %.10g %.10g", m[12], m[13], m[14], m[15]);
    if (s_colon)
	bu_flog(fp, ";");
    bu_flog(fp, "\n");
}


void
anim_mat_printf(
    FILE *fp,
    const mat_t m,
    const char *formstr,
    const char *linestr,
    const char *endstr)
{
    char mystr[80];
    snprintf(mystr, 80, "%s%s%s%s%%s", formstr, formstr, formstr, formstr);
    bu_flog(fp, mystr, m[0], m[1], m[2], m[3], linestr);
    bu_flog(fp, mystr, m[4], m[5], m[6], m[7], linestr);
    bu_flog(fp, mystr, m[8], m[9], m[10], m[11], linestr);
    bu_flog(fp, mystr, m[12], m[13], m[14], m[15], endstr);
}


void
anim_view_rev(mat_t m)
{
    m[0] = -m[0];
    m[1] = -m[1];
    m[4] = -m[4];
    m[5] = -m[5];
    m[8] = -m[8];
    m[9] = -m[9];
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
