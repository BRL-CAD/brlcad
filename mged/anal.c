/*
 *			A N A L
 *
 * Functions -
 *	f_analyze	"analyze" command
 *	findang		Given a normal vector, find rotation & fallback angles
 *
 *  Author -
 *	Keith A Applin
 *  
 *  Source -
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

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "./sedit.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"

extern struct rt_db_internal	es_int;	/* from edsol.c */
extern struct rt_tol		mged_tol;		/* from ged.c */

static void	do_anal();
static void	arb_anal();
static double	anal_face();
static void	anal_edge();
static double	find_vol();
static void	tgc_anal();
static void	ell_anal();
static void	tor_anal();
static void	ars_anal();
static void	rpc_anal();
static void	rhc_anal();

/*
 *			F _ A N A L Y Z E
 *
 *	Analyze command - prints loads of info about a solid
 *	Format:	analyze [name]
 *		if 'name' is missing use solid being edited
 */

int
f_analyze(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	*argv[];
{
	register struct directory *ndp;
	mat_t new_mat;
	register int i;
	struct bu_vls		v;
	struct rt_db_internal	intern;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	bu_vls_init(&v);

	if( argc == 1 ) {
		/* use the solid being edited */
		if (illump == SOLID_NULL) {
		  state_err( "Default SOLID Analyze" );
		  return TCL_ERROR;
		}
		ndp = illump->s_path[illump->s_last];
		if(illump->s_Eflag) {
		  Tcl_AppendResult(interp, "analyze: cannot analyze evaluated region containing ",
				   ndp->d_namep, "\n", (char *)NULL);
		  return TCL_ERROR;
		}
		switch( state ) {
		case ST_S_EDIT:
		  /* Use already modified version. "new way" */
		  do_anal(&v, &es_int);
		  Tcl_AppendResult(interp, bu_vls_addr(&v), (char *)NULL);
		  bu_vls_free(&v);
		  return TCL_OK;

		case ST_O_EDIT:
			/* use solid at bottom of path */
			break;

		default:
		  state_err( "Default SOLID Analyze" );
		  return TCL_ERROR;
		}
		mat_mul(new_mat, modelchanges, es_mat);

		if( rt_db_get_internal( &intern, ndp, dbip, new_mat ) < 0 )  {
		  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
		  return TCL_ERROR;
		}

		do_anal(&v, &intern);
		Tcl_AppendResult(interp, bu_vls_addr(&v), (char *)NULL);
		bu_vls_free(&v);
		rt_db_free_internal( &intern );
		return TCL_OK;
	}

	/* use the names that were input */
	for( i = 1; i < argc; i++ )  {
		if( (ndp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) == DIR_NULL )
			continue;

		if( rt_db_get_internal( &intern, ndp, dbip, rt_identity ) < 0 )  {
		  Tcl_AppendResult(interp, "rt_db_get_internal() error\n", (char *)NULL);
		  return TCL_ERROR;
		}

		bu_vls_trunc( &v, 0 );		
		do_list( &v, ndp, 1 );
		Tcl_AppendResult(interp, bu_vls_addr(&v), (char *)NULL);
		bu_vls_trunc( &v, 0 );

		do_anal(&v, &intern);
		Tcl_AppendResult(interp, bu_vls_addr(&v), (char *)NULL);
		bu_vls_free(&v);
		rt_db_free_internal( &intern );
	}

	return TCL_OK;
}


/* Analyze solid in internal form */
static void
do_anal(vp, ip)
struct bu_vls		*vp;
CONST struct rt_db_internal	*ip;
{
	/* XXX Could give solid name, and current units, here */

	switch( ip->idb_type ) {

	case ID_ARS:
		ars_anal(vp, ip);
		break;

	case ID_ARB8:
		arb_anal(vp, ip);
		break;

	case ID_TGC:
		tgc_anal(vp, ip);
		break;

	case ID_ELL:
		ell_anal(vp, ip);
		break;

	case ID_TOR:
		tor_anal(vp, ip);
		break;

	case ID_RPC:
		rpc_anal(vp, ip);
		break;

	case ID_RHC:
		rhc_anal(vp, ip);
		break;

	default:
		bu_vls_printf(vp,"analyze: unable to process %s solid\n",
			rt_functab[ip->idb_type].ft_name );
		break;
	}
}



/* edge definition array */
static CONST int nedge[5][24] = {
	{0,1, 1,2, 2,0, 0,3, 3,2, 1,3, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},	/* ARB4 */
	{0,1, 1,2, 2,3, 0,3, 0,4, 1,4, 2,4, 3,4, -1,-1,-1,-1,-1,-1,-1,-1},	/* ARB5 */
	{0,1, 1,2, 2,3, 0,3, 0,4, 1,4, 2,5, 3,5, 4,5, -1,-1,-1,-1,-1,-1},	/* ARB6 */
	{0,1, 1,2, 2,3, 0,3, 0,4, 3,4, 1,5, 2,6, 4,5, 5,6, 4,6, -1,-1},		/* ARB7 */
	{0,1, 1,2, 2,3, 0,3, 0,4, 4,5, 1,5, 5,6, 6,7, 4,7, 3,7, 2,6},
};

/*
 *			A R B _ A N A L
 */
static void
arb_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	struct rt_arb_internal	*arb = (struct rt_arb_internal *)ip->idb_ptr;
	register int	i;
	point_t		center_pt;
	double		tot_vol;
	double		tot_area;
	int		cgtype;		/* COMGEOM arb type: # of vertices */
	int		type;

	/* find the specific arb type, in GIFT order. */
	if( (cgtype = rt_arb_std_type( ip, &mged_tol )) == 0 ) {
		bu_vls_printf(vp,"arb_anal: bad ARB\n");
		return;
	}

	tot_area = tot_vol = 0.0;

	type = cgtype - 4;

	/* analyze each face, use center point of arb for reference */
	bu_vls_printf(vp,"\n------------------------------------------------------------------------------\n");
	bu_vls_printf(vp,"| FACE |   ROT     FB  |        PLANE EQUATION            |   SURFACE AREA   |\n");
	bu_vls_printf(vp,"|------|---------------|----------------------------------|------------------|\n");
	rt_arb_centroid( center_pt, arb, cgtype );

	for(i=0; i<6; i++) 
		tot_area += anal_face( vp, i, center_pt, arb, type, &mged_tol );

	bu_vls_printf(vp,"------------------------------------------------------------------------------\n");

	/* analyze each edge */
	bu_vls_printf(vp,"    | EDGE     LEN   | EDGE     LEN   | EDGE     LEN   | EDGE     LEN   |\n");
	bu_vls_printf(vp,"    |----------------|----------------|----------------|----------------|\n  ");

	/* set up the records for arb4's and arb6's */
	{
		struct rt_arb_internal	earb;

		earb = *arb;		/* struct copy */
		if( cgtype == 4 ) {
			VMOVE(earb.pt[3], earb.pt[4]);
		} else if( cgtype == 6 ) {
			VMOVE(earb.pt[5], earb.pt[6]);
		}
		for(i=0; i<12; i++) {
			anal_edge( vp, i, &earb, type );
			if( nedge[type][i*2] == -1 )
				break;
		}
	}
	bu_vls_printf(vp,"  ---------------------------------------------------------------------\n");

	/* find the volume - break arb8 into 6 arb4s */
	for(i=0; i<6; i++)
		tot_vol += find_vol( i, arb, &mged_tol );

	bu_vls_printf(vp,"      | Volume = %18.3f    Surface Area = %15.3f |\n",
			tot_vol*base2local*base2local*base2local,
			tot_area*base2local*base2local);
	bu_vls_printf(vp,"      |          %18.3f gal                               |\n",
		tot_vol/3787878.79);
	bu_vls_printf(vp,"      -----------------------------------------------------------------\n");
}

