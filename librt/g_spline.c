/*
 *			S P L I N E . C
 *
 *		********************************************
 *		* WARNING:  EXPERIMENTAL NON-WORKING CODE! *
 *		********************************************
 *
 *  Purpose -
 *	Intersect a ray with a set of spline patches
 *	which together form a solid object
 *
 *  Authors -
 *	Dr. Dave Rogers		(Analysis)
 *	Mark G. Daghir		(Programming)
 *	Michael John Muuss	(Integration)
 *  
 *  Source -
 *	United States Naval Academy
 *  
 */
#ifndef lint
static char RCSspline[] = "@(#)$Header $ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *  Algorithm:
 *  
 */

struct surf {
	struct surf	*spl_forw;
	short		spl_order[2];
	short		spl_kv_size[2];
	int		*spl_ku;
	int		*spl_kw;
	short		spl_ctl_size[2];
	float		*spl_mesh;
#define SUBDIVLVL 4
	fastf_t		spl_uinterv[SUBDIVLVL];	/* interval list */
	fastf_t		spl_winterv[SUBDIVLVL];
	int		spl_lvl;	/* current level (remembered) */
	int		spl_olvl;	/* prev level (remembered) */
	fastf_t		spl_ulow[SUBDIVLVL];	/* list of U lower limits */
	fastf_t		spl_wlow[SUBDIVLVL];	/* list of W lower limits */
};
#define SPL_NULL	((struct surf *)0)

int lp1[4], lp2[4];	/* how much tosubdivide for each level */

/*
 *  			S P L _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid ellipsoid, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	ELL is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct surf is created, and it's address is stored in
 *  	stp->st_specific for use by spl_shot().
 */
