/*                           G U I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_GUI_H
#define RTWIZARD_GUI_H

#include "common.h"

struct rtwizard_settings;

__BEGIN_DECLS
int rtwizard_gui(const char *program, const struct rtwizard_settings *settings,
    char picture_type);
__END_DECLS

#ifdef __cplusplus

#include <atomic>
#include <string>

#include <QMainWindow>
#include <QObject>
#include <QStringList>

class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTableWidget;
class QThread;
class QWidget;
class QgModel;
class QgTreeView;
class QgView;

class RtWizardRenderWorker : public QObject
{
    Q_OBJECT

public:
    explicit RtWizardRenderWorker(const QStringList &arguments);
    void requestCancel();

public slots:
    void run();

signals:
    void output(const QString &text);
    void finished(int status, const QString &message);

private:
    QStringList m_arguments;
    std::atomic_bool m_cancelled{false};
};

class RtWizardMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    RtWizardMainWindow(const QString &program, const rtwizard_settings *settings,
        char pictureType);
    ~RtWizardMainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openDatabase();
    void openSpecification();
    void saveSpecification();
    void saveSpecificationAs();
    void chooseOutput();
    void addColorRole();
    void addLineRole();
    void addGhostRole();
    void removeRole();
    void autoviewScene();
    void clearScene();
    void refreshScene();
    void updateSceneView(unsigned long long flags);
    void updateValidation();
    void updateAnimationPage();
    void addCameraKeyframe();
    void removeCameraKeyframe();
    void renderPreview();
    void renderFull();
    void cancelRender();
    void renderFinished(int status, const QString &message);

private:
    void createActions();
    void createWorkspace();
    void loadDatabase(const QString &path);
    void applySettings(const rtwizard_settings *settings, char pictureType);
    bool loadSpecification(const QString &path);
    bool writeSpecification(const QString &path, bool preview);
    QString selectedObjectPath() const;
    void addRole(QListWidget *list);
    QStringList roleValues(const QListWidget *list) const;
    void setRoleValues(QListWidget *list, const QStringList &values);
    void attachModelView();
    void showRolePage(int index);
    void showSettingsPage(int index);
    void startRender(bool preview);
    void loadSettings();
    void saveSettings();
    void importLegacySettings();
    void syncTimeline();

    QString m_program;
    QString m_database;
    QString m_specPath;
    QString m_previewOutput;
    QString m_activeOutput;
    std::string m_document;

    QgModel *m_model = nullptr;
    QgTreeView *m_tree = nullptr;
    QgView *m_view = nullptr;
    QWidget *m_treeContainer = nullptr;
    QDockWidget *m_rolesDock = nullptr;
    QDockWidget *m_settingsDock = nullptr;
    QDockWidget *m_jobDock = nullptr;
    QMenu *m_imageMenu = nullptr;
    QMenu *m_stepsMenu = nullptr;
    QLabel *m_result = nullptr;
    QListWidget *m_colorRoles = nullptr;
    QListWidget *m_lineRoles = nullptr;
    QListWidget *m_ghostRoles = nullptr;
    QTabWidget *m_roleTabs = nullptr;
    QTabWidget *m_tabs = nullptr;
    QComboBox *m_type = nullptr;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QLineEdit *m_background = nullptr;
    QLineEdit *m_lineColor = nullptr;
    QLineEdit *m_nonLineColor = nullptr;
    QDoubleSpinBox *m_ghostIntensity = nullptr;
    QSpinBox *m_occlusion = nullptr;
    QSpinBox *m_aoSamples = nullptr;
    QDoubleSpinBox *m_aoRadius = nullptr;
    QComboBox *m_viewMode = nullptr;
    QDoubleSpinBox *m_azimuth = nullptr;
    QDoubleSpinBox *m_elevation = nullptr;
    QDoubleSpinBox *m_twist = nullptr;
    QDoubleSpinBox *m_zoom = nullptr;
    QDoubleSpinBox *m_perspective = nullptr;
    QLineEdit *m_center = nullptr;
    QLineEdit *m_eye = nullptr;
    QLineEdit *m_orientation = nullptr;
    QDoubleSpinBox *m_viewSize = nullptr;
    QLineEdit *m_outputPath = nullptr;
    QLineEdit *m_frameDir = nullptr;
    QLineEdit *m_fbDevice = nullptr;
    QSpinBox *m_fbPort = nullptr;
    QComboBox *m_fbTransport = nullptr;
    QComboBox *m_animation = nullptr;
    QStackedWidget *m_animationPages = nullptr;
    QDoubleSpinBox *m_duration = nullptr;
    QSpinBox *m_fps = nullptr;
    QSpinBox *m_frames = nullptr;
    QLineEdit *m_cutDirection = nullptr;
    QDoubleSpinBox *m_orbitAngle = nullptr;
    QLineEdit *m_orbitAxis = nullptr;
    QLineEdit *m_orbitCenter = nullptr;
    QDoubleSpinBox *m_orbitElevation = nullptr;
    QDoubleSpinBox *m_orbitRadius = nullptr;
    QLineEdit *m_turntableObject = nullptr;
    QDoubleSpinBox *m_turntableAngle = nullptr;
    QLineEdit *m_turntableAxis = nullptr;
    QLineEdit *m_turntableCenter = nullptr;
    QTableWidget *m_timeline = nullptr;
    QDoubleSpinBox *m_keyTime = nullptr;
    QLabel *m_advancedTracks = nullptr;
    QLabel *m_validation = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_previewButton = nullptr;
    QPushButton *m_renderButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QThread *m_renderThread = nullptr;
    RtWizardRenderWorker *m_worker = nullptr;
};

#endif /* __cplusplus */
#endif /* RTWIZARD_GUI_H */
