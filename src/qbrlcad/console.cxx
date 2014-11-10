#include <iostream>
#include "console.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QFontDatabase>
#include <QFont>

ConsoleInput::ConsoleInput(QWidget *pparent, Console *pc) : QTextEdit(pparent)
{
    append("console>");
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
    QMutexLocker locker(&parent_console->log->writemutex);

    //capture command text
    end_pos = textCursor().position();
    itc.setPosition(anchor_pos, QTextCursor::MoveAnchor);
    itc.setPosition(end_pos, QTextCursor::KeepAnchor);
    command = itc.selectedText();

    // Add the command to the log
    if (parent_console->log->is_empty) {
	parent_console->log->clear();
	parent_console->log->is_empty = 0;
    } else {
	parent_console->log->insertHtml("<br>");
    }

    parent_console->log->textCursor().insertHtml(parent_console->console_prompt);
    parent_console->log->textCursor().insertHtml(command.trimmed());
    parent_console->log->textCursor().insertHtml("<br>");

    // Update the size of the log
    parent_console->log->setMinimumHeight(parent_console->log->document()->size().height());
    parent_console->log->setMaximumHeight(parent_console->log->document()->size().height());

    locker.unlock();

    // Reset the input buffer
    clear();
    insertHtml(parent_console->console_prompt);
    itc= textCursor();
    anchor_pos = itc.position();
    setMinimumHeight(document()->size().height());

    // Emit the command - up to the application to run it and return results
    emit parent_console->executeCommand(command);
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
	    QTextEdit::keyPressEvent(e);
	    setMinimumHeight(document()->size().height());
	    break;
    }
}


ConsoleLog::ConsoleLog(QWidget *pparent, Console *pc) : QTextBrowser(pparent)
{
    is_empty = 1;
    offset = 0;
    document()->setDocumentMargin(0);
    parent_console = pc;
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    document()->setMaximumBlockCount(5000);
    setWordWrapMode(QTextOption::NoWrap);
    setOpenExternalLinks(false);
    setOpenLinks(false);
}

void ConsoleLog::resizeEvent(QResizeEvent *e)
{
    QTextBrowser::resizeEvent(e);
    if (!is_empty) {
	setMinimumHeight(document()->size().height() - offset);
	setMaximumHeight(document()->size().height() - offset);
    } else {
	setMinimumHeight(1);
	setMaximumHeight(1);
    }
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

Console::Console(QWidget *pparent) : QWidget(pparent)
{


    QVBoxLayout *mlayout = new QVBoxLayout(pparent);
    scrollarea= new QScrollArea();

    mlayout->addWidget(scrollarea);
    mlayout->setSpacing(5);
    mlayout->setContentsMargins(5,5,5,5);


    QWidget *contents = new QWidget;
    vlayout = new QVBoxLayout(contents);
    vlayout->setSpacing(0);
    vlayout->setContentsMargins(0,0,0,0);

    input = new ConsoleInput(this, this);
    log = new ConsoleLog(this, this);

    int font_id = QFontDatabase::addApplicationFont(":/fonts/Inconsolata.otf");
    QString family = QFontDatabase::applicationFontFamilies(font_id).at(0);
    QFont terminalfont(family);
    terminalfont.setStyleHint(QFont::Monospace);
    terminalfont.setPointSize(10);
    terminalfont.setFixedPitch(true);
    log->setFont(terminalfont);
    input->setFont(terminalfont);

    setFocusProxy(input);

    vlayout->addWidget(log);
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
    delete log;
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
}

void Console::append_results(const QString &results)
{
    QMutexLocker locker(&log->writemutex);
    log->textCursor().movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    if (results.length()) {
	log->textCursor().insertText(results.trimmed());
    } else {
	log->textCursor().insertHtml("<a href=\"/pinewood/tire1.r/outer-tire.c/wheel-outer-bump.s\">/pinewood/tire1.r/outer-tire.c/wheel-outer-bump.s</a><br><a href=\"/pinewood/tire4.r/inner-cones.c\">/pinewood/tire4.r/inner-cones.c</a><br><a href=\"/pinewood/axel2.r/axel-cut-2.c/axel-cutout-2a.s\">/pinewood/axel2.r/axel-cut-2.c/axel-cutout-2a.s</a><br><a href=\"/pinewood/pinewood_car_body.r/side-cut-left.s\">/pinewood/pinewood_car_body.r/side-cut-left.s</a>");
    }
    log->textCursor().insertHtml("<br>");
    // Update the size of the log
    log->setMinimumHeight(log->document()->size().height());
    log->setMaximumHeight(log->document()->size().height());
    locker.unlock();
}


void Console::do_update_scrollbars(int min, int max)
{
    Q_UNUSED(min);
    scrollarea->verticalScrollBar()->setValue(max);
}

void Console::resizeEvent(QResizeEvent *e)
{
    log->resizeEvent(e);
    input->resizeEvent(e);
    scrollarea->verticalScrollBar()->setValue(scrollarea->verticalScrollBar()->maximum());
}

void Console::mouseMoveEvent(QMouseEvent *e)
{
    if (input->geometry().contains(e->pos())) {
	setFocusProxy(input);
    }
    if (log->geometry().contains(e->pos())) {
	setFocusProxy(log);
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
