/*               Q G P L U G I N C O M M A N D S . H
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

#ifndef QGPLUGINCOMMANDS_H
#define QGPLUGINCOMMANDS_H

#include <QString>
#include <QStringList>

#include "qtcad/defines.h"

class QgPluginManager;

/** Standard host-console commands for inspecting a QgPluginManager. */
class QTCAD_EXPORT QgPluginCommands
{
    public:
	/** Run a plugin subcommand, appending text to optional output sinks. */
	static int run(QgPluginManager *manager, const QStringList &arguments,
		QString *output, QString *error);
	/** Return usage text for the supported plugin subcommands. */
	static QString helpText();
};

#endif /* QGPLUGINCOMMANDS_H */

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
