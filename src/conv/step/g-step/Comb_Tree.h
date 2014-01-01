/*                 Comb_Tree.h
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
/** @file step/Comb_Tree.h
 *
 */

#ifndef CONV_STEP_G_STEP_COMB_TREE_H
#define CONV_STEP_G_STEP_COMB_TREE_H

#include "common.h"
#include <map>
#include "ON_Brep.h"

struct comb_maps {
    std::map<struct directory *, STEPentity *> brep_to_step;
    std::map<struct directory *, STEPentity *> brep_to_step_shape;
    std::map<struct directory *, STEPentity *> comb_to_step;
    std::map<struct directory *, STEPentity *> comb_to_step_shape;
};

STEPentity *Comb_Tree_to_STEP(struct directory *dp,
                              struct rt_wdb *wdbp,
                              Registry *registry,
                              InstMgr *instance_list);

#endif /* CONV_STEP_G_STEP_COMB_TREE_H */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
