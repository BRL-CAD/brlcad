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

//============================================================================
/// \file   DockComponentsFactory.cpp
/// \author Uwe Kindler
/// \date   10.02.2020
/// \brief  Implementation of DockComponentsFactory
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "DockComponentsFactory.h"

#include <memory>

#include "DockWidgetTab.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockWidget.h"
#include "DockAreaWidget.h"

namespace ads
{
static std::unique_ptr<CDockComponentsFactory> DefaultFactory(new CDockComponentsFactory());


//============================================================================
CDockWidgetTab* CDockComponentsFactory::createDockWidgetTab(CDockWidget* DockWidget) const
{
	return new CDockWidgetTab(DockWidget);
}


//============================================================================
CDockAreaTabBar* CDockComponentsFactory::createDockAreaTabBar(CDockAreaWidget* DockArea) const
{
	return new CDockAreaTabBar(DockArea);
}


//============================================================================
CDockAreaTitleBar* CDockComponentsFactory::createDockAreaTitleBar(CDockAreaWidget* DockArea) const
{
	return new CDockAreaTitleBar(DockArea);
}


//============================================================================
const CDockComponentsFactory* CDockComponentsFactory::factory()
{
	return DefaultFactory.get();
}


//============================================================================
void CDockComponentsFactory::setFactory(CDockComponentsFactory* Factory)
{
	DefaultFactory.reset(Factory);
}


//============================================================================
void CDockComponentsFactory::resetDefaultFactory()
{
	DefaultFactory.reset(new CDockComponentsFactory());
}
} // namespace ads

//---------------------------------------------------------------------------
// EOF DockComponentsFactory.cpp
