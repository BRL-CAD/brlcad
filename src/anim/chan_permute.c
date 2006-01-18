/*                  C H A N _ P E R M U T E . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
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
/** @file chan_permute.c
 *	This program mixes, matches, and merges files organized in
 *  channels or columns.
 *	It's nothing cut and paste can't do, but sometimes it's nice to
 *  be able to do it in one step, especially when working with animation
 *  tables.
 *Usage: channel -i infile1 id id id ... [-i infile2 ...] -o outfile1 id id ...
 *		[-o outfile2 ...]
 *where infiles are files to be read from, outfiles are files to be written
 *to, and each id is a small positive integer identifying a channel. All of the
 *input id's should be distinct integers, or the results are not guaranteed.
 *
 *  Author -
 *	Carl J. Nuzman
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#include "common.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAXLEN 40

typedef char Word[MAXLEN];

struct unit {
    FILE *file;
    int channels;
    short *list;
    unsigned i_o;	/*i=1 o=0*/
};


char ihead[] = "-i";
char ohead[] = "-o";

int
main(int argc, char **argv)
{
    int i,j, maxlength,num_done;
    int icount, ocount;
    struct unit *x, *y;
    Word *arrayd;

    i=j=icount = ocount = maxlength = 0;
    for(i=1;i<argc;i++){
	if( !strncmp(argv[i],ihead,2) ){
	    j=0;
	    icount++;
	}
	else if( !strncmp(argv[i],ohead,2) ){
	    j=0;
	    ocount++;
	}
	else
	    maxlength = (++j>maxlength) ? j : maxlength;
    }

    y = (struct unit *) calloc(icount+ocount,sizeof(struct unit));
    x = y - 1;
    for(i=1;i<argc;i++){
	if( !strncmp(argv[i],"-",1) ){
	    j=0;
	    x++;
	    x->list = (short *) calloc(maxlength,sizeof(short));
	    if (argv[i][1] == 'i'){
		i++;
		(x)->i_o = 1;
		if ( ! strcmp(argv[i],"stdin") )
		    x->file = stdin;
		else if ( !(x->file = fopen(argv[i],"r")) )
		    fprintf(stderr,"Channel: can't open %s\n",argv[i]);
	    }
	    else if (argv[i][1] == 'o'){
		i++;
		(x)->i_o = 0;
		if ( ! strcmp(argv[i],"stdout") )
		    x->file = stdout;
		else if ( !(x->file = fopen(argv[i],"w")) )
		    fprintf(stderr,"Channel: can't write to %s\n",argv[i]);
	    }
	    else{
		fprintf(stderr,"Illegal option %c\n",argv[i][1]);
		exit(-1);
	    }
	}
	else{
	    sscanf(argv[i],"%hd",x->list+(j++));
	    x->channels++;
	}
    }
    arrayd = (Word *) calloc(argc,sizeof(Word));/*may use more memory than absolutely necessary*/
    num_done = 0;
    while(num_done < icount ){ /* go until all in files are done */
	num_done = 0;
	for (x=y;x<y+ocount+icount;x++){ /* do one line */
	    if(num_done >= icount)
		;/*chill - all in files done */
	    else if (x->i_o == 1){
		if(feof(x->file))
		    num_done += 1;
		else
		    for(j=0;j<x->channels;j++)
			fscanf(x->file,"%s ",arrayd[x->list[j]]);
	    }
	    else if (x->i_o == 0){
		for(j=0;j<x->channels;j++)
		    fprintf(x->file,"%s\t",arrayd[x->list[j]]);
		fprintf(x->file,"\n");
	    }
	}
    }
    free(arrayd);
    for (x=y;x<y+ocount+icount;x++){
	free(x->list);
    }
    free(y);
    exit(0);
}

int max(int *m, int n) /*return greatest of n integers, unless one is greater than n*/

{
    int i,j;
    j = 0;
    for (i=0;i<n;i++){
	j = (m[i]>j) ? m[i] : j;
    }
    return( (j>n) ? 0 : j );
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
