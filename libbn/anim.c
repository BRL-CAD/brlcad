/*				A N I M . C
 *
 *	Routines useful in animation programs.
 *
 *  Author -
 *	Carl J. Nuzman
 *  
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1993 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "anim.h"

#ifndef	M_PI
#define M_PI	3.14159265358979323846
#endif

#define NORMAL		0
#define ERROR1		1
#define ERROR2		2

/* V_PERMUTE - Pre-multiply a rotation matrix by a matrix 
 * which maps the z-axis to the negative x-axis, the y-axis to the 
 * z-axis and the x-axis to the negative y-axis. This is used in some 
 * situations where the virtual camera is involved.
 */
void v_permute(m)
fastf_t *m;
{
	int i;
	fastf_t store;

	for (i=0; i<9; i+=4){
		store = m[i];
		m[i] = -m[i+1];
		m[i+1] = m[i+2];
		m[i+2] = -store;
	}
}

/* UN_V_PERMUTE - Undo the mapping done by v_permute().
 */
void un_v_permute(m)
fastf_t *m;
{
	int i;
	fastf_t store;

	for (i=0; i<9; i+=4){
		store = m[i+2];
		m[i+2] = m[i+1];
		m[i+1] = -m[i];
		m[i] = -store;
	}
}

/* TRANSPOSE - Tranpose a 4x4 matrix
 */
void transpose(m)
fastf_t *m;
{
	fastf_t store;

	store = m[1];
	m[1] = m[4];
	m[4] = store;

	store = m[2];
	m[2] = m[8];
	m[8] = store;

	store = m[3];
	m[3] = m[12];
	m[12] = store;

	store = m[6];
	m[6] = m[9];
	m[9] = store;

	store = m[7];
	m[7] = m[13];
	m[13] = store;

	store = m[11];
	m[11] = m[14];
	m[14] = store;

}

/* MAT2ZYX - Convert the rotational part of a 4x4 transformation matrix
 * to zyx form, that is to say, rotations carried out in the order z, y,
 * and then x. The angles are stored in radians as a vector in the order 
 * x,y,z. A return value of ERROR1 means that arbitrary assumptions were 
 * necessary. ERROR2 means that the conversion failed.
 */
int mat2zyx(angle,viewrot)
fastf_t *viewrot;
fastf_t *angle;
{
        int i, return_value;
        fastf_t sinx, sinz, cosx, cosz;
        static fastf_t previous[3];

        if ((viewrot[1]==0.0) && (viewrot[0]==0.0)){
                return_value = ERROR1;
                angle[0] = 0.0;
                angle[2] = atan2(viewrot[4],viewrot[5]);
                fprintf(stderr,"Warning: x arbitrarily set to 0.0; z set to %f.\n",angle[2]);
        }
        else {
                return_value = NORMAL;
                angle[2] = atan2(-viewrot[1],viewrot[0]);
                angle[0] = atan2(-viewrot[6],viewrot[10]);
        }

        sinx = sin(angle[0]);
        sinz = sin(angle[2]);
        cosx = cos(angle[0]);
        cosz = cos(angle[2]);

        if ( (sinx*cosz) != 0.0 )
                angle[1]=atan2( (viewrot[4] - cosx*sinz)/(sinx*cosz), -viewrot[6]/sinx);
        else if ( (cosx*cosz) != 0.0 )
                angle[1]=atan2( (-viewrot[8] + sinx*sinz)/(cosx*cosz), viewrot[0]/cosz);
        else if ( (sinx*sinz) != 0.0 )
                angle[1]=atan2( (-viewrot[5] + cosx*cosz)/(sinx*sinz), -viewrot[1]/sinz);
        else if ( (cosx*sinz) != 0.0 )
                angle[1]=atan2( (viewrot[9] - sinx*cosz)/(cosx*sinz), viewrot[10]/cosx);
        else {
                /* unable to calculate y rotation */
                return(ERROR2);
        }

        /* assume the smallest possible arc-length from frame to frame */
        for (i=0; i<3; i++) {
                while ((angle[i] - previous[i]) > M_PI)
                        angle[i] -= (2.0*M_PI);
                while ((previous[i] - angle[i]) > M_PI)
                        angle[i] += (2.0*M_PI);
                previous[i] = angle[i];
        }

	return(return_value);
}

