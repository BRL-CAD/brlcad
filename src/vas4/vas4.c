/*                          V A S 4 . C
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
/** @file vas4.c
 *
 *  Program to control the Lyon-Lamb VAS IV video animation controller
 *  with Sony BVU-850 recorder (or equiv).
 *
 *
 *  Authors -
 *	Steve Satterfield, USNA
 *	Joe Johnson, USNA
 *	Michael John Muuss, BRL
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
                                                                                                                                                                            

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "fb.h"
#include "./vas4.h"

int		debug;

static char	*framebuffer = NULL;
static int	scr_width = 0;		/* screen tracks file if not given */
static int	scr_height = 0;

void	usage(void), usage_new(void), usage_record(void), usage_seq(void);
void	program_recording(int new, int scene_number, int start_frame), record_add_to_scene(int number_of_frames), do_record(int wait);
void	get_tape_position(void);

/*
 *			G E T _ A R G S
 */
int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "dhF:s:S:w:W:n:N:" )) != EOF )  {
		switch( c )  {
		case 'd':
			debug = 1;
			break;

		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'S':
		case 's':
			/* square file size */
			scr_height = scr_width = atoi(optarg);
			break;
		case 'w':
		case 'W':
			scr_width = atoi(optarg);
			break;
		case 'n':
		case 'N':
			scr_height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )
		return(0);

	return(1);		/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	register FBIO	*fbp = FBIO_NULL;
	int scene_number = 1;
	int start_frame = 1;
	int number_of_frames=0;
	int number_of_images=0;
	int start_seq_number=0;
	int	exit_code;

	exit_code = 0;

	if ( !get_args( argc, argv ) )  {
		usage();
		exit( 1 );
	}

	argc -= (optind-1);
	argv += (optind-1);

	/*
	 *  First group of commands.
	 *  No checking of VAS IV status is performed first.
	 */
	if (strcmp(argv[1],"init") == 0) {
		vas_open();
		if( get_vas_status() < 0 )
			exit(1);
		vas_putc(C_INIT);
		vas_await(R_INIT, 5);
		if( get_vtr_status(0) < 0 )
			exit_code = 1;
		goto done;
	}
	else if (strcmp(argv[1],"status") == 0) {
		vas_open();
		(void)get_vas_status();
		(void)get_vtr_status(1);
		get_tape_position();
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
			number_of_frames = str2frames(argv[3]);
			if (number_of_frames < 1)
				usage_seq();
			start_seq_number = 1;
		}
		else if (argc == 5) {
			number_of_images = atoi(argv[2]);
			if (number_of_images < 1)
				usage_seq();
			number_of_frames = str2frames(argv[3]);
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
		goto done;
	}
	else if (strcmp(argv[1],"play") == 0) {
		vas_putc(C_PLAY);
		goto done;
	}
	else if (strcmp(argv[1],"stop") == 0) {
		vas_putc(C_STOP);
		goto done;
	}
	else if (strcmp(argv[1],"fforward") == 0) {
		vas_putc(C_FFORWARD);
		goto done;
	}

	/*
	 *  Third group of commands.
	 *  VAS IV is opened and checked above,
	 *  and VTR status is checked here.
	 */
	if( get_vtr_status(0) < 0 )  {
		exit_code = 1;
		goto done;
	}
	if( strcmp(argv[1], "search") == 0 )  {
		if( argc >= 3 )
			start_frame = str2frames(argv[2]);
		else
			start_frame = 1;
		exit_code = search_frame(start_frame);
		goto done;
	}
	else if( strcmp(argv[1], "time0") == 0 )  {
		exit_code = time0();
		goto done;
	}
	else if( strcmp( argv[1], "reset_time" ) == 0 )  {
		exit_code = reset_tape_time();
		goto done;
	}

	/*
	 *  If this is running on a workstation, and a window has to be
	 *  opened to cause image to be re-displayed, do so now.
	 */
	if( framebuffer != (char *)0 )  {
		if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == FBIO_NULL )  {
			exit_code = 12;
			goto done;
		}
	}

	/*
	 *  Commands that will actually record some image onto tape
	 */
	if (strcmp(argv[1], "new") == 0) {
		if( argc >= 4 )  {
			if( (start_frame = str2frames(argv[3])) < 1 )
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
			if( (start_frame = str2frames(argv[3])) < 1 )
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
			number_of_frames = str2frames(argv[2]);
			if (number_of_frames < 1) {
				usage_record();
			}
		}
		else if (argc == 2) {
			number_of_frames = 1;
		}
		else {
			usage_record();
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
	if(fbp) fb_close(fbp);
	exit(exit_code);
}

void
usage(void)
{
	fprintf(stderr,"Usage: vas4 [-d] [-h] [-F framebuffer]\n");
	fprintf(stderr,"	[-{sS} squarescrsize] [-{wW} scr_width] [-{nN} scr_height]\n");
	fprintf(stderr,"	keyword [options]\n");
	fprintf(stderr,"Keywords:\n");
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

void
usage_new(void)
{
	fprintf(stderr,"Usage: vas4 new [sn]\n");
	fprintf(stderr,"\t[sn]\tscene number must be >= 1\n");
	exit(1);
}

void
usage_record(void)
{
	fprintf(stderr,"Usage: vas4 record [nf]\n");
	fprintf(stderr,"\t[nf]\tnumber of frames must be >= 1\n");
	exit(1);
}

void
usage_seq(void)
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
void
program_recording(int new, int scene_number, int start_frame)
{
	int number_of_frames = 1;

	/* Enter VAS IV program mode */	
	vas_putc(C_PROGRAM);
	vas_await(R_PROGRAM, 0);

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
	vas_putc(C_EE);
	vas_await(R_RECORD, 30);

	/* New scene only records 4 sec matte */
	if( new )  {
		fprintf(stderr,"Recording built-in title matte\n");
		do_record(1);
	}
}

/*
 *			R E C O R D _ A D D _ T O _ S C E N E
 */
void
record_add_to_scene(int number_of_frames)
{
	vas_putc(C_FRAME_CHANGE);
	vas_putnum(number_of_frames);
	vas_putc(C_ENTER);

	fprintf(stderr,"Recording %d frames, %g seconds\n",
		number_of_frames, (double)number_of_frames/30.);
	do_record(0);
}

/*
 *			D O _ R E C O R D
 *
 *  Handle the actual mechanics of starting a record operation,
 *  given that everything has been set up.
 *  If tape-head-unload timer has gone off, also handle retry
 *  operations, until frames are actually recorded.
 *
 *  NOTE:  at the present time, use of no-wait operation
 *  may result in deadlocks, if a subsequent invocation of
 *  this program is started before the backup operation has finished,
 *  because that will result in an unsolicited 'R' (ready for Record)
 *  command.  get_vtr_status() could be extended or supplemented
 *  so that most commands would wait until the VTR stops moving,
 *  but this added complexity is unwelcome.  Therefore, even though
 *  this feature is "implemented", it may cause problems.
 *  So far, this has not proven to be troublesome, but nearly all
 *  experience so far has been with frame-load time much greater than
 *  the tape backspace time.
 */
void
do_record(int wait)
{
	register int c;

	vas_putc(C_RECORD);
	for(;;)  {
		c = vas_getc();
		if(debug) vas_response(c);
		switch( c )  {
		case R_RECORD:
			/* Completely done, ready to record next frame */
			break;
		case R_DONE:
			if( wait )  {
				continue;
			} else {
				/* Don't wait for tape backspacing */
				break;
			}
		case R_MISSED:
			/*
			 * Preroll failed (typ. due to timer),
			 * VAS is backspacing for retry, wait for GO.
			 */
			vas_await(R_RECORD,99);
			vas_putc(C_RECORD);
			continue;
		case R_CUT_IN:
		case R_CUT_OUT:
			/* These are for info only */
			continue;
		default:
			if(!debug) vas_response(c);
			continue;
		}
		break;
	}
	return;
}

/*
 *			S E A R C H _ F R A M E
 *
 *  It is unclear what modes this is safe in
 */
int
search_frame(int frame)
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
 *  This is only safe when the tape is playing or not moving.
 */
int
reset_tape_time(void)
{
	vas_putc(C_RESET_TAPETIME);
	return(0);
}

/*
 *			T I M E 0
 * 
 *  Seek to time timer value 00:00:00:00, as set by ?INIT? or
 *  reset_tape_time(), above.
 *
 *  It is unclear what modes this is safe in
 */
int
time0(void)
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
int
get_vas_status(void)
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
 *  If 'chatter' is 0, only errors are logged to stderr,
 *  otherwise all conditions are logged.
 *
 *  Returns -
 *	1	all is well, VTR is ready to roll
 *	0	all is well
 *	-1	problems (with description)
 */
int
get_vtr_status(int chatter)
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
		if(chatter) fprintf(stderr,"VTR is online and ready to roll\n");
		return(1);	/* very OK */
	} else if(  buf[2] == 'N' )  {
		if(chatter) fprintf(stderr,"VTR is online and stopped\n");
		return(0);	/* OK */
	} else {
		fprintf(stderr,"VTR is online and has unknown ready status\n");
		return(-1);
	}
	/* [3] is S for stop, P for play, 
	 *   L for shuttle var speed, W for slow speed */
}

int
get_frame_code(void)
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

void
get_tape_position(void)
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

/*
 *			S T R 2 F R A M E S
 *
 *  Given a numeric string, convert it to frames.
 *  The input is expected to be a number followed by letters,
 *  which will apply a multiplier, eg
 *	10	10 frames
 *	12f	12 frames
 *	3s	3 seconds (90 frames)
 *	1m	1 minute (1800 frames)
 *
 *  Excess characters in the string are ignored, so inputs of the form
 *  "32sec" and "3min" are fine.
 */
int
str2frames(char *str)
{
	int	num;
	char	suffix[32];

	suffix[0] = '\0';
	sscanf( str, "%d%s", &num, suffix );
	switch( suffix[0] )  {
	case 'f':
	case '\0':
		break;
	case 's':
		num *= 30;
		break;
	case 'm':
		num *= 60 * 30;
		break;
	default:
		fprintf(stderr, "str2frames:  suffix '%s' unknown\n", str);
		break;
	}
	return(num);
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
