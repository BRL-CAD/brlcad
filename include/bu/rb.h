/*                            R B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @file rb.h
 *
 */
#ifndef BU_RB_H
#define BU_RB_H

#include "common.h"
#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/bitv.h"

/*----------------------------------------------------------------------*/
/** @addtogroup rb */
/** @{ */
/*
 * The data structures and constants for red-black trees.
 *
 * Many of these routines are based on the algorithms in chapter 13 of
 * Thomas H. Cormen, Charles E. Leiserson, and Ronald L. Rivest,
 * "Introduction to Algorithms", MIT Press, Cambridge, MA, 1990.
 *
 * FIXME:  check implementation given the following note:
 *
 * Note that the third edition was published in 2009 and the book
 * has had significant updates since the first edition.  Quoting the
 * authors in the preface:  "The way we delete a node from binary search
 * trees (which includes red-black trees) now guarantees that the node
 * requested for deletion is the node that is actually deleted.  In the
 * first two editions, in certain cases, some other node would be
 * deleted, with its contents moving into the node passed to the
 * deletion procedure.  With our new way to delete nodes, if other
 * components of a program maintain pointers to nodes in the tree, they
 * will not mistakenly end up with stale pointers to nodes that have
 * been deleted."
 *
 * This implementation of balanced binary red-black tree operations
 * provides all the basic dynamic set operations (e.g., insertion,
 * deletion, search, minimum, maximum, predecessor, and successor) and
 * order-statistic operations (i.e., select and rank) with optimal
 * O(log(n)) performance while sorting on multiple keys. Such an
 * implementation is referred to as an "augmented red-black tree" and
 * is discussed in Chapter 14 of the 2009 edition of "Introduction to
 * Algorithms."
 */

/**
 * List of nodes or packages.
 *
 * The red-black tree package uses this structure to maintain lists of
 * all the nodes and all the packages in the tree.  Applications
 * should not muck with these things.  They are maintained only to
 * facilitate freeing bu_rb_trees.
 *
 * This is a PRIVATE structure.
 */
struct bu_rb_list
{
    struct bu_list l;
    union
    {
	struct bu_rb_node *rbl_n;
	struct bu_rb_package *rbl_p;
    } rbl_u;
};
#define rbl_magic l.magic
#define rbl_node rbl_u.rbl_n
#define rbl_package rbl_u.rbl_p
#define BU_RB_LIST_NULL ((struct bu_rb_list *) 0)


/**
 * This is the only data structure used in the red-black tree package
 * to which application software need make any explicit reference.
 *
 * The members of this structure are grouped into three classes:
 *
 * Class I:	Reading is appropriate, when necessary,
 *		but applications should not modify.
 * Class II:	Reading and modifying are both appropriate,
 *		when necessary.
 * Class III:	All access should be through routines
 *		provided in the package.  Touch these
 *		at your own risk!
 */
struct bu_rb_tree {
    /***** CLASS I - Applications may read directly. ****************/
    uint32_t rbt_magic;                /**< Magic no. for integrity check */
    int rbt_nm_nodes;                  /**< Number of nodes */

    /**** CLASS II - Applications may read/write directly. **********/
    void (*rbt_print)(void *);         /**< Data pretty-print function */
    int rbt_debug;                     /**< Debug bits */
    const char *rbt_description;       /**< Comment for diagnostics */

    /*** CLASS III - Applications should NOT manipulate directly. ***/
    int rbt_nm_orders;                 /**< Number of simultaneous orders */
    int (**rbt_compar)(const void *, const void *); /**< Comparison functions */
    struct bu_rb_node **rbt_root;      /**< The actual trees */
    char *rbt_unique;                  /**< Uniqueness flags */
    struct bu_rb_node *rbt_current;    /**< Current node */
    struct bu_rb_list rbt_nodes;       /**< All nodes */
    struct bu_rb_list rbt_packages;    /**< All packages */
    struct bu_rb_node *rbt_empty_node; /**< Sentinel representing nil */
};
typedef struct bu_rb_tree bu_rb_tree_t;
#define BU_RB_TREE_NULL ((struct bu_rb_tree *) 0)

/**
 * asserts the integrity of a bu_rb_tree struct.
 */
#define BU_CK_RB_TREE(_rb) BU_CKMAG(_rb, BU_RB_TREE_MAGIC, "bu_rb_tree")

