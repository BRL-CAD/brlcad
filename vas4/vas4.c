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
	int start_frame = 1;
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

	/*
	 *  First group of commands.
	 *  No checking of VAS IV status is performed first.
	 */
	if (strcmp(argv[1],"init") == 0) {
		vas_open();
		if( get_vas_status() < 0 )
			exit(1);
		vas_putc_wait(C_INIT, R_INIT);
		if( get_vtr_status() < 0 )
			exit_code = 1;
		fprintf(stderr,"VAS Initialized\n");
		goto done;
	}
	else if (strcmp(argv[1],"status") == 0) {
		vas_open();
		(void)get_vas_status();
		(void)get_vtr_status();
		(void)get_tape_position();
		(void)get_frame_code();
		goto done;
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
		goto done;
	}

	/*
	 *  Second group of commands.
	 *  VAS IV is opened, and status of VAS is checked first
	 *  but VTR is not queried.
	 */
	vas_open();
	if( get_vas_status() < 0 )
		exit(1);

	if (strcmp(argv[1],"rewind") == 0) {
		vas_putc(C_REWIND);
		fprintf(stderr,"VTR Rewinding\n");
		goto done;
	}
	else if (strcmp(argv[1],"play") == 0) {
		vas_putc(C_PLAY);
		fprintf(stderr,"VTR Playing\n");
		goto done;
	}
	else if (strcmp(argv[1],"stop") == 0) {
		vas_putc(C_STOP);
/**		vas_await(0, 5);	/* XXX */
		fprintf(stderr,"VTR STOPPED\n");
		goto done;
	}
	else if (strcmp(argv[1],"fforward") == 0) {
		vas_putc(C_FFORWARD);
		fprintf(stderr,"VTR Fast Forward\n");
		goto done;
	}

	/*
	 *  Third group of commands.
	 *  VAS IV is opened and checked above,
	 *  and VTR status is checked here.
	 */
	if( get_vtr_status() < 0 )
		exit(1);
	if( strcmp(argv[1], "search") == 0 )  {
		if( argc >= 3 )
			start_frame = atoi(argv[2]);
		else
			start_frame = 1;
		exit_code = search_frame(start_frame);
		goto done;
	}
	else if( strcmp(argv[1], "time0") == 0 )  {
		exit_code = time0();
	}
	else if( strcmp( argv[1], "reset_time" ) == 0 )  {
		exit_code = reset_tape_time();
		goto done;
	}
	else if (strcmp(argv[1], "new") == 0) {
		if( argc >= 4 )  {
			if( (start_frame = atoi(argv[3])) < 1 )
				usage_new();
		}
		if (argc >= 3) {
			if( (scene_number = atoi(argv[2])) < 1 )
				usage_new();
		}
		else if (argc == 2) {
			scene_number = 1;
		}
		else {
			usage_new();
		}
		program_recording(1, scene_number, start_frame);
		goto done;
	}
	else if (strcmp(argv[1], "old") == 0) {
		if( argc >= 4 )  {
			if( (start_frame = atoi(argv[3])) < 1 )
				usage_new();
		}
		if (argc >= 3) {
			if( (scene_number = atoi(argv[2])) < 1 )
				usage_new();
		}
		else if (argc == 2) {
			scene_number = 1;
		}
		else {
			usage_new();
		}
		/* May need one more parameter here, eventually */
		program_recording(0, scene_number, start_frame);
		goto done;
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
		goto done;
	} else {
		/* None of the above */
		usage();
		exit_code = 1;
	}

done:
	vas_close();
	exit(exit_code);
}

