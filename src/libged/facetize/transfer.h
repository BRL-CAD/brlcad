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

int
facetize_transfer_staged_bot(struct db_i *target_dbip,
	const char *result_file, const char *object_name);

#endif /* LIBGED_FACETIZE_TRANSFER_H */
