/*                           M E N U . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/menu.h
 */

#ifndef MGED_MENU_H
#define MGED_MENU_H

#include "common.h"

__BEGIN_DECLS

struct rt_solid_edit;

/* Menu structures and defines
 *
 * Each active menu is installed by having a non-null entry in
 * menu_array[] which is a pointer
 * to an array of menu items.  The first ([0]) menu item is the title
 * for the menu, and the remaining items are individual menu entries.
 */
struct menu_item {
    char *menu_string;
    void (*menu_func)(struct rt_solid_edit *, int, int, int, void *);
    int menu_arg;
};

#define NMENU 3
#define MENU_L1 0 /* top-level solid-edit menu */
#define MENU_L2 1 /* second-level menu */
#define MENU_GEN 2 /* general features (mouse buttons) */

#define MENUXLIM        (-1250)         /* Value to set X lim to for menu */
#define MENUX           (-2048+115)     /* pixel position for menu, X */
#define MENUY           1780            /* pixel position for menu, Y */
#define SCROLLY         (2047)          /* starting Y pos for scroll area */
#define MENU_DY         (-104)          /* Distance between menu items */
#define SCROLL_DY       (-100)          /* Distance between scrollers */

#define TITLE_XBASE     (-2048)         /* pixel X of title line start pos */
#define TITLE_YBASE     (-1920)         /* pixel pos of last title line */
#define SOLID_XBASE     MENUXLIM        /* X to start display text */
#define SOLID_YBASE     (1920)          /* pixel pos of first solid line */
#define TEXT0_DY        (-60)           /* #pixels per line, Size 0 */
#define TEXT1_DY        (-90)           /* #pixels per line, Size 1 */

extern struct menu_item sed_menu[];
extern struct menu_item oed_menu[];

void btn_head_menu(struct rt_solid_edit *s, int i, int menu, int item, void *data);
void chg_l2menu(struct mged_state *s, int i);

extern void mmenu_init(struct mged_state *s);
extern void mmenu_display(struct mged_state *s, int y_top);
extern void mmenu_set(struct mged_state *s, int idx, struct menu_item *value);
extern void mmenu_set_all(struct mged_state *s, int idx, struct menu_item *value);
extern void sedit_menu(struct mged_state *s);
extern int mmenu_select(struct mged_state *s, int pen_y, int do_func);

__END_DECLS

#endif  /* MGED_MGED_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
