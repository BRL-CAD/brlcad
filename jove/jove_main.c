/*
 *			J O V E _ M A I N . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.4  1995/06/21  03:42:30  gwyn
 * Eliminated trailing blanks.
 * BinShell, version, Joverc are now arrays rather than string literal pointers.
 * Changed memcpy calls back to bcopy.  (Fixed confusing use of SYSV vs. SYS5.)
 * Fixed varargs functions, using VA_* macros.
 * Fixed usage of FNDELAY, which is obsolete.
 * Improved use of conditionals dealing with system variations.
 * Made reset of SIGINT handler independent of SIGTSTP.
 *
 * Revision 11.3  95/01/04  23:26:50  mike
 * Irix 5.3 fix
 *
 * Revision 11.2  1995/01/04  20:09:00  stay
 * Fixed for Irix 5.3 which no longer declares
 * FNDELAY for POSIX_SOURCE
 *
 * Revision 11.1  1995/01/04  10:35:17  mike
 * Release_4.4
 *
 * Revision 10.13  94/12/29  23:02:59  mike
 * Bug #158.  Eliminated ^V doubling on DEC Alpha, and Solaris.
 *
 * Revision 10.12  94/12/14  15:33:54  jra
 * Cray also needs file.h included.
 *
 * Revision 10.11  94/10/14  17:29:25  mike
 * SunOS 5.2
 *
 * Revision 10.10  94/09/22  18:53:54  butler
 * Added ifdef needed for Solaris
 *
 * Revision 10.9  1994/09/17  04:57:35  butler
 * changed all calls to bcopy to be memcpy instead.  Useful for Solaris 5.2
 *
 * Revision 10.8  1993/12/10  05:50:57  mike
 * tchars should be defined only on old-style BSD systems.
 *
 * Revision 10.7  93/12/10  05:35:40  mike
 * Additional POSIX support
 *
 * Revision 10.6  93/10/26  06:32:59  mike
 * Changed printf() to jprintf() so that all modules could safely
 * use stdio.h
 *
 * Revision 10.5  93/10/26  06:01:51  mike
 * Changed getchar() to jgetchar() to prevent stdio.h conflict
 *
 * Revision 10.4  93/10/26  05:42:22  mike
 * POSIX
 *
 * Revision 10.3  93/10/26  03:38:55  mike
 * termios stuff
 *
 * Revision 10.2  93/04/01  04:25:24  mike
 * Arg to FIONREAD is an "int", not a "long" these days.
 *
 * Revision 10.1  91/10/12  06:54:00  mike
 * Release_4.0
 *
 * Revision 2.16  91/08/30  20:24:48  mike
 * Added !defined(SYS5) protection around some BSD ioctls
 *
 * Revision 2.15  91/08/30  19:28:59  mike
 * Added BRL-specific identification
 *
 * Revision 2.14  91/08/30  18:11:05  mike
 * Made explicit that termcap.h to be used is the local version
 *
 * Revision 2.13  91/08/30  17:54:35  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.12  91/08/30  17:49:09  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.11  87/09/16  21:28:40  mike
 * Hopefully, the Final Word on the SysV "poll" for input
 * code.
 *
 * Revision 2.10  87/09/10  00:23:01  mike
 * Hopefully, the fix for the "scramble the input" bug on the Crays
 * and other SysV machines.
 *
 * Revision 2.9  87/05/05  21:04:12  dpk
 * Simplified getchar code. Now all systems use the read loop
 *
 * Revision 2.8  87/04/14  20:14:21  dpk
 * Added SIGHUP fix.
 *
 * Revision 2.7  86/09/23  22:28:52  mike
 * Externs now declared properly.
 * I/O fixes for SysV
 *
 * Revision 2.7  86/09/23  22:26:46  mike
 * Externs now declared properly.
 * I/O fixes for SysV
 *
 * Revision 2.6  86/04/11  09:09:51  gwyn
 * last mod was incomplete
 *
 * Revision 2.5  86/04/06  06:28:48  gwyn
 * Fixed endless loop on joverc under System V.
 *
 * Revision 2.4  86/01/17  13:23:37  gwyn
 * fixed SYS5 terminal mode setup
 *
 * Revision 2.3  86/01/11  03:01:51  gwyn
 * fixed window size code
 *
 * Revision 2.2  85/05/14  01:44:41  dpk
 * Added changes to support System V (conditional on SYS5)
 *
 * Revision 2.1  85/01/17  23:58:24  dpk
 *
 *
 * Revision 2.0  84/12/26  16:46:45  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.3  84/03/20  22:27:17  dpk
 * Added better termcap handling
 *
 * Revision 1.2  83/12/16  00:08:44  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 4-19-83

   jove_main.c

   Contains the main loop, initializations, getch routine... */

