/*
 *			J O V E _ P R O C . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.2  1995/06/21  03:43:58  gwyn
 * Eliminated trailing blanks.
 * Use tempnam() where available.
 * Process buffer temp file was mode 644, now 600.
 * ProcTmp, cerrfmt, lerrfmt are now arrays rather than string literal pointers.
 * Use LBSIZE instead of hard-wired 100.
 * Fixed all the pseudo-varargs functions, using VA_* macros.
 * SIGTSTP branch had same code as other branch!
 * Must cast 0 when used as null pointer constant in argument list.
 * Improved child status return.
 * Added check for too many arguments to UNIX command.
 * Create pipe in privacy mode.
 *
 * Revision 11.1  95/01/04  10:35:19  mike
 * Release_4.4
 *
 * Revision 10.3  94/12/21  22:04:46  mike
 * Eliminated "statement not reached" warning.
 *
 * Revision 10.2  93/10/26  05:57:41  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:54:03  mike
 * Release_4.0
 *
 * Revision 2.10  91/09/23  03:16:45  mike
 * Eliminated return / return(expr) warning.
 * Removed some dead code
 *
 * Revision 2.9  91/09/23  02:58:55  mike
 * gldav() is not a BSD system call.
 *
 * Revision 2.8  91/08/30  19:41:59  mike
 * Added ^Xe to run cake, like ^X^E runs make.
 *
 * Revision 2.7  91/08/30  19:14:21  mike
 * Modified to successfully parse C compiler errors, even on more modern
 * compilers, and on the oddball CRAYs.
 *
 * Revision 2.6  91/08/30  18:59:50  mike
 * Modifications for clean compilation on the XMP
 *
 * Revision 2.5  91/08/30  18:46:08  mike
 * Changed from BSD index/rindex nomenclature to SYSV strchr/strrchr.
 *
 * Revision 2.4  91/08/30  17:54:37  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.3  91/08/30  17:49:13  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.2  87/04/14  20:11:25  dpk
 * Added hack for supporting CRAY.
 *
 * Revision 2.1  86/04/06  06:20:27  gwyn
 * SpellCom was using sprintf return value, non-portable.
 *
 * Revision 2.0  84/12/26  16:47:33  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:09:16  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   jove_proc.c

   This file contains procedures to handle the shell to buffer commands
   and buffer to shell commands.  It isn't especially neat, but I think
   it is understandable. */

#include "./jove.h"

#include <signal.h>

extern	char cerrfmt[];
extern	char lerrfmt[];

struct error {
	BUFFER		*er_buf;	/* Buffer error is in */
	LINE		*er_mess;	/* Actual error message */
	LINE		*er_text;	/* Actual error */
	int		er_char;	/* char pos of error */
	struct error	*er_next;	/* List of error */
};

struct error	*thiserror = 0,
		*errorlist = 0;
BUFFER		*thisbuf = 0;	/* Buffer that error parsing took place */

int	MakeAll = 0,		/* Not make -k */
	WtOnMk = 1;		/* Write the modified files when we make */

extern char	*StdShell;
extern char	ProcTmp[];

void		SpParse();

/* Add an error to the end of the list of errors.  This is used for
 * parse-C/LINT-errors and for the spell-buffer command
 */

struct error *
AddError(newerror, errline, buf, line, charpos)
struct error	*newerror;
LINE	*errline,
	*line;
BUFFER	*buf;
{
	if (newerror) {
		newerror->er_next = (struct error *)
				emalloc(sizeof (struct error));
		newerror = newerror->er_next;
	} else
		thiserror = newerror = errorlist = (struct error *)
				emalloc(sizeof (struct error));
	newerror->er_buf = buf;
	newerror->er_text = line;
	newerror->er_char = charpos;
	newerror->er_next = 0;
	newerror->er_mess = errline;
	return newerror;
}

/* Returns the num'th line in buffer `b' */

LINE *
LineAt(num, b)
register int	num;
BUFFER	 *b;
{
	register LINE	*lp = b->b_zero;

	while (--num > 0 && lp)
		lp = lp->l_next;
	return lp;
}

