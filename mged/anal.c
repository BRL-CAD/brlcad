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

#include <math.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./sedit.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

extern void	arb_center();

static void	do_anal();
static void	arb_anal();
static double	anal_face();
static void	anal_edge();
static double	find_vol();
static void	tgc_anal();
static void	ell_anal();
static void	tor_anal();
static void	ars_anal();

/****************************************************************/
/********************************* XXX move to librt ************/
/*
 *			R T _ D B _ G E T _ I N T E R N A L
 *
 *  Get an object from the database, and convert it into it's internal
 *  representation.
 */
int
rt_db_get_internal( ip, dp, dbip, mat )
struct rt_db_internal	*ip;
struct directory	*dp;
struct db_i		*dbip;
CONST mat_t		mat;
{
	struct rt_external	ext;
	register int		id;

	RT_INIT_EXTERNAL(&ext);
	RT_INIT_DB_INTERNAL(ip);
	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	id = rt_id_solid( &ext );
	if( rt_functab[id].ft_import( ip, &ext, mat ) < 0 )  {
		rt_log("rt_db_get_internal(%s):  solid import failure\n",
			dp->d_namep );
	    	if( ip->idb_ptr )  rt_functab[id].ft_ifree( ip );
		db_free_external( &ext );
		return -1;		/* FAIL */
	}
	db_free_external( &ext );
	RT_CK_DB_INTERNAL( ip );
	return 0;			/* OK */
}

/*
 *			R T _ D B _ P U T _ I N T E R N A L
 *
 *  Convert the internal representation of a solid to the external one,
 *  and write it into the database.
 *  On success only, the internal representation is freed.
 *
 *  Returns -
 *	<0	error
 *	 0	success
 */
int
rt_db_put_internal( dp, dbip, ip )
struct rt_db_internal	*ip;
struct directory	*dp;
struct db_i		*dbip;
{
	struct rt_external	ext;
	register int		id;

	RT_INIT_EXTERNAL(&ext);
	RT_CK_DB_INTERNAL( ip );

	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[ip->idb_type].ft_export( &ext, ip, 1.0 ) < 0 )  {
		rt_log("rt_db_put_internal(%s):  solid export failure\n",
			dp->d_namep);
		db_free_external( &ext );
		return -2;		/* FAIL */
	}

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		db_free_external( &ext );
		return -1;		/* FAIL */
	}

    	if( ip->idb_ptr )  rt_functab[ip->idb_type].ft_ifree( ip );
	RT_INIT_DB_INTERNAL(ip);
	db_free_external( &ext );
	return 0;			/* OK */
}

void
rt_db_free_internal( ip )
struct rt_db_internal	*ip;
{
	RT_CK_DB_INTERNAL( ip );
    	if( ip->idb_ptr )  rt_functab[ip->idb_type].ft_ifree( ip );
	RT_INIT_DB_INTERNAL(ip);
}

/****************************************************************/

/*
 *			F _ A N A L Y Z E
 *
 *	Analyze command - prints loads of info about a solid
 *	Format:	analyze [name]
 *		if 'name' is missing use solid being edited
 */

void
f_analyze(argc, argv)
int	argc;
char	*argv[];
{
	register struct directory *ndp;
	mat_t new_mat;
	register int i;
	struct rt_vls		v;
	struct rt_db_internal	intern;

	RT_VLS_INIT(&v);

	if( argc == 1 ) {
		/* use the solid being edited */
		ndp = illump->s_path[illump->s_last];
		if(illump->s_Eflag) {
			(void)printf("analyze: cannot analyze evaluated region containing %s\n",
				ndp->d_namep);
			return;
		}
		switch( state ) {
		case ST_S_EDIT:
			mat_idn( modelchanges ); /* just to make sure */
			break;

		case ST_O_EDIT:
			/* use solid at bottom of path */
			break;

		default:
			state_err( "Default SOLID Analyze" );
			return;
		}
		mat_mul(new_mat, modelchanges, es_mat);

		if( rt_db_get_internal( &intern, ndp, dbip, new_mat ) < 0 )  {
			(void)printf("rt_db_get_internal() error\n");
			return;
		}

		do_anal(&v, &intern);
		fputs( rt_vls_addr(&v), stdout );
		rt_db_free_internal( &intern );
		return;
	}

	/* use the names that were input */
	for( i = 1; i < argc; i++ )  {
		if( (ndp = db_lookup( dbip,  argv[i], LOOKUP_NOISY )) == DIR_NULL )
			continue;

		if( rt_db_get_internal( &intern, ndp, dbip, rt_identity ) < 0 )  {
			(void)printf("rt_db_get_internal() error\n");
			return;
		}

		do_list( stdout, ndp, 1 );
		do_anal(&v, &intern);
		fputs( rt_vls_addr(&v), stdout );
		rt_vls_free(&v);
		rt_db_free_internal( &intern );
	}
}


