/*                     C H A N _ M U L T . C
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file chan_mult.c
 *  Multiply specified columns of data by a given factor.
 *
 *  Author -
 *	Carl J. Nuzman
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
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
