/*			A N I M _ S O R T . C
 *
 *	Combine multiple animation scripts on standard input into a 
 *  single script on standard output. The output can be in natural order
 *  or in a scrambled order for incrementally increasing time 
 *  resolution (-i option).
 * 
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

#include "conf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"

#define MAXLEN	50		/*maximum length of lines to be read */
#define MAXLINES 30		/* maximum length of lines to be stored*/

int suppressed;		/* flag: suppress printing of 'clean;' commands */
int incremental;	/* flag: order for incremental time resolution */

main(argc, argv)
int argc;
char **argv;
{
	int	length,frame_number, number, success, maxnum; 
	int 	first_frame,spread,reserve; 
	long	last_pos;
	char	line[MAXLEN];
	char    pbuffer[MAXLEN*MAXLINES];


        if (!get_args(argc,argv))
		fprintf(stderr,"Get_args error\n");

	/* copy any lines preceeding the first "start" command */
	last_pos = ftell(stdin);
	while (fgets(line,MAXLEN,stdin)!=NULL){
		if (strncmp(line,"start",5)){
			printf("%s",line);
			last_pos = ftell(stdin);
		}
		else
			break;
	}

	/* read the frame number of the first "start" command */
	sscanf( strpbrk(line,"0123456789"),"%d", &frame_number);

	/* find the highest frame number in the file */
	maxnum = 0;
	while(fgets(line,MAXLEN,stdin)!=NULL){
		if(!strncmp(line,"start",5)){
			sscanf(strpbrk(line,"0123456789"),"%d",&number);
			maxnum = (maxnum>number)?maxnum:number;
		}
	}

	length = maxnum - frame_number + 1;
	/* spread should initially be the smallest power of two larger than
	 * or equal to length */
	spread = 2;
	while (spread < length)
		spread = spread<<1;

	first_frame = frame_number;
	success = 1;
	while (length--){
		number = -1;
		success = 0; /* tells whether or not any frames have been found  which have the current frame number*/
		if (incremental){
			fseek(stdin, 0L, 0);
		} else {
			fseek(stdin, last_pos, 0);
		}

		reserve = MAXLEN*MAXLINES;
		pbuffer[0] = '\0'; /* delete old pbuffer */

		/* inner loop: search through the entire file for frames */
		/*  which have the current frame number */
		while (!feof(stdin)){

			/*read to next "start" command*/
			while (fgets(line,MAXLEN,stdin)!=NULL){
				if (!strncmp(line,"start",5)){
					sscanf( strpbrk(line,"0123456789"),"%d", &number);
					break;
				}
			}
			if (number==frame_number){
				if (!success){ /*first successful match*/
					printf("%s",line);
					if (!suppressed) printf("clean;\n");
					success = 1;
					last_pos = ftell(stdin);
				}
				/* print contents until next "end" */
				while (fgets(line,MAXLEN,stdin)!=NULL){
					if (!strncmp(line,"end;",4))
						break;
					else if (strncmp(line,"clean",5))
						printf("%s",line);
				}
				/* save contents until next "start" */
				while (fgets(line,MAXLEN,stdin)!=NULL){
					if(!strncmp(line,"start",5))
						break;
					else {
						reserve -= strlen(line);
						reserve -= 1;
						if (reserve > 0){
							strncat(pbuffer,line,MAXLEN);
							strcat(pbuffer,"\n");
						}
					}
				}
			}
		}
		if (success)
			printf("end;\n");
		/* print saved-up post-raytracing commands, if any */
		printf("%s",pbuffer);

		/* get next frame number */
		if (incremental){
			frame_number = frame_number + 2*spread;
			while (frame_number > maxnum){
				spread = spread>>1;
				frame_number = first_frame + spread;
			}
		} else {
			frame_number += 1;
		}
	}
}

#define OPT_STR "ci"

int get_args(argc,argv)
int argc;
char **argv;
{

        int c;
	suppressed = 0;

        while ( (c=bu_getopt(argc,argv,OPT_STR)) != EOF) {
                switch(c){
                case 'c':
                	suppressed = 1;
                        break;
                case 'i':
                	incremental = 1;
                	break;
                default:
                        fprintf(stderr,"Unknown option: -%c\n",c);
                        return(0);
                }
        }
        return(1);
}

		
