/*			R B _ I N S E R T . C
 *
 *		Routines to insert into a red-black tree
 *
 *	Author:	Paul Tanenbaum
 *
 */
#ifndef lint
static char RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "redblack.h"
#include "./rb_internals.h"

/*			_ R B _ I N S E R T ( )
 *
 *	    Insert a node into one linear order of a red-black tree
 *
 *	This function has three parameters: the tree and linear order into
 *	which to insert the new node and the new node itself.  If the node
 *	is equal (modulo the linear order) to a node already in the tree,
 *	_rb_insert() returns 1.  Otherwise, it returns 0.
 */
static int _rb_insert (tree, order, new_node)

rb_tree		*tree;
int		order;
struct rb_node	*new_node;

{
    struct rb_node	*node;
    struct rb_node	*parent;
    struct rb_node	*grand_parent;
    struct rb_node	*y;
    int			(*compare)();
    int			comparison;
    int			direction;
    int			result = 0;


    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    RB_CKMAG(new_node, RB_NODE_MAGIC, "red-black node");

    /*
     *	Initialize the new node
     */
    rb_parent(new_node, order) =
    rb_left_child(new_node, order) =
    rb_right_child(new_node, order) = rb_null(tree);
    rb_size(new_node, order) = 1;
    if (tree -> rbt_debug & RB_DEBUG_OS)
	bu_log("_rb_insert(%x): size(%x, %d)=%d\n",
	    new_node, new_node, order, rb_size(new_node, order));

    /*
     *	Perform vanilla-flavored binary-tree insertion
     */
    parent = rb_null(tree);
    node = rb_root(tree, order);
    compare = rb_order_func(tree, order);
    while (node != rb_null(tree))
    {
	parent = node;
	++rb_size(parent, order);
	if (tree -> rbt_debug & RB_DEBUG_OS)
	    bu_log("_rb_insert(%x): size(%x, %d)=%d\n",
		new_node, parent, order, rb_size(parent, order));
	comparison = (*compare)(rb_data(new_node, order), rb_data(node, order));
	if (comparison < 0)
	{
	    if (tree -> rbt_debug & RB_DEBUG_INSERT)
		bu_log("_rb_insert(%x): <_%d <%x>, going left\n",
		    new_node, order, node);
	    node = rb_left_child(node, order);
	}
	else
	{
	    if (tree -> rbt_debug & RB_DEBUG_INSERT)
		bu_log("_rb_insert(%x): >=_%d <%x>, going right\n",
		    new_node, order, node);
	    node = rb_right_child(node, order);
	    if (comparison == 0)
		result = 1;
	}
    }
    rb_parent(new_node, order) = parent;
    if (parent == rb_null(tree))
	rb_root(tree, order) = new_node;
    else if ((*compare)(rb_data(new_node, order), rb_data(parent, order)) < 0)
	rb_left_child(parent, order) = new_node;
    else
	rb_right_child(parent, order) = new_node;

    /*
     *	Reestablish the red-black properties, as necessary
     */
    rb_set_color(new_node, order, RB_RED);
    node = new_node;
    parent = rb_parent(node, order);
    grand_parent = rb_parent(parent, order);
    while ((node != rb_root(tree, order))
	&& (rb_get_color(parent, order) == RB_RED))
    {
	if (parent == rb_left_child(grand_parent, order))
	    direction = RB_LEFT;
	else
	    direction = RB_RIGHT;

	y = rb_other_child(grand_parent, order, direction);
	if (rb_get_color(y, order) == RB_RED)
	{
	    rb_set_color(parent, order, RB_BLACK);
	    rb_set_color(y, order, RB_BLACK);
	    rb_set_color(grand_parent, order, RB_RED);
	    node = grand_parent;
	    parent = rb_parent(node, order);
	    grand_parent = rb_parent(parent, order);
	}
	else
	{
	    if (node == rb_other_child(parent, order, direction))
	    {
		node = parent;
		rb_rotate(node, order, direction);
		parent = rb_parent(node, order);
		grand_parent = rb_parent(parent, order);
	    }
	    rb_set_color(parent, order, RB_BLACK);
	    rb_set_color(grand_parent, order, RB_RED);
	    rb_other_rotate(grand_parent, order, direction);
	}
    }
    rb_set_color(rb_root(tree, order), order, RB_BLACK);

    if (tree -> rbt_debug & RB_DEBUG_INSERT)
	bu_log("_rb_insert(%x): comparison = %d, returning %d\n",
	    new_node, comparison, result);

