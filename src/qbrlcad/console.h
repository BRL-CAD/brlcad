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
//With the above structure however, it would be important to have
//a way to save a .txt version of the log - it probably wouldn't
//be practical to select and copy the text between multiple
//widgets the way one could in a single widget.  Maybe such a
//log could be made available in a second tab or some such...
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

