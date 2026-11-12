/*               Q G P L U G I N C O M M A N D S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include "qtcad/QgPluginCommands.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgPluginManager.h"

static void
qg_append(QString *destination, const QString &text)
{
    if (destination)
	destination->append(text);
}

int
QgPluginCommands::run(QgPluginManager *manager,
	const QStringList &arguments, QString *output, QString *error)
{
    if (!manager) {
	qg_append(error, QStringLiteral("plugins: no plugin manager\n"));
	return 1;
    }
    if (arguments.isEmpty()) {
	qg_append(error, helpText());
	return 1;
    }

    const QString command = arguments.first();
    if (command == QLatin1String("list")) {
	qg_append(output, manager->diagnosticSummary());
	return 0;
    }
    if (command == QLatin1String("info")) {
	if (arguments.size() < 2) {
	    qg_append(error, QStringLiteral(
		"plugins info: missing <id>\n"));
	    return 1;
	}
	qg_append(output, manager->diagnosticInfo(arguments.at(1)));
	return 0;
    }
    if (command == QLatin1String("enable") ||
	command == QLatin1String("disable")) {
	if (arguments.size() < 2) {
	    qg_append(error, QStringLiteral(
		"plugins %1: missing <id>\n").arg(command));
	    return 1;
	}
	const bool succeeded = command == QLatin1String("enable") ?
	    manager->enable(arguments.at(1)) :
	    manager->disable(arguments.at(1));
	if (!succeeded) {
	    qg_append(error, QStringLiteral(
		"plugins %1: unknown id '%2'\n")
		.arg(command, arguments.at(1)));
	    return 1;
	}
	qg_append(output, QStringLiteral("plugins: %1 %2\n")
	    .arg(command == QLatin1String("enable") ?
		QStringLiteral("enabled") : QStringLiteral("disabled"),
		arguments.at(1)));
	return 0;
    }
    if (command == QLatin1String("reload")) {
	QStringList failures;
	if (!manager->reload(&failures)) {
	    qg_append(error, QStringLiteral("plugins: reload incomplete\n"));
	    for (const QString &failure : failures)
		qg_append(error, QStringLiteral("  %1\n").arg(failure));
	    return 1;
	}
	qg_append(output, QStringLiteral("plugins: reloaded\n"));
	return 0;
    }

    qg_append(error, QStringLiteral(
	"plugins: unknown subcommand '%1'\n").arg(command));
    qg_append(error, helpText());
    return 1;
}

bool
QgPluginCommands::dispatch(QgPluginManager *manager,
	QgPluginContext *context, const QString &category,
	const QStringList &arguments, int *result,
	QString *output, QString *error)
{
    if (!manager || arguments.isEmpty())
	return false;

    const QString commandName = arguments.first();
    const QList<IQgCommand *> commands =
	manager->factories<IQgCommand>(category);
    IQgCommand *match = nullptr;

    for (IQgCommand *command : commands) {
	if (!command || command->verb() != commandName)
	    continue;
	if (match) {
	    qg_append(error, QStringLiteral(
		"%1: multiple plugins define this command\n")
		.arg(commandName));
	    if (result)
		*result = 1;
	    return true;
	}
	match = command;
    }

    if (!match) {
	for (IQgCommand *command : commands) {
	    if (!command || !command->aliases().contains(commandName))
		continue;
	    if (match) {
		qg_append(error, QStringLiteral(
		    "%1: multiple plugins define this alias\n")
		    .arg(commandName));
		if (result)
		    *result = 1;
		return true;
	    }
	    match = command;
	}
    }

    if (!match)
	return false;

    const int commandResult = match->run(context, arguments, output, error);
    if (result)
	*result = commandResult;
    return true;
}

QString
QgPluginCommands::helpText()
{
    return QStringLiteral(
	"Usage: plugins <subcommand> [args]\n"
	"  plugins list                  List discovered plugins.\n"
	"  plugins info <id>             Show details for a plugin id.\n"
	"  plugins enable <id>           Enable a plugin (persisted).\n"
	"  plugins disable <id>          Disable a plugin (persisted).\n"
	"  plugins reload                Unload and rescan search paths.\n");
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
