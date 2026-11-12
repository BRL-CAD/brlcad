/*                  Q G G E D E V E N T B A T C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "qtcad/QgGedEventBatch.h"

QgGedEventBatch::QgGedEventBatch(struct ged *gedp) :
    m_gedp(gedp),
    m_started(gedp && ged_event_batch_begin(gedp) == GED_EVENT_OK)
{
}

QgGedEventBatch::~QgGedEventBatch()
{
    end();
}

bool
QgGedEventBatch::started() const
{
    return m_started;
}

enum ged_event_status
QgGedEventBatch::end(struct ged_event_result *result)
{
    if (!m_started)
	return GED_EVENT_OK;
    m_started = false;
    return ged_event_batch_end(m_gedp, result);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
