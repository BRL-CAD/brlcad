/*              Q G A T T R I B U T E S M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
 * Qt model for BRL-CAD object attributes.
 */

#include "common.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "bu/avs.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "qtcad/QgAttributesModel.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSession.h"

QgAttributesModel::QgAttributesModel(QObject *parentobj, struct db_i *dbip,
    struct directory *dp, int show_standard, int show_user)
    : QgKeyValModel(parentobj), std_visible(show_standard != 0),
      user_visible(show_user != 0)
{
    m_root = nullptr;
    BU_GET(avs, struct bu_attribute_value_set);
    bu_avs_init_empty(avs);
    update(dbip, dp);
}

QgAttributesModel::QgAttributesModel(QgSession *session, QObject *parentobj,
    struct directory *dp, int show_standard, int show_user)
    : QgAttributesModel(parentobj, session ? session->dbip() : nullptr, dp,
	show_standard, show_user)
{
    if (session)
	connect(session, &QgSession::db_changed, this,
	    &QgAttributesModel::do_dbi_update);
}

QgAttributesModel::~QgAttributesModel()
{
    delete m_root;
    bu_avs_free(avs);
    BU_PUT(avs, struct bu_attribute_value_set);
}

static QStringList
attribute_child_values(const QgKeyValNode *node)
{
    constexpr int color_component_count = 3;
    if (!node || node->name != "color" || node->value.isEmpty())
	return QStringList();
    QStringList values = node->value.split('/');
    return values.size() == color_component_count ? values : QStringList();
}

void
QgAttributesModel::add_standard_nodes(QgKeyValNode *root)
{
    if (!std_visible)
	return;

    for (int attribute = 0; attribute != ATTR_NULL; attribute++)
	add_pair(db5_standard_attribute(attribute), "", root, attribute);
}

bool
QgAttributesModel::canFetchMore(const QModelIndex &idx) const
{
    QgKeyValNode *current = IndexNode(idx);
    if (!current || current == m_root || rowCount(idx))
	return false;
    return !attribute_child_values(current).isEmpty();
}

void
QgAttributesModel::add_children(QgKeyValNode *current,
    const QStringList &values)
{
    add_pair("r", values.at(0).toLocal8Bit(), current,
	ATTR_COLOR);
    add_pair("g", values.at(1).toLocal8Bit(), current,
	ATTR_COLOR);
    add_pair("b", values.at(2).toLocal8Bit(), current,
	ATTR_COLOR);
}

void
QgAttributesModel::fetchMore(const QModelIndex &idx)
{
    QgKeyValNode *current = IndexNode(idx);
    if (!current || current == m_root)
	return;
    const QStringList values = attribute_child_values(current);
    if (values.isEmpty())
	return;
    beginInsertRows(idx, 0, values.size() - 1);
    add_children(current, values);
    endInsertRows();
}

bool
QgAttributesModel::hasChildren(const QModelIndex &idx) const
{
    QgKeyValNode *current = IndexNode(idx);
    if (!current)
	return false;
    if (current == m_root)
	return rowCount(idx) > 0;
    return rowCount(idx) > 0 || canFetchMore(idx);
}

int
QgAttributesModel::update(struct db_i *new_dbip, struct directory *new_dp)
{
    current_dbip = new_dbip;
    struct directory *selected = (new_dbip && new_dp) ? new_dp : nullptr;
    current_name = selected ? QByteArray(selected->d_namep) : QByteArray();

    bu_avs_free(avs);
    bu_avs_init_empty(avs);

    QgKeyValNode *new_root = new QgKeyValNode();
    add_standard_nodes(new_root);
    if (current_dbip && selected) {
	QMap<QString, QgKeyValNode *> standard_nodes;
	for (QgKeyValNode *node : new_root->children)
	    standard_nodes.insert(node->name, node);

	(void)db5_get_attributes(current_dbip, avs, selected);
	struct bu_attribute_value_pair *attribute;
	for (BU_AVS_FOR(attribute, avs)) {
	    if (db5_is_standard_attribute(attribute->name)) {
		if (!std_visible)
		    continue;
		auto standard = standard_nodes.find(attribute->name);
		if (standard != standard_nodes.end()) {
		    standard.value()->value = attribute->value;
		} else {
		    add_pair(attribute->name, attribute->value, new_root,
			db5_standardize_attribute(attribute->name));
		}
	    } else if (user_visible) {
		add_pair(attribute->name, attribute->value, new_root, ATTR_NULL);
	    }
	}
    }

    beginResetModel();
    delete m_root;
    m_root = new_root;
    endResetModel();
    return 0;
}

void
QgAttributesModel::refresh(const QModelIndex &idx)
{
    QTCAD_SLOT("QgAttributesModel::refresh", 1);
    struct directory *selected = static_cast<struct directory *>(
	idx.data(QgModel::DirectoryInternalRole).value<void *>());
    update(current_dbip, selected);
}

void
QgAttributesModel::db_change_refresh()
{
    QTCAD_SLOT("QgAttributesModel::db_change_refresh", 1);
    struct directory *selected = current_name.isEmpty() || !current_dbip ?
	nullptr : db_lookup(current_dbip, current_name.constData(), LOOKUP_QUIET);
    update(current_dbip, selected);
}

void
QgAttributesModel::do_dbi_update(struct db_i *dbip)
{
    QTCAD_SLOT("QgAttributesModel::do_dbi_update", 1);
    if (dbip != current_dbip) {
	update(dbip, nullptr);
	return;
    }
    db_change_refresh();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
