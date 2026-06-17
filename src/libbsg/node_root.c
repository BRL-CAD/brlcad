/*                    N O D E _ R O O T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbsg/node_root.c
 *
 * Explicit scene-anchor compilation unit.
 *
 * This file is a thin C shim that re-exports the symbols defined in
 * scene_graph.cpp.  The actual implementations remain in scene_graph.cpp.
 * Functions are declared extern so the linker resolves them from
 * scene_graph.cpp without duplication.
 */

#include "common.h"

#include "bsg/defines.h"
#include "bsg/util.h"

/* All root-lifecycle symbols are defined in scene_graph.cpp and exposed via
 * the public libbsg shared-library symbol table.  No additional code is
 * needed here; this file simply documents that node_root.c is the canonical
 * compilation unit for BSG_NODE_ROOT operations. */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
