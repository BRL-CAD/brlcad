/*
 *			A N A L
 *
 * Functions -
 *
 *
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include	<math.h>
#include	<stdio.h>
#include "ged_types.h"
#include "db.h"
#include "sedit.h"
#include "ged.h"
#include "dir.h"
#include "solid.h"
#include "dm.h"
#include "vmath.h"

extern int	atoi();

extern int	numargs;	/* number of args */
extern char	*cmd_args[];	/* array of pointers to args */

static void	arb_anal();
static void	anal_face();
static void	anal_edge();
static void	find_vol();
static void	tgc_anal();
static void	ell_anal();
static void	tor_anal();
static void	ars_anal();

/*	Analyze command - prints loads of info about a solid
 *	Format:	analyze [name]
 *		if 'name' is missing use solid being edited
 */

static union record temp_rec;		/* local copy of es_rec */
float	tot_vol, tot_area;

void
f_analyze()
{
	struct directory *ndp;
	mat_t new_mat;
	int i;

	if( numargs == 1 ) {
		/* use the solid being edited */
		switch( state ) {

		case ST_S_EDIT:
			temp_rec = es_rec;
			/* just to make sure */
			mat_idn( modelchanges );
		break;

		case ST_O_EDIT:
			if(illump->s_Eflag) {
				(void)printf("Analyze: cannot analyze evaluated region\n");
				return;
			}
			/* use solid at bottom of path */
			temp_rec = es_rec;
		break;

		default:
			state_err( "Default SOLID Analyze" );
		return;
		}
		mat_mul(new_mat, modelchanges, es_mat);
		MAT4X3PNT(temp_rec.s.s_values, new_mat, es_rec.s.s_values);
		for(i=1; i<8; i++) {
			MAT4X3VEC( &temp_rec.s.s_values[i*3], new_mat,
					&es_rec.s.s_values[i*3] );
		}
	} else {
		/* use the name that was input */
		if( (ndp = lookup( cmd_args[1], LOOKUP_NOISY )) == DIR_NULL )
			return;

		db_getrec(ndp, &temp_rec, 0);

		if(temp_rec.u_id == ID_P_HEAD) {
			(void)printf("Analyze cannot handle polygons\n");
			return;
		}
		if(temp_rec.u_id != ID_SOLID && 
		   temp_rec.u_id != ID_ARS_A) {
			(void)printf("%s: not a solid \n",cmd_args[1]);
			return;
		}
		f_list();
	}

	/* Now have a solid in temp_rec */
	switch( temp_rec.s.s_type ) {

		case ARS:
			ars_anal();
		break;

		case GENARB8:
			arb_anal();
		break;

		case GENTGC:
			tgc_anal();
		break;

		case GENELL:
			ell_anal();
		break;

		case TOR:
			tor_anal();
		break;

		default:
			(void)printf("Analyze: unknown solid type\n");
		return;
	}

}


static void
arb_anal()
{
	int i;

	/* got the arb - convert to point notation */
	for(i=3; i<24; i+=3) {
		VADD2( &temp_rec.s.s_values[i], &temp_rec.s.s_values[i], temp_rec.s.s_values );
	}

	tot_area = tot_vol = 0.0;

	/* analyze each face */
	(void)printf("\n--------------------------------------------------------------------------\n");
	(void)printf("| FACE |   ROT     FB  |        PLANE EQUATION        |   SURFACE AREA   |\n");
	(void)printf("|------|---------------|------------------------------|------------------|\n");
	for(i=0; i<6; i++) 
		anal_face( i );

	(void)printf("--------------------------------------------------------------------------\n");

	/* analyze each edge */
	(void)printf("    | EDGE     LEN  | EDGE     LEN  | EDGE     LEN  | EDGE     LEN  |\n");
	(void)printf("    |---------------|---------------|---------------|---------------|\n  ");
	for(i=0; i<12; i++)
		anal_edge( i );

	(void)printf("  -----------------------------------------------------------------\n");

	/* find the volume - break arb8 into 6 arb4s */
	for(i=0; i<6; i++)
		find_vol( i );

	(void)printf("    | Surface Area = %15.3f    Volume = %18.3f |\n",
			tot_area*base2local*base2local,
			tot_vol*base2local*base2local*base2local);
	(void)printf("    -----------------------------------------------------------------\n");

	return;
}

/* face definition array */
static int nface[6][4] = {
	{0, 1, 2, 3},
	{4, 5, 6, 7},
	{0, 4, 7, 3},
	{1, 2, 6, 5},
	{3, 2, 6, 7},
	{0, 1, 5, 4},
};

/* edge definition array */
static int nedge[12][2] = {
	{0, 1},
	{1, 2},
	{2, 3},
	{3, 0},
	{0, 4},
	{4, 5},
	{5, 1},
	{5, 6},
	{6, 7},
	{7, 4},
	{7, 3},
	{2, 6},
};

