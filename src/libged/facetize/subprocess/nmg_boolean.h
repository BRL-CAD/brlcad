/*                 N M G _ B O O L E A N . H
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

#ifndef FACETIZE_PROCESS_NMG_BOOLEAN_H
#define FACETIZE_PROCESS_NMG_BOOLEAN_H

#include <cstddef>

#include "../worker.h"

struct db_i;
struct ged;
struct FacetizeNmgBooleanResult;

int facetize_nmg_boolean_evaluate(struct ged *gedp,
	const FacetizeWorkerRequest &request,
	FacetizeNmgBooleanResult **result);
size_t facetize_nmg_boolean_payload_size(
	const FacetizeNmgBooleanResult *result);
int facetize_nmg_boolean_write(struct db_i *dbip,
	FacetizeNmgBooleanResult *result);
void facetize_nmg_boolean_destroy(FacetizeNmgBooleanResult *result);

#endif /* FACETIZE_PROCESS_NMG_BOOLEAN_H */
