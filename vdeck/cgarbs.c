/*
 *	@(#) cgarbs.c			retrieved 8/13/86 at 07:59:34,
 *	@(#) version 1.5		  created 7/19/83 at 10:58:32.
 *
 *	Written by Keith Applin.
 *	All rights reserved, Ballistic Research Laboratory.
 */
#include "ged_types.h"
#include "3d.h"
#include "./deck.h"
extern double	fabs();

/* C G A R B S :   determines COMGEOM arb types from GED general arbs
 */
cgarbs( rec, uvec, svec )
register
Record *rec;
int uvec[];	/* array of unique points */
int svec[];	/* array of like points */
{
	register int	i, j;
	int		numuvec, unique, done;
	int		si;

	done = NO;		/* done checking for like vectors */

	svec[0] = svec[1] = 0;
	si = 2;

	for(i=0; i<7; i++) {
		unique = YES;
		if(done == NO)
			svec[si] = i;
		for(j=i+1; j<8; j++) {
			if(	compar(	&(rec->s.s_values[i*3]),
					&(rec->s.s_values[j*3])
				) == YES
			) {
				if( done == NO ) svec[++si] = j;
				unique = NO;
			}
		}
		if( unique == NO ) {  	/* point i not unique */
			if( si > 2 && si < 6 ) {
				svec[0] = si - 1;
				if(si == 5 && svec[5] >= 6)
					done = YES;
				si = 6;
			}
			if( si > 6 ) {
				svec[1] = si - 5;
				done = YES;
			}
		}
	}
	if( si > 2 && si < 6 ) 
		svec[0] = si - 1;
	if( si > 6 )
		svec[1] = si - 5;
	for(i=1; i<=svec[1]; i++)
		svec[svec[0]+1+i] = svec[5+i];
	for(i=svec[0]+svec[1]+2; i<11; i++)
		svec[i] = -1;
	/* find the unique points */
	numuvec = 0;
	for(j=0; j<8; j++) {
		unique = YES;
		for(i=2; i<svec[0]+svec[1]+2; i++) {
			if( j == svec[i] ) {
				unique = NO;
				break;
			}
		}
		if( unique == YES )
			uvec[numuvec++] = j;
	}

	/* put comgeom solid typpe into s_num */
	switch( numuvec ) {
	case 8:
		rec->s.s_num = -8;  /* ARB8 */
		break;
	case 6:
		rec->s.s_num = -7;	/* ARB7 */
		break;
	case 4:
		if(svec[0] == 2)	rec->s.s_num = -6;	/* ARB6 */
		else			rec->s.s_num = -5;	/* ARB5 */
		break;
	case 2:
		rec->s.s_num = -4;	/* ARB4 */
		break;
	default:
		printf("solid: %s  bad number of unique vectors (%d)\n",
			rec->s.s_name,numuvec);
		return(0);
		break;
	}
	return( numuvec );
}

/*  R E D O A R B :   rearranges arbs to be GIFT compatible
 */
