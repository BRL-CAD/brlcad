/*                       U G _ M I S C . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ug_misc.h
 *
 */

#include <setjmp.h>
extern jmp_buf my_env;

/* This macro and function support a compact error-checking interface to the
 * UG library routines
 */
#define UF_func(X) report(#X, __FILE__, __LINE__, (X))
extern int report(char *call, char *file, int line, int code);
extern void Add_lists(uf_list_p_t dest, uf_list_p_t src);
extern const char *feature_sign(tag_t feat);
extern tag_t Lee_open_part(char *p_name, const int level);
extern tag_t ask_cset_of_unloaded_children(tag_t part, tag_t part_occ);
extern void unload_sub_parts(tag_t loaded_child_cset);
extern tag_t load_sub_parts(tag_t displayed_part);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
