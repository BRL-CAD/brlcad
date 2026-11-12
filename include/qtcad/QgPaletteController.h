/*               Q G P A L E T T E C O N T R O L L E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef QGPALETTECONTROLLER_H
#define QGPALETTECONTROLLER_H

#include "common.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

#include "qtcad/defines.h"

class QgPluginContext;
class QgPluginManager;
class QgToolBase;
class QgToolPalette;
class QgToolPaletteElement;
class QgView;

/** Populate and manage a tool palette from host-neutral Qt plugins. */
class QTCAD_EXPORT QgPaletteController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgPaletteController)

    public:
	QgPaletteController(QgToolPalette *palette,
		QgPluginManager *manager, const QString &category,
		QgPluginContext *context, QObject *parent = nullptr);
	~QgPaletteController() override;

	QString category() const { return m_category; }
	QgToolPalette *palette() const;
	QgToolBase *currentTool() const { return m_currentTool; }
	QgView *activeView() const;

	void populate();
	void clear();
	void setActiveView(QgView *view);

    signals:
	void currentToolChanged(QgToolBase *tool);

    private slots:
	void onElementSelected(QgToolPaletteElement *element);
	void onPaletteDestroyed();

    private:
	void attachFilter(QgToolBase *tool);
	void detachFilter(QgToolBase *tool);
	void removeTool(const QString &id);

	QPointer<QgToolPalette> m_palette;
	QPointer<QgPluginManager> m_manager;
	QString m_category;
	QgPluginContext *m_context = nullptr;
	QPointer<QgView> m_view;
	QHash<QString, QgToolBase *> m_tools;
	QHash<QgToolPaletteElement *, QString> m_elementIds;
	QgToolBase *m_currentTool = nullptr;
};

#endif /* QGPALETTECONTROLLER_H */

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
