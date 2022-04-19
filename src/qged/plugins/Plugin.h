#ifndef QTCAD_IPLUGIN_H
#define QTCAD_IPLUGIN_H

#include "common.h"
#include "brlcad_version.h"

#include "qtcad/defines.h"
#include "qtcad/Application.h"

#include <QString>

namespace plugin {

class Plugin
{
  public:
    virtual ~Plugin() = default;

    virtual const QString getName() const = 0;
    virtual const QString getDescription() const = 0;
    virtual int getLoadOrder() const = 0;
    virtual void load(qtcad::Application *app) = 0;
};

} // namespace qtcad

#endif // QTCAD_IPLUGIN_H
