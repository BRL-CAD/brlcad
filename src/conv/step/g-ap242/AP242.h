/*                      A P 2 4 2 . h
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file AP242.h
 *
 */

#ifndef AP242_H
#define AP242_H

#include "common.h"
#include <map>
#include "raytrace.h"
#include <sdai.h>
#include <STEPcomplex.h>
#include <STEPattribute.h>
#include <sdai.h>
#include <STEPfile.h>

struct AP242_primitive_result {
    STEPentity *shape;
    STEPentity *product;
};

struct AP242_tree_result {
    STEPentity *shape;
    STEPentity *product;
};

struct AP242_tree_result *
AP242_Add_Tree(struct directory *dp, struct rt_wdb *wdbp, Registry *registry, InstMgr *instance_list);

struct AP242_primitive_result *
AP242_Add_Primitive(struct directory *dp, struct rt_wdb *wdbp, Registry *registry, InstMgr *instance_list);

#endif /* AP242_H */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