/* ARB face printout array */
static CONST int prface[5][6] = {
	{123, 124, 234, 134, -111, -111},	/* ARB4 */
	{1234, 125, 235, 345, 145, -111},	/* ARB5 */
	{1234, 2365, 1564, 512, 634, -111},	/* ARB6 */
	{1234, 567, 145, 2376, 1265, 4375},	/* ARB7 */
	{1234, 5678, 1584, 2376, 1265, 4378},	/* ARB8 */
};
/* division of an arb8 into 6 arb4s */
static CONST int farb4[6][4] = {
	{0, 1, 2, 4},
	{4, 5, 6, 1},
	{1, 2, 6, 4},
	{0, 2, 3, 4},
	{4, 6, 7, 2},
	{2, 3, 7, 4},
};


/*
 *			F I N D A N G
 *
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 fastf_t's to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
void
findang( angles, unitv )
register fastf_t	*angles;
register vect_t		unitv;
{
	FAST fastf_t f;

	/* convert direction cosines into axis angles */
	if( unitv[X] <= -1.0 )  angles[X] = -90.0;
	else if( unitv[X] >= 1.0 )  angles[X] = 90.0;
	else angles[X] = acos( unitv[X] ) * radtodeg;

	if( unitv[Y] <= -1.0 )  angles[Y] = -90.0;
	else if( unitv[Y] >= 1.0 )  angles[Y] = 90.0;
	else angles[Y] = acos( unitv[Y] ) * radtodeg;

	if( unitv[Z] <= -1.0 )  angles[Z] = -90.0;
	else if( unitv[Z] >= 1.0 )  angles[Z] = 90.0;
	else angles[Z] = acos( unitv[Z] ) * radtodeg;

	/* fallback angle */
	if( unitv[Z] <= -1.0 )  unitv[Z] = -1.0;
	else if( unitv[Z] >= 1.0 )  unitv[Z] = 1.0;
	angles[4] = asin(unitv[Z]);

	/* rotation angle */
	/* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
	 * with an epsilon of +/- 1.0e-17, so the tolerance below was
	 * substituted for the original +/- 1.0e-20.
	 */
	if((f = cos(angles[4])) > 1.0e-16 || f < -1.0e-16 )  {
		f = unitv[X]/f;
		if( f <= -1.0 )
			angles[3] = 180;
		else if( f >= 1.0 )
			angles[3] = 0;
		else
			angles[3] = radtodeg * acos( f );
	}  else
		angles[3] = 0.0;
	if( unitv[Y] < 0 )
		angles[3] = 360.0 - angles[3];

	angles[4] *= radtodeg;
}

