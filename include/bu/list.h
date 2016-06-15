/*                        L I S T . H
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

#ifndef BU_LIST_H
#define BU_LIST_H

#include "common.h"

#include "bu/defines.h"
#include "bu/magic.h"

__BEGIN_DECLS

/*----------------------------------------------------------------------*/
/** @addtogroup list */
/** @{*/
/** @file libbu/list.c
 *
 * @brief Support routines for doubly-linked lists.
 *
 * These macros assume that all user-provided structures will have a
 * "struct bu_list" as their first element (often named "l" [ell]).
 * Thus, a pointer to the bu_list struct is a "pun" for the
 * user-provided structure as well, and the pointers can be converted
 * back and forth safely with type casts.
 *
 * Furthermore, the head of the linked list could be a full instance
 * of the user-provided structure (although the storage-conscious
 * programmer could make the head just an bu_list structure, with
 * careful type casting).  This results in a doubly-linked circular
 * list, with the head having the same shape as all the list members.
 * The application is free to make use of this symmetry and store data
 * values in the head, or the extra storage in the head can be
 * ignored.
 *
 * Where a macro expects an argument "p", it should be a pointer to a
 * user-provided structure.
 *
 * Where a macro expects an argument "hp", it should be a pointer to a
 * "struct bu_list" located in the list head, e.g., &(head.l).
 *
 * Where a macro expects an argument "old", "new", or "cur", it should
 * be a pointer to the "struct bu_list" located either in a
 * user-provided structure, e.g. &((p)->l), or for the case of "old"
 * it may also be in the list head.
 *
 @code
 --- BEGIN EXAMPLE ---

 // make bu_list the first element in your structure
 struct my_structure {
   struct bu_list l;
   int my_data;
 };

 // your actual list
 struct my_structure *my_list = NULL;

 // allocate and initialize your list head
 BU_GET(my_list, struct my_structure);
 BU_LIST_INIT(&(my_list->l));
 my_list->my_data = -1;

 // add a new element to your list
 struct my_structure *new_entry;
 BU_GET(new_entry, struct my_structure);
 new_entry->my_data = rand();
 BU_LIST_PUSH(&(my_list->l), &(new_entry->l));

 // iterate over your list, remove all items
 struct my_structure *entry;
 while (BU_LIST_WHILE(entry, my_structure, &(my_list->l))) {
   bu_log("Entry value is %d\n", entry->my_data);
   BU_LIST_DEQUEUE(&(entry->l));
   BU_PUT(entry, struct my_structure);
 }
 BU_PUT(my_list, struct my_structure);

 --- END EXAMPLE ---
 @endcode
 *
 * Dequeueing the head of a list is a valid and well defined operation
 * which should be performed with caution.  Unless a pointer to some
 * other element of the list is retained by the application, the rest
 * of the linked list can no longer be referred to.
 *
 * The "magic" field of the list header _must_ be set to the constant
 * BU_LIST_HEAD_MAGIC, but the "magic" field of all list members
 * should be established by user code, to identify the type of
 * structure that the bu_list structure is embedded in.  It is
 * permissible for one list to contain an arbitrarily mixed set of
 * user "magic" numbers, as long as the head is properly marked.
 *
 * There is a dual set of terminology used in some of the macros:
 *
 * FIRST/ LAST - from the point of view of the list head
 * NEXT / PREV - from the point of view of a list member
 * forw / back - the actual pointer names
 */

struct bu_list {
    uint32_t magic;		/**< @brief Magic # for mem id/check */
    struct bu_list *forw;		/**< @brief "forward", "next" */
    struct bu_list *back;		/**< @brief "back", "last" */
};
typedef struct bu_list bu_list_t;
#define BU_LIST_NULL ((struct bu_list *)0)

/**
 * macro for setting the magic number of a non-head list node
 */
#define BU_LIST_MAGIC_SET(_l, _magic) {(_l)->magic = (_magic);}

/**
 * macro for testing whether a list node's magic number is equal to a
 * specific magic number
 */