/* MAT2YPR - Convert the rotational part of a 4x4 transformation matrix
 * to yaw-pitch-roll form, that is to say, +roll degrees about the x-axis, 
 * -pitch degrees about the y-axis, and +yaw degrees about the
 * z-axis. The angles are stored in radians as a vector in the order y,p,r.
 * A return of ERROR1 means that arbitrary assumptions were necessary.
 * ERROR2 means that the conversion failed.
 */
int mat2ypr(angle,viewrot)
fastf_t *viewrot;
fastf_t angle[3];
{
        int i, return_value;
        fastf_t sin_y, sin_r, cos_y, cos_r;
        static fastf_t prev_angle[3];

        if ((viewrot[9]==0.0) && (viewrot[10]==0.0)){
                return_value = ERROR1;
                angle[2] = 0.0;
                angle[0] = atan2(-viewrot[1],viewrot[5]);
                fprintf(stderr,"Warning: roll arbitrarily set to 0.0; yaw set to %f radians.\n",angle[0]);
        }
        else {
                return_value = NORMAL;
                angle[0] = atan2(viewrot[4],viewrot[0]);
                angle[2] = atan2(viewrot[9],viewrot[10]);
        }

        sin_y = sin(angle[0]);
        sin_r = sin(angle[2]);
        cos_y = cos(angle[0]);
        cos_r = cos(angle[2]);

        if ( (cos_y*sin_r) != 0.0 )
                angle[1] = atan2( -(viewrot[1]+sin_y*cos_r)/(cos_y*sin_r),viewrot[9]/sin_r);
        else if ( (sin_y*cos_r) != 0.0)
                angle[1] = atan2( -(viewrot[6]+cos_y*sin_r)/(sin_y*cos_r),viewrot[10]/cos_r);
        else if ( (sin_y*sin_r) != 0.0)
                angle[1] = atan2( -(viewrot[5]-cos_y*cos_r)/(sin_y*sin_r),viewrot[4]/sin_y);
        else if ( (cos_y*cos_r) != 0.0)
                angle[1] = atan2( -(viewrot[2]-sin_y*sin_r)/(cos_y*cos_r),viewrot[0]/cos_y);
        else {
                /* unable to calculate pitch*/
                return(ERROR2);
        }

        /* assume the smallest possible arc-length from frame to frame */
        for (i=0; i<3; i++) {
                while ((angle[i] - prev_angle[i]) > M_PI)
                        angle[i] -= (2.0*M_PI);
                while ((prev_angle[i] - angle[i]) > M_PI)
                        angle[i] += (2.0*M_PI);
                prev_angle[i] = angle[i];
        }

	return(return_value);
}

/* MAT2QUAT -  This interprets the rotational part of a 4x4 transformation 
 *  matrix in terms of unit quaternions. The result is stored as a vector in 
 * the order x,y,z,w. 
 * The algorithm is from Ken Shoemake, Animating Rotation with Quaternion 
 * Curves, 1985 SIGGraph Conference Proceeding, p.245.
 */
int mat2quat(quat,viewrot)
fastf_t *quat, *viewrot;
{
	int i;	
	fastf_t qdiff[4], square, mag1, mag2;
	static fastf_t prev_quat[4];

	square = 0.25 * (1 + viewrot[0] + viewrot[5] + viewrot[10]);
	if ( square != 0.0 ) {
		quat[W] = sqrt(square);
		quat[X] = 0.25 * (viewrot[9] - viewrot[6])/ quat[W];
		quat[Y] = 0.25 * (viewrot[2] - viewrot[8])/ quat[W];
		quat[Z] = 0.25 * (viewrot[4] - viewrot[1])/ quat[W];
	}
	else {
		quat[W] = 0.0;
		square = -0.5 * (viewrot[5] + viewrot[10]);
		if (square != 0.0 ) {
			quat[X] = sqrt(square);
			quat[Y] = 0.5 * viewrot[4] / quat[X];
			quat[Z] = 0.5 * viewrot[8] / quat[X];
		}
		else {
			quat[X] = 0.0;
			square = 0.5 * (1 - viewrot[10]);
			if (square != 0.0){
				quat[Y] = sqrt(square);
				quat[Z] = 0.5 * viewrot[9]/ quat[Y];
			}
			else {
				quat[Y] = 0.0;
				quat[Z] = 1.0;
			}
		}
	}

	/* quaternions on opposite sides of a four-dimensional sphere
		are equivalent. Take the quaternion closest to the previous
		one */

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

	return(1);
}


