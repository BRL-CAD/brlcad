#ifndef QTCAD_GEOMETRYMODEL_H
#define QTCAD_GEOMETRYMODEL_H

#include "qtcad/defines.h"
#include <QAbstractItemModel>
#include "brlcad/ged/defines.h"
#include "qtcad/GeometryItem.h"

#define CHECK_EQUAL_IDENTITY(m) (EQUAL(m[0], 1.0) && EQUAL(m[1], 0.0) && EQUAL(m[2], 0.0) && EQUAL(m[3], 0.0) && \
                                 EQUAL(m[4], 0.0) && EQUAL(m[5], 1.0) && EQUAL(m[6], 0.0) && EQUAL(m[7], 0.0) && \
    EQUAL(m[8], 0.0) && EQUAL(m[9], 0.0) && EQUAL(m[10], 1.0) && EQUAL(m[11], 0.0) && \
    EQUAL(m[12], 0.0) && EQUAL(m[13], 0.0) && EQUAL(m[14], 0.0) && EQUAL(m[15], 1.0))

namespace qtcad {

class QTCAD_EXPORT GeometryModel : public QAbstractItemModel
{
    Q_OBJECT
  public:
    explicit GeometryModel(struct db_i *dbip, struct directory **dbObjects, int cnt, QObject *parent = nullptr);
    ~GeometryModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
    std::vector<GeometryItem *> items;

    mat_t IDENTITY_MAT = MAT_INIT_IDN;
};

} // namespace qtcad

#endif // QTCAD_GEOMETRYMODEL_H
