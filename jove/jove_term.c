/*
 *			J O V E _ T E R M . C 
 *
 * $Revision$
 *
 * $Log$
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
#include <sgtty.h>

/* Termcap definitions */

char	*UP,	/* Scroll reverse, or up */
	*CS,	/* Change scrolling */
	*SO,	/* Start standout */
	*SE,	/* End standout */
	*CM,	/* The cursor motion string */
	*CL,	/* Clear screen */
	*CE,	/* Clear to end of line */
	*HO,	/* Home cursor */
	*AL,	/* Addline (insert line) */
	*DL,	/* Delete line */
	*IS,	/* Initial start */
	*VS,	/* Visual start */
	*VE,	/* Visual end */
	*IC,	/* Insert char	*/
	*DC,	/* Delete char	*/
	*IM,	/* Insert mode */
	*EI,	/* End insert mode */
	*LL,	/* Move to last line, first column of screen */
	*BC,	/* Back space */
	*SR,
	*VB;

int	LI,		/* Number of lines */
	CO,		/* Number of columns */
	TABS,		/* Whether we are in tabs mode */
	UpLen,		/* Length of the UP string */
	HomeLen,	/* Length of Home string */
	LowerLen;	/* Length of lower string */

int	BG;		/* Are we on a bitgraph? */

int ospeed;

char	tspace[128];

char **meas[] = {
	&VS, &VE, &IS, &AL, &DL, &CS, &SO, &SE,
	&CM, &CL, &CE, &HO, &UP, &BC, &IC, &IM,
	&DC, &EI, &LL, &SR, &VB, 0
};

gets(buf)
char	*buf;
{
	buf[read(0, buf, 12) - 1] = 0;
}	

char	*sprint();

TermError(str)
char	*str;
{
	char	*cp;

	cp = sprint("Termcap error: %s\n", str);
	if (write(1, cp, strlen(cp)));
	exit(1);
}

getTERM()
{
	char	*getenv();
	struct sgttyb tty;
	char	*ts="vsveisaldlcssosecmclcehoupbcicimdceillsrvb";
	char	termbuf[13],
		*termname = 0,
		*termp = tspace,
		tbuff[BUFSIZ];
	int	i;

	if (gtty(0, &tty))
		TermError("ioctl fails");
	TABS = !(tty.sg_flags & XTABS);
	ospeed = tty.sg_ospeed;

	termname = getenv("TERM");
	if (termname == 0) {
		putstr("Enter terminal name: ");
		gets(termbuf);
		if (termbuf[0] == 0)
			TermError("");

		termname = termbuf;
	}

	BG = strcmp(termname, "bg") == 0;	/* Kludge to help out bg scroll */

	if (tgetent(tbuff, termname) < 1)
		TermError("terminal type?");

	if ((CO = tgetnum("co")) == -1)
		TermError("columns?");

	if ((LI = tgetnum("li")) == -1)
		TermError("lines?");

	for (i = 0; meas[i]; i++) {
		*(meas[i]) = (char *)tgetstr(ts,&termp);
		ts += 2;
	}
#ifdef Lossage
/* You can decide whether you want this ... */
	if (!CE || !UP)
		TermError("Your terminal sucks!");
#endif
}

/*
   Deals with output to the terminal, setting up the amount of characters
   to be buffered depending on the output baud rate.  Why it's in a 
   separate file I don't know ... */

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
			p->io_cnt = BUFSIZ;
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
		30,	/* 2400	*/
		60,	/* 4800	*/
		120,	/* 9600	*/
		100,	/* EXTA	(7200?) */
		120	/* EXT	(19.2?) */
	};

	termout.io_cnt = BufSize = CheckTime = speeds[ospeed] * max(LI / 24, 1);
	termout.io_base = termout.io_ptr = emalloc(BufSize);
	termout.io_flag = 0;
	termout.io_file = 1;	/* Standard output */
}
