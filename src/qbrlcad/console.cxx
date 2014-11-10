#include <iostream>
#include "console.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QFontDatabase>
#include <QFont>

#include "cadapp.h"
#include "cadtreeview.h"

ConsoleInput::ConsoleInput(QWidget *pparent, Console *pc) : QTextEdit(pparent)
{
    anchor_pos = textCursor().position();
    document()->setDocumentMargin(0);
    setMinimumHeight(document()->size().height());
    parent_console = pc;

    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ConsoleInput::resizeEvent(QResizeEvent *e)
{
    QTextEdit::resizeEvent(e);
}

void ConsoleInput::DoCommand()
{
    QTextCursor itc= textCursor();
    QString command;
    int end_pos;

    //capture command text
    end_pos = textCursor().position();
    itc.setPosition(anchor_pos, QTextCursor::MoveAnchor);
    itc.setPosition(end_pos, QTextCursor::KeepAnchor);
    command = itc.selectedText();

    ConsoleLog *new_log = new ConsoleLog(parent_console, parent_console);
    parent_console->logs.insert(new_log);
    QMutexLocker locker(&(new_log->writemutex));
    new_log->clear();
    new_log->textCursor().insertHtml(parent_console->console_prompt);
    new_log->textCursor().insertHtml(command.trimmed());
    new_log->textCursor().insertHtml("<br>");
    // Update the size of the log
    new_log->setMinimumHeight(new_log->document()->size().height());
    new_log->setMaximumHeight(new_log->document()->size().height());

    parent_console->vlayout->insertWidget(parent_console->vlayout->count()-1, new_log);

    locker.unlock();

    // Reset the input buffer
    clear();
    insertHtml(parent_console->console_prompt);
    itc= textCursor();
    anchor_pos = itc.position();
    setMinimumHeight(document()->size().height());

    // Emit the command - up to the application to run it and return results
    emit parent_console->executeCommand(command, new_log);
}

void ConsoleInput::keyPressEvent(QKeyEvent *e)
{
   switch(e->key())
    {
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
	    e->accept();
	    if (textCursor().position() > anchor_pos) {
		QTextEdit::keyPressEvent(e);
	    }
	    break;
	case Qt::Key_Return:
	case Qt::Key_Enter:
	    e->accept();
	    DoCommand();
	    break;
	default:
	    e->accept();
	    if (textCursor().position() < anchor_pos) {
		QTextCursor itc= textCursor();
		itc.setPosition(anchor_pos);
		setTextCursor(itc);
	    }
	    QTextEdit::keyPressEvent(e);
	    setMinimumHeight(document()->size().height());
	    break;
    }
}


ConsoleLog::ConsoleLog(QWidget *pparent, Console *pc) : QTextBrowser(pparent)
{
    document()->setDocumentMargin(0);
    parent_console = pc;
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /* Note - this limit may be necessary, but a better approach would be to
     * automatically "fold" older consolelog outputs and just show their
     * commands and maybe the first few lines of output unless the user
     * specifically clicks them open (and allow them to close again...) */
    //document()->setMaximumBlockCount(50000);
    setOpenExternalLinks(false);
    setOpenLinks(false);
    setMinimumHeight(1);
    setMaximumHeight(1);
    setFont(pc->terminalfont);
    CADTreeView *tview = ((CADApp *)qApp)->cadtreeview;
    if (tview) {
	QObject::connect(this, SIGNAL(anchorClicked(const QUrl &)), tview, SLOT(expand_link(const QUrl &)));
    }
}

void ConsoleLog::resizeEvent(QResizeEvent *e)
{
    QTextBrowser::resizeEvent(e);
    setMinimumHeight(document()->size().height());
    setMaximumHeight(document()->size().height());
}

void ConsoleLog::keyPressEvent(QKeyEvent *e)
{
    switch(e->key())
    {
	default:
	    e->accept();
	    parent_console->input->keyPressEvent(e);
	    break;
    }
}


void ConsoleLog::append_results(const QString &results)
{
    QMutexLocker locker(&writemutex);
    textCursor().movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    if (results.length()) {
	textCursor().insertText(results.trimmed());
    } else {
	textCursor().insertHtml("<a href=\"/pinewood/tire1.r/outer-tire.c/wheel-outer-bump.s\">/pinewood/tire1.r/outer-tire.c/wheel-outer-bump.s</a><br><a href=\"/pinewood/tire4.r/inner-cones.c\">/pinewood/tire4.r/inner-cones.c</a><br><a href=\"/pinewood/axel2.r/axel-cut-2.c/axel-cutout-2a.s\">/pinewood/axel2.r/axel-cut-2.c/axel-cutout-2a.s</a><br><a href=\"/pinewood/pinewood_car_body.r/side-cut-left.s\">/pinewood/pinewood_car_body.r/side-cut-left.s</a>");
    }
    textCursor().insertHtml("<br>");
    // Update the size of the log
    setMinimumHeight(document()->size().height());
    setMaximumHeight(document()->size().height());
    int scrollbar_width = parent_console->scrollarea->verticalScrollBar()->size().width();
    setMinimumWidth(parent_console->scrollarea->size().width() - 2*scrollbar_width);
    setMaximumWidth(parent_console->scrollarea->size().width() - 2*scrollbar_width);
    locker.unlock();
    // Once we've added the result, this log's size is now fixed and will not change.
    // This lets us display large logs while minimizing rendering performance issues.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}


Console::Console(QWidget *pparent) : QWidget(pparent)
{


    QVBoxLayout *mlayout = new QVBoxLayout(pparent);
    scrollarea= new QScrollArea();
    scrollarea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    mlayout->addWidget(scrollarea);
    mlayout->setSpacing(5);
    mlayout->setContentsMargins(5,5,5,5);


    QWidget *contents = new QWidget;
    vlayout = new QVBoxLayout(contents);
    vlayout->setSpacing(0);
    vlayout->setContentsMargins(0,0,0,0);

    input = new ConsoleInput(this, this);

    int font_id = QFontDatabase::addApplicationFont(":/fonts/Inconsolata.otf");
    QString family = QFontDatabase::applicationFontFamilies(font_id).at(0);
    terminalfont = QFont(family);
    terminalfont.setStyleHint(QFont::Monospace);
    terminalfont.setPointSize(10);
    terminalfont.setFixedPitch(true);
    input->setFont(terminalfont);

    setFocusProxy(input);

    vlayout->addWidget(input);
    vlayout->setSizeConstraint(QLayout::SetMinimumSize);

    scrollarea->setWidget(contents);
    scrollarea->setWidgetResizable(true);

    this->setLayout(mlayout);

    QScrollBar *scrollbar = scrollarea->verticalScrollBar();
    connect(scrollbar, SIGNAL(rangeChanged(int, int)), this, SLOT(do_update_scrollbars(int, int)));

    console_prompt = QString("console> ");
}

Console::~Console()
{
    delete input;
    foreach(ConsoleLog *rlog, logs) {
	delete rlog;
    }
}


void Console::keyPressEvent(QKeyEvent *e)
{
    switch(e->key())
    {
	default:
	    e->accept();
	    scrollarea->verticalScrollBar()->setValue(scrollarea->verticalScrollBar()->maximum());
	    input->keyPressEvent(e);
	    break;
    }
}

void Console::prompt(QString new_prompt)
{
    console_prompt = new_prompt;
    input->clear();
    input->insertHtml(console_prompt);
    input->anchor_pos = input->textCursor().position();
    input->setMinimumHeight(input->document()->size().height());
    input->setMaximumWidth(scrollarea->width());
    int scrollbar_width = scrollarea->verticalScrollBar()->size().width();
    int wwidth = size().width() - 2*scrollbar_width;
    if (wwidth > 0) {
	input->setMinimumWidth(wwidth);
	input->setMaximumWidth(wwidth);
    }
}


void Console::do_update_scrollbars(int min, int max)
{
    Q_UNUSED(min);
    scrollarea->verticalScrollBar()->setValue(max);
}

void Console::resizeEvent(QResizeEvent *e)
{
    input->setMinimumWidth(0);
    input->setMaximumWidth(0);
    input->resizeEvent(e);
    int scrollbar_width = scrollarea->verticalScrollBar()->size().width();
    int wwidth = size().width() - 2*scrollbar_width;
    if (wwidth > 0) {
	input->setMinimumWidth(wwidth);
	input->setMaximumWidth(wwidth);
    }
    scrollarea->verticalScrollBar()->setValue(scrollarea->verticalScrollBar()->maximum());
}

void Console::mouseMoveEvent(QMouseEvent *e)
{
    if (input->geometry().contains(e->pos())) {
	setFocusProxy(input);
    }
    foreach(ConsoleLog *rlog, logs) {
	if (rlog->geometry().contains(e->pos())) {
	    setFocusProxy(rlog);
	}
    }
}

// TODO - keep mouse selections out of prompt area, so we can't mess
// up the prompt string.


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
