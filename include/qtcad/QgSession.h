/*                     Q G S E S S I O N . H
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
/** @file QgSession.h
 *
 * Central ownership point for the BRL-CAD GED context used by a Qt
 * application session.
 *
 * QgSession owns the @c struct @c ged * that backs the open database.  All
 * Qt model and widget classes that need access to GED functionality accept a
 * @c QgSession* instead of a raw @c struct @c ged * or @c struct @c db_i *.
 * Ownership is thereby centralised and the coupling between individual
 * consumer classes is reduced to a single, well-defined interface.
 *
 * Signal-driven invalidation
 * --------------------------
 * When the database is opened, closed, or structurally modified, QgSession
 * emits @c db_changed(struct db_i *).  Subscriber models (QgAttributesModel,
 * etc.) connect directly to this signal rather than relying on the
 * application-level @c dbi_update relay that was previously the only
 * fan-out mechanism.
 *
 * Icon provider
 * -------------
 * QgSession maintains an internal cache of per-type icons (QImage objects
 * loaded from embedded resources).  The @c icon() method returns the
 * appropriate cached image for a given @c directory * so that the same
 * pixel data is not reloaded for every node in the tree.
 */

#ifndef QGSESSION_H
#define QGSESSION_H

#include <QHash>
#include <QImage>
#include <QObject>

#include "qtcad/defines.h"
#include "qtcad/QgSignalFlags.h"

/* Forward declarations — implementation includes the full BRL-CAD headers. */
struct bu_vls;
struct bsg_view;
struct db_i;
struct directory;
struct ged;

class QTCAD_EXPORT QgSession : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgSession)

public:
    explicit QgSession(QObject *parent = nullptr);
    ~QgSession();

    /* Return the GED context owned by this session.  Always non-null. */
    struct ged *ged() const
    {
	return gedp;
    }

    /* Return the current database pointer, or nullptr when no file is open. */
    struct db_i *dbip() const;

    /* Notify all db_changed subscribers that the database has changed.
     * Pass the new struct db_i * (may be nullptr on close). */
    void notifyDbChanged(struct db_i *new_dbip);

    /* Notify all view_changed subscribers that the rendered scene may need
     * updating. */
    void notifyViewChanged(QgViewUpdateFlags flags);

    /* Return an icon image appropriate for the given directory entry.
     * The result is cached by object type so that the same QImage pixel data
     * is shared across all entries of the same visual type. */
    QImage icon(struct directory *dp);

signals:
    /* Emitted after a database open, close, or structural edit operation.
     * The argument carries the current struct db_i * (may be nullptr). */
    void db_changed(struct db_i *dbip);

    /* Emitted when the rendered scene view may need updating. */
    void view_changed(QgViewUpdateFlags flags);

private:
    /* Allow QgModel exclusive access to set up the GED context that is
     * owned by this session. */
    friend class QgModel;

    struct ged *gedp = nullptr;

    /* Icon cache: maps an encoded (minor_type, subtype) key to the
     * corresponding QImage loaded from embedded resources. */
    QHash<int, QImage> m_icon_cache;
};

#endif /* QGSESSION_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
