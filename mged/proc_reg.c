/*
 *				P R O C _ R E G . C
 *
 * This module deals with building an edge description of a region.
 *
 * Functions -
 *	proc_reg	processes each member (solid) of a region
 *
 * Authors -
 *	Keith Applin
 *	Gary Kuehl
 *	Michael John Muuss
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include	<math.h>
#include "./machine.h"	/* special copy */
#include "../h/vmath.h"
#include "../h/db.h"
#include "ged.h"
#include "dm.h"

HIDDEN void	cpoint(), dwreg(), ell_arbn(), tgc_arbn(), tplane();
HIDDEN int	arbn_shot(), cgarbs(), gap(), region();


/* following variables are used in processing regions
 *  for producing edge representations
 */
#define NMEMB	100	/* max number of members in a region */
static int	nmemb = 0;		/* actual number of members */
static int	m_type[NMEMB];		/* member solid types */
static char	m_op[NMEMB];		/* member operations */
static float	m_param[NMEMB*24];	/* member parameters */
static int	memb_count = 0;		/* running count of members */
static int	param_count = 0; 	/* location in m_param[] array */

static float	reg_min[3], reg_max[3];		/* min,max's for the region */
static float	pinf = 1000000.0;
static float	*sp;			/* pointers to the solid parameter array m_param[] */

#define NPLANES	500
static float	peq[NPLANES*4];		/* plane equations for EACH region */
static float	solrpp[NMEMB*6];	/* enclosing RPPs for each member of the region */
static float	regi[NMEMB], rego[NMEMB];	/* distances along ray where ray enters and leaves the region */
static float	pcenter[3];		/* center (interior) point of a solid */
static float	r_pt[3];			/* starting point of ray */
static float	r_dir[3];			/* direction cosines of ray */
static float	rin, rout;		/* location where ray enters and leaves a solid */
static float	ri, ro;			/* location where ray enters and leaves a region */
static float	tol;			/* tolerance */
static int	mlc[NMEMB];		/* location of last plane for each member */
static int	la, lb, lc, id, jd, ngaps;
static int	negpos;
static char	oper;


/*
 *			P R O C _ R E G
 *
 * This routine processes each member(solid) of a region.
 * When the last member is processed, dwreg() is executed.
 *
 * Returns -
 *	-1	error
 *	 0	region drawn
 *	 1	more solids follow, please re-invoke w/next solid.
 */
