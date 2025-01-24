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

#ifdef __cplusplus

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>

class MGED_Internal {
    public:
	// First key is primitive type (e.g. ID_ETO), second key is ECMD_
	// number.
	std::map<int, std::map<int, std::pair<bu_clbk_t, void *>>> cmd_prerun_clbk;
	std::map<int, std::map<int, std::pair<bu_clbk_t, void *>>> cmd_during_clbk;
	std::map<int, std::map<int, std::pair<bu_clbk_t, void *>>> cmd_postrun_clbk;
	std::map<int, std::map<int, std::pair<bu_clbk_t, void *>>> cmd_linger_clbk;
};

#else

#define MGED_Internal void

#endif

struct mged_state_impl {
    MGED_Internal *i;
};



#ifdef __cplusplus

class MGED_SEDIT_Internal {
    public:
	// Key is ECMD_ type, populated from MGED_Internal map
	std::map<int, std::pair<bu_clbk_t, void *>> cmd_prerun_clbk;
	std::map<int, std::pair<bu_clbk_t, void *>> cmd_during_clbk;
	std::map<int, std::pair<bu_clbk_t, void *>> cmd_postrun_clbk;
	std::map<int, std::pair<bu_clbk_t, void *>> cmd_linger_clbk;

	std::map<bu_clbk_t, int> clbk_recursion_depth_cnt;
	std::map<int, int> cmd_recursion_depth_cnt;
};

#else

#define MGED_SEDIT_Internal void

#endif

struct mged_solid_edit_impl {
    MGED_SEDIT_Internal *i;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
