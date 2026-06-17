/*                 Q G E D M A I N W I N D O W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file QgEdMainWindow.cpp
 *
 * Implementation code for toplevel window for BRL-CAD GUI.
 *
 */

#include <QTimer>
#include <QDialog>
#include <QMessageBox>
#include "qtcad/QgPaletteController.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgViewCtrl.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "QgEdCategories.h"
#include "QgEdMainWindow.h"
#include "QgEdApp.h"

static int
qged_dm_during_clbk(int ac, const char **av, void *u1, void *u2)
{
    (void)u1;
    (void)u2;
    if (ac < 2 || !av)
        return BRLCAD_OK;

    const char *dbg = getenv("GED_DM_DURING_DEBUG");
    if (dbg && BU_STR_EQUAL(dbg, "1")) {
        bu_log("qged dm during callback: ");
        for (int i = 0; i < ac; i++) {
            bu_log("%s%s", av[i], (i + 1 < ac) ? " " : "\n");
        }
    }

    if (!BU_STR_EQUAL(av[1], "set") && !BU_STR_EQUAL(av[1], "bg") && !BU_STR_EQUAL(av[1], "attach"))
        return BRLCAD_OK;

    QgEdApp *ap = (QgEdApp *)qApp;
    if (ap) {
	if (ap->pluginNotifier())
	    emit ap->pluginNotifier()->settingsChanged();
        emit ap->view_update(QG_VIEW_REFRESH);
    }

    return BRLCAD_OK;
}

QgEdMainWindow::QgEdMainWindow(int canvas_type, int quad_view)
{
    QgEdApp *ap = (QgEdApp *)qApp;
    ap->w = this;

    // This solves the disappearing menubar problem on Ubuntu + fluxbox -
    // suspect Unity's "global toolbar" settings are being used even when
    // the Qt app isn't being run under unity - this is probably a quirk
    // of this particular setup, but it sure is an annoying one...
    menuBar()->setNativeMenuBar(false);

    // Redrawing the main canvas may be expensive when docking and undocking -
    // disable animation to minimize window drawing operations:
    // https://stackoverflow.com/a/17885699/2037687
    setAnimated(false);

    // Set up menu
    SetupMenu();

    // Create Widgets
    CreateWidgets(canvas_type);

    // Lay out widgets
    LocateWidgets();

    // Connect Widgets
    ConnectWidgets();

    // See if the user has requested a particular mode
    if (quad_view) {
	c4->changeToQuadFrame();
    } else {
	c4->changeToSingleFrame();
    }
}