#include "./jove.h"
#include "./termcap.h"

#include <signal.h>
#if HAS_TERMIOS
#  if !defined(_XOPEN_SOURCE)
#	define _XOPEN_SOURCE 1	/* to get TAB3, etc */
#  endif
# include <termios.h>
# include <fcntl.h>
#else
# ifndef SYS5
#  include <sgtty.h>
# else
#  include <termio.h>
#  include <fcntl.h>
# endif
#endif
#include <errno.h>

extern char	version[];
extern char	BinShell[];
extern char	Joverc[];
#if !defined(__STDC__) && !defined(_POSIX_SOURCE)	/* i.e. fairly old UNIX */
extern int	errno;
#endif

void		DoKeys();

#if defined(TIOCSLTC) && !defined(SYS5)
struct ltchars	ls1, ls2;
#endif

#if defined(TIOCSETC) && !defined(SYS5)
struct tchars	tc1, tc2;	/* only on old-style BSD */
#endif

int	errormsg;

int	iniargc;
char	**iniargv;
char	*StdShell;


/***** Global storage declarations *****/
int	BufSize;
int	origflags[NFLAGS],
	globflags[NFLAGS];
char	genbuf[LBSIZE];		/* Scatch pad */
int	peekc,
	io,		/* File descriptor for reading and writing files */
	exp,
	exp_p,
	this_cmd,
	last_cmd,
	RecDepth;
jmp_buf	mainjmp;
WINDOW	*fwind,		/* First window in list */
	*curwind;	/* Current window */
BUFFER	*world,			/* First buffer */
	*curbuf;		/* Pointer into world for current buffer */
int
	Crashing;		/* We are in the middle of crashing */

int
	Input,		/* What the current input is */
	InputPending,
 	killptr,	/* Index into killbuf */
	CanScroll,	/* Can this terminal scroll? */
	Asking;		/* Are we on read a string from the terminal? */
char	**argvp;
char	linebuf[LBSIZE];
LINE	*killbuf[NUMKILLS];	/* Array of pointers to killed stuff */
int	(*Getchar)();
struct function	*mainmap[0200],
		*pref1map[0200],
		*pref2map[0200],
		*LastFunc;
int	LastKeyStruck;
/***************************************/

void
finish(code)
{
	if (code == SIGINT) {
		char	c;

		ignorf(signal(code, finish));
		message("Quit? ");
		UpdateMesg();
		ignore(read(0, &c, 1));
		message("");
		if ((c & 0377) != 'y') {
			redisplay();
			return;
		}
	}
	if (code) {
		if (code == SIGHUP)
			ignorf(signal(code, SIG_IGN));	/* A little privacy */
		if (!Crashing) {
			putstr("Writing modified JOVE buffers...");
			Crashing++;
			exp_p = 0;
			WtModBuf();
		} else
			putstr("Complete lossage!");
	}
	ttyset(0);
	Placur(LI - 1, 0);
	putpad(CE, 1);
	if (KE)
		putpad(KE, 1);
	if (VE)
		putpad(VE, 1);
	if (TE)
		putpad(TE, 1);
	flusho();

	byebye(code);
}

#define NTOBUF	256		/* Should never get that far ahead */
static char	smbuf[NTOBUF],
		*bp = smbuf;
static int	nchars = 0;

Ungetc(c)
register int	c;
{
	if (c == EOF || nchars >= NTOBUF)
		return EOF;
	*--bp = c;
	nchars++;
	return c;
}

jgetchar()
{
	if (nchars <= 0) {
		/*
		 *  This loop will retry the read if it is interrupted
		 *  by an alarm signal.  (Like VMUNIX...)
		 */
		do {
			nchars = read(Input, smbuf, sizeof smbuf);
#ifdef SYS5
		} while ((nchars == 0 && Input == 0)
			 || (nchars < 0 && errno == EINTR));
#else
		} while (nchars < 0 && errno == EINTR);
#endif
		if(nchars <= 0) {
			if (Input)
				return EOF;
			finish(0);
		}
		bp = smbuf;
		InputPending = nchars > 1;
	}
	nchars--;
	return *bp++;
}