/* 	Analyzes an arb face
 */
static double
anal_face( vp, face, center_pt, arb, type, tol )
struct bu_vls	*vp;
int		face;
point_t		center_pt;		/* reference center point */
CONST struct rt_arb_internal	*arb;
int		type;
CONST struct rt_tol	*tol;
{
	register int i, j, k;
	int a, b, c, d;		/* 4 points of face to look at */
	fastf_t	angles[5];	/* direction cosines, rot, fb */
	fastf_t	temp;
	fastf_t	area[2], len[6];
	vect_t	v_temp;
	plane_t	plane;
	double	face_area = 0;

	a = arb_faces[type][face*4+0];
	b = arb_faces[type][face*4+1];
	c = arb_faces[type][face*4+2];
	d = arb_faces[type][face*4+3];

	if(a == -1)
		return 0;

	/* find plane eqn for this face */
	if( rt_mk_plane_3pts( plane, arb->pt[a], arb->pt[b],
	    arb->pt[c], tol ) < 0 )  {
		bu_vls_printf(vp,"| %d%d%d%d |         ***NOT A PLANE***                                          |\n",
				a+1,b+1,c+1,d+1);
		return 0;
	}

	/* the plane equations returned by planeqn above do not
	 * necessarily point outward. Use the reference center
	 * point for the arb and reverse direction for
	 * any errant planes. This corrects the output rotation,
	 * fallback angles so that they always give the outward
	 * pointing normal vector.
	 */
	if( (plane[3] - VDOT(center_pt, &plane[0])) < 0.0 ){
		for( i=0; i<4 ; i++ )
			plane[i] *= -1.0;
	}

	/* plane[] contains normalized eqn of plane of this face
	 * find the dir cos, rot, fb angles
	 */
	findang( angles, &plane[0] );

	/* find the surface area of this face */
	for(i=0; i<3; i++) {
		j = arb_faces[type][face*4+i];
		k = arb_faces[type][face*4+i+1];
		VSUB2(v_temp, arb->pt[k], arb->pt[j]);
		len[i] = MAGNITUDE( v_temp );
	}
	len[4] = len[2];
	j = arb_faces[type][face*4+0];
	for(i=2; i<4; i++) {
		k = arb_faces[type][face*4+i];
		VSUB2(v_temp, arb->pt[k], arb->pt[j]);
		len[((i*2)-1)] = MAGNITUDE( v_temp );
	}
	len[2] = len[3];

	for(i=0; i<2; i++) {
		j = i*3;
		temp = .5 * (len[j] + len[j+1] + len[j+2]);
		area[i] = sqrt(temp * (temp - len[j]) * (temp - len[j+1]) * (temp - len[j+2]));
		face_area += area[i];
	}

	bu_vls_printf(vp,"| %4d |",prface[type][face]);
	bu_vls_printf(vp," %6.2f %6.2f | %6.3f %6.3f %6.3f %11.3f |",
		angles[3], angles[4],
		plane[0],plane[1],plane[2],
		plane[3]*base2local);
	bu_vls_printf(vp,"   %13.3f  |\n",
		(area[0]+area[1])*base2local*base2local);
	return face_area;
}

