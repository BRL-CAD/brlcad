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
/// \file   IconProvider.cpp
/// \author Uwe Kindler
/// \date   18.10.2019
/// \brief  Implementation of CIconProvider
//============================================================================

//============================================================================
//                                   INCLUDES
//============================================================================
#include "IconProvider.h"
#include <QVector>

namespace ads
{
/**
 * Private data class (pimpl)
 */
struct IconProviderPrivate
{
	CIconProvider *_this;
	QVector<QIcon> UserIcons{IconCount, QIcon()};

	/**
	 * Private data constructor
	 */
	IconProviderPrivate(CIconProvider *_public);
};
// struct LedArrayPanelPrivate

//============================================================================
IconProviderPrivate::IconProviderPrivate(CIconProvider *_public) :
	_this(_public)
{

}

//============================================================================
CIconProvider::CIconProvider() :
	d(new IconProviderPrivate(this))
{

}

//============================================================================
CIconProvider::~CIconProvider()
{
	delete d;
}


//============================================================================
QIcon CIconProvider::customIcon(eIcon IconId) const
{
	Q_ASSERT(IconId < d->UserIcons.size());
	return d->UserIcons[IconId];
}


//============================================================================
void CIconProvider::registerCustomIcon(eIcon IconId, const QIcon &icon)
{
	Q_ASSERT(IconId < d->UserIcons.size());
	d->UserIcons[IconId] = icon;
}

} // namespace ads




//---------------------------------------------------------------------------
// EOF IconProvider.cpp
