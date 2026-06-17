/*                Q G P L U G I N D E S C R I P T O R . H
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
/** @file QgPluginDescriptor.h
 *
 * Plain data describing a single libqtcad plugin factory.  Used by
 * QgPluginManager to advertise plugins to a host (and by JSON metadata
 * read via QPluginLoader::metaData() before the underlying .so is
 * actually loaded).
 *
 * Hosts must not assume that any particular field beyond @c id and
 * @c displayName is populated -- defaults are sensible empty values.
 *
 * Category is a free-form host-supplied string (e.g. "qged.view",
 * "qged.object", "archer.modeling") so that libqtcad does not have to
 * bake in any application-specific namespace.
 */
#ifndef QGPLUGINDESCRIPTOR_H
#define QGPLUGINDESCRIPTOR_H

#include "common.h"

#include <QString>
#include "qtcad/defines.h"

class QTCAD_EXPORT QgPluginDescriptor
{
    public:
	QgPluginDescriptor() = default;

	/* Unique, stable identifier for this factory, e.g.
	 * "org.brlcad.qged.polygon.create".  Used by allow/deny
	 * lists, "plugins info <id>" diagnostics, and tie-breaking
	 * when sortKey collides. */
	QString id;

	/* Human-readable label shown in palettes/menus. */
	QString displayName;

	/* Host-supplied category bucket, e.g. "qged.view". */
	QString category;

	/* Resource path or filesystem path to an icon. */
	QString iconName;

	/* Optional accelerator hint shown in tooltips. */
	QString shortcutHint;

	/* Lower values sort earlier within a category.  Ties broken
	 * by id. */
	int sortKey = 0;

	/* If true, the host should hide/disable this factory until a
	 * .g database is open. */
	bool requiresOpenDb = false;

	/* If true, the host should hide/disable this factory until a
	 * bsg_view is available. */
	bool requiresView = false;

	/* Free-form long description for the "plugins info" command. */
	QString description;

	/* Optional vendor / version strings (e.g. used to detect ABI
	 * skew when third-party plugins are mixed). */
	QString vendor;
	QString version;
};

#endif /* QGPLUGINDESCRIPTOR_H */

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
