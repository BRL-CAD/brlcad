/*
**	Module to record and process Cake dependency entries.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#include	<ctype.h>

List	*entries;
Entry	*main_entry;

Test	*ground_test();		/* defined further down */

#define	ground_pat(env, pat)	make_pat(ground(env, pat->p_str), pat->p_cmd, pat->p_flag);
#define	ground_act(env, act)	make_act(ground(env, act->a_str), act->a_flag);

/*
**	Initialise ALL data structures about entries.
*/
void
init_entry()
{
	entries = makelist0();
	main_entry = (Entry *) NULL;
}

/*
**	Deposit an entry just read in.
*/
void
new_entry(deps, acts)
reg	List	*deps;				/* of Entry	*/
reg	List	*acts;				/* of Pat	*/
{
	reg	List	*ptr;
	reg	Entry	*entry;

	for_list (ptr, deps)
	{
		entry = (Entry *) ldata(ptr);
		entry->e_act  = acts;
		addtail(entries, entry);	/* no assignment */
	}
}

/*
**	Prepare entries for matches against their left hand sides.
**	This essentially means dereferencing any command patterns.
**	Another minor correction is also done here: if -n is given
**	then we set nf_PRECIOUS so no file is deleted.
*/
void
prep_entries()
{
	reg	List	*ptr, *ptr1;
	reg	Entry	*entry;
	reg	Pat	*pat;
	reg	List	*newnew;

	for_list (ptr, entries)
	{
		entry = (Entry *) ldata(ptr);

		newnew = makelist0();
		for_list (ptr1, entry->e_new)
		{
			pat = (Pat *) ldata(ptr1);
			if (on_pat(pat, nf_WHEN))
			{
				printf("cake: inappropriate flag '*' after target %s ignored\n",
					pat->p_str);
				reset_pat(pat, nf_WHEN);
			}

			if (nflag)
				set_pat(pat, nf_PRECIOUS);

			if (! pat->p_cmd)
				addtail(newnew, pat);		/* na */
			else
			{
				deref(pat, TRUE);
				addlist(newnew, break_pat(pat));/* na */
			}
		}

		entry->e_new = newnew;

		for_list (ptr1, entry->e_old)
		{
			pat = (Pat *) ldata(ptr1);
			if (pat->p_cmd && on_pat(pat, nf_WHEN))
			{
				printf("cake: inappropriate flag '*' after command source %s ignored\n",
					pat->p_str);
				reset_pat(pat, nf_WHEN);
			}
		}

#ifdef	CAKEDEBUG
		if (entrydebug)
		{
			printf("prepared entry:\n");
			print_entry(entry);
		}
#endif
	}
}

/*
**	Enter the last (main) entry. This entry has a distinguished
**	left hand side consisting of the sole pattern CHASEROOT.
**	This is the pattern which cake tries to make up-to-date.
**	If there were nonoption arguments on the command line,
**	CHASEROOT is made to depend on them; otherwise the left hand
**	side of the first concrete entry is used, as in make.
**	However, if the first concrete entry is in an included file,
**	we keep looking, unless it had a double colon.
*/
void
final_entry(argc, argv)
reg	int	argc;
reg	char	*argv[];
{
	reg	List	*ptr, *ptr1;
	reg	Entry	*entry, *foundentry;
	reg	Pat	*pat;
	reg	bool	found_main, found_incl;
	reg	int	i;

	main_entry = make(Entry);
	main_entry->e_new  = makelist(make_pat(CHASEROOT, FALSE, nf_PSEUDO|nf_NODELETE));
	main_entry->e_old  = makelist0();
	main_entry->e_cond = (Test *) NULL;
	main_entry->e_act  = makelist0();

	cdebug("target count %d\n", argc-1);

	if (argc > 1)
	{
		for (i = 1; i < argc; i++)
		{
			cdebug("target %s\n", argv[i]);
			addtail(main_entry->e_old, make_pat(argv[i], FALSE, 0));	/* na */
		}
	}
	else
	{
		/* find the first entry without variables */
		found_main = FALSE;
		found_incl = FALSE;
		for_list (ptr, entries)
		{
			entry = (Entry *) ldata(ptr);
			for_list (ptr1, entry->e_new)
			{
				pat = (Pat *) ldata(ptr1);
				if (hasvars(pat->p_str))
					goto nextentry;
			}

			if (streq(entry->e_file, cakefile) || entry->e_dblc)
			{
				found_main = TRUE;
				foundentry = entry;
				break;
			}
			or (! found_incl)
			{
				found_incl = TRUE;
				foundentry = entry;
			}

		nextentry:	;
		}

		if (! found_main && ! found_incl)
		{
			printf("cake: no entries without variables\n");
			printf("cake: don't know what cake to bake\n");
			exit(1);
		}

		/* update everything on its left side */
		for_list (ptr, foundentry->e_new)
		{
			pat = (Pat *) ldata(ptr);
			addtail(main_entry->e_old, pat);	/* na */
		}
	}

	addtail(entries, main_entry);	/* no assigment */
}


