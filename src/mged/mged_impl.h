/*                     M G E D _ I M P L . H
 * BRL-CAD
 *
 * Copyright (c) 2019-2025 United States Government as represented by
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
/** @file mged_impl.h
 *
 * Internal state implementations
 */

#include "common.h"
#include "bu.h"
#include "rt/edit.h"

#ifdef __cplusplus

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>

class MGED_Internal {
    public:

	MGED_Internal();
	~MGED_Internal();

	// key is primitive type (e.g. ID_ETO)
	std::map<int, rt_solid_edit_map *> cmd_map;
};

#else

#define MGED_Internal void

#endif

struct mged_state_impl {
    MGED_Internal *i;
};


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
