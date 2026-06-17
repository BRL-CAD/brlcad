/*                     Q G N A M E S P A C E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file QgNamespace.h
 *
 * Namespaced aliases for libqtcad public API types.
 */

#ifndef QGNAMESPACE_H
#define QGNAMESPACE_H

#include "qtcad/QgAccordion.h"
#include "qtcad/QgAppExecDialog.h"
#include "qtcad/QgAttributesModel.h"
#include "qtcad/QgCanvasBase.h"
#include "qtcad/QgColorRGB.h"
#include "qtcad/QgConsole.h"
#include "qtcad/QgConsoleListener.h"
#include "qtcad/QgDockWidget.h"
#include "qtcad/QgFlowLayout.h"
#include "qtcad/QgGedEventBatch.h"
#include "qtcad/QgGL.h"
#include "qtcad/QgGeomImport.h"
#include "qtcad/QgKeyVal.h"
#include "qtcad/QgMeasureFilter.h"
#include "qtcad/QgModel.h"
#include "qtcad/QgPaletteController.h"
#include "qtcad/QgPluginApi.h"
#include "qtcad/QgPluginCommands.h"
#include "qtcad/QgPluginManager.h"
#include "qtcad/QgPolyFilter.h"
#include "qtcad/QgQuadView.h"
#include "qtcad/QgRoles.h"
#include "qtcad/QgSW.h"
#include "qtcad/QgSelectFilter.h"
#include "qtcad/QgSession.h"
#include "qtcad/QgSignalFlags.h"
#include "qtcad/QgSketchFilter.h"
#include "qtcad/QgToolBase.h"
#include "qtcad/QgToolPalette.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/QgTypes.h"
#include "qtcad/QgUtil.h"
#include "qtcad/QgView.h"
#include "qtcad/QgViewCtrl.h"
#include "qtcad/QgViewFilter.h"

namespace qtcad {
using ::GEDShellCompleter;
using ::IQgCommand;
using ::IQgDialogFactory;
using ::IQgPanelFactory;
using ::IQgToolFactory;
using ::IQgViewEventFilter;
using ::QMeasure2DFilter;
using ::QMeasure3DFilter;
using ::QgAccordion;
using ::QgAccordionObject;
using ::QgAppExecDialog;
using ::QgAscImportDialog;
using ::QgAttributesModel;
using ::QgCanvasBase;
using ::QgColorRGB;
using ::QgCombType;
using ::QgCombTypeId;
using ::QgConsole;
using ::QgConsoleListener;
using ::QgConsoleWidgetCompleter;
using ::QgDataRoles;
using ::QgDockWidget;
using ::QgFlowLayout;
using ::QgGedEventBatch;
using ::QgGL;
using ::QgGeomImport;
using ::QgIcon;
using ::QgItem;
using ::QgKeyValDelegate;
using ::QgKeyValModel;
using ::QgKeyValNode;
using ::QgKeyValView;
using ::QgMeasureFilter;
using ::QgModel;
using ::QgPaletteController;
using ::QgPluginCommands;
using ::QgPluginContext;
using ::QgPluginDescriptor;
using ::QgPluginManager;
using ::QgPluginNotifier;
using ::QgPolyCreateFilter;
using ::QgPolyFilter;
using ::QgPolyMoveFilter;
using ::QgPolyPointFilter;
using ::QgPolySelectFilter;
using ::QgPolyUpdateFilter;
using ::QgQuadView;
using ::QgQuadrantId;
using ::QgRhinoImportDialog;
using ::QgSW;
using ::QgSelectBoxFilter;
using ::QgSelectFilter;
using ::QgSelectPntFilter;
using ::QgSelectRayFilter;
using ::QgSession;
using ::QgSketchAddVertexFilter;
using ::QgSketchArcRadiusFilter;
using ::QgSketchCursorTracker;
using ::QgSketchFilter;
using ::QgSketchMoveSegmentFilter;
using ::QgSketchMoveVertexFilter;
using ::QgSketchPickSegmentFilter;
using ::QgSketchPickVertexFilter;
using ::QgSketchSetTangencyFilter;
using ::QgStepImportDialog;
using ::QgToolBase;
using ::QgToolPalette;
using ::QgToolPaletteButton;
using ::QgToolPaletteElement;
using ::QgTreeInteractionMode;
using ::QgTreeSelectionModel;
using ::QgTreeView;
using ::QgView;
using ::QgViewCtrl;
using ::QgViewFilter;
using ::QgViewType;
using ::QgViewUpdateFlag;
using ::QgViewUpdateFlags;
using ::gObjDelegate;
}

#endif /* QGNAMESPACE_H */
