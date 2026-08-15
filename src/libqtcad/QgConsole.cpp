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
#include <cctype>

#include <QAbstractItemView>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QCompleter>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QTimer>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QtGlobal>

#include "bu.h"
#include "ged.h"

static int
qstring_pos_from_utf8_offset(const QByteArray& bytes, size_t offset)
{
    if (offset > (size_t)bytes.size())
	offset = (size_t)bytes.size();

    return QString::fromUtf8(bytes.constData(), (int)offset).length();
}

GEDShellCompleter::GEDShellCompleter(
	QWidget* parent, struct ged *ged_ptr)
{
    setParent(parent);
    gedp = ged_ptr;
    completion_model = new QStringListModel(this);
    setModel(completion_model);
}

void
GEDShellCompleter::updateCompletionModel(const QString& console_txt)
{
    updateCompletionModelAt(console_txt, console_txt.size());
}

void
GEDShellCompleter::updateCompletionModelAt(const QString& console_txt, int cursor_pos)
{
    completion_model->setStringList(QStringList());
    replace_start = -1;
    replace_end = -1;
    total_count = 0;
    truncated = false;
    common_prefix_known = false;
    common_prefix.clear();

    cursor_pos = std::max(0, std::min(cursor_pos, (int)console_txt.size()));
    QByteArray cbytes = console_txt.toUtf8();
    QByteArray cursor_bytes = console_txt.left(cursor_pos).toUtf8();
    struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
    struct ged_cmd_completion_request request = GED_CMD_COMPLETION_REQUEST_NULL;
    request.cursor_pos = (size_t)cursor_bytes.size();
    request.max_candidates = bu_cmd_completion_candidate_budget(display_columns,
	display_rows);
    int completion_status = ged_cmd_complete_query(gedp, cbytes.constData(), &request, &result);
    if (completion_status != 0 || !result.completion_count || !result.completion_candidates) {
	ged_cmd_completion_result_clear(&result);
	return;
    }

    QStringList clist = QStringList();
    for (size_t i = 0; i < result.completion_count; i++) {
	const unsigned char *cp = (const unsigned char *)result.completion_candidates[i];
	bool terminal_safe = cp != NULL;
	for (; terminal_safe && *cp; cp++)
	    if (*cp < 0x20 || *cp == 0x7f) terminal_safe = false;
	if (terminal_safe)
	    clist.append(QString::fromUtf8(result.completion_candidates[i]));
    }

    if (clist.isEmpty()) {
	ged_cmd_completion_result_clear(&result);
	return;
    }

    replace_start = qstring_pos_from_utf8_offset(cbytes, result.replacement_start);
    replace_end = qstring_pos_from_utf8_offset(cbytes, result.replacement_end);
    total_count = result.total_count;
    truncated = result.truncated != 0;
    common_prefix_known = result.common_prefix != NULL;
    bool common_prefix_safe = true;
    for (const unsigned char *cp =
	    (const unsigned char *)(result.common_prefix ? result.common_prefix : "");
	    *cp; cp++) {
	if (*cp < 0x20 || *cp == 0x7f) {
	    common_prefix_safe = false;
	    break;
	}
    }
    /* Keep "known" true for an unsafe full-set prefix so prefix mode does
     * not fall back to the materialized subset and over-complete past a
     * filtered control-byte candidate. */
    common_prefix = common_prefix_safe ?
	QString::fromUtf8(result.common_prefix ? result.common_prefix : "") :
	QString();

    setCompletionMode(QCompleter::PopupCompletion);
    completion_model->setStringList(clist);
    setCaseSensitivity(Qt::CaseSensitive);
    setCompletionPrefix(QString::fromUtf8(result.prefix ? result.prefix : ""));
    if (popup())
	popup()->setCurrentIndex(completionModel()->index(0, 0));

    ged_cmd_completion_result_clear(&result);
}

QString
GEDShellCompleter::completionInsertion(const QString& candidate, const QString& line,
	int replacement_start) const
{
    QByteArray value = candidate.toUtf8();
    QByteArray command = line.left(std::max(0, replacement_start)).toUtf8();
    bool quoted = false;
    bool escaped = false;
    for (char byte : command) {
	if (escaped) { escaped = false; continue; }
	if (byte == '\\') { escaped = true; continue; }
	if (byte == '"') quoted = !quoted;
    }
    QByteArray insertion;
    for (unsigned char byte : value) {
	if (byte == '\\' || byte == '"' || (!quoted && std::isspace(byte)))
	    insertion.append('\\');
	insertion.append((char)byte);
    }
    return QString::fromUtf8(insertion);
}