/* Returns non-zero if a character waiting */
charp()
{	if (Input)
		return 0;
	if (nchars > 0)
		return 1;			/* Quick check */
	else {
#ifdef BRLUNIX
		static struct sg_brl gttyBuf;

		gtty( 0, &gttyBuf );
		if( gttyBuf.sg_xflags & INWAIT )
			return( 1 );
		else
			return( 0 );
#else
#ifdef FIONREAD
		int c = 0;

		if (ioctl(0, FIONREAD, (char *) &c) == -1)
			c = 0;
#else
#if defined(SYS5) || HAS_TERMIOS
		int c, flags;

		/* Since VMIN=1, we need to be able to poll for input
		 * here.  Yes, the 4-syscalls to do it are costly,
		 * but not as costly as the infinite loop when VMIN=0!
		 * Have to be careful, as we actually read in the
		 * characters here, to see if there are any.
		 */
		flags = fcntl( Input, F_GETFL, 0);
#  if HAS_TERMIOS
		(void)fcntl( Input, F_SETFL, flags|O_NONBLOCK );
#  else
		(void)fcntl( Input, F_SETFL, flags|O_NDELAY );
#  endif
		c = &smbuf[NTOBUF] - bp - nchars;	/* space left */
		if( c < 0 )  c = 0;
		c = read(Input, bp+nchars, c );
		if (c > 0)
			nchars += c;
		(void)fcntl( Input, F_SETFL, flags );
#else
		int c;

		if (ioctl(0, TIOCEMPTY, (char *) &c) == -1)
			c = 0;
#endif
#endif
		return (c > 0);
#endif
	}
}


void
ResetTerm()
{
	if (TI)
		putpad(TI, 1);
	if (VS)
		putpad(VS, 1);
	if (KS)
		putpad(KS, 1);
	ttyset(1);
}

void
UnsetTerm()
{
	ttyset(0);
	Placur(LI - 1, 0);
	if (KE)
		putpad(KE, 1);
	if (VE)
		putpad(VE, 1);
	if (TE)
		putpad(TE, 1);
	flusho();
}

void
PauseJove()
{
#ifdef SIGTSTP
	UnsetTerm();
	ignore(kill(0, SIGTSTP));
	ResetTerm();
	ClAndRedraw();
#else
	RunShell( 0 );
	ClAndRedraw();
#endif
}

#define	DOCOMMAND	1		/* Must be non-zero */

void
SubShell()
{
	register char yorn;

	do {
		Placur(LI - 1, 0);
		putstr("\rCommand line? ");
		RunShell( DOCOMMAND );
		putstr("Continue? ");
		flusho();
		yorn = jgetchar();
	} while (yorn != '\r' && yorn != '\n' && yorn != ' '
		 && yorn != 'y' && yorn != 'Y');
	ClAndRedraw();
}

void
RunShell(prompt)
register int	prompt;
{
	register int	pid;

	UnsetTerm();

	switch (pid = fork()) {
	case -1:
		complain("Fork failed");

	case 0:
		signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);

		if( prompt == DOCOMMAND )
			execl(StdShell, "shell", "-t", 0);
		else
			execl(StdShell, "shell", 0);
		message("Execl failed");
		byebye(1);

	default:
		while (wait(0) != pid)
			;
	}

	ResetTerm();
}

int	OKXonXoff = 0;		/* ^S and ^Q initially DON'T work */

#ifndef	BRLUNIX
# if HAS_TERMIOS
struct termios	oldtty, newtty;
# else
#  if defined(SYS5)
struct termio	oldtty, newtty;
#  else
struct sgttyb	oldtty, newtty;
#  endif
# endif
#else
struct sg_brl	oldtty, newtty;
#endif