/* division of an arb8 into 6 arb4s */
static int farb4[6][4] = {
	{0, 1, 2, 4},
	{4, 5, 6, 1},
	{1, 2, 6, 4},
	{0, 2, 3, 4},
	{4, 6, 7, 2},
	{2, 3, 7, 4},
};


/* 	Analyzes an arb face
 */
static void
anal_face( face )
int face;
{
	register int i, j, k;
	static int a, b, c, d;		/* 4 points of face to look at */
	static float angles[5];	/* direction cosines, rot, fb */
	static float temp, area[2], len[6];
	static vect_t v_temp;

	a = nface[face][0];
	b = nface[face][1];
	c = nface[face][2];
	d = nface[face][3];

	/* find plane eqn for this face */
	if( plane(a, b, c, d, &temp_rec.s) >= 0 ) {
		(void)printf("| %d%d%d%d    ***NOT A PLANE***                                          |\n",
				a+1,b+1,c+1,d+1);
		return;
	}

	/* es_plant[] contains normalized eqn of plane of this face
	 * find the dir cos, rot, fb angles
	 */
	findang( angles, es_plant );

	/* find the surface area of this face */
	for(i=0; i<3; i++) {
		j = nface[face][i];
		k = nface[face][i+1];
		VSUB2(v_temp, &temp_rec.s.s_values[k*3], &temp_rec.s.s_values[j*3]);
		len[i] = MAGNITUDE( v_temp );
	}
	len[4] = len[2];
	j = nface[face][0];
	for(i=2; i<4; i++) {
		k = nface[face][i];
		VSUB2(v_temp, &temp_rec.s.s_values[k*3], &temp_rec.s.s_values[j*3]);
		len[((i*2)-1)] = MAGNITUDE( v_temp );
	}
	len[2] = len[3];

	for(i=0; i<2; i++) {
		j = i*3;
		temp = .5 * (len[j] + len[j+1] + len[j+2]);
		area[i] = sqrt(temp * (temp - len[j]) * (temp - len[j+1]) * (temp - len[j+2]));
		tot_area += area[i];
	}

	(void)printf("| %d%d%d%d |",a+1,b+1,c+1,d+1);
	(void)printf(" %6.2f %6.2f | %5.2f %5.2f %5.2f %10.2f |",angles[3],angles[4],
			es_plant[0],es_plant[1],es_plant[2],es_plant[3]*base2local);
	(void)printf("   %13.3f  |\n",(area[0]+area[1])*base2local*base2local);
}

/*	Analyzes arb edges - finds lengths */
static void
anal_edge( edge )
int edge;
{
	register int a, b;
	static vect_t v_temp;

	a = nedge[edge][0];
	b = nedge[edge][1];

	VSUB2(v_temp, &temp_rec.s.s_values[b*3], &temp_rec.s.s_values[a*3]);
	(void)printf("  |  %d%d %8.2f",a+1,b+1,MAGNITUDE(v_temp)*base2local);

	if( ++edge%4 == 0 )
		(void)printf("  |\n  ");

}


/*	Finds volume of an arb4 defined by farb4[loc][] 	*/
static void
find_vol( loc )
int loc;
{
	int a, b, c, d;
	float vol, height, len[3], temp, areabase;
	vect_t v_temp;

	/* a,b,c = base of the arb4 */
	a = farb4[loc][0];
	b = farb4[loc][1];
	c = farb4[loc][2];

	/* d = "top" point of arb4 */
	d = farb4[loc][3];

	vol = 0.0;	/* volume of this arb */

	if( planeqn(a, b, c, &temp_rec.s) ) {
		/* have a good arb4 - find its volume */
		height = fabs(es_plant[3] - VDOT(es_plant, &temp_rec.s.s_values[d*3]));
		VSUB2(v_temp, &temp_rec.s.s_values[b*3], &temp_rec.s.s_values[a*3]);
		len[0] = MAGNITUDE(v_temp);
		VSUB2(v_temp, &temp_rec.s.s_values[c*3], &temp_rec.s.s_values[a*3]);
		len[1] = MAGNITUDE(v_temp);
		VSUB2(v_temp, &temp_rec.s.s_values[c*3], &temp_rec.s.s_values[b*3]);
		len[2] = MAGNITUDE(v_temp);
		temp = 0.5 * (len[0] + len[1] + len[2]);
		areabase = sqrt(temp * (temp-len[0]) * (temp-len[1]) * (temp-len[2]));
		vol = areabase * height / 3.0;
	}
	tot_vol += vol;
}

static double pi = 3.1415926535898;


