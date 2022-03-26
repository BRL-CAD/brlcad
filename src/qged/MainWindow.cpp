#include "MainWindow.h"

#include <QMenuBar>
#include <QSettings>

namespace qged {

MainWindow::MainWindow(QWidget *parent, int pSwrastMode, int pQuadMode)
    : QMainWindow{parent}
{
    // This solves the disappearing menubar problem on Ubuntu + fluxbox -
    // suspect Unity's "global toolbar" settings are being used even when
    // the Qt app isn't being run under unity - this is probably a quirk
    // of this particular setup, but it sure is an annoying one...
    menuBar()->setNativeMenuBar(false);

    // Redrawing the main canvas may be expensive when docking and undocking -
    // disable animation to minimize window drawing operations:
    // https://stackoverflow.com/a/17885699/2037687
    setAnimated(false);
    if (pSwrastMode) {

    }
    if (pQuadMode) {

    }
    // Create Menus
    fileMenu = menuBar()->addMenu("File");
    cadOpen = new QAction("Open", this);
    connect(cadOpen, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(cadOpen);

    cadSaveSettings = new QAction("Save Settings", this);
    connect(cadSaveSettings, &QAction::triggered, this, &MainWindow::writeSettings);
    fileMenu->addAction(cadSaveSettings);

    cadSingleView = new QAction("Single View", this);
    connect(cadSingleView, &QAction::triggered, this, &MainWindow::switchToSingleView);
    fileMenu->addAction(cadSingleView);

    cadQuadView = new QAction("Quad View", this);
    connect(cadQuadView, &QAction::triggered, this, &MainWindow::switchToQuadView);
    fileMenu->addAction(cadQuadView);

    cadExit = new QAction("Exit", this);
    QObject::connect(cadExit, &QAction::triggered, this, &MainWindow::close);
    fileMenu->addAction(cadExit);

    viewMenu = menuBar()->addMenu("View");

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu("Help");

    setAnimated(false);
    show();
}

void MainWindow::openFile()
{

}

void MainWindow::writeSettings()
{
    QSettings settings("BRL-CAD", "QGED");
    settings.beginGroup("BRLCAD_MainWindow");
    settings.setValue("size", size());
    settings.endGroup();
}

void MainWindow::switchToQuadView() {}

void MainWindow::switchToSingleView() {}

bool MainWindow::close()
{
    emit closing();
    return QMainWindow::close();
}

void MainWindow::FloatableDock::toWindow(bool floating)
{
    if (floating) {
        setWindowFlags(
            Qt::CustomizeWindowHint |
            Qt::Window |
            Qt::WindowMinimizeButtonHint |
            Qt::WindowMaximizeButtonHint |
            Qt::WindowCloseButtonHint
            );
        show();
    }}

} // namespace ged
