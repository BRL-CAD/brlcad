/*
 * vas4.c - program to control the Lyon-Lamb VAS IV 
 *
 * See man page for details.
 *
 * Steve Satterfield, CADIG, USNA
 *
 * The vas record routines are based upon work by Joe Johnson
 */

#include <stdio.h>
#include "./vas4.h"

extern int	debug;

main(argc, argv)
int argc;
char **argv;
{
	int scene_number = 1;
	int number_of_frames;
	int number_of_images;
	int start_seq_number;
	int	exit_code;

	exit_code = 0;

	if (argc < 2)
		usage();

	if( strcmp(argv[1], "-d") == 0 )  {
		debug = 1;
		argv++; argc--;
	}

	if (strcmp(argv[1],"init") == 0) {
		vas_open();
		vas_putc_wait(C_INIT, R_INIT);
		if( get_vtr_status() < 0 )
			exit_code = 1;
		else
			fprintf(stderr,"VAS Initialized\n");
		vas_close();
		exit(exit_code);
	}
	else if (strcmp(argv[1],"rewind") == 0) {
		vas_open();
		vas_putc(C_REWIND);
		fprintf(stderr,"VTR Rewinding\n");
		vas_close();
	}
	else if (strcmp(argv[1],"play") == 0) {
		vas_open();
		vas_putc(C_PLAY);
		fprintf(stderr,"VTR Playing\n");
		vas_close();
	}
	else if (strcmp(argv[1],"stop") == 0) {
		vas_open();
		vas_putc(C_STOP);
		vas_await(0, 5);
		fprintf(stderr,"VTR STOPPED\n");
		vas_close();
	}
	else if (strcmp(argv[1],"fforward") == 0) {
		vas_open();
		vas_putc(C_FFORWARD);
		fprintf(stderr,"VTR Fast Forward\n");
		vas_close();
	}
	else if (strcmp(argv[1],"new") == 0) {
		if (argc == 3) {
			scene_number = atoi(argv[2]);
			if (scene_number < 1) {
				usage_new();
			}
		}
		else if (argc == 2) {
			scene_number = 1;
		}
		else {
			usage_new();
		}
		record_new_scene(scene_number);
	}
	else if (strcmp(argv[1],"record") == 0) {
		if (argc == 3) {
			number_of_frames = atoi(argv[2]);
			if (number_of_frames < 1) {
				usage_record();
			}
		}
		else if (argc == 2) {
			number_of_frames = 1;
		}
		else {
			usage_new();
		}
		record_add_to_scene(number_of_frames);
	}
	else if (strcmp(argv[1],"status") == 0) {
		vas_open();

		/* Request activity, result code will be interpreted for us */
		vas_putc(C_ACTIVITY);
		get_vtr_status();
		vas_close();
	}
	else if (strcmp(argv[1],"sequence") == 0) {
		if (argc == 2) {
			number_of_images = 1;
			number_of_frames = 1;
			start_seq_number = 1;
		}
		else if (argc == 4) {
			number_of_images = atoi(argv[2]);
			if (number_of_images < 1)
				usage_seq();
			number_of_frames = atoi(argv[3]);
			if (number_of_frames < 1)
				usage_seq();
			start_seq_number = 1;
		}
		else if (argc == 5) {
			number_of_images = atoi(argv[2]);
			if (number_of_images < 1)
				usage_seq();
			number_of_frames = atoi(argv[3]);
			if (number_of_frames < 1)
				usage_seq();
			start_seq_number = atoi(argv[4]);
		}
		else
			usage_seq();


		record_seq(number_of_images, number_of_frames, start_seq_number);
	}
	else {
		usage();
		exit(1);
	}
	exit(exit_code);
}

usage(code)
int code;
{
	fprintf(stderr,"Usage: vas4 [-d] keyword [options]\n");
	fprintf(stderr,"keywords: init, rewind, play, stop, new, record, sequence, status\n");
	exit(1);
}

usage_new()
{
	fprintf(stderr,"Usage: vas4 new [sn]\n");
	fprintf(stderr,"\t[sn]\tscene number must be > 1\n");
	exit(1);
}

