#
/*
 *			C O N V E R T . C
 *
 *	Conversion subroutine for CVT program, used to convert
 * COMGEOM card decks into GED object files.  This conversion routine
 * is used to translate between COMGEOM object types, and the
 * more general GED object types.
 *
 * Michael Muuss
 * Ballistic Research Laboratory
 * March, 1980
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	Feb, 1981 Chuck Stanley	Added code to process TORUS (TOR)
 *
 *	02/26/81  MJM	Fixed bug in TORUS code...(PI,PI,PI) was being
 *			added to F1, and put in the file.
 */

#include <stdio.h>
#include <math.h>
#include "ged_types.h"
#include "3d.h"

 double sqrt();

#define PI	3.14159265358979323846264	/* Approx */

#define Xmin	iv[0]
#define Xmax	iv[1]
#define Ymin	iv[2]
#define Ymax	iv[3]
#define Zmin	iv[4]
#define Zmax	iv[5]

/*
 * Input Vector Fields
 */
#define Fi	iv+(i-1)*3
#define F1	iv+(1-1)*3
#define F2	iv+(2-1)*3
#define F3	iv+(3-1)*3
#define F4	iv+(4-1)*3
#define F5	iv+(5-1)*3
#define F6	iv+(6-1)*3
#define F7	iv+(7-1)*3
#define F8	iv+(8-1)*3
/*
 * Output vector fields
 */
#define Oi	ov+(i-1)*3
#define O1	ov+(1-1)*3
#define O2	ov+(2-1)*3
#define O3	ov+(3-1)*3
#define O4	ov+(4-1)*3
#define O5	ov+(5-1)*3
#define O6	ov+(6-1)*3
#define O7	ov+(7-1)*3
#define O8	ov+(8-1)*3
#define O9	ov+(9-1)*3
#define O10	ov+(10-1)*3
#define O11	ov+(11-1)*3
#define O12	ov+(12-1)*3
#define O13	ov+(13-1)*3
#define O14	ov+(14-1)*3
#define O15	ov+(15-1)*3
#define O16	ov+(16-1)*3

convert( in )
struct solids *in;
{
	static struct solids out;
	register float *iv;
	register float *ov;
	static float r1, r2;
	static float work[3];
	static float m1, m2, m3, m4;	/* Magnitude temporaries */
	static float m5, m6;	/* TOR temporaries */
	static float r3,r4; /* magnitude temporaries */
	static int i;

	iv = &in->s_values[0];
	ov = &out.s_values[0];

