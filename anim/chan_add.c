/*			C H A N _ A D D . C
 *
 *  Add a given value to specified columns of data.
 *
 *  Author -
 *	Carl J. Nuzman
 *  
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1993 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int
main(argc,argv)
int argc;
char **argv;
{
	double addend, temp;
	int i,j,doit, of, count, val, *col_list;


	if (argc < 3) {
		fprintf(stderr,"Usage: chan_add value num_columns column [column ...] < in.file > out.file\n");
		return(-1);
	}

	sscanf(*(argv+1),"%lf",&addend);
	sscanf(*(argv+2),"%d",&of);
	col_list = (int *) calloc(argc-2,sizeof(int));
	for (i=3;i<argc;i++){
		sscanf(*(argv+i),"%d",col_list+(i-3));
	}

	count = 0;
	while (!feof(stdin)){
		val = scanf("%lf",&temp);
		if (val<1) 
			;
		else {
			doit = 0;
			for (j=0;j<argc-3;j++){
				if (col_list[j]==count)
					doit = 1;
			}
			if (doit)
				printf("%.10g\t",temp + addend);
			else
				printf("%.10g\t",temp);
		}
		if ( count == (of-1))
			printf("\n");
		count = (count+1)%of;
	}
	return( 0 );
}
