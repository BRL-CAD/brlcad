/*
 *			J O V E . H
 *
 * $Header$
 *
 */

#include "common.h"

#ifdef _GNU_SOURCE
#  undef _GNU_SOURCE
#endif

/* jove.h header file to be included by EVERYONE */
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#else
#  include <string.h>
#endif

#ifdef SYSV
#  define SYS5	1
#endif


/*
 *  DAG's portable support for functions taking variable number of arguments
 *
 *  Usage example:
 *
 *	extern int Advise(VA_T(const char *) VA_T(int) VA_DOTS);
 *
 *	#include <stdio.h>
 *	int Advise(VA_T(const char *format) VA_T(int verbose) VA_ALIST)
 *		VA_DCL
 *	{
 *		VA_D(char *format)		// note absence of "const"
 *		VA_D(int verbose)
 *		VA_LIST(ap)
 *		int verbosity, status;
 *		// end of declarations
 *		VA_START(ap, verbose)
 *		VA_I(ap, char *, format)	// note absence of "const"
 *		VA_I(ap, int, verbose)
 *		verbosity = verbose? VA_ARG(ap, int): 0;
 *		status = verbosity > 0? vfprintf(stderr, format, ap) > 0: 1;
 *		VA_END(ap)
 *		return status;
 *	}
 */
#ifdef HAVE_STDARG_H
#  include <stdarg.h>
#  define	VA_T(t)		t,
#  define	VA_DOTS		...
#  define	VA_ALIST	...
#  define	VA_D(d)         /* nothing */
#  define	VA_DCL		/* nothing */
#  define	VA_LIST(ap)	va_list ap;
#  define	VA_START(ap,A0)	va_start(ap,A0);
#  define	VA_I(ap,T,Ai)	/* nothing */
#  define	VA_ARG(ap,T)	va_arg(ap,T)
#  define	VA_END(ap)	va_end(ap);
#else
#  include <varargs.h>
#  define	VA_T(t)		/* nothing */
#  define	VA_DOTS		/* nothing */
#  define	VA_ALIST	va_alist
#  define	VA_D(d)		d;
#  define	VA_DCL		va_dcl
#  define	VA_LIST(ap)	va_list ap;
#  define	VA_START(ap,A0)	va_start(ap);
#  define	VA_I(ap,T,Ai)	Ai = va_arg(ap,T);
#  define	VA_ARG(ap,T)	va_arg(ap,T)
#  define	VA_END(ap)	va_end(ap);
#endif

#ifdef __convex__
#  define	HAS_TEMPNAM	0	/* No tempnam()! */
#endif

#ifndef	HAS_TEMPNAM
#  ifdef	L_tmpnam		/* From modern stdio.h */
#    define	HAS_TEMPNAM	1
#  else
#    define	HAS_TEMPNAM	0
#  endif
#endif

#if HAS_TEMPNAM
extern char	*tempnam();
#endif

/* The following is tailorable */
#if !defined(pdp11)

typedef	long	disk_line;
#define BSIZ	4096

#else

typedef	short	disk_line;
#define BSIZ	512

#endif

#ifndef NULL
#  define NULL	0
#endif

#define OKAY	0		/* Return codes for when telling */
#define ABORT	1
#define STOP	2

#define LOGOEXIT	30	/* Sounds like a nice number.  Exit non-zero,
				 * but finish doesn't crash with a core dump.
				 * Logo checks for non-zero to check to see
				 * if it should abort the editing
				 */

#define NFUNCS		130
#define NMACROS		52	/* One for each letter???? VI mode for JF */
#define NVARS		20

#define FUNCTION	1
#define VARIABLE	2
#define MACRO		3

#define DEFINE		1
#define EXECUTE		2

#define	LBSIZE		BSIZ	/* Same as a logical disk block */
#define	ESIZE		128
#define	GBSIZE		256
#define	ALSIZE		(LBSIZE / sizeof(char *))	/* argument list max */

#define RMARGIN		72	/* Default right margin */

#define flusho()	flushout(-1, &termout)
#define Placur(l, c)	if ((l) != CapLine || (c) != CapCol) DoPlacur(l, c)

#define CTL(c)		((c) & 037)
#define META(c)		((c) | 0200)

