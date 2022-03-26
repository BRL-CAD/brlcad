#ifndef QGED_MAINWINDOW_H
#define QGED_MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QDockWidget>

namespace qged {

class MainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr, int pSwrastMode = 0, int pQuadMode = 0);

  public slots:
    void openFile();
    void writeSettings();
    void switchToSingleView();
    void switchToQuadView();

  signals:
    void closing();

  public:
    bool close();

  private:
    class FloatableDock;

    QMenu *fileMenu;
    QAction *cadOpen;
    QAction *cadSaveSettings;
    QAction *cadExit;

    QMenu *viewMenu;

    QMenu *helpMenu;
    QAction *cadSingleView;
    QAction *cadQuadView;

    FloatableDock *consoleDock;
    FloatableDock *treeDock;
};

class MainWindow::FloatableDock : public QDockWidget
{
    Q_OBJECT

  public:
    FloatableDock(const QString &title, QWidget *parent) : QDockWidget(title, parent) {}
  public slots:
    void toWindow(bool floating);
};

} // namespace ged

#endif // GED_MAINWINDOW_H
