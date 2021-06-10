/*               C A D A T T R I B U T E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file cadattributes.cpp
 *
 * Brief description
 *
 */

#include <QPainter>
#include <QString>

#include "attributes.h"
#include "app.h"
#include "bu/sort.h"
#include "bu/avs.h"
#include "bu/malloc.h"

CADViewModel::CADViewModel(QObject *parentobj, struct bview **v)
    : QKeyValModel(parentobj)
{
    m_root = new QKeyValNode();
    refresh(v);
}

CADViewModel::~CADViewModel()
{
}

void
CADViewModel::update()
{
    printf("view model update\n");
    refresh(m_v);
}

void
CADViewModel::refresh(struct bview **nv)
{
    m_v = nv;

    if (m_v && *m_v) {
	struct bview *v = *m_v;
	struct bu_vls val = BU_VLS_INIT_ZERO;
	QMap<QString, QKeyValNode*> standard_nodes;
	int i = 0;
	m_root = new QKeyValNode();
	beginResetModel();

	standard_nodes.insert("Name", add_pair("Name", bu_vls_cstr(&v->gv_name), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_size);
	standard_nodes.insert("Size", add_pair("Size", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%d", v->gv_width);
	standard_nodes.insert("Width", add_pair("Width", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%d", v->gv_height);
	standard_nodes.insert("Height", add_pair("Height", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_aet[0]);
	standard_nodes.insert("Az", add_pair("Az", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_aet[1]);
	standard_nodes.insert("El", add_pair("El", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_aet[2]);
	standard_nodes.insert("Tw", add_pair("Tw", bu_vls_cstr(&val), m_root, i));

	endResetModel();

	bu_vls_free(&val);
    } else {
	m_root = new QKeyValNode();
	beginResetModel();
	endResetModel();
    }
}


CADAttributesModel::CADAttributesModel(QObject *parentobj, struct db_i *dbip, struct directory *dp, int show_standard, int show_user)
    : QKeyValModel(parentobj)
{
    int i = 0;
    current_dbip = dbip;
    current_dp = dp;
    if (show_standard) {
	std_visible = 1;
    } else {
	std_visible = 0;
    }
    if (show_user) {
	user_visible = 1;
    } else {
	user_visible = 0;
    }
    m_root = new QKeyValNode();
    BU_GET(avs, struct bu_attribute_value_set);
    bu_avs_init_empty(avs);
    if (std_visible) {
	while (i != ATTR_NULL) {
	    add_pair(db5_standard_attribute(i), "", m_root, i);
	    i++;
	}
    }
    if (dbip != DBI_NULL && dp != RT_DIR_NULL) {
	update(dbip, dp);
    }
}

CADAttributesModel::~CADAttributesModel()
{
    bu_avs_free(avs);
    BU_PUT(avs, struct bu_attribute_value_set);
}

HIDDEN int
attr_children(const char *attr)
{
    if (BU_STR_EQUAL(attr, "color")) return 3;
    return 0;
}


bool CADAttributesModel::canFetchMore(const QModelIndex &idx) const
{
    QKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return false;
    if (rowCount(idx)) {
	return false;
    }
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

void
CADAttributesModel::add_Children(const char *name, QKeyValNode *curr_node)
{
    if (BU_STR_EQUAL(name, "color")) {
	QString val(bu_avs_get(avs, name));
	QStringList vals = val.split(QRegExp("/"));
	(void)add_pair("r", vals.at(0).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_pair("g", vals.at(1).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_pair("b", vals.at(2).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	return;
    }
    (void)add_pair(name, bu_avs_get(avs, name), curr_node, db5_standardize_attribute(name));
}


void CADAttributesModel::fetchMore(const QModelIndex &idx)
{
    QKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt) { // && !idx.child(cnt-1, 0).isValid()) {
	beginInsertRows(idx, 0, cnt);
	add_Children(curr_node->name.toLocal8Bit(),curr_node);
	endInsertRows();
    }
}

bool CADAttributesModel::hasChildren(const QModelIndex &idx) const
{
    QKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    if (curr_node->value == QString("")) return false;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

int CADAttributesModel::update(struct db_i *new_dbip, struct directory *new_dp)
{
    current_dp = new_dp;
    current_dbip = new_dbip;
    if (current_dbip != DBI_NULL && current_dp != RT_DIR_NULL) {
	QMap<QString, QKeyValNode*> standard_nodes;
	int i = 0;
	m_root = new QKeyValNode();
	beginResetModel();
	struct bu_attribute_value_pair *avpp;
	for (BU_AVS_FOR(avpp, avs)) {
	    bu_avs_remove(avs, avpp->name);
	}
	(void)db5_get_attributes(current_dbip, avs, current_dp);

	if (std_visible) {
	    while (i != ATTR_NULL) {
		standard_nodes.insert(db5_standard_attribute(i), add_pair(db5_standard_attribute(i), "", m_root, i));
		i++;
	    }
	    for (BU_AVS_FOR(avpp, avs)) {
		if (db5_is_standard_attribute(avpp->name)) {
		    if (standard_nodes.find(avpp->name) != standard_nodes.end()) {
			QString new_value(avpp->value);
			QKeyValNode *snode = standard_nodes.find(avpp->name).value();
			snode->value = new_value;
		    } else {
			add_pair(avpp->name, avpp->value, m_root, db5_standardize_attribute(avpp->name));
		    }
		}
	    }
	}
	if (user_visible) {
	    for (BU_AVS_FOR(avpp, avs)) {
		if (!db5_is_standard_attribute(avpp->name)) {
		    add_pair(avpp->name, avpp->value, m_root, ATTR_NULL);
		}
	    }
	}
	endResetModel();
    } else {
	m_root = new QKeyValNode();
	beginResetModel();
	endResetModel();
    }
    return 0;
}

void
CADAttributesModel::refresh(const QModelIndex &idx)
{
    current_dbip = ((CADApp *)qApp)->dbip();
    current_dp = (struct directory *)(idx.data(DirectoryInternalRole).value<void *>());
    update(current_dbip, current_dp);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

