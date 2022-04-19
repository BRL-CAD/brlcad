#ifndef PLUGIN_HIERARCHY_HIERARCHYVIEW_H
#define PLUGIN_HIERARCHY_HIERARCHYVIEW_H

#include "qtcad/IView.h"

#include <QString>
#include <QDockWidget>

namespace plugin {
namespace hierarchy {

class HierarchyView : public qtcad::IView
{
  public:
    inline QString getName() override { return "BRLCAD_Hierarchy_View"; }
};

} // namespace hierarchy
} // namespace plugin

#endif // PLUGIN_HIERARCHY_HIERARCHYVIEW_H