proc_reg( recordp, xform, flag, more )
register union record *recordp;
register mat_t xform;
int flag, more;
{
	register int i;
	register float *op;	/* Used for scanning vectors */
	static vect_t	work;		/* Vector addition work area */
	static int length, type;
	static int cgtype;
	static int j;
	static float	sol_min[3], sol_max[3];
	static float a,b,c,d,v1,v2,vt,vb;
	static union record input;	/* Holds an object file record */

	input = *recordp;		/* struct copy */

	type = recordp->s.s_type;
	cgtype = type;

	if(flag == ROOT)
		m_op[memb_count] = '+';
	else
	if(flag == 999)
		m_op[memb_count] = 'u';
	else
		m_op[memb_count] = '-';

	m_type[memb_count] = cgtype;
	if(memb_count >= NMEMB) {
		(void)printf("proc_reg: region has more than %d members\n", NMEMB);
		nmemb = param_count = memb_count = 0;
		return(-1);	/* ERROR */
	}
	if( memb_count == 0 )  {
		/* First part of this region, initialize */
		reg_min[0]=reg_min[1]=reg_min[2] = -pinf;
		reg_max[0]=reg_max[1]=reg_max[2]=pinf;
		sp = &m_param[0];	/* XXX */
	}

	/* Find enclosing RPP for the solid */
	sol_min[0]=sol_min[1]=sol_min[2]=pinf;
	sol_max[0]=sol_max[1]=sol_max[2] = -pinf;

	switch( cgtype )  {

	case GENARB8:
		MAT4X3PNT( work, xform, &input.s.s_values[0] );
		VMOVE( &input.s.s_values[0], work );
		VMOVE(&m_param[param_count], &input.s.s_values[0]);
		param_count += 3;
		op = &input.s.s_values[1*3];
		for( i=1; i<8; i++ )  {
			MAT4X3VEC( work, xform, op );
			VADD2(&m_param[param_count], input.s.s_values, work);
			param_count += 3;
			op += 3;
		}
		for(i=0;i<8;i++){
			for(j=0;j<3;j++){
				MIN(sol_min[j],*sp);
				MAX(sol_max[j],*sp);
				sp++;
			}
		}
		break;

	case GENTGC:
		op = &recordp->s.s_values[0*3];
		MAT4X3PNT( work, xform, op );
		VMOVE(&m_param[param_count], work);
		param_count += 3;
		op += 3;

		for( i=1; i<6; i++ )  {
			MAT4X3VEC( work, xform, op );
			VMOVE(&m_param[param_count], work);
			param_count += 3;
			op += 3;
		}
		for(i=0;i<3;i++,sp++){
			vt = *sp + *(sp+3);
			a = *(sp+6);
			b = *(sp+9);
			c = *(sp+12);
			d = *(sp+15);
			v1=sqrt(a*a+b*b);
			v2=sqrt(c*c+d*d);
			MIN(sol_min[i],*(sp)-v1);
			MIN(sol_min[i],vt-v2);
			MAX(sol_max[i],*(sp)+v1);
			MAX(sol_max[i],vt+v2);			
		}
		sp+=15;
		break;

	case GENELL:
		op = &recordp->s.s_values[0*3];
		MAT4X3PNT( work, xform, op );
		VMOVE( op, work );
		VMOVE(&m_param[param_count], op);
		param_count += 3;
		op += 3;

		for( i=1; i<4; i++ )  {
			MAT4X3VEC( work, xform, op );
			VMOVE(&m_param[param_count], work);
			param_count += 3;
			op += 3;
		}
		for(i=0;i<3;i++,sp++){
			vb = *sp - *(sp+3);
			vt = *sp + *(sp+3);
			a = *(sp+6);
			b = *(sp+9);
			v1=sqrt(a*a+b*b);
			v2=sqrt(c*c+d*d);
			MIN(sol_min[i],vb-v1);
			MIN(sol_min[i],vt-v2);
			MAX(sol_max[i],vb+v1);
			MAX(sol_max[i],vt+v2);
		}
		sp+=9;
		break;

	default:
		(void)printf("proc_reg:  Cannot draw solid type %d (%s)\n",
			type, type == TOR ? "TOR":
			 type == ARS ? "ARS" : "UNKNOWN TYPE" );
		nmemb = param_count = memb_count = 0;
		return(-1);	/* ERROR */
	}

	/* save min,maxs for this solid */
	for(i=0; i<3; i++) {
		solrpp[memb_count*6+i] = sol_min[i];
		solrpp[memb_count*6+i+3] = sol_max[i];
	}

	if(m_op[memb_count] != '-') {
		for(i=0;i<3;i++){
			MAX(reg_min[i],sol_min[i]);
			MIN(reg_max[i],sol_max[i]);
		}
	}
	memb_count++;

	if(more)
		return(1);		/* MORE solids follow */

	/* this was the last member solid - draw the region */
	nmemb = memb_count;
	param_count = memb_count = 0;
	if(nmemb == 0) {
		nmemb = param_count = memb_count = 0;
		return(-1);	/* ERROR */
	}
	dwreg();
	return(0);	/* OK, region was drawn */
}


/*
 *      		D W R E G
 *
 *	Draws an "edge approximation" of a region
 *
 *	All solids are converted to planar approximations.
 *	Logical operations are then applied to the solids to
 *	produce the edge representation of the region.
 *
 * 	Gary Kuehl 	2Feb83
 *
 * 	Routines used by dwreg():
 *	  1. gap()   	puts gaps in the edges
 *	  2. region()	finds intersection of ray with a region
 *	  4. cpoint()	finds point of intersection of three planes
 *	  6. tplane()	tests if plane is inside enclosing rpp
 *	  7. arbn_shot()	finds intersection of ray with an arb
 *	  8. tgc_arbn()	converts tgc to arbn
 *	  9. ell_arbn()	converts ellg to arbn
 */
