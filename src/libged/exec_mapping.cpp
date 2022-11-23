/*                    E X E C _ M A P P I N G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file exec_wrapping.cpp
 *
 * Provide compile time wrappers that pass specific libged function calls
 * through to the plugin system.
 *
 * Some validation is also performed to ensure the argv[0] string value makes
 * sense - unlike raw function calls, correct argv[0] values are important with
 * ged_exec to ensure the expected functionality is invoked.
 */

#include "common.h"

#include "./include/plugin.h"


/* For this file, we need to undefine stat if defined so the command template
 * doesn't end up being expanded with the defined version of the string.
 * Without this we end up with ged__stati64 on Windows, instead of ged_stat.
 *
 * If this happens for other commands, the solution will be similar - the
 * purpose of this function is just to expand templates into definitions to
 * call ged_cmd_valid and ged_exec, not to actually execute any other
 * functions; it should be safe to undef them here.
 */
#undef stat



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

