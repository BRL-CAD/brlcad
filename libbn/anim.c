/* ANIM.C - Commonly used routines in animation programs by Carl Nuzman */
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "anim.h"

#ifndef	M_PI
#define M_PI	3.14159265358979323846
#endif

/* DO_ZYX() - Converts a view-rotation matrix to a z rotation,
 * 	a y rotation, and an x rotation. 
 * 	A matrix which rotates the view Z degrees about the z-axis, 
 * then Y degrees about the y-axis, and finally X degrees about the 
 * x-axis should have the following form:
 * 
 * element 0: 	 cosY*cosZ
 * element 1:	-cosY*sinZ
 * element 2:	 sinY
 * element 3:	 0
 * element 4:	 cosX*sinZ + sinX*sinY*cosZ
 * element 5:	 cosX*cosZ - sinX*sinY*sinZ
 * element 6:	-sinX*cosY
 * element 7:	 0
 * element 8:	 sinX*sinZ - cosX*sinY*cosZ
 * element 9:	 sinX*cosZ + cosX*sinY*sinZ
 * element 10:	 cosX*cosY
 * element 11:	 0
 * element 12:	 0
 * element 13:	 0
 * element 14:	 0
 * element 15:	 1.0
 * 
 * This function extracts the angles of rotation from the given matrix,
 * using the atan2() function.
 *	A return value of 0 is normal. A value of 1 means that arbitrary 
 * assumptions were made, and a value of 2 is the error signal.
 */

#define DEGREES		0
#define RADIANS		1

int do_zyx(viewrot,units_flag)
fastf_t *viewrot;
int units_flag;
{
	int i, return_value;
	fastf_t angle[3], sinx, sinz, cosx, cosz;
	static fastf_t previous[3];

	if ((viewrot[1]==0.0) && (viewrot[0]==0.0)){
		return_value = 1; /* indicates arbitrary assumptions */
		angle[X] = 0.0;
		angle[Z] = atan2(viewrot[4],viewrot[5]);
		fprintf(stderr,"Warning: x arbitrarily set to 0.0; z set to %f.\n",angle[Z]);
	}
	else {
		return_value = 0; /* normal */
		angle[Z] = atan2(-viewrot[1],viewrot[0]);
		angle[X] = atan2(-viewrot[6],viewrot[10]);
	}

	sinx = sin(angle[X]);
	sinz = sin(angle[Z]);
	cosx = cos(angle[X]);
	cosz = cos(angle[Z]);

	if ( (sinx*cosz) != 0.0 )
		angle[Y]=atan2( (viewrot[4] - cosx*sinz)/(sinx*cosz), -viewrot[6]/sinx);
	else if ( (cosx*cosz) != 0.0 )
		angle[Y]=atan2( (-viewrot[8] + sinx*sinz)/(cosx*cosz), viewrot[0]/cosz);
	else if ( (sinx*sinz) != 0.0 )
		angle[Y]=atan2( (-viewrot[5] + cosx*cosz)/(sinx*sinz), -viewrot[1]/sinz);
	else if ( (cosx*sinz) != 0.0 )
		angle[Y]=atan2( (viewrot[9] - sinx*cosz)/(cosx*sinz), viewrot[10]/cosx);
	else {
		/* unable to calculate y rotation */
		return(2);
	}

	/* assume the smallest possible arc-length from frame to frame */
	for (i=0; i<3; i++) {
		while ((angle[i] - previous[i]) > M_PI)
			angle[i] -= (2.0*M_PI);
		while ((previous[i] - angle[i]) > M_PI)
			angle[i] += (2.0*M_PI);
		previous[i] = angle[i];
	}

	/* convert to degrees if necessary */
	if (units_flag==DEGREES)  {
		angle[Z] *= (180.0/M_PI);
		angle[Y] *= (180.0/M_PI);
		angle[X] *= (180.0/M_PI);
	}

	/* print results */
	printf("%f %f %f\n", angle[X], angle[Y], angle[Z]);
	return(return_value);

}

/* DO_AET() - Interprets a view-rotation matrix in terms of an azimuth, 
 *	an elevation, and a twist. 
 * A matrix which rotates the view to a given azimuth elevation and twist
 * has the following form:
 *        
 * element	0:	-cosa*sine*sint-sina*cost
 * element	1:	-sina*sine*sint+cosa*cost
 * element	2:	 cose*sint
 * element	3:	 0
 * element	4:	-cosa*sine*cost+sina*sint
 * element	5:	-sina*sine*cost-cosa*sint
 * element	6:	 cose*cost
 * element	7:	 0
 * element	8:	 cosa*cose
 * element	9:	 sina*cose
 * element	10:	 sine
 * element	11:	 0
 * element	12:	 0
 * element	13:	 0
 * element	14:	 0
 * element	15:	 1.0
 * 
 * This function extracts the azimuth, elevation and twist from the given
 * matrix, using the atan2() function.
 *	A return value of 0 is normal. A value of 1 means that arbitrary 
 * assumptions were made, and a value of 2 is the error signal.
 */

