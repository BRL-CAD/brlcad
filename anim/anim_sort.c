/*			A N I M _ S O R T . C
 *
 *	Combine multiple animation scripts on standard input into a 
 *  single script on standard output.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXLEN	50		/*maximum length of lines to be read */

typedef struct list {		/* linked list of post-raytracing commands */
	struct list *next;
	char 	line[MAXLEN];
} LINE_LIST;

int suppressed;		/* flag: suppress printing of 'clean;' commands */

main(argc, argv)
int argc;
char **argv;
{
	int	frame_number, number, success; 
	long	last_pos;
	char	line[MAXLEN];

	LINE_LIST *buf_start=NULL, *buf_end=NULL;
	void do_free();

        if (!get_args(argc,argv))
		fprintf(stderr,"Get_args error\n");

	/* copy any lines preceeding the first "start" command */
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

	/* main loop: repeat for every integer, beginning with the first */
	/*  frame number, until reaching a frame number not found anywhere*/
	/*  in the file */
	success = 1;
	while (success > 0){
		number = -1;
		success = 0; /* tells whether or not any frames have been found  which have the current frame number*/
		fseek(stdin, last_pos, 0);

		do_free(buf_start);
		buf_start = buf_end = NULL;
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
						if (buf_start==NULL){
							buf_start = (LINE_LIST *) malloc(sizeof(LINE_LIST));
							buf_end = buf_start;
						}
						else {
							buf_end->next = (LINE_LIST *) malloc(sizeof(LINE_LIST));
							buf_end = buf_end->next;
						}
						buf_end->next = NULL;
						strcpy(buf_end->line,line);
					}
				}
			}
		}
		if (success)
			printf("end;\n");
		/* print saved-up post-raytracing commands, if any */
		buf_end = buf_start;
		while(buf_end != NULL){
			printf("%s",buf_end->line);
			buf_end = buf_end->next;
		}

		frame_number += 1;
	}
	do_free(buf_start);
}

void do_free(pl)
LINE_LIST *pl;
{
	LINE_LIST *px;

	while (pl != NULL){
		px = pl->next;
		free(pl);
		pl = px;
	}
}

#define OPT_STR "c"

int get_args(argc,argv)
int argc;
char **argv;
{

        int c;
	suppressed = 0;

        while ( (c=getopt(argc,argv,OPT_STR)) != EOF) {
                switch(c){
                case 'c':
                	suppressed = 1;
                        break;
                default:
                        fprintf(stderr,"Unknown option: -%c\n",c);
                        return(0);
                }
        }
        return(1);
}

		
