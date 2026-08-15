/*                  Q G C O N S O L E _ C O M P L E T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>

#include <QApplication>
#include <QAbstractItemView>
#include <QFontMetrics>
#include <QPlainTextEdit>
#include <QStringListModel>
#include <QSignalSpy>
#include <QtTest/QTest>

#include "bu/app.h"
#include "bu/log.h"
#include "qtcad/QgConsole.h"


class TestCompleter : public QgConsoleWidgetCompleter
{
public:
    TestCompleter(QWidget *parent) : QgConsoleWidgetCompleter()
    {
	setParent(parent);
    }

    void updateCompletionModel(const QString &line) override
    {
	QString seed = line.mid(4);
	QStringList all = {"alpha", "alpine"};
	QStringList matches;
	for (const QString &candidate : all) {
	    if (candidate.startsWith(seed))
		matches.append(candidate);
	}
	setModel(new QStringListModel(matches, this));
	setCompletionPrefix(seed);
	replace_start = 4;
	replace_end = line.size();
    }

    int completionReplacementStart() const override { return replace_start; }
    int completionReplacementEnd() const override { return replace_end; }

private:
    int replace_start = 4;
    int replace_end = 5;
};

class HighlightCompleter : public TestCompleter
{
public:
    HighlightCompleter(QWidget *parent) : TestCompleter(parent) {}

    void analyze(const QString &line, std::vector<QgConsoleHighlight> &highlights) override
    {
	analysis_calls++;
	highlights.clear();
	if (line.size() >= 3)
	    highlights.push_back({0, 3, QG_CONSOLE_COMMAND});
	if (line.size() > 4)
	    highlights.push_back({4, (int)line.size(), QG_CONSOLE_INVALID});
    }

    int analysis_calls = 0;
};


class RefinementCompleter : public QgConsoleWidgetCompleter
{
public:
    RefinementCompleter(QWidget *parent, bool ambiguous) : QgConsoleWidgetCompleter(), include_original(ambiguous)
    {
	setParent(parent);
    }

    void updateCompletionModel(const QString &line) override
    {
	QString seed = line.mid(4);
	QStringList all = {"alpha", "alpha3"};
	if (include_original)
	    all.append("a3match");
	QStringList matches;
	for (const QString &candidate : all) {
	    if (candidate.startsWith(seed))
		matches.append(candidate);
	}
	setModel(new QStringListModel(matches, this));
	setCompletionPrefix(seed);
	replace_start = 4;
	replace_end = line.size();
    }

    int completionReplacementStart() const override { return replace_start; }
    int completionReplacementEnd() const override { return replace_end; }

private:
    bool include_original = false;
    int replace_start = 4;
    int replace_end = 5;
};


class LongCompleter : public QgConsoleWidgetCompleter
{
public:
    LongCompleter(QWidget *parent) : QgConsoleWidgetCompleter()
    {
	setParent(parent);
    }

    void updateCompletionModel(const QString &line) override
    {
	QStringList matches;
	for (int i = 0; i < 3; i++)
	    matches.append(QString("aab%1").arg(i));
	for (int i = 0; i < 20; i++)
	    matches.append(QString("aac%1").arg(i, 2, 10, QLatin1Char('0')));
	for (int i = 0; i < 300; i++)
	    matches.append(QString("d5m%1").arg(i, 3, 10, QLatin1Char('0')));
	setModel(new QStringListModel(matches, this));
	setCompletionPrefix(line.mid(4));
	replace_start = 4;
	replace_end = line.size();
    }

    int completionReplacementStart() const override { return replace_start; }
    int completionReplacementEnd() const override { return replace_end; }

private:
    int replace_start = 4;
    int replace_end = 4;
};


static QString
command_text(QPlainTextEdit *edit)
{
    QString text = edit->toPlainText();
    int prompt = text.lastIndexOf("$ ");
    QString command = (prompt >= 0) ? text.mid(prompt + 2) : text;
    int display = command.indexOf(QLatin1Char('\n'));
    return (display >= 0) ? command.left(display) : command;
}


static QString
completion_text(QPlainTextEdit *edit)
{
    QString text = edit->toPlainText();
    int prompt = text.lastIndexOf("$ ");
    QString command_and_display = (prompt >= 0) ? text.mid(prompt + 2) : text;
    int display = command_and_display.indexOf(QLatin1Char('\n'));
    return (display >= 0) ? command_and_display.mid(display + 1) : QString();
}


static int
completion_row_budget(QPlainTextEdit *edit)
{
    int line_height = std::max(1, QFontMetrics(edit->font()).lineSpacing());
    int total_rows = std::max(1, edit->viewport()->height() / line_height);
    int prior_rows = (total_rows + 2) / 3;
    int scroll_budget = std::max(1, total_rows - prior_rows - 1);
    /* With no prior output to preserve, the console may use the whole
     * viewport and scroll the prompt to its top edge. */
    if (edit->toPlainText().lastIndexOf("$ ") == 0)
	return total_rows;
    int pixels_below = edit->viewport()->height() -
	edit->cursorRect(edit->textCursor()).bottom() - 1;
    int rows_below = std::max(0, pixels_below / line_height);
    return std::max(rows_below, scroll_budget);
}