    return (result);
}

/*			R B _ I N S E R T ( )
 *
 *		Applications interface to _rb_insert()
 *
 *	This function has two parameters: the tree into which to insert
 *	the new node and the contents of the node.  If a uniqueness
 *	requirement would be violated, rb_insert() does nothing but return
 *	a number from the set {-1, -2, ..., -nm_orders} of which the
 *	absolute value is the first order for which a violation exists.
 *	Otherwise, it returns the number of orders for which the new node
 *	was equal to a node already in the tree.
 */
int rb_insert (tree, data)

rb_tree	*tree;
void	*data;

{
    int			nm_orders;
    int			order;
    int			result = 0;
    struct rb_node	*node;
    struct rb_package	*package;
    struct rb_list	*rblp;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");

    nm_orders = tree -> rbt_nm_orders;

    /*
     *	Enforce uniqueness
     *
     *	NOTE: The approach is that for each order that requires uniqueness,
     *	    we look for a match.  This is not the most efficient way to do
     *	    things, since _rb_insert() is just going to turn around and
     *	    search the tree all over again.
     */
    for (order = 0; order < nm_orders; ++order)
	if (rb_get_uniqueness(tree, order) &&
	    (rb_search(tree, order, data) != NULL))
	{
	    if (tree -> rbt_debug & RB_DEBUG_UNIQ)
		bu_log("rb_insert(<%x>, <%x>, TBD) will return %d\n",
		    tree, data, -(order + 1));
	    return (-(order + 1));
	}

    /*
     *	Make a new package
     *	and add it to the list of all packages.
     */
    package = (struct rb_package *)
		bu_malloc(sizeof(struct rb_package), "red-black package");
    package -> rbp_node = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black package nodes");
    rblp = (struct rb_list *)
		bu_malloc(sizeof(struct rb_list), "red-black list element");
    rblp -> rbl_magic = RB_LIST_MAGIC;
    rblp -> rbl_package = package;
    RT_LIST_PUSH(&(tree -> rbt_packages.l), rblp);
    package -> rbp_list_pos = rblp;

    /*
     *	Make a new node
     *	and add it to the list of all nodes.
     */
    node = (struct rb_node *)
		bu_malloc(sizeof(struct rb_node), "red-black node");
    node -> rbn_parent = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black parents");
    node -> rbn_left = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black left children");
    node -> rbn_right = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black right children");
    node -> rbn_color = (char *)
		bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black colors");
    node -> rbn_size = (int *)
		bu_malloc(nm_orders * sizeof(int),
			    "red-black subtree sizes");
    node -> rbn_package = (struct rb_package **)
		bu_malloc(nm_orders * sizeof(struct rb_package *),
			    "red-black packages");
    rblp = (struct rb_list *)
		bu_malloc(sizeof(struct rb_list), "red-black list element");
    rblp -> rbl_magic = RB_LIST_MAGIC;
    rblp -> rbl_node = node;
    RT_LIST_PUSH(&(tree -> rbt_nodes.l), rblp);
    node -> rbn_list_pos = rblp;

    /*
     *	Fill in the package
     */
    package -> rbp_magic = RB_PKG_MAGIC;
    package -> rbp_data = data;
    for (order = 0; order < nm_orders; ++order)
	(package -> rbp_node)[order] = node;

    /*
     *	Fill in the node
     */
    node -> rbn_magic = RB_NODE_MAGIC;
    node -> rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order)
	(node -> rbn_package)[order] = package;
    node -> rbn_pkg_refs = nm_orders;

    /*
     *	If the tree was empty, install this node as the root
     *	and give it a null parent and null children
     */
    if (rb_root(tree, 0) == rb_null(tree))
	for (order = 0; order < nm_orders; ++order)
	{
	    rb_root(tree, order) = node;
	    rb_parent(node, order) =
	    rb_left_child(node, order) =
	    rb_right_child(node, order) = rb_null(tree);
	    rb_set_color(node, order, RB_BLACK);
	    rb_size(node, order) = 1;
	    if (tree -> rbt_debug & RB_DEBUG_OS)
		bu_log("rb_insert(<%x>, <%x>, <%x>): size(%x, %d)=%d\n",
		    tree, data, node, node, order, rb_size(node, order));
	}
    /*	Otherwise, insert the node into the tree */
    else
    {
	for (order = 0; order < nm_orders; ++order)
	    result += _rb_insert(tree, order, node);
	if (tree -> rbt_debug & RB_DEBUG_UNIQ)
	    bu_log("rb_insert(<%x>, <%x>, <%x>) will return %d\n",
		tree, data, node, result);
    }

    ++(tree -> rbt_nm_nodes);
    rb_current(tree) = node;
    return (result);
}

