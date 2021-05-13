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

#include <iostream>
#include "qtcad/QtConsoleListener.h"

#define RT_MAXLINE 10240

QConsoleListener::QConsoleListener(int *fd, struct ged_subprocess *p)
{
    this->process = p;
    QObject::connect(
	    this, &QConsoleListener::finishedGetLine,
	    this, &QConsoleListener::on_finishedGetLine,
	    Qt::QueuedConnection
	    );
#ifdef Q_OS_WIN
    HANDLE h = (!fd) ? GetStdHandle(STDIN) : (HANDLE)_get_osfhandle(*fd);
    m_notifier = new QWinEventNotifier(h);
#else
    int lfd = (!fd) ? fileno(stdin) : *fd;
    bu_log("lfd: %d\n", lfd);
    m_notifier = new QSocketNotifier(lfd, QSocketNotifier::Read);
#endif
    // NOTE : move to thread because std::getline blocks, then we sync with
    // main thread using a QueuedConnection with finishedGetLine
    m_notifier->moveToThread(&m_thread);
    QObject::connect(&m_thread , &QThread::finished, m_notifier, &QObject::deleteLater);
#ifdef Q_OS_WIN
    QObject::connect(m_notifier, &QWinEventNotifier::activated,
#else
    QObject::connect(m_notifier, &QSocketNotifier::activated,
#endif
	[this]() {
	int count = 0;
	char line[RT_MAXLINE] = {0};
	printf("trying...\n");
	if (bu_process_read((char *)line, &count, process->p, BU_PROCESS_STDERR, RT_MAXLINE) <= 0) {
	printf("nope.\n");
	return;
	}
	printf("got something: %s\n", line);
	QString strLine = QString::fromStdString(std::string(line));
	Q_EMIT this->finishedGetLine(strLine);
	});
    m_thread.start();
}

void QConsoleListener::on_finishedGetLine(const QString &strNewLine)
{
    Q_EMIT this->newLine(strNewLine);
}

QConsoleListener::~QConsoleListener()
{
    m_thread.quit();
    m_thread.wait();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