/* YPR2MAT - Create a premultiplication rotation matrix to turn the front
 * of an object (its x-axis) to the given yaw, pitch, and roll, 
 * which is stored in radians in the vector a.
 */
void ypr2mat(m,a)
fastf_t m[11];
fastf_t a[3];
{
	fastf_t cos_y,cos_p,cos_r,sin_y,sin_p,sin_r;

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

/* YPR2VMAT - Create a post-multiplication rotation matrix ,which could
 * be used to move the virtual camera to the given yaw, pitch, 
 * and roll,  which are stored in radians in the given vector a. The 
 * following are equivalent sets of commands:
 * 	ypr2vmat(matrix,a);
 *		or
 *	ypr2mat(matrix,a);
 * 	v_permute(matrix);
 *	transpose(matrix;
 */
void ypr2vmat(m,a)
fastf_t m[11];
fastf_t a[3];
{
	fastf_t cos_y,cos_p,cos_r,sin_y,sin_p,sin_r;

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

/* Y_P_R2MAT - Make matrix to rotate an object to the given yaw,
 * pitch, and roll. (Specified in radians.)
 */
void y_p_r2mat(m,a,e,t) /*make object rotation matrix from radian ypr*/
double m[], a, e, t;
{
        double cos_y = cos(a);
        double sin_y = sin(a);
        double cos_p = cos(e);
        double sin_p = sin(e);
        double cos_r = cos(t);
        double sin_r = sin(t);

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




/* DY_P_R2MAT - Make matrix to rotate an object to the given yaw,
 * pitch, and roll. (Specified in degrees.)
 */
void dy_p_r2mat(m,a,e,t) /*make object rotation matrix from ypr*/
double m[], a, e, t;
{
        double radian_yaw = a*(M_PI*0.0055555555556);
        double radian_pitch = e*(M_PI*0.0055555555556);
        double radian_roll = t*(M_PI*0.0055555555556);

        double cos_y = cos(radian_yaw);
        double sin_y = sin(radian_yaw);
        double cos_p = cos(radian_pitch);
        double sin_p = sin(radian_pitch);
        double cos_r = cos(radian_roll);
        double sin_r = sin(radian_roll);

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


/* DY_P_R2VMAT - Make a view rotation matrix, given desired yaw, pitch
 * and roll. (Note that the matrix is a permutation of the object rotation
 * matrix).
 */
void dy_p_r2vmat(m,az,el,tw) /*make view rotation matrix from ypr*/
float m[], az, el, tw;
{

	float raz = az*(M_PI*0.0055555555556);
	float rel = el*(M_PI*0.0055555555556);
	float rtw = tw*(M_PI*0.0055555555556);
	
	float cos_y = cos(raz);
	float sin_y = sin(raz);
	float cos_p = cos(rel);
	float sin_p = sin(rel);
	float cos_r = cos(rtw);
	float sin_r = sin(rtw);
	
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

/* X_Y_Z2MAT - Make a rotation matrix corresponding to a rotation of 
 * "x" radians about the x-axis, "y" radians about the y-axis, and
 * then "z" radians about the z-axis.
 */
void x_y_z2mat(m, x, y, z)
double m[], x, y, z;
{
        double cosx = cos(x);
        double sinx = sin(x);
        double cosy = cos(y);
        double siny = sin(y);
        double cosz = cos(z);
        double sinz = sin(z);

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



/* DX_Y_Z2MAT - Make a rotation matrix corresponding to a rotation of 
 * "x" degrees about the x-axis, "y" degrees about the y-axis, and
 * then "z" degrees about the z-axis.
 */
void dx_y_z2mat(m, x, y, z)
double m[], x, y, z;
{
	double cosx,cosy,cosz,sinx,siny,sinz;

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

/* ZYX2MAT - Make a rotation matrix corresponding to a rotation of 
 * "z" radians about the z-axis, "y" radians about the y-axis, and
 * then "x" radians about the x-axis. 
 */
void zyx2mat(m,a)
fastf_t *m, *a;
{
	fastf_t cosX,cosY,cosZ,sinX,sinY,sinZ;

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

/* Z_Y_X2MAT - Make a rotation matrix corresponding to a rotation of 
 * "z" radians about the z-axis, "y" radians about the y-axis, and
 * then "x" radians about the x-axis.
 */
void z_y_x2mat(m,x,y,z)
double m[], x, y, z;
{
        double cosx = cos(x);
        double sinx = sin(x);
        double cosy = cos(y);
        double siny = sin(y);
        double cosz = cos(z);
        double sinz = sin(z);

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


/* DZ_Y_X2MAT - Make a rotation matrix corresponding to a rotation of 
 * "z" degrees about the z-axis, "y" degrees about the y-axis, and
 * then "x" degrees about the x-axis.
 */
void dz_y_x2mat(m,x,y,z)
double m[], x, y, z;
{
	double cosx,cosy,cosz,sinx,siny,sinz;

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


/* QUAT_MAT - Make 4x4 matrix from the given quaternion
 */

void quat2mat(m, q)
fastf_t *m, *q;
{
	fastf_t two_q[4];


	QUNITIZE(q);

	VADD2N(two_q,q,q,4);

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



/* DIR2MAT - make a matrix which turns a vehicle from the x-axis to 
 * point in the desired direction. A second direction vector, representing
 * the vehicle's previous direction, is consulted when the given direction
 * is vertical.
 */
void dir2mat(m,d,d2)
double m[], d[], d2[];
{
        double hypotenuse, sign;
        sign = 1.0;
        hypotenuse = sqrt(d[0]*d[0]+d[1]*d[1]);
        if (hypotenuse < VDIVIDE_TOL){ /* vertical direction - use d2 to
                                        * determine roll */
                hypotenuse = sqrt(d2[0]*d2[0]+d2[1]*d2[1]);
                if (hypotenuse < VDIVIDE_TOL){ /* use x-axis as default*/
                        VSET(d2,1,0,0);
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
        }
        else { /* normal - no roll*/
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

/* ADD_TRANS - Add pre- and post- translation to a rotation matrix.
 */
void add_trans(m,post,pre)
double m[], post[], pre[];
{
        int i;
        for (i=0; i<3; i++)
        m[3+i*4] += m[i*4]*pre[0] + m[1+i*4]*pre[1]+m[2+i*4]*pre[2] + post[i];
}

/* ROTATEZ - Rotate the vector "d" through "a" radians about the z-axis.
 */
void rotatez(a,d)
double a, d[];
{
        double temp[3];
        double cos_y = cos(a);
        double sin_y = sin(a);
        temp[0] = d[0]*cos_y - d[1]*sin_y;
        temp[1] = d[0]*sin_y + d[1]*cos_y;
        d[0]=temp[0];
        d[1]=temp[1];
}

/* AN_MAT_PRINT - print out 4X4 matrix, with optional colon
 */
void an_mat_print(m,s_colon)
double m[];
int s_colon;
{
        printf("%f %f %f %f\n", m[0], m[1], m[2], m[3]);
        printf("%f %f %f %f\n", m[4], m[5], m[6], m[7]);
        printf("%f %f %f %f\n", m[8], m[9], m[10], m[11]);
        printf("%f %f %f %f", m[12], m[13], m[14], m[15]);
        if (s_colon)
                printf(";");
        printf("\n");
}

/* VIEW_REV - Reverse the direction of a view matrix, keeping it
 * right-side up
 */
void view_rev(m) /* reverses view matrix, but keeps it 'right-side-up'*/
double m[];
{
        m[0] = -m[0];
        m[1] = -m[1];
        m[4] = -m[4];
        m[5] = -m[5];
        m[8] = -m[8];
        m[9] = -m[9];
}



