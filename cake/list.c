/*
**	Linked list module.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

/*
**	Make an empty list.
*/

List *
makelist0()
{
	reg	List	*list;

	list = make(List);
	ldata(list) = (Cast) 0;
	next(list)  = list;
	prev(list)  = list;
	
	return list;
}

/*
**	Make a list with the argument is its only element.
*/

List *
_makelist(data)
reg	Cast	data;
{
	return addhead(makelist0(), data);
}

/*
**	Add some data to the head of a list.
*/

List *
_addhead(list, data)
reg	List	*list;
reg	Cast	data;
{
	reg	List	*item;

	if (list == NULL)
		list = makelist0();

	item = make(List);
	ldata(item) = data;
	ldata(list) = (Cast) ((int) ldata(list) + 1);

	/* item's pointers	*/
	next(item) = next(list);
	prev(item) = list;
	/* neighbours' pointers	*/
	next(prev(item)) = item;
	prev(next(item)) = item;

	return list;
}

/*
**	Add some data to the tail of a list.
*/

List *
_addtail(list, data)
reg	List	*list;
reg	Cast	data;
{
	reg	List	*item;

	if (list == NULL)
		list = makelist0();

	item = make(List);
	ldata(item) = data;
	ldata(list) = (Cast) ((int) ldata(list) + 1);

	/* item's pointers	*/
	next(item) = list;
	prev(item) = prev(list);
	/* neighbours' pointers	*/
	next(prev(item)) = item;
	prev(next(item)) = item;

	return list;
}

/*
**	Destructively append list2 to list1. Since the header of
**	list2 is not meaningful after the operation, it is freed.
*/

List *
addlist(list1, list2)
reg	List	*list1;
reg	List	*list2;
{
	if (list1 == NULL)
		list1 = makelist0();

	if (list2 == NULL)
		list2 = makelist0();

	if (length(list2) > 0)
	{
		if (length(list1) == 0)
		{
			ldata(list1) = ldata(list2);
			/* pointers from header	*/
			next(list1)  = next(list2);
			prev(list1)  = prev(list2);
			/* pointers to header	*/
			prev(next(list1)) = list1;
			next(prev(list1)) = list1;
		}
		else
		{
			ldata(list1) = (Cast) ((int) ldata(list1) + (int) ldata(list2));
			/* end of list 1 to start of list 2	*/
			next(prev(list1)) = next(list2);
			prev(next(list2)) = prev(list1);
			/* end of list 2 to start of list 1	*/
			next(prev(list2)) = list1;
			prev(list1) = prev(list2);
		}
	}

	oldmem((Cast) list2);
	return list1;
}

/*
**	Return the length of a given list.
*/

int
length(list)
reg	List	*list;
{
	if (list == NULL)
		return 0;

	return (int) ldata(list);
}

/*
**	Delete an item from its linked list, and free the node.
*/
void
delete(list, item)
reg	List	*list;
reg	List	*item;
{
	if (list == NULL)
		return;

	if (item == NULL)
		return;

	ldata(list) = (Cast) ((int) ldata(list) - 1);
	next(prev(item)) = next(item);
	prev(next(item)) = prev(item);

	oldmem((Cast) item);
}
