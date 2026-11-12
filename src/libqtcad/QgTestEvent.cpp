/*                    Q G T E S T E V E N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "qtcad/QgTestEvent.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QChildEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QRadioButton>
#include <QSaveFile>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVariant>
#include <QWheelEvent>
#include <QWidget>

#include <cmath>

namespace {

static const char qg_test_event_schema[] = "brlcad.qtcad.events";
static const char qg_test_canvas_property[] = "qgTestCanvas";
static const char qg_test_requested_window_size_property[] =
    "qgTestRequestedWindowSize";
static const char qg_test_requested_window_size_generation_property[] =
    "qgTestRequestedWindowSizeGeneration";
static const char qg_test_requested_window_state_property[] =
    "qgTestRequestedWindowState";
static const char qg_test_requested_window_state_generation_property[] =
    "qgTestRequestedWindowStateGeneration";
static constexpr int qg_test_window_request_timeout_milliseconds = 5000;
static constexpr int qg_test_window_request_retry_milliseconds = 10;
static constexpr int qg_test_window_stability_milliseconds = 100;
static constexpr int qg_test_event_stream_version = 1;
static constexpr double qg_test_default_numeric_tolerance = 1.0e-9;
static constexpr int qg_test_window_guard_delays_milliseconds[] = {
    250, 500, 1000, 2000, 5000, 10000, 30000
};

static qulonglong
qg_test_advance_request_generation(QObject *object, const char *propertyName)
{
    const qulonglong generation =
	object->property(propertyName).toULongLong() + 1;
    object->setProperty(propertyName, QVariant::fromValue(generation));
    return generation;
}

static bool
qg_test_numeric_text(const QString &text, double *value)
{
    if (!value)
	return false;

    bool converted = false;
    const double numericValue = text.trimmed().section(
	QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty).toDouble(&converted);
    if (!converted || !std::isfinite(numericValue))
	return false;

    *value = numericValue;
    return true;
}

static QString
qg_event_encode(const QString &text)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(text));
}

static QString
qg_event_decode(const QString &text)
{
    return QUrl::fromPercentEncoding(text.toLatin1());
}

static QJsonArray
qg_event_index_rows(const QModelIndex &index)
{
    QVector<int> reverseRows;
    for (QModelIndex current = index; current.isValid();
	 current = current.parent())
	reverseRows.push_back(current.row());
    QJsonArray rows;
    for (auto i = reverseRows.crbegin(); i != reverseRows.crend(); ++i)
	rows.append(*i);
    return rows;
}

static QJsonObject
qg_event_index_arguments(const QModelIndex &index)
{
    QJsonObject arguments;
    arguments.insert(QStringLiteral("rows"), qg_event_index_rows(index));
    arguments.insert(QStringLiteral("column"), index.column());
    arguments.insert(QStringLiteral("text"),
	index.data(Qt::DisplayRole).toString());
    return arguments;
}

static QModelIndex
qg_event_resolve_index(QAbstractItemView *view, const QJsonObject &arguments)
{
    if (!view || !view->model())
	return QModelIndex();
    QAbstractItemModel *model = view->model();
    const QJsonArray labels =
	arguments.value(QStringLiteral("labels")).toArray();
    if (!labels.isEmpty()) {
	QModelIndex parent;
	for (const QJsonValue &labelValue : labels) {
	    if (model->canFetchMore(parent))
		model->fetchMore(parent);
	    const QString label = labelValue.toString();
	    QModelIndex match;
	    const int count = model->rowCount(parent);
	    for (int row = 0; row < count; ++row) {
		const QModelIndex candidate = model->index(row, 0, parent);
		if (candidate.data(Qt::DisplayRole).toString() == label) {
		    match = candidate;
		    break;
		}
	    }
	    if (!match.isValid())
		return QModelIndex();
	    parent = match;
	}
	const int column = arguments.value(QStringLiteral("column")).toInt(0);
	return column == 0 || !parent.isValid() ? parent :
	    model->index(parent.row(), column, parent.parent());
    }
    const QJsonArray rows = arguments.value(QStringLiteral("rows")).toArray();
    QModelIndex parent;
    for (const QJsonValue &row : rows) {
	if (model->canFetchMore(parent))
	    model->fetchMore(parent);
	parent = model->index(row.toInt(-1), 0, parent);
	if (!parent.isValid())
	    return QModelIndex();
    }
    const int column = arguments.value(QStringLiteral("column")).toInt(0);
    return column == 0 || !parent.isValid() ? parent :
	model->index(parent.row(), column, parent.parent());
}

static void
qg_event_error(QString *error, const QString &message)
{
    if (error)
	*error = message;
}

static QPointF
qg_event_normalized_position(const QWidget *widget, const QPointF &position)
{
    if (!widget || widget->width() <= 0 || widget->height() <= 0)
	return QPointF();
    return QPointF(position.x() / widget->width(),
	position.y() / widget->height());
}

static QPointF
qg_event_local_position(const QWidget *widget, const QJsonObject &arguments)
{
    if (!widget)
	return QPointF();
    return QPointF(arguments.value(QStringLiteral("x")).toDouble() *
	widget->width(), arguments.value(QStringLiteral("y")).toDouble() *
	widget->height());
}

static bool
qg_event_is_canvas(QObject *object)
{
    return object && object->property(qg_test_canvas_property).toBool();
}

}

QJsonObject
QgTestEvent::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("target"), target);
    object.insert(QStringLiteral("action"), action);
    if (!arguments.isEmpty())
	object.insert(QStringLiteral("arguments"), arguments);
    return object;
}

bool
QgTestEvent::fromJson(const QJsonObject &object, QgTestEvent &event,
    QString *error)
{
    const QJsonValue target = object.value(QStringLiteral("target"));
    const QJsonValue action = object.value(QStringLiteral("action"));
    if (!target.isString() || !action.isString() || action.toString().isEmpty()) {
	qg_event_error(error, QStringLiteral("event requires string target and action"));
	return false;
    }
    event.target = target.toString();
    event.action = action.toString();
    event.arguments = object.value(QStringLiteral("arguments")).toObject();
    return true;
}

bool
QgEventTranslator::translate(QObject *target, QEvent *event,
    QgTestEvent &translated)
{
    QWidget *widget = qobject_cast<QWidget *>(target);
    if (!widget || !event)
	return false;

    if (event->type() == QEvent::MouseButtonPress ||
	event->type() == QEvent::MouseButtonRelease ||
	event->type() == QEvent::MouseMove) {
	QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const QPointF position = mouse->position();
#else
	const QPointF position = mouse->localPos();
#endif
	const QPointF normalized = qg_event_normalized_position(widget, position);
	translated.action = event->type() == QEvent::MouseButtonPress ?
	    QStringLiteral("mouse_press") :
	    (event->type() == QEvent::MouseButtonRelease ?
	     QStringLiteral("mouse_release") : QStringLiteral("mouse_move"));
	translated.arguments.insert(QStringLiteral("x"), normalized.x());
	translated.arguments.insert(QStringLiteral("y"), normalized.y());
	translated.arguments.insert(QStringLiteral("button"),
	    static_cast<int>(mouse->button()));
	translated.arguments.insert(QStringLiteral("buttons"),
	    static_cast<int>(mouse->buttons()));
	translated.arguments.insert(QStringLiteral("modifiers"),
	    static_cast<int>(mouse->modifiers()));
	return true;
    }

    if (event->type() == QEvent::Wheel) {
	QWheelEvent *wheel = static_cast<QWheelEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	const QPointF position = wheel->position();
#else
	const QPointF position = wheel->posF();
#endif
	const QPointF normalized = qg_event_normalized_position(widget, position);
	translated.action = QStringLiteral("wheel");
	translated.arguments.insert(QStringLiteral("x"), normalized.x());
	translated.arguments.insert(QStringLiteral("y"), normalized.y());
	translated.arguments.insert(QStringLiteral("pixel_x"), wheel->pixelDelta().x());
	translated.arguments.insert(QStringLiteral("pixel_y"), wheel->pixelDelta().y());
	translated.arguments.insert(QStringLiteral("angle_x"), wheel->angleDelta().x());
	translated.arguments.insert(QStringLiteral("angle_y"), wheel->angleDelta().y());
	translated.arguments.insert(QStringLiteral("modifiers"),
	    static_cast<int>(wheel->modifiers()));
	return true;
    }

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
	QKeyEvent *key = static_cast<QKeyEvent *>(event);
	translated.action = event->type() == QEvent::KeyPress ?
	    QStringLiteral("key_press") : QStringLiteral("key_release");
	translated.arguments.insert(QStringLiteral("key"), key->key());
	translated.arguments.insert(QStringLiteral("modifiers"),
	    static_cast<int>(key->modifiers()));
	translated.arguments.insert(QStringLiteral("text"), key->text());
	translated.arguments.insert(QStringLiteral("autorepeat"), key->isAutoRepeat());
	translated.arguments.insert(QStringLiteral("count"), key->count());
	return true;
    }
    return false;
}

QgEventRecorder::QgEventRecorder(QObject *root, QObject *parent) :
    QObject(parent), m_root(root)
{
}

QgEventRecorder::~QgEventRecorder()
{
    stop();
}

void
QgEventRecorder::start()
{
    if (m_recording || !m_root)
	return;
    m_recording = true;
    registerObject(m_root);
}

void
QgEventRecorder::stop()
{
    m_recording = false;
}

bool
QgEventRecorder::isRecording() const
{
    return m_recording;
}

void
QgEventRecorder::clear()
{
    m_events.clear();
}

const QVector<QgTestEvent> &
QgEventRecorder::events() const
{
    return m_events;
}

void
QgEventRecorder::addEvent(const QgTestEvent &event)
{
    m_events.push_back(event);
}

void
QgEventRecorder::checkpoint(QWidget *widget, const QString &name)
{
    QJsonObject arguments;
    arguments.insert(QStringLiteral("name"), name);
    append(widget, QStringLiteral("checkpoint"), arguments);
}

QString
QgEventRecorder::objectPath(QObject *root, QObject *target)
{
    if (!root || !target)
	return QString();
    if (root == target)
	return QStringLiteral(".");

    QStringList segments;
    for (QObject *current = target; current && current != root;
	 current = current->parent()) {
	QObject *parent = current->parent();
	if (!parent)
	    return QString();
	const QObjectList siblings = parent->children();
	const QString testId = current->property("qgTestId").toString();
	if (!testId.isEmpty()) {
	    segments.prepend(QStringLiteral("i:") + qg_event_encode(testId));
	    continue;
	}
	QString base = current->objectName();
	int ordinal = 0;
	int matches = 0;
	if (!base.isEmpty()) {
	    for (QObject *sibling : siblings) {
		if (sibling->objectName() == base) {
		    if (sibling == current)
			ordinal = matches;
		    matches++;
		}
	    }
	    QString segment = QStringLiteral("n:") + qg_event_encode(base);
	    if (matches > 1)
		segment += QStringLiteral(":") + QString::number(ordinal);
	    segments.prepend(segment);
	} else {
	    const QString className = QString::fromLatin1(current->metaObject()->className());
	    for (QObject *sibling : siblings) {
		if (QString::fromLatin1(sibling->metaObject()->className()) == className) {
		    if (sibling == current)
			ordinal = matches;
		    matches++;
		}
	    }
	    segments.prepend(QStringLiteral("c:") + qg_event_encode(className) +
		QStringLiteral(":") + QString::number(ordinal));
	}
    }
    return QStringLiteral("./") + segments.join(QLatin1Char('/'));
}

QObject *
QgEventRecorder::resolveObject(QObject *root, const QString &path)
{
    if (!root || path.isEmpty())
	return nullptr;
    if (path == QLatin1String("."))
	return root;
    /* Stable test ids are application contracts, whereas the QObject parent
     * chain below is an implementation detail which legitimately changes as
     * tool palettes, docks, and scroll areas are reorganized.  Permit a
     * unique descendant lookup for hand-authored production scripts.  A
     * duplicate is rejected rather than making event replay depend on child
     * construction order.  Recorder-authored streams continue to use their
     * explicit ./ hierarchy paths. */
    if (path.startsWith(QLatin1String("i:"))) {
	const QString testId = qg_event_decode(path.mid(2));
	QObject *match = root->property("qgTestId").toString() == testId ?
	    root : nullptr;
	const QObjectList descendants =
	    root->findChildren<QObject *>(QString(),
		Qt::FindChildrenRecursively);
	for (QObject *candidate : descendants) {
	    if (candidate->property("qgTestId").toString() != testId)
		continue;
	    if (match)
		return nullptr;
	    match = candidate;
	}
	return match;
    }
    if (!path.startsWith(QLatin1String("./")))
	return nullptr;

    QObject *current = root;
    const QStringList segments = path.mid(2).split(QLatin1Char('/'),
	Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
	const QStringList fields = segment.split(QLatin1Char(':'));
	if (fields.size() < 2)
	    return nullptr;
	const QObjectList children = current->children();
	QObject *next = nullptr;
	int ordinal = fields.size() > 2 ? fields.last().toInt() : 0;
	int match = 0;
	if (fields[0] == QLatin1String("i")) {
	    const QString testId = qg_event_decode(fields[1]);
	    for (QObject *child : children) {
		if (child->property("qgTestId").toString() == testId) {
		    next = child;
		    break;
		}
	    }
	} else if (fields[0] == QLatin1String("n")) {
	    const QString name = qg_event_decode(fields[1]);
	    for (QObject *child : children) {
		if (child->objectName() == name && match++ == ordinal) {
		    next = child;
		    break;
		}
	    }
	} else if (fields[0] == QLatin1String("c")) {
	    const QString className = qg_event_decode(fields[1]);
	    for (QObject *child : children) {
		if (QString::fromLatin1(child->metaObject()->className()) == className &&
		    match++ == ordinal) {
		    next = child;
		    break;
		}
	    }
	}
	if (!next)
	    return nullptr;
	current = next;
    }
    return current;
}

