/*
	@(#) cgarbs.c			retrieved: 8/13/86 at 08:00:50,
	@(#) version 2.5		last edit: 7/10/86 at 11:07:42., G S Moss.

	Written by Keith Applin.
 */
#include <stdio.h>
#include "./vextern.h"

/* C G A R B S :   determines COMGEOM arb types from GED general arbs
 */
cgarbs( rec, uvec, svec )
register Record *rec;
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

	/* put comgeom solid typpe into s_cgtype */
	switch( numuvec ) {
	case 8:
		rec->s.s_cgtype = -8;  /* ARB8 */
		break;
	case 6:
		rec->s.s_cgtype = -7;	/* ARB7 */
		break;
	case 4:
		if(svec[0] == 2)	rec->s.s_cgtype = -6;	/* ARB6 */
		else	rec->s.s_cgtype = -5;	/* ARB5 */
		break;
	case 2:
		rec->s.s_cgtype = -4;	/* ARB4 */
		break;
	default:
		(void) fprintf( stderr,
		    "solid: %s  bad number of unique vectors (%d)\n",
		    rec->s.s_name,numuvec
		    );
		return(0);
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

	cgtype = rec->s.s_cgtype * -1;
	switch( cgtype ) {
	case ARB8:
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
			(void) fprintf( stderr, "%s: bad arb7\n", rec->s.s_name );
			return( 0 );
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
			else	move(rec,0,1,2,3,4,4,6,6);
			break;
		case 1680:	/* 4567 unique */
			/* 0=3 and 1=2  OR  0=1 and 2=3 */
			if(svec[3] == 3)	move(rec,7,4,5,6,0,0,1,1);
			else	move(rec,4,5,6,7,0,0,2,2);
			break;
		case 160:	/* 0473 unique */
			/* 1=2 and 5=6  OR  1=5 and 2=6 */
			if(svec[3] == 2)	move(rec,0,3,7,4,1,1,5,5);
			else	move(rec,4,0,3,7,1,1,2,2);
			break;
		case 672:	/* 3267 unique */
			/* 0=1 and 4=5  OR  0=4 and 1=5 */
			if(svec[3] == 1)	move(rec,3,2,6,7,0,0,4,4);
			else	move(rec,7,3,2,6,0,0,1,1);
			break;
		case 252:	/* 1256 unique */
			/* 0=3 and 4=7  OR 0=4 and 3=7 */
			if(svec[3] == 3)	move(rec,1,2,6,5,0,0,4,4);
			else	move(rec,5,1,2,6,0,0,3,3);
			break;
		case 60:	/* 0154 unique */
			/* 2=3 and 6=7  OR  2=6 and 3=7 */
			if(svec[3] == 3)	move(rec,0,1,5,4,2,2,6,6);
			else	move(rec,5,1,0,4,2,2,3,3);
			break;
		default:
			(void) fprintf( stderr,"%s: bad arb6\n", rec->s.s_name);
			return( 0 );
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
			(void) fprintf( stderr,"%s: bad arb5\n", rec->s.s_name);
			return( 0 );
		}
		break;
		/* end of ARB5 case */
	case ARB4:	/* arb4 vectors:  0 1 2 0 4 4 4 4 */
		j = svec[6];
		if( svec[0] == 2 )	j = svec[4];
		move(rec,uvec[0],uvec[1],svec[2],uvec[0],j,j,j,j);
		break;
	default:
		(void) fprintf( stderr,
		    "solid %s: unknown arb type (%d)\n",
		    rec->s.s_name,rec->s.s_cgtype
		    );
		return( 0 );
	}
	return( 1 );
}

move( rec, p0, p1, p2, p3, p4, p5, p6, p7 )
register Record	*rec;
int		p0, p1, p2, p3, p4, p5, p6, p7;
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

	/* Copy of the record.						*/
	for( i = 0; i <= 21; i += 3 )
	{
		VMOVE( &copy[i], &(rec->s.s_values[i]) );
	}
	for( i = 0; i < 8; i++ )
	{
		j = pts[i];
		VMOVE( &(rec->s.s_values[i*3]), &copy[j] );
	}
	return;
}

compar( x, y )
register float *x, *y;
{	
	register int i;
	for( i = 0; i < 3; i++ )
	{
		if( fabs( *x++ - *y++ ) > CONV_EPSILON )
			return	0;   /* Different */
	}
	return	1;  /* Same */
}
