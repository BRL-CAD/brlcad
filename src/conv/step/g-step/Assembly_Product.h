/*                 Assembly_Product.h
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/Assembly_Product.h
 *
 */

#ifndef ASSEMBLY_PRODUCT_H_
#define ASSEMBLY_PRODUCT_H_

#include <map>

#include "ON_Brep.h"

STEPentity *Add_Assembly_Product(struct directory *dp,
                                 struct db_i *dbip,
                                 struct bu_ptbl *children,
				 std::map<struct directory *, STEPentity *> *comb_to_step,
				 std::map<struct directory *, STEPentity *> *brep_to_step,
				 Registry *registry,
				 InstMgr *instance_list);

#endif /* ASSEMBLY_PRODUCT_H_ */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
