#ifndef QTCAD_APPLICATION_H
#define QTCAD_APPLICATION_H

#include "qtcad/MainWindow.h"
#include <QApplication>
#include <QMap>

namespace qtcad {


class Application : public QApplication
{
    Q_OBJECT
  public:
    Application(int argc, char **argv, int pConsoleMode, int pSwrastMode, int pQuadMode);

  public slots:
    void mainWindowClosing();

  signals:
    void closing();

  private:
    void initialize();

    MainWindow *mainWindow;
};

} // namespace qtcad

#endif // QTCAD_APPLICATION_H
