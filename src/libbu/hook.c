/*                          H O O K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bu.h"


void
bu_hook_list_init(struct bu_hook_list *hlp)
{
    BU_LIST_INIT(&hlp->l);
    hlp->hookfunc = NULL;
    hlp->clientdata = GENPTR_NULL;
}

void
bu_add_hook(struct bu_hook_list *hlp, bu_hook_t func, genptr_t clientdata)
{
    struct bu_hook_list *new_hook;

    BU_GETSTRUCT(new_hook, bu_hook_list);
    new_hook->hookfunc = func;
    new_hook->clientdata = clientdata;
    new_hook->l.magic = BU_HOOK_LIST_MAGIC;
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
    struct bu_hook_list *call_hook;

    for (BU_LIST_FOR(call_hook, bu_hook_list, &hlp->l)) {
	if (UNLIKELY(!(call_hook->hookfunc))) {
	    exit(EXIT_FAILURE);	/* don't call through 0! */
	}
	call_hook->hookfunc(call_hook->clientdata, buf);
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