/*
**	Return a new entry which is a ground version of the one passed.
**	Process '*' flags and set up e_when accordingly.
*/

Entry *
ground_entry(env, entry)
Env		env;
reg	Entry	*entry;
{
	reg	List	*ptr;
	reg	Entry	*newentry;
	reg	Pat	*pat, *newpat;
	reg	Act	*act, *newact;

	put_trail("ground_entry", "start");
	newentry = make(Entry);
	newentry->e_new  = makelist0();
	newentry->e_old  = makelist0();
	newentry->e_act  = makelist0();
	newentry->e_when = makelist0();

	for_list (ptr, entry->e_new)
	{
		/* prep_entries made these patterns command free */
		pat = (Pat *) ldata(ptr);
		newpat = ground_pat(env, pat);
		do_when(newentry, newpat);
		addtail(newentry->e_new, newpat);	/* na */
	}

	for_list (ptr, entry->e_old)
	{
		pat = (Pat *) ldata(ptr);
		newpat = ground_pat(env, pat);
		do_when(newentry, newpat);
		addtail(newentry->e_old, newpat);	/* na */
	}

	for_list (ptr, entry->e_act)
	{
		act = (Act *) ldata(ptr);
		newact = ground_act(env, act);
		addtail(newentry->e_act, newact);	/* na */
	}

	newentry->e_cond = ground_test(env, entry->e_cond);

	put_trail("ground_entry", "finish");
	return newentry;
}

/*
**	Look after '*' (when) flags.
*/
void
do_when(entry, pat)
reg	Entry	*entry;
reg	Pat	*pat;
{
	if (on_pat(pat, nf_WHEN))
	{
		reset_pat(pat, nf_WHEN);
		entry->e_when = addtail(entry->e_when, pat);
	}
}

/*
**	Ground a test.
*/

Test *
ground_test(env, test)
Env		env;
reg	Test	*test;
{
	reg	List	*ptr;
	reg	Pat	*pat, *newpat;
	reg	Test	*newtest;

	if (test == (Test *) NULL)
		return test;

	put_trail("ground_test", "start");
	switch (test->t_kind)
	{

case t_TRUE:
case t_FALSE:	put_trail("ground_test", "finish");
		return test;

case t_AND:
case t_OR:	put_trail("ground_test", "finish");
		return make_test_b(test->t_kind,
			ground_test(env, test->t_left), ground_test(env, test->t_right));

case t_NOT:	put_trail("ground_test", "finish");
		return make_test_u(test->t_kind, ground_test(env, test->t_left));

case t_CMD:	put_trail("ground_test", "finish");
		return make_test_c(ground(env, test->t_cmd));

case t_MATCH:	pat = ground_pat(env, test->t_pat);
		newtest = make_test_mm(pat);
		for_list (ptr, test->t_list)
		{
			pat = (Pat *) ldata(ptr);
			newpat = ground_pat(env, pat);
			addtail(newtest->t_list, newpat);	/* na */
		}

		put_trail("ground_test", "finish");
		return newtest;

case t_LIST:	pat = ground_pat(env, test->t_pat);
		newtest = make_test_l(pat, makelist0());
		for_list (ptr, test->t_list)
		{
			pat = (Pat *) ldata(ptr);
			newpat = ground_pat(env, pat);
			addtail(newtest->t_list, newpat);	/* na */
		}

		put_trail("ground_test", "finish");
		return newtest;

case t_EXIST:	newpat = ground_pat(env, test->t_pat);
		put_trail("ground_test", "finish");
		return make_test_s(t_EXIST, newpat);

case t_CANDO:	newpat = ground_pat(env, test->t_pat);
		put_trail("ground_test", "finish");
		return make_test_s(t_CANDO, newpat);

case t_OK:	newpat = ground_pat(env, test->t_pat);
		put_trail("ground_test", "finish");
		return make_test_s(t_OK, newpat);

default:	printf("cake internal error: bad test type %x in ground_test\n", test->t_kind);
		break;
	}

	put_trail("ground_test", "finish");
	return (Test *) NULL;
}

