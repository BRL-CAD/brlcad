/*
 *			A N I M . C
 *
 *  Author -
 *	Carl J. Nuzman
 *  
  Source -
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

/* ANIM.C - Commonly used routines in animation programs by Carl Nuzman */

#include <stdio.h>
#include <math.h>
#include <brlcad/machine.h>
#include <brlcad/vmath.h>
#include "anim.h"

void rotmat_inv(m,a) /*inverts 3x3 part of 4x4 matrix*/
double m[], a[];
{
        float det, invdet;
        det = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] + a[1]*a[6]*a[8] - a[1]*a[4]*a[10] + a[2]*a[4]*a[9] - a[2]*a[5]*a[8];
	invdet = 1/det;
	m[0] = (a[5]*a[10] - a[6]*a[9])*invdet;
	m[1] = (a[2]*a[9] - a[1]*a[10])*invdet;
	m[2] = (a[1]*a[6] - a[2]*a[5])*invdet;
	m[4] = (a[6]*a[8] - a[4]*a[10])*invdet;
	m[5] = (a[0]*a[10] - a[2]*a[8])*invdet;
	m[6] = (a[2]*a[4] - a[0]*a[6])*invdet;
	m[8] = (a[4]*a[9] - a[5]*a[8])*invdet;
	m[9] = (a[1]*a[8] - a[0]*a[9])*invdet;
	m[10] = (a[0]*a[5] - a[1]*a[4])*invdet;
	m[3]=m[7]=m[11]=m[12]=m[13]=m[14]=0.0;
	m[15]=1.0;
}

void rotmat3_inv(m,a) /*inverts 3x3 matrix*/
fastf_t m[], a[];
{
        float det, invdet;
        det = a[0]*a[4]*a[8] - a[0]*a[5]*a[7] + a[1]*a[5]*a[6] - a[1]*a[3]*a[8] + a[2]*a[3]*a[7] - a[2]*a[4]*a[6];
	invdet = 1/det;
	m[0] = (a[4]*a[8] - a[5]*a[7])*invdet;
	m[1] = (a[2]*a[7] - a[1]*a[8])*invdet;
	m[2] = (a[1]*a[5] - a[2]*a[4])*invdet;
	m[3] = (a[5]*a[6] - a[3]*a[8])*invdet;
	m[4] = (a[0]*a[8] - a[2]*a[6])*invdet;
	m[5] = (a[2]*a[3] - a[0]*a[5])*invdet;
	m[6] = (a[3]*a[7] - a[4]*a[6])*invdet;
	m[7] = (a[1]*a[6] - a[0]*a[7])*invdet;
	m[8] = (a[0]*a[4] - a[1]*a[3])*invdet;
}

void aet2mat(m,a,e,t) /*make object rotation matrix from aet*/
double m[], a, e, t;
{
        double radian_azimuth = a*DTOR;
        double radian_elevation = e*DTOR;
        double radian_twist = t*DTOR;

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



void xyz_mat(m, x, y, z)/* rotate x then y then z */
double m[], x, y, z;
{
	double cosx,cosy,cosz,sinx,siny,sinz;

	x *= DTOR;
	y *= DTOR;
	z *= DTOR;

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
void rad_xyz_mat(m, x, y, z)/* rotate x then y then z */
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

void zyx_mat(m,x,y,z)/*rotate z y x */
double m[], x, y, z;
{
	double cosx,cosy,cosz,sinx,siny,sinz;

	x *= DTOR;
	y *= DTOR;
	z *= DTOR;

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

void rad_zyx_mat(m,x,y,z)/*rotate z y x */
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

void aet2mat_v(m,az,el,tw) /*make view rotation matrix from aet*/
float m[], az, el, tw;
{

	float raz = az*DTOR;
	float rel = el*DTOR;
	float rtw = tw*DTOR;
	
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


/*****not for now
int steer_mat(mat,point)
double mat[],point[];
{
        void dir2mat(), add_trans(), rotatez(), view_rev();
        static double p1[3], p2[3], p3[3];
        double dir[3], dir2[3], temp[3];

        VMOVE(p1,p2);
        VMOVE(p2,p3);
        VMOVE(p3,point);
        if (frame == 0){ /* first frame*//*
                VSUBUNIT(dir,p3,p2);
                VMOVE(dir2,dir);
        }
        else if (last_steer){ /*last frame*//*
                VSUBUNIT(dir,p2,p1);
                VMOVE(dir2,dir);
        }
        else if (frame > 0){ /*normal*//*
                VSUBUNIT(dir,p3,p1);
                VSUBUNIT(dir2,p2,p1);/*needed for vertical case*//*
        }
        else return(0);

        if (angle){ /* front of object at angle from x-axis*//*
                rotatez(-angle,dir);
                rotatez(-angle,dir2);
        }
        dir2mat(mat,dir,dir2);
        add_trans(mat,p2,rcentroid);
        if (view){
                view_rev(mat);/*because aet is opposite for the view*//*
        }
        return(1);
}
*****/

void dir2mat(m,d,d2) /*make matrix which turns vehicle from x-axis*/
double m[], d[], d2[]; /* to desired direction*/
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

void add_trans(m,post,pre) /* add pre and post translation*/
double m[], post[], pre[];  /* to rotation matrix */
{
        int i;
        for (i=0; i<3; i++)
        m[3+i*4] += m[i*4]*pre[0] + m[1+i*4]*pre[1]+m[2+i*4]*pre[2] + post[i];
}

void rotatez(a,d) /*rotate given vector a radians about zaxis*/
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


mat_x_mat(o,a,b)
mat_t o,a,b;
{
/*
	(o)[ ] = (a)[ ]*(b)[ ] + (a)[ ]*(b)[ ] + (a)[ ]*(b)[ ] + (a)[ ]*(b)[ ];
*/
}