HIDDEN void
dwreg()
{
	register int i,j;
	static int k,l;
	static int n,m;
	static int itemp;
	static float c1[3*4]={
		1.,0.,0.,0.,
		0.,1.,0.,0.,
		0.,0.,1.,0.};
	static int lmemb, umemb;/* lower and upper limit of members of region
				 * from one OR to the next OR */
	static float *savesp;
	static float tt;
	static int ls;
	static float *pp;

	lmemb = 0;
	savesp = &m_param[0];

orregion:	/* sent here if region has or's */
	for(umemb = lmemb+1; (umemb < nmemb && m_op[umemb] != 'u'); umemb++)
		;

	lc = 0;
	tol = -pinf;
	sp = savesp;

	for(i=0;i<3;i++){
		MAX(tol,fabs(reg_min[i]));
		MAX(tol,fabs(reg_max[i]));
	}
	tol=tol*0.00001;

	/* convert all solids to ARBNs */
	sp = savesp;
	lc=0;
	tt=tol*10.;
	for(id=lmemb;id<umemb;id++){
		ls=lc;
		switch( m_type[id] )  {
		case GENARB8:
			for(i=0;i<3;i++){
				register float ppc;
				register int k;
				k=i;
				ppc=0.0;
				for(j=0; j<8; j++) {
					ppc += *(sp+k);
					k+=3;
				}
				pcenter[i]=ppc/8.;
			}
#define P(x)	(sp+3*x)
			tplane( P(0),P(1),P(2),P(3) );
			tplane( P(4),P(5),P(6),P(7) );
			tplane( P(0),P(4),P(7),P(3) );
			tplane( P(1),P(2),P(6),P(5) );
			tplane( P(0),P(1),P(5),P(4) );
			tplane( P(3),P(2),P(6),P(7) );
#undef P
			sp += 8*3;
			break;
		case GENTGC:
			tgc_arbn();
			sp+=18;
			break;
		case GENELL:
			ell_arbn();
			sp+=12;
			break;
		}

		if(m_op[id]=='-'){
			pp = &peq[4*ls]+3;
			while(ls++ < lc) {
				*pp-=tt;
				pp+=4;
			}
		}

		mlc[id]=lc-1;
	}

	/* save the parameter pointer in case region has ORs */
	savesp = sp;

	/* Make 3 basic plane eqns of min bounds */
	for(i=0; i<3; i++)
		c1[(i*4)+3]=reg_min[i];
	l=0;

	/* process all members until the next OR */
	for(id=lmemb; id<umemb; id++) {
		if(mlc[id] < l)
			goto skipid;

		/* plane i is associated with member id */
		for(i=l; (i<(lc-1) && i<=mlc[id]); i++){
			m = i + 1;
			itemp = id;
			if(i == mlc[id])
				itemp = id + 1;
			for(jd=itemp; jd<umemb; jd++) {

				negpos = 0;
				if( (m_op[id] == '-' && m_op[jd] != '-') ||
				    (m_op[id] != '-' && m_op[jd] == '-') )
					negpos = 1;

				/* plane j is associated with member jd */
				for(j=m; j<=mlc[jd]; j++){
					if(id!=jd && m_op[id]=='-' && m_op[jd]=='-') { 
						for(k=0; k<3; k++) {
							if(solrpp[6*id+k] > solrpp[6*jd+k+3] || 
							   solrpp[6*id+k+3] < solrpp[6*jd+k] )
								goto sksolid;
						}

						for(k=mlc[jd-1]+1; k<=mlc[jd]; k++) {
							if(fabs(peq[i*4+3] - peq[k*4+3]) < tol && 
							    VDOT(&peq[i*4], &peq[k*4]) > .9999) {
								for(k=l; k<=mlc[id]; k++) {
									if(fabs(peq[j*4+3] - peq[k*4+3]) < tol && 
									    VDOT(&peq[j*4], &peq[k*4]) > .9999)
										goto noskip;
								}
								goto sksolid;
							}
						}
						for(k=l; k<= mlc[id]; k++) {
							if(fabs(peq[j*4+3] - peq[k*4+3]) < tol && 
							    VDOT(&peq[j*4], &peq[k*4]) > .9999)
								goto skplane;
						}
					}
noskip:
					if(fabs(VDOT(&peq[i*4],&peq[j*4]))>=.9999)
						continue; /* planes parallel */

					/* planes not parallel */
					/* compute direction vector for ray
					 * perpendicular to both plane normals */
					VCROSS(r_dir,&peq[i*4],&peq[j*4]);
					VUNITIZE( r_dir );

					k=0;
					if(fabs(r_dir[1]) > fabs(r_dir[0])) k=1;
					if(fabs(r_dir[2]) > fabs(r_dir[k])) k=2;
					if(r_dir[k] < 0.0)  {
						VREVERSE( r_dir, r_dir );
					}

					/* starting point for this ray:
					 * intersection of two planes (line),
					 * and closest min RPP bound plane.
					 */
					cpoint(r_pt,&c1[k*4],&peq[i*4],&peq[j*4]);

					/* check if ray intersects region */
					if( (k=region(lmemb,umemb))<=0)
						continue;

					/* ray intersects region */
					/* plot this ray  including gaps */
					for(n=0; n<k; n++){
						static float pi[3],po[3];
						VJOIN1( pi, r_pt, regi[n], r_dir );
						VJOIN1( po, r_pt, rego[n], r_dir );
						DM_GOTO( pi, PEN_UP);
						DM_GOTO( po, PEN_DOWN);
					}
skplane:				 ;
				}
sksolid:
				m = mlc[jd] + 1;
			}
		}
skipid:
		l=mlc[id]+1;
	}


	lmemb = umemb;
	if(umemb < nmemb)
		goto orregion;

	nmemb = 0;
}


