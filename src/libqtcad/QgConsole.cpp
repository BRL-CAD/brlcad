/*
 * Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
 All rights reserved.
 *
 * Sandia National Laboratories, New Mexico PO Box 5800 Albuquerque, NM 87185
 *
 * Kitware Inc.
 * 28 Corporate Drive
 * Clifton Park, NY 12065
 * USA
 *
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive license
 * for use of this work by or on behalf of the U.S. Government.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of Kitware nor the names of any contributors may be used
 *      to endorse or promote products derived from this software without specific
 *      prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 * This widget is based off of ParaView's QgConsole
 */

#include "common.h"

#include "qtcad/QgConsole.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QCompleter>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPointer>
#include <QStringListModel>
#include <QTextCursor>
#include <QTimer>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QtGlobal>

#include "bu.h"
#include "ged.h"

static int
qstring_pos_from_local8bit_offset(const QByteArray& bytes, size_t offset)
{
    if (offset > (size_t)bytes.size())
	offset = (size_t)bytes.size();

    return QString::fromLocal8Bit(bytes.constData(), (int)offset).length();
}

GEDShellCompleter::GEDShellCompleter(
	QWidget* parent, struct ged *ged_ptr)
{
    setParent(parent);
    gedp = ged_ptr;
}

void
GEDShellCompleter::updateCompletionModel(const QString& console_txt)
{
    setModel(NULL);
    replace_start = -1;
    replace_end = -1;

    if (console_txt.isEmpty())
	return;

    QByteArray cbytes = console_txt.toLocal8Bit();
    struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
    int completion_cnt = ged_cmd_complete_result(gedp, cbytes.constData(), (size_t)cbytes.size(), &result);
    if (completion_cnt <= 0) {
	ged_cmd_completion_result_clear(&result);
	return;
    }

    QStringList clist = QStringList();
    for (int i = 0; i < completion_cnt; i++) {
	clist.append(QString::fromLocal8Bit(result.completion_candidates[i]));
    }

    replace_start = qstring_pos_from_local8bit_offset(cbytes, result.replacement_start);
    replace_end = qstring_pos_from_local8bit_offset(cbytes, result.replacement_end);

    setCompletionMode(QCompleter::PopupCompletion);
    setModel(new QStringListModel(clist, this));
    setCaseSensitivity(Qt::CaseSensitive);
    setCompletionPrefix(QString::fromLocal8Bit(result.prefix ? result.prefix : ""));
    if (popup())
	popup()->setCurrentIndex(completionModel()->index(0, 0));

    ged_cmd_completion_result_clear(&result);
}

void
GEDShellCompleter::analyze(const QString& console_txt, std::vector<QgConsoleHighlight>& highlights)
{
    highlights.clear();
    if (!gedp || console_txt.isEmpty())
	return;

    QByteArray cbytes = console_txt.toLocal8Bit();
    struct ged_cmd_analysis analysis = {0, NULL};
    if (ged_cmd_analyze(gedp, cbytes.constData(), &analysis) != 0)
	return;

    for (size_t i = 0; i < analysis.token_count; i++) {
	const struct ged_cmd_analysis_token *token = &analysis.tokens[i];
	QgConsoleHighlight h;
	if (token->char_end <= token->char_start)
	    continue;
	h.start = qstring_pos_from_local8bit_offset(cbytes, token->char_start);
	h.end = qstring_pos_from_local8bit_offset(cbytes, token->char_end);
	if (token->semantic_state == GED_CMD_SEMANTIC_INVALID)
	    h.style = QG_CONSOLE_INVALID;
	else if (token->semantic_state == GED_CMD_SEMANTIC_INCOMPLETE ||
		token->semantic_state == GED_CMD_SEMANTIC_PENDING)
	    h.style = QG_CONSOLE_INCOMPLETE;
	else if (token->role == GED_CMD_TOKEN_COMMAND || token->role == GED_CMD_TOKEN_SUBCOMMAND)
	    h.style = QG_CONSOLE_COMMAND;
	else if (token->role == GED_CMD_TOKEN_OPTION)
	    h.style = QG_CONSOLE_OPTION;
	else if (token->semantic_state == GED_CMD_SEMANTIC_VALID &&
		(token->value_type == BU_CMD_VALUE_DB_OBJECT || token->value_type == BU_CMD_VALUE_DB_PATH))
	    h.style = QG_CONSOLE_VALID;
	else
	    continue;
	highlights.push_back(h);
    }
    ged_cmd_analysis_clear(&analysis);
}

extern "C" int
qg_console_log_hook(void *console_data, void *log_data)
{
    QgConsole *console = (QgConsole *)console_data;
    const char *output = (const char *)log_data;
    if (!console || !output)
	return 0;

    Q_EMIT console->queued_log(QString::fromLocal8Bit(output));
    return (int)strlen(output);
}

extern "C" int
qg_ged_search_exec_callback(int argc, const char **argv, void *ged_data, void *console_data)
{
    struct ged *gedp = (struct ged *)ged_data;
    QgConsole *console = (QgConsole *)console_data;
    if (!gedp || argc < 1 || !argv)
	return 0;

    struct bu_vls saved = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&saved, "%s", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);
    gedp->ged_skip_clbks++;
    int ret = ged_exec(gedp, argc, argv);
    gedp->ged_skip_clbks--;

    if (bu_vls_strlen(gedp->ged_result_str)) {
	const char *output = bu_vls_cstr(gedp->ged_result_str);
	size_t olen = bu_vls_strlen(gedp->ged_result_str);
	QString qoutput = QString::fromLocal8Bit(output);
	if (!olen || output[olen - 1] != '\n')
	    qoutput.append('\n');
	if (console)
	    qg_console_log_hook((void *)console, (void *)qoutput.toLocal8Bit().constData());
	else
	    bu_log("%s", qoutput.toLocal8Bit().constData());
    }
    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&saved));
    bu_vls_free(&saved);
    return (ret == BRLCAD_OK) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////
