static char rcsid[] = "$Id: caseitem.c,v 1.3 1997/01/21 19:19:51 dar Exp $";

/************************************************************************
** Module:	Case_Item
** Description:	This module implements the Case_Item abstraction.  A
**	case item represents a single branch in a case statement; it
**	thus consists of a list of labels and a statement to execute
**	when one of the labels matches the selector.
** Constants:
**	CASE_ITEM_NULL	- the null item
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: caseitem.c,v $
 * Revision 1.3  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.3  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 */

#include "express/caseitem.h"

struct freelist_head CASE_IT_fl;

/*
** Procedure:	CASE_ITinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Case Item module.
*/

void
CASE_ITinitialize(void)
{
	MEMinitialize(&CASE_IT_fl,sizeof(struct Case_Item_),500,100);
}

/*
** Procedure:	CASE_ITcreate
** Parameters:	Linked_List labels	- list of Expressions for case labels
**		Statement   statement	- statement associated with this branch
**		Error*      experrc	- buffer for error code
** Returns:	Case_Item		- the case item created
** Description:	Create and return a new case item.
**
** Notes:	If the 'labels' parameter is LIST_NULL, a case item
**		matching in the default case is created.
*/

Case_Item
CASE_ITcreate(Linked_List labels, Statement statement)
{
	struct Case_Item_ *s = CASE_IT_new();

	s->labels = labels;
	s->action = statement;
	return(s);
}