/**
 * initializes a bu_rb_tree struct without allocating any memory.
 */
#define BU_RB_TREE_INIT(_rb) { \
	(_rb)->rbt_magic = BU_RB_TREE_MAGIC; \
	(_rb)->rbt_nm_nodes = 0; \
	(_rb)->rbt_print = NULL; \
	(_rb)->rbt_debug = 0; \
	(_rb)->rbt_description = NULL; \
	(_rb)->rbt_nm_orders = 0; \
	(_rb)->rbt_compar = NULL; \
	(_rb)->rbt_root = (_rb)->rbt_unique = (_rb)->rbt_current = NULL; \
	BU_LIST_INIT(&(_rb)->rbt_nodes.l); \
	(_rb)->rbt_nodes.rbl_u.rbl_n = (_rb)->rbt_nodes.rbl_u.rbl_p = NULL; \
	BU_LIST_INIT(&(_rb)->rbt_packages.l); \
	(_rb)->rbt_packages.rbl_u.rbl_n = (_rb)->rbt_packages.rbl_u.rbl_p = NULL; \
	(_rb)->rbt_empty_node = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_rb_tree struct.  does not allocate memory.
 */
#define BU_RB_TREE_INIT_ZERO { BU_RB_TREE_MAGIC, 0, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL, \
	{ BU_LIST_INIT_ZER0, {NULL, NULL} }, { BU_LIST_INIT_ZER0, {NULL, NULL} }, NULL, NULL, NULL }

/**
 * returns truthfully whether a bu_rb_tree has been initialized.
 */
#define BU_RB_TREE_IS_INITIALIZED(_rb) (((struct bu_rb_tree *)(_rb) != BU_RB_TREE_NULL) && LIKELY((_rb)->rbt_magic == BU_RB_TREE_MAGIC))


/*
 * Debug bit flags for member rbt_debug
 */
#define BU_RB_DEBUG_INSERT 0x00000001	/**< Insertion process */
#define BU_RB_DEBUG_UNIQ 0x00000002	/**< Uniqueness of inserts */
#define BU_RB_DEBUG_ROTATE 0x00000004	/**< Rotation process */
#define BU_RB_DEBUG_OS 0x00000008	/**< Order-statistic operations */
#define BU_RB_DEBUG_DELETE 0x00000010	/**< Deletion process */

/**
 * Wrapper for application data.
 *
 * This structure provides a level of indirection between the
 * application software's data and the red-black nodes in which the
 * data is stored.  It is necessary because of the algorithm for
 * deletion, which generally shuffles data among nodes in the tree.
 * The package structure allows the application data to remember which
 * node "contains" it for each order.
 */
struct bu_rb_package
{
    uint32_t rbp_magic;	/**< Magic no. for integrity check */
    struct bu_rb_node **rbp_node;	/**< Containing nodes */
    struct bu_rb_list *rbp_list_pos;	/**< Place in the list of all pkgs.  */
    void *rbp_data;	/**< Application data */
};
#define BU_RB_PKG_NULL ((struct bu_rb_package *) 0)

/**
 * For the most part, there is a one-to-one correspondence between
 * nodes and chunks of application data.  When a node is created, all
 * of its package pointers (one per order of the tree) point to the
 * same chunk of data.  However, subsequent deletions usually muddy
 * this tidy state of affairs.
 */
struct bu_rb_node
{
    uint32_t rbn_magic;		        /**< Magic no. for integrity check */
    struct bu_rb_tree *rbn_tree;	/**< Tree containing this node */
    struct bu_rb_node **rbn_parent;	/**< Parents */
    struct bu_rb_node **rbn_left;	/**< Left subtrees */
    struct bu_rb_node **rbn_right;	/**< Right subtrees */
    char *rbn_color;			/**< Colors of this node */
    int *rbn_size;			/**< Sizes of subtrees rooted here */
    struct bu_rb_package **rbn_package;	/**< Contents of this node */
    int rbn_pkg_refs;			/**< How many orders are being used?  */
    struct bu_rb_list *rbn_list_pos;	/**< Place in the list of all nodes */
};
#define BU_RB_NODE_NULL ((struct bu_rb_node *) 0)

/*
 * Applications interface to bu_rb_extreme()
 */
