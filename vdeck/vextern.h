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

#define MAXARG	20	/* max arguments on command line */

/* Standard flag settings.						*/
#define YES	1
#define NO	0

extern int	debug;
extern char	*usage[], *cmd[];
extern mat_t	identity;

extern void		abort_sig(), quit();
extern void		toc(), list_toc();
extern void		prompt();

extern int	curr_ct;
extern char	*arg_list[];
extern int	arg_ct;
extern int	tmp_ct;

extern char	*objfile;

extern int	solfd;		extern char	st_file[];
extern int	regfd;		extern char	rt_file[];
extern int	ridfd;		extern char	id_file[];

extern int	ndir, nns, nnr;

extern int			delsol, delreg;
extern char			buff[];
extern long			savsol;

extern jmp_buf		env;
#define EPSILON		0.0001
#define CONV_EPSILON	0.01

extern struct db_i	*dbip;		/* Database instance ptr */