/* put gaps into region line */
HIDDEN int
gap(si,so)
float si,so;
{
	register int igap,lgap,i;

	if(si<=ri+tol) goto front;
	if(so>=ro-tol) goto back;
	if(ngaps>0){
		for(igap=0;igap<ngaps;igap++) {
			/* locate position of gap along ray */
			if(si<=(regi[igap+1]+tol)) 
				goto insert;
		}
	}
	if((++ngaps)==25) return(-1);

	/* last gap along ray */
	rego[ngaps-1]=si;
	regi[ngaps]=so;
	return(1);

insert:		/* insert gap in ray between existing gaps */
	if(so < (rego[igap]-tol)) goto novlp;

	/* have overlapping gaps - sort out */
	MIN(rego[igap],si);
	if(regi[igap+1]>=so) return(1);
	regi[igap+1]=so;
	for(lgap=igap;(lgap<ngaps&&so<(rego[lgap]-tol));lgap++)
		;
	if(lgap==igap) return(1);
	if(so>regi[lgap+1]) {
		lgap++;
		igap++;
	}
	while(lgap<ngaps){
		rego[igap]=rego[lgap];
		regi[igap+1]=regi[lgap+1];
		lgap++;
		igap++;
	}
	ngaps=ngaps-lgap+igap;
	return(1);

novlp:		/* no overlapping gaps */
	if((++ngaps)>=25) return(-1);
	for(lgap=igap+1;lgap<ngaps;lgap++){
		rego[lgap]=rego[lgap-1];
		regi[lgap+1]=regi[lgap];
	}
	rego[igap]=si;
	regi[igap+1]=so;
	return(1);

front:		/* gap starts before beginning of ray */
	if((so+tol)>ro)
		return(0);
	MAX(ri,so);
	if(ngaps<1) return(1);
	for(igap=0; ((igap<ngaps) && ((ri+tol)>rego[igap])); igap++)
		;
	if(igap<1) return(1);
	MAX(ri, regi[igap]);
	lgap=ngaps;
	ngaps=0;
	if(igap>=lgap) return(1);
	for(i=igap;i<lgap;i++){
		rego[ngaps++]=rego[i];
		regi[ngaps]=regi[i+1];
	}
	return(1);

back:		/* gap ends after end of ray */
	MIN(ro,si);
	if(ngaps<1) return(1);
	for(igap=ngaps; (igap>0 && (ro<(regi[igap]+tol))); igap--)
		;
	if(igap < ngaps) {
		MIN(ro, rego[igap]);
	}
	ngaps=igap;
	return(1);
}