static bool
run_mode_test(bu_cmd_completion_mode_t mode, const QString &expected_after_tab,
	const QString &typed, const QString &expected_after_type)
{
    QgConsole console(NULL);
    TestCompleter *completer = new TestCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(mode);
    console.prompt("$ ");
    console.printCommand("cmd a");

    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    if (command_text(edit) != expected_after_tab) {
	qWarning("mode %d Tab produced [%s], expected [%s]", (int)mode,
		command_text(edit).toLocal8Bit().constData(),
		expected_after_tab.toLocal8Bit().constData());
	return false;
    }
    bool preview_mode = (mode == BU_CMD_COMPLETE_FILTER || mode == BU_CMD_COMPLETE_CYCLE);
    if (preview_mode) {
	if (edit->extraSelections().size() != 1 ||
		edit->extraSelections().front().cursor.selectedText() != "lpha" ||
		completion_text(edit).isEmpty() || completer->popup()->isVisible()) {
	    qWarning("mode %d preview state is inconsistent: selections=%d selected=[%s] display=[%s] popup=%d",
		    (int)mode, (int)edit->extraSelections().size(),
		    edit->extraSelections().isEmpty() ? "" :
		    edit->extraSelections().front().cursor.selectedText().toLocal8Bit().constData(),
		    completion_text(edit).toLocal8Bit().constData(), (int)completer->popup()->isVisible());
	    return false;
	}
    } else if (!edit->extraSelections().isEmpty() || !completion_text(edit).isEmpty()) {
	qWarning("mode %d unexpectedly retained transient completion state", (int)mode);
	return false;
    }

    if (!typed.isNull()) {
	QTest::keyClicks(edit, typed);
	QApplication::processEvents();
	if (command_text(edit) != expected_after_type) {
	    qWarning("mode %d typing produced [%s], expected [%s]", (int)mode,
		    command_text(edit).toLocal8Bit().constData(),
		    expected_after_type.toLocal8Bit().constData());
	    return false;
	}
	if (!edit->extraSelections().isEmpty() || !completion_text(edit).isEmpty()) {
	    qWarning("mode %d typing did not clear transient state", (int)mode);
	    return false;
	}
    }

    return true;
}


static bool
run_navigation_test()
{
    QgConsole console(NULL);
    TestCompleter *completer = new TestCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.prompt("$ ");
    console.printCommand("cmd a");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();

    QTest::keyClick(edit, Qt::Key_Tab);
    QTest::keyPress(edit, Qt::Key_Shift);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpha" || completion_text(edit).isEmpty()) {
	qWarning("modifier cleared completion display");
	return false;
    }
    QTest::keyRelease(edit, Qt::Key_Shift);
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpine" || edit->extraSelections().size() != 1 ||
	    edit->extraSelections().front().cursor.selectedText() != "lpine") {
	qWarning("second Tab failed: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
	}

    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpha") {
	qWarning("wrap failed: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
	}

    QTest::keyClick(edit, Qt::Key_Escape);
    QApplication::processEvents();
    if (command_text(edit) != "cmd a" || !edit->extraSelections().isEmpty()) {
	qWarning("escape failed: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
	}

    QTest::keyClick(edit, Qt::Key_Tab, Qt::ShiftModifier);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpine") {
	qWarning("backtab failed: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
	}

    QTest::keyClick(edit, Qt::Key_Backspace);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpin" || !edit->extraSelections().isEmpty()) {
	qWarning("backspace failed: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
    }
    return true;
}


static bool
run_cycle_navigation_test()
{
    QgConsole console(NULL);
    TestCompleter *completer = new TestCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_CYCLE);
    console.prompt("$ ");
    console.printCommand("cmd a");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();

    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpha") {
	qWarning("cycle first Tab: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
    }
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpine") {
	qWarning("cycle second Tab: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
    }
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    if (command_text(edit) != "cmd alpha") {
	qWarning("cycle wrap: [%s]", command_text(edit).toLocal8Bit().constData());
	return false;
    }
    QTest::keyClick(edit, Qt::Key_Escape);
    QApplication::processEvents();
    bool restored = command_text(edit) == "cmd a" && edit->extraSelections().isEmpty();
    if (!restored)
	qWarning("cycle escape failed: command=[%s] selections=%d display=[%s]",
		command_text(edit).toLocal8Bit().constData(),
		(int)edit->extraSelections().size(),
		completion_text(edit).toLocal8Bit().constData());
    return restored;
}


