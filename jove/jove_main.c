/*
 *			J O V E _ M A I N . C 
 *
 * $Revision$
 *
 * $Log$
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

#include "jove.h"
#include "termcap.h"

#include <signal.h>
#include <sgtty.h>
#include <errno.h>

extern char	*version;
extern char	*BinShell;
extern char	*Joverc;
extern int	errno;

extern char	*sprint();

#ifndef	BRLUNIX			/* this is not found on a BRL pdp-11.	*/
#include <sys/ioctl.h>
#endif BRLUNIX

#ifdef TIOCSLTC
struct ltchars	ls1, ls2;
#endif TIOCSLTC

struct tchars	tc1, tc2;

int	errormsg;

int	iniargc;
char	**iniargv;
char	*StdShell;

finish(code)
{
	if (code == SIGINT) {
		char	c;

#ifndef SIGTSTP			/* Job stopping in other words */
		ignorf(signal(code, finish));
#endif
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

#define NTOBUF	20		/* Should never get that far ahead */
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

getchar()
{
	if (nchars <= 0) {
#ifdef JOBCONTROL
		nchars = read(Input, smbuf, sizeof smbuf);
#else
		/*
		 *  This loop will retry the read if it is interrupted
		 *  by an alarm signal.  (Like VMUNIX...)
		 */
		do {
			nchars = read(Input, smbuf, sizeof smbuf);
		} while (nchars < 0 && errno == EINTR);
#endif JOBCONTROL

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
#else BRLUNIX
#ifdef FIONREAD
		long c;

		if (ioctl(0, FIONREAD, (char *) &c) == -1)
#else
		int c;

		if (ioctl(0, TIOCEMPTY, (char *) &c) == -1)
#endif
			c = 0;
		return (c > 0);
#endif BRLUNIX
	}
}


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

SubShell()
{
	register char yorn;

	do {
		Placur(LI - 1, 0);
		putstr("\rCommand line? ");
		RunShell( DOCOMMAND );
		putstr("Continue? ");
		flusho();
		yorn = getchar();
	} while (yorn != '\r' && yorn != '\n' && yorn != ' '
		 && yorn != 'y' && yorn != 'Y');
	ClAndRedraw();
}

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
struct sgttyb	oldtty, newtty;
#else
struct sg_brl	oldtty, newtty;
#endif

ttsetup() {
#ifndef BRLUNIX
	if (ioctl(0, TIOCGETP, (char *) &oldtty) == -1) {
#else
	if (gtty(0, &oldtty) < 0) {
#endif BRLUNIX
		putstr("Input not a terminal");
		byebye(1);
	}

	/* One time setup of "raw mode" stty struct */
	newtty = oldtty;
#ifndef BRLUNIX
	newtty.sg_flags &= ~(ECHO | CRMOD);
	newtty.sg_flags |= CBREAK;
#else
	newtty.sg_flags &= ~(ECHO | CRMOD);
	newtty.sg_flags |= CBREAK;

	/* VT100 Kludge: leave STALL on for flow control if DC3DC1 (Yuck.) */
	newtty.sg_xflags &= ~((newtty.sg_xflags&DC3DC1 ? 0 : STALL) | PAGE);
#endif
}

ReInitTTY()
{
	ttyset(0);	/* Back to original settings */
	ttinit();
}

ttsize()
{
#ifdef TIOCGWINSZ
	struct winsize win;

	if (ioctl (0, TIOCGWINSZ, &win) == 0) {
		if (win.col)
			CO = win.col;
		if (win.row)
			LI = win.row;
	}
#else TIOCGWINSZ
#ifdef BTL_BLIT
#include <sys/jioctl.h>
	struct jwinsize jwin;

	if (ioctl (0, JWINSIZE, &jwin) == 0) {
		if (jwin.bytesx)
			CO = win.bytesx;
		if (jwin.bytesy)
			LI = win.bytesy;
	}
#endif BTL_BLIT
#endif TIOCGWINSZ
}

ttinit()
{
#ifdef TIOCSLTC
	ioctl(0, TIOCGLTC, (char *) &ls1);
	ls2 = ls1;
	ls2.t_suspc = (char) -1;
	ls2.t_dsuspc = (char) -1;
	ls2.t_flushc = (char) -1;
	ls2.t_lnextc = (char) -1;
#endif

	/* Change interupt and quit. */
	ioctl(0, TIOCGETC, (char *) &tc1);
	tc2 = tc1;
	tc2.t_intrc = (char) -1;
	tc2.t_quitc = (char) -1;
	if (OKXonXoff) {
		tc2.t_stopc = (char) -1;
		tc2.t_startc = (char) -1;
	}
	ttyset(1);

	/* Go into cbreak, and -echo and -CRMOD */

	ignorf(signal(SIGHUP, finish));
	ignorf(signal(SIGINT, SIG_IGN));
	ignorf(signal(SIGQUIT, SIG_IGN));
	ignorf(signal(SIGBUS, finish));
	ignorf(signal(SIGSEGV, finish));
	ignorf(signal(SIGPIPE, finish));
	ignorf(signal(SIGTERM, SIG_IGN));
}

/* If "n" is zero, reset to orignal modes */
#ifndef BRLUNIX
ttyset(n)
{
	struct sgttyb	tty;

	if (n)
		tty = newtty;
	else
		tty = oldtty;

	if (ioctl(0, TIOCSETN, (char *) &tty) == -1)
	{
		putstr("ioctl error?");
		byebye(1);
	}
	ioctl(0, TIOCSETC, n == 0 ? (char *) &tc1 : (char *) &tc2);
#ifdef TIOCSLTC
	ioctl(0, TIOCSLTC, n == 0 ? (char *) &ls1 : (char *) &ls2);
#endif
}
#else
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

	if (ptr = malloc((unsigned) size))
		return ptr;
	GCchunks();
	if (ptr = malloc((unsigned) size))
		return ptr;
	error("out of memory");
	/* NOTREACHED */
}

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

int	LastKeyStruck;

getch()
{
	register int	c;

	if (stackp >= 0 && macstack[stackp]->Flags & EXECUTE)
		c = MacGetc();
	else {
		redisplay();
		if ((c = getchar()) == EOF)
			finish(SIGHUP);
		c &= 0177;
		if (KeyMacro.Flags & DEFINE)
			MacPutc(c);
	}
	LastKeyStruck = c;
	return c;
}

parse(argc, argv)
register char	*argv[];
{
	BUFFER	*firstbuf = 0;
	BUFFER	*secondbuf = 0;
	register char	c;

	s_mess("Jonathan's Own Version of Emacs  (%s)", version);

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

/* VARARGS1 */

error(fmt, a, b, c, d)
char	*fmt;
char	*a, *b, *c, *d;
{
	if (fmt) {
		sprintf(mesgbuf, fmt, a, b, c, d);
		UpdMesg++;
	}
	rbell();
	longjmp(mainjmp, ERROR);
}

/* VARARGS1 */

complain(fmt, a, b, c, d)
char	*fmt;
char	*a, *b, *c, *d;
{
	if (fmt) {
		sprintf(mesgbuf, fmt, a, b, c, d);
		UpdMesg++;
	}
	rbell();	/* Always ring the bell now */
	longjmp(mainjmp, COMPLAIN);
}

/* VARARGS1 */

confirm(fmt, a, b, c, d)
char	*fmt;
char	*a, *b, *c, *d;
{
	char *yorn;

	sprintf(mesgbuf, fmt, a, b, c, d);
	yorn = ask((char *)0, mesgbuf);
	if (*yorn != 'Y' && *yorn != 'y')
		longjmp(mainjmp, COMPLAIN);
}

byebye(status)
{
	flusho();
	exit(status);
}

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

DoKeys(first)
{
	register int	c;
	jmp_buf	savejmp;

	bcopy((char *) mainjmp, (char *) savejmp, sizeof savejmp);

	switch (setjmp(mainjmp)) {
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
	 	dispatch(c);
		if (this_cmd == ARG_CMD)
			goto cont;
	}
}

int	Crashing = 0;

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

	if (setjmp(mainjmp)) {
		printf("Pre-error: \"%s\"; tell system support\n", mesgbuf);
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
