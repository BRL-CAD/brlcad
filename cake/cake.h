/*
**	Cake definitions.
**
**	$Header$
*/

#include	<sys/types.h>
#include	<stdio.h>
#include	"std.h"
#include	"conf.h"
#include	"wait.h"
#include	"list.h"
#include	"table.h"

#define	nf_NONVOL	0x0001
#define	nf_PRECIOUS	0x0002
#define	nf_PSEUDO	0x0004
#define	nf_REDUNDANT	0x0008
#define	nf_WHEN		0x0010
#define	nf_DEPNONVOL	0x0020
#define	nf_NODELETE	0x0040
#define	nf_NEWFILE	0x0080
#define	nf_EXIST	0x0100
#define	nf_BUSY		0x0200
#define	nf_ERR		0x0400
#define	nf_TRACED	0x0800
#define	nf_WARNED	0x1000
#define	nf_ORIG		0x2000

#define	af_SILENT	0x0001
#define	af_IGNORE	0x0002
#define	af_MINUSN	0x0004
#define	af_SYSTEM	0x0008
#define	af_SCRIPT	0x0010

/* type of internal patterns */
typedef	struct	s_pat
{
	char	*p_str;
	short	p_flag;
	short	p_cmd;
} Pat;

/* type of internal actions */
typedef	struct	s_act
{
	char	*a_str;
	short	a_flag;
} Act;

/* kinds of actions */
typedef	enum
{
	Exec, System, Script
} A_kind;

/* kinds of tests */
typedef	enum
{
	t_TRUE, t_FALSE, t_AND, t_OR, t_NOT,
	t_CMD, t_MATCH, t_LIST, t_EXIST, t_CANDO, t_OK
} T_kind;

/* type of tests */
typedef	struct	s_test
{
	T_kind		t_kind;
	Pat		*t_pat;
	List		*t_list;	/* of Pat(L), char(M)	*/
	char		*t_cmd;
	struct	s_test	*t_left;
	struct	s_test	*t_right;
} Test;

/* type of substitution variables */
typedef	struct	s_var
{
	char	*v_val;
	bool	v_bound;
} Var;

#define	NOVAR		10
#define	MAXVAR		11

/* type of substitution environments */
typedef	Var	Env[MAXVAR];

/* type of entries */
typedef	struct	s_entry
{
	char	*e_file;
	List	*e_new;				/* of Pat	*/
	List	*e_old;				/* of Pat	*/
	List	*e_when;			/* of Pat	*/
	List	*e_act;				/* of Act	*/
	Test	*e_cond;
	bool	e_dblc;
} Entry;

typedef	enum
{
	n_OK, n_CANDO, n_NOWAY
} N_kind;

/* type of nodes in the dependency graph */
typedef	struct	s_node
{
	char	*n_name;	/* name of the node		*/
	N_kind	n_kind;		/* current status of the chase	*/
	int	n_flag;		/* the applicable nf_[A-Z]*	*/
	time_t	n_rtime;	/* Real modify time if exist	*/
	time_t	n_stime;	/* Saved copy of n_rtime	*/
	time_t	n_utime;	/* last ancestor Update time	*/
	List	*n_new;		/* left side or rule, of Node	*/
	List	*n_old;		/* right side of rule, of Node	*/
	List	*n_when;	/* starred right side, of Pat	*/
	List	*n_act;		/* the actions, of Act		*/
#if 0	/* N must be 1 for the time being */
	int	n_pid;		/* current update process	*/
	char	*n_file;	/* name of output file		*/
#endif
	char	*n_msg;		/* error message, if nf_ERR	*/
	List	*n_badguys;	/* error causes, if nf_ERR	*/
} Node;

typedef	struct	s_proc
{
	int	pr_pid;
	bool	pr_run;
	Wait	pr_stat;
	int	(*pr_func)();
	List	*pr_args;
	Node	*pr_node;
} Proc;

#include	"extern.h"
#include	"macro.h"
