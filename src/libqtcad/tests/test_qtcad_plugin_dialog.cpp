/*      T E S T _ Q T C A D _ P L U G I N _ D I A L O G . C P P
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

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>

#include "qtcad/QgPluginInterfaces.h"

class QgTestDialogPlugin : public QObject, public IQgDialogFactory
{
    Q_OBJECT
    Q_INTERFACES(IQgDialogFactory)
    Q_PLUGIN_METADATA(IID QGTCAD_IID("IQgDialogFactory")
	FILE "test_qtcad_plugin_dialog.json")

    public:
	QgPluginDescriptor descriptor() const override
	{
	    QgPluginDescriptor plugin;
	    plugin.id = QStringLiteral("org.brlcad.qtcad.test.dialog");
	    plugin.displayName = QStringLiteral("Test Dialog");
	    plugin.category = QStringLiteral("test.dialog");
	    return plugin;
	}

	QDialog *create(QgPluginContext *,
		QWidget *parent = nullptr) override
	{
	    QDialog *dialog = new QDialog(parent);
	    QVBoxLayout *layout = new QVBoxLayout(dialog);
	    layout->addWidget(new QLabel(QStringLiteral("dialog"), dialog));
	    return dialog;
	}
};

#include "test_qtcad_plugin_dialog.moc"

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
