/*
 *			R T L I S T . H
 *
 *  A set of general-purpose doubly-linked list macros.
 *  Used throughout LIBRT, but also valuable for general use.
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */

#ifndef SEEN_RTLIST_H
#define SEEN_RTLIST_H yes


/************************************************************************
 *									*
 *			Doubly-linked list support			*
 *									*
 ************************************************************************/

struct rt_list  {
	long		magic;
	struct rt_list	*forw;		/* "forward", "next" */
	struct rt_list	*back;		/* "back", "last" */
};
#define RT_LIST_MAGIC	0x01016580
#define RT_LIST_NULL	((struct rt_list *)0)

/* These macros all expect pointers to rt_list structures */

/* Insert "new" item in front of "old" item.  Often, "old" is the head. */
/* To put the new item at the tail of the list, insert before the head */
#define RT_LIST_INSERT(old,new)	{ \
	(new)->back = (old)->back; \
	(old)->back = (new); \
	(new)->forw = (old); \
	(new)->back->forw = (new);  }

/* Append "new" item after "old" item.  Often, "old" is the head. */
/* To put the new item at the head of the list, append after the head */
#define RT_LIST_APPEND(old,new)	{ \
	(new)->forw = (old)->forw; \
	(new)->back = (old); \
	(old)->forw = (new); \
	(new)->forw->back = (new);  }

/* Dequeue "cur" item from anywhere in doubly-linked list */
#define RT_LIST_DEQUEUE(cur)	{ \
	(cur)->forw->back = (cur)->back; \
	(cur)->back->forw = (cur)->forw; \
	(cur)->forw = (cur)->back = (struct rt_list *)NULL; }

/* Test if a doubly linked list is empty, given head pointer */
#define RT_LIST_IS_EMPTY(hp)	((hp)->forw == (hp))
#define RT_LIST_NON_EMPTY(hp)	((hp)->forw != (hp))

#define RT_LIST_INIT(hp)	{ \
	(hp)->forw = (hp)->back = (hp); \
	(hp)->magic = RT_LIST_MAGIC;	/* sanity */ }

/*
 *  Macros for walking a linked list, where the first element of
 *  some application structure is an rt_list structure.
 *  Thus, the pointer to the rt_list struct is a "pun" for the
 *  application structure as well.
 */
/* Return re-cast pointer to first element on list.
 * No checking is performed to see if list is empty.
 */
#define RT_LIST_LAST(structure,hp)	\
	((struct structure *)((hp)->back))
#define RT_LIST_FIRST(structure,hp)	\
	((struct structure *)((hp)->forw))
#define RT_LIST_NEXT(structure,hp)	\
	((struct structure *)((hp)->forw))
#define RT_LIST_MORE(p,structure,hp)	\
	((p) != (struct structure *)(hp))
#define RT_LIST_PNEXT(structure,p)	\
	((struct structure *)(((struct rt_list *)(p))->forw))
#define RT_LIST_PLAST(structure,p)	\
	((struct structure *)(((struct rt_list *)(p))->back))

#define RT_LIST_PNEXT_PNEXT(structure,p)	\
	((struct structure *)(((struct rt_list *)(p))->forw->forw))
#define RT_LIST_PNEXT_PLAST(structure,p)	\
	((struct structure *)(((struct rt_list *)(p))->forw->back))
#define RT_LIST_PLAST_PNEXT(structure,p)	\
	((struct structure *)(((struct rt_list *)(p))->back->forw))
#define RT_LIST_PLAST_PLAST(structure,p)	\
	((struct structure *)(((struct rt_list *)(p))->back->back))

/* Intended as innards for a for() loop to visit all nodes on list */
#define RT_LIST(p,structure,hp)	\
	(p)=RT_LIST_FIRST(structure,hp); \
	RT_LIST_MORE(p,structure,hp); \
	(p)=RT_LIST_PNEXT(structure,p)

/* Innards for a while() loop that picks off first elements */
#define RT_LIST_LOOP(p,structure,hp)	\
	(((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))

/* Return the magic number of the first (or last) item on a list */
#define RT_LIST_FIRST_MAGIC(hp)		((hp)->forw->magic)
#define RT_LIST_LAST_MAGIC(hp)		((hp)->back->magic)

/* Return pointer to circular next element; ie, ignoring the list head */
#define RT_LIST_PNEXT_CIRC(structure,p)	\
	((RT_LIST_FIRST_MAGIC((struct rt_list *)(p)) == RT_LIST_MAGIC) ? \
		RT_LIST_PNEXT_PNEXT(structure,(struct rt_list *)(p)) : \
		RT_LIST_PNEXT(structure,p) )

/* Return pointer to circular last element; ie, ignoring the list head */
#define RT_LIST_PLAST_CIRC(structure,p)	\
	((RT_LIST_LAST_MAGIC((struct rt_list *)(p)) == RT_LIST_MAGIC) ? \
		RT_LIST_PLAST_PLAST(structure,(struct rt_list *)(p)) : \
		RT_LIST_PLAST(structure,p) )


#endif /* SEEN_RTLIST_H */
