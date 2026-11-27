/*                     T R A N S F E R . H
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

#ifndef LIBGED_FACETIZE_TRANSFER_H
#define LIBGED_FACETIZE_TRANSFER_H

#include "rt/db_instance.h"

inline constexpr char FACETIZE_PARTITION_METHOD[] = "CDT_PLANAR_PARTITION";
inline constexpr char FACETIZE_WORKER_PART_OBJECT[] = "facetize_worker_result_part";

bool
facetize_partition_result(struct db_i *dbip, const struct directory *dp);

/** Install a complete partition under a new name with locally named children. */
int
facetize_copy_partition(struct db_i *target, const char *name,
	struct db_i *source, struct directory *partition);

/**
 * Move the worker's fixed-name staged result into @p target_dbip under
 * @p object_name.  The expected type is checked before the target changes.
 */
int
facetize_transfer_staged_object(struct db_i *target_dbip,
	const char *result_file, const char *object_name,
	int expected_object_type, bool allow_partition = false);

#endif /* LIBGED_FACETIZE_TRANSFER_H */
