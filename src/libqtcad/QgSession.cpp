/*                    Q G S E S S I O N . C P P
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
/** @file QgSession.cpp
 *
 * Implementation of the QgSession GED ownership and icon-provider class.
 */

#include "common.h"

#include "bu/env.h"
#include "raytrace.h"

#include "qtcad/QgSession.h"
#include "qtcad/QgUtil.h"
#include "qtcad/QgSignalFlags.h"


/* ---------------------------------------------------------------------------
 * Icon cache helpers
 * ---------------------------------------------------------------------------
 *
 * The cache is keyed by an integer that encodes both the minor DB5 type and,
 * for types whose icon depends on further inspection (ARB8 subtype, comb
 * type), an additional sub-type discriminator:
 *
 *   key = (d_minor_type * 16) + subtype
 *
 * where subtype is:
 *   0   — most primitive types (no further discrimination required)
 *   0-8 — ARB8 actual type (return value of get_arb_type; 0 = unknown/other)
 *   0-4 — combination type  (G_STANDARD_OBJ / G_REGION / G_AIR / …)
 *
 * The special key -1 is used when dp or dbip is invalid.
 */

static int
session_icon_key(struct directory *dp, struct db_i *dbip)
{
    if (!dp || dbip == DBI_NULL)
	return -1;

    int sub = 0;
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_ARB8) {
	/* get_arb_type reads the actual geometry to determine ARB4-8 or 0. */
	const struct bn_tol arb_tol = BN_TOL_INIT_TOL;
	struct rt_db_internal intern;
	if (rt_db_get_internal(&intern, dp, dbip, static_cast<fastf_t *>(nullptr)) >= 0) {
	    sub = rt_arb_std_type(&intern, &arb_tol);
	    rt_db_free_internal(&intern);
	}
    } else if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	sub = QgCombType(dp, dbip);
    }

    return (int)dp->d_minor_type * 16 + sub;
}


/* ---------------------------------------------------------------------------
 * QgSession
 * ---------------------------------------------------------------------------*/

QgSession::QgSession(QObject *parent)
    : QObject(parent)
{
    /* Allocate and initialise the GED context.  This is done here rather than
     * in QgModel so that ownership is unambiguously in one place. */
    BU_GET(gedp, struct ged);
    ged_init(gedp);

    bu_setenv("DM_SWRAST", "1", 1);
}

QgSession::~QgSession()
{
    ged_close(gedp);
    gedp = nullptr;
}

struct db_i *
QgSession::dbip() const
{
    return gedp ? gedp->dbip : nullptr;
}

void
QgSession::notifyDbChanged(struct db_i *new_dbip)
{
    emit db_changed(new_dbip);
}

void
QgSession::notifyViewChanged(QgViewUpdateFlags flags)
{
    emit view_changed(flags);
}

QImage
QgSession::icon(struct directory *dp)
{
    int key = session_icon_key(dp, dbip());

    auto it = m_icon_cache.constFind(key);
    if (it != m_icon_cache.constEnd())
	return it.value();

    /* Cache miss — load the icon via QgIcon (which embeds the full decision
     * tree for every BRL-CAD primitive type) and store it. */
    QImage img = QgIcon(dp, dbip());
    m_icon_cache[key] = img;
    return img;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
