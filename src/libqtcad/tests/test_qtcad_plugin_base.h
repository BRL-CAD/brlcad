/*          T E S T _ Q T C A D _ P L U G I N _ B A S E . H
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

#ifndef TEST_QTCAD_PLUGIN_BASE_H
#define TEST_QTCAD_PLUGIN_BASE_H

#include <QObject>

#include "qtcad/QgPluginApi.h"

class QgTestCommandPlugin : public QObject, public IQgCommand
{
    Q_OBJECT
    Q_INTERFACES(IQgCommand)

    public:
	QgTestCommandPlugin() = default;

	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qtcad.test.command");
	    plugin.displayName = QStringLiteral("Test Command");
	    plugin.category = QStringLiteral("test.command");
	    return plugin;
	}

	QString verb() const override
	{
	    return QStringLiteral("qtcad_test");
	}

	int run(QgPluginContext *, const QStringList &arguments,
		QString *output, QString *) override
	{
	    if (output)
		output->append(arguments.join(QLatin1Char(' ')));
	    return 0;
	}
};

#endif /* TEST_QTCAD_PLUGIN_BASE_H */

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