void
ttsetup() {
#ifndef BRLUNIX
#  if HAS_TERMIOS
	if (tcgetattr( 0, &oldtty ) < 0) {
#  else
#    if defined(SYS5)
	if (ioctl (0, TCGETA, &oldtty) < 0) {
#    else
	if (ioctl(0, TIOCGETP, (char *) &oldtty) == -1) {
#    endif
#  endif	/* HAS_TERMIOS */
#else
	if (gtty(0, &oldtty) < 0) {
#endif
		putstr("Input not a terminal");
		byebye(1);
	}

	/* One time setup of "raw mode" stty struct */
	newtty = oldtty;
#ifndef BRLUNIX
#if !defined(SYS5) && !HAS_TERMIOS
	newtty.sg_flags &= ~(ECHO | CRMOD);
	newtty.sg_flags |= CBREAK;
#else
	newtty.c_iflag &= ~(INLCR|ICRNL);
	newtty.c_lflag &= ~(ISIG|ICANON|ECHO);

	/* Not all exist in POSIX.  If not there, no need to clear them. */
#	if defined(OLCUC)
		newtty.c_oflag &= ~(OLCUC);
#	endif
#	if defined(ONLCR)
		newtty.c_oflag &= ~(ONLCR);
#	endif
#	if defined(OCRNL)
		newtty.c_oflag &= ~(OCRNL);
#	endif
#	if defined(ONOCR)
		newtty.c_oflag &= ~(ONOCR);
#	endif
#	if defined(ONLRET)
		newtty.c_oflag &= ~(ONLRET);
#	endif
#	if defined(OFILL)
		newtty.c_oflag &= ~(OFILL);
#	endif

#	if defined(IEXTEN)
		newtty.c_lflag &= ~(IEXTEN);	/* (Solaris) Eliminate ^V doubling */
#	endif

	/* VMIN = 0 causes us to loop in jgetchar() */
	newtty.c_cc[VMIN] = 1;
	newtty.c_cc[VTIME] = 0;
#	if defined(VLNEXT)
		newtty.c_cc[VLNEXT] = 0;	/* (Alpha) Eliminate ^V doubling */
#	endif
#	if defined(LNEXT)
		newtty.c_cc[LNEXT] = 0;		/* (Solaris) Eliminate ^V doubling */
#	endif
#endif
#else
	newtty.sg_flags &= ~(ECHO | CRMOD);
	newtty.sg_flags |= CBREAK;

	/* VT100 Kludge: leave STALL on for flow control if DC3DC1 (Yuck.) */
	newtty.sg_xflags &= ~((newtty.sg_xflags&DC3DC1 ? 0 : STALL) | PAGE);
#endif
}

void
ReInitTTY()
{
	ttyset(0);	/* Back to original settings */
	ttinit();
}

void
ttsize()
{
#ifdef TIOCGWINSZ
	struct winsize win;

	if (ioctl (0, TIOCGWINSZ, &win) == 0) {
		if (win.ws_col)
			CO = win.ws_col;
		if (win.ws_row)
			LI = win.ws_row;
	}
#else
#ifdef BTL_BLIT
#include <sys/jioctl.h>
	struct jwinsize jwin;

	if (ioctl (0, JWINSIZE, &jwin) == 0) {
		if (jwin.bytesx)
			CO = jwin.bytesx;
		if (jwin.bytesy)
			LI = jwin.bytesy;
	}
#endif
#endif
}

void
ttinit()
{
#if defined(TIOCSLTC) && !defined(SYS5)
	ioctl(0, TIOCGLTC, (char *) &ls1);
	ls2 = ls1;
	ls2.t_suspc = (char) -1;
	ls2.t_dsuspc = (char) -1;
	ls2.t_flushc = (char) -1;
	ls2.t_lnextc = (char) -1;
#endif

	/* Change interrupt and quit. */
#if defined(TIOCSETC) && !defined(SYS5)
	ioctl(0, TIOCGETC, (char *) &tc1);
	tc2 = tc1;
	tc2.t_intrc = (char) -1;
	tc2.t_quitc = (char) -1;
	if (OKXonXoff) {
		tc2.t_stopc = (char) -1;
		tc2.t_startc = (char) -1;
	}
#endif
	ttyset(1);

	/* Go into cbreak, and -echo and -CRMOD */

	ignorf(signal(SIGHUP, finish));
	ignorf(signal(SIGINT, SIG_IGN));
	ignorf(signal(SIGQUIT, SIG_IGN));
#if defined(SIGBUS)
	ignorf(signal(SIGBUS, finish));
#endif
	ignorf(signal(SIGSEGV, finish));
	ignorf(signal(SIGPIPE, finish));
	ignorf(signal(SIGTERM, SIG_IGN));
}

/* If "n" is zero, reset to orignal modes */
#ifndef BRLUNIX
void
ttyset(n)
{
#if HAS_TERMIOS
	struct termios	tty;
#else
# if defined(SYS5)
	struct termio	tty;
# else
	struct sgttyb	tty;
# endif
#endif

	if (n)
		tty = newtty;
	else
		tty = oldtty;

#if HAS_TERMIOS
	if (tcsetattr( 0, TCSANOW, &tty) == -1)
#else
#  if !defined(SYS5)
	if (ioctl(0, TIOCSETN, (char *) &tty) == -1)
#  else
	if (ioctl (0, TCSETAW, (char *) &tty) == -1)
#  endif
#endif
	{
		putstr("ioctl error?");
		byebye(1);
	}
#if defined(TIOCSETC) && !defined(SYS5)
	ioctl(0, TIOCSETC, n == 0 ? (char *) &tc1 : (char *) &tc2);
#endif
#if defined(TIOCSLTC) && !defined(SYS5)
	ioctl(0, TIOCSLTC, n == 0 ? (char *) &ls1 : (char *) &ls2);
#endif
}
#else	/* BRLUNIX */
void
ttyset(n)
{
	struct sg_brl	tty;

	if (n)
		tty = newtty;
	else
		tty = oldtty;

	if (stty(0, (char *) &tty) < 0)
	{
		putstr("stty error?");
		byebye(1);
	}
}
#endif

/* NOSTRICT */

char *
emalloc(size)
{
	register char	*ptr;

	if (ptr = malloc((unsigned) size))  {
		return ptr;
	}
	GCchunks();
	if (ptr = malloc((unsigned) size))  {
		return ptr;
	}
	error("out of memory");
	/* NOTREACHED */
	return (char *)NULL;
}

void
dispatch(c)
register int	c;
{
	register struct function	*fp;

	fp = mainmap[c];
	if (fp == 0) {
		rbell();
		exp = 1;
		exp_p = errormsg = 0;
		message("");
		return;
	}
	ExecFunc(fp, 0);
}

int
getch()
{
	register int	c;

	if (stackp >= 0 && macstack[stackp]->Flags & EXECUTE)
		c = MacGetc();
	else {
		redisplay();
		if ((c = jgetchar()) == EOF)  {
			finish(SIGHUP);
		}
		c &= 0177;
		if (KeyMacro.Flags & DEFINE)
			MacPutc(c);
	}
	LastKeyStruck = c;
	return c;
}

void
parse(argc, argv)
register char	*argv[];
{
	BUFFER	*firstbuf = 0;
	BUFFER	*secondbuf = 0;
	register char	c;

	s_mess("Jonathan's Own Version of Emacs  (ARL %s)", version);

	*argv = (char *) 0;
	argvp = argv + 1;

	while (argc > 1) {
		if (argv[1][0] == '-') {
			if (argv[1][1] == 't') {
				++argv;
				--argc;
				exp_p = 1;
				find_tag(argv[1]);
			}
		} else if (argv[1][0] == '+' &&
					(c = argv[1][1]) >= '0' && c <= '9') {
			++argv;
			--argc;
			SetBuf(do_find(curwind, argv[1]));
			SetLine(next_line(curline, atoi(&argv[0][1]) - 1));
		} else {
			SetBuf(do_find(curwind, argv[1]));
		}

		if (!firstbuf)
			firstbuf = curbuf;
		else if (!secondbuf)
			secondbuf = curbuf;

		++argv;
		--argc;
	}

	if (secondbuf) {
		curwind = div_wind(curwind);
		SetBuf(secondbuf);
		PrevWindow();
	}
	if (firstbuf)
		SetBuf(firstbuf);
}

#ifdef lint
Ignore(a)
	char *a;
{

	a = a;
}

Ignorf(a)
	int (*a)();
{

	a = a;
}
#endif

/* VARARGS */

void
error(VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char *fmt)
	VA_LIST(ap)

	VA_START(ap, fmt)
	VA_I(ap, char *, fmt)
	if (fmt) {
		vsprintf(mesgbuf, fmt, ap);
		UpdMesg++;
	}
	VA_END(ap)
	rbell();
	longjmp(mainjmp, ERROR);
}

/* VARARGS */

void
complain(VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char *fmt)
	VA_LIST(ap)

	VA_START(ap, fmt)
	VA_I(ap, char *, fmt)
	if (fmt) {
		vsprintf(mesgbuf, fmt, ap);
		UpdMesg++;
	}
	VA_END(ap)
	rbell();	/* Always ring the bell now */
	longjmp(mainjmp, COMPLAIN);
}

/* VARARGS */

void
confirm(VA_T(const char *fmt) VA_ALIST)
	VA_DCL
{
	VA_D(char *fmt)
	VA_LIST(ap)
	char *yorn;

	VA_START(ap, fmt)
	VA_I(ap, char *, fmt)
	vsprintf(mesgbuf, fmt, ap);
	VA_END(ap)
	yorn = ask((char *)0, mesgbuf);
	if (*yorn != 'Y' && *yorn != 'y')
		longjmp(mainjmp, COMPLAIN);
}

void
byebye(status)
{
	flusho();
	exit(status);
}

void
Recurse()
{
	RecDepth++;
	DoKeys(0);
	RecDepth--;
}

read_ch()
{
	int	c;

	if ((c = peekc) != -1) {
		peekc = -1;
		return c;
	}
	return getch();
}

void
DoKeys(first)
{
	register int	c;
	jmp_buf	savejmp;

	bcopy((char *) mainjmp, (char *) savejmp, sizeof savejmp);

	switch( setjmp(mainjmp) ) {
	case 0:
		if (first)
			parse(iniargc, iniargv);
		break;

	case QUIT:
		bcopy((char *) savejmp, (char *) mainjmp, sizeof mainjmp);
		return;

	case ERROR:
		getDOT();	/* God knows what state linebuf was in */

	case COMPLAIN:
		IOclose();
		Getchar = getch;
		if (Input) {
			ignore(close(Input));
			Input = 0;	/* Terminal has control now */
		}
		errormsg++;
		FixMacros();
		Asking = 0;		/* Not anymore we ain't */
		redisplay();
		break;
	}

	this_cmd = last_cmd = 0;

	for (;;) {
		exp = 1;
		exp_p = 0;
		last_cmd = this_cmd;
cont:
		this_cmd = 0;
		c = read_ch();
		if (c == -1)
			continue;
		else if (c < -1)
			break;
	 	dispatch(c);
		if (this_cmd == ARG_CMD)
			goto cont;
	}
}


main(argc, argv)
char	*argv[];
{
	extern char searchbuf[];
	char	*home;

	peekc = -1;
	killptr = errormsg = 0;
	curbuf = world = (BUFFER *) 0;

	iniargc = argc;
	iniargv = argv;

	searchbuf[0] = '\0';
	InputPending = 0;
	Asking = 0;
	Crashing = 0;
	Input = 0;		/* Terminal? */
	RecDepth = 0;		/* Top level */
	Getchar = getch;

	if( setjmp(mainjmp) )  {
		jprintf("Pre-error: \"%s\"; tell system support\n", mesgbuf);
		finish(0);
	}

	getTERM();
	ttsize();
	ttsetup();
	InitCM();
	CanScroll = ((AL && DL) || CS);
	settout();
	make_scr();	/* Do this before making zero, allocates memory */
	tmpinit();	/* Init temp file */
	MacInit();	/* Initialize Macros */
	InitFuncs();	/* Initialize functions and variables */
	InitBindings();	/* Everyday EMACS commands */
	winit();	/* Initialize window */

	noflags(origflags);
	curbuf = do_select(curwind, Mainbuf);

	if ((StdShell = getenv("SHELL")) == 0)
		StdShell = BinShell;
	ignore(joverc(Joverc));
	if (home = getenv("HOME"))
		ignore(joverc(sprint("%s/.joverc", home)));

	ttinit();	/* Initialize terminal (after ~/.joverc) */

	if (TI)
		putpad(TI, 1);
	if (VS)
		putpad(VS, 1);
	if (KS)
		putpad(KS, 1);
	putpad(CL, 1);

	bcopy(curbuf->b_flags, origflags, NFLAGS*sizeof(int));
	/* All new buffers will have these flags when created. */
	RedrawDisplay();	/* Start the redisplay process */
	DoKeys(1);
	finish(0);
}
