/*                         D B _ F P . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#ifndef RT_DB_FP_H
#define RT_DB_FP_H

#include "common.h"

__BEGIN_DECLS

#include "vmath.h"

#include "bu/vls.h"
#include "rt/defines.h"
#include "rt/db_fullpath.h"


struct db_i;      /* forward declaration */
struct directory; /* forward declaration */

/** @addtogroup db_fullpath
 *
 * @brief
 * Structures and routines for collecting and manipulating paths through the database tree.
 *
 * Originally this header held an experimental API for reworking the implementation
 * of db_full_path.  Although that is still of interest in the long run, for now
 * it has been repurposed to hold versions of full path routines that are aware
 * of a syntax for specifying specific instances of objects in comb trees.  For
 * example, if a tree has the following structure:
 *
 * a
 *  u b
 *  u [M] b
 *
 * the path string "a/b" is insufficient to distinguish between the first and the
 * second instance of b in the comb
 *
 * The functions here support an "obj@#" syntax in path specification strings.
 * In the above example, the first instance of be would be a/b@0 (or just a/b) while
 * the second instance (with the matrix above it) would be a/b@1.
 */
/** @{ */
/** @file rt/db_fp.h */

RT_EXPORT extern int
db_fp_op(struct db_full_path *pathp,
	struct db_i *dbip,
	int depth, /* number of arcs - 0 == all */
	struct resource *resp);

/** @} */

__END_DECLS

#endif /*RT_DB_FP_H*/

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
