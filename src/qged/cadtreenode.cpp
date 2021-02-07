/*                 C A D T R E E N O D E . C X X
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file cadtreenode.cxx
 *
 * BRL-CAD Qt tree node handling.
 *
 */

#include "cadtreenode.h"

CADTreeNode::CADTreeNode(struct directory *in_dp, CADTreeNode *aParent)
: node_dp(in_dp), parent(aParent)
{
    if(parent) {
	parent->children.append(this);
    }
    is_highlighted = 0;
    instance_highlight = 0;
}

CADTreeNode::CADTreeNode(QString dp_name, CADTreeNode *aParent)
: name(dp_name), parent(aParent)
{
    node_dp = RT_DIR_NULL;
    if(parent) {
	parent->children.append(this);
    }
}

CADTreeNode::~CADTreeNode()
{
    qDeleteAll(children);
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

