/*
**	Module to execute Cake actions.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

/*
**	Update the node by executing the attached actions.
**	Actually, all the buddies will be updated, and
**	no action will be executed unless it is necessary.
**
**	The level of a file is one greater than the level of its
**	parent in the chase graph; the level of !MAINCAKE! is 0.
**	This controls the printing of messages about the primary
**	targets.
*/
void
update(node, level, force_exec)
reg	Node	*node;
reg	int	level;
reg	bool	force_exec;
{
	extern		save_novol();
	extern	bool	diff_novol();
	extern		carry_out();
	extern	char	*list_names();
	reg	Node	*bnode, *onode;
	reg	List	*ptr, *ptr1;
	reg	bool	oksofar, needact, mayskip;
	reg	List	*errnodes;
	Wait		code;

	put_trail("update", "start");
	cdebug("hunting %s at level %d\n", node->n_name, level);

	/* force printing of "up-to-date" messages */
	if (level == 0 && is_ok(node))
		node->n_kind = n_CANDO;
		
	if (off_node(node, nf_ERR) && is_ok(node))
	{
		cdebug("everything a-ok, nothing to do\n");
		if (level == 1)
		{
			if (on_node(node, nf_ORIG))
			{
				printf("cake: target %s has no ancestors\n", node->n_name);
				node->n_kind = n_NOWAY;
			}
			or (! xflag)
				printf("cake: %s is up to date\n", node->n_name);
		}

		put_trail("update", "finish");
		return;
	}

#ifdef	CAKEDEBUG
	if (cakedebug)
	{
		printf("level %d starting update of\n", level);
		print_node(node);
	}
#endif

	if (on_node(node, nf_ERR))
	{
		trace_errs(node);
		return;
	}

	errnodes = NULL;

	/* skip the actions if they are useless or not needed */
	oksofar = TRUE;
	needact = FALSE;
	mayskip = FALSE;
	for_list (ptr, node->n_new)
	{
		bnode  = (Node *) ldata(ptr);

		if (on_node(bnode, nf_ERR))
		{
			oksofar = FALSE;
			errnodes = addtail(errnodes, bnode);
		}
		else
		{
			if (off_node(bnode, nf_EXIST) || bnode->n_rtime < bnode->n_utime)
				needact = TRUE;
			
			if (on_node(bnode, nf_DEPNONVOL))
				mayskip = TRUE;
		}
	}

	if (! oksofar)
	{
		cdebug("error in buddies\n");
		if (vflag && strdiff(node->n_name, CHASEROOT))
		{
			sprintf(scratchbuf, "cannot proceed with update of %s because of problems with %s %s",
				node->n_name, (length(errnodes) == 1)? "buddy": "buddies", list_names(errnodes));
			add_error(node, new_name(scratchbuf), errnodes, TRUE);
			trace_errs(node);
		}

		goto endit;
	}

	if (! needact)
	{
		cdebug("no need for actions\n");
		if (strdiff(node->n_name, CHASEROOT))
			printf("cake: %s is up to date\n", node->n_name);

		goto endit;
	}

	/* update all ancestors of all buddies */
	for_list (ptr, node->n_new)
	{
		bnode  = (Node *) ldata(ptr);
		for_list (ptr1, bnode->n_old)
		{
			onode = (Node *) ldata(ptr1);
			update(onode, level+1, force_exec);
			if (on_node(onode, nf_ERR) || ! is_ok(onode))
			{
				oksofar = FALSE;
				errnodes = addtail(errnodes, onode);
			}
		}
	}

	/* skip the actions if any ancestors are missing */
	if (! oksofar)
	{
		cdebug("error in prerequisites\n");
		if (vflag && strdiff(node->n_name, CHASEROOT))
		{
			sprintf(scratchbuf, "cannot proceed with update of %s because of problems with %s %s",
				node->n_name, (length(errnodes) == 1)? "ancestor": "ancestors", list_names(errnodes));
			add_error(node, new_name(scratchbuf), errnodes, TRUE);
			trace_errs(node);
		}

		goto endit;
	}

	/* skip the actions; node is a missing base file */
	if (is_noway(node))
	{
		cdebug("error in planning\n");
		trace_errs(node);
		goto endit;
	}

	/* find any NOWAY buddies; these may have n_old == [] */
	for_list (ptr, node->n_new)
	{
		bnode  = (Node *) ldata(ptr);
		if (is_noway(bnode))
		{
			oksofar = FALSE;
			errnodes = addtail(errnodes, bnode);
		}
	}

	/* skip the actions if there are any NOWAY buddies */
	if (! oksofar)
	{
		cdebug("error in buddy planning\n");
		sprintf(scratchbuf, "dare not update %s because of possible effects on %s %s", node->n_name,
			(length(errnodes) == 1)? "buddy": "buddies", list_names(errnodes));
		add_error(node, new_name(scratchbuf), LNULL, TRUE);
		trace_errs(node);
		goto endit;
	}

	/* reevaluate the necessity to act in the light of the */
	/* actions taken to update possibly nonvolatile ancestors */
	if (mayskip)
	{
		needact = FALSE; /* assume so for the moment */
		cdebug("deciding whether to skip\n");
		for_list (ptr, node->n_new)
		{
			bnode  = (Node *) ldata(ptr);

			if (on_node(bnode, nf_DEPNONVOL))
			{
				cdebug("considering %s\n", bnode->n_name);
				get_utime(bnode, FALSE);
			}

			if (off_node(bnode, nf_EXIST) || bnode->n_rtime < bnode->n_utime)
			{
				cdebug("%s needs action\n", bnode->n_name);
				needact = TRUE;
			}
		}
	}

	if (! needact)
	{
		cdebug("no need for actions after all\n");
		if (strdiff(node->n_name, CHASEROOT))
			printf("cake: %s is up to date\n", node->n_name);

		goto endit;
	}

	if (tflag)
	{
		/* instead of action just touch the targets */
		for_list (ptr, node->n_new)
		{
			bnode = (Node *) ldata(ptr);
			if (on_node(bnode, nf_PSEUDO))
				continue;

			if (rflag)
				cake_utimes(bnode, bnode->n_utime);
			else
				cake_utimes(bnode, GENESIS);
			
			printf("touch %s\n", bnode->n_name);
		}
	}
	else
	{
		/* prepare for actions */
		for_list (ptr, node->n_new)
		{
			bnode = (Node *) ldata(ptr);
			if (on_node(bnode, nf_NONVOL))
				save_novol(bnode);
		}

		/* execute actions */
		code.w_status = carry_out(node, force_exec);
		if (code.w_status != 0)
		{
			oksofar = FALSE;
			sprintf(scratchbuf, "error in actions for %s", node->n_name);
			for_list (ptr, node->n_new)
			{
				bnode = (Node *) ldata(ptr);
				add_error(bnode, new_name(scratchbuf), LNULL, TRUE);
			}

			if (! xflag)
			{
				if (code.w_termsig == 0)
					printf("*** Error code %d\n", code.w_retcode);
				else
					printf("*** Termination code %d\n", code.w_termsig);
			}
			
			cake_error(node);
			goto endit;
		}

		/* clean up after actions */
		for_list (ptr, node->n_new)
		{
			bnode = (Node *) ldata(ptr);
			if (on_node(bnode, nf_NONVOL) && ! diff_novol(bnode))
				node_resetstat(bnode);
			or (rflag)
				node_setstat(bnode);
			else
				node_stat(bnode);
		}
	}

endit:

	cdebug("update level %d finished\n", level);
	for_list (ptr, node->n_new)
	{
		bnode = (Node *) ldata(ptr);
		if (oksofar)
		{
			if (on_node(bnode, nf_EXIST) || on_node(bnode, nf_PSEUDO) || nflag)
				bnode->n_kind = n_OK;
			else
			{
				set_node(bnode, nf_ERR);
				if (length(node->n_act) == 0)
					sprintf(scratchbuf, "no actions to make %s with", bnode->n_name);
				else
					sprintf(scratchbuf, "action did not create %s", bnode->n_name);

				add_error(bnode, new_name(scratchbuf), LNULL, TRUE);
				trace_errs(bnode);
			}
		}

#ifdef	CAKEDEBUG
		if (cakedebug)
			print_node(bnode);
#endif
	}

	put_trail("update", "finish");
}

