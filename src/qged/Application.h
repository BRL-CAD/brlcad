#ifndef QGED_APPLICATION_H
#define QGED_APPLICATION_H

#include "MainWindow.h"
#include <QApplication>
#include <QMap>

/* Command type for application level commands */
typedef int (*appCommandFunc)(void *, int, const char **);

namespace qged {

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

} // namespace qged

#endif // QGED_APPLICATION_H
