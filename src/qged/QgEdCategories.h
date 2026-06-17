/*                 Q G E D C A T E G O R I E S . H
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
/** @file QgEdCategories.h
 *
 * qged-owned category name constants for Qt plugin composition.
 *
 * These strings identify the plugin groups that qged owns:
 *
 *   QGED_CATEGORY_VIEW   - view-control tools (the "View Controls" dock).
 *   QGED_CATEGORY_OBJECT - object-editing tools (the "Object Editing" dock).
 *   QGED_CATEGORY_PANEL  - dockable non-tool panels owned by qged.
 *   QGED_CATEGORY_DIALOG - top-level dialogs launched from qged menus.
 *   QGED_CATEGORY_COMMAND - qged-specific console commands.
 *
 * They are used in:
 *   - QgPaletteController construction in QgEdMainWindow
 *   - QgPluginDescriptor::category metadata in converted qged plugins
 *   - InteractionMode / ActivePaletteCategory logic in QgEdMainWindow
 *   - qged-owned panel/dialog/command composition code
 *
 * Keep these namespaced ("qged.*") so that other libqtcad-based hosts
 * can have their own categories without colliding with qged.
 */

#ifndef QGEDCATEGORIES_H
#define QGEDCATEGORIES_H

#define QGED_CATEGORY_VIEW   "qged.view"
#define QGED_CATEGORY_OBJECT "qged.object"
#define QGED_CATEGORY_PANEL  "qged.panel"
#define QGED_CATEGORY_DIALOG "qged.dialog"
#define QGED_CATEGORY_COMMAND "qged.command"

#endif /* QGEDCATEGORIES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
