/*
**	Printout routines for Cake data structures.
*/

#ifdef	CAKEDEBUG

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

char *
str_pflag(flag)
reg	int	flag;
{
	char		buf[256];

	strcpy(buf, "[");
	if (flag & nf_NONVOL)
		strcat(buf, "nonvol ");
	if (flag & nf_PRECIOUS)
		strcat(buf, "precious ");
	if (flag & nf_PSEUDO)
		strcat(buf, "pseudo ");
	if (flag & nf_REDUNDANT)
		strcat(buf, "redundant ");
	if (flag & nf_WHEN)
		strcat(buf, "when ");
	if (flag & nf_DEPNONVOL)
		strcat(buf, "depnonvol ");
	if (flag & nf_NODELETE)
		strcat(buf, "nodelete ");
	if (flag & nf_NEWFILE)
		strcat(buf, "newfile ");
	if (flag & nf_EXIST)
		strcat(buf, "exist ");
	if (flag & nf_BUSY)
		strcat(buf, "busy ");
	if (flag & nf_ERR)
		strcat(buf, "err ");
	if (flag & nf_TRACED)
		strcat(buf, "traced ");
	if (flag & nf_WARNED)
		strcat(buf, "warned ");
	if (flag & nf_ORIG)
		strcat(buf, "orig ");

	if (strlen(buf) > (unsigned)1)
		buf[strlen(buf)-1] = '\0';

	strcat(buf, "]");
	return new_name(buf);
}

char *
str_aflag(flag)
reg	int	flag;
{
	char		buf[128];

	strcpy(buf, "[");
	if (flag & af_SILENT)
		strcat(buf, "silent ");
	if (flag & af_IGNORE)
		strcat(buf, "ignore ");
	if (flag & af_MINUSN)
		strcat(buf, "minusn ");
	if (flag & af_SYSTEM)
		strcat(buf, "system ");
	if (flag & af_SCRIPT)
		strcat(buf, "script ");

	if (strlen(buf) > (unsigned)1)
		buf[strlen(buf)-1] = '\0';

	strcat(buf, "]");
	return new_name(buf);
}

void
print_pat(pat)
reg	Pat	*pat;
{
	if (pat->p_cmd)
		printf("`%s`", pat->p_str);
	else
		printf("%s", pat->p_str);

	printf(str_pflag(pat->p_flag));
}

void
print_act(act)
reg	Act	*act;
{
	printf(str_aflag(act->a_flag));
	printf("%s", act->a_str);
}

void
print_test(test)
reg	Test	*test;
{
	reg	List	*ptr;
	reg	char	*pre;
	reg	Pat	*pat;

	if (test == NULL)
	{
		printf("null");
		return;
	}

	switch (test->t_kind)
	{

case t_TRUE:	printf("true");
		break;
case t_FALSE:	printf("false");
		break;

case t_AND:	print_test(test->t_left);
		printf(" and ");
		print_test(test->t_right);
		break;

case t_OR:	print_test(test->t_left);
		printf(" or ");
		print_test(test->t_right);
		break;

case t_NOT:	printf("not ");
		print_test(test->t_left);
		break;

case t_CMD:	printf("cmd `%s`", test->t_cmd);
		break;

case t_MATCH:	printf("match ");
		print_pat(test->t_pat);
		printf(" against");
		printf(" (");
		print_pat((Pat *) first(test->t_list));
		printf(") ");
		print_pat((Pat *) last(test->t_list));
		break;

case t_LIST:	printf("list ");
		print_pat(test->t_pat);

		printf(" in (");
		pre = "";
		for_list (ptr, test->t_list)
		{
			pat = (Pat *) ldata(ptr);
			printf(pre);
			print_pat(pat);
			pre = ", ";
		}

		printf(")");
		break;

case t_EXIST:	printf("exist ");
		print_pat(test->t_pat);
		break;

case t_CANDO:	printf("cando ");
		print_pat(test->t_pat);
		break;

case t_OK:	printf("ok ");
		print_pat(test->t_pat);
		break;

default:	printf("Bad type kind %d in print_test\n", test->t_kind);

	}
}

void
print_entry(entry)
reg	Entry	*entry;
{
	reg	List	*ptr;
	reg	Pat	*pat;
	reg	Act	*act;
	reg	char	*pre;

	printf("ENTRY\nnew: ");
	pre = "";
	for_list (ptr, entry->e_new)
	{
		pat = (Pat *) ldata(ptr);
		printf(pre);
		print_pat(pat);
		pre = ", ";
	}

	printf("\nold: ");
	pre = "";
	for_list (ptr, entry->e_old)
	{
		pat = (Pat *) ldata(ptr);
		printf(pre);
		print_pat(pat);
		pre = ", ";
	}

	printf("\nwhen: ");
	pre = "";
	for_list (ptr, entry->e_when)
	{
		pat = (Pat *) ldata(ptr);
		printf(pre);
		print_pat(pat);
		pre = ", ";
	}

	printf("\ntest: ");
	print_test(entry->e_cond);

	printf("\nactions:\n");
	for_list (ptr, entry->e_act)
	{
		act = (Act *) ldata(ptr);
		print_act(act);
	}

	printf("\n");
}

char *
str_nkind(nkind)
reg	N_kind	nkind;
{
	switch (nkind)
	{
case n_OK:	return "ok";
case n_NOWAY:	return "noway";
case n_CANDO:	return "cando";
	}

	return "bizarre";
}

void
print_time(str, ntime)
reg	char	*str;
time_t		ntime;
{
	extern	char	*ctime();

	printf("%s time: %d, %s", str, ntime, ctime(&ntime));
}

void
print_node(node)
reg	Node	*node;
{
	reg	List	*ptr;
	reg	char	*pre;
	reg	Node	*bnode;
	reg	Act	*act;

	printf("\nNODE\n%s: kind %s flag %s\n", node->n_name, 
		str_nkind(node->n_kind), str_pflag(node->n_flag));
	print_time("real",  node->n_rtime);
	print_time("used",  node->n_utime);
	print_time("saved", node->n_stime);

	printf("new: ");
	pre = "";
	for_list (ptr, node->n_new)
	{
		bnode = (Node *) ldata(ptr);
		printf(pre);
		printf(bnode->n_name);
		pre = ", ";
	}

	printf("\nold: ");
	pre = "";
	for_list (ptr, node->n_old)
	{
		bnode = (Node *) ldata(ptr);
		printf(pre);
		printf(bnode->n_name);
		pre = ", ";
	}

	printf("\naction:\n");
	for_list (ptr, node->n_act)
	{
		act = (Act *) ldata(ptr);
		print_act(act);
	}

	if (node->n_badguys != (List *) NULL)
	{
		printf("bad guys: ");
		pre = "";
		for_list (ptr, node->n_badguys)
		{
			bnode = (Node *) ldata(ptr);
			printf(pre);
			printf(bnode->n_name);
			pre = ", ";
		}

		printf("\n");
	}

	if (node->n_msg != NULL)
		printf("msg: %s", node->n_msg);

	printf("\n");
}

#endif
