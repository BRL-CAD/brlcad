/*			C H A N _ M U L T . C
 *
 *  Multiply specified columns of data by a given factor.
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
 *      This software is Copyright (C) 1993-2004 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	double factor, temp;
	int i,j,doit, of, count, val, *col_list;

	if (argc < 4){
		fprintf(stderr,"Usage: chan_mult factor num_columnss column [col ... ] < in.file > out.file\n");
		return(-1);
	}

	sscanf(*(argv+1),"%lf",&factor);
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
				printf("%.10g\t",temp*factor);
			else
				printf("%.10g\t",temp);
		}
		if ( count == (of-1))
			printf("\n");
		count = (count+1)%of;
	}
	return( 0 );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