void
CParse()
{
	ErrParse(cerrfmt);
}

void
LintParse()
{
	ErrParse(lerrfmt);
}

/* Parse for C/LINT errors in the current buffer.  Set up for the next-error
   command. */

void
ErrParse(fmtstr)
char	*fmtstr;
{
	BUFLOC	*bp;
	char	fname[LBSIZE],
		lineno[10];
	struct error	*ep = 0;
	BUFFER	*buf;

	Bof();
	if (errorlist)
		ErrFree();
	thisbuf = curbuf;
	/* Find a line with a number on it */
	while (bp = dosearch(fmtstr, 1, 1)) {
		SetDot(bp);
		putmatch(1, fname, sizeof fname);
		putmatch(2, lineno, sizeof lineno);
		buf = do_find((WINDOW *) 0, fname);
		ep = AddError(ep, curline, buf, LineAt(atoi(lineno), buf), 0);
	}
}

/* Send the buffer to the spell program (for some reason, writing the
   file and running spell on that file caused spell to just sit there!)
   and then go find each occurrence of the mispelled words that the user
   says to go find.  Check out SpParse() */

void
SpellCom()
{
	WINDOW	*errwind,
		*savewind;
	char	command[LBSIZE];

	savewind = curwind;
	exp_p = 1;
	if (IsModified(curbuf))
		SaveFile();
	ignore(sprintf(command, "spell %s", curbuf->b_fname));
	ignore(UnixToBuf("Spell", 1, !exp_p, "/bin/sh", "sh", "-c",
			command, (char *)0));
	NotModified();
	Bof();		/* Beginning of (error messages) file */
	errwind = curwind;
	if (linebuf[0]) {
		SpParse(errwind, savewind);
		if (thiserror)
			NextError();
		message("Go get 'em");
	} else {
		message("No errors");
		SetWind(savewind);
	}
}

/* There is one word per line in the current buffer.  Read that word
   and ask the user whether he wants us to search for it (if it is a
   big buffer he may not want to if he thinks it is spelled correctly). */
void
SpParse(err, buf)
WINDOW	*err,
	*buf;
{
	BUFLOC	*bp;
	char	string[LBSIZE], ans = '\0';
	struct error	*newerr = 0;

	if (errorlist)
		ErrFree();
	thisbuf = err->w_bufp;
	for (;;) {
		SetWind(err);
		if (linebuf[0] == 0)
			goto nextword;
		s_mess("Is \"%s\" misspelled (Y or N)? ", linebuf);
		ignore(sprintf(string, "\\b%s\\b", linebuf));
		SetWind(buf);		/* Paper buffer */
		Bof();
		switch (ans = Upper(getch())) {
		case ' ':
		case '\n':
		case '\r':
			ans = 'N';
		case 'Y':
		case 'y':
		case 'N':
		case 'n':
			break;
		case '\07':
			return;
		default:
			rbell();
			continue;
		}
		if (ans == 'Y') {	/* Not correct */
			while (bp = dosearch(string, 1, 1)) {
				SetDot(bp);
				newerr = AddError(newerr,
				    thisbuf->b_dot, buf->w_bufp,
				    curline, curchar);
			}
		}

nextword:
		SetWind(err);	/* Back to error window to move to
				   the next word */
		if (eobp())
			break;
		if (ans == 'N') {	/* Delete the word ... */
			Bol();
			DoTimes(KillEOL, 1);	/* by deleting the line */
		} else {
			if (lastp(curline))
				break;
			else
				SetLine(curline->l_next);
		}
	}
}

/* Free up all the errors */

void
ErrFree()
{
	register struct error	*ep;

	for (ep = errorlist; ep; ep = ep->er_next)
		free((char *) ep);
	errorlist = thiserror = 0;
}

/* Go the the next error, if there is one.  Put the error buffer in
   one window and the buffer with the error in another window.
   It checks to make sure that the error actually exists. */

void
NextError()
{
	register int	i = exp;

	while (--i >= 0)
		DoNextErr();
}

