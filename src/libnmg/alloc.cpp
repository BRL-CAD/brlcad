/*                       A L L O C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
/** @file alloc.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "nmg.h"

/* A naive version, until the correct one is working */

int nmg_memtrack = 0;
static struct bu_ptbl *nmgmem;

extern "C" void *
nmg_alloc(size_t size)
{
    void *nmem = bu_calloc(1, size, "NMG_ALLOC");
    if (nmg_memtrack) {
	if (!nmgmem) {
	    BU_GET(nmgmem, struct bu_ptbl);
	    bu_ptbl_init(nmgmem, 8, "nmg mem init");
	}
	bu_ptbl_ins(nmgmem, (long *)nmem);
    }
    return nmem;
}

extern "C" void
nmg_free(void *m, size_t UNUSED(s))
{
    if (!m) return;
    if (nmg_memtrack) {
	bu_ptbl_rm(nmgmem, (long *)m);
    } else {
    }
    bu_free(m, "nmg free");
}

extern "C" void
nmg_destroy()
{
    unsigned int i = 0;
    nmg_memtrack = 0;
    if (!nmgmem) return;
    for (i = 0; i < BU_PTBL_LEN(nmgmem); i++) {
	void *m = (void *)BU_PTBL_GET(nmgmem, i);
	bu_free(m, "NMG_FREE");
    }
    BU_PUT(nmgmem, struct bu_ptbl);
    nmgmem = NULL;
}

#if 0

#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif

#include <cassert>

#if defined(HAVE_THREAD_LOCAL)
static thread_local struct bu_pool *data = {NULL};

#elif defined(HAVE___THREAD)

static __thread struct bu_pool *data = {NULL};

#elif defined(HAVE___DECLSPEC_THREAD)

static __declspec(thread) struct bu_pool *data = {NULL};

#endif

/* #define NMG_GETSTRUCT(p, _type) p = (struct _type *)nmg_alloc(sizeof(struct _type)) */

extern "C" void *
nmg_alloc(size_t size)
{
    assert(size < 2048);
    if (!data)
	data = bu_pool_create(2048);
    return bu_pool_alloc(data, 1, size);
}

void
nmg_free(void *)
{
    /* nada */
}

extern "C" void
nmg_destroy()
{
    bu_pool_delete(data);
}

#endif



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