#define DEGREES		0
#define RADIANS		1

#define A		0
#define E		1
#define T		2

int do_aet(viewrot, units_flag)
fastf_t *viewrot;
int	units_flag;
{
	int i, return_value;
	fastf_t aet[3], sina, sint, cosa, cost;
	static fastf_t prev_aet[3];

	if ((viewrot[2]==0.0) && (viewrot[6]==0.0)){
		return_value = 1; /* indicates arbitrary assumptions */
		aet[T] = 0.0;
		aet[A] = atan2(-viewrot[0],viewrot[1]);
		fprintf(stderr,"Warning: twist arbitrarily set to 0.0; azimuth set to %f radians.\n",aet[A]);
	}
	else {
		return_value = 0; /* normal */
		aet[A] = atan2(viewrot[9],viewrot[8]);
		aet[T] = atan2(viewrot[2],viewrot[6]);
	}

	sina = sin(aet[A]);
	sint = sin(aet[T]);
	cosa = cos(aet[A]);
	cost = cos(aet[T]);

	if ( (cosa*sint) != 0.0 )
		aet[E] = atan2( -(viewrot[0]+sina*cost)/(cosa*sint),viewrot[2]/sint);
	else if ( (sina*cost) != 0.0)
		aet[E] = atan2( -(viewrot[5]+cosa*sint)/(sina*cost),viewrot[6]/cost);
	else if ( (sina*sint) != 0.0)
		aet[E] = atan2( -(viewrot[0]-cosa*cost)/(sina*sint),viewrot[9]/sina);
	else if ( (cosa*cost) != 0.0)
		aet[E] = atan2( -(viewrot[0]-sina*sint)/(cosa*cost),viewrot[8]/cosa);
	else {
		/* unable to calculate elevation*/
		return(2);
	}

	/* assume the smallest possible arc-length from frame to frame */
	for (i=0; i<3; i++) {
		while ((aet[i] - prev_aet[i]) > M_PI)
			aet[i] -= (2.0*M_PI);
		while ((prev_aet[i] - aet[i]) > M_PI)
			aet[i] += (2.0*M_PI);
		prev_aet[i] = aet[i];
	}

	/* convert to degrees if necessary */
	if (units_flag==DEGREES) {
		aet[A] *= (180.0/M_PI);
		aet[E] *= (180.0/M_PI);
		aet[T] *= (180.0/M_PI);
	}

	/* print results */
	/* negative signs are introduced because ascript interprets azimuth */
	/* and elevation as looking TOWARD that azimuth and elevation, while*/
	/* mged interprets it as looking FROM that azimuth and elevation*/
	printf("%f %f %f\n", -aet[A], -aet[E], aet[T]);
	return(return_value);

}

/* DO_QUAT -  This interprets a view rotation matrix in terms of quaternions.
 * The algorithm is from Ken Shoemake, Animating Rotation with Quaternion 
 * Curves, 1985 SIGGraph Conference Proceeding, p.245.
 */