#define SENSE_MIN 0
#define SENSE_MAX 1
#define bu_rb_min(t, o) bu_rb_extreme((t), (o), SENSE_MIN)
#define bu_rb_max(t, o) bu_rb_extreme((t), (o), SENSE_MAX)
#define bu_rb_pred(t, o) bu_rb_neighbor((t), (o), SENSE_MIN)
#define bu_rb_succ(t, o) bu_rb_neighbor((t), (o), SENSE_MAX)

/*
 * Applications interface to bu_rb_walk()
 */
enum BU_RB_WALK_ORDER {
    BU_RB_WALK_PREORDER,
    BU_RB_WALK_INORDER,
    BU_RB_WALK_POSTORDER
};

/** @file libbu/rb_create.c
 *
 * Routines to create a red-black tree
 *
 */

/**
 * Create a red-black tree
 *
 * This function has three parameters: a comment describing the tree
 * to create, the number of linear orders to maintain simultaneously,
 * and the comparison functions (one per order).  bu_rb_create()
 * returns a pointer to the red-black tree header record.
 */
BU_EXPORT extern struct bu_rb_tree *bu_rb_create(const char *description, int nm_orders, int (**compare_funcs)(const void *, const void *));
/* A macro for correct casting of a rb compare function for use a function argument: */
#define BU_RB_COMPARE_FUNC_CAST_AS_FUNC_ARG(_func) ((int (*)(void))_func)

/**
 * Create a single-order red-black tree
 *
 * This function has two parameters: a comment describing the tree to
 * create and a comparison function.  bu_rb_create1() builds an array
 * of one function pointer and passes it to bu_rb_create().
 * bu_rb_create1() returns a pointer to the red-black tree header
 * record.
 *
 * N.B. - Since this function allocates storage for the array of
 * function pointers, in order to avoid memory leaks on freeing the
 * tree, applications should call bu_rb_free1(), NOT bu_rb_free().
 */
BU_EXPORT extern struct bu_rb_tree *bu_rb_create1(const char *description, int (*compare_func)(void));

/** @file libbu/rb_delete.c
 *
 * Routines to delete a node from a red-black tree
 *
 */

/**
 * Applications interface to _rb_delete()
 *
 * This function has two parameters: the tree and order from which to
 * do the deleting.  bu_rb_delete() removes the data block stored in
 * the current node (in the position of the specified order) from
 * every order in the tree.
 */
BU_EXPORT extern void bu_rb_delete(struct bu_rb_tree *tree,
				   int order);
#define bu_rb_delete1(t) bu_rb_delete((t), 0)

/** @file libbu/rb_diag.c
 *
 * Diagnostic routines for red-black tree maintenance
 *
 */

/**
 * Produce a diagnostic printout of a red-black tree
 *
 * This function has three parameters: the root and order of the tree
 * to print out and the type of traversal (preorder, inorder, or
 * postorder).
 */
BU_EXPORT extern void bu_rb_diagnose_tree(struct bu_rb_tree *tree,
					  int order,
					  int trav_type);

/**
 * Describe a red-black tree
 *
 * This function has one parameter: a pointer to a red-black tree.
 * bu_rb_summarize_tree() prints out the header information for the
 * tree.  It is intended for diagnostic purposes.
 */
BU_EXPORT extern void bu_rb_summarize_tree(struct bu_rb_tree *tree);

/** @file libbu/rb_extreme.c
 *
 * Routines to extract mins, maxes, adjacent, and current nodes
 * from a red-black tree
 *
 */


/**
 * Applications interface to rb_extreme()
 *
 * This function has three parameters: the tree in which to find an
 * extreme node, the order on which to do the search, and the sense
 * (min or max).  On success, bu_rb_extreme() returns a pointer to the
 * data in the extreme node.  Otherwise it returns NULL.
 */
BU_EXPORT extern void *bu_rb_extreme(struct bu_rb_tree *tree,
				     int order,
				     int sense);

/**
 * Return a node adjacent to the current red-black node
 *
 * This function has three parameters: the tree and order on which to
 * do the search and the sense (min or max, which is to say
 * predecessor or successor) of the search.  bu_rb_neighbor() returns
 * a pointer to the data in the node adjacent to the current node in
 * the specified direction, if that node exists.  Otherwise, it
 * returns NULL.
 */