spl_prep( vec, stp, mat )
register fastf_t *vec;
struct soltab *stp;
matp_t mat;			/* Homogenous 4x4, with translation, [15]=1 */
{
	struct surf *list;
	register int i, j;
	static union record rec;
	register float *vp;
	int cur_gran;
	int nby;
	float *fp;

	/* Global information */
	lp1[1] = lp2[1] = 11;
	lp1[2] = lp2[2] = 6;
	lp1[3] = lp2[3] = 3;

	list = (struct surf *)0;
	cur_gran = 1;
	while(1)  {
/***	while( cur_gran < dp->d_len )  { ***/
		register struct surf *spl;

/***		db_getrec( dp, &rec, cur_gran++ ); ***/
		i = fread( (char *)&rec, sizeof(rec), 1, rt_i.fp );
		if( feof(rt_i.fp) )  break;
		if( i != 1 )
			rt_bomb("spl_prep:  read error");
		cur_gran++;

		if( rec.u_id != ID_BSURF )  {
			break;
		}
		if( rec.d.d_geom_type != 3 && rec.d.d_geom_type != 4 )  {
			printf("BSURF geom_type=%d?\n", rec.d.d_geom_type);
			return(-1);
		}
		GETSTRUCT( spl, surf );
		spl->spl_forw = list;
		list = spl;
		spl->spl_order[0] = rec.d.d_order[0];
		spl->spl_order[1] = rec.d.d_order[1];
		spl->spl_ctl_size[0] = rec.d.d_ctl_size[0];
		spl->spl_ctl_size[1] = rec.d.d_ctl_size[1];

		/* Read in spl_ku, spl_kw */
		spl->spl_kv_size[0] = rec.d.d_kv_size[0];
		spl->spl_kv_size[1] = rec.d.d_kv_size[1];
		nby = rec.d.d_nknots * sizeof(union record);
		if( (vp = (float *)malloc(nby)) == (float *)0 )  {
			printf("draw_spline:  malloc error\n");
			return(-1);
		}
		fp = vp;
/***		db_getmany( dp, (char *)vp, cur_gran, rec.d.d_nknots ); ***/
		i = read( (char *)vp, nby, 1, rt_i.fp );
		if( i != 1 )  rt_bomb("spl_prep:  knot read");
		cur_gran += rec.d.d_nknots;

		spl->spl_ku = (int *)malloc( spl->spl_kv_size[0]*sizeof(int) );
		spl->spl_kw = (int *)malloc( spl->spl_kv_size[1]*sizeof(int) );
		for( i=0; i<spl->spl_kv_size[0]; i++ )
			spl->spl_ku[i] = (int)*vp++;
		for( i=0; i<spl->spl_kv_size[1]; i++ )
			spl->spl_kw[i] = (int)*vp++;
		(void)free( (char *)fp);

		/* Read in control mesh */
		nby = rec.d.d_nctls * sizeof(union record);
		if( (spl->spl_mesh = (float *)malloc(nby)) == (float *)0 )  {
			printf("draw_spline:  malloc error\n");
			return(-1);
		}
/***		db_getmany( dp, (char *)spl->spl_mesh, cur_gran, rec.d.d_nctls ); ***/
		i = fread( (char *)spl->spl_mesh, nby, 1, rt_i.fp );
		if( i != 1 )  rt_bomb("spl_prep:  knot read");
		cur_gran += rec.d.d_nctls;

		/* Transform all the control points */
		vp = spl->spl_mesh;
		i = rec.d.d_ctl_size[0]*rec.d.d_ctl_size[1];
		for( ; i>0; i--, vp += rec.d.d_geom_type )  {
			static vect_t	homog;
			if( rec.d.d_geom_type == 3 )  {
				MAT4X3PNT( homog, mat, vp );
				VMOVE( vp, homog );
			} else {

#define HDIVIDE(a,b)  \
	(a)[X] = (b)[X] / (b)[H];\
	(a)[Y] = (b)[Y] / (b)[H];\
	(a)[Z] = (b)[Z] / (b)[H];
				HDIVIDE( homog, vp );
				MAT4X3PNT( vp, mat, homog );
				/* Leaves us with [x,y,z,1] */
			}
#define MINMAX(a,b,c)	{ FAST fastf_t ftemp;\
			if( (ftemp = (c)) < (a) )  a = ftemp;\
			if( ftemp > (b) )  b = ftemp; }

#define MM(v)	MINMAX( stp->st_min[X], stp->st_max[X], v[X] ); \
		MINMAX( stp->st_min[Y], stp->st_max[Y], v[Y] ); \
		MINMAX( stp->st_min[Z], stp->st_max[Z], v[Z] )
			MM( vp );
		}

		/* Preparations */
		/* Build the interval list in parametric space */
		spl->spl_uinterv[1] = (float)(spl->spl_ctl_size[0] - spl->spl_order[0] + 1) / (lp1[1] - 1);
		spl->spl_uinterv[2] = spl->spl_uinterv[1] / (lp1[2] - 1);
		spl->spl_uinterv[3] = spl->spl_uinterv[2] / (lp1[3] - 1);
		spl->spl_winterv[1] = (float)(spl->spl_ctl_size[1] - spl->spl_order[1] + 1) / (lp2[1] - 1);
		spl->spl_winterv[2] = spl->spl_winterv[1] / (lp2[2] - 1);
		spl->spl_winterv[3] = spl->spl_winterv[2] / (lp2[3] - 1);
		/* Set initial level and lower left corner */
		spl->spl_lvl = 1;	/* coarsest level */
		spl->spl_olvl = 2;
		spl->spl_ulow[spl->spl_lvl] = spl->spl_ulow[spl->spl_olvl] = 0;	/* Low values */
		spl->spl_wlow[spl->spl_lvl] = spl->spl_wlow[spl->spl_olvl] = 0;
	}

	/* Solid is OK, compute constant terms now */
	stp->st_specific = (int *)list;

	VSET( stp->st_center,
		(stp->st_max[X] + stp->st_min[X])/2,
		(stp->st_max[Y] + stp->st_min[Y])/2,
		(stp->st_max[Z] + stp->st_min[Z])/2 );

	/* An enclosing bounding sphere */
	{
		fastf_t f, dx, dy, dz;
		dx = (stp->st_max[X] - stp->st_min[X])/2;
		f = dx;
		dy = (stp->st_max[Y] - stp->st_min[Y])/2;
		if( dy > f )  f = dy;
		dz = (stp->st_max[Z] - stp->st_min[Z])/2;
		if( dz > f )  f = dz;
		stp->st_aradius = f;
		stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
	}
	return(0);			/* OK */
}

