/*
**	Module to manipulate Cake patterns.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#include	<ctype.h>

/*
**	This function serves as an interface to the rest of the system
**	for domatch, looking after its environment.
*/

bool
match(env, str, pat)
Env		env;
reg	char	*str;
reg	Pat	*pat;
{
	extern	bool	domatch();
	reg	bool	result;
	reg	int	i;
	reg	char	*s, *p;

	if (patdebug)
		printf("Match %s vs %s\n", str, pat->p_str);

	if (pat->p_cmd)
	{
		fprintf(stderr, "cake internal error: undereferenced pattern %s in match\n",
			pat->p_str);
		exit_cake(TRUE);
	}

	if (streq(str, CHASEROOT))
		result = streq(pat->p_str, CHASEROOT);
	else
	{
		result = TRUE; /* assume so for the moment */
		p = pat->p_str+strlen(pat->p_str)-1;
		if (*p != '%' && !isdigit(*p))	/* not part of a var */
		{
			s = str+strlen(str)-1;
			if (*s != *p)		/* last chars differ */
				result = FALSE;
		}
		
		if (result) /* if last-char test inconclusive */
		{
			for (i = 0; i < MAXVAR; i++)
				env[i].v_bound = FALSE;

			result = domatch(env, str, pat->p_str);
		}
	}

	if (patdebug)
	{
		if (result == FALSE)
			printf("Match failed\n");
		else
		{
			printf("Match succeeded\n");
			for (i = 0; i < MAXVAR; i++)
				if (env[i].v_bound)
					printf("X%d: %s\n", i, env[i].v_val);
		}
	}

	return result;
}

/*
**	Match a string against a pattern.
**	The pattern is expected to have been dereferenced.
**	To handle nondeterminism, a brute force recursion approach
**	is taken.
*/

bool
domatch(env, str, patstr)
Env		env;
reg	char	*str;
reg	char	*patstr;
{
	char		buf[MAXPATSIZE];
	reg	char	*follow;
	reg	char	*s, *t;
	reg	int	varno;
	reg	int	i;
	reg	bool	more;

	put_trail("domatch", "start");
	if (patstr[0] == '%')
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

		if (env[varno].v_bound)
		{
			/* lifetime of buf is local */
			strcpy(buf, env[varno].v_val);
			strcat(buf, follow);
			checkpatlen(buf);
			put_trail("domatch", "recurse");
			return domatch(env, str, buf);
		}

		i = 0;
		buf[0] = '\0';
		env[varno].v_bound = TRUE;
		env[varno].v_val = buf;

		/* keep invariant: buf = tentative value of var  */
		/* the value of a variable may never contain a % */
		/* must consider *s == \0, but do not overshoot  */
		for (s = str, more = TRUE; more && *s != '%'; s++)
		{
			if (patdebug)
				printf("trying X%d = '%s'\n", varno, buf);

			if (domatch(env, s, follow))
			{
				checkpatlen(buf);
				env[varno].v_val = new_name(buf);
				/* lifetime of buf is now global */
				put_trail("domatch", "finish");
				return TRUE;
			}

			/* maintain invariant */
			buf[i++] = *s;
			buf[i]   = '\0';

			more = (*s != '\0');
		}
		
		/* no luck, match failed */
		env[varno].v_bound = FALSE;
		put_trail("domatch", "finish");
		return FALSE;
	}

	/* here we have something other than a variable first off */
	for (s = str, t = patstr; *t != '\0' && *t != '%'; s++, t++)
	{
		if (*t == '\\')
			t++;	/* the new *t is not checked for % */
		
		if (*s != *t)
		{
			put_trail("domatch", "finish");
			return FALSE;
		}
	}

	/* see if we have come to the end of the pattern */
	if (*t == '\0')
	{
		put_trail("domatch", "finish");
		return *s == '\0';
	}
	
	/* if not, recurse on the next variable */
	put_trail("domatch", "recurse");
	return domatch(env, s, t);
}

/*
**	Ground the argument string, i.e. replace all occurrences
**	of variables in it. It is fatal error for the string to
**	contain a variable which has no value.
*/

char *
ground(env, str)
Env		env;
reg	char	*str;
{
	reg	char	*s, *t;
	reg	int	i, var;
	char		buf[MAXSIZE];

	put_trail("ground", "start");
	i = 0;
	for (s = str; *s != '\0'; s++)
	{
		if (*s == '%')
		{
			if (isdigit(s[1]))
				var = *++s - '0';
			else
				var = NOVAR;
			
			if (! env[var].v_bound)
			{
				if (var == NOVAR)
					fprintf(stderr, "cake: %s is undefined in %s\n", "%", str);
				else
					fprintf(stderr, "cake: %s%1d is undefined in %s\n", "%", var, str);
				exit_cake(FALSE);
			}

			for (t = env[var].v_val; *t != '\0'; t++)
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

	buf[i] = '\0';
	if (i >= MAXSIZE)
	{
		fprintf(stderr, "cake internal error: pattern buffer overflow for %s\n", buf);
		exit_cake(FALSE);
	}

	put_trail("ground", "new_name finish");
	return new_name(buf);
}

/*
**	See if the argument contains any variebles.
*/

bool
hasvars(str)
reg	char	*str;
{
	reg	char	*s;

	for (s = str; *s != '\0'; s++)
	{
		if (*s == '%')
			return TRUE;
		or (*s == '\\')
		{
			if (s[1] != '\0')
				s++;
		}
	}

	return FALSE;
}

/*
**	Dereference the pattern; i.e. if it a command pattern
**	replace it with the output of the command. It is the task
**	of the caller to break this up if necessary.
**	Note that the pattern will never have to be dereferenced again.
**
**	The second arg says whether the pattern is to be broken after
**	dereferencing: this is needed purely for debugging purposes.
*/
void
deref(pat, broken)
reg	Pat	*pat;
reg	bool	broken;
{
	extern	char	*expand_cmds();

	if (! pat->p_cmd)
		return;
	
	pat->p_cmd = FALSE;
	pat->p_str = expand_cmds(pat->p_str);

	if (! broken)
		checkpatlen(pat->p_str);
}

/*
**	Break the given pattern up into a (possibly empty) list
**	of smaller patterns at white space positions.
*/

List *
break_pat(pat)
reg	Pat	*pat;
{
	reg	Pat	*newpat;
	reg	List	*list;
	reg	char	*s;

	put_trail("break_pat", "start");
	if (pat->p_cmd)
	{
		fprintf(stderr, "cake internal error: trying to break command pattern %s\n", pat->p_str);
		exit_cake(TRUE);
	}

	list = makelist0();
	s = pat->p_str;
	while (*s != '\0')
	{
		char		buf[MAXSIZE];
		reg	int	i;

		for (; *s != '\0' && isspace(*s); s++)
			;

		i = 0;
		for (; *s != '\0' && !isspace(*s); s++)
			buf[i++] = *s;
		
		buf[i] = '\0';
		if (i > 0)
		{
			newpat = make_pat(new_name(buf), FALSE, pat->p_flag);
			addtail(list, newpat);	/* no assignment */
		}
	}

	put_trail("break_pat", "finish");
	return list;
}

/*
**	Add flags to a list of patterns.
*/

List *
set_flag(patlist, flags)
reg	List	*patlist;
reg	int	flags;
{
	reg	List	*ptr;
	reg	Pat	*pat;

	for_list (ptr, patlist)
	{
		pat = (Pat *) ldata(ptr);
		pat->p_flag |= flags;
	}

	return patlist;
}