#define BU_LIST_MAGIC_EQUAL(_l, _magic) ((_l)->magic == (_magic))

/**
 * there is no reliable way to assert the integrity of an arbitrary
 * bu_list struct since the magic can be anything, therefore there is
 * no BU_CK_LIST().  we can, however, check for a valid head node.
 */
#define BU_CK_LIST_HEAD(_p) BU_CKMAG((_p), BU_LIST_HEAD_MAGIC, "bu_list")

/**
 * initializes a bu_list struct as a circular list without allocating
 * any memory.  call BU_LIST_MAGIC_SET() to change the list type.
 */
#define BU_LIST_INIT(_hp) { \
	(_hp)->forw = (_hp)->back = (_hp); \
	(_hp)->magic = BU_LIST_HEAD_MAGIC;	/* used by circ. macros */ }

/**
 * initializes a bu_list struct node with a particular non-head node
 * magic number without allocating any memory.
 */
#define BU_LIST_INIT_MAGIC(_hp, _magic) { \
	BU_LIST_INIT((_hp)); \
	BU_LIST_MAGIC_SET((_hp), (_magic)); \
    }

/**
 * macro suitable for declaration statement zero-initialization of a
 * bu_list struct, but not suitably for validation with
 * BU_CK_LIST_HEAD() as the list pointers are NULL.  does not allocate
 * memory.
 */
#define BU_LIST_INIT_ZERO { 0, BU_LIST_NULL, BU_LIST_NULL }

/**
 * returns truthfully whether a bu_list has been initialized via
 * BU_LIST_INIT().  lists initialized with BU_LIST_INIT_ZERO or
 * zero-allocated will not return true as their forward/backward
 * pointers reference nothing.
 */
#define BU_LIST_IS_INITIALIZED(_hp) (((struct bu_list *)(_hp) != BU_LIST_NULL) && LIKELY((_hp)->forw != BU_LIST_NULL))


/**
 * Insert "new" item in front of "old" item.  Often, "old" is the
 * head.  To put the new item at the tail of the list, insert before
 * the head, e.g.  * BU_LIST_INSERT(&(head.l), &((p)->l));
 */
#define BU_LIST_INSERT(old, new) { \
	BU_ASSERT((void *)(old) != (void *)NULL); \
	BU_ASSERT((void *)(new) != (void *)NULL); \
	(new)->back = (old)->back; \
	(old)->back = (new); \
	(new)->forw = (old); \
	BU_ASSERT((void *)((new)->back) != (void *)NULL); \
	(new)->back->forw = (new);  }

/**
 * Append "new" item after "old" item.  Often, "old" is the head.  To
 * put the new item at the head of the list, append after the head,
 * e.g.  * BU_LIST_APPEND(&(head.l), &((p)->l));
 */
#define BU_LIST_APPEND(old, new) { \
	BU_ASSERT((void *)(old) != (void *)NULL); \
	BU_ASSERT((void *)(new) != (void *)NULL); \
	(new)->forw = (old)->forw; \
	(new)->back = (old); \
	(old)->forw = (new); \
	BU_ASSERT((void *)((old)->forw) != (void *)NULL); \
	(new)->forw->back = (new);  }

/**
 * Dequeue "cur" item from anywhere in doubly-linked list
 */
#define BU_LIST_DEQUEUE(cur) { \
	BU_ASSERT((void *)(cur) != (void *)NULL); \
	if (LIKELY((cur)->forw != NULL)) (cur)->forw->back = (cur)->back; \
	if (LIKELY((cur)->back != NULL)) (cur)->back->forw = (cur)->forw; \
	(cur)->forw = (cur)->back = BU_LIST_NULL;  /* sanity */ }

/**
 * Dequeue "cur" but do not fix its links
 */
#define BU_LIST_DQ(cur) {\
	BU_ASSERT((void *)(cur) != (void *)NULL); \
	if (LIKELY((cur)->forw != NULL)) (cur)->forw->back = (cur)->back; \
	if (LIKELY((cur)->back != NULL)) (cur)->back->forw = (cur)->forw; }