redoarb( rec, uvec, svec, numvec )
register
Record  *rec;
int uvec[], svec[], numvec;
{
	register int	i, j;
	int		prod, cgtype;
	float		testm;
	float		vec[3];
	int		nleg1, nleg2, nleg3;

	cgtype = rec->s.s_num * -1;
	switch( cgtype ) {
	case ARB8: /* New stuff Mar 29, 1983.				*/
#ifdef 0 /* THIS CODE DOES NOT WORK ====================================*/
	fprintf( stderr,
	"redoarb():ERROR this statement should not be reached!\n" );
	exit( 1 );
		/* Convert to vector notation to check for BOX and RPP.	*/
		vectors( rec );

		/* ARB8 is a BOX if all following true:
		 *   1.  vectors [3],[9],[12] mutually perpendicular
		 *   2.  |[15]-[3]|  == |[12]|
		 *   3.  |[6]-[3]|   == |[9]|
		 *   4.  |[18]-[15]| == |[9]|
		 *   5.  |[21]-[12]| == |[9]|
		 */

		/* Test condition 1.					*/
		if(	fabs(	DOT(	&(rec->s.s_values[3]),
					&(rec->s.s_values[9])
				)) > .0001
		    ||	fabs(	DOT(	&(rec->s.s_values[9]),
					&(rec->s.s_values[12])
				)) > .0001
		) { 
			points( rec );
			break;
		}

		/* Test condition 2.					*/
		testm = MAGNITUDE( &(rec->s.s_values[12]) );
		VSUB2(vec, &(rec->s.s_values[15]), &(rec->s.s_values[3]) );
		if( fabs( MAGNITUDE( vec ) - testm ) > .0001 ) {
			points( rec );
			break;
		}
		testm = MAGNITUDE( &(rec->s.s_values[9]) );

		/* Test condition 3.					*/
		VSUB2( vec, &(rec->s.s_values[6]), &(rec->s.s_values[3]) );
		if( fabs( MAGNITUDE( vec ) - testm ) > .0001 ) {
			points( rec );
			break;
		}

		/* Test condition 4.					*/
		VSUB2(	vec,
			&(rec->s.s_values[18]),
			&(rec->s.s_values[15])
		);
		if( fabs( MAGNITUDE( vec ) - testm ) > .0001 ) {
			points( rec );
			break;
		}

		/* Test condition 5.					*/
		VSUB2(	vec,
			&(rec->s.s_values[21]),
			&(rec->s.s_values[12])
		);
		if( fabs( MAGNITUDE( vec ) - testm ) > .0001 ) {
			points( rec );
			break;
		}

		/* Have a BOX.						*/
		rec->s.s_num = -2;

		/* Check for RPP.					*/
		nleg1 = nleg2 = nleg3 = 0;
		for( i = 0; i < 3; i++ ) {
			if( fabs( rec->s.s_values[3+i] ) < .0001 )
				nleg1++;
			if( fabs( rec->s.s_values[9+i] ) < .0001 )
				nleg2++;
			if( fabs( rec->s.s_values[12+i] ) < .0001 )
				nleg3++;
		}
		if( nleg1 == 2 && nleg2 == 2 && nleg3 == 2 )
			rec->s.s_num = -1;	/* RPP */

		/* Back to point notation.				*/
		points( rec );
#endif
		break;
	case ARB7:	/* arb7 vectors: 0 1 2 3 4 5 6 4 */
		switch( svec[2] ) {
		case 0:			/* 0 = 1, 3, or 4 */
			if(svec[3] == 1)	move(rec,4,7,6,5,1,4,3,1);
			if(svec[3] == 3)	move(rec,4,5,6,7,0,1,2,0);
			if(svec[3] == 4)	move(rec,1,2,6,5,0,3,7,0);
			break;
		case 1:			/* 1 = 2 or 5 */
			if(svec[3] == 2)	move(rec,0,4,7,3,1,5,6,1);
			if(svec[3] == 5)	move(rec,0,3,7,4,1,2,6,1);
			break;
		case 2:			/* 2 = 3 or 6 */
			if(svec[3] == 3)	move(rec,6,5,4,7,2,1,0,2);
			if(svec[3] == 6)	move(rec,3,0,4,7,2,1,5,2);
			break;
		case 3:			/* 3 = 7 */
			move(rec,2,1,5,6,3,0,4,3);
			break;
		case 4:			/* 4 = 5 */
					/* if 4 = 7  do nothing */
			if(svec[3] == 5)	move(rec,1,2,3,0,5,6,7,5);
			break;
		case 5:			/* 5 = 6 */
			move(rec,2,3,0,1,6,7,4,6);
			break;
		case 6:			/* 6 = 7 */
			move(rec,3,0,1,2,7,4,5,7);
			break;
		default:
			printf("%s: bad arb7\n", rec->s.s_name);
			return( 0 );
			break;
		}
		break;  
		/* end of ARB7 case */
	case ARB6:	/* arb6 vectors:  0 1 2 3 4 4 6 6 */
		prod = 1;
		for(i=0; i<numvec; i++)
			prod = prod * (uvec[i] + 1);
		switch( prod ) {
		case 24:	/* 0123 unique */
				/* 4=7 and 5=6  OR  4=5 and 6=7 */
			if(svec[3] == 7)	move(rec,3,0,1,2,4,4,5,5);
			else			move(rec,0,1,2,3,4,4,6,6);
			break;
		case 1680:	/* 4567 unique */
				/* 0=3 and 1=2  OR  0=1 and 2=3 */
			if(svec[3] == 3)	move(rec,7,4,5,6,0,0,1,1);
			else			move(rec,4,5,6,7,0,0,2,2);
			break;
		case 160:	/* 0473 unique */
				/* 1=2 and 5=6  OR  1=5 and 2=6 */
			if(svec[3] == 2)	move(rec,0,3,7,4,1,1,5,5);
			else			move(rec,4,0,3,7,1,1,2,2);
			break;
		case 672:	/* 3267 unique */
				/* 0=1 and 4=5  OR  0=4 and 1=5 */
			if(svec[3] == 1)	move(rec,3,2,6,7,0,0,4,4);
			else			move(rec,7,3,2,6,0,0,1,1);
			break;
		case 252:	/* 1256 unique */
				/* 0=3 and 4=7  OR 0=4 and 3=7 */
			if(svec[3] == 3)	move(rec,1,2,6,5,0,0,4,4);
			else			move(rec,5,1,2,6,0,0,3,3);
			break;
		case 60:	/* 0154 unique */
				/* 2=3 and 6=7  OR  2=6 and 3=7 */
			if(svec[3] == 3)	move(rec,0,1,5,4,2,2,6,6);
			else			move(rec,5,1,0,4,2,2,3,3);
			break;
		default:
			printf("%s: bad arb6\n", rec->s.s_name);
			return( 0 );
			break;
		}
		break;
		/* end of ARB6 case */
	case ARB5:	/* arb5 vectors:  0 1 2 3 4 4 4 4 */
		prod = 1;
		for(i=2; i<6; i++)
			prod = prod * (svec[i] + 1);
		switch( prod ) {
		case 24:	/* 0=1=2=3 */
			move(rec,4,5,6,7,0,0,0,0);
			break;
		case 1680:	/* 4=5=6=7 */
				/* do nothing */
			break;
		case 160:	/* 0=3=4=7 */
			move(rec,1,2,6,5,0,0,0,0);
			break;
		case 672:	/* 2=3=7=6 */
			move(rec,0,1,5,4,2,2,2,2);
			break;
		case 252:	/* 1=2=5=6 */
			move(rec,0,3,7,4,1,1,1,1);
			break;
		case 60:	/* 0=1=5=4 */
			move(rec,3,2,6,7,0,0,0,0);
			break;
		default:
			printf("%s: bad arb5\n", rec->s.s_name);
			return( 0 );
			break;
		}
		break;
		/* end of ARB5 case */
	case ARB4:	/* arb4 vectors:  0 1 2 0 4 4 4 4 */
		j = svec[6];
		if( svec[0] == 2 )	j = svec[4];
		move(rec,uvec[0],uvec[1],svec[2],uvec[0],j,j,j,j);
		break;
	default:
		printf("solid %s: unknown arb type (%d)\n",
				rec->s.s_name,rec->s.s_num);
		return( 0 );
		break;
	}
	return( 1 );
}

