/*
**	Module to chase Cake dependencies.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

time_t	pick_time();		/* Defined further down */

/*
**	The main chasing function. It checks all entries to see
**	if they describe a dependency of the name. If the second arg
**	is not NULL, it is taken as a fixed choice (by another do_chase)
**	as to which entry will actually be used to generate the name.
**
**	The kinds of nodes returned by do_chase are as follows.
**	OK means that no action is needed to update the name.
**	CANDO means that the action is known and there is no reason
**	why it shouldn't succeed. NOWAY is set when neither of those
**	conditions is satisfied. nf_ERR means that some error has
**	occurred; nf_ERR nodes are never touched again; except for
**	printing the error messages themselves. (NOWAY nodes *are*
**	touched by act.c to update as many ancestors as possible.)
**
**	Whenever nf_ERR is not set, do_chase sets the utime field,
**	and the rtime field as well if the file exists.
*/
void
do_chase(node, picked)
reg	Node	*node;
reg	Entry	*picked;
{
	Env		env;
	reg	List	*ptr, *ptr1, *ptr2;
	reg	Node	*newnode, *onode;
	reg	Entry	*entry, *newentry;
	reg	Pat	*pat;
	reg	List	*entry_anay, *entry_ayea;	/* of Entry	*/
	reg	List	*miss_anay, *miss_ayea;		/* of Node	*/
	reg	List	*pot_entry;		/* of Entry	*/
	reg	List	*pot_pat;		/* of Pat	*/
	reg	List	*probe_old;		/* of Node	*/
	reg	bool	found, missing;
	reg	bool	singleton, orphan;

	put_trail("do_chase", "start");
	cdebug("chasing %s\n", node->n_name);

	if (off_node(node, nf_EXIST))
		set_node(node, nf_NEWFILE);

	/* find all entries which could generate this name */
	pot_entry = makelist0();
	pot_pat   = makelist0();
	for_list (ptr, entries)
	{
		entry = (Entry *) ldata(ptr);
		/* try everything on the left side of the entry */
		for_list (ptr1, entry->e_new)
		{
			pat = (Pat *) ldata(ptr1);
			if (match(env, node->n_name, pat))
			{
				if ((newentry = ground_entry(env, entry)) == (Entry *) NULL)
				{
					sprintf(scratchbuf, "cannot perform substitutions on entry for %s",
						node->n_name);
					add_error(node, new_name(scratchbuf), LNULL, TRUE);
					goto end;
				}

#ifdef	CAKEDEBUG
				if (entrydebug)
				{
					printf("Applicable entry\n");
					print_entry(newentry);
				}
#endif

				addtail(pot_entry, newentry);	/* na */
				addtail(pot_pat, pat);		/* na */
				break;
			}
		}
	}

	cdebug("%s: %d potentials\n", node->n_name, length(pot_entry));

	/* find out which tests are satisfied */
	entry_anay = makelist0();
	entry_ayea = makelist0();
	for_2list (ptr1, ptr2, pot_entry, pot_pat)
	{
		entry = (Entry *) ldata(ptr1);
		pat = (Pat *) ldata(ptr2);

		for_list (ptr, entry->e_when)
		{
			reg	Node	*whennode;
			reg	Pat	*whenpat;

			whenpat  = (Pat *) ldata(ptr);
			whennode = chase(whenpat->p_str, whenpat->p_flag, (Entry *) NULL);
			if (off_node(whennode, nf_ERR))
			{
				if (is_ok(whennode))
					continue;

				if (nflag && off_node(whennode, nf_WARNED))
				{
					printf("cake -n: warning, need to make %s to find dependencies\n",
						whenpat->p_str);
					set_node(whennode, nf_WARNED);
				}

				update(whennode, 100, TRUE);
			}

			if (on_node(whennode, nf_ERR) || ! is_ok(whennode))
			{
				sprintf(scratchbuf, "cannot find out if entry applies to %s",
					node->n_name);
				add_error(node, new_name(scratchbuf), makelist(whennode), TRUE);
				return;
			}
		}

#ifdef	CAKEDEBUG
		if (entrydebug)
		{
			printf("about to test condition\n");
			print_entry(entry);
		}
#endif

		entry->e_cond = deref_test(entry->e_cond);
		if (! eval(node, entry->e_cond, env))
		{
			cdebug("condition FALSE\n");
			continue;
		}

		/* here we know that this entry applies */
		deref_entry(env, entry);

		if (Lflag)
		{
			for_list (ptr, entry->e_old)
			{
				reg	Pat	*opat;

				opat = (Pat *) ldata(ptr);
				if (streq(node->n_name, opat->p_str))
				{
					cdebug("found loop in entry at %s\n", node->n_name);
					continue;
				}
			}
		}

		node->n_flag |= pat->p_flag;
		if (length(entry->e_act) == 0)
			addtail(entry_anay, entry);	/* no assigment */
		else
			addtail(entry_ayea, entry);	/* no assigment */
	}

	if (on_node(node, nf_ERR))
		return;

	cdebug("%s: no action: %d, with actions: %d\n",
		node->n_name, length(entry_anay), length(entry_ayea));

	/* All entries considered from now on are ground */

	/* see if the file is an original */
	if (length(entry_anay) == 0 && length(entry_ayea) == 0)
	{
		if (off_node(node, nf_EXIST))
		{
			cdebug("%s: noway\n", node->n_name);
			node->n_kind  = n_NOWAY;
			node->n_utime = GENESIS;
			sprintf(scratchbuf, "base file %s does not exist", node->n_name);
			add_error(node, new_name(scratchbuf), LNULL, FALSE);
			goto end;
		}

		set_node(node, nf_ORIG);
		cdebug("%s: orig ok\n", node->n_name);
		node->n_kind  = n_OK;
		node->n_utime = node->n_rtime;
		goto end;
	}

	/* see to actionless dependencies */
	missing = FALSE;
	miss_anay = NULL;
	for_list (ptr, entry_anay)
	{
		entry = (Entry *) ldata(ptr);
		for_list (ptr1, entry->e_old)
		{
			pat = (Pat *) ldata(ptr1);
			newnode = chase(pat->p_str, pat->p_flag, (Entry *) NULL);
			if (on_node(newnode, nf_ERR) || is_noway(newnode))
			{
				missing = TRUE;
				miss_anay = addtail(miss_anay, newnode);
			}

			addtail(node->n_old, newnode);	/* na */
		}
	}

	cdebug("%s: unconditional dependencies, %s\n",
		node->n_name, missing? "some missing": "all there");

	/*
	**	If necessary, choose a feasible action dependency.
	**	However, if there is only one, select it even if
	**	it is not feasible.
	*/

	found = FALSE;
	miss_ayea = NULL;
	if (picked == (Entry *) NULL)
	{
		singleton = (length(entry_ayea) == 1);
		orphan    = (length(entry_ayea) == 0);

		for_list (ptr, entry_ayea)
		{
			entry = (Entry *) ldata(ptr);
			/* first copy of this code, with chase */
			found = TRUE; /* assume it for the moment */
			probe_old = makelist0();
			for_list (ptr1, entry->e_old)
			{
				pat = (Pat *) ldata(ptr1);
				newnode = chase(pat->p_str, pat->p_flag, (Entry *) NULL);
				if (on_node(newnode, nf_ERR) || is_noway(newnode))
				{
					found = FALSE;
					miss_ayea = addtail(miss_ayea, newnode);
					if (! singleton)
						break;
				}

				addtail(probe_old, newnode);	/* na */
			}

			if (found)
				break;
		}
	}
	else
	{
		singleton = TRUE;
		orphan    = FALSE;

		cdebug("%s: was given generator\n", node->n_name);
		entry = picked;

		/* second copy of this code, with chase_node */
		found = TRUE; /* assume it for the moment */
		probe_old = makelist0();
		for_list (ptr1, entry->e_old)
		{
			pat = (Pat *) ldata(ptr1);
			newnode = chase_node(pat->p_str);
			if (on_node(newnode, nf_ERR) || is_noway(newnode))
			{
				found = FALSE;
				miss_ayea = addtail(miss_ayea, newnode);
				if (! singleton)
					break;
			}

			addtail(probe_old, newnode);	/* na */
		}
	}

	if (found)
	{
		/* we have found an applicable entry */
		cdebug("%s: found feasible generator\n", node->n_name);
		addlist(node->n_old, probe_old);	/* na */
		node->n_act = entry->e_act;
		if (picked == (Entry *) NULL)
			set_buddies(node, entry);
		picked = entry;
	}
	or (singleton)
	{
		/* we have only one (inapplicable) entry */
		/* so we shall make do as best we can */
		cdebug("%s: found single infeasible generator\n", node->n_name);
		addlist(node->n_old, probe_old);	/* na */
		node->n_act = entry->e_act;
		if (picked == (Entry *) NULL)
			set_buddies(node, entry);
		picked = entry;
	}
	or (orphan && on_node(node, nf_PSEUDO))
	{
		/* no entry is needed, act as we had one */
		found = TRUE;
		cdebug("%s: no generator, no problem\n", node->n_name);
		node->n_act = makelist0();
	}
	else
	{
		cdebug("%s: no feasible generator\n", node->n_name);
		node->n_act = makelist0();
	}

	if (length(node->n_act) > 0 && length(picked->e_old) == 0)
		node->n_utime = cake_gettime();
	else
		get_utime(node, TRUE);

	for_list (ptr, node->n_old)
	{
		newnode = (Node *) ldata(ptr);
		if (on_node(newnode, nf_NONVOL))
			set_node(node, nf_DEPNONVOL);
	}

	if (missing)
	{
		node->n_kind = n_NOWAY;
		sprintf(scratchbuf, "%s is missing the prerequisite%s %s",
			node->n_name, length(miss_anay)==1? "": "s", list_names(miss_anay));
		add_error(node, new_name(scratchbuf), miss_anay, FALSE);
	}
	or (! found)
	{
		node->n_kind = n_NOWAY;
		if (miss_ayea == (List *) NULL)
			sprintf(scratchbuf, "%s has no applicable entries with actions\n",
				node->n_name);
		else
			sprintf(scratchbuf, "%s is missing the ancestor%s %s",
				node->n_name, length(miss_ayea)==1? "": "s", list_names(miss_ayea));

		add_error(node, new_name(scratchbuf), miss_ayea, FALSE);
	}
	or (on_node(node, nf_EXIST) && node->n_utime <= node->n_rtime)
		node->n_kind = n_OK;
	or (on_node(node, nf_PSEUDO) && length(node->n_act) == 0)
	{
		node->n_kind = n_OK;
		for_list (ptr, node->n_old)
		{
			onode = (Node *) ldata(ptr);
			if (is_cando(onode))
				node->n_kind = n_CANDO;
		}
	}
	else
		node->n_kind = n_CANDO;
	
end:
#ifdef	CAKEDEBUG
	if (cakedebug)
	{
		printf("chase exit\n");
		print_node(node);
	}
#endif

	put_trail("do_chase", "finish");
}

