/*
**	Module to make Cake data structures.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

Node *
make_node(name)
reg	char	*name;
{
	reg	Node	*node;

	node = make(Node);
	node->n_name    = name;

	if (name == NULL)
		return node;

	node->n_kind = n_NOWAY;		/* not yet */
	node->n_flag = 0;
	node->n_old  = makelist0();
	node->n_when = NULL;
	node->n_act  = NULL;
	node->n_new  = makelist(node);
#if 0	/* N must be 1 for the time being */
	node->n_pid  = 0;
	node->n_file = NULL;
#endif
	node_stat(node);
	node->n_utime   = 0;
	node->n_stime   = node->n_rtime;
	node->n_badguys = NULL;
	node->n_msg     = NULL;

	return node;
}

Entry *
make_dep(new, old, cond, file, colon)
reg	List	*new;				/* of Pat	*/
reg	List	*old;				/* of Pat	*/
reg	Test	*cond;
reg	char	*file;
reg	char	*colon;
{
	reg	Entry	*entry;

	entry = make(Entry);
	entry->e_new  = new;
	entry->e_old  = old;
	entry->e_cond = cond;
	entry->e_file = file;
	entry->e_dblc = streq(colon, "::");
	entry->e_when = makelist0();

	return entry;
}

Test *
make_test_mm(name)
reg	Pat	*name;
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind = t_MATCH;
	test->t_pat  = name;
	test->t_list = makelist0();

	return test;
}

Test *
make_test_m(name, varpat, pattern)
reg	Pat	*name;
reg	Pat	*varpat;
reg	Pat	*pattern;
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind = t_MATCH;
	test->t_pat  = name;
	test->t_list = makelist0();

	if (varpat == NULL)
		addtail(test->t_list, make_pat("-vX", FALSE, 0));
	else
		addtail(test->t_list, varpat);	/* no assignment */

	addtail(test->t_list, pattern);		/* no assignment */
	return test;
}

Test *
make_test_c(cmd)
reg	char	*cmd;
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind = t_CMD;
	test->t_cmd  = cmd;

	return test;
}

Test *
make_test_s(tkind, pat)
reg	T_kind	tkind;
reg	Pat	*pat;
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind = tkind;
	test->t_pat  = pat;

	return test;
}

Test *
make_test_l(pat, list)
reg	Pat	*pat;
reg	List	*list;				/* of Pat	*/
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind = t_LIST;
	test->t_pat  = pat;
	test->t_list = list;

	return test;
}

Test *
make_test_b(tkind, left, right)
reg	T_kind	tkind;
reg	Test	*left;
reg	Test	*right;
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind  = tkind;
	test->t_left  = left;
	test->t_right = right;

	return test;
}

Test *
make_test_u(tkind, left)
reg	T_kind	tkind;
reg	Test	*left;
{
	reg	Test	*test;

	test = make(Test);
	test->t_kind  = tkind;
	test->t_left  = left;
	test->t_right = (Test *) NULL;

	return test;
}

Pat *
make_pat(text, iscmd, flags)
reg	char	*text;
reg	bool	iscmd;
reg	int	flags;
{
	reg	Pat	*pat;

	pat = make(Pat);
	pat->p_str  = text;
	pat->p_cmd  = iscmd;
	pat->p_flag = flags;

	checkpatlen(text);
	return pat;
}

Act *
make_act(str, flag)
reg	char	*str;
reg	int	flag;
{
	reg	Act	*act;

	act = make(Act);
	act->a_str  = str;
	act->a_flag = flag;

	return act;
}
