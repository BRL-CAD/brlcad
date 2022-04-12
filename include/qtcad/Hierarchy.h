#ifndef HIERARCHY_HIERARCHY_H
#define HIERARCHY_HIERARCHY_H

#include <QWidget>

namespace hierarchy {

class Hierarchy: public QWidget
{
    Q_OBJECT
  public:
    explicit Hierarchy(QWidget *parent = nullptr);

  signals:
};

} // namespace hierarchy

#endif // HIERARCHY_HIERARCHY_H
