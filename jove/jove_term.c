/*
 *			J O V E _ T E R M . C 
 *
 * $Revision$
 *
 * $Log$
 * Revision 2.3  88/03/10  05:25:30  phil
 * ignore ll if li != winsize
 * 
 * Revision 2.2  87/04/14  21:58:07  dpk
 * Added TERMINFO fix for sys5.  Strips %p# strings.
 * 
 * Revision 2.1  85/05/14  01:44:34  dpk
 * Added changes to support System V (conditional on SYS5)
 * 
 * Revision 2.0  84/12/26  16:50:32  dpk
 * Version as sent to Berkeley 26 December 84
 * 
 * Revision 1.5  84/11/07  20:57:18  dpk
 * Changed  code that handles initializing of scrolling regions.
 * The implied default of newline for  scroll up is not supported.
 * 
 * Revision 1.4  84/10/03  21:51:40  dpk
 * Added code to disable use of scrolling regions if you dont have
 * both SR and SF (and CS for that matter).
 * 
 * Revision 1.3  84/10/03  21:48:13  dpk
 * Numerous bug fixes/enhancements.
 * 
 * Revision 1.2  83/12/16  00:09:50  dpk
 * Added distinctive RCS header
 * 
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_term.c

   Gets the termcap information and complains if there are not enough
   of the basic features on the particular terminal. */

#include "jove.h"
#ifndef SYS5
#include <sgtty.h>
#else
#include <termio.h>
#endif

/* Termcap definitions */

char	*UP,	/* Move cursor up */
	*CS,	/* Change scrolling */
	*SO,	/* Start standout */
	*SE,	/* End standout */
	*CM,	/* The cursor motion string */
	*CL,	/* Clear screen */
	*CE,	/* Clear to end of line */
	*HO,	/* Home cursor */
	*AL,	/* Add line (insert line) */
	*DL,	/* Delete line */
	*VS,	/* Visual start */
	*VE,	/* Visual end */
	*KS,	/* Keypad transmit start */
	*KE,	/* Keypad transmit end */
	*TI,	/* Cursor addressing start */
	*TE,	/* Cursor addressing end */
	*IC,	/* Insert char	*/
	*DC,	/* Delete char	*/
	*IM,	/* Insert mode */
	*EI,	/* End insert mode */
	*LL,	/* Move to last line, first column of screen */
	*BC,	/* Back space */
	*M_AL,	/* Meta Add line (takes arg) */
	*M_DL,	/* Meta Delete line (takes arg) */
	*M_IC,	/* Meta Insert Char (takes arg) */
	*M_DC,	/* Meta Delete Char (takes arg) */
	*SF,	/* Scroll forward (up, for scrolling regions) */
	*SR,	/* Scroll reverse (down, for scrolling regions) */
	*VB;	/* Visual bell (e.g. a flash) */

int	LI_termcap;	/* LI as given in termcap - to see if ll makes sense */
int	LI,		/* Number of lines */
	CO,		/* Number of columns */
	TABS,		/* Whether we are in tabs mode */
	SG,		/* Number of magic cookies left by SO and SE */
	XS,		/* Wether standout is braindamaged */
	UpLen,		/* Length of the UP string */
	HomeLen,	/* Length of Home string */
	LowerLen;	/* Length of lower string */

int	BG;		/* Are we on a bitgraph? */

int ospeed;

char	tspace[256];

/* The ordering of ts and meas must agree !! */
char	*ts="vsvealdlcssosecmclcehoupbcicimdceillsfsrvbksketiteALDLICDC";
char	**meas[] = {
	&VS, &VE, &AL, &DL, &CS, &SO, &SE,
	&CM, &CL, &CE, &HO, &UP, &BC, &IC, &IM,
	&DC, &EI, &LL, &SF, &SR, &VB, &KS, &KE,
	&TI, &TE, &M_AL, &M_DL, &M_IC, &M_DC, 0
};
char	*sprint();

TermError(str)
char	*str;
{
	char	*cp;

	cp = sprint("Termcap error: %s.\n", str);
	if (write(1, cp, strlen(cp)));
	exit(1);
}