move(	rec, p0, p1, p2, p3, p4, p5, p6, p7 )
register
Record *rec;
int p0, p1, p2, p3, p4, p5, p6, p7;
{

	register int	i, j;
	int		pts[8];
	float		copy[24];

	pts[0] = p0 * 3;
	pts[1] = p1 * 3;
	pts[2] = p2 * 3;
	pts[3] = p3 * 3;
	pts[4] = p4 * 3;
	pts[5] = p5 * 3;
	pts[6] = p6 * 3;
	pts[7] = p7 * 3;

	/* Copy of the record.				*/
	for( i = 0; i <= 21; i += 3 ) {
		VMOVE( &copy[i], &(rec->s.s_values[i]) );
	}
	for( i = 0; i < 8; i++ ) {
		j = pts[i];
		VMOVE( &(rec->s.s_values[i*3]), &copy[j] );
	}
	return;
}

compar( x, y )
register
float *x,*y;
{
	register int i;
	for( i = 0; i < 3; i++ ) {
		if( fabs( *x++ - *y++ ) > .0001 )
			return( 0 );   /* Different */
	}
	return( 1 );  /* Same */
}

vectors(rec)
register
Record *rec;
{
	register int	i;

	for( i = 3; i <= 21; i += 3 ) {
		VSUB2(	&(rec->s.s_values[i]),
			&(rec->s.s_values[i]),
			&(rec->s.s_values[0])
		);
	}
	return;
}

points( rec )
register
Record *rec;
{
	register int	i;

	for( i = 3; i <= 21; i += 3 ) {
		VADD2(	&(rec->s.s_values[i]),
			&(rec->s.s_values[i]),
			&(rec->s.s_values[0])
		);
	}
	return;
}
