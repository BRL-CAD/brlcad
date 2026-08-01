/*          S E M A P H O R E _ R E G I S T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "bu/parallel.h"
#include "./parallel.h"

/* #define DEBUGSEM 1 */
#ifdef DEBUGSEM
#  include <iostream>
#endif


static std::vector<std::string> &
semaphore_registry()
{
    /*
     * Registration is process-lifetime infrastructure.  Intentionally keep
     * the registry alive through global destruction so a client destructor
     * may safely use libbu regardless of static object destruction order.
     */
    static std::vector<std::string> *semaphores = []() {
	std::vector<std::string> *registry =
	    new std::vector<std::string>(BU_SEM_DYNAMIC_BASE - 1);
#define BU_SEMAPHORE_RESERVED_NAME(_id, _symbol, _name) (*registry)[(_id) - 1] = (_name);
	BU_SEMAPHORE_RESERVED_LIST(BU_SEMAPHORE_RESERVED_NAME)
#undef BU_SEMAPHORE_RESERVED_NAME
	return registry;
    }();

#ifdef DEBUGSEM
    std::cout << "!!! registry has " << semaphores->size() << " semaphores" << std::endl;
#endif
    return *semaphores;
}


static size_t
semaphore_registered(const char *name)
{
    const std::vector<std::string> &semaphores = semaphore_registry();

    for (size_t i = 0; i < semaphores.size(); ++i) {
	if (!semaphores[i].empty() && semaphores[i] == name) {
#ifdef DEBUGSEM
	    printf("!!! found %s = %zu\n", semaphores[i].c_str(), i+1);
#endif
	    return i+1;
	}
    }
    return 0;
}


extern "C" int
bu_semaphore_register(const char *name)
{
    std::vector<std::string> &semaphores = semaphore_registry();

    if (!name || !name[0]) {
	std::fprintf(stderr, "bu_semaphore_register(): a non-empty name is required\n");
	std::exit(2);
    }

#ifdef DEBUGSEM
    printf("!!! registering %s (have %zu)\n", name, semaphores.size());
#endif

    bu_semaphore_acquire(BU_SEM_ID_REGISTRY);
    size_t idx = semaphore_registered(name);
    if (!idx) {
	if (semaphores.size() >= BU_SEMAPHORE_MAX - 1) {
	    bu_semaphore_release(BU_SEM_ID_REGISTRY);
	    std::fprintf(stderr, "bu_semaphore_register(): maximum semaphore count reached\n");
	    std::exit(2);
	}
	semaphores.push_back(name);
	idx = semaphores.size();
    }
    bu_semaphore_release(BU_SEM_ID_REGISTRY);

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
