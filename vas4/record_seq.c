/*
 * record_seq.c - control the display of a series of ikonas images
 *	and record them frame by frame on the ikonas
 *
 * 	The interface to the ikonas is that this program will execute
 *	a command int the form: display_image n    where n will be a sequence
 *	number from 0 to N-1 where N is the number of images specified
 *	in the command line of this command.
 *
 *	The user will typically provide a shell script
 *	that will display the appropriate image on the Ikonas.
 */

#include <stdio.h>
#include <strings.h>

/* #define DEBUG 1 /* Define to only print system commands */

#ifdef DEBUG
#define SYSTEM(c)	fprintf(stderr,"system(%s);\n",c)
#else
#define SYSTEM(c)	fprintf(stderr,"system(%s);\n",c); system(c)
#endif

#define SECONDS(x)	(x * 30)    /* 30 frames/sec */

#define CBARS_TIME	10		/* Normal */
/*#define CBARS_TIME	2		/* Testing mode */


record_seq(number_of_images, number_of_frames, start_seq_number)
int number_of_images, number_of_frames;
int start_seq_number;
{
	char cmd[100];
	int i;
	int scene_number = 1;
	int start_frame = 1;


	fprintf(stderr,
		"number of images is %d, number of frames per image is %d\n",
		number_of_images,number_of_frames);

	
	SYSTEM("ikcolorbars");	/* Start out with color bars */
	init_ik();		/* Set up Ikonas */

	/* Make initial scene title matte recording */
	SYSTEM("vas4 new");

	/* Handle the color bars specially. It is the first recording */
	fprintf(stderr,"Record color bars for %d seconds\n",CBARS_TIME);
	sprintf(cmd,"vas4 record %d", SECONDS(CBARS_TIME));
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
	SYSTEM("vas4 record 900\n");

	/* Wrap up by stopping the controller and rewind */
	fprintf(stderr,"Init and Rewind\n");
	SYSTEM("vas4 init\n");
	SYSTEM("vas4 rewind\n");

}