spl_print( stp )
register struct soltab *stp;
{
	register struct surf *spl =
		(struct surf *)stp->st_specific;
	register int i;

	for( ; spl != SPL_NULL; spl = spl->spl_forw ) {
		printf("order %d x %d, kv_size %d x %d\n",
			spl->spl_order[0],
			spl->spl_order[1],
			spl->spl_kv_size[0],
			spl->spl_kv_size[1] );
		printf("control mesh %d x %d\n",
			spl->spl_ctl_size[0],
			spl->spl_ctl_size[1] );
		for( i=0; i<spl->spl_kv_size[0]; i++ )
			printf("%d ", spl->spl_ku[i] );
		printf("\n");
		for( i=0; i<spl->spl_kv_size[1]; i++ )
			printf("%d ", spl->spl_kw[i] );
		printf("\n");
	}
}

/*
 *  			S P L _ S H O T
 *  
 *  Intersect a ray with a set of spline patches.
 *  If an intersection occurs, a struct seg will be acquired and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *  	segp	HIT
 */
struct seg *
spl_shot( stp, rp )
struct soltab *stp;
register struct xray *rp;
{
	register struct surf *spl =
		(struct surf *)stp->st_specific;
	register struct seg *segp;
	LOCAL fastf_t	k1, k2;		/* distance constants of solution */
	int pointer[4];		/* indexs of current triangle points, clockwise */
	double refined[11*11*3];	/* refined array pts, ROW major */
	int npts;	/* number of refined points */
	int ntri;	/* number of refined triangles */
	double norm[6][4]; /* normal vectorS. */
	int hole;	/* flag */

	/* FIRST VERSION:  Just a single surface */
/*** should be able to remove lvl reset ****/
	spl->spl_lvl = 1;	/* restart: top lvl */
	spl->spl_olvl = 2;
	hole = 0;	/* flag */

labela:
printf("%d: low u,w=%f,%f\n", spl->spl_lvl, spl->spl_ulow[spl->spl_lvl], spl->spl_wlow[spl->spl_lvl] );
	bsurf3(
		spl->spl_ctl_size[0], spl->spl_ctl_size[1],
		spl->spl_order[0], spl->spl_order[1], 
		lp1[spl->spl_lvl], lp2[spl->spl_lvl],
		spl->spl_ulow[spl->spl_lvl], spl->spl_wlow[spl->spl_lvl],
		spl->spl_uinterv[spl->spl_lvl], spl->spl_winterv[spl->spl_lvl],
		refined, spl);

	if( cybk2( refined, 
		rp,
		norm,
		pointer, &k1,
		lp1[spl->spl_lvl], lp2[spl->spl_lvl]
	  ) == 0 )  {
	  	/* MISSED */
	  	if( spl->spl_lvl == 1 )
	  		return(SEG_NULL);	/* top-level MISS */

		/* MISS at current level  Go to next larger (cruder) area */
		spl->spl_olvl = spl->spl_lvl--;

		/* If get to index level 1 twice, no intersection */
		if(spl->spl_lvl == 1)  {
			if(hole == 1)  {
				spl->spl_lvl = 1;	/* restart: top lvl */
				spl->spl_olvl = 2;
				return(SEG_NULL);	/* MISS */
			}
			hole = 1;
		}
		goto labela;
	}

	if(rt_g.debug&DEBUG_SPLINE)printf("hit at lvl %d\n", spl->spl_lvl);
	if (spl->spl_lvl < 3)  {
		int wintrval;
		/* Have a hit, want to be certain that this is accurate.
		 * If not the finest level, go to the finest level.
		 */
		spl->spl_olvl = spl->spl_lvl++;
		/* pointer[1] is lower left corner */
		wintrval = (int)(pointer[1] / lp1[spl->spl_olvl]);

		/* New lower limit values for next iteration */
		spl->spl_ulow[spl->spl_lvl] =
			((((pointer[1]) % lp1[spl->spl_olvl]) - 1) *
			spl->spl_uinterv[spl->spl_olvl]) +
			spl->spl_ulow[spl->spl_olvl];
		spl->spl_wlow[spl->spl_lvl] = spl->spl_winterv[spl->spl_olvl] * wintrval + spl->spl_wlow[spl->spl_olvl];
		goto labela;
	}

	/* We have a hit */
	GET_SEG(segp);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = k1;
	VJOIN1( segp->seg_in.hit_point, rp->r_pt, segp->seg_in.hit_dist, rp->r_dir );
	VSET( segp->seg_in.hit_normal, norm[5][1], norm[5][2], norm[5][3] );
	VUNITIZE( segp->seg_in.hit_normal );
	segp->seg_out.hit_dist = k1+0.1;	/* fake it */
	VJOIN1( segp->seg_out.hit_point, rp->r_pt, segp->seg_out.hit_dist, rp->r_dir );
	VREVERSE( segp->seg_out.hit_normal, segp->seg_in.hit_normal );
	return(segp);			/* HIT */
}

