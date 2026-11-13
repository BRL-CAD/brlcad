/*                 V I E W _ S E T T I N G S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @file view_settings.cpp
 *
 */

#include <QIcon>
#include <QPixmap>

#include "qtcad/QgPluginContext.h"
#include "qtcad/QgPluginInterfaces.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "QgEdCategories.h"
#include "CADViewSettings.h"

class ViewSettingsTool : public QgToolBase
{
    Q_OBJECT

    public:
	explicit ViewSettingsTool(QgPluginContext *context,
		QObject *parent = nullptr)
	    : QgToolBase(context, parent)
	{
	}

	QgToolPaletteElement *paletteElement() override
	{
	    QgToolPaletteElement *element = QgToolBase::paletteElement();
	    if (m_settings && element && !m_settingsWired) {
		connect(m_settings, &CADViewSettings::settings_changed,
			element, &QgToolPaletteElement::element_view_changed);
		m_settingsWired = true;
	    }
	    return element;
	}

	QWidget *createWidget() override
	{
	    m_settings = new CADViewSettings();
	    m_settings->setContext(context());
	    m_settings->checkbox_refresh(QG_VIEW_REFRESH);
	    return m_settings;
	}

	QIcon *createIcon() override
	{
	    return new QIcon(QPixmap(":settings.svg"));
	}

    public slots:
	void refresh() override
	{
	    if (m_settings)
		m_settings->checkbox_refresh(QG_VIEW_REFRESH);
	}

	void onViewChanged() override
	{
	    refresh();
	}

    private:
	CADViewSettings *m_settings = nullptr;
	bool m_settingsWired = false;
};

class ViewSettingsFactory : public QObject, public IQgToolFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgToolFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgToolFactory")
	FILE "view_settings.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qged.view.settings");
	    plugin.displayName = QStringLiteral("View Settings");
	    plugin.category = QStringLiteral(QGED_CATEGORY_VIEW);
	    plugin.iconName = QStringLiteral(":settings.svg");
	    plugin.sortKey = 1;
	    plugin.requiresView = true;
	    plugin.description = QStringLiteral(
		    "Controls faceplate and view parameter display settings.");
	    plugin.vendor = QStringLiteral("BRL-CAD");
	    plugin.version = QStringLiteral("1.0");
	    return plugin;
	}

	QgToolBase *create(QgPluginContext *context,
		QObject *parent = nullptr) override
	{
	    return new ViewSettingsTool(context, parent);
	}
};

#include "view_settings.moc"



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