#define	rbell()		(RingBell++)
#define	eolp()		(linebuf[curchar] == '\0')
#define bolp()		(curchar == 0)
#define	lastp(line)	((line) == curbuf->b_dol)
#define	firstp(line)	((line) == curbuf->b_zero)
#define eobp()		(lastp(curline) && eolp())
#define bobp()		(firstp(curline) && bolp())
#define HALF(wp)	(((wp)->w_height - 1) / 2)
#define SIZE(wp)	((wp)->w_height - 1)
#define makedirty(line)	line->l_dline |= DIRTY
#define IsModified(b)	((b)->b_status & B_MODIFIED)
#define	IsScratch(b)	((b)->b_status & B_SCRATCH)
#define	SetScratch(b)	((b)->b_status |= B_SCRATCH)
#define	ClrScratch(b)	((b)->b_status &= ~B_SCRATCH)
#define	IsReadOnly(b)	((b)->b_status & B_READONLY)
#define	SetReadOnly(b)	((b)->b_status |= B_READONLY);
#define	ClrReadOnly(b)	((b)->b_status &= ~B_READONLY);

#define DoTimes(f, n)	jove_exp_p = 1, jove_exp = (n), f()

extern int	BufSize;
extern int	CheckTime,
		errormsg;

/* This procedure allows redisplay to be aborted if the buffer is
 * ready to be flushed, and there are some characters waiting
 */

#define outchar(c) Putc(c, (&termout))
/* #define Putc(x,p) (--(p)->io_cnt>=0 ?  ((void)(*(p)->io_ptr++=(unsigned)(x))):flushout(x, p) ) */
#define Putc(x,p)	{ if( --(p)->io_cnt >= 0 ) \
				(*(p)->io_ptr++=(unsigned)(x)); \
			  else \
				flushout(x, p); \
			}

/*
 * C doesn't have a (void) cast, so we have to fake it for lint's sake.
 */
#ifdef lint
#	define	ignore(a)	Ignore((char *) (a))
#	define	ignorf(a)	Ignorf((int (*) ()) (a))
#else
#	define	ignore(a)	a
#	define	ignorf(a)	a
#endif

#define ARG_CMD		1
#define LINE_CMD	2
#define KILLCMD		3	/* So we can merge kills */
#define YANKCMD		4

/* Buffer type */

#define SCRATCHBUF	1
#define NORMALBUF	2

#define OnFlag(flags,f)		(flags[f] = 1)
#define OffFlag(flags,f)	(flags[f] = 0)
#define	IsFlagSet(flags,f)	(flags[f] > 0)	/* negative disables */
#define IsDisabled(flags,f)	(flags[f] < 0)

/* Buffer flags */

#define TEXTFILL	0	/* Text fill mode */
#define OVERWRITE	1	/* Over write mode */
#define CMODE		2	/* C mode */
#define MAGIC		3	/* If set allow pattern matching searching */
#define CASEIND		4	/* Case independent search */
#define MATCHING	5	/* In show matching mode */
#define AUTOIND		6	/* Indent same as previous line after return */
#define NFLAGS		7	/* DON'T FORGET THIS! */

extern int	origflags[NFLAGS],
	globflags[NFLAGS];

#define FIRSTCALL	0
#define ERROR		1
#define COMPLAIN	2	/* Do the error without a getDOT */
#define QUIT		3	/* Leave this level of recusion */

extern	char	*Mainbuf;
extern	int	RingBell;

extern char	genbuf[LBSIZE];		/* Scatch pad */
extern int	peekc,
	io,		/* File descriptor for reading and writing files */
	jove_exp,
	jove_exp_p,
	this_cmd,
	last_cmd,
	RecDepth;

#define	READ	0
#define	WRITE	1

extern jmp_buf	mainjmp;

typedef struct window	WINDOW;
typedef struct position	BUFLOC;
typedef struct mark	MARK;
typedef struct buffer	BUFFER;
typedef struct line	LINE;
typedef struct iobuf	IOBUF;

typedef int	(*FUNC)();

/* Pseudo-Standary library struct _iobuf */
struct iobuf {
	char	*io_ptr;
	int	io_cnt;
	char	*io_base;
	short	io_flag;
	char	io_file;
};

struct line {
	LINE	*l_prev,	/* Pointer to prev */
		*l_next;	/* Pointer to next */
	disk_line	l_dline;	/* Pointer to disk location */
};

struct window {
	WINDOW	*w_prev,	/* Circular list */
		*w_next;
	BUFFER	*w_bufp;	/* Buffer associated with this window */
	LINE	*w_top,		/* The top line */
		*w_line;	/* The current line */
	int	w_char,
		w_height,	/* Height of the window */
		w_topnum,	/* Line number of the topline */
		w_offset,	/* Printing offset for the current line */
		w_numlines,	/* Should we number lines in this window? */
		w_dotcol,	/* find_pos sets this */
		w_dotline,	/* UpdateWindow sets this */
		w_flags;
};

