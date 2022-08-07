/*                   S I G N A L F L A G S . H
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file SignalFlags.h
 *
 * Qt uses a signals/slots mechanism to communicate when (for example)
 * application UI elements need to update in response to data changes.
 * However, it is very easy to set up a nest of complicated connections that
 * can trigger feedback loops or just make the logic very difficult to debug.
 *
 * To keep things manageable, we define a couple of conventions for Qt widgets
 * to use when defining "public facing" signals and slots that have
 * implications beyond their own internal display logic:
 *
 * 1.  An application will provide a view_update signal, which the application
 * itself will connect to the slots of whatever view widgets need to execute
 * the updates.  Subcomponents shouldn't directly connect to other subcomponent
 * view update signals - they should instead rely on the signal from the
 * application.
 *
 * 2.  The application will provide a do_view_changed slot, whose
 * responsibility is to issue a view_update signal in response to signals of
 * view changes from widgets.  The application should connect the view_changed
 * signal to from any widgets that potentially have an impact on the view to
 * this slot.
 *
 * 3.  The widgets will provide a view_changed signal, which is emitted by the
 * widget when something happens that requires view updates to be triggered in
 * other widgets.  This signal is connected by the app to the app's
 * do_view_changed slot
 *
 * 4.  The widgets will provide a do_view_update slot, which is connected by
 * the application to the app's view_update signal.  The do_view_update slot
 * does the work of updating the widget in response to the view_update signal.
 * It should NEVER emit a view_changed signal either directly or indirectly
 * (i.e. by calling other methods) to avoid setting up infinite loops.
 *
 * This results in some widget logic feeling a bit indirect - the "do the data
 * setup" part of a widget must be separated from the "update my component
 * graphical widgets to reflect the data" logic, and will only function
 * "properly" when hooked up to a parent application that accepts its
 * view_changed signal and turns around to call the widget's do_view_update
 * slot.  This is simply a reflection of the reality that many (most?) BRL-CAD
 * Qt widgets must also be responsive not simply to their own graphical
 * updates, but to GED command line induced data changes and/or model state
 * changes induced by other widgets.  Without this separation and connections,
 * widgets will get "out of sync" with other parts of the interface.
 *
 * Because not all view updates have similar consequences (a removal of a
 * database object from the drawn scene, for example, requires treeview updates
 * to reflect drawn status changes.  However, a rotation around the model
 * changes only the view matrix, and requires updating only of the central
 * display widget.)  To reflect this, without requiring lots of separate signal
 * connections, a flag is passed between the various view signals and slots.
 * Widgets can set various values of the flags, and by checking those flags
 * will know if a particular view update signal requires them to update their
 * aspect of the user interface.  (Some updates may be a bit resource intensive
 * if they require full processing of a large geometry file's data, so we want
 * to be able to exercise some discretion on what work we do when.
 *
 * We make these defines in this header file so all widgets and applications
 * can share a common, mutually understood convention.
 */

#include "common.h"

#ifndef SIGNAL_FLAGS_H

#define QTCAD_VIEW_REFRESH  0x00000001  // Potential camera updates, no structural changes
#define QTCAD_VIEW_DRAWN    0x00000002  // Used when what is drawn in the scene changes
#define QTCAD_VIEW_SELECT   0x00000004  // Used when what is selected changes
#define QTCAD_VIEW_MODE     0x00000008  // Used when mode-aware highlighting or drawing changes
#define QTCAD_VIEW_DB       0x00000010  // Used when .g database content changes

#endif // SIGNAL_FLAGS_H
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
