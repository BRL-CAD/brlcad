/* Constants used with VAS IV */

/* Use 'C_' prefix for commands */
/* Use 'R_' prefix for result codes */


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

#ifdef USNA
#define C_FFORWARD	'!'
#define C_PAUSE		'"'
#define C_REWIND	'#'
#else
/* BRL version */
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
#define R_DOWN		'D'
#define R_MISSED	'M'


#define TRUE 1
#define FALSE 0
