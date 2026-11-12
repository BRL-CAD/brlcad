/*             Q G P L U G I N D E S C R I P T O R . H
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

#ifndef QGPLUGINDESCRIPTOR_H
#define QGPLUGINDESCRIPTOR_H

#include <QString>

/** Host-neutral metadata describing one Qt plugin factory. */
class QgPluginDescriptor
{
    public:
	QgPluginDescriptor() = default;

	/** Stable identifier used for settings and diagnostics. */
	QString id;
	/** Human-readable label for menus and palettes. */
	QString displayName;
	/** Host-defined grouping key. */
	QString category;
	/** Resource or filesystem path to the plugin icon. */
	QString iconName;
	/** Optional accelerator hint for user interfaces. */
	QString shortcutHint;
	/** Ordering key within a category; lower values sort first. */
	int sortKey = 0;
	/** Whether the factory requires an open geometry database. */
	bool requiresOpenDb = false;
	/** Whether the factory requires an active graphical view. */
	bool requiresView = false;
	/** Longer text for plugin diagnostics or help. */
	QString description;
	/** Optional plugin provider. */
	QString vendor;
	/** Optional plugin implementation version. */
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
