/*			R E D B L A C K . H
 *
 *	The data structures, constants, and applications interface
 *	to LIBREDBLACK(3), the BRL-CAD red-black tree library.
 *
 *	Many of the routines in LIBREDBLACK(3) are based on the algorithms
 *	in chapter 13 of T. H. Cormen, C. E. Leiserson, and R. L. Rivest,
 *	_Introduction to algorithms_, MIT Press, Cambridge, MA, 1990.
 *
 *	Author:	Paul Tanenbaum
 *
 *  $Header$
 */

#ifndef REDBLACK_H
#define REDBLACK_H seen

/*
 *			    R B _ L I S T
 *
 *		    List of nodes or packages
 *
 *	LIBREDBLACK(3) uses this structure to maintain lists of
 *	all the nodes and all the packages in the tree.  Applications
 *	should not muck with these things.  They are maintained only
 *	to facilitate freeing rb_trees.
 */
struct rb_list
{
    struct bu_list	l;
    union
    {
	struct rb_node    *rbl_n;
	struct rb_package *rbl_p;
    }			rbl_u;
};
#define	rbl_magic	l.magic
#define	rbl_node	rbl_u.rbl_n
#define	rbl_package	rbl_u.rbl_p
#define	RB_LIST_NULL	((struct rb_list *) 0)


/*
 *			R B _ T R E E
 *
 *	This is the only data structure used in LIBREDBLACK
 *	to which application software need make any explicit
 *	reference.
 *
 *	The members of this structure are grouped into three
 *	classes:
 *	    Class I:	Reading is appropriate, when necessary,
 *			but applications should not modify.
 *	    Class II:	Reading and modifying are both appropriate,
 *			when necessary.
 *	    Class III:	All access should be through routines
 *			provided in LIBREDBLACK.  Touch these
 *			at your own risk!
 */
typedef struct
{
    /* CLASS I - Applications may read directly... */
    long	 	rbt_magic;	  /* Magic no. for integrity check */
    int			rbt_nm_nodes;	  /* Number of nodes */
    /* CLASS II - Applications may read/write directly... */
    void		(*rbt_print)();	  /* Data pretty-print function */
    int			rbt_debug;	  /* Debug bits */
    char		*rbt_description; /* Comment for diagnostics */
    /* CLASS III - Applications should not manipulate directly... */
    int		 	rbt_nm_orders;	  /* Number of simultaneous orders */
    int			(**rbt_order)();  /* Comparison functions */
    struct rb_node	**rbt_root;	  /* The actual trees */
    char		*rbt_unique;	  /* Uniqueness flags */
    struct rb_node	*rbt_current;	  /* Current node */
    struct rb_list	rbt_nodes;	  /* All nodes */
    struct rb_list	rbt_packages;	  /* All packages */
    struct rb_node	*rbt_empty_node;  /* Sentinel representing nil */
}	rb_tree;
#define	RB_TREE_NULL	((rb_tree *) 0)
#define	RB_TREE_MAGIC	0x72627472

/*
 *	Debug bit flags for member rbt_debug
 */
#define RB_DEBUG_INSERT	0x00000001	/* Insertion process */
#define RB_DEBUG_UNIQ	0x00000002	/* Uniqueness of inserts */
#define RB_DEBUG_ROTATE	0x00000004	/* Rotation process */
#define RB_DEBUG_OS	0x00000008	/* Order-statistic operations */
#define RB_DEBUG_DELETE	0x00000010	/* Deletion process */

/*
 *			R B _ P A C K A G E
 *
 *		    Wrapper for application data
 *
 *	This structure provides a level of indirection between
 *	the application software's data and the red-black nodes
 *	in which the data is stored.  It is necessary because of
 *	the algorithm for deletion, which generally shuffles data
 *	among nodes in the tree.  The package structure allows
 *	the application data to remember which node "contains" it
 *	for each order.
 */
struct rb_package
{
    long		rbp_magic;	/* Magic no. for integrity check */
    struct rb_node	**rbp_node;	/* Containing nodes */
    struct rb_list	*rbp_list_pos;	/* Place in the list of all pkgs. */
    void		*rbp_data;	/* Application data */
};
#define	RB_PKG_NULL	((struct rb_package *) 0)

/*
 *			    R B _ N O D E
 *
 *	For the most part, there is a one-to-one correspondence
 *	between nodes and chunks of application data.  When a
 *	node is created, all of its package pointers (one per
 *	order of the tree) point to the same chunk of data.
 *	However, subsequent deletions usually muddy this tidy
 *	state of affairs.
 */