void
QgEdMainWindow::CreateWidgets(int canvas_type)
{
    QgEdApp *ap = (QgEdApp *)qApp;
    QgModel *m = ap->mdl;
    struct ged *gedp = m->ged();

    // Define a widget to hold the main view and its associated
    // view control toolbar
    cw = new QWidget(this);

    // The core of the interface is the CAD view widget, which is capable
    // of either a single display or showing 4 views in a grid arrangement
    // (the "quad" view).  By default it displays a single view, unless
    // overridden by a user option.
    c4 = new QgQuadView(cw, gedp, canvas_type);
    if (!c4) {
	QMessageBox *msgbox = new QMessageBox();
	msgbox->setText("Fatal error: unable to create QgQuadView widget");
	msgbox->exec();
	bu_exit(EXIT_FAILURE, "Unable to create QgQuadView widget\n");
    }
    c4->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Define a graphical toolbar with control widgets
    vcw = new QgViewCtrl(cw, gedp);
    vcw->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Having set up the central widget's components, we now define dockable
    // control widgets.  These are the console, controls, etc. that can be
    // attached and detached from the main window.

    // Palettes and their associated dock widgets
    vc = new QgEdPalette(QgEdPalette::ViewMode, this);
    vcd = new QDockWidget("View Controls", this);
    vcd->setObjectName("View_Controls");
    vcd->setWidget(vc);
    oc = new QgEdPalette(QgEdPalette::ObjectMode, this);
    ocd = new QDockWidget("Object Editing", this);
    ocd->setObjectName("Object_Editing");
    ocd->setWidget(oc);

    // We start out with the View Control panel as the current panel - by
    // default we are viewing, not editing
    vc->makeCurrent(vc);

    // Create palette controllers for the two qged palette categories.
    // Ownership is via Qt parentage (parent = this).  populate() is
    // called once in ConnectWidgets() after the plugin manager has been
    // fully wired.  Active view is set in ConnectWidgets() too.
    vc_ctrl = new QgPaletteController(vc, ap->pluginManager(),
				      QStringLiteral(QGED_CATEGORY_VIEW),
				      ap->pluginContext(), this);
    oc_ctrl = new QgPaletteController(oc, ap->pluginManager(),
				      QStringLiteral(QGED_CATEGORY_OBJECT),
				      ap->pluginContext(), this);

    // Command console
    console = new QgConsole(console_dock);
    console->prompt("$ ");
    console_dock = new QgDockWidget("Console", this);
    console_dock->setObjectName("Console");
    console_dock->setWidget(console);
    // The Qt console itself has no direct knowledge of GED commands.  For contextually
    // aware command prompts, we need a customized completer with knowledge of the database
    // contents and GED commands.
    GEDShellCompleter *cshellcomp = new GEDShellCompleter(console, gedp);
    console->setCompleter(cshellcomp);

    /* Geometry Tree */
    treeview = new QgTreeView(tree_dock, ap->mdl);
    tree_dock = new QgDockWidget("Hierarchy", this);
    tree_dock->setObjectName("Hierarchy");
    tree_dock->setWidget(treeview);
    tree_dock->setModel(ap->mdl);

    /* Object Attribute widgets */
    sattrd = new QDockWidget("Standard Attributes", this);
    sattrd->setObjectName("Standard_Attributes");
    stdpropmodel = new QgAttributesModel(0, m->session(), RT_DIR_NULL, 1, 0);
    QgKeyValView *stdpropview = new QgKeyValView(this, 1);
    stdpropview->setModel(stdpropmodel);
    sattrd->setWidget(stdpropview);

    uattrd = new QDockWidget("User Attributes", this);
    uattrd->setObjectName("User_Attributes");
    userpropmodel = new QgAttributesModel(0, m->session(), RT_DIR_NULL, 0, 1);
    QgKeyValView *userpropview = new QgKeyValView(this, 0);
    userpropview->setModel(userpropmodel);
    uattrd->setWidget(userpropview);

}

void
QgEdMainWindow::LocateWidgets()
{
    // Central widget will hold the 3D display widget + Icon toolbar.
    // Create a layout to organize them
    QVBoxLayout *cwl = new QVBoxLayout;
    // The toolbar is added to the layout first, so it is drawn above the Quad view
    cwl->addWidget(vcw);
    cwl->addWidget(c4);
    // Having defined the layout, we set cw to use it and let the main window
    // know cw is the central widget.
    cw->setLayout(cwl);
    setCentralWidget(cw);

    // Set up the dock.  Generally speaking we will also define menu actions to
    // enable and disable these windows, to all them to be recovered even if
    // the user closes them completely.

    /* Because the console usually doesn't need a huge amount of horizontal
     * space and the tree can use all the vertical space it can get when
     * viewing large .g hierarchies, give the bottom corners to the left/right
     * docks */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // Dock the widgets and constrain their allowed placements
    addDockWidget(Qt::BottomDockWidgetArea, console_dock);
    console_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    vm_panels->addAction(console_dock->toggleViewAction());

    addDockWidget(Qt::RightDockWidgetArea, vcd);
    vcd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(vcd->toggleViewAction());

    addDockWidget(Qt::RightDockWidgetArea, ocd);
    ocd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(ocd->toggleViewAction());

    addDockWidget(Qt::LeftDockWidgetArea, tree_dock);
    tree_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(tree_dock->toggleViewAction());

    addDockWidget(Qt::LeftDockWidgetArea, sattrd);
    sattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(sattrd->toggleViewAction());

    addDockWidget(Qt::LeftDockWidgetArea, uattrd);
    uattrd->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    vm_panels->addAction(uattrd->toggleViewAction());
}

