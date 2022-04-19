#ifndef QTCAD_APPLICATION_H
#define QTCAD_APPLICATION_H

#include "qtcad/defines.h"
#include "qtcad/MainWindow.h"
#include "qtcad/ImageCache.h"
#include "qtcad/IView.h"
#include "brlcad/ged/defines.h"

#include <QApplication>
#include <QMap>
#include <QDockWidget>
#include <unordered_set>

#include <QDialog>
#include <QSize>
#include <QTreeView>

/* Command type for application level commands */
typedef int (*appCommandFunc)(void *, int, const char **);

namespace qtcad {

class TestWindow : public QWidget
{
    Q_OBJECT
  public:
    explicit TestWindow(QWidget *parent = 0);
};

class QTCAD_EXPORT Application : public QApplication
{
    Q_OBJECT
  public:
    Application(int argc, char **argv);

    void addView(IView *view);

    static ImageCache *getImageCache();

  public slots:
    void mainWindowClosing();

  signals:
    void closing();

  protected:
    bool eventFilter(QObject *obj, QEvent *event);

  private:
    void initialize();
    void initGedp();
    int runCmd(struct bu_vls *msg, int argc, const char **argv);

    TestWindow *testWindow;
    QTreeView *treeView;

    MainWindow *mainWindow;
    struct ged *gedp = NULL;
    struct bview *emptyGvp = NULL;
    QMap<QString, IView *> views;

    static ImageCache *imageCache;
};

} // namespace qtcad

#endif // QTCAD_APPLICATION_H
