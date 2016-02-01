/*                       T E X T U R E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>

#include "bu/parallel.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./mat_db.h"


#define DEBUG_TEXTURE	0

typedef unsigned short	icon_t;
#define BIT_TEST(w, b)	(b == 0 ? (int)(w)&1 : (int)(w)&(1<<(b)))
#define Fb_Lookup(fbp, u, v)	(fbp->map + (v) * fbp->wid + (u))
#define BITS_PER_BYTE	8
#define BYTES_WIDE	(wid/BITS_PER_BYTE)
#define ITEMS_WIDE	(BYTES_WIDE/sizeof(icon_t))
#define ICON_SUFFIX	"icon"
#define FB_SUFFIX	"pix"

struct icon_texture
{
    char			*filenm;
    icon_t			*map;
    int			wid;
    int			hgt;
    struct icon_texture	*next;
}
    *icons = NULL;

struct fb_texture
{
    char			*filenm;
    RGBpixel		*map;
    int			wid;
    int			hgt;
    struct fb_texture	*next;
}
    *fbs = NULL;

static char *
suffix(char *str)
{
    char	*p = str + strlen(str) - 1;
    while (*p != '.' && p != str)
	p--;
    if (*p == '.')
	return	p+1;
    else
	return	p;
}


static RGBpixel	*
icon_Lookup(struct icon_texture	*iconp, int u, int v)
{
    static int	word_sz = sizeof(icon_t) * BITS_PER_BYTE;
    static RGBpixel	black_pixel = { 0, 0, 0 };
    static RGBpixel	white_pixel = { 255, 255, 255 };
    int	offset = (iconp->hgt-1-v)*iconp->wid/word_sz + u/word_sz;
    int	bit = (word_sz-1) - (u % word_sz);
    icon_t	word = iconp->map[offset];
    if (BIT_TEST(word, bit))
	return	(RGBpixel *) black_pixel;
    else
	return	(RGBpixel *) white_pixel;
}


static struct icon_texture *
init_Icon_Texture(char *file, Mat_Db_Entry *entry)
{
    FILE	*iconfp;
    struct icon_texture	*iconp;
    icon_t	*iconmap;
    int	wid = entry->df_rgb[0] << 3;
    int	hgt = entry->df_rgb[1] << 3;
    if ((iconfp = fopen(file, "rb")) == NULL) {
	bu_log("Can't open icon texture \"%s\" for reading.\n",
	       file);
	return	NULL;
    }
    iconmap = (icon_t *) bu_malloc((size_t)BYTES_WIDE*hgt, "iconmap");
#if DEBUG_TEXTURE
    bu_log("init_Icon_Texture(%s) wid=%d hgt=%d\n", file, wid, hgt);
    bu_log("%d bytes allocated for texture map.\n",
	   BYTES_WIDE*hgt);
#endif
    if (fread(iconmap, sizeof(icon_t), ITEMS_WIDE*hgt, iconfp)
	!= ITEMS_WIDE*hgt)
    {
	bu_log("Read of icon texture map failed.\n");
	fclose(iconfp);
	return	NULL;
    }
    BU_ALLOC(iconp, struct icon_texture);
    iconp->filenm = (char *)bu_malloc(strlen(file)+1, "iconp->filenm");

    bu_strlcpy(iconp->filenm, file, strlen(file)+1);
    iconp->map = iconmap;
    iconp->wid = wid;
    iconp->hgt = hgt;
    iconp->next = icons;
    icons = iconp;
#if DEBUG_TEXTURE
    {
	int	u, v;
	for (v = 0; v < hgt; v++)
	    for (u = 0; u < wid; u++) {
		RGBpixel	*pixel;
		pixel = icon_Lookup(iconp, u, v);
		prnt_Pixel(*pixel, u, v);
	    }
    }
#endif
    (void) fclose(iconfp);
    return	iconp;
}


static struct fb_texture *
init_Fb_Texture(char *file, Mat_Db_Entry *entry)
{
    fb		*txfbiop;
    struct fb_texture	*fbp;
    RGBpixel	*fbmap;
    int		wid = entry->df_rgb[0] << 3;
    int		hgt = entry->df_rgb[1] << 3;
    if ((txfbiop = fb_open(file, wid, hgt)) == FB_NULL)
	return	NULL;
    fbmap =	(RGBpixel *) bu_malloc(wid*hgt*sizeof(RGBpixel), "fbmap");
#if DEBUG_TEXTURE
    bu_log("init_Fb_Texture(%s) wid=%d hgt=%d\n", file, wid, hgt);
    bu_log("%d bytes allocated for texture map.\n",
	   wid*hgt*sizeof(RGBpixel));
#endif
    if (fb_read(txfbiop, 0, 0, (unsigned char *)fbmap, wid*hgt) == -1) {
	bu_log("Read of frame buffer texture failed.\n");
	return	NULL;
    }
    BU_ALLOC(fbp, struct fb_texture);
    fbp->filenm = (char *)bu_malloc(strlen(file)+1, "fbp->filenm");
    bu_strlcpy(fbp->filenm, file, strlen(file)+1);
    fbp->map = fbmap;
    fbp->wid = wid;
    fbp->hgt = hgt;
    fbp->next = fbs;
    fbs = fbp;
#if DEBUG_TEXTURE
    {
	int	u, v;
	for (v = 0; v < hgt; v++)
	    for (u = 0; u < wid; u++) {
		RGBpixel	*pixel;
		pixel = Fb_Lookup(fbp, u, v);
		prnt_Pixel(pixel, u, v);
	    }
    }
#endif
    (void) fb_close(txfbiop);
    return	fbp;
}


int
tex_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry)
{
    if (BU_STR_EQUAL(ICON_SUFFIX, suffix(entry->name)))
	return	icon_Entry(uvp, entry);
    else
	if (BU_STR_EQUAL(FB_SUFFIX, suffix(entry->name)))
	    return	fb_Entry(uvp, entry);
	else
	    return	0;
}


int
icon_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry)
{
    int	ui;
    int	vi;
    RGBpixel		*pixel;
    struct icon_texture	*iconp;
    char				*file = entry->name + TEX_KEYLEN;
    bu_semaphore_acquire(RT_SEM_RESULTS);
    for (iconp = icons;
	 iconp != NULL && !BU_STR_EQUAL(iconp->filenm, file);
	 iconp = iconp->next)
	;
    if (iconp == NULL)
	iconp = init_Icon_Texture(file, entry);
    bu_semaphore_release(RT_SEM_RESULTS);
    if (iconp == NULL)
	return	0;
    if (uvp->uv_u > 1.0 || uvp->uv_u < 0.0
	||	uvp->uv_v > 1.0 || uvp->uv_v < 0.0
	)
    {
	bu_log("uv coordinates out of range: u=%g v=%g\n",
	       uvp->uv_u, uvp->uv_v);
	return	0;
    }
    ui = uvp->uv_u * iconp->wid;
    vi = uvp->uv_v * iconp->hgt;
#if DEBUG_TEXTURE
    bu_log("uvp->uv_u=%g ui=%d uvp->uv_v=%g vi=%d\n",
	   uvp->uv_u, ui, uvp->uv_v, vi);
#endif
    pixel = icon_Lookup(iconp, ui, vi);
    COPYRGB(entry->df_rgb, *pixel);
    return	1;
}


int
fb_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry)
{
    int				ui;
    int				vi;
    RGBpixel		*pixel;
    struct fb_texture	*fbp;
    char				*file = entry->name + TEX_KEYLEN;
    bu_semaphore_acquire(RT_SEM_RESULTS);
    for (fbp = fbs;
	 fbp != NULL && !BU_STR_EQUAL(fbp->filenm, file);
	 fbp = fbp->next)
	;
    if (fbp == NULL)
	fbp = init_Fb_Texture(file, entry);
    bu_semaphore_release(RT_SEM_RESULTS);
    if (fbp == NULL)
	return	0;
    if (uvp->uv_u > 1.0 || uvp->uv_u < 0.0
	||	uvp->uv_v > 1.0 || uvp->uv_v < 0.0
	)
    {
	bu_log("uv coordinates out of range: u=%g v=%g\n",
	       uvp->uv_u, uvp->uv_v);
	return	0;
    }
    ui = uvp->uv_u * fbp->wid;
    vi = uvp->uv_v * fbp->hgt;
    pixel = Fb_Lookup(fbp, ui, vi);
    COPYRGB(entry->df_rgb, *pixel);
#if DEBUG_TEXTURE
    bu_log("uvp->uv_u=%g uvp->uv_v=%g\n",
	   uvp->uv_u, uvp->uv_v);
    bu_log("fbp->map[%d]=<%d, %d, %d>\n",
	   vi*fbp->wid + ui,
	   (*(fbp->map+vi*fbp->wid+ui))[0],
	   (*(fbp->map+vi*fbp->wid+ui))[1],
	   (*(fbp->map+vi*fbp->wid+ui))[2]);
    prnt_Pixel(pixel, ui, vi);
#endif
    return	1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
