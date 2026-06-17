/*           H O S T S T A T U S C O M M O N . H
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
/** @file HostStatusCommon.h
 *
 * Shared helpers for the sample Phase 7 non-tool qged plugins.
 */

#ifndef HOSTSTATUSCOMMON_H
#define HOSTSTATUSCOMMON_H

#include <QWidget>

class QLabel;
class QgPluginContext;

QString qged_host_status_summary(QgPluginContext *ctx);

class HostStatusWidget : public QWidget
{
    public:
	explicit HostStatusWidget(QgPluginContext *ctx, QWidget *parent = nullptr);
	void refresh();

    private:
	void requestViewRefresh();

	QgPluginContext *m_ctx = nullptr;
	QLabel *m_host_value = nullptr;
	QLabel *m_db_value = nullptr;
	QLabel *m_view_value = nullptr;
	QLabel *m_summary_value = nullptr;
};

#endif /* HOSTSTATUSCOMMON_H */

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