/*	Analyzes arb edges - finds lengths */
static void
anal_edge( vp, edge, arb, type )
struct bu_vls		*vp;
int			edge;
CONST struct rt_arb_internal	*arb;
int			type;
{
	register int a, b;
	static vect_t v_temp;

	a = nedge[type][edge*2];
	b = nedge[type][edge*2+1];

	if( b == -1 ) {
		/* fill out the line */
		if( (a = edge%4) == 0 ) 
			return;
		if( a == 1 ) {
			bu_vls_printf(vp,"  |                |                |                |\n  ");
			return;
		}
		if( a == 2 ) {
			bu_vls_printf(vp,"  |                |                |\n  ");
			return;
		}
		bu_vls_printf(vp,"  |                |\n  ");
		return;
	}

	VSUB2(v_temp, arb->pt[b], arb->pt[a]);
	bu_vls_printf(vp, "  |  %d%d %9.3f",
		a+1, b+1, MAGNITUDE(v_temp)*base2local);

	if( ++edge%4 == 0 )
		bu_vls_printf(vp,"  |\n  ");
}


/*	Finds volume of an arb4 defined by farb4[loc][] 	*/
static double
find_vol( loc, arb, tol )
int	loc;
struct rt_arb_internal	*arb;
struct rt_tol		*tol;
{
	int a, b, c, d;
	fastf_t vol, height, len[3], temp, areabase;
	vect_t	v_temp;
	plane_t	plane;

	/* a,b,c = base of the arb4 */
	a = farb4[loc][0];
	b = farb4[loc][1];
	c = farb4[loc][2];

	/* d = "top" point of arb4 */
	d = farb4[loc][3];

	if( rt_mk_plane_3pts( plane, arb->pt[a], arb->pt[b],
	    arb->pt[c], tol ) < 0 )
		return 0.0;

