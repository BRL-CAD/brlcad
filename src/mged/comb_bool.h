/*                     C O M B _ B O O L . H
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file comb_bool.h
 *
 *  Author -
 *	Paul Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

struct bool_tree_node
{
    long			    btn_magic;
    int				    btn_opn;
    union
    {
	char			*leaf_name;
	struct bool_tree_node	*operands[2];
    }				    btn_operands;
};
#define	BOOL_TREE_NODE_NULL	((struct bool_tree_node *) 0)

#define	OPN_NULL		0
#define	OPN_UNION		1
#define	OPN_INTERSECTION	2
#define	OPN_DIFFERENCE		3

#define	BT_LEFT			0
#define	BT_RIGHT		1

#define bt_opn(n)		((n) -> btn_opn)
#define bt_leaf_name(n)		((n) -> btn_operands.leaf_name)
#define bt_opd(n,d)		((n) -> btn_operands.operands[(d)])

#define bt_is_leaf(n)		(bt_opn((n)) == OPN_NULL)

struct tree_tail
{
    long		tt_magic;
    int			tt_opn;
    struct tree_tail	*tt_next;
};
#define	TREE_TAIL_NULL	((struct tree_tail *) 0)

#define ZAPMAG(p)		(*((long *)(p)) = 0)
#define	BOOL_TREE_NODE_MAGIC	0x62746e64
#define	TREE_TAIL_MAGIC		0x74727461

extern struct bool_tree_node	*comb_bool_tree;

/*                      T A L L O C ( )
 *
 *                  Simple interface to malloc()
 *
 *      This macro has three parameters:  a pointer, a C data type,
 *      and a number of data objects.  Talloc() allocates enough
 *      memory to store n objects of type t.  It has the side-effect
 *      of causing l to point to the allocated storage.
 */
#define         talloc(l, t, n)                                         \
        if (((l) = (t *) malloc(n * sizeof(t))) == (t *) 0)             \
        {                                                               \
            fprintf(stderr, "%s:%d: Ran out of memory\n",		\
                    __FILE__, __LINE__);                                \
            exit(1);                                                    \
        }

/*
 *
 */

#ifdef USE_PROTOTYPES
extern struct bool_tree_node	*bt_create_internal (
				    int,
				    struct bool_tree_node *,
				    struct bool_tree_node *);
extern struct bool_tree_node	*bt_create_leaf (char*);
extern void			show_tree_infix (
				    struct bool_tree_node *,
				    int);
extern void			show_tree_lisp (struct bool_tree_node *);
extern int			cvt_to_gift_bool (struct bool_tree_node *);
extern void			show_gift_bool (struct bool_tree_node *, int);
#else
extern struct bool_tree_node *bt_create_internal(), *bt_create_leaf();
extern void show_tree_infix(), show_tree_lisp(), show_gift_bool();
extern int cvt_to_gift_bool();
#endif


#define show_tree(t,l)		if (l)				\
				{				\
				    show_tree_lisp((t));	\
				    printf("\n");		\
				}				\
				else				\
				    show_tree_infix((t), 0)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
