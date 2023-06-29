/*                   Q G E D P A L E T T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file QgEdPalette.cpp
 *
 */

#include <map>
#include <set>
#include <QColor>
#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/str.h"
#include "plugins/plugin.h"
#include "QgEdPalette.h"

QgEdPalette::QgEdPalette(int mode, QWidget *pparent)
    : QgToolPalette(pparent)
{
    m_mode = mode;

    // Load plugins for this particular palette type
    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "qged", NULL);
    char **filenames;
    struct bu_vls plugin_pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&plugin_pattern, "*%s", QGED_PLUGIN_SUFFIX);
    size_t nfiles = bu_file_list(ppath, bu_vls_cstr(&plugin_pattern), &filenames);
    std::map<int, std::set<QgToolPaletteElement *>> c_map;
    for (size_t i = 0; i < nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "qged", filenames[i], NULL);
	void *dl_handle;
	dl_handle = bu_dlopen(pfile, BU_RTLD_NOW);
	if (!dl_handle) {
	    const char * const error_msg = bu_dlerror();
	    if (error_msg)
		bu_log("%s\n", error_msg);

	    bu_log("Unable to dynamically load '%s' (skipping)\n", pfile);
	    continue;
	}
	{
	    const char *psymbol = "qged_plugin_info";
	    void *info_val = bu_dlsym(dl_handle, psymbol);
	    const struct qged_plugin *(*plugin_info)() = (const struct qged_plugin *(*)())(intptr_t)info_val;
	    if (!plugin_info) {
		const char * const error_msg = bu_dlerror();

		if (error_msg)
		    bu_log("%s\n", error_msg);

		bu_log("Unable to load symbols from '%s' (skipping)\n", pfile);
		bu_log("Could not find '%s' symbol in plugin\n", psymbol);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct qged_plugin *plugin = plugin_info();
	    if (!plugin) {
		bu_log("Invalid plugin file '%s' encountered (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmds) {
		bu_log("Invalid plugin file '%s' encountered (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    if (!plugin->cmd_cnt) {
		bu_log("Plugin '%s' contains no commands, (skipping)\n", pfile);
		bu_dlclose(dl_handle);
		continue;
	    }

	    const struct qged_tool **cmds = plugin->cmds;
	    uint32_t ptype = *((const uint32_t *)(plugin));

	    if (ptype == (uint32_t)mode) {
		for (int c = 0; c < plugin->cmd_cnt; c++) {
		    const struct qged_tool *cmd = cmds[c];
		    QgToolPaletteElement *el = (QgToolPaletteElement *)(*cmd->i->tool_create)();
		    c_map[cmd->palette_priority].insert(el);
		}
	    }
	}
    }
    bu_argv_free(nfiles, filenames);
    bu_vls_free(&plugin_pattern);

    std::map<int, std::set<QgToolPaletteElement *>>::iterator e_it;
    for (e_it = c_map.begin(); e_it != c_map.end(); e_it++) {
	std::set<QgToolPaletteElement *>::iterator el_it;
	for (el_it = e_it->second.begin(); el_it != e_it->second.end(); el_it++) {
	    QgToolPaletteElement *el = *el_it;
	    addElement(el);
	}
    }

    // Add placeholder oc tool until we implement more real tools
    if (mode == QGED_OC_TOOL_PLUGIN) {
	QIcon *obj_icon = new QIcon();
	QString obj_label("primitive controls ");
	QPushButton *obj_control = new QPushButton(obj_label);
	QgToolPaletteElement *el = new QgToolPaletteElement(obj_icon, obj_control);
	addElement(el);
    }
}

void
QgEdPalette::makeCurrent(QWidget *w)
{
    QTCAD_SLOT("QgEdPalette::makeCurrent", 1);
    if (w == this) {
	if (selected)
	    palette_displayElement(selected);
	emit interaction_mode(m_mode);
    } else {
	if (selected)
	    selected->button->setStyleSheet("");
    }
}

QgEdPalette::~QgEdPalette()
{
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

