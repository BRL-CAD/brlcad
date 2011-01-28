/*                    B T G . H
 * BRL-CAD
 *
 * Copyright (c) 2010-2010 United States Government as represented by
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
/** @file btg.h
 *
 * the bot/tie glue.
 *
 */

void bottie_push_double(void *vtie, TIE_3 **tri, unsigned int ntri, void *usr, unsigned int pstride);
int bottie_prep_double(struct soltab *stp, struct rt_bot_internal *bot, struct rt_i *rtip);
int bottie_shot_double(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);
void bottie_free_double(void *vtie);

void bottie_push_float(void *vtie, float **tri, unsigned int ntri, void *usr, unsigned int pstride);
int bottie_prep_float(struct soltab *stp, struct rt_bot_internal *bot, struct rt_i *rtip);
int bottie_shot_float(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead);
void bottie_free_float(void *vtie);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
