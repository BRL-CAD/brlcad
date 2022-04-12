#ifndef HIERARCHY_TREEVIEW_H
#define HIERARCHY_TREEVIEW_H

#include <QWidget>

namespace hierarchy {

class TreeView : public QWidget
{
    Q_OBJECT
  public:
    explicit TreeView(QWidget *parent = nullptr);

  signals:

};

} // namespace hierarchy

#endif // HIERARCHY_TREEVIEW_H