int do_quat(viewrot)
fastf_t *viewrot;
{
	int i;	
	fastf_t quat[4], qdiff[4], square, mag1, mag2;
	static fastf_t prev_quat[4];

	square = 0.25 * (1 + viewrot[0] + viewrot[5] + viewrot[10]);
	if ( square != 0.0 ) {
		quat[W] = sqrt(square);
		quat[X] = 0.25 * (viewrot[6] - viewrot[9])/ quat[W];
		quat[Y] = 0.25 * (viewrot[8] - viewrot[2])/ quat[W];
		quat[Z] = 0.25 * (viewrot[1] - viewrot[4])/ quat[W];
	}
	else {
		quat[W] = 0.0;
		square = -0.5 * (viewrot[5] + viewrot[10]);
		if (square != 0.0 ) {
			quat[X] = sqrt(square);
			quat[Y] = 0.5 * viewrot[1] / quat[X];
			quat[Z] = 0.5 * viewrot[2] / quat[X];
		}
		else {
			quat[X] = 0.0;
			square = 0.5 * (1 - viewrot[10]);
			if (square != 0.0){
				quat[Y] = sqrt(square);
				quat[Z] = 0.5 * viewrot[6]/ quat[Y];
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

	/* print results */
	printf("%f %f %f %f\n",quat[X], quat[Y], quat[Z], quat[W]);
	return(1.0);
}


/* AET2MAT - Make matrix to rotate an object to the given azimuth,
 * elevation, and twist. (Specified in degrees.)
 */
void aet2mat(m,a,e,t) /*make object rotation matrix from aet*/
double m[], a, e, t;
{
        double radian_azimuth = a*(M_PI*0.0055555555556);
        double radian_elevation = e*(M_PI*0.0055555555556);
        double radian_twist = t*(M_PI*0.0055555555556);

        double cosa = cos(radian_azimuth);
        double sina = sin(radian_azimuth);
        double cose = cos(radian_elevation);
        double sine = sin(radian_elevation);
        double cost = cos(radian_twist);
        double sint = sin(radian_twist);

        m[0] = cosa*cose;
        m[1] = -cosa*sine*sint-sina*cost;
        m[2] = -cosa*sine*cost+sina*sint;
        m[4] = sina*cose;
        m[5] = -sina*sine*sint+cosa*cost;
        m[6] = -sina*sine*cost-cosa*sint;
        m[8]= sine;
        m[9] = cose*sint;
        m[10] = cose*cost;
        m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
        m[15]=1;
}

/* RAD_AET2MAT - Make matrix to rotate an object to the given azimuth,
 * elevation, and twist. (Specified in radians.)
 */
void rad_aet2mat(m,a,e,t) /*make object rotation matrix from radian aet*/
double m[], a, e, t;
{
        double cosa = cos(a);
        double sina = sin(a);
        double cose = cos(e);
        double sine = sin(e);
        double cost = cos(t);
        double sint = sin(t);

        m[0] = cosa*cose;
        m[1] = -cosa*sine*sint-sina*cost;
        m[2] = -cosa*sine*cost+sina*sint;
        m[4] = sina*cose;
        m[5] = -sina*sine*sint+cosa*cost;
        m[6] = -sina*sine*cost-cosa*sint;
        m[8]= sine;
        m[9] = cose*sint;
        m[10] = cose*cost;
        m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
        m[15]=1;
}



/* XYZ_MAT - Make a rotation matrix corresponding to a rotation of 
 * "x" degrees about the x-axis, "y" degrees about the y-axis, and
 * then "z" degrees about the z-axis.
 */
void xyz_mat(m, x, y, z)
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
        m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
        m[15]=1;
}

/* RAD_XYZ_MAT - Make a rotation matrix corresponding to a rotation of 
 * "x" radians about the x-axis, "y" radians about the y-axis, and
 * then "z" radians about the z-axis.
 */
void rad_xyz_mat(m, x, y, z)
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

/* ZYX_MAT - Make a rotation matrix corresponding to a rotation of 
 * "z" degrees about the z-axis, "y" degrees about the y-axis, and
 * then "x" degrees about the x-axis.
 */
void zyx_mat(m,x,y,z)
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

/* RAD_ZYX_MAT - Make a rotation matrix corresponding to a rotation of 
 * "z" radians about the z-axis, "y" radians about the y-axis, and
 * then "x" radians about the x-axis.
 */
void rad_zyx_mat(m,x,y,z)
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
        m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0;
        m[15]=1;
}

/* AET2MAT_V - Make a view rotation matrix, given desired azimuth, elevation
 * and twist. (Note that the matrix is a permutation of the object rotation
 * matrix).
 */
void aet2mat_v(m,az,el,tw) /*make view rotation matrix from aet*/
float m[], az, el, tw;
{

	float raz = az*(M_PI*0.0055555555556);
	float rel = el*(M_PI*0.0055555555556);
	float rtw = tw*(M_PI*0.0055555555556);
	
	float cosa = cos(raz);
	float sina = sin(raz);
	float cose = cos(rel);
	float sine = sin(rel);
	float cost = cos(rtw);
	float sint = sin(rtw);
	
	m[0] = -cosa*sine*sint-sina*cost;
	m[1] = -sina*sine*sint+cosa*cost;
	m[2] = cose*sint;
	m[4] = -cosa*sine*cost+sina*sint;
	m[5] = -sina*sine*cost-cosa*sint;
	m[6] = cose*cost;
	m[8] = cosa*cose;
	m[9] = sina*cose;
	m[10]= sine;
	m[3]=m[7]=m[11]=0;
	m[12]=m[13]=m[14]=0;
	m[15]=1;

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
                                        * determine twist */
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
        else { /* normal - no twist*/
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
        double cosa = cos(a);
        double sina = sin(a);
        temp[0] = d[0]*cosa - d[1]*sina;
        temp[1] = d[0]*sina + d[1]*cosa;
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