void
QgEventRecorder::append(QObject *target, const QString &action,
    const QJsonObject &arguments)
{
    if (!m_recording || !target)
	return;
    const QString path = objectPath(m_root, target);
    if (path.isEmpty())
	return;
    QgTestEvent event;
    event.target = path;
    event.action = action;
    event.arguments = arguments;
    m_events.push_back(event);
}

void
QgEventRecorder::registerObject(QObject *object)
{
    if (!object || m_registered.contains(object))
	return;
    m_registered.insert(object);
    object->installEventFilter(this);
    connect(object, &QObject::destroyed, this, [this, object]() {
	m_registered.remove(object);
    });

    if (QAction *action = qobject_cast<QAction *>(object)) {
	connect(action, &QAction::triggered, this, [this, action](bool checked) {
	    QJsonObject args;
	    args.insert(QStringLiteral("checked"), checked);
	    append(action, QStringLiteral("activate"), args);
	});
    } else if (QAbstractButton *button = qobject_cast<QAbstractButton *>(object)) {
	connect(button, &QAbstractButton::clicked, this, [this, button](bool checked) {
	    QJsonObject args;
	    args.insert(QStringLiteral("checked"), checked);
	    append(button, QStringLiteral("activate"), args);
	});
    } else if (QLineEdit *edit = qobject_cast<QLineEdit *>(object)) {
	connect(edit, &QLineEdit::editingFinished, this, [this, edit]() {
	    QJsonObject args;
	    args.insert(QStringLiteral("text"), edit->text());
	    append(edit, QStringLiteral("set_text"), args);
	});
    } else if (QComboBox *combo = qobject_cast<QComboBox *>(object)) {
	connect(combo, static_cast<void (QComboBox::*)(int)>(
	    &QComboBox::currentIndexChanged), this, [this, combo](int index) {
	    QJsonObject args;
	    args.insert(QStringLiteral("index"), index);
	    args.insert(QStringLiteral("text"), combo->currentText());
	    append(combo, QStringLiteral("set_index"), args);
	});
    } else if (QSpinBox *spin = qobject_cast<QSpinBox *>(object)) {
	connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	    this, [this, spin](int value) {
		QJsonObject args;
		args.insert(QStringLiteral("value"), value);
		append(spin, QStringLiteral("set_value"), args);
	    });
    } else if (QDoubleSpinBox *double_spin = qobject_cast<QDoubleSpinBox *>(object)) {
	connect(double_spin, static_cast<void (QDoubleSpinBox::*)(double)>(
	    &QDoubleSpinBox::valueChanged), this, [this, double_spin](double value) {
		QJsonObject args;
		args.insert(QStringLiteral("value"), value);
		append(double_spin, QStringLiteral("set_value"), args);
	    });
    } else if (QSlider *slider = qobject_cast<QSlider *>(object)) {
	connect(slider, &QSlider::valueChanged, this, [this, slider](int value) {
	    QJsonObject args;
	    args.insert(QStringLiteral("value"), value);
	    append(slider, QStringLiteral("set_value"), args);
	});
    } else if (QTabWidget *tabs = qobject_cast<QTabWidget *>(object)) {
	connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
	    QJsonObject args;
	    args.insert(QStringLiteral("index"), index);
	    append(tabs, QStringLiteral("set_index"), args);
	});
    }

    if (QAbstractItemView *view = qobject_cast<QAbstractItemView *>(object)) {
	if (view->selectionModel()) {
	    connect(view->selectionModel(), &QItemSelectionModel::currentChanged,
		this, [this, view](const QModelIndex &current, const QModelIndex &) {
		    if (current.isValid())
			append(view, QStringLiteral("set_current"),
			    qg_event_index_arguments(current));
		});
	}
	if (QTreeView *tree = qobject_cast<QTreeView *>(view)) {
	    connect(tree, &QTreeView::expanded, this,
		[this, tree](const QModelIndex &index) {
		    QJsonObject args = qg_event_index_arguments(index);
		    args.insert(QStringLiteral("expanded"), true);
		    append(tree, QStringLiteral("set_expanded"), args);
		});
	    connect(tree, &QTreeView::collapsed, this,
		[this, tree](const QModelIndex &index) {
		    QJsonObject args = qg_event_index_arguments(index);
		    args.insert(QStringLiteral("expanded"), false);
		    append(tree, QStringLiteral("set_expanded"), args);
		});
	}
    }

    const QObjectList children = object->children();
    for (QObject *child : children)
	registerObject(child);
}

