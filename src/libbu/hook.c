/*
 *			H O O K . C
 *
 * BRL-CAD support library's hook utility.
 *
 *  Authors -
 *	Robert G. Parker
 *	Michael John Muuss
 *	Glenn Durfee
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 * Acknowledgements -
 *	This builds on the work in libbu/log.c.
 *
 */

#include "common.h"



#include <stdio.h>

#include "machine.h"
#include "bu.h"

void
bu_hook_list_init(struct bu_hook_list *hlp)
{
	BU_LIST_INIT(&hlp->l);
	hlp->hookfunc = BUHOOK_NULL;
	hlp->clientdata = GENPTR_NULL;
}

void
bu_add_hook(struct bu_hook_list *hlp, bu_hook_t func, genptr_t clientdata)
{
	struct bu_hook_list *new_hook;

	BU_GETSTRUCT(new_hook, bu_hook_list);
	new_hook->hookfunc = func;
	new_hook->clientdata = clientdata;
	new_hook->l.magic = BUHOOK_LIST_MAGIC;
	BU_LIST_APPEND(&hlp->l, &new_hook->l);
}

void
bu_delete_hook(struct bu_hook_list *hlp, bu_hook_t func, genptr_t clientdata)
{
	struct bu_hook_list *cur = hlp;

	for (BU_LIST_FOR(cur, bu_hook_list, &hlp->l)) {
		if (cur->hookfunc == func && cur->clientdata == clientdata) {
			struct bu_hook_list *old = BU_LIST_PLAST(bu_hook_list, cur);
			BU_LIST_DEQUEUE(&(cur->l));
			bu_free((genptr_t)cur, "bu_delete_hook");
			cur = old;
		}
	}    
}

void
bu_call_hook(struct bu_hook_list *hlp, genptr_t buf)
{
	struct bu_hook_list	*call_hook;

	for (BU_LIST_FOR(call_hook, bu_hook_list, &hlp->l)) {
		if( !(call_hook->hookfunc) )  abort();	/* don't call through 0! */
		call_hook->hookfunc(call_hook->clientdata, buf);
	}
}
