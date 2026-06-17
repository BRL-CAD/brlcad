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

#include "ged/event_txn.h"
#include "qtcad/defines.h"

struct ged;

class QTCAD_EXPORT QgGedEventBatch {
public:
    explicit QgGedEventBatch(struct ged *gedp) :
	m_gedp(gedp),
	m_started(gedp && ged_event_batch_begin(gedp) > 0)
    {
    }

    ~QgGedEventBatch()
    {
	end();
    }

    QgGedEventBatch(const QgGedEventBatch &) = delete;
    QgGedEventBatch &operator=(const QgGedEventBatch &) = delete;
    QgGedEventBatch(QgGedEventBatch &&) = delete;
    QgGedEventBatch &operator=(QgGedEventBatch &&) = delete;

    bool started() const
    {
	return m_started;
    }

    int end(struct ged_event_txn_result *result = nullptr)
    {
	if (!m_started)
	    return 0;
	m_started = false;
	return ged_event_batch_end(m_gedp, result);
    }

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