	switch( in->s_type )  {

	case RPP:
		VSET( O1, Xmax, Ymin, Zmin );
		VSET( O2, Xmax, Ymax, Zmin );
		VSET( O3, Xmax, Ymax, Zmax );
		VSET( O4, Xmax, Ymin, Zmax );
		VSET( O5, Xmin, Ymin, Zmin );
		VSET( O6, Xmin, Ymax, Zmin );
		VSET( O7, Xmin, Ymax, Zmax );
		VSET( O8, Xmin, Ymin, Zmax );
		goto ccommon;

	case BOX:
		VMOVE( O1, F1 );
		VADD2( O2, F1, F3 );
		VADD3( O3, F1, F3, F2 );
		VADD2( O4, F1, F2 );
		VADD2( O5, F1, F4 );
		VADD3( O6, F1, F4, F3 );
		VADD4( O7, F1, F4, F3, F2 );
		VADD3( O8, F1, F4, F2 );
		goto ccommon;

	case RAW:
		VMOVE( O1, F1 );
		VADD2( O2, F1, F3 );
		VMOVE( O3, O2 );
		VADD2( O4, F1, F2 );
		VADD2( O5, F1, F4 );
		VADD3( O6, F1, F4, F3 );
		VMOVE( O7, O6 );
		VADD3( O8, F1, F4, F2 );
	ccommon:
		VMOVE( F1, O1 );
		for( i=2; i<=8; i++ )  {
			VSUB2( Fi, Oi, O1 );
		}
		in->s_type = GENARB8;
		return;

	case ARB8:
	arb8common:
		for(i=2; i<=8; i++)  {
			VSUB2( Fi, Fi, F1 );
		}
		in->s_type = GENARB8;
		return;

	case ARB7:
		VMOVE( F8, F5 );
		in->s_type = ARB8;
		goto arb8common;

	case ARB6:
		/* Note that the ordering is important, as data is in F5, F6 */
		VMOVE( F8, F6 );
		VMOVE( F7, F6 );
		VMOVE( F6, F5 );
		in->s_type = ARB8;
		goto arb8common;

	case ARB5:
		VMOVE( F6, F5 );
		VMOVE( F7, F5 );
		VMOVE( F8, F5 );
		in->s_type = ARB8;
		goto arb8common;

	case ARB4:
		/* Order is important, data is in F4 */
		VMOVE( F8, F4 );
		VMOVE( F7, F4 );
		VMOVE( F6, F4 );
		VMOVE( F5, F4 );
		VMOVE( F4, F1 );
		in->s_type = ARB8;
		goto arb8common;

	case RCC:
		r1 = iv[6];	/* R */
		r2 = iv[6];
		goto trccommon;		/* sorry */

	case REC:
		VMOVE( F5, F3 );
		VMOVE( F6, F4 );
		in->s_type = GENTGC;
		return;

		/*
		 * For the TRC, if the V vector (F1) is of zero length,
		 * a divide by zero will occur when scaling by the magnitude.
		 * We add the vector [PI, PI, PI] to V to produce a unique
		 * (and most likely non-zero) resultant vector.  This will
		 * do nicely for purposes of cross-product.
		 */

	case TRC:
		r1 = iv[6];	/* R1 */
		r2 = iv[7];	/* R2 */
	trccommon:
		VMOVE( work, F1 );
		work[0] += PI;
		work[1] += PI;
		work[2] += PI;
		VCROSS( F3, work, F2 );
		m1 = MAGNITUDE( F3 );
		VSCALE( F3, F3, r1/m1 );

		VCROSS( F4, F2, F3 );
		m2 = MAGNITUDE( F4 );
		VSCALE( F4, F4, r1/m2 );

		VSCALE( F5, F3, r2/r1 );
		VSCALE( F6, F4, r2/r1 );
		in->s_type = GENTGC;
		return;

	case TEC:
		r1 = iv[12];	/* P */
		VSCALE( F5, F3, (1.0/r1) );
		VSCALE( F6, F4, (1.0/r1) );
		in->s_type = GENTGC;
		return;

	case TGC:
		r1 = iv[12] / MAGNITUDE( F3 );	/* A/|A| * C */
		r2 = iv[13] / MAGNITUDE( F4 );	/* B/|B| * D */
		VSCALE( F5, F3, r1 );
		VSCALE( F6, F4, r2 );
		in->s_type = GENTGC;
		return;

	case SPH:
		r1 = iv[3];		/* R */
		VSET( F2, r1, 0, 0 );
		VSET( F3, 0, r1, 0 );
		VSET( F4, 0, 0, r1 );
		in->s_type = GENELL;
		return;

	case ELL:
		/*
		 * For simplicity, we convert ELL to ELL1, then
		 * fall through to ELL1 code.  Format of ELL is
		 * F1, F2, l
		 * ELL1 format is V, A, r
		 */
		r1 = iv[6];		/* length */
		VADD2( O1, F1, F2 );
		VSCALE( O1, O1, 0.5 );	/* O1 holds V */

		VSUB2( O3, F2, F1 );	/* O3 holds F2 -  F1 */
		m1 = MAGNITUDE( O3 );
		r2 = 0.5 * r1 / m1;
		VSCALE( O2, O3, r2 );	/* O2 holds A */

		iv[6] = sqrt( MAGSQ( O2 ) - (m1 * 0.5)*(m1 * 0.5) );	/* r */
		VMOVE( F1, O1 );	/* Move V */
		VMOVE( F2, O2 );	/* Move A */
		/* fall through */

	case ELL1:
		/* GENELL is V, A, B, C */
		r1 = iv[6];		/* R */
		VMOVE( work, F1 );
		work[0] += PI;
		work[1] += PI;
		work[2] += PI;
		VCROSS( F3, work, F2 );		/* See the TRC comments */
		m1 = MAGNITUDE( F3 );
		VSCALE( F3, F3, r1/m1 );

		VCROSS( F4, F2, F3 );
		m2 = MAGNITUDE( F4 );
		VSCALE( F4, F4, r1/m2 );
		in->s_type = GENELL;
		return;

	case TOR:
		r1=iv[6];	/* Dist from end of V to center of (solid portion) of TORUS */
		r2=iv[7];	/* Radius of solid portion of TORUS */
		r3=r1-r2;	/* Radius to inner circular edge */
		r4=r1+r2;	/* Radius to outer circular edge */

		/*
		 * To allow for V being (0,0,0), for VCROSS purposes only,
		 * we add (PI,PI,PI).  THIS DOES NOT GO OUT INTO THE FILE!!
		 */
		VMOVE(work,F1);
		work[0] +=PI;
		work[1] +=PI;
		work[2] +=PI;

		m2 = MAGNITUDE( F2 );	/* F2 is NORMAL to Torus, with Radius length */
		VSCALE( F2, F2, r2/m2 );

		/* F3, F4 are perpendicular, goto center of Torus (solid part), for top/bottom */
		VCROSS(F3,work,F2);
		m1=MAGNITUDE(F3);
		VSCALE(F3,F3,r1/m1);
 
		VCROSS(F4,F3,F2);
		m3=MAGNITUDE(F4);
		VSCALE(F4,F4,r1/m3);

		m5 = MAGNITUDE(F3);
		m6 = MAGNITUDE( F4 );

		/* F5, F6 are perpindicular, goto inner edge of ellipse */
		VSCALE( F5, F3, r3/m5 );
		VSCALE( F6, F4, r3/m6 );

		/* F7, F8 are perpendicular, goto outer edge of ellipse */
		VSCALE( F7, F3, r4/m5 );
		VSCALE( F8, F4, r4/m6 );
 
		in->s_type=TOR;
		return;

	default:
		return;
	}

	/*
	 * We should never reach here
	 */
	printf("convert:  fell out of loop\n");
}