void
QgEdMainWindow::ConnectWidgets()
{
    QgEdApp *ap = (QgEdApp *)qApp;
    QgModel *m = ap->mdl;

    // If the model does something that it things should trigger a view update, let the app know
    QObject::connect(m, &QgModel::view_changed, ap, &QgEdApp::do_view_changed);

    // Make the fundamental connection that allows the view to update in
    // response to commands or widgets taking actions that will impact the
    // scene.  Camera view changes, adding/removing objects or view elements
    // from the scene, and updates such as incremental display of raytracing
    // results in an embedded framebuffer all need to notify the QgQuadView
    // it is time to update.
    QObject::connect(ap, &QgEdApp::view_update, c4, &QgQuadView::do_view_update);

    // 3D graphical widget
    QObject::connect(c4, &QgQuadView::selected, ap, &QgEdApp::do_quad_view_change);
    QObject::connect(c4, &QgQuadView::changed, ap, &QgEdApp::do_quad_view_change);
    // Some of the dm initialization has to be delayed - make the connections so we can
    // do the work after widget initialization is complete.
    QObject::connect(c4, &QgQuadView::init_done, this, &QgEdMainWindow::do_dm_init);


    // Graphical toolbar
    QObject::connect(vcw, &QgViewCtrl::view_changed, ap, &QgEdApp::do_view_changed);
    QObject::connect(ap, &QgEdApp::view_update, vcw, &QgViewCtrl::do_view_update);
    // Make the connection so the view control can change the mouse mode of the Quad View
    QObject::connect(vcw, &QgViewCtrl::lmouse_mode, c4, &QgQuadView::set_lmouse_move_default);

    // Cross-palette deselection: only one palette should appear "active"
    // (highlighted tool button) at a time.
    //
    // When vc selects any element, tell oc to visually deselect.
    QObject::connect(vc, &QgToolPalette::palette_element_selected,
		     this, [this](QgToolPaletteElement *) { oc->makeCurrent(vc); });
    // When oc selects any element, tell vc to visually deselect.
    QObject::connect(oc, &QgToolPalette::palette_element_selected,
		     this, [this](QgToolPaletteElement *) { vc->makeCurrent(oc); });
    // When a new-style tool in vc_ctrl is activated, deselect oc.
    QObject::connect(vc_ctrl, &QgPaletteController::currentToolChanged,
		     this, [this](QgToolBase *) { oc->makeCurrent(vc); });
    // When a new-style tool in oc_ctrl is activated, deselect vc.
    QObject::connect(oc_ctrl, &QgPaletteController::currentToolChanged,
		     this, [this](QgToolBase *) { vc->makeCurrent(oc); });

    // Set initial active view on both controllers so that any immediately
    // populated Qt plugin tools can attach their view event filters.
    QgView *init_view = c4->get();
    if (init_view) {
	vc_ctrl->setActiveView(init_view);
	oc_ctrl->setActiveView(init_view);
    }

    // Populate both controllers from the discovered Qt plugins.
    vc_ctrl->populate();
    oc_ctrl->populate();
    rebuildPluginExtensions();
    QObject::connect(ap->pluginManager(), &QgPluginManager::factoryRegistered,
		     this, [this](const QString &) { rebuildPluginExtensions(); });
    QObject::connect(ap->pluginManager(), &QgPluginManager::factoryUnregistered,
		     this, [this](const QString &) { rebuildPluginExtensions(); });

    // The tools in the view and edit panels may have consequences for the view.
    // Connect to the palette signals and slots (the individual tool connections
    // are handled by the palette container.)
    QObject::connect(ap, &QgEdApp::view_update, vc, &QgToolPalette::do_view_update);
    QObject::connect(vc, &QgToolPalette::view_changed, ap, &QgEdApp::do_view_changed);
    QObject::connect(ap, &QgEdApp::view_update, oc, &QgToolPalette::do_view_update);
    QObject::connect(oc, &QgToolPalette::view_changed, ap, &QgEdApp::do_view_changed);

    // Console
    connect(console_dock, &QgDockWidget::topLevelChanged, console_dock, &QgDockWidget::toWindow);
    // The console's run of a command has implications for the entire
    // application, so rather than embedding the command execution logic in the
    // widget we use a signal/slot connection to have the application's slot
    // execute the command.
    QObject::connect(console, &QgConsole::executeCommand, ap, &QgEdApp::run_qcmd);

    // Geometry Tree
    connect(tree_dock, &QgDockWidget::topLevelChanged, tree_dock, &QgDockWidget::toWindow);
    connect(tree_dock, &QgDockWidget::banner_click, m, &QgModel::toggle_hierarchy);
    connect(vm_treeview_mode_toggle, &QAction::triggered, m, &QgModel::toggle_hierarchy);
    connect(m, &QgModel::opened_item, treeview, &QgTreeView::qgitem_select_sync);
    connect(m, &QgModel::view_changed, ap, &QgEdApp::do_view_changed);
    QObject::connect(treeview, &QgTreeView::view_changed, ap, &QgEdApp::do_view_changed);
    QObject::connect(ap, &QgEdApp::view_update, treeview, &QgTreeView::do_view_update);
    // We need to record the expanded/contracted state of the tree items,
    // and restore them after a model reset
    connect(treeview, &QgTreeView::expanded, m, &QgModel::item_expanded);
    connect(treeview, &QgTreeView::collapsed, m, &QgModel::item_collapsed);
    connect(m, &QgModel::mdl_changed_db, treeview, &QgTreeView::redo_expansions);
    connect(m, &QgModel::check_highlights, treeview, &QgTreeView::redo_highlights);

    // Update props if we change the dbip or select a new item in the tree.
    // The attribute models subscribed directly to the session's db_changed
    // signal in their constructor (QgSession* path), so no explicit
    // dbi_update wiring is needed here for those connections.
    QObject::connect(treeview, &QgTreeView::clicked, stdpropmodel, &QgAttributesModel::refresh);
    QObject::connect(ap, &QgEdApp::view_update, stdpropmodel, &QgAttributesModel::db_change_refresh);
    QObject::connect(treeview, &QgTreeView::clicked, userpropmodel, &QgAttributesModel::refresh);
    QObject::connect(ap, &QgEdApp::view_update, userpropmodel, &QgAttributesModel::db_change_refresh);

}