/*		        _ R B _ S E T _ U N I Q ( )
 *
 *	    Raise or lower the uniqueness flag for one linear order
 *			    of a red-black tree
 *
 *	This function has three parameters: the tree, the order for which
 *	to modify the flag, and the new value for the flag.  _rb_set_uniq()
 *	sets the specified flag to the specified value and returns the
 *	previous value of the flag.
 */
static int _rb_set_uniq (tree, order, new_value)

rb_tree	*tree;
int	order;
int	new_value;

{
    int	prev_value;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    new_value = (new_value != 0);

    prev_value = rb_get_uniqueness(tree, order);
    rb_set_uniqueness(tree, order, new_value);
    return (prev_value);
}

/*		         R B _ U N I Q _ O N ( )
 *		        R B _ U N I Q _ O F F ( )
 *
 *		Applications interface to _rb_set_uniq()
 *
 *	These functions have two parameters: the tree and the order for
 *	which to require uniqueness/permit nonuniqueness.  Each sets the
 *	specified flag to the specified value and returns the previous
 *	value of the flag.
 */
int rb_uniq_on (tree, order)

rb_tree	*tree;
int	order;

{
    return (_rb_set_uniq(tree, order, 1));
}

int rb_uniq_off (tree, order)

rb_tree	*tree;
int	order;

{
    return (_rb_set_uniq(tree, order, 0));
}

/*		         R B _ I S _ U N I Q ( )
 *
 *	  Query the uniqueness flag for one order of a red-black tree
 *
 *	This function has two parameters: the tree and the order for
 *	which to query uniqueness.
 */
int rb_is_uniq (tree, order)

rb_tree	*tree;
int	order;

{
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    return(rb_get_uniqueness(tree, order));
}

/*		        R B _ S E T _ U N I Q V ( )
 *
 *	    Set the uniqueness flags for all the linear orders
 *			    of a red-black tree
 *
 *	This function has two parameters: the tree and a bitv_t
 *	encoding the flag values.  rb_set_uniqv() sets the flags
 *	according to the bits in flag_rep.  For example, if
 *	flag_rep = 1011_2, then the first, second, and fourth
 *	orders are specified unique, and the third is specified
 *	not-necessarily unique.
 */
void rb_set_uniqv (tree, flag_rep)

rb_tree	*tree;
bitv_t	flag_rep;

{
    int	nm_orders;
    int	order;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");

    nm_orders = tree -> rbt_nm_orders;
    for (order = 0; order < nm_orders; ++order)
	rb_set_uniqueness(tree, order, 0);
    
    for (order = 0; (flag_rep != 0) && (order < nm_orders); flag_rep >>= 1,
							    ++order)
	if (flag_rep & 0x1)
	    rb_set_uniqueness(tree, order, 1);

    if (flag_rep != 0)
	bu_log("rb_set_uniqv(): Ignoring bits beyond rightmost %d\n",
	    nm_orders);
}

/*		    _ R B _ S E T _ U N I Q _ A L L ( )
 *
 *	    Raise or lower the uniqueness flags for all the linear orders
 *			    of a red-black tree
 *
 *	This function has two parameters: the tree, and the new value
 *	for all the flags.
 */
static void _rb_set_uniq_all (tree, new_value)

rb_tree	*tree;
int	new_value;

{
    int	nm_orders;
    int	order;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    new_value = (new_value != 0);

    nm_orders = tree -> rbt_nm_orders;
    for (order = 0; order < nm_orders; ++order)
	rb_set_uniqueness(tree, order, new_value);
}

/*		     R B _ U N I Q _ A L L _ O N ( )
 *		    R B _ U N I Q _ A L L _ O F F ( )
 *
 *	      Applications interface to _rb_set_uniq_all()
 *
 *	These functions have one parameter: the tree for which to
 *	require uniqueness/permit nonuniqueness.
 */
void rb_uniq_all_on (tree)

rb_tree	*tree;

{
    _rb_set_uniq_all(tree, 1);
}

void rb_uniq_all_off (tree)

rb_tree	*tree;

{
    _rb_set_uniq_all(tree, 0);
}
