#ifndef QTCAD_IPLUGIN_H
#define QTCAD_IPLUGIN_H

#include "qtcad/defines.h"
#include "Application.h"

#include <QString>
#include <QStringList>

namespace qtcad {

class IPlugin
{
  public:
    virtual ~IPlugin() = default;

    virtual QString *getName() = 0;
    virtual QStringList *getDependList() = 0;
    virtual void load(Application app) = 0;
};

} // namespace qtcad

#endif // QTCAD_IPLUGIN_H
