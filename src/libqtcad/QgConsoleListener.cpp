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

#include "common.h"

#include <iostream>
#include "qtcad/QgConsoleListener.h"

#define RT_MAXLINE 10240

QgConsoleListener::QgConsoleListener(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d, QObject *parent)
    : QObject(parent)
{
	this->process = p;
	this->callback = c;
	this->data = d;
	this->type = t;

	// finishedGetLine -> on_finishedGetLine must be a QueuedConnection so
	// the slot executes on the GUI thread even though the signal is emitted
	// from the worker thread.
	QObject::connect(
	        this, &QgConsoleListener::finishedGetLine,
	        this, &QgConsoleListener::on_finishedGetLine,
	        Qt::QueuedConnection
	);

	// Create the platform notifier on the GUI thread, then move it to the
	// private worker thread so that activated() fires there and does not
	// block the GUI event loop.
#ifdef Q_OS_WIN
	HANDLE h = (fd < 0) ? GetStdHandle(STD_INPUT_HANDLE) : (HANDLE)_get_osfhandle(fd);
	m_notifier = new QWinEventNotifier(h);
#else
	int lfd = (fd < 0) ? fileno(stdin) : fd;
	m_notifier = new QSocketNotifier(lfd, QSocketNotifier::Read);
#endif

	// Heap-allocate the worker thread and parent it to this so that QObject
	// parent–child ownership keeps it alive as long as we are alive.  The
	// destructor still calls quit()+wait() explicitly before the parent
	// chain can destroy the thread object.
	m_thread = new QThread(this);

	m_notifier->moveToThread(m_thread);

	// When the thread finishes its event loop, schedule the notifier for
	// deletion on the thread that last used it.
	QObject::connect(m_thread, &QThread::finished, m_notifier, &QObject::deleteLater);

#ifdef Q_OS_WIN
	QObject::connect(m_notifier, &QWinEventNotifier::activated, m_notifier,
#else
	QObject::connect(m_notifier, &QSocketNotifier::activated, m_notifier,
#endif
	[this]() {
		// Runs on the worker thread.  Invoke the libged I/O callback and
		// relay any new output back to the GUI thread via finishedGetLine.
		if (callback) {
			size_t s1 = bu_vls_strlen(process->gedp->ged_result_str);
			(*callback)(data, (int)type);
			size_t s2 = bu_vls_strlen(process->gedp->ged_result_str);
			if (s1 != s2) {
				struct bu_vls nstr = BU_VLS_INIT_ZERO;
				bu_vls_substr(&nstr, process->gedp->ged_result_str, s1, s2 - s1);
				QString strLine = QString::fromStdString(std::string(bu_vls_cstr(&nstr)));
				bu_vls_free(&nstr);
				Q_EMIT this->finishedGetLine(strLine);
			}
		}
	});
	m_thread->start();
}

QgConsoleListener::~QgConsoleListener()
{
	// Ensure no more activated() callbacks fire before we tear down.
	disconnectNotifier();
	// Stop the worker thread's event loop and wait for it to exit before
	// the QObject parent chain deletes m_thread.
	m_thread->quit();
	m_thread->wait();
	// m_notifier was scheduled for deleteLater() via the finished() signal;
	// it will be destroyed on the worker thread's final event delivery.
}

void QgConsoleListener::disconnectNotifier()
{
	// Sever all signal/slot connections from the platform notifier so that
	// no further activated() lambdas fire.  This is thread-safe: it is safe
	// to call QObject::disconnect() from any thread.
	if (m_notifier)
		m_notifier->disconnect();
}

void QgConsoleListener::on_finishedGetLine(const QString &strNewLine)
{
	Q_EMIT this->newLine(strNewLine);
}

void QgConsoleListener::on_finished()
{
	Q_EMIT this->is_finished(this->process, (int)this->type);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

