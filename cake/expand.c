/*
**	Module to expand out commands in patterns and actions.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#include	<ctype.h>

/*
**	Expand any commands in actions. Doing this here instead of
**	in the shell saves time because of cake's command cache.
*/

char *
expand_cmds(str)
reg	char	*str;
{
	extern	char	*get_output();
	reg	char	*s;
	reg	int	n, cmds, offset;
	reg	int	stackp;
	reg	char	*rightbr;
	char		*leftbr_stack[MAXNEST];
	char		*nextbr_stack[MAXNEST];
	char		copybuf[MAXSIZE];
	static	char	buf[MAXSIZE];
	reg	char	*result;

	if (index(str, '[') == NULL)
		return str;

	put_trail("expand_cmds", "start");
	cmds = 0;
	stackp = 0;
	if (strlen(str) >= (unsigned)MAXSIZE)
	{
		fprintf(stderr, "cake internal error: command too long %s\n", str);
		exit_cake(FALSE);
	}

	strcpy(copybuf, str);
	leftbr_stack[stackp] = copybuf;
	nextbr_stack[stackp] = NULL;

	/* Leftbr_stack[stackp] and rightbr enclose commands	   */
	/* for stackp > 0; for stackp == 0 they enclose the string */
	/* Nextbr_stack gives the beginning of the next segment	   */
	/* after the corresponding leftbr; == 0 if not known (yet) */
	/* The top entry on the stack always has nextbr == NULL	   */

	for (s = copybuf; s[0] != '\0'; s++)
	{
		if (s[0] == '[' && s[1] == '[')
		{
			nextbr_stack[stackp] = s;
			if (++stackp >= MAXNEST)
			{
				fprintf(stderr, "cake internal error: command nesting level too deep\n");
				exit_cake(FALSE);
			}

			leftbr_stack[stackp] = s;
			nextbr_stack[stackp] = NULL;
			s++;	/* avoid problems with [[[cmd]] ... */
		}
		or (s[0] == ']' && s[1] == ']')
		{
			if (stackp <= 0)
				continue;

			cmds++;
			rightbr = s;

			leftbr_stack[stackp][0] = '\0';
			leftbr_stack[stackp][1] = '\0';
			rightbr[0] = '\0';
			rightbr[1] = '\0';

			result = get_output(new_name(leftbr_stack[stackp]+2));
			cdebug("expansion [[%s]] => [[%s]]\n", leftbr_stack[stackp]+2, result);

			segcpy(buf, leftbr_stack[stackp-1], nextbr_stack[stackp-1]);
			strcat(buf, result);
			offset = strlen(buf) - 1;
			strcat(buf, rightbr+2);

			if (strlen(buf) >= (unsigned)MAXSIZE)
			{
				fprintf(stderr, "cake: expansion result '%s' is too long\n", buf);
				exit_cake(FALSE);
			}

			stackp--;
			leftbr_stack[stackp] = new_name(buf);
			nextbr_stack[stackp] = NULL;

			/* start next iteration with the */
			/* first character after the ]]  */
			s = leftbr_stack[stackp] + offset;
		}
		or (s[0] == '\\')
		{
			/* don't check next char */
			if (s[1] != '\0')
				s++; 
		}
	}

#ifdef	CAKEDEBUG
	if (entrydebug)
		printf("after expand_cmds: [[%s]]\n", str);
#endif

	if (cmds <= 0)
	{
		put_trail("expand_cmds", "finish");
		return str;
	}
	or (stackp == 0)
	{
		put_trail("expand_cmds", "finish");
		return leftbr_stack[0];
	}

	segcpy(buf, leftbr_stack[0], nextbr_stack[0]);
	for (n = 1; n <= stackp; n++)
		segcat(buf, leftbr_stack[n], nextbr_stack[n]);
	if (strlen(buf) >= (unsigned)MAXSIZE)
	{
		fprintf(stderr, "cake: expansion result '%s' is too long\n", buf);
		exit_cake(FALSE);
	}

	put_trail("expand_cmds", "finish");
	return new_name(buf);
}

void
segcpy(target, left, right)
reg	char	*target, *left, *right;
{
	reg	int	i;
	reg	char	*s;

	if (right == NULL)
		strcpy(target, left);
	else
	{
		for (s = left, i = 0; s != right; s++, i++)
			target[i] = *s;

		target[i] = '\0';
	}
}

void
segcat(target, left, right)
reg	char	*target, *left, *right;
{
	reg	int	i;
	reg	char	*s;

	if (right == NULL)
		strcat(target, left);
	else
	{
		for (s = left, i = strlen(target); s != right; s++, i++)
			target[i] = *s;

		target[i] = '\0';
	}
}

/*
**	Execute the given command and return its output.
**	It is a fatal error for the command to terminate abnormally.
*/

char *
get_output(cmd)
reg	char	*cmd;
{
	extern	char	*mktemp();
	extern	char	*get_out();
	extern	char	*flatten();
	extern	int	cake_proc();
	extern	int	cake_wait();
	char		buf[MAXSIZE];
	int		code;
	reg	char	*out_filename;
	reg	FILE	*fp;
	reg	int	i, c;
	reg	int	pid;
	reg	char	*s, *result;

	put_trail("get_output", "start");
	cdebug("get_output of [%s]\n", cmd);

	/* see if we have tried this command before */
	if ((result = get_out(cmd)) != NULL)
	{
		cdebug("cache yields [%s]\n", result);
		put_trail("get_output", "early finish");
		return result;
	}

	out_filename = get_newname();
	pid = cake_proc(cmd, Exec, out_filename, (Node *) NULL, (int (*)()) NULL, (List *) NULL);
	code = cake_wait(pid);
	if (code != 0 && ! zflag)
	{
		printf("cake, %s: nonzero exit status\n", cmd);
		exit_cake(FALSE);
	}

	if ((fp = fopen(out_filename, "r")) == NULL)
	{
		sprintf(scratchbuf, "cake system error, fopen %s", out_filename);
		perror(scratchbuf);
		exit_cake(FALSE);
	}

	/* convert the chars in file fp to a string */
	i = 0;
	while ((c = getc(fp)) != EOF)
		buf[i++] = c;
	
	buf[i] = '\0';
	fclose(fp);
	if (MAXSIZE <= i)
	{
		printf("cake, %s: output too long\n", cmd);
		exit_cake(FALSE);
	}

#ifndef	LEAVE_DIR
	cdebug("get_output unlink out_filename %s\n", out_filename);
	if (unlink(out_filename) != 0)
	{
		sprintf(scratchbuf, "cake system error, unlink %s", out_filename);
		perror(scratchbuf);
	}
#endif

	/* save the result for posterity */
	s = new_name(flatten(buf));
	new_out(cmd, s);
	cdebug("put result [%s] into cache\n", s);
	put_trail("get_output", "finish");
	return s;
}

/*
**	Flatten the output of commands by converting newlines to spaces
**	and by removing blanks around the edges.
*/

char *
flatten(str)
reg	char	*str;
{
	reg	char	*s;

	/* convert newlines (and formfeeds) to spaces */
	for (s = str; *s != '\0'; s++)
		if (*s == '\n' || *s == '\f')
			*s = ' ';

	/* remove blanks around the edges */
	for (s = str+strlen(str)-1; str <= s && isspace(*s); s--)
		*s = '\0';
	for (s = str; *s != '\0' && isspace(*s); s++)
		;

	return s;
}
