/*                          H O O K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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

/* inteface header */
#include "bu/hook.h"

/* systeam headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* implementation headers */
#include "bu/malloc.h"
#include "bu/log.h"


void
bu_hook_list_init(struct bu_hook_list *hlp)
{
    if (UNLIKELY(hlp == (struct bu_hook_list *)NULL))
	bu_bomb("bu_hook_list_init passed NULL pointer");

    hlp->capacity = hlp->size =  0;
    hlp->hooks = NULL;
}


void
bu_hook_add(struct bu_hook_list *hlp, bu_hook_t func, void *clientdata)
{
    if (hlp->size == hlp->capacity) {
	hlp->capacity = (hlp->capacity > 0) ? hlp->capacity * 2 : 1;
	hlp->hooks = (struct bu_hook *)bu_realloc(
		hlp->hooks,
		sizeof (struct bu_hook) * hlp->capacity,
		"resize hooks");
    }

    hlp->hooks[hlp->size].hookfunc = func;
    hlp->hooks[hlp->size].clientdata = clientdata;
    hlp->size++;
}


void
bu_hook_delete(struct bu_hook_list *hlp, bu_hook_t func, void *clientdata)
{
    struct bu_hook *crt;
    size_t i;

    for (i = 0; i < hlp->size; i++) {
	    crt = &hlp->hooks[i];
	if (crt->hookfunc == func && crt->clientdata == clientdata) {
	    memmove(crt, crt + 1, sizeof (struct bu_hook) * (hlp->size - i));
	    hlp->size--;
	}
    }
}


void
bu_hook_call(struct bu_hook_list *hlp, void *buf)
{
    struct bu_hook *call_hook;
    size_t i;

    for (i = 0; i < hlp->size; i++) {
	    call_hook = &hlp->hooks[i];
	if (UNLIKELY(!(call_hook->hookfunc))) {
	    exit(EXIT_FAILURE);	/* don't call through 0! */
	}
	call_hook->hookfunc(call_hook->clientdata, buf);
    }
}


void
bu_hook_save_all(struct bu_hook_list *hlp, struct bu_hook_list *save_hlp)
{
    size_t i;

    for (i = 0; i < hlp->size; i++) {
	bu_hook_add(save_hlp, hlp->hooks[i].hookfunc, hlp->hooks[i].clientdata);
    }
}


void
bu_hook_delete_all(struct bu_hook_list *hlp)
{
    if (hlp->hooks) bu_free(hlp->hooks, "free hooks");
    hlp->size = hlp->capacity = 0;
}


void
bu_hook_restore_all(struct bu_hook_list *hlp, struct bu_hook_list *restore_hlp)
{
    /* first delete what's there */
    bu_hook_delete_all(hlp);

    /* restore using restore_hlp */
    bu_hook_save_all(restore_hlp, hlp);
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