bool
QgEventRecorder::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::ChildAdded) {
	QChildEvent *child = static_cast<QChildEvent *>(event);
	if (child->child())
	    registerObject(child->child());
    }
    if (m_recording && qg_event_is_canvas(watched)) {
	QgTestEvent translated;
	if (QgEventTranslator::translate(watched, event, translated))
	    append(watched, translated.action, translated.arguments);
    }
    return QObject::eventFilter(watched, event);
}

bool
QgEventRecorder::save(const QString &fileName, QString *error) const
{
    QJsonArray records;
    for (const QgTestEvent &event : m_events)
	records.append(event.toJson());
    QJsonObject root;
    root.insert(QStringLiteral("schema"),
	QString::fromLatin1(qg_test_event_schema));
    root.insert(QStringLiteral("version"), qg_test_event_stream_version);
    root.insert(QStringLiteral("events"), records);

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
	qg_event_error(error, file.errorString());
	return false;
    }
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size() || !file.commit()) {
	qg_event_error(error, file.errorString());
	return false;
    }
    return true;
}

bool
QgEventRecorder::load(const QString &fileName, QVector<QgTestEvent> &events,
    QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
	qg_event_error(error, file.errorString());
	return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(),
	&parseError);
    if (document.isNull() || !document.isObject()) {
	qg_event_error(error, parseError.errorString());
	return false;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schema")).toString() !=
	QString::fromLatin1(qg_test_event_schema) ||
	root.value(QStringLiteral("version")).toInt() !=
	qg_test_event_stream_version) {
	qg_event_error(error, QStringLiteral("unsupported qtcad event stream"));
	return false;
    }
    QVector<QgTestEvent> loaded;
    const QJsonArray records = root.value(QStringLiteral("events")).toArray();
    for (int i = 0; i < records.size(); ++i) {
	QgTestEvent event;
	if (!records[i].isObject() ||
	    !QgTestEvent::fromJson(records[i].toObject(), event, error)) {
	    if (error && error->isEmpty())
		*error = QStringLiteral("invalid event %1").arg(i);
	    return false;
	}
	loaded.push_back(event);
    }
    events = loaded;
    return true;
}

