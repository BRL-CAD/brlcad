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
#include "vas4.h"


main(argc, argv)
int argc;
char **argv;
{
	int scene_number = 1;
	int number_of_frames;
	int number_of_images;
	int start_seq_number;


	if (argc < 2)
		usage();

	if (strcmp(argv[1],"init") == 0) {
		vas_open();
		vas_putc_wait(C_INIT, R_INIT);
		fprintf(stderr,"VAS Initialized\n");
		vas_close();
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
	else if (strcmp(argv[1],"rs170") == 0) {
		init_ik();
	}
	else if (strcmp(argv[1],"status") == 0) {
		status();
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
	}

}

usage(code)
int code;
{
	fprintf(stderr,"Usage: vas4 keyword [options]\n");
	fprintf(stderr,"\nkeywords: init, rewind, play, stop, new, record, rs170, sequence, status\n");
	exit(1);
}

usage_new()
{
	fprintf(stderr,"Usage: vas4 new [sn]\n");
	fprintf(stderr,"\n\t[sn]\tscene number must be > 1\n");
	exit(1);
}

usage_record()
{
	fprintf(stderr,"Usage: vas4 record [nf]\n");
	fprintf(stderr,"\n\t[nf]\tnumber of frames must be > 1\n");
	exit(1);
}

usage_seq()
{
	fprintf(stderr,"Usage: vas4 sequence [n nf] [start]\n");
	fprintf(stderr,"\n\t[n nf]\tthe number of images n must be > 1\n");
	fprintf(stderr,"\t\tand the number of frames nf  must be > 1\n");
	fprintf(stderr,"\n\t[start] is the starting sequence number\n");
	fprintf(stderr,"\t\tIf start is specified, n and nf must also be specified\n");
	exit(1);
}





record_new_scene(scene_number)
int scene_number;
{
	char str[100];
	int number_of_frames = 1;
	int start_frame = 1;

	fprintf(stderr,"Recording Started\n");
	vas_open();

	/* Init the VAS */
	vas_putc_wait(C_INIT, R_INIT);
	fprintf(stderr,"VAS Initialized\n");

	/* Enter VAS IV program mode */	
	vas_putc_wait(C_PROGRAM, R_PROGRAM);

	/* Set the number of frames */
	sprintf(str,"%d",number_of_frames);
	vas_puts(str);
	vas_putc(C_ENTER);

	/* Set the scene number */
	sprintf(str,"%d",scene_number);
	vas_puts(str);
	vas_putc(C_ENTER);

	/* New or Old scene */
	vas_putc(C_ENTER);   /* For new scene simple skip */

	/* Set the starting frame */
	sprintf(str,"%d",start_frame);
	vas_puts(str);
	vas_putc(C_ENTER);

	fprintf(stderr,"sleep 2\n");
	sleep(2);

	/* E/E light should now be flashing */
	/* Press E/E */
	vas_putc_wait(C_EE, R_RECORD);

	fprintf(stderr,"Sleep 2\n");
	sleep(2);

	/* New scene only records 4 sec matte */
	fprintf(stderr,"Record 4 sec matte\n");
	vas_putc_wait(C_RECORD, R_RECORD);

	fprintf(stderr,"Sleep 2\n");
	sleep(2);

	vas_close();

	fprintf(stderr,"Recording Done\n");
}


record_add_to_scene(number_of_frames)
int number_of_frames;
{
	char str[100],c,send;
	int retry;

	vas_open();

	sprintf(str,"%c%d%c",C_FRAME_CHANGE,number_of_frames,C_ENTER);
	vas_puts(str);
	fprintf(stderr,"Sleep 1\n");
	sleep(1);

	fprintf(stderr,"Recording (approximately %g seconds)\n",(float)number_of_frames/30.);
	retry = 1;
	while (retry == 1) {
		retry = 0;
		send = C_RECORD;
		vas_putc(C_RECORD);
		while ((c = vas_getc()) !=  R_RECORD) {
			fprintf(stderr,"Send '%c', Expect '%c', Got '%c'\n",
				send,R_RECORD,c);
			if (c == R_MISSED) {
				fprintf(stderr,"Preroll failed, sleep for 5 seconds\n");
				fprintf(stderr,"Then re-try\n");
				sleep(5);
				vas_putc(C_EE);
				send = C_EE;
				retry = 1;
			}
		}
		fprintf(stderr,"Send '%c', Expect '%c', Got '%c'\n",
			send,R_RECORD,c);
	}

	fprintf(stderr,"Sleep 2\n");
	sleep(2);

	vas_close();
	fprintf(stderr,"Recording Done\n");
}
