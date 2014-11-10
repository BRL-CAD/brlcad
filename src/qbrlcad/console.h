#include <QTextEdit>
#include <QTextBrowser>
#include <QKeyEvent>
#include <QScrollArea>
#include <QMutex>

#ifndef CONSOLE_H
#define CONSOLE_H

class Console;

class ConsoleInput : public QTextEdit
{
    Q_OBJECT

    public:
	ConsoleInput(QWidget *pparent);
	~ConsoleInput(){};

	void resizeEvent(QResizeEvent *pevent);

	void DoCommand();
	void keyPressEvent(QKeyEvent *e);

	Console *parent_console;

	int anchor_pos;

};

class ConsoleLog : public QTextBrowser
{
    Q_OBJECT

    public:
	ConsoleLog(QWidget *pparent);
	~ConsoleLog(){};

	void keyPressEvent(QKeyEvent *e);
	void resizeEvent(QResizeEvent *pevent);

	Console *parent_console;
	int is_empty;
	int offset;

	QMutex writemutex;

};

//TODO - investigate whether it might not be better
//to create one ConsoleLog object per command output
//and stack those in a QVBoxLayout dynamically (if that
//can be done) - with any luck there would be some performance
//benefit rendering wise to smaller individual HTML docs, and
//it might be cleaner making sure output goes where it is
//intended in the console.
//
//Possibly even more beneficial, each command - once executed -
//would have its own dedicated output canvas into which its
//output would be written.  This may neatly solve the problem
//of keeping output ordered if we go the route of forking off
//commands to run in their own threads as non-blocking.
//
//Something else to investigate - with the idea of links being
//clickable for the tree view, see if we can also allow drag-and-drop
//of anchors from the QTextBrowsers into the input widget that
//would result in the appropriate text being added to the command
//prompt (saves typing long pathnames for things like editing commands)
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
	void append_results(const QString &results);

    signals:
	void executeCommand(const QString &);

    public slots:
	void do_update_scrollbars(int, int);

    protected:
	void resizeEvent(QResizeEvent *pevent);

    public:
	ConsoleInput *input;
	ConsoleLog *log;
	QString console_prompt;

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