/*
**	Calculate the "update time" of the given node. The second arg
**	tells us whether we should set a flag for nonvolatile ancestors.
*/
void
get_utime(node, planning)
reg	Node	*node;
reg	bool	planning;
{
	extern	time_t	get_youngest(), cake_gettime();
	reg	time_t	youngest;

	if ((youngest = get_youngest(node, planning)) == GENESIS)
		node->n_utime = cake_gettime();
	else
		node->n_utime = youngest;
}

/*
**	Return the time of update of the youngest dependent.
*/

time_t
get_youngest(node, planning)
reg	Node	*node;
reg	bool	planning;
{
	reg	List	*ptr;
	reg	Node	*onode;
	reg	time_t	youngest, picked_time;
	reg	List	*errnodes;

	youngest = GENESIS;
	errnodes = NULL;
	for_list (ptr, node->n_old)
	{
		onode = (Node *) ldata(ptr);
		if (on_node(onode, nf_ERR))
			errnodes = addtail(errnodes, onode);
		else
		{
			if ((picked_time = pick_time(onode, planning)) > youngest)
				youngest = picked_time;
		}
	}

	if (errnodes != (List *) NULL)
	{
		sprintf(scratchbuf, "don't know what to do with %s because of problems with %s",
			node->n_name, list_names(errnodes));
		add_error(node, new_name(scratchbuf), errnodes, TRUE);
	}

	return youngest;
}

