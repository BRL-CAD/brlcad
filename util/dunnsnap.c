/*
 *			D U N N S N A P . C
 *
 *	Checks status of the Dunn camera and exposes the number of frames
 *	of film specified in the argument (default is 1 frame).
 *
 *	dunnsnap [num_frames]
 *
 *  Author -
 *	Don Merritt
 *	August 1985
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

extern int	fd;
extern char	cmd;

main(argc, argv)
int argc;
char **argv;
{
	int nframes;

	dunnopen();

	/* check argument */

	if ( argc < 1 || argc > 2) {
		printf("Usage: dunnsnap [num_frames]\n");
		exit(25);
	}
	if ( argc > 1) 
		nframes = atoi(*++argv);
	else
		nframes = 1;

	if (!ready(2)) {
		printf("dunnsnap:  camera not ready at startup\n");
		exit(30);
	}
		
	/* loop until number of frames specified have been exposed */

	while (nframes) {

		while (!ready(20)) {
			printf("dunnsnap: camera not ready at frame start\n");
			exit(40);
		}

		if (!goodstatus()) {
			printf("badstatus\n");
			exit(50);
		}
		
		/* send expose command to camera */

			cmd = 'I';	/* expose command */
			write(fd, &cmd, 1);
			hangten();
			if (!ready(20)) {
				printf("dunnsnap: camera not ready after expose cmd\n");
				exit(60);
			}
		--nframes;
	}
}
