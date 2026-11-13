/*                     Q G S E S S I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file QgSession.h
 *
 * Central ownership point for a Qt application's GED context.
 */

#ifndef QGSESSION_H
#define QGSESSION_H

#include <QObject>

#include "qtcad/defines.h"
#include "qtcad/QgSignalFlags.h"

struct bview;
struct db_i;
struct ged;

class QTCAD_EXPORT QgSession : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgSession)

public:
    explicit QgSession(QObject *parent = nullptr);
    ~QgSession() override;

    struct ged *ged() const;
    struct db_i *dbip() const;
    struct bview *activeView() const;
    void setActiveView(struct bview *view);

    void notifyDbChanged(struct db_i *dbip);
    void notifyViewChanged(QgViewUpdateFlags flags);

signals:
    void db_changed(struct db_i *dbip);
    void view_changed(QgViewUpdateFlags flags);

private:
    struct ged *m_gedp = nullptr;
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