BU_EXPORT extern void *bu_rb_neighbor(struct bu_rb_tree *tree,
				      int order,
				      int sense);

/**
 * Return the current red-black node
 *
 * This function has two parameters: the tree and order in which to
 * find the current node.  bu_rb_curr() returns a pointer to the data
 * in the current node, if it exists.  Otherwise, it returns NULL.
 */
BU_EXPORT extern void *bu_rb_curr(struct bu_rb_tree *tree,
				  int order);
#define bu_rb_curr1(t) bu_rb_curr((t), 0)

/** @file libbu/rb_free.c
 *
 * Routines to free a red-black tree
 *
 */

/**
 * Free a red-black tree
 *
 * This function has two parameters: the tree to free and a function
 * to handle the application data.  bu_rb_free() traverses tree's
 * lists of nodes and packages, freeing each one in turn, and then
 * frees tree itself.  If free_data is non-NULL, then bu_rb_free()
 * calls it just* before freeing each package, passing it the
 * package's rbp_data member.  Otherwise, the application data is left
 * untouched.
 */
BU_EXPORT extern void bu_rb_free(struct bu_rb_tree *tree, void (*free_data)(void *data));
#define BU_RB_RETAIN_DATA ((void (*)(void *data)) 0)
#define bu_rb_free1(t, f)					\
    {							\
	BU_CKMAG((t), BU_RB_TREE_MAGIC, "red-black tree");	\
	bu_free((char *) ((t) -> rbt_compar),		\
		"red-black compare function");		\
	bu_rb_free(t, f);					\
    }

/** @file libbu/rb_insert.c
 *
 * Routines to insert into a red-black tree
 *
 */

/**
 * Applications interface to bu_rb_insert()
 *
 * This function has two parameters: the tree into which to insert the
 * new node and the contents of the node.  If a uniqueness requirement
 * would be violated, bu_rb_insert() does nothing but return a number
 * from the set {-1, -2, ..., -nm_orders} of which the absolute value
 * is the first order for which a violation exists.  Otherwise, it
 * returns the number of orders for which the new node was equal to a
 * node already in the tree.
 */
BU_EXPORT extern int bu_rb_insert(struct bu_rb_tree *tree,
				  void *data);

/**
 * Query the uniqueness flag for one order of a red-black tree
 *
 * This function has two parameters: the tree and the order for which
 * to query uniqueness.
 */
BU_EXPORT extern int bu_rb_is_uniq(struct bu_rb_tree *tree,
				   int order);
#define bu_rb_is_uniq1(t) bu_rb_is_uniq((t), 0)

/**
 * Set the uniqueness flags for all the linear orders of a red-black
 * tree
 *
 * This function has two parameters: the tree and a bitv_t encoding
 * the flag values.  bu_rb_set_uniqv() sets the flags according to the
 * bits in flag_rep.  For example, if flag_rep = 1011_2, then the
 * first, second, and fourth orders are specified unique, and the
 * third is specified not-necessarily unique.
 */
BU_EXPORT extern void bu_rb_set_uniqv(struct bu_rb_tree *tree,
				      bitv_t vec);

/**
 * These functions have one parameter: the tree for which to
 * require uniqueness/permit nonuniqueness.
 */
BU_EXPORT extern void bu_rb_uniq_all_off(struct bu_rb_tree *tree);

/**
 * These functions have one parameter: the tree for which to
 * require uniqueness/permit nonuniqueness.
 */
BU_EXPORT extern void bu_rb_uniq_all_on(struct bu_rb_tree *tree);

/**
 * Has two parameters: the tree and the order for which to require
 * uniqueness/permit nonuniqueness.  Each sets the specified flag to
 * the specified value and returns the previous value of the flag.
 */
BU_EXPORT extern int bu_rb_uniq_on(struct bu_rb_tree *tree,
				   int order);
#define bu_rb_uniq_on1(t) bu_rb_uniq_on((t), 0)

/**
 * Has two parameters: the tree and the order for which to require
 * uniqueness/permit nonuniqueness.  Each sets the specified flag to
 * the specified value and returns the previous value of the flag.
 */
BU_EXPORT extern int bu_rb_uniq_off(struct bu_rb_tree *tree,
				    int order);
