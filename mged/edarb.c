/*
 *			E D A R B . C
 *
 * Functions -
 *	redraw		redraw a single solid, given matrix and record.
 *	init_sedit	set up for a Solid Edit
 *	sedit		Apply Solid Edit transformation(s)
 *	findang		Given a normal vector, find rotation & fallback angles
 *	pr_solid	Print a description of a solid
 *	plane
 *
 */

#include	<math.h>
#include	<string.h>
#include "ged_types.h"
#include "3d.h"
#include "sedit.h"
#include "vmath.h"
#include "ged.h"
#include "solid.h"
#include "dir.h"
#include "dm.h"
#include "menu.h"

extern int	printf();

static void	findsam();
static int	compar(), planeqn(), special();


/*
 *  			E D I T A R B
 *  
 *  An ARB edge is moved by finding the slope of the
 *  line containing the edge, moving this line to the
 *  desired location, and intersecting the new line
 *  with the 2 bounding planes to define the new edge.
 *
 *  If es_menu = (a*10)+b then moving edge ab.
 */
editarb( pos_model )
vect_t pos_model;
{
	register float *op;
	register float *opp;
	static double t;
	register int i;
	static int j, k;
	static int pt1, pt2;

	/* convert to point notation (in place ----- DANGEROUS) */
	for(i=3; i<=21; i+=3) {
		op = &es_rec.s.s_values[i];
		VADD2( op, op, &es_rec.s.s_values[0] );
	}

	if( newedge > 0 ) {
		newedge = 0;
		if( do_new_edge() < 0 )
			goto err;
	}

	/* Now we have line and planes data - move the edge */

	pt1 = (es_menu / 10) - 1;
	pt2 = (es_menu % 10) - 1;

	/* 
	 * Line containing edge must now go throug
	 * point contained in pos_model[] array
	 */
	t = (es_plano[3] - VDOT(es_plano, pos_model)) / VDOT(es_plano, es_m);
	op = &es_rec.s.s_values[pt1*3];
	VCOMP1( op, pos_model, t, es_m );

	t = (es_plant[3] - VDOT(es_plant, pos_model)) / VDOT(es_plant, es_m);
	op = &es_rec.s.s_values[pt2*3];
	VCOMP1( op, pos_model, t, es_m );

	/* move along any like points */
	k = pt1;
	for(i=0; i<6; i++) {
		if(i >= 3)
			k = pt2;
		if( (j = es_same[i]) > -1 ) {
			op = &es_rec.s.s_values[j*3];
			opp = &es_rec.s.s_values[k*3];
			VMOVE(op, opp);
		}
	}

	/* back to vector notation */
	for(i=3; i<=21; i+=3) {
		opp = &es_rec.s.s_values[i];
		VSUB2( opp, opp, &es_rec.s.s_values[0] );
	}
	return(0);		/* OK */

err:
	/* Error handling */
	(void)printf("cannot move edge: %d\n", es_menu);
	es_edflag = IDLE;

	/* back to vector notation */
	for(i=3; i<=21; i+=3) {
		opp = &es_rec.s.s_values[i];
		VSUB2(opp, opp, &es_rec.s.s_values[0]);
	}
	return(1);		/* BAD */
}

/*
 *  			D O _ N E W _ E D G E
 *  
 *  Internal routine for editarb(), called whenever a new
 *  edge has been selected for editing.
 *  
 *  Find the line equation, and the equations of the bounding planes.
 */
do_new_edge()
{
	register float *op;
	register float *opp;
	static int i, j;
	static int pt1,pt2;

	/* find line data */
	pt1 = (es_menu / 10) - 1;
	pt2 = (es_menu % 10) - 1;

	for(i=0; i<6; i++)
		es_same[i] = -1;

	/* find any equal points to move along with the edge */
	findsam(0, pt1, &es_rec.s);
	findsam(3, pt2, &es_rec.s);
	pt1*=3;
	pt2*=3;

	/* find slope of line containing edge pt1, pt2 */
	op = &es_rec.s.s_values[pt2];
	opp = &es_rec.s.s_values[pt1];
	VSUB2(es_m, op, opp);
	if(MAGNITUDE(es_m) == 0.0)
		return(-1);

	/* find bounding planes and equations */
	/* put equation coefficients in plano[] and plant[] arrays */
	if( es_menu == 15 ||
	    es_menu == 26 ||
	    es_menu == 37 ||
	    es_menu == 48) {
		/*
		 * plane() returns:
		 *  -1  if successful
		 *   0  if unsuccessful
		 *  >0  if degenerate plane (special case)
		 */
		if( (j = plane(0, 1, 2, 3, &es_rec.s)) > 0 ) { 
			/* special case */
			i = 2;
			if( j == 3 )
				i = 1;
			/* try to find a substitute
			   plane for the degenerate plane */
			if((j = special(i, pt1, &es_rec.s))>0)
				return(-1);
		}
		if(j == 0)
			return(-1);

		VMOVE(&es_plano[0], &es_plant[0]);
		es_plano[3] = es_plant[3];

		if( (j = plane(4, 5, 6, 7, &es_rec.s)) > 0 ) {
			i = 2;
			if(j == 47)
				i = 1;
			if( (j = special(i, pt1, &es_rec.s)) > 0)
				return(-1);
		}
		if(j == 0)
			return(-1);
	} else if(
	    es_menu == 12 ||
	    es_menu == 56 ||
	    es_menu == 87 ||
	    es_menu == 43
	) {
		if( (j = plane(0, 3, 7, 4, &es_rec.s)) > 0 ) {
			i = 3;
			if(j == 3)
				i = 1;
			if( (j =special(i, pt1, &es_rec.s)) > 0 )
				return(-1);
		}
		if(j == 0)
			return(-1);

		VMOVE(&es_plano[0], &es_plant[0]);
		es_plano[3] = es_plant[3];

		if( (j = plane(1, 2, 6, 5, &es_rec.s)) > 0 ) {
			i = 3;
			if(j == 12)
				i = 1;
			if( (j = special(i, pt1, &es_rec.s)) > 0 ) {
				return(-1);
			}
		}
		if(j == 0)
			return(-1);
	} else {
		if( (j = plane(0, 1, 5, 4, &es_rec.s)) > 0 ) {
			i = 3;
			if(j == 1)
				i = 2;
			if( (j = special(i, pt1, &es_rec.s)) > 0)
				return(-1);
		}
		if(j == 0)
			return(-1);

		VMOVE(&es_plano[0], &es_plant[0]);
		es_plano[3] = es_plant[3];

		if( (j = plane(2, 3, 7, 6, &es_rec.s)) > 0 ) {
			i = 3;
			if(j == 23)
				i = 2;
			if( (j = special(i, pt1, &es_rec.s)) > 0 ) {
				return(-1);
			}
		}
		if(j == 0)
			return(-1);
	}
	return(0);		/* OK */
}

