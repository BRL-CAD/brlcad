/*                      G E T C U R V E . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file getcurve.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

#define	ARCSEGS	25	/* number of segments to use in representing a circle */

#define	myarcsinh( a )		log( a + sqrt( (a)*(a) + 1.0))

extern fastf_t splinef();
extern void Knot();
extern void B_spline();

mat_t idn={
	1.0 , 0.0 , 0.0 , 0.0 ,
	0.0 , 1.0 , 0.0 , 0.0 ,
	0.0 , 0.0 , 1.0 , 0.0 ,
	0.0 , 0.0 , 0.0 , 1.0 };


int Getcurve( curve , curv_pts )
int curve;
struct ptlist **curv_pts;
{
	int type;
	int npts = 0;
	int i,j;
	double pi;
	struct ptlist *ptr,*prev;

	pi = atan2( 0.0 , -1.0 );

	(*curv_pts) = NULL;
	prev = NULL;

	switch( dir[curve]->type )
	{
		case 110:	/* line */
		{
			point_t pt1;

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}
			(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: curv_pts" );
			ptr = (*curv_pts);

			/* Read first point */
			for( i=0 ; i<3 ; i++ )
				Readcnv( &pt1[i] , "" );
			MAT4X3PNT( ptr->pt , *dir[curve]->rot , pt1 );

			ptr->prev = NULL;
			prev = ptr;
			ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: ptr->next" );
			ptr = ptr->next;

			/* Read second point */
			for( i=0 ; i<3 ; i++ )
				Readcnv( &pt1[i] , "" );
			MAT4X3PNT( ptr->pt , *dir[curve]->rot , pt1 );
			ptr->next = NULL;
			ptr->prev = prev;

			npts = 2;
			break;
		}
		case 100:	/* circular arc */
		{
			point_t center,start,stop,tmp;
			fastf_t common_z,ang1,ang2,delta;
			double cosdel,sindel,rx,ry;

			delta = (2.0*pi)/ARCSEGS;

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}

			/* Read common Z coordinate */
			Readcnv( &common_z , "" );

			/* Read center point */
			Readcnv( &center[X] , "" );
			Readcnv( &center[Y] , "" );
			center[Z] = common_z;

			/* Read start point */
			Readcnv( &start[X] , "" );
			Readcnv( &start[Y] , "" );
			start[Z] = common_z;

			/* Read stop point */
			Readcnv( &stop[X] , "" );
			Readcnv( &stop[Y] , "" );
			stop[Z] = common_z;

			ang1 = atan2( start[Y] - center[Y] , start[X] - center[X] );
			ang2 = atan2( stop[Y] - center[Y] , stop[X] - center[X] );
			while( ang2 <= ang1 )
				ang2 += (2.0*pi);

			npts = (ang2 - ang1)/delta;
			npts++;
			if( npts < 3 )
				npts = 3;
			delta = (ang2 - ang1)/(npts-1);
			cosdel = cos( delta );
			sindel = sin( delta );

			/* Calculate points on curve */
			(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: curv_pts" );
			ptr = (*curv_pts);
			prev = NULL;

			MAT4X3PNT( ptr->pt , *dir[curve]->rot , start );

			ptr->prev = prev;
			prev = ptr;
			ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: ptr->next" );
			ptr = ptr->next;
			ptr->prev = prev;
			VMOVE( tmp , start );
			for( i=1 ; i<npts ; i++ )
			{
				rx = tmp[X] - center[X];
				ry = tmp[Y] - center[Y];
				tmp[X] = center[X] + rx*cosdel - ry*sindel;
				tmp[Y] = center[Y] + rx*sindel + ry*cosdel;
				MAT4X3PNT( ptr->pt , *dir[curve]->rot , tmp );
				prev = ptr;
				ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: ptr->next" );
				ptr = ptr->next;
				ptr->prev = prev;
			}
			ptr = prev;
			bu_free( (char *)ptr->next, "Getcurve: ptr->next" );
			ptr->next = NULL;
			break;
		}
		case 106:	/* copius data */
		{
			int	interpflag;	/* interpretation flag
							1 => x,y pairs (common z-coord)
							2 => x,y,z coords
							3 => x,y,z coords and i,j,k vectors */
			int	ntuples;	/* number of points */
			fastf_t common_z;	/* common z-coordinate */
			point_t	pt1;		/* temporary storage for incoming point */

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}

			Readint( &interpflag , "" );
			Readint( &ntuples , "" );

			switch( dir[curve]->form )
			{
				case 1:
				case 11:
				case 40:
				case 63:	/* data are coordinate pairs with common z */
				{
					if( interpflag != 1 )
					{
						bu_log( "Error in Getcurve for copius data entity D%07d, IP=%d, should be 1\n",
							dir[curve]->direct , interpflag );
						npts = 0;
						break;
					}
					Readcnv( &common_z , "" );
					(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: curv_pts" );
					ptr = (*curv_pts);
					ptr->prev = NULL;
					for( i=0 ; i<ntuples ; i++ )
					{
						Readcnv( &pt1[X] , "" );
						Readcnv( &pt1[Y] , "" );
						pt1[Z] = common_z;
						MAT4X3PNT( ptr->pt , *dir[curve]->rot , pt1 );
						prev = ptr;
						ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
							"Getcurve: ptr->next" );
						ptr = ptr->next;
						ptr->prev = prev;
						ptr->next = NULL;
					}
					ptr = ptr->prev;
					bu_free( (char *)ptr->next, "Getcurve: ptr->next" );
					ptr->next = NULL;
					npts = ntuples;
					break;
				}
				case 2:
				case 12:	/* data are coordinate triples */
				{
					if( interpflag != 2 )
					{
						bu_log( "Error in Getcurve for copius data entity D%07d, IP=%d, should be 2\n",
							dir[curve]->direct , interpflag );
						npts = 0;
						break;
					}
					(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
							"Getcurve: curv_pts" );
					ptr = (*curv_pts);
					ptr->prev = NULL;
					for( i=0 ; i<ntuples ; i++ )
					{
						Readcnv( &pt1[X] , "" );
						Readcnv( &pt1[Y] , "" );
						Readcnv( &pt1[Z] , "" );
						MAT4X3PNT( ptr->pt , *dir[curve]->rot , pt1 );
						prev = ptr;
						ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
							"Getcurve: ptr->next" );
						ptr = ptr->next;
						ptr->prev = prev;
					}
					ptr = ptr->prev;
					bu_free( (char *)ptr->next, "Getcurve: ptr->next" );
					ptr->next = NULL;
					npts = ntuples;
					break;
				}
#if 0
				case 20:
				case 21:
				{
					if( interpflag != 1 )
					{
						bu_log( "Error in Getcurve for copius data entity D%07d, IP=%d, should be 1\n",
							dir[curve]->direct , interpflag );
						npts = 0;
						break;
					}
					Readcnv( &common_z , "" );
					(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: curv_pts" );
					ptr = (*curv_pts);
					ptr->prev = NULL;
					for( i=0 ; i<ntuples ; i += 2 )
					{
						Readcnv( &pt1[X] , "" );
						Readcnv( &pt1[Y] , "" );
						pt1[Z] = common_z;
						MAT4X3PNT( ptr->pt , *dir[curve]->rot , pt1 );
						prev = ptr;
						ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
							"Getcurve: ptr->next" );
						ptr = ptr->next;
						ptr->prev = prev;
						ptr->next = NULL;
					}
					ptr = ptr->prev;
					bu_free( (char *)ptr->next, "Getcurve: ptr->next" );
					ptr->next = NULL;
					npts = ntuples;
					break;
				}
#endif
				default:
				{
					bu_log( "Error in Getcurve for copius data entity D%07d, form %d is not a legal choice\n",
						dir[curve]->direct , dir[curve]->form );
					npts = 0;
					break;
				}
			}
			break;
		}
		case 112:	/* parametric spline */
		{
			struct spline *splroot;
			struct segment *seg,*seg1;
			vect_t tmp;
			fastf_t a;

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}
	   		Readint( &i , "" );	/* Skip over type */
	   		Readint( &i , "" );	/* Skip over continuity */
	   		splroot = (struct spline *)bu_malloc( sizeof( struct spline ),
					"Getcurve: splroot" );
	   		splroot->start = NULL;
	   		Readint( &splroot->ndim , "" ); /* 2->planar, 3->3d */
	   		Readint( &splroot->nsegs , "" ); /* Number of segments */
	   		Readdbl( &a , "" );	/* first breakpoint */

	   		/* start a linked list of segments */
	   		seg = splroot->start;
	   		for( i=0 ; i<splroot->nsegs ; i++ )
	   		{
	   			if( seg == NULL )
	   			{
	   				seg = (struct segment *)bu_malloc( sizeof( struct segment ),
						"Getcurve: seg" );
	   				splroot->start = seg;
	   			}
	   			else
	   			{
	   				seg->next = (struct segment *)bu_malloc( sizeof( struct segment ),
						"Getcurve: seg->next" );
	   				seg = seg->next;
	   			}
	   			seg->segno = i+1;
	   			seg->next = NULL;
	   			seg->tmin = a; /* set minimum T for this segment */
	   			Readflt( &seg->tmax , "" ); /* get maximum T for segment */
	   			a = seg->tmax;
	   		}

	   		/* read coefficients for polynomials */
	   		seg = splroot->start;
	   		for( i=0 ; i<splroot->nsegs ; i++ )
	   		{
	   			for( j=0 ; j<4 ; j++ )
	   				Readflt( &seg->cx[j] , "" ); /* x coeff's */
 	   			for( j=0 ; j<4 ; j++ )
	   				Readflt( &seg->cy[j] , "" ); /* y coeff's */
	   			for( j=0 ; j<4 ; j++ )
	   				Readflt( &seg->cz[j] , "" ); /* z coeff's */
	   			seg = seg->next;
	   		}

	   		/* Calculate points */

			(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: curv_pts" );
			ptr = (*curv_pts);
			prev = NULL;
			ptr->prev = NULL;

			npts = 0;
	   		seg = splroot->start;
	   		while( seg != NULL )
	   		{
	   			/* plot 9 points per segment (This should
	   				be replaced by some logic) */
	   			for( i=0 ; i<9 ; i++ )
	   			{
	   				a = (fastf_t)i/(8.0)*(seg->tmax-seg->tmin );
	   				tmp[0] = splinef( seg->cx , a );
	   				tmp[1] = splinef( seg->cy , a );
	   				if( splroot->ndim == 3 )
	   					tmp[2] = splinef( seg->cz , a );
	   				else
	   					tmp[2] = seg->cz[0];
	   				MAT4X3PNT( ptr->pt , *dir[curve]->rot , tmp );
	   				for( j=0 ; j<3 ; j++ )
	   					ptr->pt[j] *= conv_factor;
	   				npts++;
					prev = ptr;
					ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: ptr->next" );
					ptr = ptr->next;
	   				ptr->prev = prev;
	   			}
	   			seg = seg->next;
	   		}
			ptr = ptr->prev;
			bu_free( (char *)ptr->next, "Getcurve: ptr->next" );
			ptr->next = NULL;

	   		/* free the used memory */
	   		if( splroot != NULL )
	   		{
	   			seg = splroot->start;
	   			while( seg != NULL )
	   			{
	   				seg1 = seg;
	   				seg = seg->next;
	   				bu_free( (char *)seg1, "Getcurve: seg1" );
	   			}
	   			bu_free( (char *)splroot, "Getcurve: splroot" );
	   			splroot = NULL;
	   		}
			break;
		}
		case 104:	/* conic arc */
		{
			double A,B,C,D,E,F,a,b,c,del,I,theta,dpi,t1,t2,xc,yc;
			point_t v1,v2,tmp;
			mat_t rot1;
			int num_points;

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}

	   		/* read coefficients */
			Readdbl( &A , "" );
			Readdbl( &B , "" );
			Readdbl( &C , "" );
			Readdbl( &D , "" );
			Readdbl( &E , "" );
			Readdbl( &F , "" );

	   		/* read common z-coordinate */
			Readflt( &v1[2] , "" );
			v2[2]=v1[2];

	   		/* read start point */
			Readflt( &v1[0] , "" );
			Readflt( &v1[1] , "" );

	   		/* read terminate point */
			Readflt( &v2[0] , "" );
			Readflt( &v2[1] , "" );

	   		type = 0;
	   		if( dir[curve]->form == 1 ) /* Ellipse */
	   		{
	   			if( fabs( E ) < SMALL )
	   				E = 0.0;
	   			if( fabs( B ) < SMALL )
	   				B = 0.0;
	   			if( fabs( D ) < SMALL )
	   				D = 0.0;

	   			if( B == 0.0 && D == 0.0 && E == 0.0 )
	   				type = 1;
	   			else
	   				bu_log( "Entity #%d is an incorrectly formatted ellipse\n" , curve );
	   		}

	   		/* make coeff of X**2 equal to 1.0 */
	   		a = A*C - B*B/4.0;
	   		if( fabs(a) < 1.0  && fabs(a) > TOL )
	   		{
	   			a = fabs(A);
	   			if( fabs(B)<a && B != 0.0 )
	   				a = fabs(B);
	   			if( fabs(C)<a )
	   				a = fabs(C);
	   			A = A/a;
	   			B = B/a;
	   			C = C/a;
	   			D = D/a;
	   			E = E/a;
	   			F = F/a;
		   		a = A*C - B*B/4.0;
	   		}

	   		if( !type )
	   		{
		   		/* check for type of conic */
		   		del = A*(C*F-E*E/4.0)-0.5*B*(B*F/2.0-D*E/4.0)+0.5*D*(B*E/4.0-C*D/2.0);
		   		I = A+C;
		   		if( del == 0.0 ) /* not a conic */
		   		{
		   			bu_log( "Entity #%d, claims to be conic arc, but isn't\n" , curve );
		   			break;
		   		}
		   		else if( a > 0.0 && del*I < 0.0)
		   			type = 1; /* ellipse */
		   		else if( a < 0.0 )
		   			type = 2; /* hyperbola */
		   		else if( a == 0.0 )
		   			type = 3; /* parabola */
		   		else	/* imaginary ellipse */
		   		{
		   			bu_log( "Entity #%d is an imaginary ellipse!!\n", curve );
		   			break;
		   		}
	   		}

	   		switch( type )
	   		{

	   			double p,r1;

	   		    case 3:	/* parabola */

	   			/* make A+C == 1.0 */
	   			if( A+C != 1.0 )
	   			{
	   				b = A+C;
		   			A = A/b;
		   			B = B/b;
		   			C = C/b;
		   			D = D/b;
		   			E = E/b;
		   			F = F/b;
	   			}

	   			/* theta is the angle that the parabola axis is rotated
	   				about the origin from the x-axis */
	   			theta = 0.5*atan2( B , C-A );

	   			/* p is the distance from vertex to directrix */
	   			p = (-E*sin(theta) - D*cos(theta))/4.0;
	   			if( fabs( p ) < TOL )
	   			{
	   				bu_log( "Cannot plot entity %d, p=%g\n" , curve , p );
	   				break;
	   			}

	   			/* calculate vertex (xc,yc). This is based on the
	   				parametric representation:
					   x = xc + a*t*t*cos(theta) - t*sin(theta)
	   				   y = yc + a*t*t*sin(theta) + t*cos(theta)
	   				and the fact that v1 and v2 are on the curve
									*/
	   			a = 1.0/(4.0*p);
	   			b = ((v1[0]-v2[0])*cos(theta) + (v1[1]-v2[1])*sin(theta))/a;
				c = ((v1[1]-v2[1])*cos(theta) - (v1[0]-v2[0])*sin(theta));
	   			if( fabs( c ) < TOL*TOL )
	   			{
	   				bu_log( "Cannot plot entity %d\n" , curve );
	   				break;
	   			}
	   			b = b/c;
	   			t1 = (b + c)/2.0; /* vaule of 't' at v1 */
	   			t2 = (b - c)/2.0; /* value of 't' at v2 */
	   			xc = v1[0] - a*t1*t1*cos(theta) + t1*sin(theta);
	   			yc = v1[1] - a*t1*t1*sin(theta) - t1*cos(theta);

		   		/* Calculate points */

				(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: curv_pts" );
				ptr = (*curv_pts);
	   			ptr->prev = NULL;
	   			prev = NULL;

				npts = 0;
	   			num_points = ARCSEGS+1;
	   			dpi = (t2-t1)/(double)num_points; /* parameter incremenmt */

	   			/* start point */
	   			VSET( tmp, xc, yc, v1[2] );
   				MAT4X3PNT( ptr->pt , *dir[curve]->rot , tmp );
	   			VSCALE( ptr->pt , ptr->pt , conv_factor );
   				npts++;
				prev = ptr;
				ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: ptr->next" );
				ptr = ptr->next;
				ptr->prev = prev;


	   			/* middle points */
	   			b = cos( theta );
	   			c = sin( theta );
	   			for( i=1 ; i<num_points-1 ; i++ )
	   			{
	   				r1 = t1 + dpi*i;
	   				tmp[0] = xc + a*r1*r1*b - r1*c;
	   				tmp[1] = yc + a*r1*r1*c + r1*b;
	   				MAT4X3PNT( ptr->pt , *dir[curve]->rot , tmp );
	   				VSCALE( ptr->pt , ptr->pt , conv_factor );
	   				npts++;
					prev = ptr;
					ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: ptr->next" );
					ptr = ptr->next;
					ptr->prev = prev;
	   			}

	   			/* plot terminate point */
	   			tmp[0] = v2[0];
	   			tmp[1] = v2[1];
   				MAT4X3PNT( ptr->pt , *dir[curve]->rot , tmp );
   				for( j=0 ; j<3 ; j++ )
   					ptr->pt[j] *= conv_factor;
   				npts++;
	   			ptr->next = NULL;
	   			break;

	   		    case 1:	/* ellipse */
	   		    case 2:	/* hyperbola */
	   		    {
	   		    	double A1,C1,F1,alpha,beta;
	   		    	mat_t rot2;
	   		    	point_t v3,tmp2;

	   			/* calculate center of ellipse or hyperbola */
		   		xc = (B*E/4.0 - D*C/2.0)/a;
		   		yc = (B*D/4.0 - A*E/2.0)/a;

	   			/* theta is angle that the curve axis is rotated about
	   				the origin from the x-axis */
	   			if( B != 0.0 )
		   			theta = 0.5*atan2( B , A-C );
	   			else
	   				theta = 0.0;

	   			/* calculate coeff's for same curve, but with
	   				vertex at origin and theta = 0.0 */
	   			A1 = A + 0.5*B*tan(theta);
	   			C1 = C - 0.5*B*tan(theta);
	   			F1 = F - A*xc*xc - B*xc*yc - C*yc*yc;


	   			if( type == 2 && F1/A1 > 0.0 )
	   				theta += pi/2.0;

	   			/* set-up matrix to translate and rotate
	   				the start and terminate points to match
	   				the simpler curve (A1, C1, and F1 coeff's)	*/

	   			for( i=0 ; i<16 ; i++ )
		   			rot1[i] = idn[i];
	   			MAT_DELTAS( rot1 , -xc , -yc , 0.0 );
	   			MAT4X3PNT( tmp , rot1 , v1 );
	   			VMOVE( v1 , tmp );
	   			MAT4X3PNT( tmp , rot1 , v2 );
	   			VMOVE( v2 , tmp );
	   			MAT_DELTAS( rot1 , 0.0 , 0.0 , 0.0 );
	   			rot1[0] = cos( theta );
	   			rot1[1] = sin( theta );
	   			rot1[4] = (-rot1[1]);
	   			rot1[5] = rot1[0];
	   			MAT4X3PNT( tmp , rot1 , v1 );
	   			VMOVE( v1 , tmp );
	   			MAT4X3PNT( tmp , rot1 , v2 );
	   			VMOVE( v2 , tmp );
	   			MAT_DELTAS( rot1 , 0.0 , 0.0 , 0.0 );

	   			/* calculate:
	   				alpha = start angle
	   				beta = terminate angle
	   					*/
	   			beta = 0.0;
		   		if( v2[0] == v1[0] && v2[1] == v1[1] ) /* full circle */
	   			{
	   				alpha = 0.0;
		   			beta = 2.0*pi;
	   			}
   				a = sqrt( fabs(F1/A1) ); /* semi-axis length */
   				b = sqrt( fabs(F1/C1) ); /* semi-axis length */

	   			if( type == 1 ) /* ellipse */
	   			{
	   				alpha = atan2( a*v1[1] , b*v1[0] );
	   				if( beta == 0.0 )
	   				{
		   				beta = atan2( a*v2[1] , b*v2[0] );
		   				beta = beta - alpha;
	   				}
	   			}
	   			else /* hyperbola */
	   			{
	   				alpha = myarcsinh( v1[1]/b );
	   				beta = myarcsinh( v2[1]/b );
	   				if( fabs( a*cosh(beta) - v2[0] ) > 0.01 )
						a = (-a); 
	   				beta = beta - alpha;
		   		}
	   			num_points = ARCSEGS;

	   			/* set-up matrix to translate and rotate
	   				the simpler curve back to the original
	   				position */

	   			MAT_DELTAS( rot1 , xc , yc , 0.0 );
	   			rot1[1] = (-rot1[1]);
	   			rot1[4] = (-rot1[4]);
	   			Matmult( *(dir[curve]->rot) , rot1 , rot2 );

	   			/* calculate start point */
				(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: curv_pts" );
				ptr = (*curv_pts);
	   			prev = NULL;
	   		    	ptr->prev = NULL;
				npts = 0;
	   		    	VSCALE( v3 , v1 , conv_factor );
   				MAT4X3PNT( ptr->pt , rot2 , v3 );
	   			npts++;
				prev = ptr;
				ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: ptr->next" );
				ptr = ptr->next;
				ptr->prev = prev;

	   			/* middle points */
	   			for( i=1 ; i<num_points ; i++ )
	   			{
	   				theta = alpha + (double)i/(double)num_points*beta;
	   				if( type == 2 )
	   				{
		   				tmp2[0] = a*cosh(theta);
		   				tmp2[1] = b*sinh(theta);
	   				}
	   				else
	   				{
	   					tmp2[0] = a*cos(theta);
	   					tmp2[1] = b*sin(theta);
	   				}
	   				VSCALE( tmp2 , tmp2 , conv_factor );
	   				MAT4X3PNT( ptr->pt , rot2 , tmp2 );
	   				npts++;
					prev = ptr;
					ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
						"Getcurve: ptr->next" );
					ptr = ptr->next;
					ptr->prev = prev;
	   			}

	   			/* terminate point */
	   		    	VSCALE( v2 , v2 , conv_factor );
   				MAT4X3PNT( ptr->pt , rot2 , v2 );
   				npts++;
				ptr->next = NULL;
	   		    	break;
	   		    }
	   		}
			break;
		}
		case 102:	/* composite curve */
		{

			int ncurves,*curvptr;
			struct ptlist *tmp_ptr;

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}

			Readint( &ncurves , "" );
			curvptr = (int *)bu_calloc( ncurves , sizeof( int ), "Getcurve: curvptr" );
			for( i=0 ; i<ncurves ; i++ )
			{
				Readint( &curvptr[i] , "" );
				curvptr[i] = (curvptr[i]-1)/2;
			}

			npts = 0;
			(*curv_pts) = NULL;
			for( i=0 ; i<ncurves ; i++ )
			{
				npts += Getcurve( curvptr[i] , &tmp_ptr );
				if( (*curv_pts) == NULL )
					(*curv_pts) = tmp_ptr;
				else
				{
					ptr = (*curv_pts);
					while( ptr->next != NULL )
						ptr = ptr->next;
					ptr->next = tmp_ptr;
					ptr->next->prev = ptr;
					if( EQUAL( ptr->pt[X] , tmp_ptr->pt[X] ) &&
					    EQUAL( ptr->pt[Y] , tmp_ptr->pt[Y] ) &&
					    EQUAL( ptr->pt[Z] , tmp_ptr->pt[Z] ) )
					{
						ptr->next = ptr->next->next;
						if( ptr->next != NULL )
							ptr->next->prev = ptr;
						bu_free( (char *)tmp_ptr, "Getcurve: tmp_ptr" );
						npts--;
					}
				}
			}
			break;
		}
		case 126:	/* rational B-spline */
		{
			int k,m,n,a,prop1,prop2,prop3,prop4;
			fastf_t *t;	/* knot values */
			fastf_t *w;	/* weights */
			point_t *cntrl_pts;	/* control points */
			fastf_t v0,v1;	/* starting and stopping parameter values */
			fastf_t v;	/* current parameter value */
			fastf_t delv;	/* parameter increment */

			Readrec( dir[curve]->param );
			Readint( &type , "" );
			if( type != dir[curve]->type )
			{
				bu_log( "Error in Getcurve, looking for curve type %d, found %d\n" ,
					dir[curve]->type , type );
				npts = 0;
				break;
			}

			Readint( &k , "" );
			Readint( &m , "" );
			Readint( &prop1 , "" );
			Readint( &prop2 , "" );
			Readint( &prop3 , "" );
			Readint( &prop4 , "" );

			n = k-m+1;
			a = n+2*m;

			t = (fastf_t *)bu_calloc( a+1 , sizeof( fastf_t ) , "Getcurve: spline t" );
			for( i=0 ; i<a+1 ; i++ )
				Readflt( &t[i] , "" );
			Knot( a+1 , t );

			w = (fastf_t *)bu_calloc( k+1 , sizeof( fastf_t ) , "Getcurve: spline w" );
			for( i=0 ; i<k+1 ; i++ )
				Readflt( &w[i] , "" );

			cntrl_pts = (point_t *)bu_calloc( k+1 , sizeof( point_t ) , "Getcurve: spline cntrl_pts" );
			for( i=0 ; i<k+1 ; i++ )
			{
				fastf_t tmp;

				for( j=0 ; j<3 ; j++ )
				{
					Readcnv( &tmp , "" );
					cntrl_pts[i][j] = tmp;
				}
			}

			Readflt( &v0 , "" );
			Readflt( &v1 , "" );

			delv = (v1 - v0)/((fastf_t)(3*k));

	   		/* Calculate points */

			(*curv_pts) = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
				"Getcurve: curv_pts" );
			ptr = (*curv_pts);
			ptr->prev = NULL;
			prev = NULL;
			npts = 0;
			v = v0;
			while( v < v1 )
			{
				point_t tmp;

				B_spline( v , k , m+1 , cntrl_pts , w , tmp );
   				MAT4X3PNT( ptr->pt , *dir[curve]->rot , tmp );
   				npts++;
				prev = ptr;
				ptr->next = (struct ptlist *)bu_malloc( sizeof( struct ptlist ),
					"Getcurve: ptr->next" );
				ptr = ptr->next;
				ptr->prev = prev;

				v += delv;
			}
			VMOVE( ptr->pt , cntrl_pts[k] );
			npts++;
			ptr->next = NULL;

			/* Free memory */
			Freeknots();
			bu_free( (char *)cntrl_pts , "Getcurve: spline cntrl_pts" );
			bu_free( (char *)w, "Getcurve: spline w" );
			bu_free( (char *)t, "Getcurve: spline t" );

			break;
		}
	}
	return( npts );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
