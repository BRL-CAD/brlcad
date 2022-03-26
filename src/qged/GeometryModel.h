#ifndef QGED_GEOMETRYMODEL_H
#define QGED_GEOMETRYMODEL_H

#include <QAbstractItemModel>

namespace qged {

class GeometryModel : public QAbstractItemModel
{
    Q_OBJECT
  public:
    explicit GeometryModel(QObject *parent = nullptr);
};

} // namespace ged

#endif // GED_GEOMETRYMODEL_H