/*
**	Dereference all command patterns in the given entry.
*/
void
deref_entry(env, entry)
Env		env;
reg	Entry	*entry;
{
	reg	List	*ptr, *ptr1;
	reg	List	*newlist;
	reg	List	*patlist;
	reg	Pat	*pat, *oldpat, *newpat;

	newlist = makelist0();
	for_list (ptr, entry->e_old)
	{
		pat = (Pat *) ldata(ptr);
		if (! pat->p_cmd)
			addtail(newlist, pat);	/* na */
		else
		{
			deref(pat, TRUE);
			patlist = break_pat(pat);
			for_list (ptr1, patlist)
			{
				oldpat = (Pat *) ldata(ptr1);
				newpat = make_pat(ground(env, oldpat->p_str), FALSE, 0);
				do_when(entry, newpat);
				addtail(newlist, newpat);	/* na */
			}
		}
	}

	entry->e_old  = newlist;
}

/*
**	Dereference a test.
*/

Test *
deref_test(test)
reg	Test	*test;
{
	reg	List	*ptr;
	reg	List	*newlist;
	reg	Pat	*pat;

	if (test == (Test *) NULL)
		return test;

	put_trail("deref_test", "start");
	switch (test->t_kind)
	{

case t_TRUE:
case t_FALSE:	put_trail("deref_test", "finish");
		return test;

case t_AND:
case t_OR:	test->t_left  = deref_test(test->t_left);
		test->t_right = deref_test(test->t_right);
		put_trail("deref_test", "finish");
		return test;

case t_NOT:	test->t_left  = deref_test(test->t_left);
		put_trail("deref_test", "finish");
		return test;

case t_CMD:	test->t_cmd = expand_cmds(test->t_cmd);
		put_trail("deref_test", "finish");
		return test;

case t_MATCH:	deref(test->t_pat, FALSE);
		for_list (ptr, test->t_list)
		{
			pat = (Pat *) ldata(ptr);
			deref(pat, FALSE);
		}

		put_trail("deref_test", "finish");
		return test;

case t_LIST:	deref(test->t_pat, FALSE);
		newlist = makelist0();
		for_list (ptr, test->t_list)
		{
			pat = (Pat *) ldata(ptr);
			if (! pat->p_cmd)
				addtail(newlist, pat);			/* na */
			else
			{
				deref(pat, TRUE);
				addlist(newlist, break_pat(pat));	/* na */
			}
		}

		test->t_list = newlist;
		put_trail("deref_test", "finish");
		return test;

case t_EXIST:	deref(test->t_pat, FALSE);
		put_trail("deref_test", "finish");
		return test;

case t_CANDO:	deref(test->t_pat, FALSE);
		put_trail("deref_test", "finish");
		return test;

case t_OK:	deref(test->t_pat, FALSE);
		put_trail("deref_test", "finish");
		return test;

default:	printf("cake internal error: bad test type %x in deref_test\n", test->t_kind);
		break;
	}

	put_trail("deref_test", "finish");
	return (Test *) NULL;
}

Act *
prep_act(text)
reg	char	*text;
{
	reg	Act	*act;
	reg	char	*s;

	act = make(Act);
	act->a_flag = 0;

	/* strip trailing space - there is at least the newline */
	for (s = text+strlen(text)-1; isspace(*s); s--)
		*s = '\0';

	/* put just the newline back */
	strcat(s, "\n");

	/* strip spaces before flags */
	for (s = text; isspace(*s); s++)
		;

	/* process the flags */
	for (; *s != '\0'; s++)
		if (*s == '@')
			set_act(act, af_SILENT);
		or (*s == '!')
			set_act(act, af_SYSTEM);
		or (*s == '-')
			set_act(act, af_IGNORE);
		or (*s == '+')
			set_act(act, af_MINUSN);
		else
			break;

	/* strip spaces after flags */
	for (; isspace(*s); s++)
		;

	act->a_str = new_name(s);
	return act;
}

Act *
prep_script(first_act, middle_act, last_act)
reg	Act	*first_act, *last_act;
reg	List	*middle_act;
{
	reg	List	*ptr;
	reg	Act	*act;
	reg	char	*s;
	char		buf[MAXSCRIPT];

	buf[0] = '\0';
	/* handle the first action */
	for (s = first_act->a_str+1; *s == ' ' || *s == '\t'; s++)
		;

	if (strdiff(s, "\n"))
		strcat(buf, s);

	/* handle the middle actions, if any */
	for_list (ptr, middle_act)
	{
		act = (Act *) ldata(ptr);
		strcat(buf, act->a_str);
	}

	/* handle the last action as the first */
	for (s = last_act->a_str+1; *s == ' ' || *s == '\t'; s++)
		;

	if (strdiff(s, "\n"))
		strcat(buf, s);

	/* check for overflow */
	if (strlen(buf) > (unsigned)MAXSCRIPT)
	{
		printf("cake: script too long\n");
		printf("%s", buf);
		exit_cake(FALSE);
	}

	first_act->a_str = new_name(buf);
	set_act(first_act, af_SCRIPT);
	return first_act;
}