struct rb_node
{
    long		rbn_magic;	/* Magic no. for integrity check */
    rb_tree		*rbn_tree;	/* Tree containing this node */
    struct rb_node	**rbn_parent;	/* Parents */
    struct rb_node	**rbn_left;	/* Left subtrees */
    struct rb_node	**rbn_right;	/* Right subtrees */
    char		*rbn_color;	/* Colors of this node */
    int			*rbn_size;	/* Sizes of subtrees rooted here */
    struct rb_package	**rbn_package;	/* Contents of this node */
    int			rbn_pkg_refs;	/* How many orders are being used? */
    struct rb_list	*rbn_list_pos;	/* Place in the list of all nodes */
};
#define	RB_NODE_NULL	((struct rb_node *) 0)

/*
 *	Applications interface to rb_extreme()
 */
#define	SENSE_MIN	0
#define	SENSE_MAX	1
#define	rb_min(t,o)	rb_extreme((t), (o), SENSE_MIN)
#define	rb_max(t,o)	rb_extreme((t), (o), SENSE_MAX)
#define rb_pred(t,o)	rb_neighbor((t), (o), SENSE_MIN)
#define rb_succ(t,o)	rb_neighbor((t), (o), SENSE_MAX)

/*
 *	Applications interface to rb_walk()
 */
#define	PREORDER	0
#define	INORDER		1
#define	POSTORDER	2

/*
 *	Applications interface to LIBREDBLACK
 */
BU_EXTERN(rb_tree *rb_create,	(char		*description,
				 int 		nm_orders,
				 int		(**order_funcs)()
				));
BU_EXTERN(rb_tree *rb_create1,	(char		*description,
				 int		(*order_func)()
				));
BU_EXTERN(void *rb_curr,	(rb_tree	*tree,
				 int		order
				));
#define		rb_curr1(t)	rb_curr((t), 0)
BU_EXTERN(void rb_delete,	(rb_tree	*tree,
				 int		order
				));
#define		rb_delete1(t)	rb_delete((t), 0)
BU_EXTERN(void rb_diagnose_tree,(rb_tree	*tree,
				 int		order,
				 int		trav_type
				));
BU_EXTERN(void *rb_extreme,	(rb_tree	*tree,
				 int		order,
				 int		sense
				));
BU_EXTERN(void rb_free,		(rb_tree	*tree,
				 void		(*free_data)()
				));
#define	RB_RETAIN_DATA	((void (*)()) 0)
#define		rb_free1(t,f)						\
		{							\
		    RB_CKMAG((t), RB_TREE_MAGIC, "red-black tree");	\
		    rt_free((char *) ((t) -> rbt_order),		\
				"red-black order function");		\
		    rb_free(t,f);					\
		}
BU_EXTERN(void *rb_select,	(rb_tree	*tree,
				 int		order,
				 int		k
				));
#define		rb_select1(t,k)	rb_select((t), 0, (k))
BU_EXTERN(int rb_insert,	(rb_tree	*tree,
				 void		*data
				));
BU_EXTERN(int rb_is_uniq,	(rb_tree	*tree,
				 int		order
				));
#define		rb_is_uniq1(t)	rb_is_uniq((t), 0)
BU_EXTERN(void *rb_neighbor,	(rb_tree	*tree,
				 int		order,
				 int		sense
				));
BU_EXTERN(int rb_rank,		(rb_tree	*tree,
				 int		order
				));
#define		rb_rank1(t)	rb_rank1((t), 0)
BU_EXTERN(void *rb_search,	(rb_tree	*tree,
				 int		order,
				 void		*data
				));
#define		rb_search1(t,d)	rb_search((t), 0, (d))
BU_EXTERN(void rb_set_uniqv,	(rb_tree	*tree,
				 bitv_t		vec
				));
BU_EXTERN(void rb_summarize_tree,(rb_tree	*tree
				 ));
BU_EXTERN(void rb_uniq_all_off,	(rb_tree	*tree
				));
BU_EXTERN(void rb_uniq_all_on,	(rb_tree	*tree
				));
BU_EXTERN(int rb_uniq_off,	(rb_tree	*tree,
				 int		order
				));
#define		rb_uniq_off1(t)	rb_uniq_off((t), 0)
BU_EXTERN(int rb_uniq_on,	(rb_tree	*tree,
				 int		order
				));
#define		rb_uniq_on1(t)	rb_uniq_on((t), 0)
BU_EXTERN(void rb_walk,		(rb_tree	*tree,
				 int		order,
				 void		(*visit)(),
				 int		trav_type
				));
#define		rb_walk1(t,v,d)	rb_walk((t), 0, (v), (d))

#endif /* REDBLACK_H */
