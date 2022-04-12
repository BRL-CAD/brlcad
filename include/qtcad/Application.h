#ifndef QTCAD_APPLICATION_H
#define QTCAD_APPLICATION_H

#include "qtcad/defines.h"
#include "qtcad/MainWindow.h"
#include "qtcad/ImageCache.h"
#include "brlcad/ged/defines.h"

#include <QApplication>
#include <QMap>
#include <unordered_set>

/* Command type for application level commands */
typedef int (*appCommandFunc)(void *, int, const char **);

namespace qtcad {

class QTCAD_EXPORT Application : public QApplication
{
    Q_OBJECT
  public:
    Application(int argc, char **argv, int pConsoleMode, int pSwrastMode, int pQuadMode);

    static ImageCache *getImageCache();

  public slots:
    void mainWindowClosing();

  signals:
    void closing();

  private:
    void initialize();
    void initGedp();
    int runCmd(struct bu_vls *msg, int argc, const char **argv);

    MainWindow *mainWindow;
    struct ged *gedp = NULL;
    struct bview *emptyGvp = NULL;

    static ImageCache *imageCache;
};

} // namespace qtcad

#endif // QTCAD_APPLICATION_H
