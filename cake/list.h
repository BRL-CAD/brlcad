/*
**	Definitions for the list module.
**
**	$Header$
*/

typedef	struct	s_list
{
	Cast		l_data;
	struct	s_list	*l_prev;
	struct	s_list	*l_next;
} List;

#define	next(ptr)	(ptr)->l_next
#define	prev(ptr)	(ptr)->l_prev
#define	ldata(ptr)	(ptr)->l_data
#define	first_ptr(list)	((list)->l_next)
#define	last_ptr(list)	((list)->l_prev)
#define	first(list)	((list)->l_next->l_data)
#define	last(list)	((list)->l_prev->l_data)

#define	for_list(p, l)	for (p = (l? next(l): NULL); p != l && p != NULL; p = next(p))
#define	for_2list(p1, p2, l1, l2)	for \
			(p1 = (l1? next(l1): NULL), p2 = (l2? next(l2): NULL); \
			p1 != l1 && p1 != NULL && p2 != l2 && p2 != NULL; \
			p1 = next(p1), p2 = next(p2))
#define	end_list(p, l)	(p == l || p == NULL)

#define	makelist(d)		_makelist((Cast) d)
#define	addhead(l, d)		_addhead(l, (Cast) d)
#define	addtail(l, d)		_addtail(l, (Cast) d)
#define	insert_before(l, w, d)	_insert_before(l, w, (Cast) d)
#define	insert_after(l, w, d)	_insert_after(l, w, (Cast) d)

extern	List	*makelist0();
extern	List	*_makelist();
extern	List	*_addhead();
extern	List	*_addtail();
extern	List	*addlist();
extern	void	delete();