void
DoNextErr()
{
	/* Make sure we haven't deleted the line with the actual error
	   by accident. */

	while (errorlist && thiserror) {
		if (inlist(thiserror->er_buf->b_zero, thiserror->er_text))
			break;
		thiserror = thiserror->er_next;
	}
	if (errorlist == 0 || thiserror == 0)
		complain("No errors");

	pop_wind(thisbuf->b_name, 0);
	SetLine(thiserror->er_mess);
	SetTop(curwind, (curwind->w_line = thiserror->er_mess));
	pop_wind(thiserror->er_buf->b_name, 0);
	DotTo(thiserror->er_text, thiserror->er_char);
	thiserror = thiserror->er_next;
}

/* Run make, first writing all the modified buffers (if the WtOnMk flag is
 * non-zero), parse the errors, and go the first error.
 */

void
MakeErrors()
{
	WINDOW	*old = curwind;
	int	status;
	char	*what;
	char	*null = "";

	what = ask(null, FuncName());
	if (what == null)
		what = 0;
	if (WtOnMk)
		WtModBuf();
	if (MakeAll)
		status = UnixToBuf("make", 1, 1, "make", "Make", "-k", what, (char *)0);
	else
		status = UnixToBuf("make", 1, 1, "make", "Make", what, (char *)0);
	com_finish(status, "make");
	if (errorlist)
		ErrFree();

	if (status)
		ErrParse(cerrfmt);

	if (thiserror)
		NextError();
	else
		SetWind(old);
}

/*
 *			C A K E E R R O R S
 *
 *  Run cake, first writing all the modified buffers (if the WtOnMk flag is
 *  non-zero), parse the errors, and go the first error.
 *  BRL Addition, Mike Muuss, 30-Aug-91.
 */
void
CakeErrors()
{
	WINDOW	*old = curwind;
	int	status;
	char	*what;
	char	*null = "";

	what = ask(null, FuncName());
	if (what == null)
		what = 0;
	if (WtOnMk)
		WtModBuf();
	if (MakeAll)
		status = UnixToBuf("cake", 1, 1, "cake", "cake", "-k", what, (char *)0);
	else
		status = UnixToBuf("cake", 1, 1, "cake", "cake", what, (char *)0);
	com_finish(status, "cake");
	if (errorlist)
		ErrFree();

	if (status)
		ErrParse(cerrfmt);

	if (thiserror)
		NextError();
	else
		SetWind(old);
}

/*
 *  Make a buffer name given the command `command', i.e. "fgrep -n foo *.c"
 *  will return the buffer name "fgrep".
 */
char *
MakeName(command)
char	*command;
{
	static char	bufname[50];
	char	*cp = bufname, c;

	while ((c = *command++) && (c == ' ' || c == '\t'))
		;
	do
		*cp++ = c;
	while ((c = *command++) && (c != ' ' && c != '\t'));
	*cp = 0;
	cp = strrchr(bufname, '/');
	if (cp)
		strcpy(bufname, cp + 1);
	return bufname;
}

static char	ShcomBuf[LBSIZE] = {0};	/* I hope ??? */

void
ShToBuf()
{
	char	bufname[LBSIZE];

	strcpy(bufname, ask((char *) 0, "Buffer: "));
	DoShell(bufname, ask(ShcomBuf, "Command: "));
}

void
ShellCom()
{
	strcpy(ShcomBuf, ask(ShcomBuf, FuncName()));
	DoShell(MakeName(ShcomBuf), ShcomBuf);
}

/* Run the shell command into `bufname'.  Empty the buffer except when we
   give a numeric argument, in which case it inserts the output at the
   current position in the buffer.  */

void
DoShell(bufname, command)
char	*bufname,
	*command;
{
	WINDOW	*savewp = curwind;
	int	status;

	exp = 1;
	status = UnixToBuf(bufname, 1, !exp_p, StdShell, "shell", "-c",
			command, (char *)0);
	com_finish(status, command);
	SetWind(savewp);
}