/*
 * 			P L A N E
 * 
 * Attempts to find equation of plane abcd
 *
 *   returns  >0 if plane abcd is degenerate
 *	    -1 if equation of plane abcd is found
 *	     0 if cannot find equation of plane abcd
 *		  yet plane is NOT degenerate
 */
int
plane( a, b, c, d, sp )
struct solidrec *sp;
int a,b,c,d;
{

	/* check for special case (degenerate plane) */
	if(compar(&sp->s_values[a*3],&sp->s_values[b*3]) && 
	   compar(&sp->s_values[c*3],&sp->s_values[d*3])) 
		return((a*10 + b));
	if(compar(&sp->s_values[a*3],&sp->s_values[c*3]) && 
	   compar(&sp->s_values[b*3],&sp->s_values[d*3])) 
		return((a*10 + c));
	if(compar(&sp->s_values[a*3],&sp->s_values[d*3]) &&
	   compar(&sp->s_values[b*3],&sp->s_values[c*3])) 
		return((a*10 + d));

	/* find equation of plane abcd */
	if( planeqn(a,b,c,sp) )
		return(-1);
	if( planeqn(a,b,d,sp) )
		return(-1);
	if( planeqn(a,c,d,sp) )
		return(-1);
	if( planeqn(b,c,d,sp) )
		return(-1);
	return(0);
}

/*
 *  			S P E C I A L
 */
static int
special( a , b, sp )
struct solidrec *sp;
int a,b;
{
	b /= 3;
	switch( a ) {

	case 1:
		if( b == 0 || b == 1 || b == 4 )
			return( plane(2,3,6,7,sp) );
		else
			return( plane(0,1,5,4,sp) );

	case 2:
		if( b == 1 || b == 2 || b == 5 )
			return( plane(0,4,3,7,sp) );
		else
			return( plane(1,2,6,5,sp) );

	case 3:
		if( b == 4 || b == 5 || b == 7 )
			return( plane(0,1,2,3,sp) );
		else
			return( plane(4,5,6,7,sp) );

	default:
		(void)printf("subroutine special: edge input error\n");
		return(0);
	}
	/*NOTREACHED*/
}


static int
compar( x, y )
float *x,*y;
{

	int i;

	for(i=0; i<3; i++) {
		if( *x++ != *y++ )
			return(0);   /* different */
	}
	return(1);  /* same */
}


static void
findsam( flag, pt, sp )
int flag,pt;
struct solidrec *sp;
{
	int i;

	for(i=0; i<8; i++) {
		if( (compar(&sp->s_values[i*3],&sp->s_values[pt*3])) &&
								(i != pt) ) 
			es_same[flag++] = i;
	}
}


static int
planeqn( a, b, c, sp )
int a,b,c;
struct solidrec *sp;
{
	int i;
	float work[3],worc[3],mag;

	if( compar(&sp->s_values[a*3],&sp->s_values[b*3]) )
		return(0);
	if( compar(&sp->s_values[b*3],&sp->s_values[c*3]) )
		return(0);
	if( compar(&sp->s_values[a*3],&sp->s_values[c*3]) )
		return(0);
	VSUB2(work,&sp->s_values[b*3],&sp->s_values[a*3]);
	VSUB2(worc,&sp->s_values[c*3],&sp->s_values[a*3]);
	VCROSS(es_plant,work,worc);
	if( (mag = MAGNITUDE(es_plant)) == 0.0 )  
		return(0);
	VSCALE(es_plant,es_plant,1.0/mag);
	i = a*3;
	es_plant[3] = VDOT(es_plant,&sp->s_values[i]);
	return(1);
}
