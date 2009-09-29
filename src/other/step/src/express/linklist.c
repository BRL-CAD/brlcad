static char rcsid[] = "$Id: linklist.c,v 1.3 1997/01/21 19:19:51 dar Exp $";

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: linklist.c,v $
 * Revision 1.3  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.4  1993/01/19  22:17:27  libes
 * *** empty log message ***
 *
 * Revision 1.3  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 */

#define LINKED_LIST_C
#include "express/linklist.h"

void
LISTinitialize(void)
{
	MEMinitialize(&LINK_fl,sizeof(struct Link_),500,100);
	MEMinitialize(&LIST_fl,sizeof(struct Linked_List_),100,50);

	ERROR_empty_list = ERRORcreate(
"Empty list in %s.", SEVERITY_ERROR);

}

Linked_List
LISTcreate()
{
	Linked_List list = LIST_new();
	list->mark = LINK_new();
	list->mark->next = list->mark->prev = list->mark;
	return(list);
}

#if 0
/* could optimize this function! */
Linked_List
LISTcreate_with(Generic g)
{
	Linked_List dst = LISTcreate();
	LISTadd(dst,g);
	return(dst);
}
#endif

Linked_List
LISTcopy(Linked_List src)
{
	Linked_List dst = LISTcreate();
	LISTdo(src,x,Generic)
		LISTadd(dst,x);
	LISTod
	return dst;
}
	

void
LISTfree(list)
Linked_List list;
{
	Link p, q = list->mark->next;

	for (p = q->next; p != list->mark; q = p, p = p->next) {
			LINK_destroy(q);
	}
	if (q != list->mark) LINK_destroy(q);
	LINK_destroy(list->mark);
	LIST_destroy(list);
}

Generic
LISTadd_first(Linked_List list, Generic item)
{
    Link		node;

    node = LINK_new();
    node->data = item;
    (node->next = list->mark->next)->prev = node;
    (list->mark->next = node)->prev = list->mark;
    return item;
}

Generic
LISTadd_last(Linked_List list, Generic item)
{
    Link		node;

    node = LINK_new();
    node->data = item;
    (node->prev = list->mark->prev)->next = node;
    (list->mark->prev = node)->next = list->mark;
    return item;
}

Generic
LISTadd_after(Linked_List list, Link link, Generic item)
{
    Link node;

    if (link == LINK_NULL)
	LISTadd_first(list, item);
    else {
	node = LINK_new();
	node->data = item;
	(node->next = link->next)->prev = node;
	(link->next = node)->prev = link;
    }
    return item;
}

Generic
LISTadd_before(Linked_List list, Link link, Generic item)
{
    Link node;

    if (link == LINK_NULL)
	LISTadd_last(list, item);
    else {
	node = LINK_new();
	node->data = item;

	link->prev->next = node;	/* fix up previous link */
	node->prev = link->prev;
	node->next = link;
	link->prev = node;		/* fix up next link */
    }
    return item;
}


Generic
LISTremove_first(Linked_List list)
{
    Link		node;
    Generic		item;

    node = list->mark->next;
    if (node == list->mark) {
	ERRORreport(ERROR_empty_list, "LISTremove_first");
	return NULL;
    }
    item = node->data;
    (list->mark->next = node->next)->prev = list->mark;
    LINK_destroy(node);
    return item;
}

/* 1st arg is historical and can be removed */
/*ARGSUSED*/
Generic
LISTremove(Linked_List list, Link link)
{
    Generic		item;

    link->next->prev = link->prev;
    link->prev->next = link->next;
    item = link->data;
    LINK_destroy(link);
    return item;
}

Generic
LISTget_first(Linked_List list)
{
    Link node;
    Generic item;

    node = list->mark->next;
    if (node == list->mark) {
	return NULL;
    }
    item = node->data;
    return item;
}
Generic
LISTget_second(Linked_List list)
{
    Link		node;
    Generic		item;

    node = list->mark->next;
    if (node == list->mark) {
	return NULL;
    }
    node = node->next;
    if (node == list->mark) {
	return NULL;
    }
    item = node->data;
    return item;
}

/* first is 1, not 0 */
Generic
LISTget_nth(Linked_List list, int n)
{
	int count = 1;
	Link node;

	for (node = list->mark->next; node != list->mark; node = node->next) {
		if (n == count++) return(node->data);
	}
	return(0);
}

int
LISTget_length(Linked_List list)
{
	Link node;
	int count = 0;

	if (!list) return 0;

	for (node = list->mark->next; node != list->mark; node = node->next) {
		count++;
	}
	return count;
}