#define BU_LIST_DQ_T(cur, type) (\
	(cur)->forw->back = (cur)->back, \
	(cur)->back->forw = (cur)->forw, \
	(type *)(cur))

/**
 * This version of BU_LIST_DEQUEUE uses the comma operator in order to
 * return a typecast version of the dequeued pointer
 */
#define BU_LIST_DEQUEUE_T(cur, type) (\
	(cur)->forw->back = (cur)->back, \
	(cur)->back->forw = (cur)->forw, \
	(cur)->forw = (cur)->back = BU_LIST_NULL, \
	(type *)(cur))


/**
 * The Stack Discipline
 *
 * BU_LIST_PUSH places p at the tail of hp.  BU_LIST_POP sets p to
 * last element in hp's list (else NULL) and, if p is non-null,
 * dequeues it.
 */
#define BU_LIST_PUSH(hp, p)					\
    BU_LIST_APPEND(hp, (struct bu_list *)(p))

#define BU_LIST_POP(structure, hp, p)				\
    {							\
	if (BU_LIST_NON_EMPTY(hp)) {			\
	    (p) = ((struct structure *)((hp)->forw));	\
	    BU_LIST_DEQUEUE((struct bu_list *)(p));	\
	} else {					\
	    (p) = (struct structure *) 0;		\
	}						\
    }

#define BU_LIST_POP_T(hp, type)					\
    (type *)bu_list_pop(hp)

/**
 * "Bulk transfer" all elements from the list headed by src_hd onto
 * the list headed by dest_hd, without examining every element in the
 * list.  src_hd is left with a valid but empty list.
 *
 * BU_LIST_INSERT_LIST places src_hd elements at head of dest_hd list,
 * BU_LIST_APPEND_LIST places src_hd elements at end of dest_hd list.
 */
#define BU_LIST_INSERT_LIST(dest_hp, src_hp) \
    if (LIKELY(BU_LIST_NON_EMPTY(src_hp))) { \
	struct bu_list *_first = (src_hp)->forw; \
	struct bu_list *_last = (src_hp)->back; \
	(dest_hp)->forw->back = _last; \
	_last->forw = (dest_hp)->forw; \
	(dest_hp)->forw = _first; \
	_first->back = (dest_hp); \
	(src_hp)->forw = (src_hp)->back = (src_hp); \
    }

#define BU_LIST_APPEND_LIST(dest_hp, src_hp) \
    if (LIKELY(BU_LIST_NON_EMPTY(src_hp))) {\
	struct bu_list *_first = (src_hp)->forw; \
	struct bu_list *_last = (src_hp)->back; \
	_first->back = (dest_hp)->back; \
	(dest_hp)->back->forw = _first; \
	(dest_hp)->back = _last; \
	_last->forw = (dest_hp); \
	(src_hp)->forw = (src_hp)->back = (src_hp); \
    }

/**
 * Test if a doubly linked list is empty, given head pointer
 */
#define BU_LIST_IS_EMPTY(hp)	((hp)->forw == (hp))
#define BU_LIST_NON_EMPTY(hp)	((hp)->forw != (hp))
#define BU_LIST_NON_EMPTY_P(p, structure, hp)	\
    (((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))
#define BU_LIST_IS_CLEAR(hp)	((hp)->magic == 0 && \
				 (hp)->forw == BU_LIST_NULL && \
				 (hp)->back == BU_LIST_NULL)

/* Return re-cast pointer to first element on list.
 * No checking is performed to see if list is empty.
 */
#define BU_LIST_LAST(structure, hp)	\
    ((struct structure *)((hp)->back))
#define BU_LIST_BACK(structure, hp)	\
    ((struct structure *)((hp)->back))
#define BU_LIST_PREV(structure, hp)	\
    ((struct structure *)((hp)->back))
