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

static void	anal_face();
static void	anal_edge();

/*	Analyze command - prints loads of info about an arb
 *	Format:	analyze [name]
 *		if 'name' is missing using arb being edited
 */

static union record temp_rec;		/* local copy of es_rec */

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
			if(es_gentype != GENARB8) {
				(void)printf("Analyze: solid must be an arb\n");
				return;
			}
			temp_rec = es_rec;
			/* just to make sure */
			mat_idn( modelchanges );
			break;

		case ST_O_EDIT:
			if(illump->s_Eflag) {
				(void)printf("Analyze: cannot analyze evaluated region\n");
				return;
			}
			temp_rec = es_rec;
			break;

		default:
			state_err( "Default ARB Analyze" );
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

		if(temp_rec.u_id != ID_SOLID || 
		   temp_rec.s.s_type != GENARB8) {
			(void)printf("%s: not a solid or not an arb\n",cmd_args[1]);
			return;
		}
	}

	/* got the arb - convert to point notation */
	for(i=3; i<24; i+=3) {
		VADD2( &temp_rec.s.s_values[i], &temp_rec.s.s_values[i], temp_rec.s.s_values );
	}

	/* analyze each face */
	(void)printf("\n----------------------------------------------------------------------\n");
	(void)printf("| FACE |   ROT     FB  |        PLANE EQUATION        | SURFACE AREA |\n");
	(void)printf("|------|---------------|------------------------------|--------------|\n");
	for(i=0; i<6; i++) 
		anal_face( i );

	(void)printf("----------------------------------------------------------------------\n");

	/* analyze each edge */
	(void)printf("  | EDGE     LEN  | EDGE     LEN  | EDGE     LEN  | EDGE     LEN  |\n");
	(void)printf("  |---------------|---------------|---------------|---------------|\n");
	for(i=0; i<12; i++)
		anal_edge( i );

	(void)printf("  -----------------------------------------------------------------\n");
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
		len[i] = MAGNITUDE( v_temp ) * base2local;
	}
	len[4] = len[2];
	j = nface[face][0];
	for(i=2; i<4; i++) {
		k = nface[face][i];
		VSUB2(v_temp, &temp_rec.s.s_values[k*3], &temp_rec.s.s_values[j*3]);
		len[((i*2)-1)] = MAGNITUDE( v_temp ) * base2local;
	}
	len[2] = len[3];

	for(i=0; i<2; i++) {
		j = i*3;
		temp = .5 * (len[j] + len[j+1] + len[j+2]);
		area[i] = sqrt(temp * (temp - len[j]) * (temp - len[j+1]) * (temp - len[j+2]));
	}

	(void)printf("| %d%d%d%d |",a+1,b+1,c+1,d+1);
	(void)printf(" %6.2f %6.2f | %5.2f %5.2f %5.2f %10.2f |",angles[3],angles[4],
			es_plant[0],es_plant[1],es_plant[2],es_plant[3]*base2local);
	(void)printf("   %9.3f  |\n",(area[0]+area[1])*base2local);
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
	(void)printf("  |  %d%d   %6.2f",a+1,b+1,MAGNITUDE(v_temp)*base2local);

	if( ++edge%4 == 0 )
		(void)printf("  |\n");

}


