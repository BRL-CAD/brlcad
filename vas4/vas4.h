/*
 *			V A S 4 . H
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
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
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

extern void	vas_open();
extern int	get_vas_status();
extern int	vas_putc();
extern int	vas_await();
extern int	get_vtr_status();
extern int	get_frame_code();
extern int	str2frames();
extern void	record_seq();
extern int	search_frame();
extern int	time0();
extern int	reset_tape_time();
extern void	vas_close();
extern void	vas_putnum();
extern int	vas_getc();
extern void	vas_response();
extern int	vas_rawputc();