#define BU_LIST_FIRST(structure, hp)	\
    ((struct structure *)((hp)->forw))
#define BU_LIST_FORW(structure, hp)	\
    ((struct structure *)((hp)->forw))
#define BU_LIST_NEXT(structure, hp)	\
    ((struct structure *)((hp)->forw))

/**
 * Boolean test to see if current list element is the head
 */
#define BU_LIST_IS_HEAD(p, hp)	\
    (((struct bu_list *)(p)) == (struct bu_list *)(hp))
#define BU_LIST_NOT_HEAD(p, hp)	\
    (!BU_LIST_IS_HEAD(p, hp))

/**
 * Boolean test to see if previous list element is the head
 */
#define BU_LIST_PREV_IS_HEAD(p, hp)\
    (((struct bu_list *)(p))->back == (struct bu_list *)(hp))
#define BU_LIST_PREV_NOT_HEAD(p, hp)\
    (!BU_LIST_PREV_IS_HEAD(p, hp))

/**
 * Boolean test to see if the next list element is the head
 */
#define BU_LIST_NEXT_IS_HEAD(p, hp)	\
    (((struct bu_list *)(p))->forw == (struct bu_list *)(hp))
#define BU_LIST_NEXT_NOT_HEAD(p, hp)	\
    (!BU_LIST_NEXT_IS_HEAD(p, hp))

#define BU_LIST_EACH(hp, p, type) \
    for ((p)=(type *)BU_LIST_FIRST(bu_list, hp); \
	 (p) && BU_LIST_NOT_HEAD(p, hp); \
	 (p)=(type *)BU_LIST_PNEXT(bu_list, p)) \

#define BU_LIST_REVEACH(hp, p, type) \
    for ((p)=(type *)BU_LIST_LAST(bu_list, hp); \
	 (p) && BU_LIST_NOT_HEAD(p, hp); \
	 (p)=(type *)BU_LIST_PREV(bu_list, ((struct bu_list *)(p)))) \

#define BU_LIST_TAIL(hp, start, p, type) \
    for ((p)=(type *)start; \
	 (p) && BU_LIST_NOT_HEAD(p, hp); \
	 (p)=(type *)BU_LIST_PNEXT(bu_list, (p)))

/**
 * Intended as innards for a for loop to visit all nodes on list, e.g.:
 *
 * for (BU_LIST_FOR(p, structure, hp)) {
 *	work_on(p);
 * }
 */
#define BU_LIST_FOR(p, structure, hp)	\
    (p)=BU_LIST_FIRST(structure, hp); \
       (p) && BU_LIST_NOT_HEAD(p, hp); \
       (p)=BU_LIST_PNEXT(structure, p)

#define BU_LIST_FOR_BACKWARDS(p, structure, hp)	\
    (p)=BU_LIST_LAST(structure, hp); \
       (p) && BU_LIST_NOT_HEAD(p, hp); \
       (p)=BU_LIST_PLAST(structure, p)

/**
 * Process all the list members except hp and the actual head.  Useful
 * when starting somewhere besides the head.
 */
#define BU_LIST_FOR_CIRC(p, structure, hp)	\
    (p)=BU_LIST_PNEXT_CIRC(structure, hp); \
       (p) && BU_LIST_NOT_HEAD(p, hp); \
       (p)=BU_LIST_PNEXT_CIRC(structure, p)

/**
 * Intended as innards for a for loop to visit elements of two lists
 * in tandem, e.g.:
 *
 * for (BU_LIST_FOR2(p1, p2, structure, hp1, hp2)) {
 *	process(p1, p2);
 * }
 */
#define BU_LIST_FOR2(p1, p2, structure, hp1, hp2)				\
    (p1)=BU_LIST_FIRST(structure, hp1),			\
	(p2)=BU_LIST_FIRST(structure, hp2);			\
				      (p1) && BU_LIST_NOT_HEAD((struct bu_list *)(p1), (hp1)) &&	\
				      (p2) && BU_LIST_NOT_HEAD((struct bu_list *)(p2), (hp2));		\
				      (p1)=BU_LIST_NEXT(structure, (struct bu_list *)(p1)),	\
	(p2)=BU_LIST_NEXT(structure, (struct bu_list *)(p2))

