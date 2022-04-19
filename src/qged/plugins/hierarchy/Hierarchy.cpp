#include "qtcad/Hierarchy.h"
#include "common.h"
#include "brlcad_version.h"

extern "C" {

COMPILER_DLLEXPORT const plugin::Plugin *qged_plugin()
{
    return new plugin::hierarchy::Hierarchy();
}

}

namespace plugin::hierarchy {

QString *Hierarchy::name = new QString("BRLCAD_Hierarchy");
QString *Hierarchy::description = new QString("This is the plugin that will provide the BRLCAD base outline editor functionality");
int Hierarchy::loadOrder = 1000;

Hierarchy::Hierarchy() {}

const QString Hierarchy::getName() const
{
    return *name;
}

const QString Hierarchy::getDescription() const
{
    return *description;
}

int Hierarchy::getLoadOrder() const
{
    return loadOrder;
}

void Hierarchy::load(qtcad::Application *)
{

}

} // namespace hierarchy