void
QgEdMainWindow::SetupMenu()
{
    QMenu *file_menu = menuBar()->addMenu("File");
    cad_open = new QAction("Open", this);
    QObject::connect(cad_open, &QAction::triggered, ((QgEdApp *)qApp), &QgEdApp::open_file);
    file_menu->addAction(cad_open);

    cad_save_settings = new QAction("Save Settings", this);
    connect(cad_save_settings, &QAction::triggered, ((QgEdApp *)qApp), &QgEdApp::write_settings);
    file_menu->addAction(cad_save_settings);

#if 0
    cad_save_image = new QAction("Save Image", this);
    connect(cad_save_image, &QAction::triggered, this, &QgEdMainWindow::save_image);
    file_menu->addAction(cad_save_image);
#endif

    cad_single_view = new QAction("Single View", this);
    connect(cad_single_view, &QAction::triggered, this, &QgEdMainWindow::SingleDisplay);
    file_menu->addAction(cad_single_view);

    cad_quad_view = new QAction("Quad View", this);
    connect(cad_quad_view, &QAction::triggered, this, &QgEdMainWindow::QuadDisplay);
    file_menu->addAction(cad_quad_view);

    cad_exit = new QAction("Exit", this);
    QObject::connect(cad_exit, &QAction::triggered, this, &QgEdMainWindow::close);
    file_menu->addAction(cad_exit);

    QMenu *view_menu = menuBar()->addMenu("View");
    vm_treeview_mode_toggle = new QAction("Toggle Hierarchy (ls/tops)", this);
    view_menu->addAction(vm_treeview_mode_toggle);
    vm_panels = view_menu->addMenu("Panels");

    QMenu *tools_menu = menuBar()->addMenu("Tools");
    tm_dialogs = tools_menu->addMenu("Dialogs");
    tm_dialogs->setEnabled(false);

    menuBar()->addSeparator();

    menuBar()->addMenu("Help");
}

void
QgEdMainWindow::clearPluginPanels()
{
    if (vm_panels_plugin_separator && vm_panels) {
	vm_panels->removeAction(vm_panels_plugin_separator);
	delete vm_panels_plugin_separator;
	vm_panels_plugin_separator = NULL;
    }

    for (auto it = m_plugin_panels.begin(); it != m_plugin_panels.end(); ++it) {
	QDockWidget *dock = it.value();
	if (!dock)
	    continue;
	if (vm_panels)
	    vm_panels->removeAction(dock->toggleViewAction());
	removeDockWidget(dock);
	dock->deleteLater();
    }
    m_plugin_panels.clear();
}

