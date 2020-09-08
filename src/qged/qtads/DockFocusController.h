/*******************************************************************************
** Qt Advanced Docking System
** Copyright (C) 2017 Uwe Kindler
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with this library; If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#ifndef DockFocusControllerH
#define DockFocusControllerH
//============================================================================
/// \file   DockFocusController.h
/// \author Uwe Kindler
/// \date   05.06.2020
/// \brief  Declaration of CDockFocusController class
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include <QObject>
#include "ads_globals.h"
#include "DockManager.h"

namespace ads
{
struct DockFocusControllerPrivate;
class CDockManager;
class CFloatingDockContainer;

/**
 * Manages focus styling of dock widgets and handling of focus changes
 */
class ADS_EXPORT CDockFocusController : public QObject
{
	Q_OBJECT
private:
	DockFocusControllerPrivate* d; ///< private data (pimpl)
    friend struct DockFocusControllerPrivate;

private slots:
	void onApplicationFocusChanged(QWidget *old, QWidget *now);
	void onFocusedDockAreaViewToggled(bool Open);
	void onStateRestored();
	void onDockWidgetVisibilityChanged(bool Visible);

public:
	using Super = QObject;
	/**
	 * Default Constructor
	 */
	CDockFocusController(CDockManager* DockManager);

	/**
	 * Virtual Destructor
	 */
	virtual ~CDockFocusController();

	/**
	 * Helper function to set focus depending on the configuration of the
	 * FocusStyling flag
	 */
	template <class QWidgetPtr>
	static void setWidgetFocus(QWidgetPtr widget)
	{
		if (!CDockManager::testConfigFlag(CDockManager::FocusHighlighting))
		{
			return;
		}

		widget->setFocus(Qt::OtherFocusReason);
	}

	/**
	 * A container needs to call this function if a widget has been dropped
	 * into it
	 */
	void notifyWidgetOrAreaRelocation(QWidget* RelocatedWidget);

	/**
	 * This function is called, if a floating widget has been dropped into
	 * an new position.
	 * When this function is called, all dock widgets of the FloatingWidget
	 * are already inserted into its new position
	 */
	void notifyFloatingWidgetDrop(CFloatingDockContainer* FloatingWidget);

public slots:
	/**
	 * Request a focus change to the given dock widget
	 */
	void setDockWidgetFocused(CDockWidget* focusedNow);
}; // class DockFocusController
}
 // namespace ads
//-----------------------------------------------------------------------------
#endif // DockFocusControllerH