/*
 *  			S P L _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
spl_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	register struct surf *ell =
		(struct surf *)stp->st_specific;

}

/*
 *  			S P L _ U V
 *  
 */
spl_uv( ap, stp, hitp, uvp )
struct application *ap;
struct soltab *stp;
register struct hit *hitp;
register struct uvcoord *uvp;
{
	register struct surf *ell =
		(struct surf *)stp->st_specific;
	uvp->uv_u = .2;
	uvp->uv_v = .2;
	uvp->uv_du = uvp->uv_dv = 0;
}
/*** bsurf3 ***/

/*********************************************************************

cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
c
c name: bsurf3.c - b-spline surface
c
c date: 7 February 1986
c
c purpose: to generate the points of a b-spline surface
c          form a ploygon net.
c
c method:  refer to "mathematical elements for computer graphics",
c          rogers & adams, chapter 6, section 6-11, eq. 6-51.
c
c author:
c        david f. rogers
c        computer aided design and interactive graphics group
c        u. s. naval academy
c
c language: pdp-11 rt-11 v03b fortran
c
c filename: bsurf.c
c
c
c parameters:
c        
c        b()= array containing the polygon net points
c        b(i)= x-component
c        b(i+1)= y-component
c        b(i+2)= z-component
c        b(1)-b(3*n2)= first row in w direction
c        b((n1-1)*3*n2+1)-b(n1*3*n2)= last row in w direction
c           the values are stored as b(i,j) with i fixed and j varying
c           in groups of three x,y,z components.
c        
c        q()= array containing the resulting surface
c        q(i)= x-componwnt
c        q(i+1)= y-component
c        q(i+2)= z-component
c        q((p2-1)*3*p1+1)-q(p2*3*p1) last row in u direction
c           the first 3*p2 values are for u=0 with w varying in groups
c           of 3 as x,y,z components followed by the second group of 3*p2
c           values with u=xmax/(p1-1) with w varying etc.
c
c
c        c1= order in the u direction
c        c2= order in the w direction
c
c        n1= number of polygon points in the u direction
c        n2= number of polygon points in the w direction
c            thus the polygon net is n1 x n2
c
c        p1= number of pts on the display surface/w in u direction
c        p2= number of pts on the display surface/u in the w direction
c            thus the resulting surface is p1 x p2
c
c        x()=  array to contain the knot vector in u direction.
c              maximum value is n1-c1+1.
c              number of elements is n1+c1.
c        y()=  array to contain the knot vector in w  direction.
c              maximum value is n2-c2+1.
c              number of elements is nz+c2.
c
c        n() =  array to contain the basis function in the w direction.
c               the first group of p1 values is for  u=0, the second
c               group of p1 values is for u=xmax/(p1-1), etc.
c
c        m() =  array containing the basis functions in the w direction.
c               the firrst group of p2 values is for w=0, the second group
c               of p2 values for w=ymax/(p2-1), etc.
c
*********************************************************************/

bsurf3(n1,n2,c1,c2,p1,p2,ulo,wlo,stempu,stempw,q,spl)
int c1,c2,n1,n2,p1,p2;
double *q;
double ulo,wlo,stempu,stempw;
struct surf *spl;
{
	double t1, t2, t3, nbasis, nmbas, u1, w1;
	double n[100], m[100];		/* basis functions */
	int p1m1, p2m1, k1n1, kn2;
	int n1plc1, n2plc2, l, i, i1, i2, i3;
	int j, j2, j3, k1, k;

	t1 = 0;
	t2 = 0;
	t3 = 0;

	/*------------------precalculate certain often used values         */
	n1plc1 = n1 + c1;
	n2plc2 = n2 + c2;
	p1m1 = p1 - 1;
	p2m1 = p2 - 1;

	/*	calculate basis functions */

	/*---------------------------------set n element counter           */
	i1=0;

	/*----------------calculate u basis function n for each u          */
	for(i=0;i<=p1m1;i++)
	{
		/*-increment in parameter u  0<=u<=xmax          */
		u1 = i * stempu + ulo;
		basis(c1,u1,n1,n1plc1,spl->spl_ku,&i1,m);

	}

