/*          S E M A P H O R E _ R E G I S T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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

#include <vector>

#include "bu/str.h"
#include "bu/parallel.h"

/* #define DEBUGSEM 1 */
#ifdef DEBUGSEM
#  include <iostream>
#endif


static const int SEM_LOCK = 1;

static std::vector<const char *> &
semaphore_registry(bool wipe = false)
{
    /* semaphore #1 is used internally here and only here, so we start with it */
    static std::vector<const char *> semaphores = {"SEM_LOCK"};

    if (wipe) {
#ifdef DEBUGSEM
	std::cout << "!!! clearing " << semaphores.size() << " semaphores" << std::endl;
	for (size_t i = 0; i < semaphores.size(); i++) {
	    std::cout << "!!! found " << semaphores[i] << " = " << i+1 << std::endl;
	}
#endif
	bu_semaphore_acquire(SEM_LOCK);
	semaphores.clear();
	bu_semaphore_release(SEM_LOCK);
    }

    return semaphores;
}


extern "C" void
semaphore_clear(void)
{
    (void)semaphore_registry(true);
}


static size_t
semaphore_registered(const char *name)
{
    const std::vector<const char *> &semaphores = semaphore_registry();

    for (size_t i = 0; i < semaphores.size(); ++i) {
	if (BU_STR_EQUAL(semaphores[i], name)) {
#ifdef DEBUGSEM
	    printf("!!! found %s = %zu\n", semaphores[i], i+1);
#endif
	    return i+1;
	}
    }
    return 0;
}


extern "C" int
bu_semaphore_register(const char *name)
{
    std::vector<const char *> &semaphores = semaphore_registry();

#ifdef DEBUGSEM
    printf("!!! registering %s (have %zu)\n", name, semaphores.size());
#endif

    bu_semaphore_acquire(SEM_LOCK);
    size_t idx = semaphore_registered(name);
    if (!idx) {
	semaphores.push_back(name);
	idx = semaphores.size();
    }
    bu_semaphore_release(SEM_LOCK);

#ifdef DEBUGSEM
    printf("!!! added %s = %zu\n", name, idx);
#endif

    return idx;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
