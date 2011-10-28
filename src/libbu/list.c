/*                          L I S T . C
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

#include <stdio.h>
#include "bu.h"


struct bu_list *
bu_list_new(void)
{
    struct bu_list *new_list;

    BU_GETSTRUCT(new_list, bu_list);
    BU_LIST_INIT(new_list);

    return new_list;
}

struct bu_list *
bu_list_pop(struct bu_list *hp)
{
    struct bu_list *p;

    BU_LIST_POP(bu_list, hp, p);

    return p;
}

int
bu_list_len(register const struct bu_list *hd)
{
    register int count = 0;
    register const struct bu_list *ep;

    for (BU_LIST_FOR(ep, bu_list, hd)) {
	count++;
    }
    return count;
}

void
bu_list_reverse(register struct bu_list *hd)
{
    struct bu_list tmp_hd;
    register struct bu_list *ep = NULL;

    BU_LIST_INIT(&tmp_hd);
    BU_CK_LIST_HEAD(hd);
    BU_LIST_INSERT_LIST(&tmp_hd, hd);

    while (BU_LIST_WHILE(ep, bu_list, &tmp_hd)) {
	BU_LIST_DEQUEUE(ep);
	BU_LIST_APPEND(hd, ep);
    }
}

void
bu_list_free(struct bu_list *hd)
{
    struct bu_list *p;

    while (BU_LIST_WHILE(p, bu_list, hd)) {
	BU_LIST_DEQUEUE(p);
	bu_free((genptr_t)p, "struct bu_list");
    }
}

void
bu_list_parallel_append(struct bu_list *headp, struct bu_list *itemp)
{
    bu_semaphore_acquire(BU_SEM_LISTS);
    BU_LIST_INSERT(headp, itemp);		/* insert before head = append */
    bu_semaphore_release(BU_SEM_LISTS);
}

struct bu_list *
bu_list_parallel_dequeue(struct bu_list *headp)
{
    for (;;) {
	register struct bu_list *p;

	bu_semaphore_acquire(BU_SEM_LISTS);
	p = BU_LIST_FIRST(bu_list, headp);
	if (BU_LIST_NOT_HEAD(p, headp)) {
	    BU_LIST_DEQUEUE(p);
	    bu_semaphore_release(BU_SEM_LISTS);
	    return p;
	}
	bu_semaphore_release(BU_SEM_LISTS);
    }
    /* NOTREACHED */
}

void
bu_ck_list(const struct bu_list *hd, const char *str)
{
    register const struct bu_list *cur;
    int head_count = 0;

    cur = hd;
    do  {
	if (cur->magic == BU_LIST_HEAD_MAGIC)
	    head_count++;

	if (UNLIKELY(!cur->forw)) {
	    bu_log("bu_ck_list(%s) cur=%p, cur->forw=%p, hd=%p\n",
		   str, (void *)cur, (void *)cur->forw, (void *)hd);
	    bu_bomb("bu_ck_list() forw\n");
	}
	if (UNLIKELY(cur->forw->back != cur)) {
	    bu_log("bu_ck_list(%s) cur=%p, cur->forw=%p, cur->forw->back=%p, hd=%p\n",
		   str, (void *)cur, (void *)cur->forw, (void *)cur->forw->back, (void *)hd);
	    bu_bomb("bu_ck_list() forw->back\n");
	}
	if (UNLIKELY(!cur->back)) {
	    bu_log("bu_ck_list(%s) cur=%p, cur->back=%p, hd=%p\n",
		   str, (void *)cur, (void *)cur->back, (void *)hd);
	    bu_bomb("bu_ck_list() back\n");
	}
	if (UNLIKELY(cur->back->forw != cur)) {
	    bu_log("bu_ck_list(%s) cur=%p, cur->back=%p, cur->back->forw=%p, hd=%p\n",
		   str, (void *)cur, (void *)cur->back, (void *)cur->back->forw, (void *)hd);
	    bu_bomb("bu_ck_list() back->forw\n");
	}
	cur = cur->forw;
    } while (cur != hd);

    if (UNLIKELY(head_count != 1)) {
	bu_log("bu_ck_list(%s) head_count = %d, hd=%p\n",
	       str, head_count, (void *)hd);
	bu_bomb("bu_ck_list() headless!\n");
    }
}