	/*----------------------------------set m element counter          */
	i2=0;

	/*----------------calculate w basis function m for each w          */
	for(i=0;i<=p2m1;i++)  {
		/*----------increment in parameter w   0<=w<=ymax          */
		w1 = i * stempw + wlo;
		basis(c2,w1,n2,n2plc2,spl->spl_kw,&i2,n);
	}

	/*	generate completely new surface  */
	/*-----------------------initialize surface array counter          */
	i3=0;
	for(k1=0; k1<=p1m1; k1++)  	{
		k1n1=k1*n1;

		/*-!calc. along w for fixed u           */
		for(k=0; k<=p2m1; k++)  {
			register float *vp;
			/* !initialize surface elements to round up           */
			/* !when truncating result to integer.           */
                        t1=t2=t3=0;
			kn2=k*n2;
			vp = spl->spl_mesh;

			/* !vary n for fixed u           */
			for(l=1; l<=n1; l++)  {
				nbasis=n[k1n1+l];
				if(nbasis==0.0)  {
					/* Skip this row */
					vp += 3*n2;
				}  else  {
					for(j=1; j<=n2; j++)  {
						/*vary m for fixed n */
						nmbas = m[kn2+j] * nbasis;
						if(nmbas!=0.0)  {
							t1 += *vp++ *nmbas;
							t2 += *vp++ *nmbas;
							t3 += *vp++ *nmbas;
						} else
							vp += 3;
					}
				}
			}

			/* return surface in integer array          */
			q[i3+1]=t1;
			q[i3+2]=t2;
			q[i3+3]=t3;

			/* increment polygon net array counter          */
			i3+=3;
		}
	}
}

/**********************************************************************************

                 Program Name:   cybk2.c

First:	Take the output of the b-spline surface
	generating program (a collection of data points column major
	in a one dimensional array), and arranges them into a polygon
	file suitable for display via the "threed" program and the 
	PS300 system.  The polygons are triangles in three-space.

                   Programmer:   Midn  Mark G. Daghir
                 Version/Date:   3rd // 2 February 1986
		       System:   UNIX
     		     Language:   C
		      Purpose:   To demonstrate the FUNCTION "cybk" and the
				     FUNCTION "normal".   Additionally, the functions
				     are timed to examine their effectiveness.
		       Method:   Sample data sets may be defined or read from
				     files and the results may be viewed after
				     applying the above functions.
	        Variable List:   i, ii, j, n........Loop Counters/Subscripted 
						    Variables Indices
				 dot1(4), dot2(4)...Dot product Coefficients
				 norm(6,4)..........Normal Vector i,j,k Coeff.
				 vertex(4,4)............Three Arbitrary Position
						    Vector i,j,k Coefficients
				 sd(4,4)............Three Side Vector i,j,k Coeff.
	          Subroutines:   normal(vertex,norm)....Given three arbitrary points
						    this function returns the 
						    five normals.
				 cybk(vertex,norm,ray1,ray2,intersect1,intersect2)..................
						    Given the above determined normals
						    and the arbitrary points defining a triangular volume, and
						    the endpoints of an incident line, this
						    function returns the points of intersection of the line
						    and the volume.
*/