/* Analyze solid in internal form */
static void
do_anal(vp, ip)
struct rt_vls		*vp;
struct rt_db_internal	*ip;
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

	default:
		rt_vls_printf(vp,"analyze: unable to process %s solid\n",
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
struct rt_vls	*vp;
struct rt_db_internal	*ip;
{
	struct rt_arb_internal	*arb = (struct rt_arb_internal *)ip->idb_ptr;
	register int	i;
	point_t		center_pt;
	double		tot_vol;
	double		tot_area;
	struct rt_tol	tol;
	int		cgtype;		/* COMGEOM arb type: # of vertices */
	int		type;

	/* XXX These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;	/* 0.005 matches planeqn() val, 0.0001 matches dist checking */
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	/* find the specific arb type, in GIFT order. */
	if( (cgtype = rt_arb_std_type( ip, &tol )) == 0 ) {
		rt_vls_printf(vp,"arb_anal: bad ARB\n");
		return;
	}

	tot_area = tot_vol = 0.0;

	type = cgtype - 4;

	/* analyze each face, use center point of arb for reference */
	rt_vls_printf(vp,"\n------------------------------------------------------------------------------\n");
	rt_vls_printf(vp,"| FACE |   ROT     FB  |        PLANE EQUATION            |   SURFACE AREA   |\n");
	rt_vls_printf(vp,"|------|---------------|----------------------------------|------------------|\n");
	rt_arb_centroid( center_pt, arb, cgtype );
VPRINT("center_pt", center_pt);

	for(i=0; i<6; i++) 
		tot_area += anal_face( vp, i, center_pt, arb, type, &tol );

	rt_vls_printf(vp,"------------------------------------------------------------------------------\n");

	/* analyze each edge */
	rt_vls_printf(vp,"    | EDGE     LEN   | EDGE     LEN   | EDGE     LEN   | EDGE     LEN   |\n");
	rt_vls_printf(vp,"    |----------------|----------------|----------------|----------------|\n  ");

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
	rt_vls_printf(vp,"  ---------------------------------------------------------------------\n");

	/* find the volume - break arb8 into 6 arb4s */
	for(i=0; i<6; i++)
		tot_vol += find_vol( i, arb, &tol );

	rt_vls_printf(vp,"      | Volume = %18.3f    Surface Area = %15.3f |\n",
			tot_vol*base2local*base2local*base2local,
			tot_area*base2local*base2local);
	rt_vls_printf(vp,"      |          %18.3f gal                               |\n",
		tot_vol/3787878.79);
	rt_vls_printf(vp,"      -----------------------------------------------------------------\n");
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


/* 	Analyzes an arb face
 */
static double
anal_face( vp, face, center_pt, arb, type, tol )
struct rt_vls	*vp;
int		face;
point_t		center_pt;		/* reference center point */
struct rt_arb_internal	*arb;
int		type;
struct rt_tol	*tol;
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
		rt_vls_printf(vp,"| %d%d%d%d |         ***NOT A PLANE***                                          |\n",
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

	rt_vls_printf(vp,"| %4d |",prface[type][face]);
	rt_vls_printf(vp," %6.2f %6.2f | %6.3f %6.3f %6.3f %11.3f |",
		angles[3], angles[4],
		plane[0],plane[1],plane[2],
		plane[3]*base2local);
	rt_vls_printf(vp,"   %13.3f  |\n",
		(area[0]+area[1])*base2local*base2local);
	return face_area;
}

/*	Analyzes arb edges - finds lengths */
static void
anal_edge( vp, edge, arb, type )
struct rt_vls		*vp;
int			edge;
struct rt_arb_internal	*arb;
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
			rt_vls_printf(vp,"  |                |                |                |\n  ");
			return;
		}
		if( a == 2 ) {
			rt_vls_printf(vp,"  |                |                |\n  ");
			return;
		}
		rt_vls_printf(vp,"  |                |\n  ");
		return;
	}

	VSUB2(v_temp, arb->pt[b], arb->pt[a]);
	rt_vls_printf(vp, "  |  %d%d %9.3f",
		a+1, b+1, MAGNITUDE(v_temp)*base2local);

	if( ++edge%4 == 0 )
		rt_vls_printf(vp,"  |\n  ");
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
struct rt_vls	*vp;
struct rt_db_internal	*ip;
{
	struct rt_tor_internal	*tor = (struct rt_tor_internal *)ip->idb_ptr;
	fastf_t r1, r2, vol, sur_area;