#define bu_rb_uniq_off1(t) bu_rb_uniq_off((t), 0)

/** @file libbu/rb_order_stats.c
 *
 * Routines to support order-statistic operations for a red-black tree
 *
 */

/**
 * Determines the rank of a node in one order of a red-black tree.
 *
 * This function has two parameters: the tree in which to search and
 * the order on which to do the searching.  If the current node is
 * null, bu_rb_rank() returns 0.  Otherwise, it returns the rank of
 * the current node in the specified order.  bu_rb_rank() is an
 * implementation of the routine OS-RANK on p. 283 of Cormen et al.
 */
BU_EXPORT extern int bu_rb_rank(struct bu_rb_tree *tree,
				int order);
#define bu_rb_rank1(t) bu_rb_rank1((t), 0)

/**
 * This function has three parameters: the tree in which to search,
 * the order on which to do the searching, and the rank of interest.
 * On success, bu_rb_select() returns a pointer to the data block in
 * the discovered node.  Otherwise, it returns NULL.
 */
BU_EXPORT extern void *bu_rb_select(struct bu_rb_tree *tree,
				    int order,
				    int k);
#define bu_rb_select1(t, k) bu_rb_select((t), 0, (k))

/** @file libbu/rb_search.c
 *
 * Routines to search for a node in a red-black tree
 *
 */

/**
 * This function has three parameters: the tree in which to search,
 * the order on which to do the searching, and a data block containing
 * the desired value of the key.  On success, bu_rb_search() returns a
 * pointer to the data block in the discovered node.  Otherwise, it
 * returns NULL.
 */
BU_EXPORT extern void *bu_rb_search(struct bu_rb_tree *tree,
				    int order,
				    void *data);
#define bu_rb_search1(t, d) bu_rb_search((t), 0, (d))

/** @file libbu/rb_walk.c
 *
 * Routines for traversal of red-black trees
 *
 * The function bu_rb_walk() is defined in terms of the function
 * rb_walk(), which, in turn, calls any of the six functions
 *
 * @arg		- static void prewalknodes()
 * @arg		- static void inwalknodes()
 * @arg		- static void postwalknodes()
 * @arg		- static void prewalkdata()
 * @arg		- static void inwalkdata()
 * @arg		- static void postwalkdata()
 *
 * depending on the type of traversal desired and the objects
 * to be visited (nodes themselves, or merely the data stored
 * in them).  Each of these last six functions has four parameters:
 * the root of the tree to traverse, the order on which to do the
 * walking, the function to apply at each visit, and the current
 * depth in the tree.
 */

/**
 * This function has four parameters: the tree to traverse, the order
 * on which to do the walking, the function to apply to each node, and
 * the type of traversal (preorder, inorder, or postorder).
 *
 * Note the function to apply has the following signature ONLY when it
 * is used as an argument:
 *
 *   void (*visit)(void)
 *
 * When used as a function the pointer should be cast back to one of its real
 * signatures depending on what it is operating on (node or data):
 *
 *   node:
 *
 *     void (*visit)((struct bu_rb_node *, int)
 *
 *   data:
 *
 *     void (*visit)((void *, int)
 *
 * Use the macros below to ensure accurate casting.  See
 * libbu/rb_diag.c and libbu/rb_walk.c for examples of their use.
 *
 */
BU_EXPORT extern void bu_rb_walk(struct bu_rb_tree *tree, int order, void (*visit)(void), int trav_type);
#define bu_rb_walk1(t, v, d) bu_rb_walk((t), 0, (v), (d))

#define BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(_func) ((void (*)(void))_func)
#define BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(_func) ((void (*)(struct bu_rb_node *, int))_func)
#define BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(_func) ((void (*)(void *, int))_func)
#define BU_RB_WALK_FUNC_CAST_AS_FUNC_FUNC(_func) ((void (*)(struct bu_rb_node *, int, void (*)(void), int))_func)
#define BU_RB_WALK_FUNC_NODE_DECL(_func) void (*_func)(struct bu_rb_node *, int)
#define BU_RB_WALK_FUNC_DATA_DECL(_func) void (*_func)(void *, int)
#define BU_RB_WALK_FUNC_FUNC_DECL(_func) void (*_func)(struct bu_rb_node *, int, void (*)(void), int)

/** @} */

#endif  /* BU_RB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
