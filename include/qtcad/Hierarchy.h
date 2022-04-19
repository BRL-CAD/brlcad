#ifndef PLUGIN_HIERARCHY_HIERARCHY_H
#define PLUGIN_HIERARCHY_HIERARCHY_H

#include "../Plugin.h"

namespace plugin::hierarchy {

class Hierarchy : public Plugin
{
  public:
    explicit Hierarchy();

    const QString getName() const override;
    const QString getDescription() const override;
    int getLoadOrder() const override;
    void load(qtcad::Application *app) override;

  private:
    static QString *name;
    static QString *description;
    static int loadOrder;
};

} // namespace plugin_hierarchy

#endif // PLUGIN_HIERARCHY_HIERARCHY_H
