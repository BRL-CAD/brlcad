/*
 *			J O V E . H 
 *
 * $Header$
 *
 */
/* jove.h header file to be included by EVERYONE */
#include <stdio.h>

#ifdef SYSV
# define SYS5	1
#endif

#ifdef SYSV
#	define	USE_STRING_H	1	/* use <string.h>, not <strings.h> */
#endif

/*
 *  Some very common BSD --> SYSV conversion aids
 */
#if defined(SYSV) && !defined(bzero)
#	define bzero(str,n)		memset( str, '\0', n )
#	define bcopy(from,to,count)	memcpy( to, from, count )
#endif

#if defined(BSD) && !defined(SYSV) && (BSD <= 43)
#	define strchr(sp,c)	index(sp,c)
#	define strrchr(sp,c)	rindex(sp,c)
	extern char *index();
	extern char *rindex();
#endif

#if defined(__STDC__) || (BSD >= 44) || defined(_POSIX_SOURCE)
#	define HAS_TERMIOS	1
#endif

#include <setjmp.h>

#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#ifdef __convex__
#define tempnam(x, y) tmpnam(x)		/* No tempnam()! */
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
# define NULL	0
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

#define RMARGIN		72	/* Default right margin */

#define flusho()	flushout(-1, &termout)
#define Placur(l, c)	if (l != CapLine || c != CapCol) DoPlacur(l, c) 

#define CTL(c)		(   c & 037)
#define META(c)		(   c | 0200)

#define	rbell()		(RingBell++)
#define	eolp()		(linebuf[curchar] == '\0')
#define bolp()		(curchar == 0)
#define	lastp(line)	(line == curbuf->b_dol)
#define	firstp(line)	(line == curbuf->b_zero)
#define eobp()		(lastp(curline) && eolp())
#define bobp()		(firstp(curline) && bolp())
#define HALF(wp)	((wp->w_height - 1) / 2)
#define SIZE(wp)	(wp->w_height - 1)
#define makedirty(line)	line->l_dline |= DIRTY
#define IsModified(b)	(b->b_status & B_MODIFIED)
#define	IsScratch(b)	(b->b_status & B_SCRATCH)
#define	SetScratch(b)	(b->b_status |= B_SCRATCH)
#define	ClrScratch(b)	(b->b_status &= ~B_SCRATCH)
#define	IsReadOnly(b)	(b->b_status & B_READONLY)
#define	SetReadOnly(b)	(b->b_status |= B_READONLY);
#define	ClrReadOnly(b)	(b->b_status &= ~B_READONLY);

#define DoTimes(f, n)	exp_p = 1, exp = n, f()

extern int	BufSize;
extern int	CheckTime,
		errormsg;

/* This procedure allows redisplay to be aborted if the buffer is
 * ready to be flushed, and there are some characters waiting
 */

#define outchar(c) Putc(c, (&termout))
#define Putc(x,p) (--(p)->io_cnt>=0 ? ((int)(*(p)->io_ptr++=(unsigned)(x))):flushout(x, p))

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
	exp,
	exp_p,
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

extern long	lseek();

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
	*index(),
	*ask(),
	*RunEdit(),
	*getline(),
	*getblock(),
	*malloc(),
	*emalloc(),
	*mktemp(),
	*place(),
	*realloc(),
	*getright(),
	*getcptr(),
	*rindex(),
	*getenv(),
	*tgoto(),
	*sprint(),
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

#define	SAVE_NO		0
#define	SAVE_ASK	1
#define	SAVE_ALWAYS	2

#ifdef CRAY2
/* Common SYSV definitions not supported on the CRAY2 */
#define SIGIOT  SIGHWE
#define SIGBUS  SIGPRE
#define SIGSEGV SIGORE

#define VQUIT   1
#define VERASE  2
#define VKILL   3
#define VMIN    4
#define VTIME   5
#define	INLCR	0000100
#define ICRNL   0000400
#define IUCLC   0001000
#define	OLCUC	0000002
#define	ONLCR	0000004
#define	OCRNL	0000010
#define	ONOCR	0000020
#define	ONLRET	0000040
#define	OFILL	0000100
#define TABDLY  0014000
#define TAB3    0014000
#define CBAUD   0000017

#define	ISIG	0000001		/* line disc. 0 modes */
#define TCSETAW TCSETA
#endif
