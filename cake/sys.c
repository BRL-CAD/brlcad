/*
**	Cake system interface.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

/*
**	Change to the directory of the cakefile.
**	Return the shortened name of the cakefile.
*/

char *
dir_setup(name)
reg	char	*name;
{
	char		buf[MAXSIZE];
	reg	int	i, l;	/* l is the subscript of the last / */

	l = -1;
	for (i = 0; name[i] != '\0'; i++)
		if (name[i] == '/')
			l = i;

	if (l < 0)
		return name;
	else
	{
		for (i = 0; i < l; i++)
			buf[i] = name[i];
		
		if (i == 0)
			buf[i++] = '/';

		buf[i] = '\0';
		if (chdir(buf) != 0)
		{
			sprintf(scratchbuf, "cake system error, chdir %s", buf);
			perror(scratchbuf);
			exit(1);
		}

		return name+l+1;
	}
}

/*
**	Set up the arguments to shell exec's.
**	pattern: execl("/bin/sh", "sh", "-c", s, 0);
**	pattern: execl("/bin/csh", "csh", "-cf", s, 0);
*/

char	*shell_path[2];
char	*shell_cmd[2];
char	*shell_opt[2];

void
shell_setup(shell, which)
reg	char	*shell;
reg	int	which;
{
	char		buf[MAXSIZE];
	reg	char	*s;
	reg	int	i, l;

	/* find the shell path */
	i = 0;
	for (s = shell; *s != '\0' && *s != ' ' && *s != '\t'; s++)
		buf[i++] = *s;
	
	buf[i] = '\0';
	shell_path[which] = new_name(buf);

	for (; *s != '\0' && *s != '-'; s++)
		;

	/* find the options */
	i = 0;
	for (; *s != '\0' && *s != ' ' && *s != '\t'; s++)
		buf[i++] = *s;
	
	buf[i] = '\0';
	if (i != 0)
		shell_opt[which] = new_name(buf);
	else
		shell_opt[which] = NULL;

	if (*s != '\0')
	{
		fprintf(stderr, "cake: cannot parse shell command '%s'\n", shell);
		exit_cake(FALSE);
	}

	/* find the shell command itself */
	s = shell_path[which];
	l = -1;
	for (i = 0; s[i] != '\0'; i++)
		if (s[i] == '/')
			l = i;

	shell_cmd[which] = new_name(s+l+1);

	if (cakedebug)
	{
		printf("shell path%d: %s\n", which, shell_path[which]);
		printf("shell cmd%d:  %s\n", which, shell_cmd[which]);
		if (shell_opt[which] != NULL)
			printf("shell opt%d:  %s\n", which, shell_opt[which]);
		else
			printf("shell opt%d:  NULL\n", which);
	}
}

bool	metatab[CHARSETSIZE];

/*
**	Set up the metacharacter table.
*/
void
meta_setup(metachars)
reg	char	*metachars;
{
	reg	int	i;
	reg	char	*s;

	for (i = 0; i < CHARSETSIZE; i++)
		metatab[i] = FALSE;
	
	for (s = metachars; *s != '\0'; s++)
		metatab[*s] = TRUE;
}

/*
**	Decide whether the given string has metacharacters or not.
**	The second arg tells whether to honour backslash escapes.
*/

bool
has_meta(str, allow_esc)
reg	char	*str;
reg	bool	allow_esc;
{
	reg	char	*s;

	for (s = str; *s != '\0'; s++)
		if (metatab[(unsigned) *s])
			return TRUE;
		or (allow_esc && (*s == '\\'))
		{
			if (s[1] != '\0')
				s++;
		}
	
	return FALSE;
}