// QgConsole::pqImplementation

class QgConsole::pqImplementation :
    public QPlainTextEdit
{
    public:
	pqImplementation(QgConsole& p) :
	    QPlainTextEdit(&p),
	    Parent(p),
	    InteractivePosition(documentEnd())
    {
	bu_lineedit_palette_init(&LineeditPalette);
	(void)bu_lineedit_palette_load_user(&LineeditPalette);
	this->setTabChangesFocus(false);
	this->setAcceptDrops(false);
	this->setUndoRedoEnabled(false);
	this->setMinimumHeight(200);
	this->setMaximumBlockCount(10000);

	QFont f("Courier");
	f.setStyleHint(QFont::TypeWriter);
	f.setFixedPitch(true);
	this->setFont(f);

	this->CommandHistory.append("");
	this->CommandPosition = 0;
    }

	void applyExtraSelections()
	{
	    QList<QTextEdit::ExtraSelection> selections = SemanticSelections;
	    selections.append(CompletionSelections);
	    setExtraSelections(selections);
	}

	QColor semanticColor(QgConsoleHighlightStyle style) const
	{
	    bu_lineedit_role_t role = BU_LINEEDIT_ROLE_COUNT;
	    switch (style) {
		case QG_CONSOLE_COMMAND: role = BU_LINEEDIT_ROLE_COMMAND; break;
		case QG_CONSOLE_OPTION: role = BU_LINEEDIT_ROLE_OPTION; break;
		case QG_CONSOLE_VALID: role = BU_LINEEDIT_ROLE_VALID; break;
		case QG_CONSOLE_INVALID: role = BU_LINEEDIT_ROLE_INVALID; break;
		case QG_CONSOLE_INCOMPLETE: role = BU_LINEEDIT_ROLE_INCOMPLETE; break;
	    }
	    bool dark = palette().color(QPalette::Base).lightness() < 128;
	    QColor color;
	    switch (style) {
		case QG_CONSOLE_COMMAND: color = dark ? QColor(90, 220, 110) : QColor(0, 125, 25); break;
		case QG_CONSOLE_OPTION: color = dark ? QColor(235, 205, 75) : QColor(155, 105, 0); break;
		case QG_CONSOLE_VALID: color = dark ? QColor(70, 215, 220) : QColor(0, 125, 135); break;
		case QG_CONSOLE_INVALID: color = dark ? QColor(255, 105, 105) : QColor(190, 25, 25); break;
		case QG_CONSOLE_INCOMPLETE: color = dark ? QColor(225, 120, 225) : QColor(150, 45, 150); break;
	    }
	    if (!color.isValid())
		color = palette().color(QPalette::Text);
	    if (role != BU_LINEEDIT_ROLE_COUNT) {
		const struct bu_lineedit_style &configured = LineeditPalette.roles[role];
		if (configured.flags & BU_LINEEDIT_STYLE_COLOR)
		    color = QColor(configured.rgb[0], configured.rgb[1], configured.rgb[2]);
		if ((configured.flags & (BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) ==
		    (BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM))
		    color.setAlphaF(0.45);
	    }
	    return color;
	}

	void updateSemanticSelections()
	{
	    SemanticSelections.clear();
	    if (Completer) {
		std::vector<QgConsoleHighlight> highlights;
		Completer->analyze(commandBuffer(), highlights);
		for (const QgConsoleHighlight &h : highlights) {
		    if (h.end <= h.start || h.start < 0 || h.end > commandBuffer().size())
			continue;
		    QTextEdit::ExtraSelection selection;
		    selection.cursor = QTextCursor(document());
		    selection.cursor.setPosition(InteractivePosition + h.start);
		    selection.cursor.setPosition(InteractivePosition + h.end, QTextCursor::KeepAnchor);
		    selection.format.setForeground(semanticColor(h.style));
		    SemanticSelections.append(selection);
		}
	    }
	    applyExtraSelections();
	}

	void clearCompletionState(bool hide_popup = true)
	{
	    CompletionActive = false;
	    CompletionBase.clear();
	    CompletionBaseCursor = 0;
	    CompletionIndex = -1;
	    if (hide_popup && this->Completer && this->Completer->popup())
		this->Completer->popup()->hide();
	    if (CompletionDisplay)
		CompletionDisplay->hide();
	    CompletionSelections.clear();
	    applyExtraSelections();
	}

	void updateCompletionDisplay()
	{
	    if (CompletionDisplay)
		CompletionDisplay->hide();
	    if (!Completer || !Completer->completionModel() || Completer->completionCount() < 2)
		return;

	    std::vector<QByteArray> encoded;
	    std::vector<const char *> candidates;
	    encoded.reserve((size_t)Completer->completionCount());
	    candidates.reserve((size_t)Completer->completionCount());
	    for (int i = 0; i < Completer->completionCount(); i++) {
		encoded.push_back(completionAt(i).toLocal8Bit());
		candidates.push_back(encoded.back().constData());
	    }

	    QFontMetrics metrics(font());
	    int cell_width = metrics.horizontalAdvance(QLatin1Char('0'));
	    size_t columns = (cell_width > 0) ?
		(size_t)std::max(20, viewport()->width() / cell_width) : 80;
	    struct bu_cmd_completion_layout layout = BU_CMD_COMPLETION_LAYOUT_INIT_ZERO;
	    if (bu_cmd_completion_layout_create(&layout, candidates.data(), candidates.size(),
		    columns, 5) != BRLCAD_OK || !layout.summarized || !layout.line_count) {
		bu_cmd_completion_layout_clear(&layout);
		return;
	    }

	    QStringList lines;
	    for (size_t i = 0; i < layout.line_count; i++)
		lines.append(QString::fromLocal8Bit(layout.lines[i]));
	    bu_cmd_completion_layout_clear(&layout);
	    if (!CompletionDisplay) {
		CompletionDisplay = new QLabel(viewport());
		CompletionDisplay->setObjectName(QStringLiteral("completionCandidateDisplay"));
		CompletionDisplay->setTextFormat(Qt::PlainText);
		CompletionDisplay->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		CompletionDisplay->setFrameStyle(QFrame::Box | QFrame::Plain);
		CompletionDisplay->setMargin(3);
		CompletionDisplay->setAutoFillBackground(true);
	    }
	    QPalette display_palette = CompletionDisplay->palette();
	    display_palette.setColor(QPalette::Window, palette().color(QPalette::Base));
	    const struct bu_lineedit_style &configured =
		LineeditPalette.roles[BU_LINEEDIT_ROLE_COMPLETION_PREVIEW];
	    QColor display_color = (configured.flags & BU_LINEEDIT_STYLE_COLOR) ?
		QColor(configured.rgb[0], configured.rgb[1], configured.rgb[2]) :
		palette().color(QPalette::Text);
	    display_palette.setColor(QPalette::WindowText, display_color);
	    CompletionDisplay->setPalette(display_palette);
	    CompletionDisplay->setText(lines.join(QLatin1Char('\n')));
	    CompletionDisplay->adjustSize();

	    QRect cursor_box = cursorRect();
	    int x = cursor_box.left();
	    int y = cursor_box.bottom() + 4;
	    if (x + CompletionDisplay->width() > viewport()->width())
		x = viewport()->width() - CompletionDisplay->width();
	    if (y + CompletionDisplay->height() > viewport()->height())
		y = cursor_box.top() - CompletionDisplay->height() - 4;
	    CompletionDisplay->move(std::max(0, x), std::max(0, y));
	    CompletionDisplay->show();
	    CompletionDisplay->raise();
	    if (Completer->popup())
		Completer->popup()->hide();
	}

	void showCompletionPreview(int start, int end)
	{
	    CompletionSelections.clear();
	    if (end > start) {
		QTextEdit::ExtraSelection preview;
		preview.cursor = QTextCursor(document());
		preview.cursor.setPosition(InteractivePosition + start);
		preview.cursor.setPosition(InteractivePosition + end, QTextCursor::KeepAnchor);
		const struct bu_lineedit_style &configured =
		    LineeditPalette.roles[BU_LINEEDIT_ROLE_COMPLETION_PREVIEW];
		QColor preview_color = (configured.flags & BU_LINEEDIT_STYLE_COLOR) ?
		    QColor(configured.rgb[0], configured.rgb[1], configured.rgb[2]) :
		    palette().color(QPalette::Text);
		bool dim = true;
		if (configured.flags & BU_LINEEDIT_STYLE_DIM_SET)
		    dim = (configured.flags & BU_LINEEDIT_STYLE_DIM) != 0;
		preview_color.setAlphaF(dim ? 0.45 : 1.0);
		preview.format.setForeground(preview_color);
		CompletionSelections.append(preview);
	    }
	    applyExtraSelections();
	}

	void restoreCompletionBase()
	{
	    if (!CompletionActive)
		return;
	    replaceCommandBuffer(CompletionBase);
	    QTextCursor c = textCursor();
	    c.setPosition(InteractivePosition + CompletionBaseCursor);
	    setTextCursor(c);
	}

	bool filterEditExtendsPreview(const QString &edit)
	{
	    if (!CompletionActive || !Completer || edit.isEmpty())
		return false;

	    QString original = CompletionBase.left(CompletionBaseCursor) + edit;
	    Completer->updateCompletionModel(original);
	    if (Completer->completionCount() > 0)
		return false;

	    int preview_cursor = textCursor().position() - InteractivePosition;
	    QString preview = commandBuffer().left(preview_cursor) + edit;
	    Completer->updateCompletionModel(preview);
	    return Completer->completionCount() > 0;
	}

	QString completionAt(int index) const
	{
	    if (!this->Completer || !this->Completer->completionModel() || index < 0)
		return QString();
	    return this->Completer->completionModel()->index(index, 0).data().toString();
	}

	bool insertCompletionAtReplacement(const QString &completion)
	{
	    if (!this->Completer)
		return false;
	    int rstart = this->Completer->completionReplacementStart();
	    int rend = this->Completer->completionReplacementEnd();
	    int clen = commandBuffer().length();
	    if (rstart < 0 || rend < rstart || rstart > clen || rend > clen)
		return false;

	    QTextCursor rtc(document());
	    rtc.setPosition(InteractivePosition + rstart, QTextCursor::MoveAnchor);
	    rtc.setPosition(InteractivePosition + rend, QTextCursor::KeepAnchor);
	    rtc.insertText(completion);
	    setTextCursor(rtc);
	    updateCommandBuffer();
	    return true;
	}

	void previewCompletion(int direction = 1)
	{
	    if (!this->Completer)
		return;

	    if (!CompletionActive) {
		CompletionBase = commandBuffer();
		CompletionBaseCursor = textCursor().position() - InteractivePosition;
		updateCompleter();
		if (this->Completer->completionCount() <= 0)
		    return;
		CompletionActive = true;
		CompletionIndex = (direction < 0) ? this->Completer->completionCount() - 1 : 0;
	    } else {
		int count = this->Completer->completionCount();
		if (count <= 0) {
		    clearCompletionState();
		    return;
		}
		CompletionIndex = (CompletionIndex + direction + count) % count;
	    }

	    restoreCompletionBase();
	    QString candidate = completionAt(CompletionIndex);
	    if (!candidate.isNull()) {
		int rstart = this->Completer->completionReplacementStart();
		int rend = this->Completer->completionReplacementEnd();
		QString typed = CompletionBase.mid(rstart, rend - rstart);
		int confirmed = 0;
		while (confirmed < typed.size() && confirmed < candidate.size() &&
			typed[confirmed] == candidate[confirmed])
		    confirmed++;
		insertCompletionAtReplacement(candidate);
		showCompletionPreview(rstart + confirmed, rstart + candidate.size());
	    }
	    if (this->Completer->popup())
		this->Completer->popup()->setCurrentIndex(this->Completer->completionModel()->index(CompletionIndex, 0));
	    updateCompletionDisplay();
	}

	void prefixCompletion()
	{
	    if (!this->Completer)
		return;
	    updateCompleter();
	    int count = this->Completer->completionCount();
	    if (count <= 0)
		return;
	    QString common = completionAt(0);
	    for (int i = 1; i < count && !common.isEmpty(); i++) {
		QString candidate = completionAt(i);
		int j = 0;
		while (j < common.size() && j < candidate.size() && common[j] == candidate[j])
		    j++;
		common.truncate(j);
	    }
	    if (!common.isEmpty())
		Parent.insertCompletion(common);
	    clearCompletionState();
	}

	void setFont(const QFont& i_font)
	{
	    QPlainTextEdit::setFont(i_font);
	}

	QFont getFont()
	{
	    return this->font();
	}

	// TODO - figure out how to implement this...
	bool consolidateHistory(size_t start, size_t end)
	{
	    if (start > end)
		return false;
	    QString nline;
	    for (size_t i = start; i < end; i++) {
		nline.append(CommandHistory.at(i));
		if (i != end - 1)
		    nline.append(" ");
	    }
	    while (CommandHistory.count() > (int)start) {
		CommandHistory.pop_back();
	    }
	    CommandHistory.push_back(nline);
	    CommandHistory.push_back("");
	    CommandPosition = CommandHistory.size() - 1;
	    return true;
	}

	std::string historyAt(size_t ind)
	{
	    const char *cmd = CommandHistory.at(ind).toLocal8Bit().data();
	    std::string scmd(cmd);
	    return scmd;
	}

	// Try to keep the scrollbar slider from getting too small to be usable
	void resizeEvent(QResizeEvent *e)
	{
	    this->setMaximumBlockCount(2*this->height());
	    QPlainTextEdit::resizeEvent(e);
	    if (CompletionDisplay && CompletionDisplay->isVisible())
		updateCompletionDisplay();
	}

	void insertFromMimeData(const QMimeData * s)
	{
	    QTextCursor text_cursor = this->textCursor();

	    // Set to true if the cursor overlaps the history area
	    const bool history_area =
		text_cursor.anchor() < this->InteractivePosition
		|| text_cursor.position() < this->InteractivePosition;

	    // Avoid pasting into history
	    if (history_area) {
		return;
	    }

	    QPlainTextEdit::insertFromMimeData(s);

	    // The text changed - make sure the command buffer knows
	    this->updateCommandBuffer();
	}

	void keyPressEvent(QKeyEvent* e)
	{
	    if (this->Completer && this->Completer->popup()->isVisible()) {
		// The following keys are forwarded by the completer to the widget
		switch (e->key()) {
		case Qt::Key_Enter:
		case Qt::Key_Return:
		    if (CompletionMode != BU_CMD_COMPLETE_LEGACY && CompletionActive) {
			QModelIndex selected = this->Completer->popup()->currentIndex();
			QString candidate = selected.isValid() ? selected.data().toString() : QString();
			if (!candidate.isEmpty())
			    Parent.insertCompletion(candidate);
			else
			    clearCompletionState();
			e->accept();
			return;
		    }
			e->ignore();
			return; // let the completer do default behavior
		    case Qt::Key_Tab:
		    case Qt::Key_Backtab:
			if (CompletionMode != BU_CMD_COMPLETE_LEGACY)
			    break;
			e->ignore();
			return; // let the completer do default behavior
		    case Qt::Key_Escape:
			if (CompletionActive)
			    restoreCompletionBase();
			clearCompletionState();
			e->accept();
			return;
		    default:
			break;
		}
	    }

	if (CompletionActive && e->key() != Qt::Key_Tab && e->key() != Qt::Key_Backtab) {
	    if (CompletionMode == BU_CMD_COMPLETE_FILTER && e->key() != Qt::Key_Space &&
		    e->key() != Qt::Key_Slash &&
		    e->key() != Qt::Key_Backspace && e->key() != Qt::Key_Delete &&
		    e->key() != Qt::Key_Return && e->key() != Qt::Key_Enter) {
		/* Prefer refining the original seed.  When that produces no
		 * candidates, retain the preview if appending this text to the
		 * selected candidate does produce a valid completion. */
		if (!filterEditExtendsPreview(e->text()))
		    restoreCompletionBase();
	    }
	    clearCompletionState();
	}

	    QTextCursor text_cursor = this->textCursor();

	    // Set to true if there's a current selection
	    const bool selection = text_cursor.anchor() != text_cursor.position();
	    // Set to true if the cursor overlaps the history area
	    const bool history_area =
		text_cursor.anchor() < this->InteractivePosition
		|| text_cursor.position() < this->InteractivePosition;

	    // Allow copying anywhere in the console ...
	    if (e->key() == Qt::Key_C && e->modifiers() == Qt::ControlModifier) {
		if (selection) {
		    this->copy();
		}

		e->accept();
		return;
	    }

	    // Allow cut only if the selection is limited to the interactive area ...
	    if (e->key() == Qt::Key_X && e->modifiers() == Qt::ControlModifier) {
		if (selection && !history_area) {
		    this->cut();
		}

		e->accept();
		return;
	    }

	    // Allow paste only if the selection is in the interactive area ...
	    if (e->key() == Qt::Key_V && e->modifiers() == Qt::ControlModifier) {
		if (!history_area) {
		    const QMimeData* const clipboard = QApplication::clipboard()->mimeData();
		    const QString text = clipboard->text();
		    if (!text.isNull()) {
			text_cursor.insertText(text);
			this->updateCommandBuffer();
		    }
		}

		e->accept();
		return;
	    }

	    // Force the cursor back to the interactive area
	    if (history_area && e->key() != Qt::Key_Control) {
		text_cursor.setPosition(this->documentEnd());
		this->setTextCursor(text_cursor);
	    }

	    switch (e->key()) {
		case Qt::Key_Up:
		    e->accept();
		    if (this->CommandPosition > 0) {
			this->replaceCommandBuffer(this->CommandHistory[--this->CommandPosition]);
		    }
		    break;

		case Qt::Key_Down:
		    e->accept();
		    if (this->CommandPosition < this->CommandHistory.size() - 2) {
			this->replaceCommandBuffer(this->CommandHistory[++this->CommandPosition]);
		    } else {
			this->CommandPosition = this->CommandHistory.size()-1;
			this->replaceCommandBuffer("");
		    }
		    break;

		case Qt::Key_Left:
		    if (text_cursor.position() > this->InteractivePosition) {
			QPlainTextEdit::keyPressEvent(e);
		    } else {
			e->accept();
		    }
		    break;


		case Qt::Key_Delete:
		    e->accept();
		    QPlainTextEdit::keyPressEvent(e);
		    this->updateCommandBuffer();
		    break;

		case Qt::Key_Backspace:
		    e->accept();
		    if (text_cursor.position() > this->InteractivePosition) {
			QPlainTextEdit::keyPressEvent(e);
			this->updateCommandBuffer();
			this->updateCompleterIfVisible();
		    }
		    break;

		case Qt::Key_Tab:
		    e->accept();
		    if (CompletionMode == BU_CMD_COMPLETE_OFF)
			break;
		    if (CompletionMode == BU_CMD_COMPLETE_FILTER || CompletionMode == BU_CMD_COMPLETE_CYCLE) {
			previewCompletion((e->modifiers() & Qt::ShiftModifier) ? -1 : 1);
			break;
		    }
		    if (CompletionMode == BU_CMD_COMPLETE_PREFIX) {
			prefixCompletion();
			break;
		    }
		    {
			int anchor = text_cursor.anchor();
			int position = text_cursor.position();
			text_cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
			text_cursor.setPosition(position, QTextCursor::KeepAnchor);
			QString text = text_cursor.selectedText().trimmed();
			text_cursor.setPosition(anchor, QTextCursor::MoveAnchor);
			text_cursor.setPosition(position, QTextCursor::KeepAnchor);
			if (text == ">>>" || text == "...") {
			    text_cursor.insertText("    ");
			} else {
			    this->updateCompleter();
			    this->selectCompletion();
			}
		    }
		    break;

		case Qt::Key_Backtab:
		    e->accept();
		    if (CompletionMode == BU_CMD_COMPLETE_FILTER || CompletionMode == BU_CMD_COMPLETE_CYCLE)
			previewCompletion(-1);
		    break;

		case Qt::Key_Home:
		    e->accept();
		    text_cursor.setPosition(this->InteractivePosition);
		    this->setTextCursor(text_cursor);
		    break;

		case Qt::Key_Return:
		case Qt::Key_Enter:
		    e->accept();

		    text_cursor.setPosition(this->documentEnd());
		    this->setTextCursor(text_cursor);

		    this->internalExecuteCommand();
		    break;

		default:
		    e->accept();
		    QPlainTextEdit::keyPressEvent(e);
		    this->updateCommandBuffer();
		    this->updateCompleterIfVisible();
		    break;
	    }
	}

	/// Returns the end of the document
	int documentEnd()
	{
	    QTextCursor c(this->document());
	    c.movePosition(QTextCursor::End);
	    return c.position();
	}

	void focusOutEvent(QFocusEvent *e)
	{
	    QPlainTextEdit::focusOutEvent(e);

	    // For some reason the QCompleter tries to set the focus policy to
	    // NoFocus, set let's make sure we set it back to the default WheelFocus.
	    this->setFocusPolicy(Qt::WheelFocus);
	}


	void updateCompleterIfVisible()
	{
	    if (this->Completer && this->Completer->popup()->isVisible()) {
		this->updateCompleter();
	    }
	}

	/// If there is exactly 1 completion, insert it and hide the completer,
	/// else do nothing.
	void selectCompletion()
	{
	    if (this->Completer && this->Completer->completionCount() == 1) {
		this->Parent.insertCompletion(this->Completer->currentCompletion());
		this->Completer->popup()->hide();
	    }
	}

	void updateCompleter()
	{
	    if (this->Completer) {
		// Get the text between the current cursor position
		// and the start of the line
		QTextCursor text_cursor = this->textCursor();
		text_cursor.setPosition(this->InteractivePosition, QTextCursor::KeepAnchor);
		QString commandText = text_cursor.selectedText();

		// Call the completer to update the completion model
		this->Completer->updateCompletionModel(commandText);

		// Place and show the completer if there are available completions
		if (this->Completer->completionCount()) {
		    // Anchor the popup below the full cursor rectangle so it does
		    // not cover the command line being completed.
		    text_cursor = this->textCursor();
		    text_cursor.movePosition(QTextCursor::StartOfWord);
		    QRect cr = this->cursorRect(text_cursor);
		    cr.translate(0, cr.height() + 4);
		    cr.setWidth(this->Completer->popup()->sizeHintForColumn(0) +
			    this->Completer->popup()->verticalScrollBar()->sizeHint().width());
		    this->Completer->complete(cr);
		} else {
		    this->Completer->popup()->hide();
		}
	    }
	}

	/// Update the contents of the command buffer from the contents of the widget
	void updateCommandBuffer()
	{
	    this->commandBuffer() = this->toPlainText().mid(this->InteractivePosition);
	    updateSemanticSelections();
	}

	/// Replace the contents of the command buffer, updating the display
	void replaceCommandBuffer(const QString& Text)
	{
	    this->commandBuffer() = Text;

	    QTextCursor c(this->document());
	    c.setPosition(this->InteractivePosition);
	    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	    c.removeSelectedText();
	    c.insertText(Text);
	    updateSemanticSelections();
	}

	/// References the buffer where the current un-executed command is stored
	QString& commandBuffer()
	{
	    return this->CommandHistory.back();
	}

	/// Implements command-execution
	void internalExecuteCommand()
	{
	    // First update the history cache. It's essential to update the
	    // this->CommandPosition before calling internalExecuteCommand() since that
	    // can result in a clearing of the current command (BUG #8765).
	    QString command = this->commandBuffer();
	    if (!command.isEmpty()) { // Don't store empty commands in the history
		this->CommandHistory.push_back("");
		this->CommandPosition = this->CommandHistory.size() - 1;
	    }
	    QTextCursor c(this->document());
	    c.movePosition(QTextCursor::End);
	    c.insertText("\n");

	    this->InteractivePosition = this->documentEnd();
	    SemanticSelections.clear();
	    CompletionSelections.clear();
	    applyExtraSelections();
	    this->Parent.internalExecuteCommand(command);
	}

	void setCompleter(QgConsoleWidgetCompleter* completer)
	{
	    if (this->Completer) {
		this->Completer->setWidget(nullptr);
		QObject::disconnect(this->Completer, QOverload<const QString &>::of(&QCompleter::activated), &this->Parent, &QgConsole::insertCompletion);
	    }
	    this->Completer = completer;
	    if (this->Completer) {
		this->Completer->setWidget(this);
		QObject::connect(this->Completer, QOverload<const QString &>::of(&QCompleter::activated), &this->Parent, &QgConsole::insertCompletion);
	    }
	    updateSemanticSelections();
	}

	/// Stores a back-reference to our owner
	QgConsole& Parent;

	/// A custom completer
	QPointer<QgConsoleWidgetCompleter> Completer;
	QPointer<QLabel> CompletionDisplay;
	struct bu_lineedit_palette LineeditPalette;
	bu_cmd_completion_mode_t CompletionMode = BU_CMD_COMPLETE_FILTER;
	bool CompletionActive = false;
	QString CompletionBase;
	int CompletionBaseCursor = 0;
	int CompletionIndex = -1;
	QList<QTextEdit::ExtraSelection> SemanticSelections;
	QList<QTextEdit::ExtraSelection> CompletionSelections;

	/** Stores the beginning of the area of interactive input, outside which
	  changes can't be made to the text edit contents */
	int InteractivePosition;
	/// Stores command-history, plus the current command buffer
	QStringList CommandHistory;
	/// Stores the current position in the command-history
	int CommandPosition;
};

/////////////////////////////////////////////////////////////////////////
// QgConsole

QgConsole::QgConsole(QWidget* Parent) :
    QWidget(Parent),
    Implementation(new pqImplementation(*this))
{
    QVBoxLayout* const l = new QVBoxLayout(this);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // TODO - figure out what to do for Qt6 here...
    l->setMargin(0);
#endif
    l->addWidget(this->Implementation);
    QObject::connect(this, &QgConsole::queued_log, this, &QgConsole::printStringBeforePrompt);
}

//-----------------------------------------------------------------------------
QgConsole::~QgConsole()
{
    delete this->Implementation;
}

//-----------------------------------------------------------------------------
QFont QgConsole::getFont()
{
    return this->Implementation->getFont();
}

//-----------------------------------------------------------------------------
bool QgConsole::consolidateHistory(size_t start, size_t end)
{
    return this->Implementation->consolidateHistory(start, end);
}

//-----------------------------------------------------------------------------
size_t QgConsole::historyCount()
{
    return this->Implementation->CommandHistory.count();
}

//-----------------------------------------------------------------------------
std::string QgConsole::historyAt(size_t ind)
{
    return this->Implementation->historyAt(ind);
}

//-----------------------------------------------------------------------------
void QgConsole::setFont(const QFont& i_font)
{
    this->Implementation->setFont(i_font);
}

//-----------------------------------------------------------------------------
QPoint QgConsole::getCursorPosition()
{
    QTextCursor tc = this->Implementation->textCursor();

    return this->Implementation->cursorRect(tc).topLeft();
}

//-----------------------------------------------------------------------------
void QgConsole::listen(int fd, struct ged_subprocess *p, bu_process_io_t t, ged_io_func_t c, void *d)
{
    QConsoleListener *l = new QConsoleListener(fd, p, t, c, d);
    bu_log("Start listening: %d\n", (int)t);
    QObject::connect(l, &QConsoleListener::newLine, this, &QgConsole::printStringBeforePrompt);
    /* EOF may be reported from inside the notifier callback.  Queue detach
     * so the listener is not deleted while that callback is still active. */
    QObject::connect(l, &QConsoleListener::is_finished, this, &QgConsole::detach, Qt::QueuedConnection);
    listeners[std::make_pair(p, t)] = l;
}
void QgConsole::detach(struct ged_subprocess *p, int t)
{
    QTCAD_SLOT("QgConsole::detach", 1);
    std::map<std::pair<struct ged_subprocess *, int>, QConsoleListener *>::iterator l_it, si_it, so_it, e_it;
    l_it = listeners.find(std::make_pair(p,t));

    struct ged_subprocess *process = NULL;
    ged_io_func_t callback = NULL;
    void *gdata = NULL;

    if (l_it != listeners.end()) {
	bu_log("Stop listening: %d\n", (int)t);
	QConsoleListener *l = l_it->second;
	process = l->process;
	callback = l->callback;
	gdata = l->data;
	listeners.erase(l_it);
	delete l;
    }

    if (process) {
	si_it = listeners.find(std::make_pair(p,(int)BU_PROCESS_STDIN));
	so_it = listeners.find(std::make_pair(p,(int)BU_PROCESS_STDOUT));
	e_it = listeners.find(std::make_pair(p,(int)BU_PROCESS_STDERR));

	// We don't want to destroy the process until all the listeners are removed.
	// If they all have been, do a final callback call with -1 key to instruct
	// the callback to finalize process and memory removal.
	if (si_it == listeners.end() && so_it == listeners.end() && e_it == listeners.end() && callback) {
	    (*callback)(gdata, -1);
	    // This is also the point at which we know any output from the subprocess
	    // is at an end.  Finalize the before-prompt printing.
	    log_timestamp = 0;
	    QString estr("");
	    printStringBeforePrompt(estr);
	}
    }
}

//-----------------------------------------------------------------------------
void QgConsole::setCompleter(QgConsoleWidgetCompleter* completer)
{
    this->Implementation->setCompleter(completer);
}

//-----------------------------------------------------------------------------
void QgConsole::setCompletionMode(bu_cmd_completion_mode_t mode)
{
    this->Implementation->clearCompletionState();
    this->Implementation->CompletionMode = mode;
}

//-----------------------------------------------------------------------------
bu_cmd_completion_mode_t QgConsole::completionMode() const
{
    return this->Implementation->CompletionMode;
}

//-----------------------------------------------------------------------------
void QgConsole::insertCompletion(const QString& completion)
{
    QTCAD_SLOT("QgConsole::insertCompletion", 1);
    /* Popup selection must replace the original seed, not the currently
     * displayed preview.  Otherwise selecting a different item can leave the
     * suffix from the preview behind (for example, tgc -> tor becomes torgc). */
    if (this->Implementation->CompletionActive) {
	this->Implementation->restoreCompletionBase();
	this->Implementation->clearCompletionState();
    }
    if (this->Implementation->insertCompletionAtReplacement(completion))
	return;

    if (this->Implementation->Completer) {
	int rstart = this->Implementation->Completer->completionReplacementStart();
	int rend = this->Implementation->Completer->completionReplacementEnd();
	int clen = this->Implementation->commandBuffer().length();
	if (rstart >= 0 && rend >= rstart && rstart <= clen && rend <= clen) {
	    QTextCursor rtc(this->Implementation->document());
	    rtc.setPosition(this->Implementation->InteractivePosition + rstart, QTextCursor::MoveAnchor);
	    rtc.setPosition(this->Implementation->InteractivePosition + rend, QTextCursor::KeepAnchor);
	    rtc.insertText(completion);
	    this->Implementation->setTextCursor(rtc);
	    this->Implementation->updateCommandBuffer();
	    return;
	}
    }

    QTextCursor tc = this->Implementation->textCursor();
    tc.setPosition(tc.position(), QTextCursor::MoveAnchor);
    QString text = tc.selectedText();
    //const char *txt = text.toLocal8Bit().data();
    //bu_log("txt: %s\n", txt);
    while (tc.position() > 0 && (!text.length() || (text.at(0) != ' ' && (!split_slash || text.at(0) != '/')))) {
	tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
	text = tc.selectedText();
	//txt = text.toLocal8Bit().data();
	//bu_log("txt: %s\n", txt);
    }
    if (tc.selectedText() == ".") {
	tc.insertText(QString(".") + completion);
    } else {
	tc.setPosition(tc.position()+1, QTextCursor::MoveAnchor);
	tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	tc.insertText(completion);
	this->Implementation->setTextCursor(tc);
    }
    this->Implementation->updateCommandBuffer();
}


//-----------------------------------------------------------------------------
void QgConsole::printString(const QString& Text)
{
    QTCAD_SLOT("QgConsole::printString", 1);
    QTextCursor text_cursor = this->Implementation->textCursor();
    text_cursor.setPosition(this->Implementation->documentEnd());
    this->Implementation->setTextCursor(text_cursor);
    text_cursor.insertText(Text);

    this->Implementation->InteractivePosition = this->Implementation->documentEnd();
    this->Implementation->ensureCursorVisible();
}

//-----------------------------------------------------------------------------
// This style of insertion is is too slow to just attempt it every time the
// subprocesses send output (Goliath rtcheck is an example.)  Instead, we
// keep track of the last time we updated and if not enough time has passed
// we just queue up the text for later insertion.
//
// This approach also depends on the listener clean-up finalizing the string -
// otherwise there would be a chance of losing some printing due to timing
// issues if the last bits of the output come in right after an insertion.
//
// TODO: It would be better if the input prompt and command were one
// QPlainTextEdit and the output another, so they could operate independently -
// this approach will still introduce brief periods where the input prompt
// disappears and reappears during updating.  However, that would
// require restructuring QgConsole's widget design - for now this functions,
// and if this becomes the production solution we can/should revisit it later.
void QgConsole::printStringBeforePrompt(const QString& Text)
{
    QTCAD_SLOT("QgConsole::printStringBeforePrompt", 1);
    logbuf.append(Text);
    int64_t ctime = bu_gettime();
    double elapsed = ((double)ctime - (double)log_timestamp)/1000000.0;
    if (elapsed > 0.1 && logbuf.length()) {
	// Make a local printing copy and clear the buffer
	QString llogbuf = logbuf;
	logbuf.clear();

	log_timestamp = bu_gettime();

	// While we're manipulating the console contents, we don't want
	// the user modifying anything.
	this->Implementation->setReadOnly(true);

	// Make a copy of the text cursor on which to operate.  Note that this
	// is not the actual cursor being used for editing and we need to use
	// setTextCursor to impact that cursor.  For most of this operation we
	// don't need to, but it's important for restoring the editing state
	// to the user at the end.
	QTextCursor tc = this->Implementation->textCursor();

	// Store the current editing point relative to the prompt (it may not
	// be the end of the command, if the user is editing mid-command.)
	int curr_pos_offset = tc.position() - (prompt_start + prompt_str.length());

	// Before appending new content to the log, clear the old prompt and
	// command string.  We restore them after the new log content is added.
	tc.setPosition(prompt_start);
	tc.setPosition(this->Implementation->documentEnd(), QTextCursor::KeepAnchor);
	tc.removeSelectedText();

	// Print the accumulated logged output to the command window.
	this->Implementation->insertPlainText(llogbuf);

	// Before we re-add the prompt and command buffer, store the new prompt
	// starting point for use in the next write cycle.
	prompt_start = tc.position();

	// Restore the prompt and the next command (if any)
	this->Implementation->insertPlainText(prompt_str);
	this->Implementation->textCursor().insertText(this->Implementation->commandBuffer());

	// Denote the interactive portion of the new state of the buffer as starting after
	// the new prompt.
	this->Implementation->InteractivePosition = prompt_start + prompt_str.length();

	// Make sure the scrolling is set to view the new prompt
	this->Implementation->ensureCursorVisible();

	// The final touch - in case the cursor was not at the end of the command buffer,
	// restore it to its prior offset from the prompt.  Since we are altering the
	// user-interactive editing cursor this time, and not just manipulating the text,
	// we must use setTextCursor to apply the change.
	tc.setPosition(prompt_start + prompt_str.length() + curr_pos_offset);
	this->Implementation->setTextCursor(tc);

	// All done - unlock
	this->Implementation->setReadOnly(false);
    }

    // If there is anything queued up, we need to make sure we print it soon(ish)
    if (logbuf.length()) {
	QTimer::singleShot(1000, this, &QgConsole::emit_queued);
    }
}

//-----------------------------------------------------------------------------
void QgConsole::printCommand(const QString& cmd)
{
    QTCAD_SLOT("QgConsole::printCommand", 1);
    this->Implementation->textCursor().insertText(cmd);
    this->Implementation->updateCommandBuffer();
}

//-----------------------------------------------------------------------------
void QgConsole::prompt(const QString& text)
{
    QTCAD_SLOT("QgConsole::prompt", 1);
    QTextCursor text_cursor = this->Implementation->textCursor();

    // if the cursor is currently on a clean line, do nothing, otherwise we move
    // the cursor to a new line before showing the prompt.
    text_cursor.movePosition(QTextCursor::StartOfLine);
    int startpos = text_cursor.position();
    text_cursor.movePosition(QTextCursor::EndOfLine);
    int endpos = text_cursor.position();
    if (endpos != startpos) {
	this->Implementation->textCursor().insertText("\n");
    }

    prompt_start = text_cursor.position();
    prompt_str = text;

    this->Implementation->textCursor().insertText(text);
    this->Implementation->InteractivePosition = this->Implementation->documentEnd();
    this->Implementation->ensureCursorVisible();
}

//-----------------------------------------------------------------------------
void QgConsole::clear()
{
    QTCAD_SLOT("QgConsole::clear", 1);
    this->Implementation->clear();

    // For some reason the QCompleter tries to set the focus policy to
    // NoFocus, set let's make sure we set it back to the default WheelFocus.
    this->Implementation->setFocusPolicy(Qt::WheelFocus);
}

//-----------------------------------------------------------------------------
void QgConsole::internalExecuteCommand(const QString& Command)
{
    emit this->executeCommand(Command);
}

//-----------------------------------------------------------------------------

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
