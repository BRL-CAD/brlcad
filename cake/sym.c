/*
**	Symbol table module
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

typedef	struct	s_out
{
	char	*o_cmd;
	char	*o_out;
} Out;

typedef	struct	s_stat
{
	char	*s_cmd;
	int	s_stat;
} Stat;

extern	int	hash();
extern	Cast	name_key();	extern	bool	name_equal();
extern	Cast	node_key();	extern	bool	node_equal();
extern	Cast	out_key();	extern	bool	out_equal();
extern	Cast	stat_key();	extern	bool	stat_equal();
Table	name_tab = { SIZE, NULL, name_key, hash, name_equal };
Table	node_tab = { SIZE, NULL, node_key, hash, node_equal };
Table	out_tab  = { SIZE, NULL, out_key,  hash, out_equal  };
Table	stat_tab = { SIZE, NULL, stat_key, hash, stat_equal };

/*
**	Initialize the name and the command tables.
*/

void
init_sym()
{
	init_table(name_tab);
	init_table(node_tab);
	init_table(out_tab);
	init_table(stat_tab);
}

/**********************************************************************/
/*	Name table						      */

/*
**	Save the given string in the name table if not already there;
**	return its new address. This address is unique, so comparing
**	two identifiers for equality can be done by comparing their
**	addresses.
*/

char *
new_name(str)
reg	char	*str;
{
	reg	Cast	old;
	reg	char	*copy;

	if ((old = lookup_table(name_tab, str)) != (Cast)NULL)
		return (char *) old;

	copy = (char *) newmem(strlen(str) + 1);
	strcpy(copy, str);
	insert_table(name_tab, copy);

	return copy;
}

Cast
name_key(entry)
reg	Cast	entry;
{
	return entry;
}

bool
name_equal(key1, key2)
reg	Cast	key1, key2;
{
	return streq((char *) key1, (char *) key2);
}

/**********************************************************************/
/*	Node table						      */

/*
**	Th insertion function for the node table is chase(),
**	which is in chase.c with the rest of the chase stuff.
*/

/*
**	This function merely reports on the results
**	of past calls to chase.
*/

Node *
chase_node(name)
reg	char	*name;
{
	return (Node *) lookup_table(node_tab, name);
}

/*
**	Return a list of all the nodes of this run.
*/

List *
get_allnodes()
{
	return contents_table(node_tab);
}

Cast
node_key(entry)
reg	Cast	entry;
{
	return (Cast) ((Node *) entry)->n_name;
}

bool
node_equal(key1, key2)
reg	Cast	key1, key2;
{
#ifdef	EXTRACHECK
	if (key1 != key2 && streq((char *) key1, (char *) key2))
	{
		fprintf(stderr, "cake internal error: inconsistency in node_equal\n");
		exit_cake(TRUE);
	}
#endif

	return key1 == key2;
}

/**********************************************************************/
/*	Command output table					      */

#ifdef	STATS_FILE
int	out_tried = 0;
int	out_found = 0;
#endif

/*
**	Save a command and its output.
*/
void
new_out(cmd, output)
reg	char	*cmd;
reg	char	*output;
{
	reg	Out	*out;

	out = make(Out);
	out->o_cmd = cmd;
	out->o_out = output;
	insert_table(out_tab, out);
}

/*
**	Return the output if any associated with a given command.
*/

char *
get_out(cmd)
reg	char	*cmd;
{
	reg	Out	*out;

#ifdef	STATS_FILE
	out_tried++;
#endif

	if ((out = (Out *) lookup_table(out_tab, cmd)) == NULL)
		return NULL;

#ifdef	STATS_FILE
	out_found++;
#endif

	return out->o_out;
}

Cast
out_key(entry)
reg	Cast	entry;
{
	return (Cast) ((Out *) entry)->o_cmd;
}

bool
out_equal(key1, key2)
reg	Cast	key1, key2;
{
#ifdef	EXTRACHECK
	if (key1 != key2 && streq((char *) key1, (char *) key2))
	{
		fprintf(stderr, "cake internal error: inconsistency in out_equal\n");
		exit_cake(TRUE);
	}
#endif

	return key1 == key2;
}

/**********************************************************************/
/*	Command status table					      */

#ifdef	STATS_FILE
int	stat_tried = 0;
int	stat_found = 0;
#endif

/*
**	Save a command and its status.
*/
void
new_stat(cmd, status)
reg	char	*cmd;
reg	int	status;
{
	reg	Stat	*stat;

	stat = make(Stat);
	stat->s_cmd  = cmd;
	stat->s_stat = status;
	insert_table(stat_tab, stat);
}

/*
**	Return the status if any associated with a given command.
*/

bool
get_stat(cmd, code)
reg	char	*cmd;
reg	int	*code;
{
	reg	Stat	*stat;

#ifdef	STATS_FILE
	stat_tried++;
#endif

	if ((stat = (Stat *) lookup_table(stat_tab, cmd)) == NULL)
		return FALSE;

#ifdef	STATS_FILE
	stat_found++;
#endif

	*code = stat->s_stat;
	return TRUE;
}

Cast
stat_key(entry)
reg	Cast	entry;
{
	return (Cast) ((Stat *) entry)->s_cmd;
}

bool
stat_equal(key1, key2)
reg	Cast	key1, key2;
{
#ifdef	EXTRACHECK
	if (key1 != key2 && streq((char *) key1, (char *) key2))
	{
		fprintf(stderr, "cake internal error: inconsistency in stat_equal\n");
		exit_cake(TRUE);
	}
#endif

	return key1 == key2;
}
