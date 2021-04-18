/*                        E V E N T S . H
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file events.h
 *
 * View objects may define their behaviors in response to events (key
 * presses, mouse movements, etc.).  Because such events are defined
 * on a per-environment basis, we need to define a common language to
 * express responses to those events in order to avoid dependence on
 * that environment.
 *
 * The calling code is responsible for translating the environment
 * specific events into bview events.
 *
 * TODO - eventually these should probably be augmented by common purpose
 * specific options (i.e. define a BVIEW_X_CONSTRAIN so the calling application
 * can map either x or X to the enabling of that particular motion constraint
 * during editing, for example...)  However, I think we'll also need to pass
 * the lower level info as there are too many purpose specific possibilities
 * to encode them all up front.
 */

#define BVIEW_KEY_PRESS            0x001
#define BVIEW_KEY_RELEASE          0x002
#define BVIEW_LEFT_MOUSE_PRESS     0x004
#define BVIEW_LEFT_MOUSE_RELEASE   0x008
#define BVIEW_RIGHT_MOUSE_PRESS    0x010
#define BVIEW_RIGHT_MOUSE_RELEASE  0x020
#define BVIEW_MIDDLE_MOUSE_PRESS   0x040
#define BVIEW_MIDDLE_MOUSE_RELEASE 0x080
#define BVIEW_CTRL_MOD             0x100
#define BVIEW_SHIFT_MOD            0x200
#define BVIEW_ALT_MOD              0x400

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
