/*
**	Error handling module.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

/*
**	This function prints out the history of errors
**	that prevent the proper update of the given node.
*/

void
trace_errs(node)
reg	Node	*node;
{
	reg	List	*ptr;
	reg	Node	*bnode;

	if (on_node(node, nf_TRACED))
		return;

	set_node(node, nf_TRACED);
	for_list (ptr, node->n_badguys)
	{
		bnode = (Node *) ldata(ptr);
		trace_errs(bnode);
	}

	if (strdiff(node->n_name, CHASEROOT))
		printf("%s", node->n_msg);
}

/*
**	This function adds msg to the list of error messages
**	to be printed by trace_errs for the given node.
*/
void
add_error(node, msg, badguys, iserror)
reg	Node	*node;
reg	char	*msg;
reg	List	*badguys;
reg	int	iserror;
{
	char	buf[MAXSIZE];

	if (node->n_msg == NULL)
		node->n_msg = "";

	if (iserror)
		set_node(node, nf_ERR);

	sprintf(buf, "%scake: %s\n", node->n_msg, msg);
	if (strlen(buf) >= (unsigned)MAXSIZE)
	{
		fprintf(stderr, "cake internal error: buffer overflow in add_error\n");
		exit_cake(FALSE);
	}

	node->n_msg = new_name(buf);
	node->n_badguys = addlist(node->n_badguys, badguys);
}

char *
find_circle(name)
reg	char	*name;
{
	extern	int	goal_stackp;
	extern	char	*goal_stack[];
	char		buf[MAXSIZE];
	reg	int	i;

	cdebug("stackp = %d\n", goal_stackp);
	cdebug("looking for %s\n", goal_stack[goal_stackp-1]);

	if (goal_stack[goal_stackp-1] == name)
		return "directly";

	for (i = 0; i < goal_stackp; i++)
	{
		cdebug("checking %d: %s\n", i, goal_stack[i]);
		if (goal_stack[i] == name)
		{
			cdebug("hit\n");
			strcpy(buf, "through");
			for (; i < goal_stackp; i++)
			{
				strcat(buf, " ");
				strcat(buf, goal_stack[i]);
			}

			if (strlen(buf) >= (unsigned)MAXSIZE)
			{
				fprintf(stderr, "cake internal error: buffer overflow in find_circle\n");
				exit_cake(FALSE);
			}

			return new_name(buf);
		}
	}

	fprintf(stderr, "cake internal error: no circularity in find_circle\n");
	exit_cake(TRUE);
	return "";		/* to satisfy lint */
}

char *
list_names(list)
reg	List	*list;		/* of Node	*/
{
	reg	List	*ptr;
	reg	Node	*node;
	reg	char	*sep;
	char		buf[MAXSIZE];

	buf[0] = '\0';
	sep = "";
	for_list (ptr, list)
	{
		node = (Node *) ldata(ptr);
		strcat(buf, sep);
		strcat(buf, node->n_name);
		sep = " ";
	}

	if (strlen(buf) >= (unsigned)MAXSIZE)
	{
		fprintf(stderr, "cake internal error: buffer overflow in list_names\n");
		exit_cake(FALSE);
	}

	return new_name(buf);
}
