/*                  Q G C O N S O L E _ C O M P L E T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStringListModel>
#include <QSignalSpy>
#include <QtTest/QTest>

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
	highlights.clear();
	if (line.size() >= 3)
	    highlights.push_back({0, 3, QG_CONSOLE_COMMAND});
	if (line.size() > 4)
	    highlights.push_back({4, (int)line.size(), QG_CONSOLE_INVALID});
    }
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
    return (prompt >= 0) ? text.mid(prompt + 2) : text;
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
    if (command_text(edit) != expected_after_tab)
	return false;
    bool preview_mode = (mode == BU_CMD_COMPLETE_FILTER || mode == BU_CMD_COMPLETE_CYCLE);
    if (preview_mode) {
	if (edit->extraSelections().size() != 1 ||
		edit->extraSelections().front().cursor.selectedText() != "lpha")
	    return false;
    } else if (!edit->extraSelections().isEmpty()) {
	return false;
    }

    if (!typed.isNull()) {
	QTest::keyClicks(edit, typed);
	QApplication::processEvents();
	if (command_text(edit) != expected_after_type)
	    return false;
	if (!edit->extraSelections().isEmpty())
	    return false;
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
    console.resize(430, 260);
    console.show();
    console.prompt("$ ");
    console.printCommand("cmd ");
    QPlainTextEdit *edit = console.findChild<QPlainTextEdit *>();
    if (!edit)
	return false;
    edit->setFocus();

    QTest::keyClick(edit, Qt::Key_Tab);
    QApplication::processEvents();
    QLabel *display = edit->findChild<QLabel *>(QStringLiteral("completionCandidateDisplay"));

    /* The exact smaller bins depend on the font's measured cell width. */
    if (!display || !display->isVisible() ||
	(!display->text().contains(" (") &&
	 !display->text().contains("matches)") &&
	 !display->text().contains("more)"))) {
	qWarning("long completion display failed: visible=%d text=[%s]",
		display ? (int)display->isVisible() : 0,
		display ? display->text().toLocal8Bit().constData() : "missing");
	return false;
	}

    QTest::keyClick(edit, Qt::Key_Escape);
    QApplication::processEvents();
    return !display->isVisible() && command_text(edit) == "cmd ";
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
	    selections[1].cursor.selectedText() != "a")
	return false;

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
    if (!have_command || !have_preview)
	return false;

    QTest::keyClick(edit, Qt::Key_Space);
    QApplication::processEvents();
    selections = edit->extraSelections();
    for (const QTextEdit::ExtraSelection &selection : selections) {
	if (selection.cursor.selectedText() == "lpha")
	    return false;
    }
    return !selections.isEmpty();
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
    QPlainTextEdit *completion_edit = completion_console.findChild<QPlainTextEdit *>();
    if (!completion_edit) {
	ged_close(gedp);
	return false;
    }
    completion_edit->setFocus();
    QTest::keyClick(completion_edit, Qt::Key_Tab);
    QApplication::processEvents();
    bool completion_ok = command_text(completion_edit) == "search -exec draw";

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
    type_completer->popup()->setCurrentIndex(tor_index);
    QTest::keyClick(type_edit, Qt::Key_Return);
    QApplication::processEvents();
    bool popup_replace_ok = command_text(type_edit) == "search -type tor";

    QgConsole path_console(NULL);
    GEDShellCompleter *path_completer = new GEDShellCompleter(&path_console, gedp);
    path_console.setCompleter(path_completer);
    path_console.setCompletionMode(BU_CMD_COMPLETE_FILTER);
    path_console.prompt("$ ");
    path_console.printCommand("draw /al");
    struct ged_cmd_completion_result path_result = GED_CMD_COMPLETION_RESULT_NULL;
    int path_result_count = ged_cmd_complete_result(gedp, "draw /al", std::strlen("draw /al"), &path_result);
    QString path_result_prefix = QString::fromLocal8Bit(path_result.prefix ? path_result.prefix : "");
    QString path_result_candidate = (path_result_count > 0 && path_result.completion_candidates[0]) ?
	QString::fromLocal8Bit(path_result.completion_candidates[0]) : QString();
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

    if (!completion_ok || !popup_replace_ok || !path_continuation_ok || !execution_ok) {
	qWarning("GED console completion failed: exec=%d popup=%d path=%d (%s -> %s, prefix=%s, count=%d, candidate=%s; core prefix=%s, count=%d, candidate=%s) callback=%d",
		(int)completion_ok, (int)popup_replace_ok, (int)path_continuation_ok,
		path_after_tab.toLocal8Bit().constData(), path_command.toLocal8Bit().constData(),
		path_prefix.toLocal8Bit().constData(), path_candidate_count,
		path_candidate.toLocal8Bit().constData(), path_result_prefix.toLocal8Bit().constData(),
		path_result_count, path_result_candidate.toLocal8Bit().constData(), (int)execution_ok);
    }
    ged_close(gedp);
    return completion_ok && popup_replace_ok && path_continuation_ok && execution_ok;
}


int
main(int argc, char **argv)
{
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
    if (!run_highlight_composition_test())
	return 7;
    if (!run_log_queue_test())
	return 8;
    if (!run_ged_semantic_test(argv[1]))
	return 9;
    if (!run_refinement_fallback_test(false, "cmd alpha3"))
	return 10;
    if (!run_refinement_fallback_test(true, "cmd a3"))
	return 11;
    if (!run_long_completion_display_test())
	return 12;

    return 0;
}
