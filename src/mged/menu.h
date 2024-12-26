/*                           M E N U . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
#include "mged.h"

/* The MENU_* values in these headers end up in es_menu,
 * as do ARB vertex numbers */
#include "primitives/edarb.h"
#include "primitives/edars.h"
#include "primitives/edbot.h"
#include "primitives/edbspline.h"
#include "primitives/edcline.h"
#include "primitives/eddsp.h"
#include "primitives/edebm.h"
#include "primitives/edehy.h"
#include "primitives/edell.h"
#include "primitives/edepa.h"
#include "primitives/edeto.h"
#include "primitives/edextrude.h"
#include "primitives/edhyp.h"
#include "primitives/edmetaball.h"
#include "primitives/ednmg.h"
#include "primitives/edpart.h"
#include "primitives/edpipe.h"
#include "primitives/edrhc.h"
#include "primitives/edrpc.h"
#include "primitives/edsuperell.h"
#include "primitives/edtgc.h"
#include "primitives/edtor.h"
#include "primitives/edvol.h"

extern struct menu_item sed_menu[];
extern struct menu_item oed_menu[];

void btn_head_menu(struct mged_state *s, int i, int menu, int item);
void chg_l2menu(struct mged_state *s, int i);

extern void mmenu_init(struct mged_state *s);
extern void mmenu_display(struct mged_state *s, int y_top);
extern void mmenu_set(struct mged_state *s, int idx, struct menu_item *value);
extern void mmenu_set_all(struct mged_state *s, int idx, struct menu_item *value);
extern void sedit_menu(struct mged_state *s);
extern int mmenu_select(struct mged_state *s, int pen_y, int do_func);

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