QgEventPlayer::QgEventPlayer(QObject *root) : m_root(root)
{
}

void
QgEventPlayer::setCheckpointHandler(const CheckpointHandler &handler)
{
    m_checkpoint = handler;
}

bool
QgEventPlayer::play(const QgTestEvent &event, QString *error) const
{
    /* Descriptor-driven editors replace parameter rows when a command or
     * primitive type changes.  QWidget row teardown uses deferred deletion;
     * a replay runs inside one outer call stack and can otherwise observe both
     * the retired and replacement qgTestId briefly, even after a timed wait.
     * Real user input returns to the application event loop between actions,
     * so make that lifecycle boundary explicit before resolving a target. */
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QObject *target = QgEventRecorder::resolveObject(m_root, event.target);
    if (!target) {
	QString detail;
	if (m_root && event.target.startsWith(QLatin1String("i:"))) {
	    const QString testId = qg_event_decode(event.target.mid(2));
	    QStringList matches;
	    const QObjectList descendants = m_root->findChildren<QObject *>(
		QString(), Qt::FindChildrenRecursively);
	    for (QObject *candidate : descendants) {
		if (candidate->property("qgTestId").toString() != testId)
		    continue;
		QStringList parents;
		for (QObject *parent = candidate->parent(); parent &&
		    parent != m_root; parent = parent->parent()) {
		    parents.prepend(QStringLiteral("%1(%2)").arg(
			QString::fromLatin1(parent->metaObject()->className()),
			parent->objectName()));
		}
		QWidget *widget = qobject_cast<QWidget *>(candidate);
		matches.append(QStringLiteral("%1(%2, visible=%3, parents=%4)")
		    .arg(QString::fromLatin1(candidate->metaObject()->className()),
			candidate->objectName(),
			widget && widget->isVisible() ? QStringLiteral("true") :
			QStringLiteral("false"),
			parents.join(QLatin1Char('/'))));
	    }
	    detail = QStringLiteral(" (%1 matching objects%2)")
		.arg(matches.size())
		.arg(matches.isEmpty() ? QString() :
		    QStringLiteral(": ") + matches.join(QStringLiteral(", ")));
	}
	qg_event_error(error,
	    QStringLiteral("event target not found: %1%2")
	    .arg(event.target, detail));
	return false;
    }

    if (event.action == QLatin1String("activate")) {
	if (QAction *action = qobject_cast<QAction *>(target)) {
	    if (action->isCheckable() &&
		event.arguments.contains(QStringLiteral("checked"))) {
		const bool desired = event.arguments.value(
		    QStringLiteral("checked")).toBool();
		/* trigger() toggles a checkable action.  Setting the desired state
		 * first therefore produced its inverse, making recorded checkbox
		 * transitions dependent on their starting state. */
		if (action->isChecked() == desired)
		    return true;
	    }
	    action->trigger();
	    return true;
	}
	if (QAbstractButton *button = qobject_cast<QAbstractButton *>(target)) {
	    if (button->isCheckable() &&
		event.arguments.contains(QStringLiteral("checked"))) {
		const bool desired = event.arguments.value(
		    QStringLiteral("checked")).toBool();
		if (button->isChecked() == desired) {
		    /* Clicking an already selected radio button is non-toggling and
		     * may still be the operation a replay requested. */
		    if (qobject_cast<QRadioButton *>(button))
			button->click();
		    return true;
		}
	    }
	    button->click();
	    return true;
	}
    } else if (event.action == QLatin1String("set_text")) {
	if (QLineEdit *edit = qobject_cast<QLineEdit *>(target)) {
	    edit->setText(event.arguments.value(QStringLiteral("text")).toString());
	    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
	    return true;
	}
    } else if (event.action == QLatin1String("set_index")) {
	const int index = event.arguments.value(QStringLiteral("index")).toInt(-1);
	if (QComboBox *combo = qobject_cast<QComboBox *>(target)) {
	    combo->setCurrentIndex(index);
	    return index >= -1 && index < combo->count();
	}
	if (QTabWidget *tabs = qobject_cast<QTabWidget *>(target)) {
	    tabs->setCurrentIndex(index);
	    return index >= 0 && index < tabs->count();
	}
    } else if (event.action == QLatin1String("set_value")) {
	const double value = event.arguments.value(QStringLiteral("value")).toDouble();
	if (QSpinBox *spin = qobject_cast<QSpinBox *>(target)) {
	    spin->setValue(static_cast<int>(value));
	    return true;
	}
	if (QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox *>(target)) {
	    spin->setValue(value);
	    return true;
	}
	if (QSlider *slider = qobject_cast<QSlider *>(target)) {
	    slider->setValue(static_cast<int>(value));
	    return true;
	}
    } else if (event.action == QLatin1String("assert_state")) {
	/* Semantic GUI tests need to verify that editor state follows scene
	 * state, not only that an input event was accepted.  Keep these
	 * assertions widget-generic so plugin tests do not depend on QObject
	 * construction paths or private implementation types. */
	const auto fail = [error](const QString &message) {
	    qg_event_error(error, message);
	    return false;
	};
	if (event.arguments.contains(QStringLiteral("enabled")) &&
	    target->isWidgetType() &&
	    static_cast<QWidget *>(target)->isEnabled() !=
	    event.arguments.value(QStringLiteral("enabled")).toBool())
	    return fail(QStringLiteral("assert_state enabled mismatch"));
	if (event.arguments.contains(QStringLiteral("visible")) &&
	    target->isWidgetType() &&
	    static_cast<QWidget *>(target)->isVisible() !=
	    event.arguments.value(QStringLiteral("visible")).toBool())
	    return fail(QStringLiteral("assert_state visible mismatch"));
	if (event.arguments.contains(QStringLiteral("checked"))) {
	    QAbstractButton *button = qobject_cast<QAbstractButton *>(target);
	    if (!button || button->isChecked() !=
		event.arguments.value(QStringLiteral("checked")).toBool())
		return fail(QStringLiteral("assert_state checked mismatch"));
	}
	QString text;
	bool hasText = false;
	if (QLineEdit *edit = qobject_cast<QLineEdit *>(target)) {
	    text = edit->text();
	    hasText = true;
	} else if (QComboBox *combo = qobject_cast<QComboBox *>(target)) {
	    text = combo->currentText();
	    hasText = true;
	} else if (QAbstractButton *button =
		qobject_cast<QAbstractButton *>(target)) {
	    text = button->text();
	    hasText = true;
	} else if (QLabel *label = qobject_cast<QLabel *>(target)) {
	    text = label->text();
	    hasText = true;
	}
	if (event.arguments.contains(QStringLiteral("text")) &&
	    (!hasText || text !=
		event.arguments.value(QStringLiteral("text")).toString()))
	    return fail(QStringLiteral("assert_state text mismatch: '%1'")
		.arg(text));
	const QString textNumericKeys[] = {
	    QStringLiteral("text_numeric_value"),
	    QStringLiteral("text_numeric_gt"),
	    QStringLiteral("text_numeric_ge"),
	    QStringLiteral("text_numeric_lt"),
	    QStringLiteral("text_numeric_le")
	};
	bool needsTextNumericValue = false;
	for (const QString &key : textNumericKeys)
	    needsTextNumericValue = needsTextNumericValue ||
		event.arguments.contains(key);
	if (needsTextNumericValue) {
	    double actual = 0.0;
	    if (!hasText || !qg_test_numeric_text(text, &actual))
		return fail(QStringLiteral(
		    "assert_state text has no finite numeric prefix: '%1'")
		    .arg(text));
	    for (const QString &key : textNumericKeys) {
		if (!event.arguments.contains(key))
		    continue;
		const double expected = event.arguments.value(key).toDouble();
		const double tolerance = event.arguments.value(
		    QStringLiteral("tolerance")).toDouble(
			qg_test_default_numeric_tolerance);
		const bool matches = key == QLatin1String("text_numeric_value") ?
		    qAbs(actual - expected) <= tolerance :
		    key == QLatin1String("text_numeric_gt") ? actual > expected :
		    key == QLatin1String("text_numeric_ge") ? actual >= expected :
		    key == QLatin1String("text_numeric_lt") ? actual < expected :
		    actual <= expected;
		if (!matches)
		    return fail(QStringLiteral(
			"assert_state %1 mismatch: %2 versus %3")
			.arg(key).arg(actual, 0, 'g', 17)
			.arg(expected, 0, 'g', 17));
	    }
	}
	const QString valueKeys[] = {
	    QStringLiteral("value"), QStringLiteral("value_gt"),
	    QStringLiteral("value_ge"), QStringLiteral("value_lt"),
	    QStringLiteral("value_le")
	};
	bool needsValue = false;
	for (const QString &key : valueKeys)
	    needsValue = needsValue || event.arguments.contains(key);
	if (needsValue) {
	    double actual = 0.0;
	    bool hasValue = true;
	    if (QDoubleSpinBox *doubleSpin = qobject_cast<QDoubleSpinBox *>(target))
		actual = doubleSpin->value();
	    else if (QSpinBox *integerSpin = qobject_cast<QSpinBox *>(target))
		actual = integerSpin->value();
	    else if (QSlider *slider = qobject_cast<QSlider *>(target))
		actual = slider->value();
	    else
		hasValue = false;
	    if (!hasValue)
		return fail(QStringLiteral("assert_state target has no numeric value"));
	    for (const QString &key : valueKeys) {
		if (!event.arguments.contains(key))
		    continue;
		const double expected = event.arguments.value(key).toDouble();
		const double tolerance = event.arguments.value(
		    QStringLiteral("tolerance")).toDouble(
			qg_test_default_numeric_tolerance);
		const bool matches = key == QLatin1String("value") ?
		    qAbs(actual - expected) <= tolerance :
		    key == QLatin1String("value_gt") ? actual > expected :
		    key == QLatin1String("value_ge") ? actual >= expected :
		    key == QLatin1String("value_lt") ? actual < expected :
		    actual <= expected;
		if (!matches)
		    return fail(QStringLiteral(
			"assert_state %1 mismatch: %2 versus %3")
			.arg(key).arg(actual, 0, 'g', 17)
			.arg(expected, 0, 'g', 17));
	    }
	}
	if (QComboBox *combo = qobject_cast<QComboBox *>(target)) {
	    if (event.arguments.contains(QStringLiteral("count")) &&
		combo->count() !=
		event.arguments.value(QStringLiteral("count")).toInt())
		return fail(QStringLiteral("assert_state count mismatch: %1")
		    .arg(combo->count()));
	    if (event.arguments.contains(QStringLiteral("index")) &&
		combo->currentIndex() !=
		event.arguments.value(QStringLiteral("index")).toInt())
		return fail(QStringLiteral("assert_state index mismatch: %1")
		    .arg(combo->currentIndex()));
	    const QJsonArray expectedItems =
		event.arguments.value(QStringLiteral("items")).toArray();
	    if (!expectedItems.isEmpty()) {
		if (expectedItems.size() != combo->count())
		    return fail(QStringLiteral(
			"assert_state item count mismatch: %1")
			.arg(combo->count()));
		for (int i = 0; i < combo->count(); ++i) {
		    if (combo->itemText(i) != expectedItems.at(i).toString())
			return fail(QStringLiteral(
			    "assert_state item %1 mismatch: '%2'")
			    .arg(i).arg(combo->itemText(i)));
		}
	    }
	}
	if (QListWidget *list = qobject_cast<QListWidget *>(target)) {
	    if (event.arguments.contains(QStringLiteral("count")) &&
		list->count() !=
		event.arguments.value(QStringLiteral("count")).toInt())
		return fail(QStringLiteral("assert_state count mismatch: %1")
		    .arg(list->count()));
	    const QJsonArray expectedItems =
		event.arguments.value(QStringLiteral("items")).toArray();
	    if (!expectedItems.isEmpty()) {
		if (expectedItems.size() != list->count())
		    return fail(QStringLiteral(
			"assert_state item count mismatch: %1")
			.arg(list->count()));
		for (int i = 0; i < list->count(); ++i) {
		    QListWidgetItem *item = list->item(i);
		    const QString actual = item ? item->text() : QString();
		    if (actual != expectedItems.at(i).toString())
			return fail(QStringLiteral(
			    "assert_state item %1 mismatch: '%2'")
			    .arg(i).arg(actual));
		}
	    }
	}
	return true;
    } else if (event.action == QLatin1String("set_current")) {
	QAbstractItemView *view = qobject_cast<QAbstractItemView *>(target);
	const QModelIndex index = qg_event_resolve_index(view, event.arguments);
	if (view && index.isValid()) {
	    view->setCurrentIndex(index);
	    if (view->selectionModel()) {
		QItemSelectionModel::SelectionFlags flags =
		    QItemSelectionModel::ClearAndSelect;
		if (view->selectionBehavior() == QAbstractItemView::SelectRows)
		    flags |= QItemSelectionModel::Rows;
		view->selectionModel()->select(index, flags);
	    }
	    view->scrollTo(index);
	    return true;
	}
    } else if (event.action == QLatin1String("clear_selection")) {
	QAbstractItemView *view = qobject_cast<QAbstractItemView *>(target);
	if (view && view->selectionModel()) {
	    view->selectionModel()->select(QModelIndex(),
		QItemSelectionModel::Clear);
	    view->selectionModel()->clearCurrentIndex();
	    return true;
	}
    } else if (event.action == QLatin1String("set_expanded")) {
	QTreeView *tree = qobject_cast<QTreeView *>(target);
	const QModelIndex index = qg_event_resolve_index(tree, event.arguments);
	if (tree && index.isValid()) {
	    const bool expanded =
		event.arguments.value(QStringLiteral("expanded")).toBool(true);
	    tree->setExpanded(index, expanded);
	    if (expanded && tree->model()->canFetchMore(index))
		tree->model()->fetchMore(index);
	    tree->scrollTo(index);
	    return tree->isExpanded(index) == expanded;
	}
    } else if (event.action == QLatin1String("checkpoint")) {
	QWidget *widget = qobject_cast<QWidget *>(target);
	if (widget && m_checkpoint)
	    return m_checkpoint(widget,
		event.arguments.value(QStringLiteral("name")).toString(), error);
	qg_event_error(error, QStringLiteral("checkpoint has no widget or handler"));
	return false;
    } else if (event.action == QLatin1String("resize")) {
	QWidget *widget = qobject_cast<QWidget *>(target);
	const int width =
	    event.arguments.value(QStringLiteral("width")).toInt();
	const int height =
	    event.arguments.value(QStringLiteral("height")).toInt();
	if (widget && width > 0 && height > 0) {
	    const QSize requestedSize(width, height);
	    const auto applySize = [widget, requestedSize]() {
		widget->resize(requestedSize);
	    };
	    applySize();
	    if (!widget->isWindow())
		return widget->size() == requestedSize;

	    /* A window manager may acknowledge an older configure request after
	     * QWidget::resize() has already returned the requested local size.
	     * Keep the latest scripted size authoritative, but tag every request
	     * so a delayed guard can never resurrect a superseded size. */
	    const qulonglong generation = qg_test_advance_request_generation(
		widget, qg_test_requested_window_size_generation_property);
	    widget->setProperty(qg_test_requested_window_size_property,
		QVariant(requestedSize));
	    for (int delay : qg_test_window_guard_delays_milliseconds) {
		QTimer::singleShot(delay, widget,
		    [widget, requestedSize, generation, applySize]() {
			if (widget->property(
				qg_test_requested_window_size_generation_property)
				.toULongLong() != generation ||
			    widget->property(
				qg_test_requested_window_size_property).toSize() !=
				requestedSize)
			    return;
			if (widget->size() != requestedSize)
			    applySize();
		    });
	    }

	    const int stableMilliseconds = qMax(0,
		event.arguments.value(QStringLiteral("stable_ms")).toInt());
	    if (!stableMilliseconds)
		return widget->size() == requestedSize;
	    const int timeoutMilliseconds = qMax(stableMilliseconds,
		event.arguments.value(QStringLiteral("timeout_ms")).toInt(
		    qg_test_window_request_timeout_milliseconds));
	    QElapsedTimer elapsed;
	    QElapsedTimer stable;
	    elapsed.start();
	    while (elapsed.elapsed() < timeoutMilliseconds) {
		QGuiApplication::sync();
		if (widget->size() == requestedSize) {
		    if (!stable.isValid())
			stable.start();
		    if (stable.elapsed() >= stableMilliseconds)
			return true;
		} else {
		    stable.invalidate();
		    applySize();
		}
		QEventLoop loop;
		QTimer::singleShot(qg_test_window_request_retry_milliseconds,
		    &loop, &QEventLoop::quit);
		loop.exec(QEventLoop::ExcludeUserInputEvents);
	    }
	    qg_event_error(error,
		QStringLiteral("resize did not remain at %1x%2")
		.arg(width).arg(height));
	    return false;
	}
    } else if (event.action == QLatin1String("window_state")) {
	QWidget *widget = qobject_cast<QWidget *>(target);
	const QString state = event.arguments.value(
	    QStringLiteral("state")).toString().toLower();
	if (!widget) {
	    qg_event_error(error,
		QStringLiteral("window_state target is not a widget"));
	    return false;
	}
	if (state != QLatin1String("normal") &&
	    state != QLatin1String("minimized") &&
	    state != QLatin1String("maximized") &&
	    state != QLatin1String("fullscreen")) {
	    qg_event_error(error,
		QStringLiteral("unknown window_state: %1").arg(state));
	    return false;
	}
	QWidget *window = widget->window();
	const qulonglong generation = qg_test_advance_request_generation(
	    window, qg_test_requested_window_state_generation_property);
	window->setProperty(qg_test_requested_window_state_property, state);
	const auto applyState = [window, state]() {
	    if (state == QLatin1String("normal")) {
		/* Clear the native state explicitly before showNormal().  On X11 an
		 * acknowledged minimize request may otherwise remain queued behind
		 * the local QWidget flag and arrive after the following restore. */
		window->setWindowState(Qt::WindowNoState);
		window->showNormal();
	    } else if (state == QLatin1String("minimized")) {
		window->showMinimized();
	    } else if (state == QLatin1String("maximized")) {
		window->showMaximized();
	    } else {
		window->showFullScreen();
	    }
	};
	applyState();
	/* Treat the event as a desired state until another window_state event
	 * supersedes it.  A native minimize acknowledgement can arrive seconds
	 * after Qt's local flag first reported normal when the owner thread starts
	 * expensive scene work immediately after restore.  Bounded delayed guards
	 * repair that stale acknowledgement without blocking the event player or
	 * fighting a later maximize/fullscreen request. */
	for (int delay : qg_test_window_guard_delays_milliseconds) {
	    QTimer::singleShot(delay, window,
		[window, state, generation, applyState]() {
		    if (window->property(
			    qg_test_requested_window_state_generation_property)
			    .toULongLong() != generation)
			return;
		    if (window->property(
			    qg_test_requested_window_state_property).toString() !=
			    state)
			return;
		    const bool reached = state == QLatin1String("minimized") ?
			window->isMinimized() :
			state == QLatin1String("maximized") ?
			(window->isMaximized() && !window->isMinimized()) :
			state == QLatin1String("fullscreen") ?
			(window->isFullScreen() && !window->isMinimized()) :
			(!window->isMinimized() && !window->isMaximized() &&
			 !window->isFullScreen());
		    if (!reached)
			applyState();
		});
	}
	/* Window-manager state transitions are asynchronous.  Returning as soon
	 * as showNormal()/showMinimized() queues the request lets a delayed older
	 * transition overtake the next scripted state; a large-scene test can then
	 * try to present into a window which became minimized again much later.
	 * Synchronize with the native window system and require a short stable
	 * interval, reasserting the requested state if an older acknowledgement
	 * arrives.  Event processing may also advance ordinary application work;
	 * that is valid elapsed time inside this explicit semantic barrier. */
	const auto stateReached = [window, state]() {
	    if (state == QLatin1String("minimized"))
		return window->isMinimized();
	    if (state == QLatin1String("maximized"))
		return window->isMaximized() && !window->isMinimized();
	    if (state == QLatin1String("fullscreen"))
		return window->isFullScreen() && !window->isMinimized();
	    return !window->isMinimized() && !window->isMaximized() &&
		!window->isFullScreen();
	};
	QElapsedTimer stateWait;
	QElapsedTimer stableWait;
	stateWait.start();
	/* Software rendering can legitimately occupy the owner thread for several
	 * hundred milliseconds.  Allow multiple complete frames while still
	 * requiring a stable native window state before continuing. */
	while (stateWait.elapsed() <
		qg_test_window_request_timeout_milliseconds) {
	    QGuiApplication::sync();
	    if (stateReached()) {
		if (!stableWait.isValid())
		    stableWait.start();
		if (stableWait.elapsed() >=
			qg_test_window_stability_milliseconds)
		    return true;
	    } else {
		stableWait.invalidate();
		applyState();
	    }
	    QEventLoop loop;
	    QTimer::singleShot(qg_test_window_request_retry_milliseconds,
		&loop, &QEventLoop::quit);
	    loop.exec(QEventLoop::ExcludeUserInputEvents);
	}
	qg_event_error(error,
	    QStringLiteral("window_state did not remain at %1").arg(state));
	return false;
    } else if (event.action == QLatin1String("wait")) {
	QEventLoop loop;
	QTimer::singleShot(qMax(0, event.arguments.value(QStringLiteral("ms")).toInt()),
	    &loop, &QEventLoop::quit);
	loop.exec();
	return true;
    } else if (event.action.startsWith(QLatin1String("mouse_"))) {
	QWidget *widget = qobject_cast<QWidget *>(target);
	if (widget) {
	    QEvent::Type type = event.action == QLatin1String("mouse_press") ?
		QEvent::MouseButtonPress :
		(event.action == QLatin1String("mouse_release") ?
		 QEvent::MouseButtonRelease : QEvent::MouseMove);
	    const QPointF local = qg_event_local_position(widget, event.arguments);
	    QMouseEvent mouse(type, local, widget->mapToGlobal(local.toPoint()),
		static_cast<Qt::MouseButton>(event.arguments.value(QStringLiteral("button")).toInt()),
		static_cast<Qt::MouseButtons>(event.arguments.value(QStringLiteral("buttons")).toInt()),
		static_cast<Qt::KeyboardModifiers>(event.arguments.value(QStringLiteral("modifiers")).toInt()));
	    return QCoreApplication::sendEvent(widget, &mouse);
	}
    } else if (event.action == QLatin1String("wheel")) {
	QWidget *widget = qobject_cast<QWidget *>(target);
	if (widget) {
	    const QPointF local = qg_event_local_position(widget, event.arguments);
	    const QPointF global = widget->mapToGlobal(local.toPoint());
	    QWheelEvent wheel(local, global,
		QPoint(event.arguments.value(QStringLiteral("pixel_x")).toInt(),
		    event.arguments.value(QStringLiteral("pixel_y")).toInt()),
		QPoint(event.arguments.value(QStringLiteral("angle_x")).toInt(),
		    event.arguments.value(QStringLiteral("angle_y")).toInt()),
		Qt::NoButton,
		static_cast<Qt::KeyboardModifiers>(event.arguments.value(QStringLiteral("modifiers")).toInt()),
		Qt::NoScrollPhase, false);
	    return QCoreApplication::sendEvent(widget, &wheel);
	}
    } else if (event.action == QLatin1String("key_press") ||
	event.action == QLatin1String("key_release")) {
	QEvent::Type type = event.action == QLatin1String("key_press") ?
	    QEvent::KeyPress : QEvent::KeyRelease;
	QKeyEvent key(type,
	    event.arguments.value(QStringLiteral("key")).toInt(),
	    static_cast<Qt::KeyboardModifiers>(event.arguments.value(QStringLiteral("modifiers")).toInt()),
	    event.arguments.value(QStringLiteral("text")).toString(),
	    event.arguments.value(QStringLiteral("autorepeat")).toBool(),
	    static_cast<ushort>(event.arguments.value(QStringLiteral("count")).toInt(1)));
	return QCoreApplication::sendEvent(target, &key);
    }

    qg_event_error(error, QStringLiteral("unsupported action '%1' for %2")
	.arg(event.action, QString::fromLatin1(target->metaObject()->className())));
    return false;
}

bool
QgEventPlayer::play(const QVector<QgTestEvent> &events, QString *error) const
{
    for (int i = 0; i < events.size(); ++i) {
	QString eventError;
	if (!play(events[i], &eventError)) {
	    qg_event_error(error, QStringLiteral("event %1: %2").arg(i).arg(eventError));
	    return false;
	}
	QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    return true;
}

bool
QgEventPlayer::playFile(const QString &fileName, QString *error) const
{
    QVector<QgTestEvent> events;
    return QgEventRecorder::load(fileName, events, error) && play(events, error);
}
