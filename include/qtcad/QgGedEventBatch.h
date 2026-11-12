/*                  Q G G E D E V E N T B A T C H . H
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
/** @file QgGedEventBatch.h
 *
 * RAII helper for Qt code that performs direct librt database edits.
 */

#ifndef QGGEDEVENTBATCH_H
#define QGGEDEVENTBATCH_H

#include "common.h"

#include "ged/event.h"
#include "qtcad/defines.h"

struct ged;

class QTCAD_EXPORT QgGedEventBatch
{
    public:
	explicit QgGedEventBatch(struct ged *gedp);
	~QgGedEventBatch();

	QgGedEventBatch(const QgGedEventBatch &) = delete;
	QgGedEventBatch &operator=(const QgGedEventBatch &) = delete;
	QgGedEventBatch(QgGedEventBatch &&) = delete;
	QgGedEventBatch &operator=(QgGedEventBatch &&) = delete;

	bool started() const;
	enum ged_event_status end(struct ged_event_result *result = nullptr);

    private:
	struct ged *m_gedp = nullptr;
	bool m_started = false;
};

#endif /* QGGEDEVENTBATCH_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