static bool
run_refinement_fallback_test(bool ambiguous, const QString &expected)
{
    QgConsole console(NULL);
    RefinementCompleter *completer = new RefinementCompleter(&console, ambiguous);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.prompt("$ ");
    console.printCommand("cmd a");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();

    QTest::keyClick(edit, Qt::Key_Tab);
    QTest::keyClicks(edit, "3");
    QApplication::processEvents();
    return command_text(edit) == expected && edit->extraSelections().isEmpty();
}

static bool
run_long_completion_display_test()
{
    QgConsole console(NULL);
    LongCompleter *completer = new LongCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.resize(430, 480);
    console.show();
    console.prompt("$ ");
    console.printCommand("cmd ");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();

    int large_budget = completion_row_budget(edit);
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    QString large_display = completion_text(edit);
    int large_rows = large_display.isEmpty() ? 0 : large_display.count(QLatin1Char('\n')) + 1;

    /* The exact smaller bins depend on the font's measured cell width.  A
     * large blank area may also admit the complete set without summarizing. */
    bool summarized = large_display.contains(" (") ||
	large_display.contains("matches)") || large_display.contains("more)");
    bool complete = large_display.contains("aab0") && large_display.contains("d5m299");
    if (large_rows <= 5 || large_rows > large_budget || (!summarized && !complete)) {
	qWarning("long completion display failed: rows=%d budget=%d text=[%s]",
		large_rows, large_budget, large_display.toLocal8Bit().constData());
	return false;
    }

    console.resize(430, 220);
    QApplication::processEvents();
    int small_budget = completion_row_budget(edit);
    QTest::qWait(60);
    QApplication::processEvents();
    QString small_display = completion_text(edit);
    int small_rows = small_display.isEmpty() ? 0 : small_display.count(QLatin1Char('\n')) + 1;
    if (small_rows < 1 || small_rows > small_budget || small_rows >= large_rows) {
	qWarning("completion resize failed: large=%d small=%d budget=%d",
		large_rows, small_rows, small_budget);
	return false;
    }

    QTest::keyClick(edit, Qt::Key_Escape);
    QApplication::processEvents();
    return completion_text(edit).isEmpty() && command_text(edit) == "cmd ";
}


static bool
run_completion_log_repaint_test()
{
    QgConsole console(NULL);
    TestCompleter *completer = new TestCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.prompt("$ ");
    console.printCommand("cmd a");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    QString before = completion_text(edit);

    console.printStringBeforePrompt("async completion output\n");
    QApplication::processEvents();
    QString text = edit->toPlainText();
    bool ok = !before.isEmpty() && !completion_text(edit).isEmpty() &&
	text.contains("async completion output\n$ cmd alpha") &&
	command_text(edit) == "cmd alpha";
    if (ok) {
	/* The candidate layout may legitimately change when the prior output
	 * changes the available viewport.  Verify the editable cursor state by
	 * continuing the command rather than comparing transient row text. */
	QTest::keyClicks(edit, "r");
	QApplication::processEvents();
	ok = command_text(edit) == "cmd ar" && completion_text(edit).isEmpty();
    }
    if (!ok)
	qWarning("completion log repaint failed: before=[%s] after=[%s] command=[%s] text=[%s]",
	    before.toLocal8Bit().constData(), completion_text(edit).toLocal8Bit().constData(),
	    command_text(edit).toLocal8Bit().constData(), text.toLocal8Bit().constData());
    return ok;
}