void
bu_ck_list_magic(const struct bu_list *hd, const char *str, const uint32_t magic)
{
    register const struct bu_list *cur;
    int head_count = 0;
    int item = 0;

    cur = hd;
    do  {
	if (cur->magic == BU_LIST_HEAD_MAGIC) {
	    head_count++;
	} else if (UNLIKELY(cur->magic != magic)) {
	    void *curmagic = (void *)(ptrdiff_t)cur->magic;
	    void *formagic = (void *)(ptrdiff_t)cur->forw->magic;
	    void *hdmagic = (void *)(ptrdiff_t)hd->magic;
	    bu_log("bu_ck_list(%s) cur magic=(%s)%p, cur->forw magic=(%s)%p, hd magic=(%s)%p, item=%d\n",
		   str,
		   bu_identify_magic(cur->magic), curmagic,
		   bu_identify_magic(cur->forw->magic), formagic,
		   bu_identify_magic(hd->magic), hdmagic,
		   item);
	    bu_bomb("bu_ck_list_magic() cur->magic\n");
	}

	if (UNLIKELY(!cur->forw)) {
	    bu_log("bu_ck_list_magic(%s) cur=%p, cur->forw=%p, hd=%p, item=%d\n",
		   str, (void *)cur, (void *)cur->forw, (void *)hd, item);
	    bu_bomb("bu_ck_list_magic() forw NULL\n");
	}
	if (UNLIKELY(cur->forw->back != cur)) {
	    bu_log("bu_ck_list_magic(%s) cur=%p, cur->forw=%p, cur->forw->back=%p, hd=%p, item=%d\n",
		   str,
		   (void *)cur, (void *)cur->forw, (void *)cur->forw->back, (void *)hd, item);
	    bu_log(" cur=%s, cur->forw=%s, cur->forw->back=%s\n",
		   bu_identify_magic(cur->magic),
		   bu_identify_magic(cur->forw->magic),
		   bu_identify_magic(cur->forw->back->magic));
	    bu_bomb("bu_ck_list_magic() cur->forw->back != cur\n");
	}
	if (UNLIKELY(!cur->back)) {
	    bu_log("bu_ck_list_magic(%s) cur=%p, cur->back=%p, hd=%p, item=%d\n",
		   str, (void *)cur, (void *)cur->back, (void *)hd, item);
	    bu_bomb("bu_ck_list_magic() back NULL\n");
	}
	if (UNLIKELY(cur->back->forw != cur)) {
	    bu_log("bu_ck_list_magic(%s) cur=%p, cur->back=%p, cur->back->forw=%p, hd=%p, item=%d\n",
		   str, (void *)cur, (void *)cur->back, (void *)cur->back->forw, (void *)hd, item);
	    bu_bomb("bu_ck_list_magic() cur->back->forw != cur\n");
	}
	cur = cur->forw;
	item++;
    } while (cur != hd);

    if (UNLIKELY(head_count != 1)) {
	bu_log("bu_ck_list_magic(%s) head_count = %d, hd=%p, items=%d\n",
	       str, head_count, (void *)hd, item);
	bu_bomb("bu_ck_list_magic() headless!\n");
    }
}

/* XXX - apparently needed by muves */
struct bu_list *
bu_list_dequeue_next(struct bu_list *UNUSED(hp), struct bu_list *p)
{
    struct bu_list *p2;

    p2 = BU_LIST_NEXT(bu_list, p);
    BU_LIST_DEQUEUE(p2);

    return p2;
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
