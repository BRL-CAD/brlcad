/*			R E D B L A C K . H
 *
 *	Written by:	Paul Tanenbaum
 *
 *  $Header$
 */

#ifndef REDBLACK_H
#define REDBLACK_H seen

/*
 *	Data structures
 */
typedef struct
{
    long	 	rbt_magic;	  /* Magic no. for integrity check */
    char		*rbt_description; /* Comment for diagnostics */
    int		 	rbt_nm_orders;	  /* Number of simultaneous orders */
    int			(**rbt_order)();  /* Comparison functions */
    struct rb_node	*rbt_root;	  /* The actual tree */
}	rb_tree;
#define	RB_TREE_NULL	((rb_tree *) 0)

struct rb_node
{
    long		rbn_magic;	/* Magic no. for integrity check */
    rb_tree		*rbn_tree;	/* Tree containing this node */
    struct rb_node	**rbn_parent;	/* Parents */
    struct rb_node	**rbn_left;	/* Left subtrees */
    struct rb_node	**rbn_right;	/* Right subtrees */
    char		*rbn_color;	/* Colors of this node */
    void		*rbn_data;	/* Contents of this node */
};
#define	RB_NODE_NULL	((struct rb_node *) 0)

/*
 *	Applications interface to LIBREDBLACK
 */
rb_tree *rb_create	(
			    char	*description,
			    int 	nm_orders,
			    int		(**order)()
			);
int rb_insert		(
			    rb_tree	*tree,
			    void	*data
			);
void *rb_min		(
			    rb_tree	*tree,
			    int		order_nm
			);

#endif /* REDBLACK_H */
