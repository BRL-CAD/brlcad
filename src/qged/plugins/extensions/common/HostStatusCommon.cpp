/*           H O S T S T A T U S C O M M O N . C P P
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
/** @file HostStatusCommon.cpp
 *
 * Shared helpers for the sample Phase 7 non-tool qged plugins.
 */

#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

#include "bu/vls.h"
#include "ged.h"
#include "qtcad/QgPluginContext.h"
#include "qtcad/QgSignalFlags.h"
#include "HostStatusCommon.h"

static QString
qged_host_db_name(QgPluginContext *ctx)
{
    if (!ctx)
	return QStringLiteral("Unavailable");
    struct ged *gedp = ctx->getGed();
    if (!gedp || !gedp->dbip || !gedp->dbip->dbi_filename)
	return QStringLiteral("No database open");
    return QString::fromLocal8Bit(gedp->dbip->dbi_filename);
}

static QString
qged_host_view_name(QgPluginContext *ctx)
{
    if (!ctx)
	return QStringLiteral("Unavailable");
    struct bsg_view *view = ctx->getView();
    if (!view)
	return QStringLiteral("No active view");
    if (!bu_vls_strlen(&view->gv_name))
	return QStringLiteral("Active view");
    return QString::fromLocal8Bit(bu_vls_cstr(&view->gv_name));
}

QString
qged_host_status_summary(QgPluginContext *ctx)
{
    QString host = (ctx && !ctx->hostName.isEmpty()) ? ctx->hostName : QStringLiteral("unknown");
    return QStringLiteral("Host: %1\nDatabase: %2\nView: %3\n")
	.arg(host, qged_host_db_name(ctx), qged_host_view_name(ctx));
}

HostStatusWidget::HostStatusWidget(QgPluginContext *ctx, QWidget *parent)
    : QWidget(parent), m_ctx(ctx)
{
    QFormLayout *layout = new QFormLayout(this);

    m_host_value = new QLabel(this);
    m_db_value = new QLabel(this);
    m_view_value = new QLabel(this);
    m_summary_value = new QLabel(this);
    m_summary_value->setWordWrap(true);

    QPushButton *refresh_button = new QPushButton(QStringLiteral("Refresh View"), this);

    layout->addRow(QStringLiteral("Host"), m_host_value);
    layout->addRow(QStringLiteral("Database"), m_db_value);
    layout->addRow(QStringLiteral("View"), m_view_value);
    layout->addRow(QStringLiteral("Summary"), m_summary_value);
    layout->addRow(refresh_button);

    QObject::connect(refresh_button, &QPushButton::clicked,
		     this, [this]() { requestViewRefresh(); });

    if (m_ctx && m_ctx->notifier) {
	QObject::connect(m_ctx->notifier, &QgPluginNotifier::dbChanged,
			 this, [this]() { refresh(); });
	QObject::connect(m_ctx->notifier, &QgPluginNotifier::viewChanged,
			 this, [this]() { refresh(); });
	QObject::connect(m_ctx->notifier, &QgPluginNotifier::viewUpdated,
			 this, [this](unsigned long long) { refresh(); });
	QObject::connect(m_ctx->notifier, &QgPluginNotifier::settingsChanged,
			 this, [this]() { refresh(); });
    }

    refresh();
}

void
HostStatusWidget::refresh()
{
    QString host = (m_ctx && !m_ctx->hostName.isEmpty()) ? m_ctx->hostName : QStringLiteral("unknown");
    m_host_value->setText(host);
    m_db_value->setText(qged_host_db_name(m_ctx));
    m_view_value->setText(qged_host_view_name(m_ctx));
    m_summary_value->setText(qged_host_status_summary(m_ctx));
}

void
HostStatusWidget::requestViewRefresh()
{
    if (m_ctx && m_ctx->notifier)
	emit m_ctx->notifier->viewUpdated(QG_VIEW_REFRESH);
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