static bool
run_transient_history_test()
{
    QgConsole console(NULL);
    TestCompleter *completer = new TestCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.resize(420, 220);
    console.show();
    QApplication::processEvents();
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;

    int cap = std::max(20, edit->maximumBlockCount());
    QString history;
    for (int i = 0; i < cap + 20; i++)
	history.append(QStringLiteral("history-%1\n").arg(i));
    console.printString(history);
    console.prompt("$ ");
    console.printCommand("cmd a");
    edit->setFocus();
    QString before = edit->toPlainText();
    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    QTest::keyClick(edit, Qt::Key_Escape);
    QApplication::processEvents();
    if (edit->toPlainText() != before) {
	qWarning("transient completion changed capped console history");
	return false;
    }
    return true;
}

static bool
run_highlight_composition_test()
{
    QgConsole console(NULL);
    HighlightCompleter *completer = new HighlightCompleter(&console);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.prompt("$ ");
    console.printCommand("cmd a");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();

    QList<QTextEdit::ExtraSelection> selections = edit->extraSelections();
    if (selections.size() != 2 || selections[0].cursor.selectedText() != "cmd" ||
	    selections[1].cursor.selectedText() != "a") {
	qWarning("initial semantic composition failed: count=%d first=[%s] second=[%s]",
	    (int)selections.size(),
	    selections.size() > 0 ? selections[0].cursor.selectedText().toUtf8().constData() : "",
	    selections.size() > 1 ? selections[1].cursor.selectedText().toUtf8().constData() : "");
	return false;
    }

    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    selections = edit->extraSelections();
    bool have_command = false;
    bool have_preview = false;
    for (const QTextEdit::ExtraSelection &selection : selections) {
	if (selection.cursor.selectedText() == "cmd")
	    have_command = true;
	if (selection.cursor.selectedText() == "lpha")
	    have_preview = true;
    }
	if (!have_command || !have_preview) {
	qWarning("preview semantic composition failed: command=%d preview=%d count=%d",
	    (int)have_command, (int)have_preview, (int)selections.size());
	return false;
	}

    QTest::keyClick(edit, Qt::Key_Space);
    QApplication::processEvents();
    selections = edit->extraSelections();
    for (const QTextEdit::ExtraSelection &selection : selections) {
	if (selection.cursor.selectedText() == "lpha")
	{
	    qWarning("completion preview selection survived ordinary input");
	    return false;
	}
    }
    if (selections.isEmpty())
	qWarning("semantic selections disappeared during debounced refresh");
    return !selections.isEmpty();
}

static bool
run_semantic_debounce_test()
{
    QgConsole console(NULL);
    HighlightCompleter *completer = new HighlightCompleter(&console);
    console.setCompleter(completer);
    console.prompt("$ ");
    console.printCommand("cmd ");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    int baseline = completer->analysis_calls;

    edit->setFocus();
    QTest::keyClicks(edit, "alpha");
    QApplication::processEvents();
    if (completer->analysis_calls != baseline)
	return false;
    QTest::qWait(60);
    QApplication::processEvents();
    return completer->analysis_calls == baseline + 1 && command_text(edit) == "cmd alpha";
}

static bool
run_log_queue_test()
{
    QgConsole console(NULL);
    console.prompt("$ ");
    console.printCommand("draw c");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;

    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 2);
    edit->setTextCursor(cursor);
    int command_cursor = edit->textCursor().position() - edit->toPlainText().lastIndexOf("$ ") - 2;

    QSignalSpy log_spy(&console, &QgConsole::queued_log);
    bu_log_add_hook(&qg_console_log_hook, (void *)&console);
    bu_log("queued diagnostic\n");
    bu_log_delete_hook(&qg_console_log_hook, (void *)&console);
    QApplication::processEvents();

    QString text = edit->toPlainText();
    int restored_cursor = edit->textCursor().position() - text.lastIndexOf("$ ") - 2;
    return log_spy.count() == 1 && text.contains("queued diagnostic\n$ draw c") &&
	command_text(edit) == "draw c" && restored_cursor == command_cursor;
}