usage_record()
{
	fprintf(stderr,"Usage: vas4 record [nf]\n");
	fprintf(stderr,"\t[nf]\tnumber of frames must be > 1\n");
	exit(1);
}

usage_seq()
{
	fprintf(stderr,"Usage: vas4 sequence [n nf] [start]\n");
	fprintf(stderr,"\t[n nf]\tthe number of images n must be > 1\n");
	fprintf(stderr,"\t\tand the number of frames nf  must be > 1\n");
	fprintf(stderr,"\t[start] is the starting sequence number\n");
	fprintf(stderr,"\t\tIf start is specified, n and nf must also be specified\n");
	exit(1);
}


/*
 *			R E C O R D _ N E W _ S C E N E
 */
record_new_scene(scene_number)
int scene_number;
{
	char str[100];
	int number_of_frames = 1;
	int start_frame = 1;

	vas_open();

	/* Init the VAS */
	vas_putc_wait(C_INIT, R_INIT);
	fprintf(stderr,"VAS Initialized\n");

	/* Enter VAS IV program mode */	
	vas_putc_wait(C_PROGRAM, R_PROGRAM);

	vas_putnum(number_of_frames);
	vas_putc(C_ENTER);

	vas_putnum(scene_number);
	vas_putc(C_ENTER);

	/* New or Old scene */
	vas_putnum(1);		/* New.  0 for Old */
	vas_putc(C_ENTER);

	vas_putnum(start_frame);
	vas_putc(C_ENTER);
	vas_await(R_SEARCH, 120);

	/* E/E light should now be flashing, Press E/E */
	vas_putc_wait(C_EE, R_RECORD);

	/* New scene only records 4 sec matte */
	fprintf(stderr,"Record 4 sec matte\n");
	vas_putc(C_RECORD);
	vas_await(R_RECORD, 30);

	vas_close();

	fprintf(stderr,"Record_new_scene Done\n");
}

/*
 *			R E C O R D _ A D D _ T O _ S C E N E
 */
record_add_to_scene(number_of_frames)
int number_of_frames;
{
	char str[100],c,send;
	int retry;

	vas_open();
	if( get_vtr_status() < 0 )
		exit(1);

	/* If it has dropped out of E/E, need to get it back into it */
	vas_putc(C_EE);	
	vas_response(vas_getc());	/* should get R_RECORD */

	vas_putc(C_FRAME_CHANGE);
	vas_putnum(number_of_frames);
	vas_putc(C_ENTER);

	fprintf(stderr,"Recording %d frames, %g seconds\n",
		number_of_frames, (double)number_of_frames/30.);
	for( retry=0; retry<4; retry++ )  {
		vas_putc(C_RECORD);
		for(;;)  {
			c = vas_getc();
			if( c == R_RECORD)
				goto done;
			vas_response(c);
			if (c == R_MISSED) {
				fprintf(stderr,"Preroll failed\n");
				vas_await(R_RECORD,99);
				break;
			}
		}
	}
done:

	vas_close();
	fprintf(stderr,"Recording Done\n");
}

/*
 *			G E T _ V T R _ S T A T U S
 *
 *  Returns -
 *	0	all is well
 *	-1	problems (with description)
 */
get_vtr_status()
{
	char	buf[4];

	vas_rawputc(C_VTR_STATUS);
	buf[0] = vas_getc();
	buf[1] = vas_getc();
	buf[2] = vas_getc();
	buf[3] = vas_getc();

	if( buf[0] != 'V' )  {
		fprintf(stderr,"Link to VTR is not working\n");
		return(-1);
	}
	if( buf[1] != 'R' )  {
		if( buf[1] == 'L' )  {
			fprintf(stderr,"VTR is in Local mode, can not be program controlled\n");
			return(-1);
		}
		fprintf(stderr, "VTR is in unknown mode\n");
		return(-1);
	}
	/* [2] is R for ready, N otherwise */
	/* [3] is S for stop, P for play, 
	 *   L for shuttle var speed, W for slow speed */
	return(0);		/* OK */
}
