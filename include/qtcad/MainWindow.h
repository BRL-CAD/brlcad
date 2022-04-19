#ifndef QTCAD_MAINWINDOW_H
#define QTCAD_MAINWINDOW_H

#include "qtcad/defines.h"
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QDockWidget>
#include <QEvent>
#include <QMoveEvent>

namespace qtcad {

class QTCAD_EXPORT MainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);

  public slots:
    void openFile();
    void writeSettings();
    void switchToSingleView();
    void switchToQuadView();

  signals:
    void closing();

  private:
    void buildMenus();

  private slots:
    void close();

  private:

    QMenu *fileMenu;
    QAction *cadOpen;
    QAction *cadSaveSettings;
    QAction *cadExit;

    QMenu *viewMenu;

    QMenu *helpMenu;
    QAction *cadSingleView;
    QAction *cadQuadView;
};

} // namespace qtcad

#endif // QTCAD_MAINWINDOW_H