extern WINDOW	*fwind,		/* First window in list */
	*curwind;	/* Current window */

struct position {
	LINE	*p_line;	/* In the array */
	int	p_char;		/* Char pos */
};

struct mark {
	LINE	*m_line;
	int	m_char;		/* Char pos of the mark */
	MARK	*m_next;	/* List of marks */
};

struct buffer {
	char	*b_name,		/* Buffer name */
		*b_fname;		/* File name associated with buffer */
	int	b_ino;			/* Inode of file name */
	int	b_dev;			/* Device on which inode lives */
	LINE	*b_zero,		/* Pointer to first line in list */
		*b_dot,			/* Current line */
		*b_dol;			/* Last line in list */
	int	b_char;			/* Current character in line */

#define NMARKS	16			/* Number of marks in the ring */

	MARK	*b_markring[NMARKS],	/* New marks are pushed saved here */
		*b_marks;		/* All the marks for this buffer */
	int	b_themark;		/* Current mark */
	char	b_status;		/* Status flags (B_MODIFIED, etc.) */
#define	B_MODIFIED	01		/* Buffer has been modified */
#define	B_READONLY	02		/* File is readonly */
#define	B_SCRATCH	04		/* Buffer is a scratch buffer */

	int	b_flags[NFLAGS];
	BUFFER	*b_next;		/* Next buffer in chain */
};

extern BUFFER	*world,			/* First buffer */
	*curbuf;		/* Pointer into world for current buffer */

#define NUMKILLS	10	/* Number of kills saved in the kill ring */

#define DIRTY		01
#define MODELINE	02

struct scrimage {
	int	StartCol,	/* Physical column start */
		Sflags;		/* DIRTY or MODELINE */
	LINE	*Line;		/* Which buffer line */
	WINDOW	*Window;	/* Window that contains this line */
};

extern struct scrimage
	*nimage,
	*oimage;		/* See jove_screen.c */

extern IOBUF	termout;

extern int
	OKXonXoff,		/* Disable start/stop characters */
	VisBell,
	phystab,
	tabstop,		/* Expand tabs to this number of spaces */
	RMargin,		/* Right margin */
	MakeAll,		/* Should we make -k or just make */
	ScrollStep,		/* How should we scroll */
	Crashing,		/* We are in the middle of crashing */
	WtOnMk,			/* Write files on compile-it command */
	UseBuffers,		/* Use a buffer during list-buffers */
	BackupFiles,		/* Controls creation of backup files */
	FastPrompt,		/* Have C-X and M- prompt immediatly */
	EndWNewline;		/* End files with a blank line */

extern int	InputPending,	/* Non-zero if there is input waiting to
			   be processed. */
	Input,		/* What the current input is */
 	killptr,	/* Index into killbuf */
	CanScroll,	/* Can this terminal scroll? */
	Asking;		/* Are we on read a string from the terminal? */

extern char	**argvp;

#define curline		curbuf->b_dot
#define curchar		curbuf->b_char
#define curmark		curbuf->b_markring[curbuf->b_themark]

extern char	linebuf[LBSIZE];
extern LINE	*killbuf[NUMKILLS];	/* Array of pointers to killed stuff */

extern char	mesgbuf[];

struct screenline {
	char	*s_line,
		*s_length;
};			/* The screen */

struct macro {
	int	MacLength,	/* Length of macro so we can use ^@ */
		MacBuflen,	/* Memory allocated for it */
		Offset,		/* Index into body for defining and running */
		Flags,		/* Defining/running this macro? */
		Ntimes;		/* Number of times to run this macro */
	char	*Name;		/* Pointer to macro's name */
	char	*Body;		/* Actual body of the macro */
};

struct function {
	char	*f_name;
	union {
		int	(*Func)();	/* This is a built in function */
		int	*Var;		/* Variable */
		struct macro	*Macro;	/* Macro */
	} f;
	char	f_type;
};

extern int	(*Getchar)();

extern struct function	*mainmap[0200],
		*pref1map[0200],
		*pref2map[0200],
		*LastFunc;

extern int	LastKeyStruck;

extern struct function	functions[],
			macros[],
			variables[];

extern struct macro	*macstack[],
			KeyMacro;
extern int	stackp;

extern int	CapLine,
		CapCol;