/*	analyze a torus	*/
static void
tor_anal()
{
	float r1, r2, vol, sur_area;

	r1 = MAGNITUDE( &temp_rec.s.s_values[6] );
	r2 = MAGNITUDE( &temp_rec.s.s_values[3] );

	vol = 2.0 * pi * pi * r1 * r2 * r2;
	sur_area = 4.0 * pi * pi * r1 * r2;

	(void)printf("Vol = %.4f   Surface Area = %.4f\n",
			vol*base2local*base2local*base2local,
			sur_area*base2local*base2local);

	return;
}

#define PROLATE 	1
#define OBLATE		2

/*	analyze an ell	*/
static void
ell_anal()
{
	float ma, mb, mc;
	float ecc, major, minor;
	int type = 0;
	float vol, sur_area;

	ma = MAGNITUDE( &temp_rec.s.s_values[3] );
	mb = MAGNITUDE( &temp_rec.s.s_values[6] );
	mc = MAGNITUDE( &temp_rec.s.s_values[9] );

	vol = 4.0 * pi * ma * mb * mc / 3.0;
	(void)printf("Volume = %.4f",vol*base2local*base2local*base2local);

	if( fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001 ) {
		/* have a sphere */
		sur_area = 4.0 * pi * ma * ma;
		(void)printf("   Surface Area = %.4f\n",
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
		(void)printf("   Cannot find surface area\n");
		return;
	}
	ecc = sqrt(major*major - minor*minor) / major;
	if( type == PROLATE ) {
		sur_area = 2.0 * pi * minor * minor +
			(2.0 * pi * (major*minor/ecc) * asin(ecc));
	}
	if( type == OBLATE ) {
		sur_area = 2.0 * pi * major * major +
			(pi * (minor*minor/ecc) * log( (1.0+ecc)/(1.0-ecc) ));
	}

	(void)printf("   Surface Area = %.4f\n",
			sur_area*base2local*base2local);

	return;
}


/*	analyze tgc */
static void
tgc_anal()
{
	float maxb, ma, mb, mc, md, mh;
	float area_base, area_top, area_side, vol;
	vect_t axb;
	int type = 0;

	VCROSS(axb, &temp_rec.s.s_values[6], &temp_rec.s.s_values[9]);
	maxb = MAGNITUDE(axb);
	ma = MAGNITUDE( &temp_rec.s.s_values[6] );
	mb = MAGNITUDE( &temp_rec.s.s_values[9] );
	mc = MAGNITUDE( &temp_rec.s.s_values[12] );
	md = MAGNITUDE( &temp_rec.s.s_values[15] );
	mh = MAGNITUDE( &temp_rec.s.s_values[3] );

	/* check for right cylinder */
	if( fabs(fabs(VDOT(&temp_rec.s.s_values[3],axb)) - (mh*maxb)) < .00001 ) {
		/* have a right cylinder */
		if(fabs(ma-mb) < .00001) {
			/* have a circular base */
			if(fabs(mc-md) < .00001) {
				/* have a circular top */
				if(fabs(ma-mc) < .00001)
					type = RCC;
				else
					type = TRC;
			}
		}
		else {
			/* have an elliptical base */
			if(fabs(ma-mc) < .00001 && fabs(mb-md) < .00001)
				type = REC;
		}
	}

	switch( type ) {

		case RCC:
			area_base = pi * ma * ma;
			area_top = area_base;
			area_side = 2.0 * pi * ma * mh;
			vol = pi * ma * ma * mh;
		break;

		case TRC:
			area_base = pi * ma * ma;
			area_top = pi * mc * mc;
			area_side = pi * (ma+mc) * sqrt((ma-mc)*(ma-mc)+(mh*mh));
			vol = pi * mh * (ma*ma + mc*mc + ma*mc) / 3.0;
		break;

		case REC:
			area_base = pi * ma * mb;
			area_top = pi * mc * md;
			/* approximate */
			area_side = 2.0 * pi * mh * sqrt(0.5 * (ma*ma + mb*mb));
			vol = pi * ma * mb * mh;
		break;

		case TEC:
		case TGC:
		default:
			(void)printf("Cannot find areas and volume\n");
		return;
	}

	/* print the results */
	(void)printf("Surface Areas:  base(AxB)=%.4f  top(CxD)=%.4f  side=%.4f\n",
			area_base*base2local*base2local,
			area_top*base2local*base2local,
			area_side*base2local*base2local);
	(void)printf("Total Surface Area=%.4f    Volume=%.4f\n",
			(area_base+area_top+area_side)*base2local*base2local,
			vol*base2local*base2local*base2local);

	return;

}



/*	anaylze ars */
static void
ars_anal()
{
(void)printf("ARS analyze not implimented\n");
}


/*
 *  		M A T H E R R
 *  
 *  Sys-V math-library error catcher.
 *  Some callers of acos trip over DOMAIN errors all the time, so...
 */
matherr()
{
	return(1);	/* be quiet */
}
