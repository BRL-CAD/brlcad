/*                    R E C O R D _ S E Q . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 */
/** @file record_seq.c
 * record_seq.c - control the display of a series of images
 *	and record them frame by frame
 *
 * 	The interface to the framebuffer is that this program will execute
 *	a command int the form: display_image n    where n will be a sequence
 *	number from 0 to N-1 where N is the number of images specified
 *	in the command line of this command.
 *
 *	The user will typically provide a shell script
 *	that will display the appropriate image on the framebuffer.
 */

#include "common.h"



#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

/* #define DEBUG 1  Define to only print system commands */

#ifdef DEBUG
#define SYSTEM(c)	fprintf(stderr,"system(%s);\n",c)
#else
#define SYSTEM(c)	fprintf(stderr,"system(%s);\n",c); system(c)
#endif

#define CBARS_TIME	10		/* Normal */

void
record_seq(int number_of_images, int number_of_frames, int start_seq_number)
{
	char cmd[100];
	int i;

	fprintf(stderr,
		"number of images is %d, number of frames per image is %d\n",
		number_of_images,number_of_frames);

	
	SYSTEM("fbcbars");	/* Start out with color bars */

	/* Make initial scene title matte recording */
	SYSTEM("vas4 new");
	SYSTEM("vas4 reset_time");

	/* Handle the color bars specially. It is the first recording */
	fprintf(stderr,"Record color bars for %d seconds\n",CBARS_TIME);
	sprintf(cmd,"vas4 record %dsec", CBARS_TIME);
	SYSTEM(cmd);

	/* Now record the user files */
	for (i=start_seq_number; i < start_seq_number+number_of_images; i++) {
		sprintf(cmd,"display_image %d",i);
		SYSTEM(cmd);
		sprintf(cmd,"vas4 record %d",number_of_frames);
		SYSTEM(cmd);
	}

	/* Record last frame for 30 more seconds */
	fprintf(stderr,"Last image\n");
	SYSTEM("vas4 record 30sec\n");

	/* Wrap up by stopping the controller and rewind */
	SYSTEM("vas4 time0\n");
	SYSTEM("vas4 stop\n");

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
