/*
**	Module to handle Cake's tests.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

bool
eval(node, test, env)
reg	Node	*node;
reg	Test	*test;
Env		env;
{
	extern	char	*expand_cmds();
	extern	int	cake_proc();
	extern	int	cake_wait();
	extern	Node	*chase();
	extern	bool	get_stat();
	extern	bool	exist();
	extern	char	*ground();
	char		buf[256];
	int		status;
	reg	List	*ptr;
	reg	Pat	*pat;
	reg	char	*text1, *text2;
	reg	char	*cmd;
	reg	Node	*chasenode;
	reg	int	result;
	reg	int	pid;

	if (test == (Test *) NULL)
		return TRUE;

#ifdef	CAKEDEBUG
	if (cakedebug)
	{
		printf("testing ");
		print_test(test);
		printf("\n");
	}
#endif

	switch (test->t_kind)
	{

case t_TRUE:	return TRUE;
case t_FALSE:	return FALSE;
case t_AND:	return eval(node, test->t_left, env) && eval(node, test->t_right, env);
case t_OR:	return eval(node, test->t_left, env) || eval(node, test->t_right, env);
case t_NOT:	return ! eval(node, test->t_left, env);

case t_CMD:	if (get_stat(test->t_cmd, &status))
		{
			test->t_kind = (status == 0)? t_TRUE: t_FALSE;
			cdebug("test cmd cache %s: %s\n", test->t_cmd,
				(status == 0)? "True": "False");
			return (status == 0)? TRUE: FALSE;
		}

		cmd = expand_cmds(ground(env, test->t_cmd));
		pid = cake_proc(cmd, Exec, "/dev/null", (Node *) NULL,
			(int (*)()) NULL, (List *) NULL);
		status = cake_wait(pid);
		new_stat(test->t_cmd, status);
		test->t_kind = (status == 0)? t_TRUE: t_FALSE;
		cdebug("test cmd %s: %s\n", test->t_cmd,
			(status == 0)? "True": "False");
		return (status == 0)? TRUE: FALSE;

case t_MATCH:	text1 = (char *) first(test->t_list);	/* -vX	*/
		text2 = (char *) last(test->t_list);	/* file	*/
		/* e.g.	sub -vX X.c NULL file.c */
		sprintf(buf, "sub %s %s NULL %s > /dev/null",
			text1, text2, test->t_pat->p_str);

		cmd = new_name(buf);
		cdebug("matching command: %s\n", cmd);
		if (get_stat(cmd, &status))
		{
			test->t_kind = (status == 0)? t_TRUE: t_FALSE;
			cdebug("test cmd cache %s: %s\n", test->t_cmd,
				(status == 0)? "True": "False");
			return (status == 0)? TRUE: FALSE;
		}

		pid = cake_proc(cmd, Exec, "/dev/null", (Node *) NULL,
			(int (*)()) NULL, (List *) NULL);
		status = cake_wait(pid);
		new_stat(test->t_cmd, status);
		test->t_kind = (status == 0)? t_TRUE: t_FALSE;
		cdebug("test cmd %s: %s\n", test->t_cmd,
			(status == 0)? "True": "False");
		return (status == 0)? TRUE: FALSE;

case t_LIST:	for_list (ptr, test->t_list)
		{
			pat = (Pat *) ldata(ptr);
			if (streq(test->t_pat->p_str, pat->p_str))
				return TRUE;
		}

		return FALSE;

case t_EXIST:	result = exist(test->t_pat->p_str);
		cdebug("test exist %s: %s\n", test->t_pat->p_str,
			result? "True": "False");
		return result;

case t_CANDO:	chasenode = chase(test->t_pat->p_str, 0, (Entry *) NULL);
		if (on_node(chasenode, nf_ERR))
		{
			sprintf(scratchbuf, "cannot evaluate 'cando %s' test for %s",
				chasenode->n_name, node->n_name);
			add_error(node, new_name(scratchbuf), LNULL, TRUE);
		}

		result = is_ok(chasenode) || is_cando(chasenode);
		cdebug("test cando %s: %s\n", test->t_pat->p_str,
			result? "True": "False");
		return result;

case t_OK:	chasenode = chase(test->t_pat->p_str, 0, (Entry *) NULL);
		if (on_node(chasenode, nf_ERR))
		{
			sprintf(scratchbuf, "cannot evaluate 'ok %s' test for %s",
				chasenode->n_name, node->n_name);
			add_error(node, new_name(scratchbuf), LNULL, TRUE);
		}

		result = is_ok(chasenode);
		cdebug("test ok %s: %s\n", test->t_pat->p_str,
			result? "True": "False");
		return result;

default:	fprintf(stderr, "cake internal error: invalid test type %x in eval\n",
			test->t_kind);
		exit_cake(TRUE);
	}

	/*NOTREACHED*/
	return FALSE;
}