/*
**	Execute the plan prepared by chase.c
**	Prevent the deletion of the primary targets by cleanup.
*/
void
execute(root)
reg	Node	*root;
{
	reg	List	*ptr;
	reg	Node	*target;

#ifdef	CAKEDEBUG
	if (cakedebug)  {			/* BRL */
		cdebug("execute plan:\n");
		print_node(root);
	}
#endif
	for_list (ptr, root->n_old)
	{
		target = (Node *) ldata(ptr);
		set_node(target, nf_NODELETE);
	}

	update(root, 0, FALSE);
}

/*
**	Carry out the specified actions and return the exit code.
**	Note that an empty list of actions is perfectly acceptable.
*/

#define	START_SCRIPT	"{"
#define	FINISH_SCRIPT	"}"

int
carry_out(node, force_exec)
reg	Node	*node;
reg	bool	force_exec;
{
	extern	char	*expand_cmds();
	extern		action();
	reg	List	*ptr;
	reg	Act	*act;
	reg	Node	*bnode;
	int		code;

#ifdef	CAKEDEBUG
	if (cakedebug)  {			/* BRL */
		cdebug("carry_out plan:\n");
		print_node(node);
	}
#endif
	if (Gflag && ! nflag)
	{
		for_list (ptr, node->n_new)
		{
			bnode = (Node *) ldata(ptr);
			if (on_node(bnode, nf_EXIST))
			{
				cdebug("removing %s for -G\n", bnode->n_name);
				cake_remove(bnode->n_name);
			}
		}
	}

	for_list (ptr, node->n_act)
	{
		act = (Act *) ldata(ptr);

		if (nflag)
		{
			if (off_act(act, af_MINUSN) && ! force_exec)
				show_act(act->a_str, (char *) NULL);
			else
			{
				reset_act(act, af_SILENT);
				printf("executing ...\n");
				code = action(act, node);
				printf("... done code=x%x\n", code);
				if (code != 0)
					return code;
			}

			continue;
		}

		code = action(act, node);
		if (code != 0)
			return code;
	}

	code = 0;
	return code;
}