/* computes intersection of ray with region 
 *   	returns ngaps+1	if intersection
 *	        0	if no intersection
 */
HIDDEN int
region(lmemb,umemb)
{
	register int	i, kd;
	static double s1[3],s2[3];
	static double a1, a2;

	ngaps=0;
	ri = -1.0 * pinf;
	ro=pinf;

	/* does ray intersect region rpp */
	VSUB2( s1, reg_min, r_pt );
	VSUB2( s2, reg_max, r_pt );

	/* find start & end point of ray with enclosing rpp */
	for(i=0;i<3;i++){
		if(fabs(r_dir[i])>.001){
			a1=s1[i]/r_dir[i];
			a2=s2[i]/r_dir[i];
			if(r_dir[i] <= 0.){
				if(a1<tol) return(0);
				MAX(ri,a2);
				MIN(ro,a1);
			} else {
				if(a2<tol) return(0);
				MAX(ri,a1);
				MIN(ro,a2);
			}
			if((ri+tol)>=ro) return(0);
		} else {
			if(s1[i]>tol || s2[i]<(-1.0*tol)) 
				return(0);
		}
	}

	/* ray intersects region - find intersection with each arbn solid */
	la=0;
	for(kd=lmemb;kd<umemb;kd++){
		oper=m_op[kd];
		lb=mlc[kd];

		/* if la > lb then no planes to check for this member */
		if(la > lb)
			continue;

		if(kd==id || kd==jd) oper='+';
		if(oper!='-'){
			/* positive solid  recalculate end points of ray */
			if( arbn_shot() == 0 )
				return(0);
			if(ngaps==0){
				MAX(ri,rin);
				MIN(ro,rout);
			} else{
				if(gap(-pinf, rin) <= 0)
					return(0);
				if(gap(rout, pinf) <= 0)
					return(0);
			}
		}
		if(oper == '-') {
			/* negative solid  look for gaps in ray */
			if(arbn_shot() != 0) {
				if(gap(rin, rout) <= 0)
					return(0);
			}
		}
		if(ri+tol>=ro) return(0);
		la=lb+1;
	}

	/*end of region - set number of intersections*/
	regi[0]=ri;
	rego[ngaps]=ro;
	return(ngaps+1);
}

/*
 *			A R B N _ S H O T
 *
 *  finds intersection of ray with an arbitrary convex polyhedron
 *  defined by enclosing half-spaces.
 */
HIDDEN int
arbn_shot()
{
	register float *pp;
	static float *pend;
	static double dxbdn,wbdn,te;

	rin = ri;
	rout = ro;

	te = tol;
	if(oper == '-' && negpos)
		te = -tol;

	pend = &peq[lb*4];
	for(pp = &peq[la*4]; pp <= pend; pp+=4){
		dxbdn = pp[3]-VDOT(r_pt,pp);
		wbdn=VDOT(r_dir,pp);
		if( wbdn < -0.001 || wbdn > 0.001 )  {
			static double s;

			s=dxbdn/wbdn;
			if(wbdn > 0.0) {
				MAX(rin, s);
			} else {
				MIN(rout,s);
			}
		} else {
			if(dxbdn>te) return(0);
		}
		if((rin+tol)>=rout || rout<=tol) return(0);
	}
	MAX(rin,0);	/* ray may start inside */
	return(1);
}

/*
 *			C P O I N T
 *
 * computes point of intersection of three planes, answer in "point".
 */
HIDDEN void
cpoint(point,c1,c2,c3)
float *point;
register float *c1, *c2, *c3;
{
	static double v1[4], v2[4], v3[4];
	register int i;
	static double d;

	VCROSS(v1,c2,c3);
	if((d=VDOT(c1,v1))==0)  {
		printf("cpoint failure\n");
		return;
	}
	d = 1.0 / d;
	VCROSS(v2,c1,c3);
	VCROSS(v3,c1,c2);
	for(i=0; i<3; i++)
		point[i]=d*(c1[3]*v1[i]-c2[3]*v2[i]+c3[3]*v3[i]);
}