getTERM()
{
	char	*getenv();
#ifndef SYS5
	struct sgttyb tty;
#else
	struct termio tty;
#endif
	char	termbuf[32],
		*termname,
		*termp = tspace,
		tbuff[1024];
	int	i;

#ifdef SYS5
	if (ioctl (0, TCGETA, &tty))
#else
	if (gtty(0, &tty))
#endif
		TermError("ioctl fails");
#ifdef SYS5
	TABS = !((tty.c_oflag & TAB3) == TAB3);
	ospeed = tty.c_cflag & CBAUD;
#else
	TABS = !(tty.sg_flags & XTABS);
	ospeed = tty.sg_ospeed;
#endif

	termname = getenv("TERM");
	if (termname == 0 || *termname == 0) {
		static char *msg = "Enter terminal name: ";

		write(1, msg, strlen(msg));	/* Buffered IO not setup yet */
		if ((i = read(0, termbuf, sizeof(termbuf))) < 0)
			TermError("");
		termbuf[i-1] = 0;	/* Zonk the newline */
		if (termbuf[0] == 0)
			TermError("");

		termname = termbuf;
	}

	BG = strcmp(termname, "bg") == 0;	/* Kludge to help out bg scroll */

	if (tgetent(tbuff, termname) < 1)
		TermError("terminal type unknown");

	if ((CO = tgetnum("co")) == -1)
		TermError("columns?");

	if ((LI = tgetnum("li")) == -1)
		TermError("lines?");
	LI_termcap = LI;			/* save this for comparison to winsize */

	if ((SG = tgetnum("sg")) == -1)
		SG = 0;			/* Used for mode line only */

	if ((XS = tgetflag("xs")) == -1)
		XS = 0;			/* Used for mode line only */

	for (i = 0; meas[i]; i++) {
		*(meas[i]) = (char *)tgetstr(ts,&termp);
		ts += 2;
	}
#ifdef SYS5
	if (M_AL) TERMINFOfix(M_AL);
	if (M_DL) TERMINFOfix(M_DL);
	if (M_IC) TERMINFOfix(M_IC);
	if (M_DC) TERMINFOfix(M_DC);
#endif
	if (XS)
		SO = SE = 0;

	if (CS && !SR)
		CS = SR = SF = 0;

	if (CS && !SF)
		SF = "\n";

	disp_opt_init();
}

#ifdef SYS5
#include <ctype.h>
/* Find TERMINFO %p# strings in the string and kill them */
/* This is a SYS5 bug fix.   -DPK- */
TERMINFOfix(cp)
char *cp;
{
	while (*cp) {
		if (cp[0]=='%' && cp[1]=='p' && isdigit(cp[2]))
			strcpy(cp, cp+3);
		else
			cp++;
	}
}
#endif
/*
   Deals with output to the terminal, setting up the amount of characters
   to be buffered depending on the output baud rate.  Why it's in a 
   separate file I don't know ...
 */

IOBUF	termout;

outc(c)
register int	c;
{
	outchar(c);
}

/* Put a string with padding */

putpad(str, lines)
char	*str;
{
	tputs(str, lines, outc);
}

/* Flush the output, and check for more characters.  If there are
 * some, then return to main, to process them, aborting redisplay.
 */

flushout(x, p)
IOBUF	*p;
{
	register int	n;

	CheckTime = 1;
	if ((n = p->io_ptr - p->io_base) > 0) {
		ignore(write(p->io_file, p->io_base, n));
		if (p == &termout) {
			CheckTime = BufSize;
			p->io_cnt = BufSize;
		} else
			p->io_cnt = BSIZ;
		p->io_ptr = p->io_base;
	}
	if (x >= 0)
		Putc(x, p);
}

/* Determine the number of characters to buffer at each
   baud rate.  The lower the number, the quicker the
   response when new input arrives.  Of course the lower
   the number, the more prone the program is to stop in
   output.  Decide what matters most to you.
   This sets the int BufSize to the right number or chars,
   allocates the buffer, and initiaizes `termout'.  */

settout()
{
	static int speeds[] = {
		30,	/* 0	0 baud, assume network connection */
		1,	/* 50	*/
		1,	/* 75	*/
		1,	/* 110	*/
		1,	/* 134	*/
		1,	/* 150	*/
		1,	/* 200	*/
		1,	/* 300	*/
		1,	/* 600	*/
		5,	/* 1200 */
		15,	/* 1800	*/
		35,	/* 2400	*/
		100,	/* 4800	*/
		200,	/* 9600	*/
		150,	/* EXTA	(7200?) */
		200	/* EXT	(19.2?) */
	};

	termout.io_cnt = BufSize = CheckTime = speeds[ospeed] * max(LI / 24, 1);
	termout.io_base = termout.io_ptr = emalloc(BufSize);
	termout.io_flag = 0;
	termout.io_file = 1;	/* Standard output */
}
