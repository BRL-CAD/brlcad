/*                   C A D T R E E N O D E . H
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
/** @file cadtreenode.h
 *
 * Specify the information associated with each tree node in a BRL-CAD
 * Qt tree.
 *
 */

#ifndef CAD_TREENODE_H
#define CAD_TREENODE_H

#include <QVariant>
#include <QString>
#include <QList>
#include <QImage>

#ifndef Q_MOC_RUN
#include "bu/file.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADTreeNode
{
public:
    CADTreeNode(struct directory *in_dp = RT_DIR_NULL, CADTreeNode *aParent=0);
    CADTreeNode(QString dp_name, CADTreeNode *aParent=0);
    ~CADTreeNode();

    QString name;
    int boolean;
    int is_highlighted;
    int instance_highlight;
    struct directory *node_dp;
    QVariant icon;

    CADTreeNode *parent;
    QList<CADTreeNode*> children;
};

#endif //CAD_TREENODE_H