	/* have a good arb4 - find its volume */
	height = fabs(plane[3] - VDOT(&plane[0], arb->pt[d]));
	VSUB2(v_temp, arb->pt[b], arb->pt[a]);
	len[0] = MAGNITUDE(v_temp);
	VSUB2(v_temp, arb->pt[c], arb->pt[a]);
	len[1] = MAGNITUDE(v_temp);
	VSUB2(v_temp, arb->pt[c], arb->pt[b]);
	len[2] = MAGNITUDE(v_temp);
	temp = 0.5 * (len[0] + len[1] + len[2]);
	areabase = sqrt(temp * (temp-len[0]) * (temp-len[1]) * (temp-len[2]));
	vol = areabase * height / 3.0;
	return vol;
}

static double pi = 3.1415926535898;


/*	analyze a torus	*/
static void
tor_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	struct rt_tor_internal	*tor = (struct rt_tor_internal *)ip->idb_ptr;
	fastf_t r1, r2, vol, sur_area;

	RT_TOR_CK_MAGIC( tor );

	r1 = tor->r_a;
	r2 = tor->r_h;

	vol = 2.0 * pi * pi * r1 * r2 * r2;
	sur_area = 4.0 * pi * pi * r1 * r2;

	bu_vls_printf(vp,"TOR Vol = %.4f (%.4f gal)   Surface Area = %.4f\n",
		vol*base2local*base2local*base2local,
		vol/3787878.79,
		sur_area*base2local*base2local);

	return;
}

#define PROLATE 	1
#define OBLATE		2

/*	analyze an ell	*/
static void
ell_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	struct rt_ell_internal	*ell = (struct rt_ell_internal *)ip->idb_ptr;
	fastf_t ma, mb, mc;
#ifdef major		/* Some systems have these defined as macros!!! */
#undef major
#endif
#ifdef minor
#undef minor
#endif
	fastf_t ecc, major, minor;
	fastf_t vol, sur_area;
	int	type;

	RT_ELL_CK_MAGIC( ell );

	ma = MAGNITUDE( ell->a );
	mb = MAGNITUDE( ell->b );
	mc = MAGNITUDE( ell->c );

	type = 0;

	vol = 4.0 * pi * ma * mb * mc / 3.0;
	bu_vls_printf(vp,"ELL Volume = %.4f (%.4f gal)",
		vol*base2local*base2local*base2local,
		vol/3787878.79);

	if( fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001 ) {
		/* have a sphere */
		sur_area = 4.0 * pi * ma * ma;
		bu_vls_printf(vp,"   Surface Area = %.4f\n",
				sur_area*base2local*base2local);
		return;
	}
	if( fabs(ma-mb) < .00001 ) {
		/* A == B */
		if( mc > ma ) {
			/* oblate spheroid */
			type = OBLATE;
			major = mc;
			minor = ma;
		}
		else {
			/* prolate spheroid */
			type = PROLATE;
			major = ma;
			minor = mc;
		}
	}
	else
	if( fabs(ma-mc) < .00001 ) {
		/* A == C */
		if( mb > ma ) {
			/* oblate spheroid */
			type = OBLATE;
			major = mb;
			minor = ma;
		}
		else {
			/* prolate spheroid */
			type = PROLATE;
			major = ma;
			minor = mb;
		}
	}
	else
	if( fabs(mb-mc) < .00001 ) {
		/* B == C */
		if( ma > mb ) {
			/* oblate spheroid */
			type = OBLATE;
			major = ma;
			minor = mb;
		}
		else {
			/* prolate spheroid */
			type = PROLATE;
			major = mb;
			minor = ma;
		}
	}
	else {
		bu_vls_printf(vp,"   Cannot find surface area\n");
		return;
	}
	ecc = sqrt(major*major - minor*minor) / major;
	if( type == PROLATE ) {
		sur_area = 2.0 * pi * minor * minor +
			(2.0 * pi * (major*minor/ecc) * asin(ecc));
	} else if( type == OBLATE ) {
		sur_area = 2.0 * pi * major * major +
			(pi * (minor*minor/ecc) * log( (1.0+ecc)/(1.0-ecc) ));
	} else {
		sur_area = 0.0;
	}

	bu_vls_printf(vp,"   Surface Area = %.4f\n",
			sur_area*base2local*base2local);
}

