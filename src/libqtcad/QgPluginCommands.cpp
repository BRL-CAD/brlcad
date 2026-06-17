/*              Q G P L U G I N C O M M A N D S . C P P
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
#include "qtcad/QgPluginManager.h"

static void
appendStr(QString *dst, const QString &s)
{
    if (dst)
	dst->append(s);
}

int
QgPluginCommands::run(QgPluginManager *manager,
		      const QStringList &argv,
		      QString *out,
		      QString *err)
{
    if (!manager) {
	appendStr(err, QStringLiteral("plugins: no plugin manager\n"));
	return 1;
    }
    if (argv.isEmpty()) {
	appendStr(err, helpText());
	return 1;
    }

    const QString verb = argv.value(0);
    if (verb == QLatin1String("list")) {
	appendStr(out, manager->diagnosticSummary());
	return 0;
    }
    if (verb == QLatin1String("info")) {
	if (argv.size() < 2) {
	    appendStr(err, QStringLiteral("plugins info: missing <id>\n"));
	    return 1;
	}
	appendStr(out, manager->diagnosticInfo(argv.value(1)));
	return 0;
    }
    if (verb == QLatin1String("enable") || verb == QLatin1String("disable")) {
	if (argv.size() < 2) {
	    appendStr(err, QStringLiteral("plugins %1: missing <id>\n").arg(verb));
	    return 1;
	}
	bool ok = (verb == QLatin1String("enable"))
	    ? manager->enable(argv.value(1))
	    : manager->disable(argv.value(1));
	if (!ok) {
	    appendStr(err, QStringLiteral("plugins %1: unknown id '%2'\n")
				.arg(verb, argv.value(1)));
	    return 1;
	}
	appendStr(out, QStringLiteral("plugins: %1 %2\n")
			    .arg(verb == QLatin1String("enable")
				    ? QStringLiteral("enabled")
				    : QStringLiteral("disabled"),
				 argv.value(1)));
	return 0;
    }
    if (verb == QLatin1String("reload")) {
	manager->reload();
	appendStr(out, QStringLiteral("plugins: reloaded\n"));
	return 0;
    }

    appendStr(err, QStringLiteral("plugins: unknown subcommand '%1'\n").arg(verb));
    appendStr(err, helpText());
    return 1;
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