static int arb_npts;
/*
 *			T P L A N E
 *
 *  Register a plane which contains the 4 specified points,
 *  unless they are degenerate, or lie outside the region RPP.
 */
HIDDEN void
tplane(p,q,r,s)
float *p, *q, *r, *s;
{
	register float *pp,*pf;
	register int i;

	/* If all 4 pts have coord outside region RPP,
	 * discard this plane, as the polygon is definitely outside */
	for(i=0;i<3;i++){
		FAST float t;
		t=reg_min[i]-tol;
		if(*(p+i)<t && *(q+i)<t && *(r+i)<t && *(s+i)<t)
			return;
		t=reg_max[i]+tol;
		if(*(p+i)>t && *(q+i)>t && *(r+i)>t  && *(s+i)>t)
			return;
	}

	/* Do these three points form a valid plane? */
	/* WARNING!!  Fourth point is never even looked at!! */
	pp = &peq[lc*4];
	arb_npts = 0;
	planept( pp, p );
	planept( pp, q );
	planept( pp, r );
	planept( pp, s );
	if( arb_npts < 3 )  return;	/* invalid as plane */

	/* See if this plane already exists in list */
	for(pf = &peq[0];pf<pp;pf+=4) {
		 if(VDOT(pp,pf)>0.9999 && fabs(*(pp+3)-*(pf+3))<tol) 
			return;
	}

	/* increment plane counter */
	if(lc >= NPLANES) {
		(void)printf("tplane: More than %d planes for a region - ABORTING\n", NPLANES);
		return;
	}
	lc++;		/* Save plane eqn */
}

#define NEAR_ZERO(val,epsilon)	( ((val) > -epsilon) && ((val) < epsilon) )
planept( eqn, point )
float *eqn, *point;
{
	register int i;
	static float arb_points[4*3];
	static float arb_Xbasis[3];
	static float P_A[3];
	static double f;
	static float work[3];
#define arb_A	arb_points		/* first saved point is A */
#define arb_N	eqn			/* first 3 # of plane eqn */

	/* Verify that this point is not the same as an earlier point */
	for( i=0; i < arb_npts; i++ )  {
		VSUB2( work, point, &arb_points[i*3] );
		if( MAGSQ( work ) < 0.005 )
			return(0);			/* BAD */
	}
	i = arb_npts++;		/* Current point number */
	VMOVE( &arb_points[i*3], point );

	/* The first 3 points are treated differently */
	switch( i )  {
	case 0:
		return(1);				/* OK */
	case 1:
		VSUB2( arb_Xbasis, point, arb_A );	/* B-A */
		return(1);				/* OK */
	case 2:
		VSUB2( P_A, point, arb_A );	/* C-A */
		/* Check for co-linear, ie, (B-A)x(C-A) == 0 */
		VCROSS( arb_N, arb_Xbasis, P_A );
		f = MAGNITUDE( arb_N );
		if( NEAR_ZERO(f,0.005) )  {
			arb_npts--;
			return(0);			/* BAD */
		}
		VUNITIZE( arb_N );

		/*
		 *  If C-A is clockwise from B-A, then the normal
		 *  points inwards, so we need to fix it here.
		 *  WARNING:  Here, INWARD normals is correct, for now.
		 */
		VSUB2( work, arb_A, pcenter );
		f = VDOT( work, arb_N );
		if( f > 0.0 )  {
			VREVERSE(arb_N, arb_N);	/* "fix" normal */
		}
		eqn[3] = VDOT( arb_N, arb_A );
		return(1);				/* OK */
	default:
		/* Merely validate 4th and subsequent points */
		VSUB2( P_A, point, arb_A );
		VUNITIZE( P_A );		/* Checking direction only */
		f = VDOT( arb_N, P_A );
		if( ! NEAR_ZERO(f,0.005) )  {
			/* Non-planar face */
			printf("arb: face non-planar, dot=%f\n", f );
			arb_npts--;
			return(0);				/* BAD */
		}
		return(1);			/* OK */
	}
}

