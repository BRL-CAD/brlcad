/*                     Q G T O O L B A S E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef QGTOOLBASE_H
#define QGTOOLBASE_H

#include "common.h"

#include <QObject>
#include <QPointer>

#include "qtcad/defines.h"
#include "qtcad/QgPluginDescriptor.h"

class QIcon;
class QWidget;
class QgPaletteController;
class QgPluginContext;
class QgToolPaletteElement;

/** Host-neutral base class for tools created by IQgToolFactory. */
class QTCAD_EXPORT QgToolBase : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QgToolBase)

    public:
	explicit QgToolBase(QgPluginContext *context,
		QObject *parent = nullptr);
	~QgToolBase() override;

	virtual QgPluginDescriptor descriptor() const
	{
	    return QgPluginDescriptor();
	}
	virtual QWidget *createWidget() = 0;
	virtual QObject *viewEventFilter() { return nullptr; }
	virtual QIcon *createIcon() { return nullptr; }
	virtual QgToolPaletteElement *paletteElement();

	QgPluginContext *context() const { return m_context; }
	bool isActive() const { return m_active; }

    signals:
	void activeChanged(bool active);

    public slots:
	virtual void refresh() {}
	virtual void onDbChanged() {}
	virtual void onViewChanged() {}
	virtual void onSettingsChanged() {}

    private:
	friend class QgPaletteController;
	void setActive(bool active);

	QgPluginContext *m_context = nullptr;
	QPointer<QWidget> m_widget;
	QPointer<QgToolPaletteElement> m_element;
	bool m_signalsWired = false;
	bool m_active = false;
};

#endif /* QGTOOLBASE_H */

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
