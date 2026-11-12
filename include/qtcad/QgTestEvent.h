/*                      Q G T E S T E V E N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file qtcad/QgTestEvent.h
 *
 * Semantic Qt GUI event recording and replay support.  The design follows
 * ParaView's translator/player split, but uses a compact versioned JSON stream
 * and normalized coordinates suitable for interactive application canvases.
 */

#ifndef QGTESTEVENT_H
#define QGTESTEVENT_H

#include "common.h"
#include "qtcad/defines.h"

#include <functional>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class QEvent;
class QWidget;

struct QTCAD_EXPORT QgTestEvent {
    QString target;
    QString action;
    QJsonObject arguments;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &object, QgTestEvent &event,
	QString *error = nullptr);
};

/** Translate low-level events that need coordinate/key fidelity. */
class QTCAD_EXPORT QgEventTranslator {
public:
    static bool translate(QObject *target, QEvent *event,
	QgTestEvent &translated);
};

/** Record semantic widget changes plus normalized canvas input.
 *
 * Canvas widgets opt in to low-level pointer and key recording by setting the
 * boolean dynamic property `qgTestCanvas`.  Other widgets are recorded through
 * their semantic signals, avoiding duplicate low-level and semantic events.
 */
class QTCAD_EXPORT QgEventRecorder : public QObject {
public:
    explicit QgEventRecorder(QObject *root, QObject *parent = nullptr);
    ~QgEventRecorder() override;

    void start();
    void stop();
    bool isRecording() const;
    void clear();
    const QVector<QgTestEvent> &events() const;
    void addEvent(const QgTestEvent &event);
    void checkpoint(QWidget *widget, const QString &name);
    bool save(const QString &fileName, QString *error = nullptr) const;

    /** Stable path prefers the qgTestId dynamic property, then objectName;
     * unnamed objects fall back to a class/ordinal segment. */
    static QString objectPath(QObject *root, QObject *target);
    static QObject *resolveObject(QObject *root, const QString &path);
    static bool load(const QString &fileName, QVector<QgTestEvent> &events,
	QString *error = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void registerObject(QObject *object);
    void append(QObject *target, const QString &action,
	const QJsonObject &arguments = QJsonObject());

    QObject *m_root = nullptr;
    bool m_recording = false;
    QSet<QObject *> m_registered;
    QVector<QgTestEvent> m_events;
};

/** Resolve stable targets and replay one semantic event at a time. */
class QTCAD_EXPORT QgEventPlayer {
public:
    using CheckpointHandler = std::function<bool(
	QWidget *widget, const QString &name, QString *error)>;

    explicit QgEventPlayer(QObject *root);
    void setCheckpointHandler(const CheckpointHandler &handler);
    bool play(const QgTestEvent &event, QString *error = nullptr) const;
    bool play(const QVector<QgTestEvent> &events,
	QString *error = nullptr) const;
    bool playFile(const QString &fileName, QString *error = nullptr) const;

private:
    QObject *m_root = nullptr;
    CheckpointHandler m_checkpoint;
};

#endif /* QGTESTEVENT_H */
