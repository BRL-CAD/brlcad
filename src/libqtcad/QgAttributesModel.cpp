/*              Q G A T T R I B U T E S M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file QgAttributesModel.cpp
 *
 */

#include "common.h"
#include <QPainter>
#include <QString>
#include <QtGlobal>

#include "bu/sort.h"
#include "bu/avs.h"
#include "bu/malloc.h"
#include "qtcad/QgAttributesModel.h"
#include "qtcad/QgModel.h"

QgAttributesModel::QgAttributesModel(QObject *parentobj, struct db_i *dbip, struct directory *dp, int show_standard, int show_user)
    : QgKeyValModel(parentobj)
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
    m_root = new QgKeyValNode();
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

QgAttributesModel::~QgAttributesModel()
{
    bu_avs_free(avs);
    BU_PUT(avs, struct bu_attribute_value_set);
}

static int
attr_children(const char *attr)
{
    if (BU_STR_EQUAL(attr, "color")) return 3;
    return 0;
}


bool QgAttributesModel::canFetchMore(const QModelIndex &idx) const
{
    QgKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return false;
    if (rowCount(idx)) {
	return false;
    }
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

void
QgAttributesModel::add_Children(const char *name, QgKeyValNode *curr_node)
{
    if (BU_STR_EQUAL(name, "color")) {
	QString val(bu_avs_get(avs, name));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QStringList vals = val.split(QRegExp("/"));
#else
	QStringList vals = val.split(QRegularExpression("/"));
#endif
	(void)add_pair("r", vals.at(0).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_pair("g", vals.at(1).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	(void)add_pair("b", vals.at(2).toLocal8Bit(), curr_node, db5_standardize_attribute(name));
	return;
    }
    (void)add_pair(name, bu_avs_get(avs, name), curr_node, db5_standardize_attribute(name));
}


void QgAttributesModel::fetchMore(const QModelIndex &idx)
{
    QgKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt) { // && !idx.child(cnt-1, 0).isValid()) {
	beginInsertRows(idx, 0, cnt);
	add_Children(curr_node->name.toLocal8Bit(),curr_node);
	endInsertRows();
    }
}

bool QgAttributesModel::hasChildren(const QModelIndex &idx) const
{
    QgKeyValNode *curr_node = IndexNode(idx);
    if (curr_node == m_root) return true;
    if (curr_node->value == QString("")) return false;
    int cnt = attr_children(curr_node->name.toLocal8Bit());
    if (cnt > 0) return true;
    return false;
}

int QgAttributesModel::update(struct db_i *new_dbip, struct directory *new_dp)
{
    current_dp = new_dp;
    current_dbip = new_dbip;
    if (current_dbip != DBI_NULL && current_dp != RT_DIR_NULL) {
	QMap<QString, QgKeyValNode*> standard_nodes;
	int i = 0;
	m_root = new QgKeyValNode();
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
			QgKeyValNode *snode = standard_nodes.find(avpp->name).value();
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
	m_root = new QgKeyValNode();
	beginResetModel();
	endResetModel();
    }
    return 0;
}

void
QgAttributesModel::refresh(const QModelIndex &idx)
{
    QTCAD_SLOT("QgAttributesModel::refresh", 1);
    current_dp = (struct directory *)(idx.data(QgModel::DirectoryInternalRole).value<void *>());
    update(current_dbip, current_dp);
}

void
QgAttributesModel::db_change_refresh()
{
    QTCAD_SLOT("QgAttributesModel::db_change_refresh", 1);
    update(current_dbip, current_dp);
}

void
QgAttributesModel::do_dbi_update(struct db_i *dbip)
{
    QTCAD_SLOT("QgAttributesModel::do_dbi_update", 1);
    current_dbip = dbip;
    m_root = new QgKeyValNode();
    beginResetModel();
    endResetModel();
    if (std_visible) {
	int i = 0;
	while (i != ATTR_NULL) {
	    add_pair(db5_standard_attribute(i), "", m_root, i);
	    i++;
	}
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