	RT_TOR_CK_MAGIC( tor );

	r1 = tor->r_a;
	r2 = tor->r_h;

	vol = 2.0 * pi * pi * r1 * r2 * r2;
	sur_area = 4.0 * pi * pi * r1 * r2;

	rt_vls_printf(vp,"TOR Vol = %.4f (%.4f gal)   Surface Area = %.4f\n",
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
struct rt_vls	*vp;
struct rt_db_internal	*ip;
{
	struct rt_ell_internal	*ell = (struct rt_ell_internal *)ip->idb_ptr;
	fastf_t ma, mb, mc;
	fastf_t ecc, major, minor;
	fastf_t vol, sur_area;
	int	type;

	RT_ELL_CK_MAGIC( ell );

	ma = MAGNITUDE( ell->a );
	mb = MAGNITUDE( ell->b );
	mc = MAGNITUDE( ell->c );

	type = 0;

	vol = 4.0 * pi * ma * mb * mc / 3.0;
	rt_vls_printf(vp,"ELL Volume = %.4f (%.4f gal)",
		vol*base2local*base2local*base2local,
		vol/3787878.79);

	if( fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001 ) {
		/* have a sphere */
		sur_area = 4.0 * pi * ma * ma;
		rt_vls_printf(vp,"   Surface Area = %.4f\n",
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
		rt_vls_printf(vp,"   Cannot find surface area\n");
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

	rt_vls_printf(vp,"   Surface Area = %.4f\n",
			sur_area*base2local*base2local);
}


/*	analyze tgc */
static void
tgc_anal(vp, ip)
struct rt_vls	*vp;
struct rt_db_internal	*ip;
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
					cgtype = RCC;
				else
					cgtype = TRC;
			}
		}
		else {
			/* have an elliptical base */
			if(fabs(ma-mc) < .00001 && fabs(mb-md) < .00001)
				cgtype = REC;
		}
	}

	switch( cgtype ) {

		case RCC:
			area_base = pi * ma * ma;
			area_top = area_base;
			area_side = 2.0 * pi * ma * mh;
			vol = pi * ma * ma * mh;
			rt_vls_printf(vp, "RCC ");
			break;

		case TRC:
			area_base = pi * ma * ma;
			area_top = pi * mc * mc;
			area_side = pi * (ma+mc) * sqrt((ma-mc)*(ma-mc)+(mh*mh));
			vol = pi * mh * (ma*ma + mc*mc + ma*mc) / 3.0;
			rt_vls_printf(vp, "TRC ");
			break;

		case REC:
			area_base = pi * ma * mb;
			area_top = pi * mc * md;
			/* approximate */
			area_side = 2.0 * pi * mh * sqrt(0.5 * (ma*ma + mb*mb));
			vol = pi * ma * mb * mh;
			rt_vls_printf(vp, "REC ");
			break;

		case TEC:
			rt_vls_printf(vp,"TEC Cannot find areas and volume\n");
			return;
		case TGC:
		default:
			rt_vls_printf(vp,"TGC Cannot find areas and volume\n");
			return;
	}

	/* print the results */
	rt_vls_printf(vp,"Surface Areas:  base(AxB)=%.4f  top(CxD)=%.4f  side=%.4f\n",
			area_base*base2local*base2local,
			area_top*base2local*base2local,
			area_side*base2local*base2local);
	rt_vls_printf(vp,"Total Surface Area=%.4f    Volume=%.4f (%.4f gal)\n",
			(area_base+area_top+area_side)*base2local*base2local,
			vol*base2local*base2local*base2local,vol/3787878.79);
	/* Print units? */
	return;

}



/*	anaylze ars */
static void
ars_anal(vp, ip)
struct rt_vls	*vp;
struct rt_db_internal	*ip;
{
	rt_vls_printf(vp,"ARS analyze not implimented\n");
}

/*	anaylze spline */
static void
spline_anal(vp, ip)
struct rt_vls	*vp;
struct rt_db_internal	*ip;
{
	rt_vls_printf(vp,"SPLINE analyze not implimented\n");
}

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

/*
 *  		M A T H E R R
 *  
 *  Sys-V math-library error catcher.
 *  Some callers of acos trip over DOMAIN errors all the time, so...
 */
#if !__STDC__ && ( defined(SYSV) || BSD >= 43 )
int
matherr(x)
struct exception *x;
{
	return(1);	/* be quiet */
}
#endif
