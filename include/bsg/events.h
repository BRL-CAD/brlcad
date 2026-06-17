/*                        E V E N T S . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
/** @addtogroup libbsg
 *
 * @brief
 * BSG view event constants.
 *
 * Canonical home; bv/events.h is a backward-compatibility bridge.
 */
/** @{ */
/* @file bsg/events.h */

#ifndef BSG_EVENTS_H
#define BSG_EVENTS_H

/** View objects may define their behaviors in response to events (key
 *  presses, mouse movements, etc.).  Because such events are defined
 *  on a per-environment basis, we need to define a common language to
 *  express responses to those events in order to avoid dependence on
 *  that environment.
 *
 *  The calling code is responsible for translating the environment
 *  specific events into bv events.
 */

/* BSG-native event constants */
#define BSG_KEY_PRESS            0x001
#define BSG_KEY_RELEASE          0x002
#define BSG_LEFT_MOUSE_PRESS     0x004
#define BSG_LEFT_MOUSE_RELEASE   0x008
#define BSG_RIGHT_MOUSE_PRESS    0x010
#define BSG_RIGHT_MOUSE_RELEASE  0x020
#define BSG_MIDDLE_MOUSE_PRESS   0x040
#define BSG_MIDDLE_MOUSE_RELEASE 0x080
#define BSG_CTRL_MOD             0x100
#define BSG_SHIFT_MOD            0x200
#define BSG_ALT_MOD              0x400


#endif /* BSG_EVENTS_H */

/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
