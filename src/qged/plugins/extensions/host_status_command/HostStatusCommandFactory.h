/* H O S T S T A T U S C O M M A N D F A C T O R Y . H
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
/** @file HostStatusCommandFactory.h
 *
 * IQgCommand sample for the Phase 7 qged extension-point slice.
 */

#ifndef HOSTSTATUSCOMMANDFACTORY_H
#define HOSTSTATUSCOMMANDFACTORY_H

#include "qtcad/QgPluginDescriptor.h"
#include "qtcad/QgPluginInterfaces.h"

class HostStatusCommandFactory : public QObject, public IQgCommand
{
    Q_OBJECT
    Q_INTERFACES(IQgCommand)
    Q_PLUGIN_METADATA(IID "org.brlcad.qtcad.IQgCommand/1" FILE "host_status_command.json")

    public:
	QgPluginDescriptor descriptor() const override;
	QString verb() const override;
	QStringList aliases() const override;
	int run(QgPluginContext *ctx,
		const QStringList &argv,
		QString *out,
		QString *err) override;
};

#endif /* HOSTSTATUSCOMMANDFACTORY_H */

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
