#ifndef QTCAD_GEOMETRYITEM_H
#define QTCAD_GEOMETRYITEM_H

#include "qtcad/defines.h"
#include <QAbstractItemModel>
#include <QImage>

#include "brlcad/ged/defines.h"
#include "brlcad/rt/op.h"

namespace qtcad {

class QTCAD_EXPORT GeometryItem
{
  public:
    explicit GeometryItem(struct db_i *dbip, struct directory *dp, mat_t transform, db_op_t op, int rowNumber, GeometryItem *parent = nullptr);
    ~GeometryItem();

    void appendChild(GeometryItem *child);

    GeometryItem *child(int row);
    int childCount();
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;

    QImage icon();
    inline fastf_t *transformationMatrix() { return itemTransform; }
    inline int row() { return itemRowNumber; }
    inline GeometryItem *parent() { return itemParent; }

  private:
    void checkChildrenLoaded();
    int loadChildren(int level, int maxLevel);
    void leafFunc(union tree *comb_leaf, int tree_op, int level, int maxLevel);
    void parseComb(struct rt_comb_internal *comb, union tree *comb_tree, int op, int level, int maxLevel);
    int getArbType();
    int getCombType();
    QImage determineIcon();

    struct db_i *itemDb;
    struct directory *itemDirectoryPointer = NULL;
    db_op_t itemOp = DB_OP_UNION;
    QString opText = nullptr;
    mat_t itemTransform = MAT_INIT_IDN;
    int itemRowNumber;
    QImage itemIcon;
    GeometryItem *itemParent;
    std::vector<GeometryItem *> children;
    int itemChildrenLoaded = 0;
};

} // namespace qtcad

#endif // QTCAD_GEOMETRYITEM_H
