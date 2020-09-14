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

#ifndef IconProviderH
#define IconProviderH
//============================================================================
/// \file   IconProvider.h
/// \author Uwe Kindler
/// \date   18.10.2019
/// \brief  Declaration of CIconProvider
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include <QIcon>

#include "ads_globals.h"

namespace ads
{

struct IconProviderPrivate;

/**
 * This object provides all icons that are required by the advanced docking
 * system.
 * The IconProvider enables the user to register custom icons in case using
 * stylesheets is not an option.
 */
class ADS_EXPORT CIconProvider
{
private:
	IconProviderPrivate* d; ///< private data (pimpl)
	friend struct IconProviderPrivate;

public:
	/**
	 * Default Constructor
	 */
	CIconProvider();

	/**
	 * Virtual Destructor
	 */
	virtual ~CIconProvider();

	/**
	 * The function returns a custom icon if one is registered and a null Icon
	 * if no custom icon is registered
	 */
	QIcon customIcon(eIcon IconId) const;

	/**
	 * Registers a custom icon for the given IconId
	 */
	void registerCustomIcon(eIcon IconId, const QIcon &icon);
}; // class IconProvider

} // namespace ads


//---------------------------------------------------------------------------
#endif // IconProviderH
