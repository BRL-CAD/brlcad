/*                        G L _ T E X T . H
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

#ifndef DM_GL_TEXT_H
#define DM_GL_TEXT_H

/* Include after the backend's GL and Fontstash headers so these helpers
 * use the same GL entry points as the backend, including OSMesa's. */

static inline bool
dm_gl_text_position(struct dm *dmp, fastf_t x, fastf_t y, int use_aspect, GLfloat origin[2])
{
    if (use_aspect)
	y *= dm_get_aspect(dmp);
    if (x > 1 || y > 1)
	return false;

    /* Shift offscreen origins into the clip volume so partially visible
     * text can still be drawn. Fontstash takes top-left pixel coordinates. */
    const bool left = x < -1;
    const bool below = y < -1;
    GLboolean valid;
    GLfloat raster[4];
    glPushAttrib(GL_CURRENT_BIT);
    glRasterPos2d(left ? x + 2 : x, below ? y + 2 : y);
    glGetBooleanv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
    if (valid)
	glGetFloatv(GL_CURRENT_RASTER_POSITION, raster);
    glPopAttrib();
    if (!valid)
	return false;

    origin[0] = raster[0] - (left ? dm_get_width(dmp) : 0);
    origin[1] = (below ? 2 : 1) * dm_get_height(dmp) - raster[1];
    return true;
}


static inline int
dm_gl_draw_text(struct dm *dmp, FONScontext *fs, int font, int font_size,
	const char *text, fastf_t x, fastf_t y, int use_aspect)
{
    GLfloat origin[2];
    if (font == FONS_INVALID || !dm_gl_text_position(dmp, x, y, use_aspect, origin))
	return BRLCAD_OK;

    /* The HUD already uses a projection stack entry. Save the matrices
     * directly so text drawing works even at the minimum stack depth. */
    GLdouble projection[16], modelview[16];
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glPushAttrib(GL_TRANSFORM_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, dm_get_width(dmp), dm_get_height(dmp), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const unsigned char *fg = dm_get_fg(dmp);
    fonsSetFont(fs, font);
    fonsSetSize(fs, font_size);
    fonsSetColor(fs, glfonsRGBA(fg[0], fg[1], fg[2], 255));
    fonsDrawText(fs, origin[0], origin[1], text, NULL);

    glLoadMatrixd(modelview);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(projection);
    glPopAttrib();
    return BRLCAD_OK;
}


static inline int
dm_gl_text_bounds(struct dm *dmp, FONScontext *fs, int font, int font_size,
	vect2d_t *bmin, vect2d_t *bmax, const char *text, fastf_t x, fastf_t y, int use_aspect)
{
    GLfloat origin[2];
    if (font == FONS_INVALID || !dm_gl_text_position(dmp, x, y, use_aspect, origin))
	return BRLCAD_ERROR;

    GLfloat bounds[4] = {0};
    fonsSetFont(fs, font);
    fonsSetSize(fs, font_size);
    fonsTextBounds(fs, origin[0], origin[1], text, NULL, bounds);
    if (bmin)
	V2SET(*bmin, bounds[0], bounds[1]);
    if (bmax)
	V2SET(*bmax, bounds[2], bounds[3]);
    return BRLCAD_OK;
}

#endif /* DM_GL_TEXT_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
