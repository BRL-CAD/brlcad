/*                          V A S 4 . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file vas4.h
 *
 *  Constants used with the Lyon-Lamb VAS IV video animation controller.
 *
 *  Use 'C_' prefix for commands
 *  Use 'R_' prefix for result codes
 *
 *  Authors -
 *	Steve Satterfield, USNA
 *	Joe Johnson, USNA
 *	Michael John Muuss, BRL
 *
 */

/* Commands */
#define C_DISPLAY_DATA	'+'
#define C_EE		'&'
#define C_EDIT		'.'
#define C_ENTER		':'
#define C_FRAME_CHANGE	'-'
#define C_FRAME_CODE	'*'
#define C_HOLD		','
#define C_PLAY		'$'
#define C_PROGRAM	')'
#define C_RECORD	'\''
#define C_REGISTER	'('
#define C_REPLACE	'/'
#define C_STOP		' '
#define C_SEARCH	'?'
#define C_RESET_TAPETIME '<'

/* There seems to be at least 2 version of the microcode in the VAS IV */
#ifdef USNA
#define C_FFORWARD	'!'
#define C_PAUSE		'"'
#define C_REWIND	'#'
#else
/* BRL version can not do these operations */
#define C_FFORWARD	C_STOP
#define C_PAUSE		C_STOP
#define C_REWIND	C_STOP
#endif

#define C_INIT		'I'
#define C_NO		'0'
#define C_ACTIVITY	'A'
#define C_SEND_TAPE_POS	'B'
#define C_SEND_FRAME_CODE 'C'
#define C_VTR_STATUS	'V'

/* Result codes */
#define R_INIT		'I'
#define R_PROGRAM	'P'
#define R_SEARCH	'S'
#define R_RECORD	'R'
#define R_DONE		'D'
#define R_MISSED	'M'
#define R_CUT_IN	'X'
#define R_CUT_OUT	'Y'

#define TRUE 1
#define FALSE 0

extern void	vas_open(void);
extern int	get_vas_status(void);
extern int	vas_putc(char c);
extern int	vas_await(int c, int sec);
extern int	get_vtr_status(int chatter);
extern int	get_frame_code(void);
extern int	str2frames(char *str);
extern void	record_seq(int number_of_images, int number_of_frames, int start_seq_number);
extern int	search_frame(int frame);
extern int	time0(void);
extern int	reset_tape_time(void);
extern void	vas_close(void);
extern void	vas_putnum(int n);
extern int	vas_getc(void);
extern void	vas_response(char c);
extern int	vas_rawputc(char c);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
