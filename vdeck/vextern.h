/*
	SCCS archive:	/vld/src/vdeck/s.vextern.h

	Author:		???
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
*/

/* Special characters.							*/
#define	LF		"\n"
#define BLANKS	"                                                                          "

/* Command line options.						*/
#define DECK		'd'
#define ERASE		'e'
#define INSERT		'i'
#define LIST		'l'
#define MENU		'?'
#define NUMBER		'n'
#define QUIT		'q'
#define REMOVE		'r'
#define RETURN		'\0'
#define SHELL		'!'
#define SORT_TOC	's'
#define TOC		't'
#define UNKNOWN		default

/* Prompts.								*/
#define CMD_PROMPT	"\ncommand( ? for menu )>> "
#define LST_PROMPT	"\nSOLIDS LIST( ? for menu )> "
#define PROMPT		"vdeck> "

/* Size limits.								*/
#define MAXLN	80	/* max length of input line */
#define MAXRR	100	/* max regions to remember */
#ifndef NDIR
#ifdef sel
#define NDIR	9000	/* max objects in input */
#else
#define NDIR	20000	/* max objects in input */
#endif
#endif
#define MAXPATH	32	/* max level of hierarchy */
#define MAXARG	20	/* max arguments on command line */
#define ARGSZ	32	/* max length of command line argument */

/* Standard flag settings.						*/
#define UP	0
#define DOWN	1
#define QUIET	0
#define NOISY	1
#define UPP     1
#define DWN     0
#define YES	1
#define NO	0
#define ON	1
#define OFF	0

/* Output vector fields.						*/
#define O1	ov+(1-1)*3
#define O2	ov+(2-1)*3
#define O3	ov+(3-1)*3
#define O4	ov+(4-1)*3
#define O5	ov+(5-1)*3
#define O6	ov+(6-1)*3
#define O7	ov+(7-1)*3
#define O8	ov+(8-1)*3
#define O9	ov+(9-1)*3
#define O10	ov+(10-1)*3
#define O11	ov+(11-1)*3
#define O12	ov+(12-1)*3
#define O13	ov+(13-1)*3
#define O14	ov+(14-1)*3
#define O15	ov+(15-1)*3
#define O16	ov+(16-1)*3

/* For solid parameter manipulation.					*/
#define SV0	&(rec->s.s_values[0])
#define	SV1	&(rec->s.s_values[3])
#define SV2     &(rec->s.s_values[6])
#define SV3	&(rec->s.s_values[9])
#define SV4     &(rec->s.s_values[12])
#define SV5     &(rec->s.s_values[15])
#define SV6     &(rec->s.s_values[18])
#define SV7     &(rec->s.s_values[21])


#define MAX( a, b )		if( (a) < (b) )		a = b
#define MIN( a, b )		if( (a) > (b) )		a = b
#define MINMAX( min, max, val )		MIN( min, val );\
				else	MAX( max, val )

extern double	unit_conversion;
extern int	debug;
extern char	*usage[], *cmd[];
extern mat_t	xform, notrans, identity;

extern void		abort_sig(), quit();
extern void		toc(), list_toc();
extern void		prompt();

extern char	*toc_list[];
extern char	*curr_list[];
extern int	curr_ct;
extern char	*arg_list[];
extern int	arg_ct;
extern char	*tmp_list[];
extern int	tmp_ct;

extern char	*objfile;

extern int	solfd;		extern char	st_file[];
extern int	regfd;		extern char	rt_file[];
extern int	ridfd;		extern char	id_file[];

extern int	ndir, nns, nnr;

extern int			delsol, delreg;
extern char			buff[];
extern long			savsol;
extern struct deck_ident	d_ident, idbuf;

extern jmp_buf		env;
#define EPSILON		0.0001
#define CONV_EPSILON	0.01

#ifdef BSD
#define strchr(a,b)	index(a,b)
#define strrchr(a,b)	rindex(a,b)
extern char *index(), *rindex();
#endif

extern struct db_i	*dbip;		/* Database instance ptr */
