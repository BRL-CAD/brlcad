/*                    Q G S E S S I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file QgSession.cpp
 *
 * Qt application session ownership.
 */

#include "common.h"

#include "ged.h"
#include "../libged/dbi.h"
#include "qtcad/QgSession.h"

QgSession::QgSession(QObject *parent)
    : QObject(parent), m_gedp(ged_create())
{
    m_gedp->dbi_state = new DbiState(m_gedp);
    m_gedp->new_cmd_forms = 1;
}

QgSession::~QgSession()
{
    delete static_cast<DbiState *>(m_gedp->dbi_state);
    m_gedp->dbi_state = nullptr;
    ged_close(m_gedp);
    m_gedp = nullptr;
}

struct ged *
QgSession::ged() const
{
    return m_gedp;
}

struct db_i *
QgSession::dbip() const
{
    return m_gedp->dbip;
}

struct bview *
QgSession::activeView() const
{
    return m_gedp->ged_gvp;
}

void
QgSession::setActiveView(struct bview *view)
{
    m_gedp->ged_gvp = view;
}

void
QgSession::notifyDbChanged(struct db_i *dbip)
{
    emit db_changed(dbip);
}

void
QgSession::notifyViewChanged(unsigned long long flags)
{
    emit view_changed(flags);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
