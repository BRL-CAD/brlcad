/*
 * MIT License
 *
 * Copyright (c) 2018 Juan Gonzalez Burgos
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Originally from https://github.com/juangburgos/QgConsoleListener
 */

#pragma once

#include "common.h"

#include <QObject>
#include <QThread>
#include <iostream>

#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#include <windows.h>
#else
#include <QSocketNotifier>
#endif

#include "bu.h"
#include "ged.h"

#include "qtcad/defines.h"


/**
 * Monitors a file descriptor for subprocess I/O on a dedicated worker thread
 * and relays output lines to the GUI thread via signals.
 *
 * Ownership and threading model
 * ─────────────────────────────
 * • QgConsoleListener itself lives on the **GUI thread** (the thread that
 *   created it).  It must be owned by a QgConsole instance (or another
 *   GUI-thread QObject) that outlives the listener.
 *
 * • The internal platform notifier (m_notifier) is moved to a private
 *   worker QThread (m_thread) so that blocking I/O reads do not stall
 *   the GUI event loop.  The notifier's activated() lambda runs on the
 *   worker thread.
 *
 * • All communication from the worker back to the GUI thread uses
 *   Qt::QueuedConnection: the worker emits finishedGetLine(), which is
 *   connected (queued) to on_finishedGetLine() on the GUI thread, which
 *   then emits newLine() to interested consumers.
 *
 * • Call disconnectNotifier() before destroying the listener (or before
 *   the worker thread's event loop is stopped) to ensure no further
 *   activated() callbacks fire.  The destructor calls quit()+wait() on
 *   the worker thread, which is safe because the notifier is already
 *   disconnected at that point.
 */
class QTCAD_EXPORT QgConsoleListener : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QgConsoleListener)


public:
	/**
	 * Construct a listener for file descriptor @p fd belonging to
	 * subprocess @p p.  The optional QObject @p parent is used for
	 * standard Qt parent–child ownership (lifetime management).
	 */
	explicit QgConsoleListener(int fd = -1,
	                           struct ged_subprocess *p = nullptr,
	                           bu_process_io_t t = BU_PROCESS_STDIN,
	                           ged_io_func_t c = nullptr,
	                           void *d = nullptr,
	                           QObject *parent = nullptr);
	~QgConsoleListener() override;

	/**
	 * Disconnect the platform notifier so no further activated() callbacks
	 * are delivered.  Must be called before the worker thread event loop is
	 * stopped, and before on_finished()/destruction if the caller needs to
	 * guarantee that no more I/O callbacks land after this call returns.
	 * Thread-safe: may be called from any thread.
	 */
	void disconnectNotifier();

	// Called by client code when it is done with the process
	void on_finished();

	struct ged_subprocess *process = nullptr;
	ged_io_func_t callback;
	bu_process_io_t type;
	void *data;

Q_SIGNALS:
	// connect to "newLine" to receive console input
	void newLine(const QString &strNewLine);

	// Emit when on_finished() is called
	void is_finished(struct ged_subprocess *p, int t);

	// finishedGetLine is for internal use
	void finishedGetLine(const QString &strNewLine);

private Q_SLOTS:
	void on_finishedGetLine(const QString &strNewLine);

private:
#ifdef Q_OS_WIN
	QWinEventNotifier *m_notifier = nullptr;
#else
	QSocketNotifier *m_notifier = nullptr;
#endif
	/* Worker thread that owns m_notifier.  Heap-allocated so it can be
	 * parented to this object (QObject parent–child ownership) and
	 * explicitly quit()+wait()'ed in the destructor before the parent
	 * chain tears the object down. */
	QThread *m_thread = nullptr;
};

using QConsoleListener = QgConsoleListener;

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