cybk2(q,rp,norm,pointer,k1,p1, p2)
struct xray *rp;
int pointer[4];
double norm[6][4];
double *k1;
double q[];
{
	register int i, j;
	int ii, n, pp;
	int icount;
	double vertex[4][4];     		               /* Test Data */
	int tri;			/* number of the triangle */
	int index[11*11*2][4];		/* 2 triangles per cell */
	int ntri, npts;
      
	/* DEFINE NUMBER OF POINTS AND NUMBER OF TRIANGLES         */

	npts = p1 * p2;
	ntri = (p1-1) * (p2-1) * 2;

	/*	LIST POINTS IN COLUMN-MAJOR ORDER		      */
#ifdef never
	if(rt_g.debug&DEBUG_SPLINE) for( ii=1; ii <= npts; ii += 3 )  {
		fprintf(stdout," %f, %f, %f\n",
				q[ii], q[ii+1], q[ii+2]);
	}
#endif

	/*	DEFINE counter-clockwise TRIANGLES			      */
#define POINT(i,j)	((i-1)*p2+j)

        tri=0;
	for(i=1; i<=(p1-1); i++)  {
		for(j=1; j<=(p2-1); j++)  {
                        tri++;
                        index[tri][1] = POINT(i,j);
                        index[tri][2] = POINT(i+1,j+1);
                        index[tri][3] = POINT(i,j+1);

                        tri++;
                        index[tri][1] = POINT(i,j);
                        index[tri][2] = POINT(i+1,j);
                        index[tri][3] = POINT(i+1,j+1);
		}
	}

	/* Loop-Read the triangle order list. */
	for (i=1; i <= ntri; i++)  {
#ifdef never
		if( rt_g.debug&DEBUG_SPLINE)  {
			fprintf(stdout, "		Triangle %d Pointers\n", i);
			fprintf(stdout, "	%d,  %d,  %d\n", index[i][1], index[i][2], index[i][3]);
		}
#endif

		for (ii=1; ii <= 3; ii++)  {
			j = (index[i][ii]-1)*3;
			if( j < 0 || j >= ntri*3 )  {
				printf("index[%d][%d]=%d!\n", i, ii, j);
				break;
			}
			vertex[ii][1] = q[j+1];
			vertex[ii][2] = q[j+2];
			vertex[ii][3] = q[j+3];
#ifdef never
			if(rt_g.debug&DEBUG_SPLINE) fprintf(stdout, "	vertex[%d] = (%f, %f, %f)\n",
				ii, vertex[ii][1], vertex[ii][2], vertex[ii][3]);
#endif
		}
		/* Create the inward and surface normals. */
  		normal(vertex,norm);

		/* Do the actual intersection with the triangle */
		if( cybk(vertex, norm, rp, k1) == 1 )  {
			for (j=1; j <= 3; j++) 	{
				pointer[j] = index[i][j];
			}
			return(1);	/* HIT */
		}
      	}
	return(0);			/* MISS */
}
/**** cybk.c ****/

	/*******************************************************************

                 Program Name:   cybk.c


                   Programmer:   Midn  Mark G. Daghir
                 Version/Date:   3nd // 2 February 1986
		       System:   UNIX
     		     Language:   C
		      Purpose:   To demonstrate the FUNCTION "cybk" and the
				     FUNCTION "normal".   Additionally, the functions
				     are timed to examine their effectiveness.
		       Method:   Sample data sets may be defined or read from
				     files and the results may be viewed after
				     applying the above functions.
	        Variable List:   i, ii, j, n........Loop Counters/Subscripted 
						    Variables Indices
				 dot1(4), dot2(4)...Dot product Coefficients
				 norm(6,4)..........Normal Vector i,j,k Coeff.
				 vertex(4,4)............Three Arbitrary Position
						    Vector i,j,k Coefficients
				 sd(4,4)............Three Side Vector i,j,k Coeff.
	          Subroutines:   normal(vertex,norm)....Given three arbitrary points
						    this function returns the 
						    five normals.
				 cybk(vertex,norm,ray1,ray2)..................
						    Given the above determined normals
						    and the arbitrary points defining a triangular volume, and
						    the endpoints of an incident line, this
						    function returns the points of intersection of the line
						    and the volume.
	        Common Blocks:   NONE
         Input/Output Devices:   Ergo terminals

**************************************************************************/


/*

			FUNCTION   "cybk"


	      Purpose:   To find the intersection point of a line
			     and a triangular plane defined by three
			     arbitrary points.

     	       Method:   This function impliments the Cyrus & Beck
			     cplipping algorithm found in the following reference:
			     Rogers, David F., Procedural Elements For Computer Graphics,
			         McGraw-Hill Book Company, New York, NY, 1985, pp. 135-159.

	    Variables:   vertex[1,2,3][x,y,z]........Three arbitrary points that
					   define the triangular plane, (input).

		 	 norm[1,2,3,4,5][x,y,z]..The five vectors normal to the
					   sides of the triangular volume, (input).

			 ray1[x,y,z]; ray2[x,y,z]....The two endpoints of incident
					   line intersecting the triangular volume, (input).

			 ddotn; wdotn..............The two dot product quantities, D*n & w*n, (internal).

			 w[1,2,3,4,5][x,y,z].....The vector difference between ray1[] and vertex[i][], (internal).

			 t; tl; tu...............Parameter values for parametric line equation, (internal).

			 i; ii; j................Counters and subscripts, (internal).

 *
 *  Returns -
 *	0 if missed,
 *	1 if hit
 */