#define MGED_ANAL_RCC	1
#define MGED_ANAL_TRC	2
#define MGED_ANAL_REC	3

/*	analyze tgc */
static void
tgc_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	struct rt_tgc_internal	*tgc = (struct rt_tgc_internal *)ip->idb_ptr;
	fastf_t maxb, ma, mb, mc, md, mh;
	fastf_t area_base, area_top, area_side, vol;
	vect_t axb;
	int cgtype = 0;

	RT_TGC_CK_MAGIC( tgc );

	VCROSS(axb, tgc->a, tgc->b);
	maxb = MAGNITUDE(axb);
	ma = MAGNITUDE( tgc->a );
	mb = MAGNITUDE( tgc->b );
	mc = MAGNITUDE( tgc->c );
	md = MAGNITUDE( tgc->d );
	mh = MAGNITUDE( tgc->h );

	/* check for right cylinder */
	if( fabs(fabs(VDOT(tgc->h,axb)) - (mh*maxb)) < .00001 ) {
		/* have a right cylinder */
		if(fabs(ma-mb) < .00001) {
			/* have a circular base */
			if(fabs(mc-md) < .00001) {
				/* have a circular top */
				if(fabs(ma-mc) < .00001)
					cgtype = MGED_ANAL_RCC;
				else
					cgtype = MGED_ANAL_TRC;
			}
		}
		else {
			/* have an elliptical base */
			if(fabs(ma-mc) < .00001 && fabs(mb-md) < .00001)
				cgtype = MGED_ANAL_REC;
		}
	}

	switch( cgtype ) {

		case MGED_ANAL_RCC:
			area_base = pi * ma * ma;
			area_top = area_base;
			area_side = 2.0 * pi * ma * mh;
			vol = pi * ma * ma * mh;
			bu_vls_printf(vp, "RCC ");
			break;

		case MGED_ANAL_TRC:
			area_base = pi * ma * ma;
			area_top = pi * mc * mc;
			area_side = pi * (ma+mc) * sqrt((ma-mc)*(ma-mc)+(mh*mh));
			vol = pi * mh * (ma*ma + mc*mc + ma*mc) / 3.0;
			bu_vls_printf(vp, "TRC ");
			break;

		case MGED_ANAL_REC:
			area_base = pi * ma * mb;
			area_top = pi * mc * md;
			/* approximate */
			area_side = 2.0 * pi * mh * sqrt(0.5 * (ma*ma + mb*mb));
			vol = pi * ma * mb * mh;
			bu_vls_printf(vp, "REC ");
			break;

		default:
			bu_vls_printf(vp,"TGC Cannot find areas and volume\n");
			return;
	}

	/* print the results */
	bu_vls_printf(vp,"Surface Areas:  base(AxB)=%.4f  top(CxD)=%.4f  side=%.4f\n",
			area_base*base2local*base2local,
			area_top*base2local*base2local,
			area_side*base2local*base2local);
	bu_vls_printf(vp,"Total Surface Area=%.4f    Volume=%.4f (%.4f gal)\n",
			(area_base+area_top+area_side)*base2local*base2local,
			vol*base2local*base2local*base2local,vol/3787878.79);
	/* Print units? */
	return;

}



/*	analyze ars */
static void
ars_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	bu_vls_printf(vp,"ARS analyze not implemented\n");
}

/*	XXX	analyze spline needed
 * static void
 * spline_anal(vp, ip)
 * struct bu_vls	*vp;
 * CONST struct rt_db_internal	*ip;
 * {
 * 	bu_vls_printf(vp,"SPLINE analyze not implemented\n");
 * }
 */

