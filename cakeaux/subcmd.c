/*
**	Subcmd - execute commands with arguments based on substitutions
*/

static	char
rcs_id[] = "$Header$";

#include	<stdio.h>
#include	<ctype.h>
#include	"std.h"

#define	MAXLEN	1024

char	var_char = 'X';


/*
**	Tell the unfortunate user how to use subcmd.
*/
void
usage()
{
	printf("Usage: subcmd [-fginz] [-vX] cmd oldpattern newpattern files ...\n");
	exit(1);
}

main(argc, argv)
int	argc;
char	**argv;
{
	extern	bool	match();
	extern	char	*ground();
	register	char	*cmd, *old, *new;
	register	int	unmatched, i;
	register	bool	fast, general, ignore, noexec, needzero;
	register	FILE	*csh;
	char		buf[MAXLEN];

	fast     = FALSE;
	general  = FALSE;
	ignore   = FALSE;
	noexec   = FALSE;
	needzero = TRUE;
	unmatched = 0;
	while (argc > 1 && argv[1][0] == '-')
	{
		for (i = 1; argv[1][i] != '\0'; i++)
		{
			switch (argv[1][i])
			{

		case 'f':	fast = TRUE;
				break;

		case 'g':	general = TRUE;
				break;

		case 'i':	ignore = TRUE;
				break;

		case 'n':	noexec = TRUE;
				break;

		case 'v':	if (i != 1)
					usage();

				var_char = argv[1][2];
				goto nextword;

		case 'z':	needzero = FALSE;
				break;

		default:	usage();

			}
		}

nextword:
		argc--;
		argv++;
	}

	if (argc < 3)
		usage();
	
	cmd = argv[1];
	old = argv[2];
	new = argv[3];
	argv += 3;
	argc -= 3;

	if (fast)
	{
		csh = popen("/bin/csh -fs", "w");
		if (csh == (FILE *) NULL)
		{
			printf("subcmd: cannot popen csh\n");
			exit(1);
		}
	}

	while (argc > 1)
	{
		if (! match(argv[1], old))
			unmatched++;
		else
		{
			if (general)
				sprintf(buf, cmd, argv[1], ground(new));
			else
				sprintf(buf, "%s %s %s", cmd, argv[1], ground(new));

			if (strlen(buf) >= MAXLEN)
				printf("subcmd: command too long\n");
			or (noexec)
				printf("would be executing %s\n", buf);
			or (fast)
			{
				printf("executing %s\n", buf);
				fprintf(csh, "%s\n", buf);
			}
			else
			{
				printf("executing %s\n", buf);
				if (system(buf) != 0 && needzero)
					printf("subcmd: command failed\n");
			}
		}

		argv++;
		argc--;
	}

	if (fast)
		pclose(csh);

	exit(ignore? 0: unmatched);
}

/*
**	Module to manipulate Cake patterns.
*/

typedef	struct	s_env
{
	char	*en_val;
	bool	en_bound;
} Env;

#define	NOVAR	 10
#define	MAXVAR	 11
#define	MAXSIZE	128

Env	env[MAXVAR];

/*
**	This function initialises the environment of domatch.
*/

bool
match(str, pat)
register	char	*str;
register	char	*pat;
{
	extern	bool	domatch();
	register	int	i;
	register	char	*s, *p;

	p = pat+strlen(pat)-1;
	if (*p != var_char && !isdigit(*p))	/* not part of a var */
	{
		s = str+strlen(str)-1;
		if (*s != *p)			/* last chars differ */
			return FALSE;
	}

	for (i = 0; i < MAXVAR; i++)
		env[i].en_bound = FALSE;

	return domatch(str, pat);
}

/*
**	Match a string against a pattern.
**	The pattern is expected to have been dereferenced.
**	To handle nondeterminism, a brute force recursion approach
**	is taken.
*/

bool
domatch(str, patstr)
register	char	*str;
register	char	*patstr;
{
	extern	char	*new_name();
	char		buf[MAXSIZE];
	register	char	*follow;
	register	char	*s, *t;
	register	int	varno;
	register	int	i;
	register	bool	more;

	if (patstr[0] == var_char)
	{
		if (isdigit(patstr[1]))
		{
			varno  = patstr[1] - '0';
			follow = patstr + 2;
		}
		else
		{
			varno  = NOVAR;
			follow = patstr + 1;
		}

		if (env[varno].en_bound)
		{
			/* lifetime of buf is local */
			strcpy(buf, env[varno].en_val);
			strcat(buf, follow);
			return domatch(str, buf);
		}

		i = 0;
		buf[0] = '\0';
		env[varno].en_bound = TRUE;
		env[varno].en_val = buf;

		/* keep invariant: buf = tentative value of var  */
		/* must consider *s == \0, but do not overshoot  */
		for (s = str, more = TRUE; more; s++)
		{
			if (domatch(s, follow))
			{
				/* lifetime of buf is now global */
				env[varno].en_val = new_name(buf);
				return TRUE;
			}

			/* maintain invariant */
			buf[i++] = *s;
			buf[i]   = '\0';

			more = (*s != '\0');
		}
		
		/* no luck, match failed */
		env[varno].en_bound = FALSE;
		return FALSE;
	}

	/* here we have something other than a variable first off */
	for (s = str, t = patstr; *t != '\0' && *t != var_char; s++, t++)
	{
		if (*t == '\\')
			t++;	/* the new *t is not checked for % */
		
		if (*s != *t)
			return FALSE;
	}

	/* see if we have come to the end of the pattern */
	if (*t == '\0')
		return *s == '\0';
	
	/* if not, recurse on the next variable */
	return domatch(s, t);
}

/*
**	Ground the argument string, i.e. replace all occurrences
**	of variables in it. It is fatal error for the string to
**	contain a variable which has no value.
*/

char *
ground(str)
register	char	*str;
{
	extern	char	*new_name();
	register	char	*s, *t;
	register	int	i, var;
	char		buf[MAXSIZE];

	i = 0;
	for (s = str; *s != '\0'; s++)
	{
		if (*s == var_char)
		{
			if (isdigit(s[1]))
				var = *++s - '0';
			else
				var = NOVAR;
			
			if (! env[var].en_bound)
			{
				printf("Attempt to use undefined value in %s\n", str);
				exit(1);
			}

			for (t = env[var].en_val; *t != '\0'; t++)
				buf[i++] = *t;
		}
		or (*s == '\\')
		{
			if (s[1] != '\0')
				buf[i++] = *++s;
		}
		else
			buf[i++] = *s;
	}

	if (i >= MAXSIZE)
	{
		printf("Ran out of buffer\n");
		exit(1);
	}

	buf[i] = '\0';
	return new_name(buf);
}

char *
new_name(str)
register	char	*str;
{
	extern	char	*malloc();
	register	char	*copy;

	copy = malloc(strlen(str) + 1);
	strcpy(copy, str);
	return copy;
}