/* convert tgc to an arbn */
HIDDEN void
tgc_arbn()
{
	static float vt[3], p[3], q[3], r[3], t[3];
	static float s,sa,c,ca,sd=.38268343,cd=.92387953;
	register int i,j,lk;

	lk = lc;

	for(i=0;i<3;i++){
		vt[i] = *(sp+i) + *(sp+i+3);
		pcenter[i]=( *(sp+i) + vt[i])*.5;
		p[i] = *(sp+i) + *(sp+i+6);
		t[i] = vt[i] + *(sp+i+12);
	}
	s=0.;
	c=1.;
	for(i=0;i<16;i++){
		sa=s*cd+c*sd;
		ca=c*cd-s*sd;
		for(j=0;j<3;j++){
			q[j] = *(sp+j) + ca * *(sp+j+6) + sa * *(sp+j+9);
			r[j]=vt[j]+ ca * *(sp+j+12) + sa* *(sp+j+15);
		}
		tplane( p, q, r, t );
		s=sa;
		c=ca;
		for(j=0; j<3; j++) {
			p[j] = q[j];
			t[j] = r[j];
		}
	}
	if(lc == lk)	return;

	/* top plane */
	for(i=0; i<3; i++) {
		p[i]=vt[i] + *(sp+i+12) + *(sp+i+15);
		q[i]=vt[i] + *(sp+i+15) - *(sp+i+12);
		r[i]=vt[i] - *(sp+i+12) - *(sp+i+15);
		t[i]=vt[i] + *(sp+i+12) - *(sp+i+15);
	}
	tplane( p, q, r, t );

	/* bottom plane */
	for(i=0;i<3;i++){
		p[i] = *(sp+i) + *(sp+i+6) + *(sp+i+9);
		q[i] = *(sp+i) + *(sp+i+9) - *(sp+i+6);
		r[i] = *(sp+i) - *(sp+i+6) - *(sp+i+9);
		t[i] = *(sp+i) + *(sp+i+6) - *(sp+i+9);
	}
	tplane( p, q, r, t );
}


/* convert ellg to an arbn */
HIDDEN void
ell_arbn()
{
	static float p[3], q[3], r[3], t[3];
	static float s1,s2,sa,c1,c2,ca,sd=.5,cd=.8660254,sgn;
	static float se, ce;
	register int i,j,ih,ie;

	sgn = -1.;
	for(i=0;i<3;i++) pcenter[i] = *(sp+i);
	for(ih=0;ih<2;ih++){
		c2=1.;
		s2=0.;

		/* find first point */
		for(ie=0;ie<3;ie++){
			s1=0.;
			c1=1.;
			se=s2*cd+c2*sd;
			ce=c2*cd-s2*sd;
			for(i=0;i<3;i++) {
				p[i] = *(sp+i) + (c2 * (*(sp+i+3)))
				       + (sgn * s2 * (*(sp+i+9)));
				t[i] = *(sp+i) + (ce * (*(sp+i+3)))
				       + (sgn * se * (*(sp+i+9)));
			}
			for(i=0;i<12;i++){
				sa=s1*cd+c1*sd;
				ca=c1*cd-s1*sd;
				for(j=0;j<3;j++){
					q[j] = *(sp+j) + (ca * c2 * (*(sp+j+3))) 
						+ (sa * c2 * (*(sp+j+6))) 
						+ (s2 * sgn * (*(sp+j+9)));
					r[j] = *(sp+j) + (c1 * ce* (*(sp+j+3))) 
						+ (s1 * ce * (*(sp+j+6))) 
						+ (se * sgn * (*(sp+j+9)));
				}
				tplane( p, q, r, t );
				s1=sa;
				c1=ca;
				for(j=0; j<3; j++) {
					p[j] = q[j];
					t[j] = r[j];
				}
			}
			c2=ce;
			s2=se;
		}
		sgn = -sgn;
	}
}
