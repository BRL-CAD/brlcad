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
 * Originally from https://github.com/juangburgos/QConsoleListener
 */

#include "common.h"

#include <iostream>
#include "qtcad/QgConsoleListener.h"

static const int qg_process_poll_interval_ms = 10;

QgConsoleListener::QgConsoleListener(int fd, struct ged_subprocess *p,
    bu_process_io_t t, ged_io_func_t c, void *d, QObject *parent)
    : QObject(parent)
{
    this->process = p;
    this->callback = c;
    this->data = d;
    this->type = t;
    m_fd = (fd < 0) ? fileno(stdin) : fd;
    QObject::connect(
	    this, &QgConsoleListener::finishedGetLine,
	    this, &QgConsoleListener::on_finishedGetLine,
	    Qt::QueuedConnection
	    );
#ifdef Q_OS_WIN
    m_notifier = new QTimer(this);
    QObject::connect(m_notifier, &QTimer::timeout, this, [this]() {
	if (bu_fd_read_ready(m_fd, 0) > 0)
	    processReady();
    });
    m_notifier->start(qg_process_poll_interval_ms);
#else
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    QObject::connect(m_notifier, &QSocketNotifier::activated, this,
	[this]() { processReady(); });
#endif
}

void QgConsoleListener::processReady()
{
    if (!callback)
	return;

    size_t start = bu_vls_strlen(process->gedp->ged_result_str);
    (*callback)(data, (int)type);
    size_t end = bu_vls_strlen(process->gedp->ged_result_str);
    if (start == end)
	return;

    struct bu_vls output = BU_VLS_INIT_ZERO;
    bu_vls_substr(&output, process->gedp->ged_result_str, start, end - start);
    QString line = QString::fromStdString(std::string(bu_vls_cstr(&output)));
    bu_vls_free(&output);
    Q_EMIT this->finishedGetLine(line);
}

QgConsoleListener::~QgConsoleListener()
{
    disconnectNotifier();
}

void QgConsoleListener::deactivate()
{
    if (!process)
	return;

    switch (type) {
	case BU_PROCESS_STDIN:
	    process->stdin_active = 0;
	    break;
	case BU_PROCESS_STDOUT:
	    process->stdout_active = 0;
	    break;
	case BU_PROCESS_STDERR:
	    process->stderr_active = 0;
	    break;
    }
}

void QgConsoleListener::disconnectNotifier()
{
    if (m_notifier)
	m_notifier->disconnect();
}

void QgConsoleListener::on_finishedGetLine(const QString &strNewLine)
{
    Q_EMIT this->newLine(strNewLine);
}

void QgConsoleListener::on_finished()
{
    deactivate();
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