usage()
{
	fprintf(stderr,"Usage: vas4 [-d] keyword [options]\n");
	fprintf(stderr,"  init\n");
	fprintf(stderr,"  status\n");
	fprintf(stderr,"  stop\n");
	fprintf(stderr,"  play\n");
	fprintf(stderr,"  rewind\n");
	fprintf(stderr,"  fforward\n");
	fprintf(stderr,"  new [scene_number [start_frame]]\n");
	fprintf(stderr,"  old [scene_number [start_frame]]\n");
	fprintf(stderr,"  record [n_frames]\n");
	fprintf(stderr,"  sequence [n_images n_frames [start_seq]]\n");
	fprintf(stderr,"  search [frame]\n");
	fprintf(stderr,"  time0\n");
	fprintf(stderr,"  reset_time\n");
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
 *			P R O G R A M _ R E C O R D I N G
 */
program_recording(new, scene_number, start_frame)
int	new;
int	scene_number;
int	start_frame;
{
	char str[100];
	int number_of_frames = 1;

	/* Enter VAS IV program mode */	
	vas_putc_wait(C_PROGRAM, R_PROGRAM);

	vas_putnum(number_of_frames);
	vas_putc(C_ENTER);

	vas_putnum(scene_number);
	vas_putc(C_ENTER);

	/* New or Old scene */
	if( new )  {
		vas_putnum(1);
		vas_putc(C_ENTER);

		vas_putnum(start_frame);
		vas_putc(C_ENTER);
	} else {
		vas_putnum(0);
		vas_putc(C_ENTER);

		vas_putnum(start_frame);	/* First frame */
		vas_putc(C_ENTER);

		/* I believe this is used only to initiate a search */
		vas_putnum(start_frame);	/* Last recorded frame */
		vas_putc(C_ENTER);
	}
	vas_await(R_SEARCH, 120);

	/* E/E light should now be flashing, Press E/E */
	vas_putc_wait(C_EE, R_RECORD);

	/* New scene only records 4 sec matte */
	fprintf(stderr,"Record built-in title matte\n");
	vas_putc(C_RECORD);
	vas_await(R_RECORD, 30);
}

/*
 *			R E C O R D _ A D D _ T O _ S C E N E
 */
record_add_to_scene(number_of_frames)
int number_of_frames;
{
	char str[100],c,send;
	int retry;

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
	get_tape_position();
	return;
}

/*
 *			S E A R C H _ F R A M E
 *
 *  It is unclear what modes this is safe in
 */
search_frame(frame)
int	frame;
{
	int	reply;

	vas_putc(C_SEARCH);
	vas_putnum(frame);
	vas_putc(C_ENTER);
	reply = vas_getc();
	if( reply == 'L' )
		return(0);	/* OK */
	/* 'K' is expected failure code */
	vas_response(reply);
	return(-1);		/* fail */
}

/*
 *			R E S E T _ T A P E _ T I M E
 * 
 *  Reset tape time to 00:00:00:00
 *
 *  It is unclear what modes this is safe in
 */
reset_tape_time()
{
	int	reply;

	vas_putc(C_RESET_TAPETIME);
	reply = vas_getc();
	if( reply == 'L' )
		return(0);	/* OK */
	vas_response(reply);
	return(-1);		/* fail */
}

/*
 *			T I M E 0
 * 
 *  Seek to time timer value 00:00:00:00, as set by INIT or
 *  reset_tape_time(), above.
 *
 *  It is unclear what modes this is safe in
 */
time0()
{
	int	reply;

	vas_putc(C_SEARCH);
	vas_putc(C_SEARCH);
	reply = vas_getc();
	if( reply == 'L' )
		return(0);	/* OK */
	vas_response(reply);
	return(-1);		/* fail */
}

/*
 *			G E T _ V A S _ S T A T U S
 *
 *  Returns -
 *	-1	VAS is unwell
 *	>0	VAS is ready, value is return code
 */
get_vas_status()
{
	int	reply;

	vas_rawputc(C_ACTIVITY);
	reply = vas_getc();		/* Needs timeout */
	if(debug) vas_response(reply);
	if( reply < 0x60 || reply > 0x78 )  return(-1);
	return(reply);
}

/*
 *			G E T _ V T R _ S T A T U S
 *
 *  Returns -
 *	1	all is well, VTR is ready to roll
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
	if( buf[2] == 'R' )  {
		fprintf(stderr,"VTR is online and ready to roll\n");
		return(1);	/* very OK */
	} else if(  buf[2] == 'N' )  {
		fprintf(stderr,"VTR is online and stopped\n");
		return(0);	/* OK */
	} else {
		fprintf(stderr,"VTR is online and has unknown ready status\n");
		return(-1);
	}
	/* [3] is S for stop, P for play, 
	 *   L for shuttle var speed, W for slow speed */
}

get_frame_code()
{
	int	status;
	char	scene[4];
	char	frame[7];

	vas_rawputc(C_SEND_FRAME_CODE);
	status = vas_getc();
	scene[0] = vas_getc();
	scene[1] = vas_getc();
	scene[2] = vas_getc();
	scene[3] = '\0';
	frame[0] = vas_getc();
	frame[1] = vas_getc();
	frame[2] = vas_getc();
	frame[3] = vas_getc();
	frame[4] = vas_getc();
	frame[5] = vas_getc();
	frame[6] = '\0';
	if( status != 'C' && status != '<' )  {
		fprintf(stderr,"get_frame_code:  unable to acquire\n");
		return(-1);
	}
	fprintf(stderr,"Scene %s, Frame %s\n", scene, frame);
	/* May want to do something more here */
	return(0);
}

get_tape_position()
{
	char	buf[9];
	int	i;

	vas_rawputc(C_SEND_TAPE_POS);
	for( i=0; i<8; i++ )
		buf[i] = vas_getc();
	buf[8] = '\0';
	fprintf(stderr, "Tape counter is at %s\n", buf);
	/* May want to do more here */
}
