#include <iostream>
#include "console.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>

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

void ConsoleInput::keyPressEvent(QKeyEvent *e)
{
    QTextCursor itc= textCursor();
    QString buffer;
    QString command;
    int end_pos;
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
		if (command.length() == 0)
		    parent_console->log->insertHtml("<br>");
	    }

	    buffer.append("<html><body><head>");
	    buffer.append("</head><p>\n");
	    buffer.append(parent_console->console_prompt);
	    buffer.append(command.trimmed());
	    buffer.append("</p></body></html>");
	    parent_console->log->insertHtml(buffer);

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
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    log->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    log->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);


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
    if (results.length()) {
	QString buffer;
	buffer.clear();
	buffer.append("<html><head>");
	buffer.append("<style>pre { display: block; white-space pre; margin: 1em 0; } </style>");
	buffer.append("</head><body><pre>\n\n");
	buffer.append(results.trimmed());
	buffer.append("\n</pre></body></html>");
	log->append(buffer);
	buffer.clear();
	buffer.append("<html><head>");
	buffer.append("<style>p { line-height: 10% } </style>");
	buffer.append("</head><body><p>\n</p></body></html>");
	log->append(buffer);

    }
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