int
cybk(vertex, norm, rp, k1)
struct xray *rp;
double vertex[4][4], norm[6][4];
double *k1;
{
	register int i, ii, j;

	static double t, tl, tu, ddotn, wdotn, w[6][4];

	/* Find intersection within bounding box limits */
	tl = rp->r_min;		/* could be 0 */
	tu = rp->r_max;		/* could be INFINITY */
#ifdef never
	if( rt_g.debug&DEBUG_SPLINE )  printf("cybk: %f<=t<=%f\n", tl, tu);
#endif
	
	for(i=1; i<=5; i++)  {
		/* NOTE:  Normals point INWARDS */
		ii = ((i-1) % 3) + 1;
		ddotn = wdotn = 0;
		for(j=1; j<=3; j++)  {
			w[i][j] = rp->r_pt[j-1] - vertex[ii][j];
			ddotn += rp->r_dir[j-1] * norm[i][j];
			wdotn += w[i][j] * norm[i][j];
		}
#ifdef never
		if( rt_g.debug&DEBUG_SPLINE )  {
		   	fprintf(stdout, "Normal Used = #%d", i);
			fprintf(stdout, "	<%f, %f, %f>\n", norm[i][1], norm[i][2], norm[i][3]);
		   	fprintf(stdout, "	ddotn = %f", ddotn);
		   	fprintf(stdout, "	wdotn = %f\n", wdotn);
		}
#endif

		if( NEAR_ZERO(ddotn, 0.005) )  {
			if(wdotn < 0)  {
				/* Line parallel, outside halfspace */
				return(0);	/* MISS */
			}
			continue;
		}
	
		t = -wdotn / ddotn;
#ifdef never
		if( rt_g.debug&DEBUG_SPLINE) fprintf(stdout, " t = %f\n", t);
#endif
		if(ddotn > 0)  {
			/* Ray is headed INTO halfspace */
   			if(t > tl)
   				tl = t;
   		} else {
   			/* Ray is headed OUT of halfspace */
			if(t < 0)
				return(0);  /* MISS -- pt outside halfspace */
			if(t < tu)
				tu = t;
   		}
	}
	/* Check for valid intersection */
	if( tl > rp->r_max )
		return(0);		/* MISS */
	if( rt_fdiff(tl,tu) > 0 ) 
		return(0);		/* MISS */

	*k1 = tl;			/* take entry distance */
	if( rt_g.debug&DEBUG_SPLINE )  printf("hit, dist=%f\n", tl);
	return(1);			/* HIT */
}
/*** normal.c ****/
/*******************************************************************

                 Program Name:   normal.c


                   Programmer:   Midn  Mark G. Daghir
                 Version/Date:   3rd/ 2 February 1986
		       System:   UNIX
     		     Language:   C
		      Purpose:   To find three vectors normal to
				     the three side vectors connecting
                                     three arbitrary points of a triangle
				     lying in the plane of the triangle, and
				     to find the two vectors normal to the 
				     plane of the triangle.
		       Method:   The method used here is to take the 
				     triple cross product of adjoining 
				     side vectors...
					    n(i) = s(i) x ( s(i+1) x s(i) )
				     This simplifies as follows...
				     n(i) = (s(i)*s(i))s(i+1)-(s(i)*s(i+1))s(i)

	        Variable List:   i, ii, j, n........Loop Counters/Subscripted 
						    Variables Indices
				 dot1(3), dot2(3)...Dot product Coefficients
				 norm(5,3)..........Normal Vector i,j,k Coeff.
				 pt(3,3)............Three Arbitrary Position
						    Vector i,j,k Coefficients
				 sd(3,3)............Three Side Vector i,j,k Coeff.
	          Subroutines:   normal(pt,norm)....Given three arbitrary points
						    this function returns the 
						    five normals.
*/
normal(pt,norm)
double pt[4][4], norm[6][4];            /* Declare Passed Variables */
{
	int i, ii, j;			       /* Declare Integers */
	double sd[4][4], dot1[4], dot2[4];      /* Declare Reals */

	/* Loop to create side vectors */
	for(i=1; i<=3; i++)  {
		ii = i + 1;
		if(ii > 3)
		ii = 1;
		for(j=1; j<=3; j++)  {
			sd[i][j] = pt[ii][j] - pt[i][j];
		}
	}

	/* Loop to create dot1 Coefficients */
	for(i=1; i<=3; i++)  {
		dot1[i] = 0;

		for(j=1; j<=3; j++)  {
			dot1[i] += sd[i][j] * sd[i][j];
		}
	}

	/* Loop to create dot2 Coefficients */
	for(i=1; i<=3; i++)  {
		dot2[i] = 0;
		ii = i + 1;
		if(ii > 3)
		ii = 1;
		for(j=1; j<=3; j++)  {
			dot2[i] += sd[i][j] * sd[ii][j];
		}
	}

	/* Loop to create side normals */
	for(i=1; i<=3; i++)  {
		ii = i + 1;
		if(ii > 3)
		ii = 1;
		for(j=1; j<=3; j++)  {
			norm[i][j] = dot1[i] * sd[ii][j] - dot2[i] * sd[i][j];
		}
	}

	/* Creation of surface normals */
	norm[4][1] = sd[1][2] * sd[2][3] - sd[2][2] * sd[1][3];
	norm[4][2] = - sd[1][1] * sd[2][3] + sd[2][1] * sd[1][3];
	norm[4][3] = sd[1][1] * sd[2][2] - sd[2][1] * sd[1][2];
	norm[5][1] = - norm[4][1];
	norm[5][2] = - norm[4][2];
	norm[5][3] = - norm[4][3];

	for(i=1; i <= 5; i++)  {
		for(j=1; j <= 3; j++)  {
			if( NEAR_ZERO( norm[i][j], 0.005 ) )
				norm[i][j] = 0;
		}
	}

#ifdef never
	if( rt_g.debug&DEBUG_SPLINE) for(i=1; i<=5; i++)  {
		fprintf(stdout, "	norm[%d][1] = %f, norm[%d][2] = %f, norm[%d][3] = %f\n", 
			i, norm[i][1], i, norm[i][2], i, norm[i][3]);
	}
#endif
}

