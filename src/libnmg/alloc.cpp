/*                       A L L O C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include <set>

extern "C" {
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "nmg.h"
}

/* A naive version, until the correct one is working */

int nmg_memtrack = 0;
static std::set<void *> *nmgmem;

extern "C" void *
nmg_malloc(size_t size, const char *str)
{
    void *nmem = bu_malloc(size, str);
    if (nmg_memtrack) {
	if (!nmgmem) {
	    nmgmem = new std::set<void *>;
	}
	nmgmem->insert(nmem);
    }
    return nmem;
}


extern "C" void *
nmg_calloc(int cnt, size_t size, const char *str)
{
    void *nmem = bu_calloc(cnt, size, str);
    if (nmg_memtrack) {
	if (!nmgmem) {
	    nmgmem = new std::set<void *>;
	}
	nmgmem->insert(nmem);
    }
    return nmem;
}


extern "C" void *
nmg_realloc(void *ptr, size_t size, const char *str)
{
    void *nmem = NULL;
    std::set<void *>::iterator p_it;
    if (ptr && nmgmem) {
	p_it = nmgmem->find(ptr);
	if (p_it != nmgmem->end()) {
	    nmgmem->erase(p_it);
	}
    }
    nmem = bu_realloc(ptr, size, str);
    if (nmg_memtrack && nmem) {
	if (!nmgmem) {
	    nmgmem = new std::set<void *>;
	}
	nmgmem->insert(nmem);
    }
    return nmem;
}


extern "C" void
nmg_free(void *m, const char *s)
{
    std::set<void *>::iterator p_it;
    if (!m) return;
    if (nmgmem) {
	p_it = nmgmem->find(m);
	if (p_it != nmgmem->end()) {
	    nmgmem->erase(p_it);
	}
    }
    bu_free(m, s);
}

extern "C" void
nmg_destroy()
{
    std::set<void *>::iterator nmg_it;
    nmg_memtrack = 0;
    if (!nmgmem) return;
    for (nmg_it = nmgmem->begin(); nmg_it != nmgmem->end(); nmg_it++) {
	void *m = *nmg_it;
	bu_free(m, "NMG_FREE");
    }
    delete nmgmem;
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