/**
 * Innards for a while loop that constantly picks off the first
 * element.  Useful mostly for a loop that will dequeue every list
 * element, e.g.:
 *
 *	while (BU_LIST_WHILE(p, structure, hp)) {
 *@n		BU_LIST_DEQUEUE(&(p->l));
 *@n		free((char *)p);
 *@n	}
 */
#define BU_LIST_WHILE(p, structure, hp)	\
    (((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))

/**
 * Return the magic number of the first (or last) item on a list
 */
#define BU_LIST_FIRST_MAGIC(hp)		((hp)->forw->magic)
#define BU_LIST_LAST_MAGIC(hp)		((hp)->back->magic)

/**
 * Return pointer to next (or previous) element, which may be the head
 */
#define BU_LIST_PNEXT(structure, p)	\
    ((struct structure *)(((struct bu_list *)(p))->forw))
#define BU_LIST_PLAST(structure, p)	\
    ((struct structure *)(((struct bu_list *)(p))->back))

/**
 * Return pointer two links away, which may include the head
 */
#define BU_LIST_PNEXT_PNEXT(structure, p)	\
    ((struct structure *)(((struct bu_list *)(p))->forw->forw))
#define BU_LIST_PNEXT_PLAST(structure, p)	\
    ((struct structure *)(((struct bu_list *)(p))->forw->back))
#define BU_LIST_PLAST_PNEXT(structure, p)	\
    ((struct structure *)(((struct bu_list *)(p))->back->forw))
#define BU_LIST_PLAST_PLAST(structure, p)	\
    ((struct structure *)(((struct bu_list *)(p))->back->back))

/**
 * Return pointer to circular next element; i.e., ignoring the list head
 */
#define BU_LIST_PNEXT_CIRC(structure, p)	\
    ((BU_LIST_FIRST_MAGIC((struct bu_list *)(p)) == BU_LIST_HEAD_MAGIC) ? \
     BU_LIST_PNEXT_PNEXT(structure, (struct bu_list *)(p)) : \
     BU_LIST_PNEXT(structure, p))

/**
 * Return pointer to circular last element; i.e., ignoring the list head
 */
#define BU_LIST_PPREV_CIRC(structure, p)	\
    ((BU_LIST_LAST_MAGIC((struct bu_list *)(p)) == BU_LIST_HEAD_MAGIC) ? \
     BU_LIST_PLAST_PLAST(structure, (struct bu_list *)(p)) : \
     BU_LIST_PLAST(structure, p))

/**
 * Support for membership on multiple linked lists.
 *
 * When a structure of type '_type' contains more than one bu_list
 * structure within it (such as the NMG edgeuse), this macro can be
 * used to convert a pointer '_ptr2' to a "midway" bu_list structure
 * (an element called '_name2' in structure '_type') back into a
 * pointer to the overall enclosing structure.  Examples:
 *
 * eu = BU_LIST_MAIN_PTR(edgeuse, midway, l2);
 *
 * eu1 = BU_LIST_MAIN_PTR(edgeuse, BU_LIST_FIRST(bu_list, &eg1->eu_hd2), l2);
 *
 * Files using BU_LIST_MAIN_PTR will need to include stddef.h
 */
#define BU_LIST_MAIN_PTR(_type, _ptr2, _name2)	\
    ((struct _type *)(((char *)(_ptr2)) - (bu_offsetof(struct _type, _name2) + bu_offsetof(struct bu_list, magic))))

/**
 * Creates and initializes a bu_list head structure
 */
BU_EXPORT extern struct bu_list *bu_list_new(void);

/**
 * Returns the results of BU_LIST_POP
 */
BU_EXPORT extern struct bu_list *bu_list_pop(struct bu_list *hp);

/**
 * Returns the number of elements on a bu_list brand linked list.
 */
BU_EXPORT extern int bu_list_len(const struct bu_list *hd);

/**
 * Reverses the order of elements in a bu_list linked list.
 */
BU_EXPORT extern void bu_list_reverse(struct bu_list *hd);

/**
 * Given a list of structures allocated with bu_malloc() or
 * bu_calloc() enrolled on a bu_list head, walk the list and free the
 * structures.  This routine can only be used when the structures have
 * no interior pointers.
 */
BU_EXPORT extern void bu_list_free(struct bu_list *hd);

/**
 * Simple parallel-safe routine for appending a data structure to the
 * end of a bu_list doubly-linked list.
 *
 * @par Issues:
 *  	Only one semaphore shared by all list heads.
 * @n	No portable way to notify waiting thread(s) that are sleeping
 */
BU_EXPORT extern void bu_list_parallel_append(struct bu_list *headp,
					      struct bu_list *itemp);

/**
 * Simple parallel-safe routine for dequeueing one data structure from
 * the head of a bu_list doubly-linked list.
 * If the list is empty, wait until some other thread puts something on
 * the list.
 *
 * @par Issues:
 * No portable way to not spin and burn CPU time while waiting
 * @n	for something to show up on the list.
 */
BU_EXPORT extern struct bu_list *bu_list_parallel_dequeue(struct bu_list *headp);

/**
 * Generic bu_list doubly-linked list checker.
 */
BU_EXPORT extern void bu_ck_list(const struct bu_list *hd,
				 const char *str);

/**
 * bu_list doubly-linked list checker which checks the magic number for
 * all elements in the linked list
 */
BU_EXPORT extern void bu_ck_list_magic(const struct bu_list *hd,
				       const char *str,
				       const uint32_t magic);


/**
 * Creates and initializes a bu_list head structure
 */
BU_EXPORT extern struct bu_list *bu_list_new(void);

/**
 * Returns the results of BU_LIST_POP
 */
BU_EXPORT extern struct bu_list *bu_list_pop(struct bu_list *hp);

/**
 * Returns the number of elements on a bu_list brand linked list.
 */
BU_EXPORT extern int bu_list_len(const struct bu_list *hd);

/**
 * Reverses the order of elements in a bu_list linked list.
 */
BU_EXPORT extern void bu_list_reverse(struct bu_list *hd);

/**
 * Given a list of structures allocated with bu_malloc() or
 * bu_calloc() enrolled on a bu_list head, walk the list and free the
 * structures.  This routine can only be used when the structures have
 * no interior pointers.
 */
BU_EXPORT extern void bu_list_free(struct bu_list *hd);

/**
 * Simple parallel-safe routine for appending a data structure to the
 * end of a bu_list doubly-linked list.
 *
 * @par Issues:
 *  	Only one semaphore shared by all list heads.
 * @n	No portable way to notify waiting thread(s) that are sleeping
 */
BU_EXPORT extern void bu_list_parallel_append(struct bu_list *headp,
					      struct bu_list *itemp);

/**
 * Simple parallel-safe routine for dequeueing one data structure from
 * the head of a bu_list doubly-linked list.
 * If the list is empty, wait until some other thread puts something on
 * the list.
 *
 * @par Issues:
 * No portable way to not spin and burn CPU time while waiting
 * @n	for something to show up on the list.
 */
BU_EXPORT extern struct bu_list *bu_list_parallel_dequeue(struct bu_list *headp);

/**
 * Generic bu_list doubly-linked list checker.
 */
BU_EXPORT extern void bu_ck_list(const struct bu_list *hd,
				 const char *str);

/**
 * bu_list doubly-linked list checker which checks the magic number for
 * all elements in the linked list
 */
BU_EXPORT extern void bu_ck_list_magic(const struct bu_list *hd,
				       const char *str,
				       const uint32_t magic);

/** @} */

__END_DECLS

#endif  /* BU_LIST_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