void
com_finish(status, com)
char	*com;
{
	s_mess("\"%s\" completed %ssuccessfully", com, status ? "un" : "");
}

void
dopipe(p)
int	p[];
{
	if (pipe(p) == -1)
		complain("Cannot pipe");
}

void
PipeClose(p)
int	p[];
{
	ignore(close(p[0]));
	ignore(close(p[1]));
}

/* Run the command to bufname, erase the buffer if clobber is non-zero,
 * and redisplay if disp is non-zero.
 */

/* VARARGS */

int
UnixToBuf(VA_T(char *bufname) VA_T(int disp) VA_T(int clobber)
	VA_T(const char *func) VA_ALIST)
	VA_DCL
{
	VA_D(char	*bufname)
	VA_D(int	disp)
	VA_D(int	clobber)
	VA_D(char	*func)
	VA_LIST(ap)
	char	*args[ALSIZE],
		**argp;
	int	p[2],
		pid;
	char	buff[LBSIZE];
	extern int	ninbuf;

	message("Starting up...");
	VA_START(ap, func)
	VA_I(ap, char *, bufname)
	VA_I(ap, int, disp)
	VA_I(ap, int, clobber)
	VA_I(ap, char *, func)
	for (argp = args; argp - args < ALSIZE; ++argp)
		if ((*argp = VA_ARG(ap, char *)) == (char *)0)
			break;
	VA_END(ap)
	if (argp - args == ALSIZE)
		complain("Too many arguments");
	pop_wind(bufname, clobber);
	if (disp)
		redisplay();
	if (clobber)
		SetScratch(curbuf);
	exp = 1;

	ttyset(0);
	dopipe(p);
	pid = fork();
	if (pid == -1) {
		PipeClose(p);
		complain("Cannot fork");
	}
	if (pid == 0) {
		ignorf(signal(SIGINT, SIG_DFL));
		ignore(close(1));
		ignore(close(2));
		ignore(dup(p[1]));
		ignore(dup(p[1]));
		PipeClose(p);
		execvp(func, args);
		ignore(write(1, "Execvp failed", 12));
		_exit(1);
	} else {
		int	status;
		char	*mess;

		ignore(close(p[1]));
		io = p[0];
		while (getfline(buff) != EOF) {
			ins_str(buff);
			LineInsert();
			if (ninbuf <= 0) {
				mess = "Chugging along...";
				message(mess);
				redisplay();
			}
		}
		UpdateMesg();
		IOclose();
		while (wait(&status) != pid)
			;
		ttyset(1);
		return status;
	}
	return 0;
}

/* Send a region to shell.  Now we can beautify C and sort programs */

void
RegToShell()
{
	char	com[LBSIZE];
	MARK	*m = CurMark();

	strcpy(com, ask((char *) 0, FuncName()));
	if (!exp_p) {
		exp_p = 1;	/* So it doesn't delete the region */
		exp = 0;
	}
	if (inorder(m->m_line, m->m_char, curline, curchar))
		RegToUnix(curbuf->b_name, 1, m->m_line, m->m_char,
					curline, curchar, com);
	else
		RegToUnix(curbuf->b_name, 1, curline, curchar, m->m_line,
					m->m_char, com);
	message("Done");
}

/* Writes the region to a tmp file and then run the command with input
   from that file */

void
RegToUnix(bufname, replace, line1, char1, line2, char2, func)
char	*bufname;
LINE	*line1,
	*line2;
char	*func;
{
#if HAS_TEMPNAM
	/* Honor $TMPDIR in user's environment */
	char	*fname = tempnam((char *)NULL, "jovep");
#else
	char	*fname = mktemp(ProcTmp);
#endif
	char	com[LBSIZE];

	if ((io = creat(fname, 0600)) == -1)
		complain(IOerr("create", fname));
	putreg(line1, char1, line2, char2);
	IOclose();
	if (replace)
		DelReg();
	ignore(sprintf(com, "%s < %s", func, fname));
	ignore(UnixToBuf(bufname, 0, 0, StdShell, "shell", "-c", com, (char *)0));
	ignore(unlink(fname));
}
