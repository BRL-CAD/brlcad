#include "qtcad/Application.h"
#include "qtcad/GeometryModel.h"
#include "bv/util.h"
#include "bu/env.h"
#include "ged/commands.h"
#include "brlcad/rt/db5.h"
#include "brlcad/rt/db_internal.h"
#include "brlcad/rt/nongeom.h"
#include "brlcad/rt/db_io.h"

#include <iostream>

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QFileInfo>

#include <QDockWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QPalette>

#include "brlcad_version.h"

namespace qtcad {

ImageCache *Application::imageCache = nullptr;

Application::Application(int argc, char **argv, int pConsoleMode, int pSwrastMode, int pQuadMode)
    : QApplication(argc, argv)
{
    setOrganizationName("BRL-CAD");
    setOrganizationDomain("brlcad.org");
    setApplicationName("QGED");
    setApplicationVersion(brlcad_version());
    initialize();

    if (imageCache == nullptr) {
        imageCache = new ImageCache();
    }

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

    initGedp();

    // This is just a test

    QDockWidget *hierarchy = new QDockWidget("Hierarchy", mainWindow);
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, hierarchy);
    hierarchy->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    QTreeView *treeView = new QTreeView(hierarchy);
    treeView->header()->hide();
    hierarchy->setWidget(treeView);

    int ac = 2;
    const char *av[3];
    QString filename = QString("../share/db/havoc.g");
    av[0] = "open";
    av[1] = filename.toLocal8Bit();
    av[2] = NULL;
    runCmd(gedp->ged_result_str, ac, (const char **) av);

    struct directory **db_objects = NULL;
    int path_cnt = db_ls(gedp->dbip, DB_LS_TOPS | DB_LS_CYCLIC, NULL, &db_objects);
    GeometryModel *model = new GeometryModel(gedp->dbip, db_objects, path_cnt);
    treeView->setModel(model);
    bu_free(db_objects, "tops obj list");

    std::cout << "Msg: " << bu_vls_cstr(gedp->ged_result_str) << "<<" << std::endl;
}

ImageCache *Application::getImageCache()
{
    if (Application::imageCache == (ImageCache *) 0) {
        Application::imageCache = new ImageCache();
    }
    return Application::imageCache;
}

void Application::initialize()
{

}

void Application::mainWindowClosing()
{
    emit closing();
}

void Application::initGedp()
{
    BU_GET(gedp, struct ged);
    ged_init(gedp);

    BU_GET(emptyGvp, struct bview);
    bv_init(emptyGvp);
    gedp->ged_gvp = emptyGvp;
    bu_vls_sprintf(&gedp->ged_gvp->gv_name, "default");
    gedp->ged_gvp->gv_db_grps = &gedp->ged_db_grps;
    gedp->ged_gvp->gv_view_shared_objs = &gedp->ged_view_shared_objs;
    gedp->ged_gvp->independent = 0;
}

///////////////////////////////////////////////////////////////////////
//                  .g Centric Methods
///////////////////////////////////////////////////////////////////////
int Application::runCmd(struct bu_vls *msg, int argc, const char **argv)
{
   bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    if (ged_cmd_valid(argv[0], NULL)) {
        const char *ccmd = NULL;
        int edist = ged_cmd_lookup(&ccmd, argv[0]);
        if (edist) {
            if (msg)
                bu_vls_sprintf(msg, "Command %s not found, did you mean %s (edit distance %d)?\n", argv[0],   ccmd, edist);
        }
        return BRLCAD_ERROR;
    }

    int ret = ged_exec(gedp, argc, argv);

    // If this is one of the GED commands supporting incremental input, this
    // return code means we need more input from the application before any
    // command can be run - no need to do the remainder of the logic below.
    if (ret & BRLCAD_MORE)
        return ret;

    if (msg && gedp)
        bu_vls_printf(msg, "%s", bu_vls_cstr(gedp->ged_result_str));

    return ret;
}

} // namespace qtcad