/*
**	Take care of the names mentioned alongside node's
**	on the left hand side of the given ground entry.
**
**	If a node exists for one of these names we know that
**	a producer must have been chosen for it, and we know that
**	it is not this rule (if it were, then the caller would not
**	have done anything about buddies). This is an inconsistency.
*/
void
set_buddies(node, entry)
reg	Node	*node;
reg	Entry	*entry;
{
	extern	Node	*chase_node(), *chase();
	reg	List	*list;
	reg	List	*ptr;
	reg	Node	*newnode;
	reg	Pat	*pat;
	reg	List	*errnodes;

	put_trail("set_buddies", "start");
	list = makelist(node);
	errnodes = NULL;
	for_list (ptr, entry->e_new)
	{
		pat = (Pat *) ldata(ptr);
		if (streq(pat->p_str, node->n_name))
			continue;
		
		if ((newnode = chase_node(pat->p_str)) != (Node *) NULL)
			errnodes = addtail(errnodes, newnode);
		else
		{
			newnode = chase(pat->p_str, pat->p_flag, entry);
			addtail(list, newnode);	/* no assigment */
		}
	}

	for_list (ptr, list)
	{
		newnode = (Node *) ldata(ptr);
		newnode->n_new = list;

		if (errnodes != (List *) NULL)
		{
			sprintf(scratchbuf, "cannot update %s because of interference with %s %s",
				newnode->n_name, (length(errnodes) == 1)? "buddy": "buddies",
				list_names(errnodes));
			add_error(node, new_name(scratchbuf), errnodes, TRUE);
		}
	}

	put_trail("set_buddies", "finish");
}

