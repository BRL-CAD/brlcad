/*	Front end to patch.
 *	John R. Anderson
 *		Based on rpatch.f by Bill Mermagen Jr. 
 *     This pre-processor program alters the data file format
 *     for use by the main conversion program.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define	MAXLINELEN	256

main()
{
	char line[MAXLINELEN];
	char str[MAXLINELEN];
	float x,y,z,hold,work;
	char minus;
	int ity,ity1,ico,isq[8],m,n,k,cc,tmp;
	int i;

	while( gets(line) )
	{
		strncpy( str , line , 8 );
		str[8] = '\0';
		x = atof( str );

		strncpy( str , &line[8] , 8 );
		str[8] = '\0';
		y = atof( str );

		strncpy( str , &line[16] , 9 );
		str[9] = '\0';
		z = atof( str );

		strncpy( str , &line[25] , 6 );
		str[6] = '\0';
		tmp = atoi( str );

		strncpy( str , &line[31] , 4 );
		str[4] = '\0';
		cc = atoi( str );

		strncpy( str , &line[35] , 11 );
		str[11] = '\0';
		isq[0] = atoi( str );

		for( i=1 ; i<8 ; i++ )
		{
			strncpy( str , &line[46 + (i-1)*4] , 4 );
			str[4] = '\0';
			isq[i] = atoi( str );
		}

		strncpy( str , &line[74] , 3 );
		str[3] = '\0';
		m = atoi( str );

		strncpy( str , &line[77] , 3 );
		str[3] = '\0';
		n = atoi( str );

		/* get plate mode flag */
		minus = '+';
		if( tmp < 0 )
		{
			tmp = (-tmp);
			minus = '-';
		}

		/* get solid type */
		hold = (float)tmp/10000.0;
		work = hold * 10.0;
		ity = work;
		hold = work - ity;
		if( ity == 4 )
			ity = 8;

		/* get thickness */
		work = hold * 100.0;
		ico = work;
		hold = work - ico;

		/* get space code */
		work = hold * 10.0;
		ity1 = work;
		hold = work - ity1;

		/* write output */
		printf( "%8.3f %8.3f %9.3f %c %2d %2d %1d %4d %11d ",
			x,y,z,minus,ity,ico,ity1,cc,isq[0] );
		for( i=1 ; i<8 ; i++ )
			printf( "%5d" , isq[i] );
		printf( " %3d %3d\n" , m , n );

	}
}
