#include <QTextEdit>
#include <QTextBrowser>
#include <QKeyEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMutex>
#include <QFont>

#ifndef CONSOLE_H
#define CONSOLE_H

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

