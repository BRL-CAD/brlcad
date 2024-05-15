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


class QTCAD_EXPORT QConsoleListener : public QObject
{
    Q_OBJECT

    public:
	QConsoleListener(int fd = -1, struct ged_subprocess *p = NULL, bu_process_io_t t = BU_PROCESS_STDIN, ged_io_func_t c = NULL, void *d = NULL);
	~QConsoleListener();

	// Called by client code when it is done with the process
	void on_finished();

	struct ged_subprocess *process = NULL;
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

    public:
#ifdef Q_OS_WIN
	QWinEventNotifier *m_notifier;
#else
	QSocketNotifier *m_notifier;
#endif
    private:
	QThread m_thread;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

