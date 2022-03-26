#include "Application.h"

#include <iostream>

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QFileInfo>

#include "brlcad_version.h"

namespace qged {

Application::Application(int argc, char **argv, int pConsoleMode, int pSwrastMode, int pQuadMode)
    : QApplication(argc, argv)
{
    setOrganizationName("BRL-CAD");
    setOrganizationDomain("brlcad.org");
    setApplicationName("QGED");
    setApplicationVersion(brlcad_version());
    initialize();

    QFile file(":/dark.qss");
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);
    setStyleSheet(stream.readAll());

    mainWindow = new MainWindow(nullptr, pSwrastMode, pQuadMode);

    connect(mainWindow, &MainWindow::closing, this, &Application::closing);

    // (Debugging) Report settings filename
    QSettings dmsettings("BRL-CAD", "QGED");
    if (QFileInfo::exists(dmsettings.fileName())) {
        std::cout << "Reading settings from " << dmsettings.fileName().toStdString() << "\n";
    }

    if (pConsoleMode) {

    }
}

void Application::initialize()
{

}

void Application::mainWindowClosing()
{
    emit closing();
}

} // namespace qged
