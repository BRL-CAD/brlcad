/* H O S T _ S T A T U S _ D I A L O G . C P P
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
/** @file host_status_dialog.cpp
 *
 * IQgDialogFactory sample for the Phase 7 qged extension-point slice.
 */

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include "qtcad/QgPluginContext.h"
#include "HostStatusDialogFactory.h"
#include "../common/HostStatusCommon.h"

QgPluginDescriptor
HostStatusDialogFactory::descriptor() const
{
    QgPluginDescriptor d;
    d.id = "org.brlcad.qged.dialog.host_status";
    d.displayName = "Host Status";
    d.category = "qged.dialog";
    d.sortKey = 0;
    d.description = "Modeless dialog showing the active qged host, database, and view.";
    d.vendor = "BRL-CAD";
    d.version = "1.0";
    return d;
}

QDialog *
HostStatusDialogFactory::create(QgPluginContext *ctx, QWidget *parent)
{
    QDialog *dialog = new QDialog(parent);
    dialog->setWindowTitle(QStringLiteral("Host Status"));

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->addWidget(new HostStatusWidget(ctx, dialog));

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);

    return dialog;
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
