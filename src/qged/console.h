/*                       C O N S O L E . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file console.h
 *
 * Brief description
 *
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ < 6) && !defined(__clang__)
#  pragma message "Disabling GCC float equality comparison warnings via pragma due to Qt headers..."
#endif
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#undef Success
#include <QTextEdit>
#undef Success
#include <QTextBrowser>
#undef Success
#include <QKeyEvent>
#undef Success
#include <QScrollArea>
#undef Success
#include <QVBoxLayout>
#undef Success
#include <QMutex>
#undef Success
#include <QFont>
#undef Success
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

class Console;

class ConsoleInput : public QTextEdit
{
    Q_OBJECT

    public:
	ConsoleInput(QWidget *pparent, Console *pc);
	~ConsoleInput(){};

	void resizeEvent(QResizeEvent *pevent);

	void DoCommand();
	void keyPressEvent(QKeyEvent *e);

	Console *parent_console;

	int anchor_pos;

	int history_pos;
	QStringList history;
};

class ConsoleLog : public QTextBrowser
{
    Q_OBJECT

    public:
	ConsoleLog(QWidget *pparent, Console *pc);
	~ConsoleLog(){};

	void keyPressEvent(QKeyEvent *e);
	void resizeEvent(QResizeEvent *pevent);
	void append_results(const QString &results, int format);

	Console *parent_console;

	QMutex writemutex;

	QString command;

};


// TODO - add a way to save a .txt version of the log...
class Console : public QWidget
{
    Q_OBJECT

    public:
	Console(QWidget *pparent);
	~Console();

	void mouseMoveEvent(QMouseEvent *e);

	QScrollArea *scrollarea;
	void keyPressEvent(QKeyEvent *e);
	void prompt(QString new_prompt);

    signals:
	void executeCommand(const QString &, ConsoleLog *rlog);

    public slots:
	void do_update_scrollbars(int, int);

    protected:
	void resizeEvent(QResizeEvent *pevent);

    public:
	ConsoleInput *input;
	QString console_prompt;
	QVBoxLayout *vlayout;
	QFont terminalfont;
	QSet<ConsoleLog *> logs;

};

#endif /* CONSOLE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

