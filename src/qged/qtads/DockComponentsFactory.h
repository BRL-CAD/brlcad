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

#ifndef DockComponentsFactoryH
#define DockComponentsFactoryH
//============================================================================
/// \file   DockComponentsFactory.h
/// \author Uwe Kindler
/// \date   10.02.2020
/// \brief  Declaration of DockComponentsFactory
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "ads_globals.h"

namespace ads
{
class CDockWidgetTab;
class CDockAreaTitleBar;
class CDockAreaTabBar;
class CDockAreaWidget;
class CDockWidget;



/**
 * Factory for creation of certain GUI elements for the docking framework.
 * A default unique instance provided by CDockComponentsFactory is used for
 * creation of all supported components. To inject your custom components,
 * you can create your own derived dock components factory and register
 * it via setDefaultFactory() function.
 * \code
 * CDockComponentsFactory::setDefaultFactory(new MyComponentsFactory()));
 * \endcode
 */
class ADS_EXPORT CDockComponentsFactory
{
public:
	/**
	 * Force virtual destructor
	 */
	virtual ~CDockComponentsFactory() {}

	/**
	 * This default implementation just creates a dock widget tab with
	 * new CDockWidgetTab(DockWIdget).
	 */
	virtual CDockWidgetTab* createDockWidgetTab(CDockWidget* DockWidget) const;

	/**
	 * This default implementation just creates a dock area tab bar with
	 * new CDockAreaTabBar(DockArea).
	 */
	virtual CDockAreaTabBar* createDockAreaTabBar(CDockAreaWidget* DockArea) const;

	/**
	 * This default implementation just creates a dock area title bar with
	 * new CDockAreaTitleBar(DockArea).
	 */
	virtual CDockAreaTitleBar* createDockAreaTitleBar(CDockAreaWidget* DockArea) const;

	/**
	 * Returns the default components factory
	 */
	static const CDockComponentsFactory* factory();

	/**
	 * Sets a new default factory for creation of GUI elements.
	 * This function takes ownership of the given Factory.
	 */
	static void setFactory(CDockComponentsFactory* Factory);

	/**
	 * Resets the current factory to the
	 */
	static void resetDefaultFactory();
};


/**
 * Convenience function to ease factory instance access
 */
inline const CDockComponentsFactory* componentsFactory()
{
	return CDockComponentsFactory::factory();
}

} // namespace ads

//---------------------------------------------------------------------------
#endif // DockComponentsFactoryH