/************************************************************************

ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
c
c name: basis
c
c purpose: to calculate a floating point bspline basis function
c
c method: cox-deboor
c
c parameters:
c
c	np=      number of polygon vertices
c
c	c=       order of b-spline basis
c
c	nplusc=  np+c
c
c	t=       parameter value on curve
c
c	x()=     knot vector
c
c	n()=     the weighting function (cf. eq. 5-78)
c
c	temp()=  an intermediate array containing basis functions
c	
ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc

************************************************************************/

basis(c,t,np,nplusc,x,tcount,n)
double n[],t;
int x[];
int c,np,nplusc,*tcount;
{
	double temp[100],d,e;
	int i,k;

#ifdef never
	if( rt_g.debug & DEBUG_SPLINE )
		fprintf(stderr, "	In Basis.c, t=%f\n", t);
#endif

	for(i=1;i<=nplusc-1;i++)  {
		/* !calc. n(i,1).  if t not in interval =0. otherwise  */
		temp[i]=0;

		if(t>=x[i-1] && t<x[i])
			temp[i]=1;
#ifdef never
		if(rt_g.debug&DEBUG_SPLINE) fprintf(stderr, "%d %d %d\n", i, x[i-1], temp[i] );
#endif
	}

	for(k=2; k<=c; k++)  {
		for(i=1;i<=nplusc-k;i++) {
			/* !if n(i,k)=0. first term r&a eq. 5-78 =0. otherwise  */
			d=0;
			if(temp[i]!=0)
				d=((t-x[i-1])*temp[i])/(x[i+k-2]-x[i-1]);

			/* !if n(i+1,k)=0. second term r&a eq. 5-78 =0. otherwise  */
			e=0;
			if(temp[i+1]!=0)
				e=((x[i+k-1]-t)*temp[i+1])/(x[i+k-1]-x[i]);
			temp[i]=d+e;
		}
	}

	/* !put in n vector sequentially for each t     */
	for(i=1;i<=np;i++)  {
		/* !beginning at t=0.  np+c-1 elements for each t   */
		*tcount+=1;
		n[*tcount]=temp[i];

#ifdef never
		if( rt_g.debug&DEBUG_SPLINE)
			fprintf(stderr, "	n[%d] = %f\n", *tcount, n[*tcount]);
#endif
	}

	/* !pick up last point      */
	if(t==x[nplusc-1])
	n[*tcount]=1;
}
