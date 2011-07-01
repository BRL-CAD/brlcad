/*                          M E N U . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file mged/menu.h
 *
 * Each active menu is installed by haveing a non-null entry in
 * menu_array[] which is a pointer
 * to an array of menu items.  The first ([0]) menu item is the title
 * for the menu, and the remaining items are individual menu entries.
 *
 */
#ifndef SEEN_MENU_H
#define SEEN_MENU_H

struct menu_item {
    char *menu_string;
    void (*menu_func)();
    int menu_arg;
};


#define MENU_NULL ((struct menu_item *)0)

#define NMENU 3

#define MENU_L1 0 /* top-level solid-edit menu */
#define MENU_L2 1 /* second-level menu (unused) */
#define MENU_GEN 2 /* general features (mouse buttons) */

#endif /* SEEN_MENU_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