void
GEDShellCompleter::analyze(const QString& console_txt, std::vector<QgConsoleHighlight>& highlights)
{
    highlights.clear();
    if (!gedp || console_txt.isEmpty())
	return;

    QByteArray cbytes = console_txt.toUtf8();
    struct ged_cmd_analysis analysis = GED_CMD_ANALYSIS_NULL;
    if (ged_cmd_analyze(gedp, cbytes.constData(), &analysis) != 0)
	return;

    for (size_t i = 0; i < analysis.token_count; i++) {
	const struct ged_cmd_analysis_token *token = &analysis.tokens[i];
	QgConsoleHighlight h;
	if (token->char_end <= token->char_start)
	    continue;
	h.start = qstring_pos_from_utf8_offset(cbytes, token->char_start);
	h.end = qstring_pos_from_utf8_offset(cbytes, token->char_end);
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

    Q_EMIT console->queued_log(QString::fromUtf8(output));
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
	QString qoutput = QString::fromUtf8(output);
	if (!olen || output[olen - 1] != '\n')
	    qoutput.append('\n');
	if (console)
	    qg_console_log_hook((void *)console, (void *)qoutput.toUtf8().constData());
	else
	    bu_log("%s", qoutput.toUtf8().constData());
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
	PermanentMaximumBlockCount = 10000;

	QFont f("Courier");
	f.setStyleHint(QFont::TypeWriter);
	f.setFixedPitch(true);
	this->setFont(f);

	this->CommandHistory.append("");
	this->CommandPosition = 0;
	CompletionResizeTimer.setSingleShot(true);
	CompletionResizeTimer.setInterval(40);
	QObject::connect(&CompletionResizeTimer, &QTimer::timeout, this, [this]() {
	    if (CompletionDisplayCandidates.isEmpty())
		return;
	    if (CompletionActive && Completer && Completer->completionTruncated()) {
		QString selected = completionAt(CompletionIndex);
		updateCompleterViewport();
		Completer->updateCompletionModelAt(CompletionBase,
		    CompletionBaseCursor);
		int selected_index = -1;
		for (int i = 0; i < Completer->completionCount(); i++)
		    if (completionAt(i) == selected) {
			selected_index = i;
			break;
		    }
		if (selected_index >= 0)
		    CompletionIndex = selected_index;
		else if (Completer->completionCount() > 0)
		    CompletionIndex = std::min(CompletionIndex,
			Completer->completionCount() - 1);
		updateCompletionDisplay();
		return;
	    }
	    renderCompletionDisplay(CompletionDisplayCandidates);
	});
	SemanticTimer.setSingleShot(true);
	SemanticTimer.setInterval(35);
	QObject::connect(&SemanticTimer, &QTimer::timeout, this, [this]() {
	    updateSemanticSelections();
	});
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
	    CompletionResizeTimer.stop();
	    removeCompletionDisplay(true);
	    CompletionActive = false;
	    CompletionBase.clear();
	    CompletionBaseCursor = 0;
	    CompletionIndex = -1;
	    if (hide_popup && this->Completer && this->Completer->popup())
		this->Completer->popup()->hide();
	    CompletionSelections.clear();
	    applyExtraSelections();
	}

	QColor completionColor() const
	{
	    const struct bu_lineedit_style &configured =
		LineeditPalette.roles[BU_LINEEDIT_ROLE_COMPLETION_PREVIEW];
	    QColor color = (configured.flags & BU_LINEEDIT_STYLE_COLOR) ?
		QColor(configured.rgb[0], configured.rgb[1], configured.rgb[2]) :
		palette().color(QPalette::Text);
	    bool dim = true;
	    if (configured.flags & BU_LINEEDIT_STYLE_DIM_SET)
		dim = (configured.flags & BU_LINEEDIT_STYLE_DIM) != 0;
	    color.setAlphaF(dim ? 0.45 : 1.0);
	    return color;
	}

	void removeCompletionDisplay(bool forget_candidates)
	{
	    if (CompletionDisplayPosition >= 0) {
		QTextCursor saved = textCursor();
		int saved_position = std::min(saved.position(), CompletionDisplayPosition);
		int saved_anchor = std::min(saved.anchor(), CompletionDisplayPosition);
		QTextCursor display(document());
		display.setPosition(CompletionDisplayPosition);
		display.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		display.removeSelectedText();
		saved.setPosition(saved_anchor);
		saved.setPosition(saved_position, QTextCursor::KeepAnchor);
		setTextCursor(saved);
		CompletionDisplayPosition = -1;
	    }
	    if (maximumBlockCount() != PermanentMaximumBlockCount)
		setMaximumBlockCount(PermanentMaximumBlockCount);
	    if (forget_candidates)
		CompletionDisplayCandidates.clear();
	}

	int completionDisplayInputRows()
	{
	    int rows = 0;
	    int input_end = CompletionDisplayPosition >= 0 ?
		CompletionDisplayPosition : documentEnd();
	    QTextBlock block = document()->findBlock(InteractivePosition);
	    while (block.isValid()) {
		QTextLayout *layout = block.layout();
		rows += layout ? std::max(1, layout->lineCount()) : 1;
		if (block.position() + block.length() >= input_end)
		    break;
		block = block.next();
	    }
	    return std::max(1, rows);
	}

	int completionDisplayMaxRows()
	{
	    QFontMetrics metrics(font());
	    int line_height = std::max(1, metrics.lineSpacing());
	    int total_rows = std::max(1, viewport()->height() / line_height);
	    int prior_rows = (total_rows + 2) / 3;
	    int input_rows = completionDisplayInputRows();
	    int scroll_budget = std::max(1, total_rows - prior_rows - input_rows);
	    QTextCursor end_cursor(document());
	    end_cursor.setPosition(CompletionDisplayPosition >= 0 ?
		CompletionDisplayPosition : documentEnd());
	    int pixels_below = viewport()->height() - cursorRect(end_cursor).bottom() - 1;
	    int rows_below = std::max(0, pixels_below / line_height);
	    return std::max(rows_below, scroll_budget);
	}

	void updateCompleterViewport()
	{
	    if (!Completer)
		return;
	    QFontMetrics metrics(font());
	    int cell_width = metrics.horizontalAdvance(QLatin1Char('0'));
	    size_t columns = cell_width > 0 ?
		(size_t)std::max(1, viewport()->width() / cell_width) : 80;
	    Completer->setCompletionViewport(columns,
		(size_t)completionDisplayMaxRows());
	}

	void renderCompletionDisplay(const QStringList &display_candidates)
	{
	    removeCompletionDisplay(false);
	    if (display_candidates.size() < 2)
		return;

	    std::vector<QByteArray> encoded;
	    std::vector<const char *> candidates;
	    encoded.reserve((size_t)display_candidates.size());
	    candidates.reserve((size_t)display_candidates.size());
	    for (const QString &candidate : display_candidates) {
		encoded.push_back(candidate.toUtf8());
		candidates.push_back(encoded.back().constData());
	    }

	    QFontMetrics metrics(font());
	    int cell_width = metrics.horizontalAdvance(QLatin1Char('0'));
	    size_t columns = (cell_width > 0) ?
		(size_t)std::max(1, viewport()->width() / cell_width) : 80;
	    size_t max_rows = (size_t)completionDisplayMaxRows();
	    struct bu_cmd_completion_layout layout = BU_CMD_COMPLETION_LAYOUT_INIT_ZERO;
	    QStringList lines;
	    if (Completer->completionTruncated() && max_rows <= 1) {
		lines.append(QStringLiteral("... (%1 matches)").arg(
		    (qulonglong)Completer->completionTotalCount()));
	    } else {
		if (Completer->completionTruncated())
		    max_rows--;
		if (bu_cmd_completion_layout_create(&layout, candidates.data(), candidates.size(),
			columns, max_rows) != BRLCAD_OK || !layout.line_count) {
		    bu_cmd_completion_layout_clear(&layout);
		    return;
		}
		for (size_t i = 0; i < layout.line_count; i++)
		    lines.append(QString::fromUtf8(layout.lines[i]));
		if (Completer->completionTruncated())
		    lines.append(QStringLiteral("... (%1 more matches)").arg(
			(qulonglong)(Completer->completionTotalCount() - candidates.size())));
	    }
	    bu_cmd_completion_layout_clear(&layout);

	    QTextCursor saved = textCursor();
	    /* Completion blocks must not count against permanent history. */
	    setMaximumBlockCount(0);
	    QTextCursor display(document());
	    display.movePosition(QTextCursor::End);
	    CompletionDisplayPosition = display.position();
	    QTextCharFormat format;
	    format.setForeground(completionColor());
	    format.setProperty(QTextFormat::UserProperty, QStringLiteral("completion-candidates"));
	    display.insertText(QStringLiteral("\n") + lines.join(QLatin1Char('\n')), format);
	    setTextCursor(saved);
	    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
	    if (Completer->popup())
		Completer->popup()->hide();
	}

	void updateCompletionDisplay()
	{
	    CompletionDisplayCandidates.clear();
	    if (!Completer || !Completer->completionModel() || Completer->completionCount() < 2) {
		removeCompletionDisplay(true);
		return;
	    }
	    for (int i = 0; i < Completer->completionCount(); i++)
		CompletionDisplayCandidates.append(completionAt(i));
	    renderCompletionDisplay(CompletionDisplayCandidates);
	}

	void showCompletionPreview(int start, int end)
	{
	    CompletionSelections.clear();
	    if (end > start) {
		QTextEdit::ExtraSelection preview;
		preview.cursor = QTextCursor(document());
		preview.cursor.setPosition(InteractivePosition + start);
		preview.cursor.setPosition(InteractivePosition + end, QTextCursor::KeepAnchor);
		preview.format.setForeground(completionColor());
		CompletionSelections.append(preview);
	    }
	    applyExtraSelections();
	}

	void restoreCompletionBase()
	{
	    if (!CompletionActive)
		return;
	    removeCompletionDisplay(false);
	    replaceCommandBuffer(CompletionBase);
	    QTextCursor c = textCursor();
	    c.setPosition(InteractivePosition + CompletionBaseCursor);
	    setTextCursor(c);
	}

	bool filterEditExtendsPreview(const QString &edit)
	{
	    if (!CompletionActive || !Completer || edit.isEmpty())
		return false;

	    QString original = CompletionBase.left(CompletionBaseCursor) + edit +
		CompletionBase.mid(CompletionBaseCursor);
	    updateCompleterViewport();
	    Completer->updateCompletionModelAt(original,
		CompletionBaseCursor + edit.size());
	    if (Completer->completionCount() > 0)
		return false;

	    int preview_cursor = textCursor().position() - InteractivePosition;
	    QString preview = commandBuffer().left(preview_cursor) + edit +
		commandBuffer().mid(preview_cursor);
	    Completer->updateCompletionModelAt(preview, preview_cursor + edit.size());
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
	    QString insertion = this->Completer->completionInsertion(completion,
		commandBuffer(), rstart);
	    rtc.insertText(insertion);
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
		QString insertion = this->Completer->completionInsertion(candidate,
		    CompletionBase, rstart);
		insertCompletionAtReplacement(candidate);
		/* Insert transient candidate rows before constructing the preview
		 * selection.  QTextCursor selections track edits at their boundary;
		 * appending the display afterward would extend a preview at end of
		 * input across the newline and candidate rows. */
		updateCompletionDisplay();
		int insertion_size = insertion.size();
		showCompletionPreview(rstart + std::min(confirmed, insertion_size),
		    rstart + insertion_size);
	    }
	    if (this->Completer->popup())
		this->Completer->popup()->setCurrentIndex(this->Completer->completionModel()->index(CompletionIndex, 0));
	}

	void prefixCompletion()
	{
	    if (!this->Completer)
		return;
	    updateCompleter();
	    int count = this->Completer->completionCount();
	    if (count <= 0)
		return;
	    QString common = this->Completer->completionCommonPrefixKnown() ?
		this->Completer->completionCommonPrefix() : completionAt(0);
	    for (int i = this->Completer->completionCommonPrefixKnown() ? count : 1;
		i < count && !common.isEmpty(); i++) {
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
	    bool repaint = !CompletionDisplayCandidates.isEmpty();
	    removeCompletionDisplay(false);
	    QPlainTextEdit::setFont(i_font);
	    if (repaint)
		renderCompletionDisplay(CompletionDisplayCandidates);
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
	    QByteArray command = CommandHistory.at(ind).toUtf8();
	    return std::string(command.constData(), (size_t)command.size());
	}

	// Try to keep the scrollbar slider from getting too small to be usable
	void resizeEvent(QResizeEvent *e)
	{
	    bool repaint = !CompletionDisplayCandidates.isEmpty();
	    /* Leave the old transient rows in place until the debounced repaint.
	     * Removing them here can hide the vertical scrollbar, which triggers a
	     * second viewport resize and an erase/repaint loop. */
	    PermanentMaximumBlockCount = std::max(1, 2*this->height());
	    if (CompletionDisplayPosition < 0)
		this->setMaximumBlockCount(PermanentMaximumBlockCount);
	    QPlainTextEdit::resizeEvent(e);
	    if (repaint)
		CompletionResizeTimer.start();
	}

	void insertFromMimeData(const QMimeData * s)
	{
	    clearCompletionState();
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

	if (CompletionActive && completionModifierKey(e->key())) {
	    QPlainTextEdit::keyPressEvent(e);
	    return;
	}

	/* Transient console rows keep QCompleter's popup hidden, so Escape must
	 * cancel independently of the popup-specific forwarding block above. */
	if (CompletionActive && e->key() == Qt::Key_Escape) {
	    restoreCompletionBase();
	    clearCompletionState();
	    e->accept();
	    return;
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

	void mousePressEvent(QMouseEvent *e)
	{
	    clearCompletionState();
	    QPlainTextEdit::mousePressEvent(e);
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
		QTextCursor text_cursor = this->textCursor();
		int command_cursor = text_cursor.position() - this->InteractivePosition;
		QString commandText = commandBuffer();

		// Size the provider query to the transient console viewport.
		updateCompleterViewport();

		// Call the completer to update the completion model
		this->Completer->updateCompletionModelAt(commandText, command_cursor);

		// The non-legacy modes render their candidates as transient console
		// text.  Keep QCompleter's scrolling popup only for legacy behavior.
		if (this->Completer->completionCount() && CompletionMode == BU_CMD_COMPLETE_LEGACY) {
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
	    removeCompletionDisplay(true);
	    this->commandBuffer() = this->toPlainText().mid(this->InteractivePosition);
	    /* Parsing and semantic database lookup can be noticeable while rapidly
	     * typing.  Coalesce edits; existing QTextCursor selections track the
	     * edit until the refreshed analysis replaces them.  Tab completion
	     * itself remains synchronous and uses the current buffer. */
	    SemanticTimer.start();
	}

	void flushSemanticSelections()
	{
	    SemanticTimer.stop();
	    updateSemanticSelections();
	}

	/// Replace the contents of the command buffer, updating the display
	void replaceCommandBuffer(const QString& Text)
	{
	    removeCompletionDisplay(false);
	    this->commandBuffer() = Text;

	    QTextCursor c(this->document());
	    c.setPosition(this->InteractivePosition);
	    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	    c.removeSelectedText();
	    c.insertText(Text);
	    SemanticTimer.stop();
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
	    SemanticTimer.stop();
	    removeCompletionDisplay(true);
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
	    clearCompletionState();
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

	bool completionModifierKey(int key) const
	{
	    return key == Qt::Key_Shift || key == Qt::Key_Control ||
		key == Qt::Key_Alt || key == Qt::Key_Meta ||
		key == Qt::Key_AltGr || key == Qt::Key_CapsLock ||
		key == Qt::Key_NumLock || key == Qt::Key_ScrollLock;
	}

	/// Stores a back-reference to our owner
	QgConsole& Parent;

	/// A custom completer
	QPointer<QgConsoleWidgetCompleter> Completer;
	QStringList CompletionDisplayCandidates;
	int CompletionDisplayPosition = -1;
	int PermanentMaximumBlockCount = 10000;
	QTimer CompletionResizeTimer;
	QTimer SemanticTimer;
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
    this->Implementation->clearCompletionState();
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
	bool repaint_completion = !this->Implementation->CompletionDisplayCandidates.isEmpty();
	this->Implementation->removeCompletionDisplay(false);
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
	if (repaint_completion)
	    this->Implementation->renderCompletionDisplay(
		    this->Implementation->CompletionDisplayCandidates);
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
    this->Implementation->clearCompletionState();
    this->Implementation->textCursor().insertText(cmd);
    this->Implementation->updateCommandBuffer();
    /* Programmatic replacement is a completed edit, not a keystroke burst. */
    this->Implementation->flushSemanticSelections();
}

//-----------------------------------------------------------------------------
void QgConsole::prompt(const QString& text)
{
    QTCAD_SLOT("QgConsole::prompt", 1);
    this->Implementation->clearCompletionState();
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
    this->Implementation->clearCompletionState();
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
