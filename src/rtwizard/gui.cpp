/*                         G U I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_set>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "bu/app.h"
#include "bu/process.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bv.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgSignalFlags.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QgView.h"

#include "../libbu/json.hpp"
#include "../libged/dbi.h"
#include "gui.h"
#include "settings.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

static json
parse_document(const std::string &text)
{
    if (text.empty()) return {{"schema", "brlcad.rtwizard.render"}, {"version", 1}};
    return json::parse(text);
}

static QStringList
json_strings(const json &value)
{
    QStringList result;
    if (!value.is_array()) return result;
    for (const json &entry : value) if (entry.is_string()) result << QString::fromStdString(entry.get<std::string>());
    return result;
}

static json
string_array(const QStringList &values)
{
    json result = json::array();
    for (const QString &value : values) result.push_back(value.toStdString());
    return result;
}

static QStringList
table_strings(const bu_ptbl *table)
{
    QStringList result;
    if (!table) return result;
    for (size_t index = 0; index < BU_PTBL_LEN(table); ++index) {
        const char *value = reinterpret_cast<const char *>(BU_PTBL_GET(table, index));
        if (value && value[0]) result << QString::fromLocal8Bit(value);
    }
    return result;
}

static QString
color_text(const bu_color *color)
{
    unsigned char rgb[3] = {0, 0, 0};
    bu_color_to_rgb_chars(color, rgb);
    return QStringLiteral("%1 %2 %3").arg(rgb[0]).arg(rgb[1]).arg(rgb[2]);
}

static json
vector_value(const QString &text, size_t expected)
{
    QString normalized = text;
    normalized.replace(',', ' ');
    normalized.replace('/', ' ');
    QStringList fields = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (static_cast<size_t>(fields.size()) != expected) throw std::runtime_error("expected " + std::to_string(expected) + " numeric values");
    json result = json::array();
    for (const QString &field : fields) {
        bool ok = false;
        double value = field.toDouble(&ok);
        if (!ok || !std::isfinite(value)) throw std::runtime_error("vector contains an invalid number");
        result.push_back(value);
    }
    return result;
}

static QString
vector_text(const json &value, const QString &fallback)
{
    if (!value.is_array()) return fallback;
    QStringList fields;
    for (const json &entry : value) {
        if (!entry.is_number()) return fallback;
        fields << QString::number(entry.get<double>(), 'g', 15);
    }
    return fields.join(' ');
}

static QWidget *
pageWithForm(QFormLayout *&form, QWidget *parent)
{
    QWidget *page = new QWidget(parent);
    form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    return page;
}

} // namespace

RtWizardRenderWorker::RtWizardRenderWorker(const QStringList &arguments) : m_arguments(arguments)
{
}

void
RtWizardRenderWorker::requestCancel()
{
    m_cancelled.store(true);
}

void
RtWizardRenderWorker::run()
{
    std::vector<QByteArray> encoded;
    std::vector<const char *> argv;
    for (const QString &argument : m_arguments) encoded.push_back(QFile::encodeName(argument));
    for (const QByteArray &argument : encoded) argv.push_back(argument.constData());
    argv.push_back(nullptr);

    bu_process *process = nullptr;
    bu_process_create(&process, argv.data(), BU_PROCESS_OUT_EQ_ERR | BU_PROCESS_HIDE_WINDOW);
    if (!process) {
        emit finished(1, tr("Unable to start rtwizard rendering process."));
        return;
    }
    bu_process_file_close(process, BU_PROCESS_STDIN);
    const int fd = bu_process_fileno(process, BU_PROCESS_STDOUT);
    bool terminated = false;
    while (bu_process_alive(process)) {
        if (m_cancelled.load() && !terminated) {
            bu_pid_terminate(bu_process_pid(process));
            terminated = true;
        }
        if (bu_process_pending(fd)) {
            char buffer[4096];
            int count = bu_process_read_n(process, BU_PROCESS_STDOUT, sizeof(buffer), buffer);
            if (count > 0) emit output(QString::fromLocal8Bit(buffer, count));
        } else {
            QThread::msleep(25);
        }
    }
    while (bu_process_pending(fd)) {
        char buffer[4096];
        int count = bu_process_read_n(process, BU_PROCESS_STDOUT, sizeof(buffer), buffer);
        if (count <= 0) break;
        emit output(QString::fromLocal8Bit(buffer, count));
    }
    const int status = bu_process_wait_n(&process, 0);
    emit finished(status, m_cancelled.load() ? tr("Render cancelled.") :
        (status == 0 ? tr("Render complete.") : tr("Render failed with status %1.").arg(status)));
}

RtWizardMainWindow::RtWizardMainWindow(const QString &program,
    const rtwizard_settings *settings, char pictureType) :
    m_program(program)
{
    setWindowTitle(tr("BRL-CAD RtWizard"));
    setObjectName("RtWizardMainWindow");
    createActions();
    createWorkspace();
    importLegacySettings();
    loadSettings();
    if (settings && bu_vls_strlen(settings->render_spec))
        loadSpecification(QString::fromLocal8Bit(bu_vls_addr(settings->render_spec)));
    applySettings(settings, pictureType);
    updateValidation();
}

RtWizardMainWindow::~RtWizardMainWindow()
{
    if (m_worker) m_worker->requestCancel();
    if (m_renderThread) {
        m_renderThread->quit();
        m_renderThread->wait();
    }
    if (m_view && m_view->view() && m_view->view()->vset)
        bv_set_rm_view(m_view->view()->vset, m_view->view());
}

void
RtWizardMainWindow::createActions()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *openDb = file->addAction(tr("Open &Database..."), this, &RtWizardMainWindow::openDatabase);
    openDb->setShortcut(QKeySequence::Open);
    file->addAction(tr("Open Render &Specification..."), this, &RtWizardMainWindow::openSpecification);
    file->addSeparator();
    QAction *save = file->addAction(tr("&Save Specification"), this, &RtWizardMainWindow::saveSpecification);
    save->setShortcut(QKeySequence::Save);
    QAction *saveAs = file->addAction(tr("Save Specification &As..."), this, &RtWizardMainWindow::saveSpecificationAs);
    saveAs->setShortcut(QKeySequence::SaveAs);
    file->addSeparator();
    QAction *exitAction = file->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    m_imageMenu = menuBar()->addMenu(tr("&Image"));
    m_imageMenu->addAction(tr("Select Image Type..."), this, [this]() { showSettingsPage(0); });
    m_imageMenu->addSeparator();
    const QStringList pictureTypes = {
        tr("Type A — Full color"), tr("Type B — Line drawing"),
        tr("Type C — Color with highlights"), tr("Type D — Mixed color and line"),
        tr("Type E — Color with ghost"), tr("Type F — Color, line, and ghost")
    };
    for (int index = 0; index < pictureTypes.size(); ++index) {
        m_imageMenu->addAction(pictureTypes[index], this, [this, index]() {
            if (m_type) m_type->setCurrentIndex(index);
            showSettingsPage(0);
        });
    }

    m_stepsMenu = menuBar()->addMenu(tr("&Steps"));
    m_stepsMenu->addAction(tr("Select Image Type"), this, [this]() { showSettingsPage(0); });
    m_stepsMenu->addSeparator();
    m_stepsMenu->addAction(tr("Configure Full-Color Elements"), this, [this]() { showRolePage(0); });
    m_stepsMenu->addAction(tr("Configure Line-Drawing Elements"), this, [this]() { showRolePage(1); });
    m_stepsMenu->addAction(tr("Configure Ghost Elements"), this, [this]() { showRolePage(2); });
    m_stepsMenu->addSeparator();
    m_stepsMenu->addAction(tr("Configure View"), this, [this]() { showSettingsPage(1); });
    m_stepsMenu->addAction(tr("Configure Framebuffer and Output"), this, [this]() { showSettingsPage(2); });
    m_stepsMenu->addAction(tr("Configure Animation"), this, [this]() { showSettingsPage(3); });

    QMenu *render = menuBar()->addMenu(tr("&Render"));
    QAction *previewAction = render->addAction(tr("&Preview"));
    previewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(previewAction, &QAction::triggered, this, &RtWizardMainWindow::renderPreview);
    QAction *fullAction = render->addAction(tr("Render &Full Size"));
    fullAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    connect(fullAction, &QAction::triggered, this, &RtWizardMainWindow::renderFull);
    QAction *cancelAction = render->addAction(tr("&Cancel"));
    cancelAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(cancelAction, &QAction::triggered, this, &RtWizardMainWindow::cancelRender);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About RtWizard"), this, [this]() {
        QMessageBox::about(this, tr("About RtWizard"),
            tr("BRL-CAD RtWizard prepares still images and animations using the native ray tracing tools."));
    });
}

void
RtWizardMainWindow::createWorkspace()
{
    QWidget *central = new QWidget(this);
    QStackedLayout *stack = new QStackedLayout(central);
    stack->setStackingMode(QStackedLayout::StackAll);
    m_view = new QgView(central, QgView_SW);
    m_view->enableDefaultKeyBindings();
    m_view->enableDefaultMouseBindings();
    stack->addWidget(m_view);
    m_result = new QLabel(central);
    m_result->setAlignment(Qt::AlignCenter);
    m_result->setScaledContents(false);
    m_result->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_result->hide();
    stack->addWidget(m_result);
    setCentralWidget(central);

    m_rolesDock = new QDockWidget(tr("Configure Render Elements"), this);
    m_rolesDock->setObjectName("ModelRolesDockV2");
    m_treeContainer = new QWidget(m_rolesDock);
    QVBoxLayout *treeLayout = new QVBoxLayout(m_treeContainer);
    QLabel *hint = new QLabel(tr("Select an object, then assign one or more rendering roles."), m_treeContainer);
    hint->setWordWrap(true);
    treeLayout->addWidget(hint);
    QWidget *treePlaceholder = new QWidget(m_treeContainer);
    treePlaceholder->setObjectName("TreePlaceholder");
    treeLayout->addWidget(treePlaceholder, 2);
    QHBoxLayout *roleButtons = new QHBoxLayout;
    roleButtons->addWidget(new QPushButton(tr("+ Color"), m_treeContainer));
    roleButtons->addWidget(new QPushButton(tr("+ Line"), m_treeContainer));
    roleButtons->addWidget(new QPushButton(tr("+ Ghost"), m_treeContainer));
    treeLayout->addLayout(roleButtons);
    connect(qobject_cast<QPushButton *>(roleButtons->itemAt(0)->widget()), &QPushButton::clicked, this, &RtWizardMainWindow::addColorRole);
    connect(qobject_cast<QPushButton *>(roleButtons->itemAt(1)->widget()), &QPushButton::clicked, this, &RtWizardMainWindow::addLineRole);
    connect(qobject_cast<QPushButton *>(roleButtons->itemAt(2)->widget()), &QPushButton::clicked, this, &RtWizardMainWindow::addGhostRole);

    m_roleTabs = new QTabWidget(m_treeContainer);
    m_colorRoles = new QListWidget(m_roleTabs);
    m_lineRoles = new QListWidget(m_roleTabs);
    m_ghostRoles = new QListWidget(m_roleTabs);
    m_roleTabs->addTab(m_colorRoles, tr("Full Color"));
    m_roleTabs->addTab(m_lineRoles, tr("Line Drawing"));
    m_roleTabs->addTab(m_ghostRoles, tr("Ghost"));
    treeLayout->addWidget(m_roleTabs, 1);
    QPushButton *remove = new QPushButton(tr("Remove Selected Role"), m_treeContainer);
    treeLayout->addWidget(remove);
    connect(remove, &QPushButton::clicked, this, &RtWizardMainWindow::removeRole);
    connect(m_roleTabs, &QTabWidget::currentChanged, this, [this](int index) {
        m_colorRoles->setProperty("activeRole", index == 0);
        m_lineRoles->setProperty("activeRole", index == 1);
        m_ghostRoles->setProperty("activeRole", index == 2);
        if (m_rolesDock) {
            const QStringList titles = {tr("Configure the Full-Color Elements"),
                tr("Configure the Line-Drawing Elements"), tr("Configure the Ghost Elements")};
            if (index >= 0 && index < titles.size()) m_rolesDock->setWindowTitle(titles[index]);
        }
    });
    m_colorRoles->setProperty("activeRole", true);
    QHBoxLayout *viewButtons = new QHBoxLayout;
    QPushButton *clear = new QPushButton(tr("Clear Display"), m_treeContainer);
    QPushButton *autoview = new QPushButton(tr("Auto View"), m_treeContainer);
    viewButtons->addWidget(clear);
    viewButtons->addWidget(autoview);
    treeLayout->addLayout(viewButtons);
    connect(clear, &QPushButton::clicked, this, &RtWizardMainWindow::clearScene);
    connect(autoview, &QPushButton::clicked, this, &RtWizardMainWindow::autoviewScene);
    m_rolesDock->setWidget(m_treeContainer);
    addDockWidget(Qt::LeftDockWidgetArea, m_rolesDock);

    m_settingsDock = new QDockWidget(tr("Image and Framebuffer Options"), this);
    m_settingsDock->setObjectName("RenderSettingsDockV2");
    m_tabs = new QTabWidget(m_settingsDock);

    QFormLayout *imageForm = nullptr;
    QWidget *imagePage = pageWithForm(imageForm, m_tabs);
    m_type = new QComboBox(imagePage);
    m_type->addItems({"A — Full color", "B — Line drawing", "C — Color with highlights",
        "D — Mixed color and line", "E — Color with ghost", "F — Color, line, and ghost"});
    m_width = new QSpinBox(imagePage); m_width->setRange(1, 32768); m_width->setValue(512);
    m_height = new QSpinBox(imagePage); m_height->setRange(1, 32768); m_height->setValue(512);
    m_background = new QLineEdit("255 255 255", imagePage);
    m_lineColor = new QLineEdit("0 0 0", imagePage);
    m_nonLineColor = new QLineEdit("0 0 0", imagePage);
    m_ghostIntensity = new QDoubleSpinBox(imagePage); m_ghostIntensity->setRange(0.01, 100.0); m_ghostIntensity->setValue(6.0);
    m_occlusion = new QSpinBox(imagePage); m_occlusion->setRange(1, 3); m_occlusion->setValue(1);
    m_aoSamples = new QSpinBox(imagePage); m_aoSamples->setRange(0, 100000);
    m_aoRadius = new QDoubleSpinBox(imagePage); m_aoRadius->setRange(0.0, 1.0e12); m_aoRadius->setDecimals(6);
    imageForm->addRow(tr("Picture template"), m_type);
    imageForm->addRow(tr("Width"), m_width); imageForm->addRow(tr("Height"), m_height);
    imageForm->addRow(tr("Background RGB"), m_background); imageForm->addRow(tr("Line RGB"), m_lineColor);
    imageForm->addRow(tr("Non-line RGB"), m_nonLineColor); imageForm->addRow(tr("Ghost intensity"), m_ghostIntensity);
    imageForm->addRow(tr("Occlusion mode"), m_occlusion); imageForm->addRow(tr("AO samples"), m_aoSamples);
    imageForm->addRow(tr("AO radius"), m_aoRadius);
    m_tabs->addTab(imagePage, tr("Image"));

    QFormLayout *viewForm = nullptr;
    QWidget *viewPage = pageWithForm(viewForm, m_tabs);
    auto angle = [viewPage]() { QDoubleSpinBox *s = new QDoubleSpinBox(viewPage); s->setRange(-36000, 36000); s->setDecimals(4); return s; };
    m_viewMode = new QComboBox(viewPage); m_viewMode->addItems({tr("Azimuth/elevation"), tr("Exact eye/quaternion")});
    m_azimuth = angle(); m_azimuth->setValue(35); m_elevation = angle(); m_elevation->setValue(25); m_twist = angle();
    m_zoom = new QDoubleSpinBox(viewPage); m_zoom->setRange(0.000001, 1.0e9); m_zoom->setValue(1); m_zoom->setDecimals(6);
    m_perspective = new QDoubleSpinBox(viewPage); m_perspective->setRange(0, 179); m_perspective->setDecimals(4);
    m_center = new QLineEdit(viewPage); m_center->setPlaceholderText(tr("Automatic, or X Y Z"));
    m_eye = new QLineEdit(viewPage); m_eye->setPlaceholderText(tr("X Y Z"));
    m_orientation = new QLineEdit(viewPage); m_orientation->setPlaceholderText(tr("Quaternion X Y Z W"));
    m_viewSize = new QDoubleSpinBox(viewPage); m_viewSize->setRange(0.000001, 1.0e12); m_viewSize->setDecimals(6);
    viewForm->addRow(tr("View mode"), m_viewMode);
    viewForm->addRow(tr("Azimuth"), m_azimuth); viewForm->addRow(tr("Elevation"), m_elevation);
    viewForm->addRow(tr("Twist"), m_twist); viewForm->addRow(tr("Zoom"), m_zoom);
    viewForm->addRow(tr("Perspective"), m_perspective); viewForm->addRow(tr("Center"), m_center);
    viewForm->addRow(tr("Eye point"), m_eye); viewForm->addRow(tr("Orientation"), m_orientation);
    viewForm->addRow(tr("View size"), m_viewSize);
    connect(m_viewMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int mode) {
        const bool exact = mode == 1;
        m_azimuth->setEnabled(!exact); m_elevation->setEnabled(!exact);
        m_twist->setEnabled(!exact); m_zoom->setEnabled(!exact); m_center->setEnabled(!exact);
        m_eye->setEnabled(exact); m_orientation->setEnabled(exact); m_viewSize->setEnabled(exact);
        updateValidation();
    });
    m_eye->setEnabled(false); m_orientation->setEnabled(false); m_viewSize->setEnabled(false);
    m_tabs->addTab(viewPage, tr("View"));

    QFormLayout *outputForm = nullptr;
    QWidget *outputPage = pageWithForm(outputForm, m_tabs);
    QWidget *outputRow = new QWidget(outputPage); QHBoxLayout *outputLayout = new QHBoxLayout(outputRow); outputLayout->setContentsMargins(0, 0, 0, 0);
    m_outputPath = new QLineEdit(outputRow); QPushButton *outputBrowse = new QPushButton(tr("..."), outputRow); outputLayout->addWidget(m_outputPath); outputLayout->addWidget(outputBrowse);
    m_frameDir = new QLineEdit(outputPage); m_frameDir->setPlaceholderText(tr("Optional numbered PNG frame directory"));
    m_fbDevice = new QLineEdit(outputPage); m_fbDevice->setPlaceholderText(tr("Optional, for example /dev/mem or /dev/wgl"));
    m_fbPort = new QSpinBox(outputPage); m_fbPort->setRange(-1, 65535); m_fbPort->setValue(-1); m_fbPort->setSpecialValueText(tr("Disabled"));
    m_fbTransport = new QComboBox(outputPage); m_fbTransport->addItems({tr("Automatic (local IPC preferred)"), tr("Local IPC"), tr("TCP")});
    outputForm->addRow(tr("Output file"), outputRow); outputForm->addRow(tr("Frame directory"), m_frameDir);
    outputForm->addRow(tr("Framebuffer device"), m_fbDevice); outputForm->addRow(tr("Framebuffer port"), m_fbPort); outputForm->addRow(tr("Framebuffer transport"), m_fbTransport);
    connect(outputBrowse, &QPushButton::clicked, this, &RtWizardMainWindow::chooseOutput);
    m_tabs->addTab(outputPage, tr("Output"));

    QWidget *animationPage = new QWidget(m_tabs); QVBoxLayout *animationLayout = new QVBoxLayout(animationPage);
    QFormLayout *timing = new QFormLayout;
    m_animation = new QComboBox(animationPage); m_animation->addItems({tr("None"), tr("Cutting plane"), tr("Orbit camera"), tr("Turntable object"), tr("Camera keyframes")});
    m_duration = new QDoubleSpinBox(animationPage); m_duration->setRange(0.001, 1.0e9); m_duration->setValue(5); m_duration->setDecimals(3);
    m_fps = new QSpinBox(animationPage); m_fps->setRange(1, 1000); m_fps->setValue(10);
    m_frames = new QSpinBox(animationPage); m_frames->setRange(0, 1000000); m_frames->setSpecialValueText(tr("Automatic"));
    timing->addRow(tr("Animation"), m_animation); timing->addRow(tr("Duration (s)"), m_duration);
    timing->addRow(tr("Frames/second"), m_fps); timing->addRow(tr("Exact frames"), m_frames); animationLayout->addLayout(timing);
    m_animationPages = new QStackedWidget(animationPage);
    m_animationPages->addWidget(new QWidget(m_animationPages));
    QFormLayout *cutForm = nullptr; QWidget *cutPage = pageWithForm(cutForm, m_animationPages); m_cutDirection = new QLineEdit("0 0 1", cutPage); cutForm->addRow(tr("Direction"), m_cutDirection); m_animationPages->addWidget(cutPage);
    QFormLayout *orbitForm = nullptr; QWidget *orbitPage = pageWithForm(orbitForm, m_animationPages); m_orbitAngle = angle(); m_orbitAngle->setValue(360); m_orbitAxis = new QLineEdit("0 0 1", orbitPage); m_orbitCenter = new QLineEdit(orbitPage); m_orbitCenter->setPlaceholderText(tr("Automatic rendered-bounds center")); m_orbitElevation = angle(); m_orbitRadius = new QDoubleSpinBox(orbitPage); m_orbitRadius->setRange(0.0, 1.0e12); m_orbitRadius->setDecimals(6); m_orbitRadius->setSpecialValueText(tr("Automatic"));
    orbitForm->addRow(tr("Angle"), m_orbitAngle); orbitForm->addRow(tr("Axis"), m_orbitAxis); orbitForm->addRow(tr("Look-at center"), m_orbitCenter); orbitForm->addRow(tr("Elevation"), m_orbitElevation); orbitForm->addRow(tr("Radius"), m_orbitRadius); m_animationPages->addWidget(orbitPage);
    QFormLayout *turntableForm = nullptr; QWidget *turntablePage = pageWithForm(turntableForm, m_animationPages); m_turntableObject = new QLineEdit(turntablePage); m_turntableAngle = angle(); m_turntableAngle->setValue(360); m_turntableAxis = new QLineEdit("0 0 1", turntablePage); m_turntableCenter = new QLineEdit(turntablePage); m_turntableCenter->setPlaceholderText(tr("Automatic object-bounds center"));
    turntableForm->addRow(tr("Object path"), m_turntableObject); turntableForm->addRow(tr("Angle"), m_turntableAngle); turntableForm->addRow(tr("Axis"), m_turntableAxis); turntableForm->addRow(tr("Pivot center"), m_turntableCenter); m_animationPages->addWidget(turntablePage);
    QWidget *keyPage = new QWidget(m_animationPages); QVBoxLayout *keyLayout = new QVBoxLayout(keyPage); m_timeline = new QTableWidget(0, 4, keyPage); m_timeline->setHorizontalHeaderLabels({tr("Time"), tr("Eye"), tr("Orientation"), tr("View size")}); m_timeline->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); keyLayout->addWidget(m_timeline);
    QHBoxLayout *keyButtons = new QHBoxLayout; m_keyTime = new QDoubleSpinBox(keyPage); m_keyTime->setRange(0, 1.0e9); QPushButton *addKey = new QPushButton(tr("Capture Current View"), keyPage); QPushButton *removeKey = new QPushButton(tr("Delete Keyframe"), keyPage); keyButtons->addWidget(m_keyTime); keyButtons->addWidget(addKey); keyButtons->addWidget(removeKey); keyLayout->addLayout(keyButtons); m_animationPages->addWidget(keyPage);
    animationLayout->addWidget(m_animationPages);
    m_advancedTracks = new QLabel(animationPage); m_advancedTracks->setWordWrap(true); animationLayout->addWidget(m_advancedTracks);
    connect(m_animation, qOverload<int>(&QComboBox::currentIndexChanged), this, &RtWizardMainWindow::updateAnimationPage);
    connect(addKey, &QPushButton::clicked, this, &RtWizardMainWindow::addCameraKeyframe); connect(removeKey, &QPushButton::clicked, this, &RtWizardMainWindow::removeCameraKeyframe);
    m_tabs->addTab(animationPage, tr("Animation"));
    m_settingsDock->setWidget(m_tabs); addDockWidget(Qt::LeftDockWidgetArea, m_settingsDock);
    tabifyDockWidget(m_rolesDock, m_settingsDock);
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    m_rolesDock->raise();

    m_jobDock = new QDockWidget(tr("Render Status"), this); m_jobDock->setObjectName("RenderJobDockV2"); QWidget *job = new QWidget(m_jobDock); QVBoxLayout *jobLayout = new QVBoxLayout(job);
    m_validation = new QLabel(job); m_validation->setWordWrap(true); jobLayout->addWidget(m_validation);
    m_progress = new QProgressBar(job); m_progress->setRange(0, 1); m_progress->setValue(0); jobLayout->addWidget(m_progress);
    m_log = new QPlainTextEdit(job); m_log->setReadOnly(true); m_log->setMaximumBlockCount(2000); m_log->setMaximumHeight(100); jobLayout->addWidget(m_log);
    QHBoxLayout *jobButtons = new QHBoxLayout; m_previewButton = new QPushButton(tr("Preview"), job); m_renderButton = new QPushButton(tr("Render"), job); m_cancelButton = new QPushButton(tr("Cancel"), job); m_cancelButton->setEnabled(false); jobButtons->addWidget(m_previewButton); jobButtons->addWidget(m_renderButton); jobButtons->addWidget(m_cancelButton); jobLayout->addLayout(jobButtons);
    connect(m_previewButton, &QPushButton::clicked, this, &RtWizardMainWindow::renderPreview); connect(m_renderButton, &QPushButton::clicked, this, &RtWizardMainWindow::renderFull); connect(m_cancelButton, &QPushButton::clicked, this, &RtWizardMainWindow::cancelRender);
    m_jobDock->setWidget(job); addDockWidget(Qt::BottomDockWidgetArea, m_jobDock);
    resizeDocks({m_rolesDock}, {390}, Qt::Horizontal);
    resizeDocks({m_jobDock}, {170}, Qt::Vertical);

    connect(m_type, qOverload<int>(&QComboBox::currentIndexChanged), this, &RtWizardMainWindow::updateValidation);
    connect(m_turntableObject, &QLineEdit::textChanged, this, &RtWizardMainWindow::updateValidation);
    connect(m_eye, &QLineEdit::textChanged, this, &RtWizardMainWindow::updateValidation);
    connect(m_orientation, &QLineEdit::textChanged, this, &RtWizardMainWindow::updateValidation);
    connect(m_viewSize, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &RtWizardMainWindow::updateValidation);
    for (QListWidget *list : {m_colorRoles, m_lineRoles, m_ghostRoles}) connect(list, &QListWidget::itemChanged, this, &RtWizardMainWindow::updateValidation);
    resize(1280, 820);
}

void
RtWizardMainWindow::showRolePage(int index)
{
    const int page = std::clamp(index, 0, 2);
    if (m_roleTabs) m_roleTabs->setCurrentIndex(page);
    if (m_rolesDock) {
        const QStringList titles = {tr("Configure the Full-Color Elements"),
            tr("Configure the Line-Drawing Elements"), tr("Configure the Ghost Elements")};
        m_rolesDock->setWindowTitle(titles[page]);
        m_rolesDock->show();
        m_rolesDock->raise();
    }
}

void
RtWizardMainWindow::showSettingsPage(int index)
{
    if (m_tabs) m_tabs->setCurrentIndex(std::clamp(index, 0, m_tabs->count() - 1));
    if (m_settingsDock) {
        m_settingsDock->show();
        m_settingsDock->raise();
    }
}

void
RtWizardMainWindow::attachModelView()
{
    if (!m_model || !m_model->gedp || !m_view || !m_view->view()) return;
    bview *view = m_view->view();
    if (view->vset && view->vset != &m_model->gedp->ged_views)
        bv_set_rm_view(view->vset, view);
    bv_set_add_view(&m_model->gedp->ged_views, view);
    m_model->gedp->ged_gvp = view;
}

void
RtWizardMainWindow::updateSceneView(unsigned long long flags)
{
    if (!m_model || !m_model->gedp || !m_view || !m_view->view()) return;
    attachModelView();
    if ((flags & QG_VIEW_DRAWN) && m_model->gedp->dbi_state) {
        DbiState *databaseState = static_cast<DbiState *>(m_model->gedp->dbi_state);
        BViewState *viewState = databaseState->get_view_state(m_view->view());
        if (viewState) {
            std::unordered_set<bview *> views = {m_view->view()};
            viewState->redraw(nullptr, views, 1);
        }
    }
    if (m_tree) m_tree->do_view_update(flags);
    m_view->need_update(flags);
}

void
RtWizardMainWindow::loadDatabase(const QString &path)
{
    if (path.isEmpty()) return;
    QFileInfo info(path);
    if (!info.exists()) { QMessageBox::critical(this, tr("Open Database"), tr("Database does not exist: %1").arg(path)); return; }
    m_database = info.absoluteFilePath();
    QgTreeView *oldTree = m_tree;
    QgModel *oldModel = m_model;
    if (m_view && m_view->view() && m_view->view()->vset)
        bv_set_rm_view(m_view->view()->vset, m_view->view());
    m_model = new QgModel(this, QFile::encodeName(m_database).constData());
    m_tree = new QgTreeView(m_treeContainer, m_model);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(m_treeContainer->layout());
    QWidget *replaced = oldTree ? static_cast<QWidget *>(oldTree) :
        m_treeContainer->findChild<QWidget *>("TreePlaceholder");
    layout->replaceWidget(replaced, m_tree);
    if (replaced) replaced->deleteLater();
    if (oldModel) oldModel->deleteLater();
    attachModelView();
    connect(m_model, &QgModel::view_change, this, [this](unsigned long long flags) {
        updateSceneView(flags);
        if (flags & QG_VIEW_DRAWN) autoviewScene();
    });
    connect(m_model, &QgModel::opened_item, m_tree, &QgTreeView::qgitem_select_sync);
    connect(m_model, &QgModel::mdl_changed_db, m_tree, &QgTreeView::redo_expansions);
    connect(m_model, &QgModel::check_highlights, m_tree, &QgTreeView::redo_highlights);
    connect(m_tree, &QgTreeView::expanded, m_model, &QgModel::item_expanded);
    connect(m_tree, &QgTreeView::collapsed, m_model, &QgModel::item_collapsed);
    disconnect(m_tree, &QTreeView::doubleClicked, m_tree, &QgTreeView::do_draw_toggle);
    connect(m_tree, &QTreeView::clicked, this, [this](const QModelIndex &) {
        if (m_result) m_result->hide();
        const QString objectPath = selectedObjectPath();
        if (m_model && !objectPath.isEmpty())
            m_model->draw(QFile::encodeName(objectPath).constData());
    });
    connect(m_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex &) {
        updateSceneView(QG_VIEW_DRAWN);
        autoviewScene();
    });
    setWindowTitle(tr("BRL-CAD RtWizard — %1").arg(info.fileName()));
    QSettings settings("BRL-CAD", "RtWizard"); settings.setValue("lastDatabaseDirectory", info.absolutePath());
    refreshScene(); updateValidation();
}

void
RtWizardMainWindow::applySettings(const rtwizard_settings *settings, char pictureType)
{
    if (!settings) return;
    const QString database = QString::fromLocal8Bit(bu_vls_addr(settings->input_file));
    if (!database.isEmpty() && QFileInfo(database).absoluteFilePath() != m_database)
        loadDatabase(database);

    setRoleValues(m_colorRoles, table_strings(settings->color));
    setRoleValues(m_lineRoles, table_strings(settings->line));
    setRoleValues(m_ghostRoles, table_strings(settings->ghost));
    if (pictureType >= 'A' && pictureType <= 'F') {
        m_type->setCurrentIndex(pictureType - 'A');
    } else if (BU_PTBL_LEN(settings->ghost) && BU_PTBL_LEN(settings->line) && BU_PTBL_LEN(settings->color)) {
        m_type->setCurrentIndex(5);
    } else if (BU_PTBL_LEN(settings->ghost) && BU_PTBL_LEN(settings->color)) {
        m_type->setCurrentIndex(4);
    } else if (BU_PTBL_LEN(settings->line) && BU_PTBL_LEN(settings->color)) {
        m_type->setCurrentIndex(3);
    } else if (BU_PTBL_LEN(settings->line)) {
        m_type->setCurrentIndex(1);
    }

    m_width->setValue(static_cast<int>(settings->width));
    m_height->setValue(static_cast<int>(settings->height));
    m_background->setText(color_text(settings->bkg_color));
    m_lineColor->setText(color_text(settings->line_color));
    m_nonLineColor->setText(color_text(settings->non_line_color));
    m_ghostIntensity->setValue(settings->ghost_intensity);
    m_occlusion->setValue(settings->occlusion);
    m_aoSamples->setValue(settings->ao_samples);
    m_aoRadius->setValue(settings->ao_radius);
    if (settings->az < DBL_MAX) m_azimuth->setValue(settings->az);
    if (settings->el < DBL_MAX) m_elevation->setValue(settings->el);
    if (settings->tw < DBL_MAX) m_twist->setValue(settings->tw);
    if (settings->zoom < DBL_MAX) m_zoom->setValue(settings->zoom);
    if (settings->perspective < DBL_MAX) m_perspective->setValue(settings->perspective);
    if (settings->center[0] < DBL_MAX)
        m_center->setText(QStringLiteral("%1 %2 %3").arg(settings->center[0], 0, 'g', 15).arg(settings->center[1], 0, 'g', 15).arg(settings->center[2], 0, 'g', 15));
    if (settings->viewsize < DBL_MAX && settings->orientation[0] < DBL_MAX && settings->eye_pt[0] < DBL_MAX) {
        m_viewMode->setCurrentIndex(1);
        m_viewSize->setValue(settings->viewsize);
        m_eye->setText(QStringLiteral("%1 %2 %3").arg(settings->eye_pt[0], 0, 'g', 15).arg(settings->eye_pt[1], 0, 'g', 15).arg(settings->eye_pt[2], 0, 'g', 15));
        m_orientation->setText(QStringLiteral("%1 %2 %3 %4").arg(settings->orientation[0], 0, 'g', 15).arg(settings->orientation[1], 0, 'g', 15).arg(settings->orientation[2], 0, 'g', 15).arg(settings->orientation[3], 0, 'g', 15));
    }
    m_outputPath->setText(QString::fromLocal8Bit(bu_vls_addr(settings->output_file)));
    m_frameDir->setText(QString::fromLocal8Bit(bu_vls_addr(settings->frame_dir)));
    m_fbDevice->setText(QString::fromLocal8Bit(bu_vls_addr(settings->fb_dev)));
    m_fbPort->setValue(settings->port);
    m_fbTransport->setCurrentIndex(std::clamp(settings->fb_transport, 0, 2));

    if (settings->animation_duration > 0.0) m_duration->setValue(settings->animation_duration);
    if (settings->animation_fps > 0) m_fps->setValue(settings->animation_fps);
    if (settings->animation_frames > 0) m_frames->setValue(settings->animation_frames);
    const QString preset = QString::fromLocal8Bit(bu_vls_addr(settings->animation_preset));
    if (preset == "cut") m_animation->setCurrentIndex(1);
    else if (preset == "orbit") m_animation->setCurrentIndex(2);
    else if (preset == "turntable") m_animation->setCurrentIndex(3);
    else if (bu_vls_strlen(settings->animation_file)) m_animation->setCurrentIndex(4);
    if (settings->cut_direction_set)
        m_cutDirection->setText(QStringLiteral("%1 %2 %3").arg(settings->cut_direction[0], 0, 'g', 15).arg(settings->cut_direction[1], 0, 'g', 15).arg(settings->cut_direction[2], 0, 'g', 15));
    m_orbitAngle->setValue(settings->orbit_angle);
    m_orbitAxis->setText(QStringLiteral("%1 %2 %3").arg(settings->orbit_axis[0], 0, 'g', 15).arg(settings->orbit_axis[1], 0, 'g', 15).arg(settings->orbit_axis[2], 0, 'g', 15));
    if (settings->orbit_center[0] < DBL_MAX)
        m_orbitCenter->setText(QStringLiteral("%1 %2 %3").arg(settings->orbit_center[0], 0, 'g', 15).arg(settings->orbit_center[1], 0, 'g', 15).arg(settings->orbit_center[2], 0, 'g', 15));
    if (settings->orbit_elevation < DBL_MAX) m_orbitElevation->setValue(settings->orbit_elevation);
    if (settings->orbit_radius < DBL_MAX) m_orbitRadius->setValue(settings->orbit_radius);
    m_turntableObject->setText(QString::fromLocal8Bit(bu_vls_addr(settings->turntable_object)));
    m_turntableAngle->setValue(settings->turntable_angle);
    m_turntableAxis->setText(QStringLiteral("%1 %2 %3").arg(settings->turntable_axis[0], 0, 'g', 15).arg(settings->turntable_axis[1], 0, 'g', 15).arg(settings->turntable_axis[2], 0, 'g', 15));
    if (settings->turntable_center[0] < DBL_MAX)
        m_turntableCenter->setText(QStringLiteral("%1 %2 %3").arg(settings->turntable_center[0], 0, 'g', 15).arg(settings->turntable_center[1], 0, 'g', 15).arg(settings->turntable_center[2], 0, 'g', 15));
    syncTimeline();
    refreshScene();
    updateValidation();
}

QString
RtWizardMainWindow::selectedObjectPath() const
{
    if (!m_tree || !m_model) return QString();
    QgItem *item = m_model->getItem(m_tree->currentIndex());
    QStringList names;
    while (item && item != m_model->root()) {
        QString name = QString::fromUtf8(bu_vls_addr(&item->name));
        if (!name.isEmpty()) names.prepend(name);
        item = item->parent();
    }
    return names.join('/');
}

void RtWizardMainWindow::addRole(QListWidget *list)
{
    QString path = selectedObjectPath();
    if (path.isEmpty()) return;
    if (list->findItems(path, Qt::MatchExactly).isEmpty()) list->addItem(path);
    refreshScene(); updateValidation();
}
void RtWizardMainWindow::addColorRole() { addRole(m_colorRoles); }
void RtWizardMainWindow::addLineRole() { addRole(m_lineRoles); }
void RtWizardMainWindow::addGhostRole() { addRole(m_ghostRoles); }

void
RtWizardMainWindow::removeRole()
{
    for (QListWidget *list : {m_colorRoles, m_lineRoles, m_ghostRoles}) {
        if (!list->property("activeRole").toBool()) continue;
        delete list->takeItem(list->currentRow());
    }
    refreshScene(); updateValidation();
}

QStringList RtWizardMainWindow::roleValues(const QListWidget *list) const
{
    QStringList values; for (int i = 0; i < list->count(); ++i) values << list->item(i)->text(); return values;
}
void RtWizardMainWindow::setRoleValues(QListWidget *list, const QStringList &values) { list->clear(); list->addItems(values); }

void
RtWizardMainWindow::refreshScene()
{
    if (!m_model || !m_model->gedp || m_database.isEmpty()) return;
    m_result->hide();
    attachModelView();
    bu_vls message = BU_VLS_INIT_ZERO;
    if (m_model->gedp->dbi_state) {
        DbiState *databaseState = static_cast<DbiState *>(m_model->gedp->dbi_state);
        BViewState *viewState = databaseState->get_view_state(m_view->view());
        if (viewState) viewState->clear();
    }
    const char *zap[] = {"zap"}; m_model->run_cmd(&message, 1, zap);
    std::set<QString> objects;
    for (QListWidget *list : {m_colorRoles, m_lineRoles, m_ghostRoles}) for (const QString &value : roleValues(list)) objects.insert(value);
    if (!objects.empty()) {
        std::vector<QByteArray> encoded; std::vector<const char *> argv; encoded.push_back("draw");
        for (const QString &object : objects) encoded.push_back(QFile::encodeName(object));
        for (const QByteArray &value : encoded) argv.push_back(value.constData());
        m_model->run_cmd(&message, static_cast<int>(argv.size()), argv.data());
        updateSceneView(QG_VIEW_DRAWN);
        const char *autoview[] = {"autoview"}; m_model->run_cmd(&message, 1, autoview);
    } else {
        updateSceneView(QG_VIEW_DRAWN);
    }
    bu_vls_free(&message);
    updateSceneView(QG_VIEW_REFRESH);
}

void
RtWizardMainWindow::autoviewScene()
{
    if (!m_model || !m_model->gedp) return;
    m_result->hide();
    attachModelView();
    bu_vls message = BU_VLS_INIT_ZERO;
    const char *autoview[] = {"autoview"};
    m_model->run_cmd(&message, 1, autoview);
    bu_vls_free(&message);
    updateSceneView(QG_VIEW_REFRESH);
}

void
RtWizardMainWindow::clearScene()
{
    if (!m_model || !m_model->gedp) return;
    m_result->hide();
    attachModelView();
    bu_vls message = BU_VLS_INIT_ZERO;
    if (m_model->gedp->dbi_state) {
        DbiState *databaseState = static_cast<DbiState *>(m_model->gedp->dbi_state);
        BViewState *viewState = databaseState->get_view_state(m_view->view());
        if (viewState) viewState->clear();
    }
    const char *zap[] = {"zap"};
    m_model->run_cmd(&message, 1, zap);
    bu_vls_free(&message);
    updateSceneView(QG_VIEW_DRAWN);
}

void
RtWizardMainWindow::updateValidation()
{
    QStringList missing;
    const int type = m_type->currentIndex();
    if (m_database.isEmpty()) missing << tr("database");
    if ((type == 0 || type == 2 || type == 3 || type == 4 || type == 5) && m_colorRoles->count() == 0) missing << tr("color objects");
    if ((type == 1 || type == 2 || type == 3 || type == 5) && m_lineRoles->count() == 0) missing << tr("line objects");
    if ((type == 4 || type == 5) && m_ghostRoles->count() == 0) missing << tr("ghost objects");
    if (m_animation->currentIndex() == 3 && m_turntableObject->text().trimmed().isEmpty()) missing << tr("turntable object");
    if (m_viewMode->currentIndex() == 1) {
        try {
            (void)vector_value(m_eye->text(), 3);
            (void)vector_value(m_orientation->text(), 4);
            if (!(m_viewSize->value() > 0.0)) missing << tr("positive exact view size");
        } catch (...) {
            missing << tr("complete exact view");
        }
    }
    const bool valid = missing.isEmpty();
    m_validation->setText(valid ? tr("Ready to render.") : tr("Required: %1.").arg(missing.join(", ")));
    m_renderButton->setEnabled(valid && !m_renderThread); m_previewButton->setEnabled(valid && !m_renderThread);
}

void RtWizardMainWindow::updateAnimationPage() { m_animationPages->setCurrentIndex(m_animation->currentIndex()); updateValidation(); }

bool
RtWizardMainWindow::loadSpecification(const QString &path)
{
    try {
        std::ifstream input(QFile::encodeName(path).constData()); json root; input >> root;
        if (root.value("schema", "") != "brlcad.rtwizard.render" || root.value("version", 0) != 1) throw std::runtime_error("unsupported render specification schema or version");
        m_document = root.dump(2); m_specPath = QFileInfo(path).absoluteFilePath();
        fs::path base = fs::path(QFile::encodeName(m_specPath).constData()).parent_path();
        if (root.contains("database")) { fs::path db = root["database"].get<std::string>(); if (db.is_relative()) db = base / db; loadDatabase(QString::fromStdString(db.lexically_normal().string())); }
        if (root.contains("objects")) { setRoleValues(m_colorRoles, json_strings(root["objects"].value("color", json::array()))); setRoleValues(m_lineRoles, json_strings(root["objects"].value("line", json::array()))); setRoleValues(m_ghostRoles, json_strings(root["objects"].value("ghost", json::array()))); }
        const json image = root.value("image", json::object()); const std::string pictureType = image.value("type", std::string("A")); m_type->setCurrentIndex(pictureType.empty() ? 0 : std::clamp(static_cast<int>(pictureType[0] - 'A'), 0, 5)); int size = image.value("size", 512); m_width->setValue(image.value("width", size)); m_height->setValue(image.value("height", size));
        const json style = root.value("style", json::object()); m_background->setText(vector_text(style.value("background", json::array()), "255 255 255")); m_lineColor->setText(vector_text(style.value("line_color", json::array()), "0 0 0")); m_nonLineColor->setText(vector_text(style.value("non_line_color", json::array()), "0 0 0")); m_ghostIntensity->setValue(style.value("ghost_intensity", 6.0)); m_occlusion->setValue(style.value("occlusion", 1)); m_aoSamples->setValue(style.value("ao_samples", 0)); m_aoRadius->setValue(style.value("ao_radius", 0.0));
        const json view = root.value("view", json::object()); const bool exactView = view.contains("eye") && view.contains("orientation") && view.contains("view_size"); m_viewMode->setCurrentIndex(exactView ? 1 : 0); m_azimuth->setValue(view.value("azimuth", 35.0)); m_elevation->setValue(view.value("elevation", 25.0)); m_twist->setValue(view.value("twist", 0.0)); m_zoom->setValue(view.value("zoom", 1.0)); m_perspective->setValue(view.value("perspective", 0.0)); m_center->setText(vector_text(view.value("center", json::array()), QString())); m_eye->setText(vector_text(view.value("eye", json::array()), QString())); m_orientation->setText(vector_text(view.value("orientation", json::array()), QString())); m_viewSize->setValue(view.value("view_size", 1.0));
        const json output = root.value("output", json::object()); if (output.contains("file")) { fs::path p = output["file"].get<std::string>(); if (p.is_relative()) p = base / p; m_outputPath->setText(QString::fromStdString(p.lexically_normal().string())); } if (output.contains("frame_dir")) m_frameDir->setText(QString::fromStdString(output["frame_dir"].get<std::string>())); m_fbDevice->setText(QString::fromStdString(output.value("framebuffer", std::string()))); m_fbPort->setValue(output.value("framebuffer_port", -1)); const std::string transport = output.value("framebuffer_transport", std::string("auto")); m_fbTransport->setCurrentIndex(transport == "ipc" ? 1 : transport == "tcp" ? 2 : 0);
        m_animation->setCurrentIndex(0); m_advancedTracks->clear();
        if (root.contains("animation")) { const json &a = root["animation"]; const json timing = a.value("timing", json::object()); m_duration->setValue(timing.value("duration", 5.0)); m_fps->setValue(timing.value("fps", 10)); m_frames->setValue(timing.value("frames", 0)); if (a.contains("preset")) { std::string preset = a["preset"].get<std::string>(); m_animation->setCurrentIndex(preset == "cut" ? 1 : preset == "orbit" ? 2 : 3); const json options = a.value("options", json::object()); m_cutDirection->setText(vector_text(options.value("cut_direction", json::array()), "0 0 1")); m_orbitAngle->setValue(options.value("orbit_angle", 360.0)); m_orbitAxis->setText(vector_text(options.value("orbit_axis", json::array()), "0 0 1")); m_orbitCenter->setText(vector_text(options.value("orbit_center", json::array()), QString())); m_orbitElevation->setValue(options.value("orbit_elevation", 0.0)); m_orbitRadius->setValue(options.value("orbit_radius", 0.0)); m_turntableObject->setText(QString::fromStdString(options.value("turntable_object", std::string()))); m_turntableAngle->setValue(options.value("turntable_angle", 360.0)); m_turntableAxis->setText(vector_text(options.value("turntable_axis", json::array()), "0 0 1")); m_turntableCenter->setText(vector_text(options.value("turntable_center", json::array()), QString())); } else if (a.contains("tracks")) { m_animation->setCurrentIndex(4); int advanced = 0; for (const json &track : a["tracks"]) if (track.value("type", "") != "camera") ++advanced; if (advanced) m_advancedTracks->setText(tr("%1 advanced object/material track(s) loaded; they will be preserved when saving.").arg(advanced)); } }
        syncTimeline(); refreshScene(); updateValidation(); return true;
    } catch (const std::exception &error) { QMessageBox::critical(this, tr("Open Render Specification"), QString::fromLocal8Bit(error.what())); return false; }
}

bool
RtWizardMainWindow::writeSpecification(const QString &path, bool preview)
{
    try {
        json root = parse_document(m_document); root["schema"] = "brlcad.rtwizard.render"; root["version"] = 1; root["database"] = m_database.toStdString();
        root["objects"] = {{"color", string_array(roleValues(m_colorRoles))}, {"line", string_array(roleValues(m_lineRoles))}, {"ghost", string_array(roleValues(m_ghostRoles))}};
        root["image"] = {{"width", preview ? std::min(256, m_width->value()) : m_width->value()}, {"height", preview ? std::min(256, m_height->value()) : m_height->value()}, {"type", std::string(1, static_cast<char>('A' + m_type->currentIndex()))}};
        root["style"] = {{"background", vector_value(m_background->text(), 3)}, {"line_color", vector_value(m_lineColor->text(), 3)}, {"non_line_color", vector_value(m_nonLineColor->text(), 3)}, {"ghost_intensity", m_ghostIntensity->value()}, {"occlusion", m_occlusion->value()}, {"ao_samples", m_aoSamples->value()}, {"ao_radius", m_aoRadius->value()}};
        if (m_viewMode->currentIndex() == 1) {
            root["view"] = {{"eye", vector_value(m_eye->text(), 3)}, {"orientation", vector_value(m_orientation->text(), 4)}, {"view_size", m_viewSize->value()}, {"perspective", m_perspective->value()}};
        } else {
            root["view"] = {{"azimuth", m_azimuth->value()}, {"elevation", m_elevation->value()}, {"twist", m_twist->value()}, {"zoom", m_zoom->value()}, {"perspective", m_perspective->value()}};
            if (!m_center->text().trimmed().isEmpty()) root["view"]["center"] = vector_value(m_center->text(), 3);
        }
        QString output = preview ? m_previewOutput : m_outputPath->text().trimmed(); root["output"]["file"] = output.toStdString(); if (!m_frameDir->text().trimmed().isEmpty() && !preview) root["output"]["frame_dir"] = m_frameDir->text().trimmed().toStdString(); else root["output"].erase("frame_dir"); if (!m_fbDevice->text().trimmed().isEmpty()) root["output"]["framebuffer"] = m_fbDevice->text().trimmed().toStdString(); else root["output"].erase("framebuffer"); if (m_fbPort->value() >= 0) root["output"]["framebuffer_port"] = m_fbPort->value(); else root["output"].erase("framebuffer_port"); root["output"]["framebuffer_transport"] = m_fbTransport->currentIndex() == 1 ? "ipc" : m_fbTransport->currentIndex() == 2 ? "tcp" : "auto"; root["runtime"]["no_gui"] = true; root["runtime"].erase("gui");
        const int animation = m_animation->currentIndex();
        if (animation == 0 || preview) root.erase("animation");
        else { json &a = root["animation"]; a["timing"] = {{"duration", m_duration->value()}, {"fps", m_fps->value()}}; if (m_frames->value() > 0) a["timing"]["frames"] = m_frames->value(); else a["timing"].erase("frames"); if (animation >= 1 && animation <= 3) { a.erase("tracks"); a["preset"] = animation == 1 ? "cut" : animation == 2 ? "orbit" : "turntable"; json options = json::object(); if (animation == 1) options["cut_direction"] = vector_value(m_cutDirection->text(), 3); if (animation == 2) { options["orbit_angle"] = m_orbitAngle->value(); options["orbit_axis"] = vector_value(m_orbitAxis->text(), 3); if (!m_orbitCenter->text().trimmed().isEmpty()) options["orbit_center"] = vector_value(m_orbitCenter->text(), 3); options["orbit_elevation"] = m_orbitElevation->value(); if (m_orbitRadius->value() > 0.0) options["orbit_radius"] = m_orbitRadius->value(); } if (animation == 3) { options["turntable_object"] = m_turntableObject->text().trimmed().toStdString(); options["turntable_angle"] = m_turntableAngle->value(); options["turntable_axis"] = vector_value(m_turntableAxis->text(), 3); if (!m_turntableCenter->text().trimmed().isEmpty()) options["turntable_center"] = vector_value(m_turntableCenter->text(), 3); } a["options"] = options; } else { a.erase("preset"); a.erase("options"); if (!a.contains("tracks")) a["tracks"] = json::array(); } }
        std::ofstream file(QFile::encodeName(path).constData(), std::ios::trunc); file << root.dump(2) << '\n'; if (!file) throw std::runtime_error("unable to write render specification"); if (!preview) m_document = root.dump(2); return true;
    } catch (const std::exception &error) { QMessageBox::critical(this, tr("Save Render Specification"), QString::fromLocal8Bit(error.what())); return false; }
}

void RtWizardMainWindow::openDatabase() { QSettings s("BRL-CAD", "RtWizard"); QString p = QFileDialog::getOpenFileName(this, tr("Open BRL-CAD Database"), s.value("lastDatabaseDirectory").toString(), tr("BRL-CAD databases (*.g);;All files (*)")); if (!p.isEmpty()) loadDatabase(p); }
void RtWizardMainWindow::openSpecification() { QString p = QFileDialog::getOpenFileName(this, tr("Open Render Specification"), QFileInfo(m_specPath).absolutePath(), tr("JSON files (*.json);;All files (*)")); if (!p.isEmpty()) loadSpecification(p); }
void RtWizardMainWindow::saveSpecification() { if (m_specPath.isEmpty()) saveSpecificationAs(); else writeSpecification(m_specPath, false); }
void RtWizardMainWindow::saveSpecificationAs() { QString p = QFileDialog::getSaveFileName(this, tr("Save Render Specification"), m_specPath.isEmpty() ? "rtwizard.json" : m_specPath, tr("JSON files (*.json)")); if (!p.isEmpty() && writeSpecification(p, false)) m_specPath = p; }
void RtWizardMainWindow::chooseOutput() { QString p = QFileDialog::getSaveFileName(this, tr("Select Render Output"), m_outputPath->text(), tr("Images (*.png *.pix *.ppm *.bw *.dpix);;Animations (*.apng *.mjpg *.avi);;All files (*)")); if (!p.isEmpty()) m_outputPath->setText(p); }

void
RtWizardMainWindow::addCameraKeyframe()
{
    try {
        json root = parse_document(m_document); json &a = root["animation"]; a.erase("preset"); a.erase("options"); a["timing"]["duration"] = std::max(m_duration->value(), m_keyTime->value()); a["timing"]["fps"] = m_fps->value(); double unitToBase = 1.0; if (!a.contains("units")) a["units"] = "mm"; const std::string units = a.value("units", std::string("mm")); if (units == "database") { if (!m_model || !m_model->gedp || !m_model->gedp->dbip) throw std::runtime_error("database units are unavailable"); unitToBase = m_model->gedp->dbip->dbi_local2base; } else { unitToBase = bu_units_conversion(units.c_str()); if (!(unitToBase > 0.0)) throw std::runtime_error("unknown animation distance unit"); } json &tracks = a["tracks"]; if (!tracks.is_array()) tracks = json::array(); json *camera = nullptr; for (json &track : tracks) if (track.value("type", "") == "camera") { camera = &track; break; } if (!camera) { tracks.push_back({{"type", "camera"}, {"interpolation", "smooth"}, {"keyframes", json::array()}}); camera = &tracks.back(); }
        bview *view = m_view->view(); if (!view) throw std::runtime_error("CAD view is not initialized"); quat_t orientation; quat_mat2quat(orientation, view->gv_rotation); point_t eye = VINIT_ZERO; point_t viewEye = {0, 0, 1}; MAT4X3PNT(eye, view->gv_view2model, viewEye);
        json key = {{"time", m_keyTime->value()}, {"eye", {eye[0] / unitToBase, eye[1] / unitToBase, eye[2] / unitToBase}}, {"orientation", {orientation[0], orientation[1], orientation[2], orientation[3]}}, {"view_size", view->gv_size / unitToBase}, {"perspective", view->gv_perspective}};
        json &keys = (*camera)["keyframes"]; bool replaced = false; for (json &existing : keys) if (std::fabs(existing.value("time", -1.0) - m_keyTime->value()) < 1.0e-12) { existing = key; replaced = true; break; } if (!replaced) keys.push_back(key); std::sort(keys.begin(), keys.end(), [](const json &l, const json &r) { return l.value("time", 0.0) < r.value("time", 0.0); }); m_document = root.dump(2); m_animation->setCurrentIndex(4); syncTimeline();
    } catch (const std::exception &error) { QMessageBox::critical(this, tr("Camera Keyframe"), QString::fromLocal8Bit(error.what())); }
}

void
RtWizardMainWindow::removeCameraKeyframe()
{
    int row = m_timeline->currentRow(); if (row < 0) return;
    json root = parse_document(m_document); if (!root.contains("animation")) return; for (json &track : root["animation"]["tracks"]) if (track.value("type", "") == "camera") { json &keys = track["keyframes"]; if (row < static_cast<int>(keys.size())) keys.erase(keys.begin() + row); break; } m_document = root.dump(2); syncTimeline();
}

void
RtWizardMainWindow::syncTimeline()
{
    m_timeline->setRowCount(0); try { json root = parse_document(m_document); if (!root.contains("animation") || !root["animation"].contains("tracks")) return; for (const json &track : root["animation"]["tracks"]) { if (track.value("type", "") != "camera") continue; for (const json &key : track["keyframes"]) { int row = m_timeline->rowCount(); m_timeline->insertRow(row); m_timeline->setItem(row, 0, new QTableWidgetItem(QString::number(key.value("time", 0.0)))); m_timeline->setItem(row, 1, new QTableWidgetItem(vector_text(key.value("eye", json::array()), QString()))); m_timeline->setItem(row, 2, new QTableWidgetItem(vector_text(key.value("orientation", json::array()), QString()))); m_timeline->setItem(row, 3, new QTableWidgetItem(QString::number(key.value("view_size", 0.0)))); } break; } } catch (...) {}
}

void RtWizardMainWindow::renderPreview() { startRender(true); }
void RtWizardMainWindow::renderFull() { startRender(false); }

void
RtWizardMainWindow::startRender(bool preview)
{
    if (m_renderThread)
        return;
    updateValidation();
    if (!m_renderButton->isEnabled() && !m_previewButton->isEnabled())
        return;
    if (!preview && m_outputPath->text().trimmed().isEmpty()) { chooseOutput(); if (m_outputPath->text().trimmed().isEmpty()) return; }
    QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation); QDir().mkpath(cache); m_previewOutput = QDir(cache).filePath("rtwizard-preview.png"); m_activeOutput = preview ? m_previewOutput : m_outputPath->text().trimmed();
    if (preview) QFile::remove(m_previewOutput);
    QString spec = QDir(cache).filePath("rtwizard-job.json"); if (!writeSpecification(spec, preview)) return;
    m_log->clear(); m_result->hide(); m_progress->setRange(0, 0); m_previewButton->setEnabled(false); m_renderButton->setEnabled(false); m_cancelButton->setEnabled(true);
    QStringList arguments = {m_program, "--no-gui", "--render-spec", spec, "--verbose", "1"};
    m_renderThread = new QThread(this);
    QThread *thread = m_renderThread;
    m_worker = new RtWizardRenderWorker(arguments);
    m_worker->moveToThread(thread);
    connect(thread, &QThread::started, m_worker, &RtWizardRenderWorker::run);
    connect(m_worker, &RtWizardRenderWorker::output, m_log, &QPlainTextEdit::appendPlainText);
    connect(m_worker, &RtWizardRenderWorker::finished, this, &RtWizardMainWindow::renderFinished);
    connect(m_worker, &RtWizardRenderWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_renderThread == thread) {
            m_renderThread = nullptr;
            m_worker = nullptr;
            updateValidation();
        }
        thread->deleteLater();
    });
    thread->start();
}

void RtWizardMainWindow::cancelRender() { if (m_worker) m_worker->requestCancel(); }

void
RtWizardMainWindow::renderFinished(int status, const QString &message)
{
    QString completion = message;
    if (status == 0 && !QFileInfo::exists(m_activeOutput)) {
        status = 1;
        completion = tr("Render failed: the expected output file was not created.");
    }
    m_log->appendPlainText(completion); m_progress->setRange(0, 1); m_progress->setValue(status == 0 ? 1 : 0); m_cancelButton->setEnabled(false);
    if (status == 0 && QFileInfo(m_activeOutput).suffix().compare("png", Qt::CaseInsensitive) == 0) { QPixmap pixmap(m_activeOutput); if (!pixmap.isNull()) { m_result->setPixmap(pixmap.scaled(m_result->size().isEmpty() ? QSize(640, 480) : m_result->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)); m_result->show(); m_result->raise(); } }
    statusBar()->showMessage(completion, 5000);
}

void
RtWizardMainWindow::importLegacySettings()
{
    QSettings settings("BRL-CAD", "RtWizard"); if (settings.value("legacyImportComplete", false).toBool()) return;
    QString path = qEnvironmentVariable("RTWIZARD_RCFILE"); if (path.isEmpty()) path = QDir::home().filePath(".rtwizardrc"); if (!QFileInfo::exists(path)) path = QDir::current().filePath(".rtwizardrc");
    QFile file(path); int width = 0, height = 0; QList<int> panes; if (file.open(QIODevice::ReadOnly | QIODevice::Text)) { while (!file.atEnd()) { QString line = QString::fromUtf8(file.readLine()).trimmed(); QRegularExpressionMatch match; match = QRegularExpression("^set\\s+::wizard_width\\s+([0-9]+)$").match(line); if (match.hasMatch()) width = match.captured(1).toInt(); match = QRegularExpression("^set\\s+::wizard_height\\s+([0-9]+)$").match(line); if (match.hasMatch()) height = match.captured(1).toInt(); match = QRegularExpression("^set\\s+::gpane\\s+\\{?([0-9]+)\\s+([0-9]+)\\}?$").match(line); if (match.hasMatch()) panes = {match.captured(1).toInt(), match.captured(2).toInt()}; } }
    if (width > 0 && height > 0)
        settings.setValue("legacySize", QSize(width, height));
    if (panes.size() == 2)
        settings.setValue("legacyPane", QVariant::fromValue(panes));
    settings.setValue("legacyImportComplete", true);
}

void
RtWizardMainWindow::loadSettings()
{
    QSettings settings("BRL-CAD", "RtWizard"); if (settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray()); else if (settings.contains("legacySize")) resize(settings.value("legacySize").toSize()); if (settings.value("layoutVersion", 0).toInt() == 2 && settings.contains("windowState")) restoreState(settings.value("windowState").toByteArray(), 2);
}
void RtWizardMainWindow::saveSettings() { QSettings settings("BRL-CAD", "RtWizard"); settings.setValue("geometry", saveGeometry()); settings.setValue("windowState", saveState(2)); settings.setValue("layoutVersion", 2); settings.setValue("previewSize", 256); }
void RtWizardMainWindow::closeEvent(QCloseEvent *event) { saveSettings(); QMainWindow::closeEvent(event); }

extern "C" int
rtwizard_gui(const char *program, const rtwizard_settings *settings, char picture_type)
{
    int argc = 1;
    QByteArray programName = QFile::encodeName(program && program[0] ? QString::fromLocal8Bit(program) : QStringLiteral("rtwizard"));
    char *argv[] = {programName.data(), nullptr};
    QApplication application(argc, argv);
    application.setOrganizationName("BRL-CAD"); application.setOrganizationDomain("brlcad.org"); application.setApplicationName("RtWizard");
    RtWizardMainWindow window(QString::fromLocal8Bit(program ? program : "rtwizard"),
        settings, picture_type);
    window.show();
    return application.exec();
}