extern int	UpdModLine,	/* Whether we want to update the mode line */
		UpdMesg;	/* Update the message line */

/* Under BSDI's BSD/OS lseek returns an off_t
 * which is a quad_t or "long long".
 * It is also delcared in <sys/types.h>
 */
#if defined(BSD) && BSD <= 43
extern long	lseek();
#endif

extern disk_line	putline();

extern LINE
	*next_line(),
	*prev_line(),
	*nbufline(),
	*reg_delete(),
	*lastline(),
	*listput();

extern char
	*FuncName(),
	*filename(),
	*getblock(),
	*IOerr(),
	*bufmod(),
#if !defined(__sp3__)
	*index(),
#endif
	*RunEdit(),
	*getline(),
	*getblock(),
	*emalloc(),
	*mktemp(),
	*place(),
	*getright(),
	*getcptr(),
#if !defined(__sp3__)
	*rindex(),
#endif
	*getenv(),
	*tgoto(),
	*StrIndex();

extern BUFLOC
	*dosearch(),
	*DoYank(),
	*m_paren();

extern MARK	*CurMark(),
		*MakeMark();

extern WINDOW
	*windlook(),
	*next_wind(),
	*div_wind();

extern BUFFER
	*do_find(),
	*do_select(),
	*mak_buf(),
	*buf_exists(),
	*file_exists();

extern int	jgetchar();

extern char	*ask(VA_T(char *) VA_T(const char *) VA_DOTS);
extern void	error(VA_T(const char *) VA_DOTS);
extern void	complain(VA_T(const char *) VA_DOTS);
extern void	confirm(VA_T(const char *) VA_DOTS);
extern char	*sprint(VA_T(const char *) VA_DOTS);
extern void	s_mess(VA_T(const char *) VA_DOTS);
extern void	jprintf(VA_T(const char *) VA_DOTS);
extern int	UnixToBuf(VA_T(char *) VA_T(int) VA_T(int) VA_T(const char *)
			VA_DOTS);

extern void b_format(),
	tiewind(),
	set_ino(),
	setfname(),
	setbname(),
	FindMatch(),
	del_char(),
	InsChar(),
	DelChar(),
	do_cl_eol(),
	UpdateLine(),
	CalcScroll(),
	CalcTop(),
	UpdateMesg(),
	Beep(),
	ReadMacs(),
	WriteMacs(),
	SetMacro(),
	MacNolen(),
	NameMac(),	
	DoMacro(),
	ExecMacro(),
	Forget(),
	Remember(),
	InitBindings(),
	lfreereg(),
	OpenLine(),
	Insert(),
	insert(),
	whitesp(),
	byebye(),
	ttyset(),
	ttinit(),
	RunShell(),
	PtToMark(),
	MarkSet(),
	DoDelMark(),
	CaseReg(),
	case_reg(),
	lower(),
	upper(),
	case_word(),
	SetDot(),
	DotTo(),
	Eol(),
	down_line(),
	OutOfBounds(),
	PrevLine(),
	NextLine(),
	BackChar(),
	ForChar(),
	ArgIns(),
	SetLine(),
	DoJustify(),
	dword(),
	RegToUnix(),
	com_finish(),
	DoShell(),
	DoNextErr(),
	NextError(),
	ErrFree(),
	ErrParse(),
	IncSearch(),
	dosub(),
	compsub(),
	substitute(),
	search(),
	replace(),
	UpMotion(),
	DownMotion(),
	BackMotion(),
	ForMotion(),
	ForTab(),
	flushout(),
	TERMINFOfix(),
	zero_wind(),
	SetTop(),
	WindSize(),
	pop_wind(),
	v_clear();

#define	SAVE_NO		0
#define	SAVE_ASK	1
#define	SAVE_ALWAYS	2

#ifdef CRAY2
/* Common SYSV definitions not supported on the CRAY2 */
#  define SIGBUS  SIGPRE
#  define SIGSEGV SIGORE

#  define VQUIT   1
#  define VERASE  2
#  define VKILL   3
#  define VMIN    4
#  define VTIME   5
#  define INLCR	0000100
#  define ICRNL 0000400
#  define IUCLC 0001000
#  define OLCUC	0000002
#  define ONLCR	0000004
#  define OCRNL	0000010
#  define ONOCR	0000020
#  define ONLRET	0000040
#  define OFILL	0000100
#  define TABDLY  0014000
#  define TAB3  0014000
#  define CBAUD 0000017
#  define ISIG	0000001		/* line disc. 0 modes */
#endif