/*
**	Pick the appropriate time to use in decisions
**	about dependencies. The algorithms differ in the
**	twp phases: in the execution phase real times
**	override any calculated times.
*/

time_t
pick_time(node, planning)
reg	Node	*node;
reg	bool	planning;
{
	if (off_node(node, nf_EXIST))
		return node->n_utime;
	
	if (planning)
		return max(node->n_rtime, node->n_utime);

	return node->n_rtime;
}

/*
**	Chase down the ancestors of the given name.
**	Return the node describing the dependency graph.
**
**	Note that this is merely an interface function,
**	serving to cache previous results and to detect cycles.
*/

char	*goal_stack[MAXGSTACK];
int	goal_stackp = 0;

Node *
chase(name, flag, picked)
reg	char	*name;
reg	bool	flag;
reg	Entry	*picked;
{
	extern	Table	node_tab;
	extern	void	do_chase();
	extern	char	*find_circle();
	reg	Node	*node;

	if ((node = (Node *) lookup_table(node_tab, name)) == NULL)
	{
		node = make_node(name);
		if (goal_stackp >= MAXGSTACK)
		{
			printf("cake: dependency nesting level %d too deep\n", goal_stackp);
			exit_cake(FALSE);
		}

		goal_stack[goal_stackp++] = name;
#ifdef	CAKEDEBUG
		cdebug("stack[%d] = %s\n", goal_stackp-1, goal_stack[goal_stackp-1]);
#endif
		set_node(node, nf_BUSY);
		insert_table(node_tab, node);
		do_chase(node, picked);
		--goal_stackp;
		reset_node(node, nf_BUSY);
	}
	else
	{
		cdebug("Using cache for %s\n", name);
		if (on_node(node, nf_BUSY))
		{
			node->n_flag |= flag;
			sprintf(scratchbuf, "%s depends upon itself %s", node->n_name,
				find_circle(node->n_name));
			add_error(node, new_name(scratchbuf), LNULL, TRUE);
			put_trail("chase", "finish");
			return node;
		}
	}

	node->n_flag |= flag;
	put_trail("chase", "finish");
	return node;
}