static bool
run_ged_semantic_test(const char *db_path)
{
    struct ged *gedp = ged_open("db", db_path, 1);
    if (!gedp)
	return false;

    GEDShellCompleter empty_completer(NULL, gedp);
    empty_completer.updateCompletionModel(QString());
    bool empty_completion_ok = empty_completer.completionCount() > 0;

    QgConsole console(NULL);
    GEDShellCompleter *completer = new GEDShellCompleter(&console, gedp);
    console.setCompleter(completer);
    console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    console.prompt("$ ");
    console.printCommand("search -exec draw \"{}\" \";\"");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit) {
	ged_close(gedp);
	return false;
    }

    bool have_search = false;
    bool have_exec = false;
    bool have_draw = false;
    bool have_substitution = false;
    for (const QTextEdit::ExtraSelection &selection : edit->extraSelections()) {
	QString selected = selection.cursor.selectedText();
	if (selected == "search") have_search = true;
	if (selected == "-exec") have_exec = true;
	if (selected == "draw") have_draw = true;
	if (selected == "\"{}\"") have_substitution = true;
    }
    if (!have_search || !have_exec || !have_draw || !have_substitution) {
	ged_close(gedp);
	return false;
    }

    QgConsole completion_console(NULL);
    GEDShellCompleter *completion_completer = new GEDShellCompleter(&completion_console, gedp);
    completion_console.setCompleter(completion_completer);
    completion_console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    completion_console.prompt("$ ");
    completion_console.printCommand("search -exec dr");
    QAbstractItemModel *stable_completion_model = completion_completer->model();
    QPlainTextEdit *completion_edit = completion_console.findChild<QPlainTextEdit *>();
    if (!completion_edit) {
	ged_close(gedp);
	return false;
    }
    completion_edit->setFocus();
    QTest::keyClick(completion_edit, Qt::Key_Tab);
    QApplication::processEvents();
    bool completion_ok = command_text(completion_edit) == "search -exec draw" &&
	completion_completer->model() == stable_completion_model;

    QgConsole type_console(NULL);
    GEDShellCompleter *type_completer = new GEDShellCompleter(&type_console, gedp);
    type_console.setCompleter(type_completer);
    type_console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    type_console.prompt("$ ");
    type_console.printCommand("search -type t");
    QPlainTextEdit *type_edit = type_console.findChild<QPlainTextEdit *>();
    if (!type_edit) {
	ged_close(gedp);
	return false;
    }
    type_edit->setFocus();
    QTest::keyClick(type_edit, Qt::Key_Tab);
    QApplication::processEvents();
    QModelIndex tor_index;
    for (int i = 0; i < type_completer->completionModel()->rowCount(); i++) {
	QModelIndex candidate = type_completer->completionModel()->index(i, 0);
	if (candidate.data().toString() == "tor") {
	    tor_index = candidate;
	    break;
	}
    }
    if (!tor_index.isValid()) {
	ged_close(gedp);
	return false;
    }

    /* Candidate rows are console text rather than a selectable popup.  Cycle
     * to the desired type just as the terminal frontends do. */
    for (int i = 0; i < tor_index.row(); i++)
	QTest::keyClick(type_edit, Qt::Key_Tab);
    QApplication::processEvents();
    bool popup_replace_ok = command_text(type_edit) == "search -type tor" &&
	!type_completer->popup()->isVisible() && !completion_text(type_edit).isEmpty();

    QgConsole path_console(NULL);
    GEDShellCompleter *path_completer = new GEDShellCompleter(&path_console, gedp);
    path_console.setCompleter(path_completer);
    path_console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    path_console.prompt("$ ");
    path_console.printCommand("draw /al");
    struct ged_cmd_completion_result path_result = GED_CMD_COMPLETION_RESULT_NULL;
    int path_result_count = ged_cmd_complete_result(gedp, "draw /al", std::strlen("draw /al"), &path_result);
    QString path_result_prefix = QString::fromUtf8(path_result.prefix ? path_result.prefix : "");
    QString path_result_candidate = (path_result_count > 0 && path_result.completion_candidates[0]) ?
	QString::fromUtf8(path_result.completion_candidates[0]) : QString();
    ged_cmd_completion_result_clear(&path_result);
    QPlainTextEdit *path_edit = path_console.findChild<QPlainTextEdit *>();
    if (!path_edit) {
	ged_close(gedp);
	return false;
    }
    path_edit->setFocus();
    QTest::keyClick(path_edit, Qt::Key_Tab);
    QString path_after_tab = command_text(path_edit);
    QString path_prefix = path_completer->completionPrefix();
    int path_candidate_count = path_completer->completionCount();
    QString path_candidate = (path_candidate_count > 0) ? path_completer->currentCompletion() : QString();
    QTest::keyClick(path_edit, Qt::Key_Slash);
    QApplication::processEvents();
    QString path_command = command_text(path_edit);
    bool path_continuation_ok = path_command == "draw /all.g/";

    QgConsole middle_console(NULL);
    GEDShellCompleter *middle_completer = new GEDShellCompleter(&middle_console, gedp);
    middle_console.setCompleter(middle_completer);
    middle_console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    middle_console.prompt("$ ");
    middle_console.printCommand(QString::fromUtf8("draw café alXYZ tail"));
    QPlainTextEdit *middle_edit = middle_console.findChild<QPlainTextEdit *>();
    bool middle_completion_ok = middle_edit != NULL;
    if (middle_edit) {
	QTextCursor middle_cursor = middle_edit->textCursor();
	int prompt = middle_edit->toPlainText().lastIndexOf("$ ") + 2;
	middle_cursor.setPosition(prompt + QString::fromUtf8("draw café al").size());
	middle_edit->setTextCursor(middle_cursor);
	middle_edit->setFocus();
	QTest::keyClick(middle_edit, Qt::Key_Tab);
	QApplication::processEvents();
	middle_completion_ok = command_text(middle_edit) ==
	    QString::fromUtf8("draw café all.gXYZ tail");
    }

    QSignalSpy log_spy(&console, &QgConsole::queued_log);
    ged_clbk_set(gedp, "search", BU_CLBK_DURING, &qg_ged_search_exec_callback, (void *)&console);
    const char *search_argv[] = {"search", "all.g", "-name", "tor.r", "-exec", "echo", "{}", ";"};
    int search_ret = ged_exec(gedp, 8, search_argv);
    QApplication::processEvents();
    bool execution_ok = (search_ret == BRLCAD_OK && log_spy.count() > 0);
    if (execution_ok) {
	QString output;
	for (int i = 0; i < log_spy.count(); i++)
	    output.append(log_spy.at(i).at(0).toString());
	execution_ok = output.contains("tor.r");
    }

    if (!empty_completion_ok || !completion_ok || !popup_replace_ok || !path_continuation_ok ||
	    !middle_completion_ok || !execution_ok) {
	qWarning("GED console completion failed: exec=%d popup=%d path=%d middle=%d (%s -> %s, prefix=%s, count=%d, candidate=%s; core prefix=%s, count=%d, candidate=%s) callback=%d",
		(int)completion_ok, (int)popup_replace_ok, (int)path_continuation_ok,
		(int)middle_completion_ok,
		path_after_tab.toLocal8Bit().constData(), path_command.toLocal8Bit().constData(),
		path_prefix.toLocal8Bit().constData(), path_candidate_count,
		path_candidate.toLocal8Bit().constData(), path_result_prefix.toLocal8Bit().constData(),
		path_result_count, path_result_candidate.toLocal8Bit().constData(), (int)execution_ok);
    }
    ged_close(gedp);
    return empty_completion_ok && completion_ok && popup_replace_ok && path_continuation_ok &&
	middle_completion_ok && execution_ok;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    QApplication app(argc, argv);

    if (argc != 2)
	return 20;

    if (!run_mode_test(BU_CMD_COMPLETE_FILTER, "cmd alpha", "r", "cmd ar"))
	return 1;
    if (!run_mode_test(BU_CMD_COMPLETE_CYCLE, "cmd alpha", "r", "cmd alphar"))
	return 2;
    if (!run_mode_test(BU_CMD_COMPLETE_PREFIX, "cmd alp", QString(), QString()))
	return 3;
    if (!run_mode_test(BU_CMD_COMPLETE_OFF, "cmd a", QString(), QString()))
	return 4;
    if (!run_mode_test(BU_CMD_COMPLETE_FILTER, "cmd alpha", " ", "cmd alpha "))
	return 5;
    if (!run_navigation_test())
	return 6;
    if (!run_cycle_navigation_test())
	return 7;
    if (!run_highlight_composition_test())
	return 8;
    if (!run_semantic_debounce_test())
	return 16;
    if (!run_log_queue_test())
	return 9;
    if (!run_ged_semantic_test(argv[1]))
	return 10;
    if (!run_refinement_fallback_test(false, "cmd alpha3"))
	return 11;
    if (!run_refinement_fallback_test(true, "cmd a3"))
	return 12;
    if (!run_long_completion_display_test())
	return 13;
    if (!run_completion_log_repaint_test())
	return 14;
    if (!run_transient_history_test())
	return 15;

    return 0;
}