/*
**	Execute the given command and return its status,
**	modified by flags and prefixes.
*/

int
action(act, node)
reg	Act	*act;
reg	Node	*node;
{
	extern	int	cake_proc();
	extern	int	cake_wait();
	int		code;
	reg	A_kind	type;
	reg	int	pid;
	reg	char	*after;

	if( cakedebug )  {			/* BRL */
		cdebug("action(): ", act->a_str, node);
		print_act(act);
		cdebug("\n");
	}
	if( act->a_str == NULL || strlen(act->a_str) == 0 )  {	/* BRL */
		printf("NOTE:  Null action skipped\n");
		code = 0;
		return(code);
	}
	put_trail("action", "start");
	after = expand_cmds(act->a_str);
	if (! (on_act(act, af_SILENT) || sflag))
		show_act(act->a_str, after);
	act->a_str = after;

	if (on_act(act, af_SCRIPT))
		type = Script;
	or (on_act(act, af_SYSTEM))
		type = System;
	else
		type = Exec;

	pid = cake_proc(act->a_str, type, (char *) NULL, node, (int (*)()) NULL, (List *) NULL);
	code = cake_wait(pid);

	if (on_act(act, af_IGNORE) || iflag)
		code = 0;

	if (code != 0 && ! kflag)
		exit_cake(FALSE);

	put_trail("action", "finish");
	return code;
}

/*
**	Print an action in the format specified by the options.
**	The two args are the action before and after expansion.
**	The second may be NULL, in which case show_act does the
**	expansion itself.
*/
void
show_act(before, after)
reg	char	*before;
reg	char	*after;
{
	extern	char	*squeeze();
	reg	char	*form;

	if (bflag)
		form = before;
	or (after != NULL)
		form = after;
	else
		form = expand_cmds(before);

	if (wflag)
		printf("%s", form);
	else
		printf("%s", squeeze(form));
}

/*
**	Reduce the width of the given command string
**	by squeezing out extra spaces and tabs.
*/