void
QgEdMainWindow::populatePluginPanels()
{
    QgEdApp *ap = (QgEdApp *)qApp;
    if (!ap || !ap->pluginManager() || !vm_panels)
	return;

    QList<QgPluginDescriptor> descs =
	ap->pluginManager()->descriptors(QStringLiteral(QGED_CATEGORY_PANEL));
    if (descs.isEmpty())
	return;

    vm_panels_plugin_separator = new QAction(this);
    vm_panels_plugin_separator->setSeparator(true);
    vm_panels->addAction(vm_panels_plugin_separator);

    for (const QgPluginDescriptor &desc : descs) {
	QObject *obj = ap->pluginManager()->instance(desc.id);
	IQgPanelFactory *fac = obj ? qobject_cast<IQgPanelFactory *>(obj) : NULL;
	if (!fac)
	    continue;
	QDockWidget *dock = fac->create(ap->pluginContext(), this);
	if (!dock)
	    continue;
	if (dock->objectName().isEmpty())
	    dock->setObjectName(desc.id);
	if (dock->windowTitle().isEmpty())
	    dock->setWindowTitle(desc.displayName);
	addDockWidget(Qt::LeftDockWidgetArea, dock);
	dock->hide();
	vm_panels->addAction(dock->toggleViewAction());
	m_plugin_panels.insert(desc.id, dock);
    }
}

void
QgEdMainWindow::clearPluginDialogs()
{
    for (auto it = m_plugin_dialog_actions.begin(); it != m_plugin_dialog_actions.end(); ++it) {
	QAction *act = it.value();
	if (!act)
	    continue;
	if (tm_dialogs)
	    tm_dialogs->removeAction(act);
	delete act;
    }
    m_plugin_dialog_actions.clear();
    if (tm_dialogs)
	tm_dialogs->setEnabled(false);
}

void
QgEdMainWindow::populatePluginDialogs()
{
    QgEdApp *ap = (QgEdApp *)qApp;
    if (!ap || !ap->pluginManager() || !tm_dialogs)
	return;

    QList<QgPluginDescriptor> descs =
	ap->pluginManager()->descriptors(QStringLiteral(QGED_CATEGORY_DIALOG));
    for (const QgPluginDescriptor &desc : descs) {
	QObject *obj = ap->pluginManager()->instance(desc.id);
	IQgDialogFactory *fac = obj ? qobject_cast<IQgDialogFactory *>(obj) : NULL;
	if (!fac)
	    continue;
	QAction *act = new QAction(desc.displayName, tm_dialogs);
	if (!desc.description.isEmpty())
	    act->setToolTip(desc.description);
	QObject::connect(act, &QAction::triggered,
			 this, [this, id = desc.id]() { launchPluginDialog(id); });
	tm_dialogs->addAction(act);
	m_plugin_dialog_actions.insert(desc.id, act);
    }

    tm_dialogs->setEnabled(!m_plugin_dialog_actions.isEmpty());
}

void
QgEdMainWindow::rebuildPluginExtensions()
{
    clearPluginPanels();
    clearPluginDialogs();
    populatePluginPanels();
    populatePluginDialogs();
}

void
QgEdMainWindow::launchPluginDialog(const QString &id)
{
    QgEdApp *ap = (QgEdApp *)qApp;
    if (!ap || !ap->pluginManager())
	return;

    QObject *obj = ap->pluginManager()->instance(id);
    IQgDialogFactory *fac = obj ? qobject_cast<IQgDialogFactory *>(obj) : NULL;
    if (!fac)
	return;

    QDialog *dialog = fac->create(ap->pluginContext(), this);
    if (!dialog)
	return;

    QgPluginDescriptor desc = ap->pluginManager()->descriptor(id);
    if (dialog->windowTitle().isEmpty())
	dialog->setWindowTitle(desc.displayName);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}



