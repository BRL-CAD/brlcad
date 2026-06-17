/*                    Q G V I E W F I L T E R . H
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
/** @file QgViewFilter.h
 *
 * Shared Qt event-filter base for view interaction tools.
 */

#ifndef QGVIEWFILTER_H
#define QGVIEWFILTER_H

#include "common.h"

#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include "qtcad/defines.h"
#include "qtcad/QgTypes.h"

struct bsg_view;

/* Contract:
 * - Install on a QgView using QgView::installFilter(QgViewFilter *).
 * - Remove from a QgView using QgView::clearFilter(QgViewFilter *).
 * - The installed QgView supplies and maintains the active bsg_view pointer. */
class QTCAD_EXPORT QgViewFilter : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QgViewFilter)


public:
	explicit QgViewFilter(QObject *parent = nullptr);
	~QgViewFilter() override;

	void set_view(struct bsg_view *nv);
	struct bsg_view *view() const;

	bool eventFilter(QObject *, QEvent *) override
	{
		return false;
	}

signals:
	void view_updated(QgViewUpdateFlags);

protected:
	QMouseEvent *view_sync(QEvent *e);

private:
	class QgViewFilterPrivate;
	QgViewFilterPrivate *m = nullptr;
};

#endif /* QGVIEWFILTER_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