char *
squeeze(cmd)
reg	char	*cmd;
{
	char		buf[MAXSIZE];
	reg	char	*s;
	reg	int	i, oldi;
	reg	bool	inquotes;
	reg	bool	insingle;
	reg	bool	indouble;
	reg	bool	lastblank;

	s = cmd;
	i = 0;
	lastblank = FALSE;
	while (*s != '\0')
	{
		while (*s != '\0' && (*s == ' ' || *s == '\t'))
			s++;

		if (lastblank && (*s == '\n' || *s == '\r' || *s == '\f'))
			i--;

		oldi = i;
		inquotes = FALSE;
		for (; *s != '\0' && ((*s != ' ' && *s != '\t') || inquotes); s++)
		{
			if (*s == '\\')
			{
				buf[i++] = *s;
				if (s[1] != '\0')
					buf[i++] = *++s;
			}
			else
			{
				if (*s == '"' && ! (inquotes && insingle))
				{
					inquotes = ! inquotes;
					indouble = TRUE;
					insingle = FALSE;
				}
				or (*s == '\'' && ! (inquotes && indouble))
				{
					inquotes = ! inquotes;
					insingle = TRUE;
					indouble = FALSE;
				}

				buf[i++] = *s;
			}
		}

		if (i == oldi || buf[i-1] == '\n' || buf[i-1] == '\r' || buf[i-1] == '\f')
			lastblank = FALSE;
		else
		{
			lastblank = TRUE;
			buf[i++] = ' ';
		}
	}

	buf[i] = '\0';
	if (i >= MAXSIZE)
	{
		fprintf(stderr, "cake internal error: command '%s' too long\n", buf);
		exit_cake(FALSE);
	}

	while (buf[i-1] == ' ' || buf[i-1] == '\t')
		buf[--i] = '\0';

	return new_name(buf);
}

/*
**	Clean up after an error or interrupt.
*/
void
cake_error(node)
reg	Node	*node;
{
	reg	List	*ptr, *ptr1;
	reg	Node	*bnode, *onode;

	for_list (ptr, node->n_new)
	{
		bnode = (Node *) ldata(ptr);
		for_list (ptr1, bnode->n_old)
		{
			onode = (Node *) ldata(ptr1);
			set_node(onode, nf_NODELETE);
		}

		if (on_node(bnode, nf_EXIST) && off_node(bnode, nf_PRECIOUS))
			cake_remove(bnode->n_name);
	}
}

/*
**	Clean up after all the fuss.
*/
void
cleanup()
{
	extern	List	*get_allnodes();
	reg	List	*ptr, *ptr1;
	reg	List	*nodes;
	reg	Node	*node, *onode;

	cdebug("cleanup:\n");

	if (nflag)
		return;

	nodes = get_allnodes();
	for_list (ptr, nodes)
	{
		node = (Node *) ldata(ptr);
		cdebug("considering %s: ", node->n_name);

		if (off_node(node, nf_EXIST))
		{
			cdebug("nonexistent\n");
			continue;
		}

		if (on_node(node, nf_ERR) || is_noway(node))
		{
			cdebug("errors or noway\n");
			continue;
		}

		if (! dflag && off_node(node, nf_REDUNDANT))
		{
			cdebug("no flag\n");
			continue;
		}

		/* file exists and OK, shall we delete it ? */

		/* not if we thought earlier it need it */
		if (on_node(node, nf_NODELETE))
		{
			cdebug("nodelete flag\n");
			continue;
		}

		/* not if it cannot be regenerated at all */
		if (length(node->n_act) == 0)
		{
			cdebug("nonregenerable\n");
			continue;
		}

		/* not if it cannot be regenerated as is */
		if ((node->n_utime < node->n_rtime) && off_node(node, nf_NEWFILE))
		{
			cdebug("nonregenerable as is\n");
			continue;
		}

		/* or if its ancestors are not all OK */
		for_list (ptr1, node->n_old)
		{
			onode = (Node *) ldata(ptr1);
			if (! is_ok(onode))
			{
				cdebug("ancestor not ok\n");
				goto nextnode;
			}
		}

		cdebug("DELETED\n");
		cake_remove(node->n_name);
	
	nextnode:	;
	}
}
