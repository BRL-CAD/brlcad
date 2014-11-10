#include <iostream>
#include "console.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QFontDatabase>
#include <QFont>

ConsoleInput::ConsoleInput(QWidget *pparent) : QTextEdit(pparent)
{
    append("console>");
    anchor_pos = textCursor().position();
    document()->setDocumentMargin(0);
    setMinimumHeight(document()->size().height());
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


ConsoleLog::ConsoleLog(QWidget *pparent) : QTextBrowser(pparent)
{
    is_empty = 1;
    offset = 0;
    document()->setDocumentMargin(0);
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

    QWidget *contents = new QWidget;
    QVBoxLayout *vlayout = new QVBoxLayout(contents);
    vlayout->setSpacing(0);
    vlayout->setContentsMargins(0,0,0,0);

    input = new ConsoleInput(this);
    log = new ConsoleLog(this);
    input->setFrameStyle(QFrame::NoFrame);
    log->setFrameStyle(QFrame::NoFrame);

    log->parent_console = this;
    input->parent_console = this;

    log->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    log->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    log->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    log->document()->setMaximumBlockCount(1000);

    int font_id = QFontDatabase::addApplicationFont(":/fonts/ProFont.ttf");
    QString family = QFontDatabase::applicationFontFamilies(font_id).at(0);
    QFont profont(family);
    profont.setStyleHint(QFont::Monospace);
    profont.setPointSize(9);
    profont.setFixedPitch(true);
    log->setFont(profont);
    input->setFont(profont);

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
    QMutexLocker locker(&writemutex);
    switch(e->key())
    {
	default:
	    e->accept();
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
    QMutexLocker locker(&writemutex);
    log->textCursor().movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    if (results.length()) {
	log->textCursor().insertText(results.trimmed());
    }
    log->textCursor().insertHtml("<br>");
    // Update the size of the log
    log->setMinimumHeight(log->document()->size().height());
    log->setMaximumHeight(log->document()->size().height());
}


void Console::do_update_scrollbars(int min, int max)
{
    Q_UNUSED(min);
    scrollarea->verticalScrollBar()->setValue(max);

    //scrollarea->ensureVisible(0, scrollAreaHeight, 0, 0);
}

void Console::resizeEvent(QResizeEvent *e)
{
    log->resizeEvent(e);
    input->resizeEvent(e);
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
