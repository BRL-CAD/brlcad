#include "qtcad/GeometryModel.h"
#include "bv/util.h"
#include "bu/env.h"
#include "ged/commands.h"
#include "brlcad/rt/db5.h"
#include "brlcad/rt/db_internal.h"
#include "brlcad/rt/nongeom.h"
#include "brlcad/rt/db_io.h"

#include <QFont>
#include <QBrush>

namespace qtcad {

GeometryModel::GeometryModel(struct db_i *dbip, struct directory **dbObjects, int cnt, QObject *parent)
    : QAbstractItemModel{parent}
{
    if (cnt) {
        for (int i = 0; i < cnt; i++) {
            items.push_back(new GeometryItem(dbip, dbObjects[i], IDENTITY_MAT, DB_OP_NULL, i));
        }
    }
}

GeometryModel::~GeometryModel()
{
    items.clear();
}

QModelIndex GeometryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    GeometryItem *item = nullptr;
    if (!parent.isValid()) {
        item = items[row];
    } else {
        item = static_cast<GeometryItem *>(parent.internalPointer())->child(row);
    }

    if (item) {
        return createIndex(row, column, item);
    }

    return QModelIndex();
}

QModelIndex GeometryModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    GeometryItem *childItem = static_cast<GeometryItem *>(index.internalPointer());
    GeometryItem *parentItem = childItem->parent();

    if (parentItem == nullptr) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int GeometryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    if (!parent.isValid()) {
        return items.size();
    }

    return static_cast<GeometryItem *>(parent.internalPointer())->childCount();
}

int GeometryModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant GeometryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        return static_cast<GeometryItem *>(index.internalPointer())->data(index.column());
    }
    if (role == Qt::FontRole) {
        QFont font;
        if (!CHECK_EQUAL_IDENTITY(static_cast<GeometryItem *>(index.internalPointer())->transformationMatrix())) {
            font.setItalic(true);
            font.setBold(true);
        }
        return font;
    }
    if (role == Qt::DecorationRole) {
        QImage icon = static_cast<GeometryItem *>(index.internalPointer())->icon();
        return icon;
    }

    return QVariant();
}

Qt::ItemFlags GeometryModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return QAbstractItemModel::flags(index);
}

QVariant GeometryModel::headerData(int, Qt::Orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        return QString("Geometry Model");
    }
    if (role == Qt::FontRole) {
        QFont font;
        font.setBold(true);
        return font;
    }
    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    return QVariant();
}

} // namespace qtcad