#define arcsinh(x) (log((x) + sqrt((x)*(x) + 1.)))

/*	analyze rpc */
static void
rpc_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	fastf_t	area_parab, area_body, b, h, r, vol_parab;
	struct rt_rpc_internal	*rpc = (struct rt_rpc_internal *)ip->idb_ptr;

	RT_RPC_CK_MAGIC( rpc );

	b = MAGNITUDE( rpc->rpc_B );
	h = MAGNITUDE( rpc->rpc_H );
	r = rpc->rpc_r;
	
	/* area of one parabolic side */
	area_parab = 4./3 * b*r;
	
	/* volume of rpc */
	vol_parab = area_parab*h;
	
	/* surface area of parabolic body */
	area_body = .5*sqrt(r*r + 4.*b*b) + .25*r*r/b*arcsinh(2.*b/r);
	area_body *= 2.;

	bu_vls_printf(vp,"Surface Areas:  front(BxR)=%.4f  top(RxH)=%.4f  body=%.4f\n",
			area_parab*base2local*base2local,
			2*r*h*base2local*base2local,
			area_body*base2local*base2local);
	bu_vls_printf(vp,"Total Surface Area=%.4f    Volume=%.4f (%.4f gal)\n",
			(2*area_parab+2*r*h+area_body)*base2local*base2local,
			vol_parab*base2local*base2local*base2local,
			vol_parab/3787878.79);
}

/*	analyze rhc */
static void
rhc_anal(vp, ip)
struct bu_vls	*vp;
CONST struct rt_db_internal	*ip;
{
	fastf_t	area_hyperb, area_body, b, c, h, r, vol_hyperb,	work1;
	struct rt_rhc_internal	*rhc = (struct rt_rhc_internal *)ip->idb_ptr;

	RT_RHC_CK_MAGIC( rhc );

	b = MAGNITUDE( rhc->rhc_B );
	h = MAGNITUDE( rhc->rhc_H );
	r = rhc->rhc_r;
	c = rhc->rhc_c;
	
	/* area of one hyperbolic side (from macsyma) WRONG!!!! */
	work1 = sqrt(b*(b + 2.*c));
	area_hyperb = -2.*r*work1*(.5*(b+c) + c*c*log(c/(work1 + b + c)));

	/* volume of rhc */
	vol_hyperb = area_hyperb*h;
	
	/* surface area of hyperbolic body */
	area_body=0.;
#if 0
	k = (b+c)*(b+c) - c*c;
#define X_eval(y) sqrt( 1. + (4.*k)/(r*r*k*k*(y)*(y) + r*r*c*c) )
#define L_eval(y) .5*k*(y)*X_eval(y) \
		  + r*k*(r*r*c*c + 4.*k - r*r*c*c/k)*arcsinh((y)*sqrt(k)/c)
	area_body = 2.*(L_eval(r) - L_eval(0.));
#endif

	bu_vls_printf(vp,"Surface Areas:  front(BxR)=%.4f  top(RxH)=%.4f  body=%.4f\n",
			area_hyperb*base2local*base2local,
			2*r*h*base2local*base2local,
			area_body*base2local*base2local);
	bu_vls_printf(vp,"Total Surface Area=%.4f    Volume=%.4f (%.4f gal)\n",
			(2*area_hyperb+2*r*h+2*area_body)*base2local*base2local,
			vol_hyperb*base2local*base2local*base2local,
			vol_hyperb/3787878.79);
}


/*
 *  		M A T H E R R
 *  
 *  Sys-V math-library error catcher.
 *  Some callers of acos trip over DOMAIN errors all the time, so...
 */
#ifdef HAVE_MATHERR
int
matherr(x)
struct exception *x;
{
	return(1);	/* be quiet */
}
#endif