// These commands can only be successfully executed after the QgQuadView widget
// initialization is *fully* complete.  Consequently, these steps are separated
// out from the main constructor.
void
QgEdMainWindow::do_dm_init()
{
    QTCAD_SLOT("QgEdMainWindow::do_dm_init", 1);
    QgEdApp *ap = (QgEdApp *)qApp;
    QgModel *m = ap->mdl;
    struct ged *gedp = m->ged();

    (void)ged_clbk_set(gedp, "dm", BU_CLBK_DURING, qged_dm_during_clbk, NULL);

    (void)ged_clbk_set(gedp, "dm", BU_CLBK_DURING, qged_dm_during_clbk, NULL);

    ///////////////////////////////////////////////////////////////////////////
    // DEBUG - turn on some of the bells and whistles by default, since they
    // won't normally be tested in other ways.  We need fully set up views and
    // dm contexts for these commands to work, and that setup (especially for
    // dm) can be delayed, so we put these invocations in a slot that is
    // specifically triggered once the underlying widgets have informed us that
    // they're ready to go.
    const char *av[5] = {NULL};
    av[0] = "dm";
    av[1] = "bg";
    av[2] = "110/110/110";
    av[3] = "0/0/50";
    ged_exec_dm(gedp, 4, (const char **)av);

    av[0] = "view";
    av[1] = "lod";
    av[2] = "mesh";
    av[3] = "1";
    ged_exec_view(gedp, 4, (const char **)av);

    emit ap->view_update(QG_VIEW_REFRESH);
    ///////////////////////////////////////////////////////////////////////////
}


bool
QgEdMainWindow::isValid3D()
{
    return c4->isValid();
}

void
QgEdMainWindow::close()
{
    closeEvent(NULL);
    QMainWindow::close();
}

void
QgEdMainWindow::closeEvent(QCloseEvent* e)
{
    QSettings settings("BRL-CAD", "QGED");
    // https://bugreports.qt.io/browse/QTBUG-16252?focusedCommentId=250562&page=com.atlassian.jira.plugin.system.issuetabpanels%3Acomment-tabpanel#comment-250562
    settings.setValue("geometry", QVariant(geometry()));
    settings.setValue("windowState", saveState());
    if (!e)
	return;
    QMainWindow::closeEvent(e);
}

/* We base conditionals on whether the target widget w is active.  Usually the
 * actual focus widget is a child of the widget in question, so we walk up the
 * parents to see if the focusWidget is underneath the target widget. */
static bool
widget_active(QWidget *w)
{
    QWidget *fw = qApp->focusWidget();
    QWidget *cw = fw;
    while (cw) {
	if (cw == w) {
	    return true;
	}
	cw = (QWidget *)cw->parent();
    }
    return false;
}

bool
QgEdMainWindow::isDisplayActive()
{
    return widget_active(c4);
}

QgView *
QgEdMainWindow::CurrentDisplay()
{
    return c4->get();
}

struct bsg_view *
QgEdMainWindow::CurrentView()
{
    return c4->view();
}

void
QgEdMainWindow::DisplayCheckpoint()
{
    c4->stash_hashes();
}

bool
QgEdMainWindow::DisplayDiff()
{
    return c4->diff_hashes();
}

void
QgEdMainWindow::QuadDisplay()
{
    c4->changeToQuadFrame();
}

void
QgEdMainWindow::SingleDisplay()
{
    c4->changeToSingleFrame();
}

void
QgEdMainWindow::IndicateRaytraceStart(int val)
{
    vcw->raytrace_start(val);
}

void
QgEdMainWindow::IndicateRaytraceDone()
{
    vcw->raytrace_done();
}

QString
QgEdMainWindow::ActivePaletteCategory(QPoint &gpos)
{
    if (vc) {
	QRect lrect = vc->geometry();
	QPoint mpos = vc->mapFromGlobal(gpos);
	if (lrect.contains(mpos))
	    return QStringLiteral(QGED_CATEGORY_VIEW);
    }

    if (oc) {
	QRect lrect = oc->geometry();
	QPoint mpos = oc->mapFromGlobal(gpos);
	if (lrect.contains(mpos))
	    return QStringLiteral(QGED_CATEGORY_OBJECT);
    }

    return QString();
}

void
QgEdMainWindow::setActiveView(QgView *view)
{
    if (vc_ctrl)
	vc_ctrl->setActiveView(view);
    if (oc_ctrl)
	oc_ctrl->setActiveView(view);
}

int
QgEdMainWindow::InteractionMode(QPoint &gpos)
{
    QString cat = ActivePaletteCategory(gpos);
    if (cat == QStringLiteral(QGED_CATEGORY_VIEW))
	return 0;
    if (cat == QStringLiteral(QGED_CATEGORY_OBJECT))
	return 2;
    return -1;
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
