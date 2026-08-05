/*                   F O N T S T A S H _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

struct test_stats {
    int creates;
    int resizes;
    int updates;
    int draws;
    int deletes;
    int bad_callback;
    int fail_create;
    int fail_resize;
    int width;
    int height;
};

struct test_renderer {
    struct test_stats *stats;
};

struct expansion_handler {
    FONScontext *stash;
    int calls;
};

static int
test_create(void *ptr, int width, int height)
{
    struct test_renderer *renderer = (struct test_renderer *)ptr;
    if (!renderer || !renderer->stats || width < 2 || height < 2)
	return 0;
    renderer->stats->creates++;
    if (renderer->stats->fail_create)
	return 0;
    renderer->stats->width = width;
    renderer->stats->height = height;
    return 1;
}

static int
test_resize(void *ptr, int width, int height)
{
    struct test_renderer *renderer = (struct test_renderer *)ptr;
    if (!renderer || !renderer->stats || width < 2 || height < 2)
	return 0;
    renderer->stats->resizes++;
    if (renderer->stats->fail_resize)
	return 0;
    renderer->stats->width = width;
    renderer->stats->height = height;
    return 1;
}

static void
test_update(void *ptr, int *rect, const unsigned char *data)
{
    struct test_renderer *renderer = (struct test_renderer *)ptr;
    if (!renderer || !renderer->stats || !rect || !data)
	return;
    if (rect[0] < 0 || rect[1] < 0 ||
	rect[2] > renderer->stats->width ||
	rect[3] > renderer->stats->height ||
	rect[0] >= rect[2] || rect[1] >= rect[3])
	renderer->stats->bad_callback = 1;
    renderer->stats->updates++;
}

static void
test_draw(void *ptr, const float *verts, const float *tcoords,
	  const unsigned int *colors, int nverts)
{
    struct test_renderer *renderer = (struct test_renderer *)ptr;
    if (!renderer || !renderer->stats || !verts || !tcoords || !colors ||
	nverts <= 0 || nverts % 3 != 0)
	return;
    renderer->stats->draws++;
}

static void
test_delete(void *ptr)
{
    struct test_renderer *renderer = (struct test_renderer *)ptr;
    if (!renderer)
	return;
    if (renderer->stats)
	renderer->stats->deletes++;
    free(renderer);
}

static FONScontext *
test_context(struct test_stats *stats, int width, int height)
{
    struct test_renderer *renderer;
    FONSparams params;

    renderer = (struct test_renderer *)calloc(1, sizeof(*renderer));
    if (!renderer)
	return NULL;
    renderer->stats = stats;
    memset(&params, 0, sizeof(params));
    params.width = width;
    params.height = height;
    params.flags = FONS_ZERO_TOPLEFT;
    params.userPtr = renderer;
    params.renderCreate = test_create;
    params.renderResize = test_resize;
    params.renderUpdate = test_update;
    params.renderDraw = test_draw;
    params.renderDelete = test_delete;
    return fonsCreateInternal(&params);
}

static void
test_atlas_error(void *ptr, int error, int value)
{
    struct expansion_handler *handler = (struct expansion_handler *)ptr;
    int width, height;
    if (!handler || !handler->stash || error != FONS_ATLAS_FULL)
	return;
    handler->calls++;
    fonsGetAtlasSize(handler->stash, &width, &height);
    do {
	width *= 2;
	height *= 2;
    } while ((width < value || height < value) && width < 2048);
    (void)fonsExpandAtlas(handler->stash, width, height);
}

#define CHECK(_expr) do { \
    if (!(_expr)) { \
	fprintf(stderr, "fontstash check failed at line %d: %s\n", \
		__LINE__, #_expr); \
	return 1; \
    } \
} while (0)

int
main(int argc, char **argv)
{
    struct test_stats stats;
    struct test_stats invalid_stats;
    struct test_stats create_fail_stats;
    struct test_stats small_stats;
    struct expansion_handler expansion;
    FONScontext *stash;
    FONScontext *small_stash;
    FONStextIter iter;
    FONSquad quad;
    const unsigned char *texture;
    float bounds[4];
    float miny = 0.0f, maxy = 0.0f;
    float ascender = 0.0f, descender = 0.0f, lineh = 0.0f;
    int font, width = 0, height = 0, glyphs = 0;
    int dirty[4];
    const char text[] = "Fontstash: caf\xc3\xa9 " "\xff";
    const char malformed[] = {'A', (char)0xc3, '(', 'B', (char)0xe2, '\0'};

    memset(&stats, 0, sizeof(stats));
    memset(&invalid_stats, 0, sizeof(invalid_stats));
    memset(&create_fail_stats, 0, sizeof(create_fail_stats));
    memset(&small_stats, 0, sizeof(small_stats));
    memset(&expansion, 0, sizeof(expansion));

    if (argc != 2) {
	fprintf(stderr, "Usage: %s font.ttf\n", argv[0]);
	return 2;
    }

    /* Invalid and renderer-create failures must still release userPtr once. */
    CHECK(test_context(&invalid_stats, 1, 256) == NULL);
    CHECK(invalid_stats.deletes == 1);
    create_fail_stats.fail_create = 1;
    CHECK(test_context(&create_fail_stats, 256, 256) == NULL);
    CHECK(create_fail_stats.creates == 1);
    CHECK(create_fail_stats.deletes == 1);

    /* Atlas-full callbacks may expand even when one glyph exceeds the atlas. */
    small_stash = test_context(&small_stats, 32, 32);
    CHECK(small_stash != NULL);
    font = fonsAddFont(small_stash, "osifont", argv[1]);
    CHECK(font != FONS_INVALID);
    fonsSetFont(small_stash, font);
    fonsSetSize(small_stash, 96.0f);
    expansion.stash = small_stash;
    fonsSetErrorCallback(small_stash, test_atlas_error, &expansion);
    CHECK(fonsDrawText(small_stash, 0.0f, 100.0f, "W", NULL) > 0.0f);
    fonsGetAtlasSize(small_stash, &width, &height);
    CHECK(expansion.calls > 0 && width > 32 && height > 32);
    fonsDeleteInternal(small_stash);
    CHECK(small_stats.deletes == 1);

    stash = test_context(&stats, 256, 256);
    CHECK(stash != NULL);
    CHECK(stats.creates == 1 && stats.deletes == 0);

    /* Public query functions are deliberately NULL tolerant. */
    CHECK(fonsGetTextureData(NULL, NULL, NULL) == NULL);
    CHECK(fonsValidateTexture(NULL, dirty) == 0);
    CHECK(fonsValidateTexture(stash, NULL) == 0);
    fonsGetAtlasSize(NULL, &width, &height);
    fonsGetAtlasSize(stash, NULL, NULL);

    font = fonsAddFont(stash, "osifont", argv[1]);
    CHECK(font != FONS_INVALID);
    CHECK(fonsGetFontByName(stash, "osifont") == font);
    fonsSetFont(stash, font);
    fonsSetSize(stash, 18.0f);
    fonsSetSpacing(stash, 0.5f);

    CHECK(fonsTextBounds(stash, 10.0f, 20.0f, text, NULL, bounds) > 0.0f);
    CHECK(bounds[2] > bounds[0]);
    fonsVertMetrics(stash, &ascender, &descender, &lineh);
    CHECK(ascender > 0.0f && descender < 0.0f && lineh > 0.0f);
    fonsLineBounds(stash, 20.0f, &miny, &maxy);
    CHECK(maxy > miny);
    fonsLineBounds(stash, 20.0f, NULL, NULL);

    CHECK(fonsTextIterInit(stash, &iter, 10.0f, 20.0f,
			   text, NULL) == 1);
    while (fonsTextIterNext(stash, &iter, &quad)) {
	glyphs++;
	CHECK(glyphs < 128);
    }
    CHECK(glyphs > 0);
    glyphs = 0;
    CHECK(fonsTextIterInit(stash, &iter, 0.0f, 20.0f,
			   malformed, NULL) == 1);
    while (fonsTextIterNext(stash, &iter, &quad))
	glyphs++;
    CHECK(glyphs == 5);
    CHECK(fonsDrawText(stash, 10.0f, 20.0f, text, NULL) > 10.0f);
    CHECK(stats.updates > 0 && stats.draws > 0 && !stats.bad_callback);

    texture = fonsGetTextureData(stash, &width, &height);
    CHECK(texture != NULL && width == 256 && height == 256);
    CHECK(texture[0] == 0xff);

    CHECK(fonsExpandAtlas(stash, 512, 384) == 1);
    fonsGetAtlasSize(stash, &width, &height);
    CHECK(width == 512 && height == 384 && stats.resizes == 1);
    texture = fonsGetTextureData(stash, NULL, NULL);
    CHECK(texture != NULL && texture[0] == 0xff);
    CHECK(fonsValidateTexture(stash, dirty) == 1);
    CHECK(dirty[0] == 0 && dirty[1] == 0 &&
	  dirty[2] == 512 && dirty[3] == 384);

    /* A rejected renderer resize leaves all live Fontstash state unchanged. */
    stats.fail_resize = 1;
    CHECK(fonsExpandAtlas(stash, 768, 384) == 0);
    fonsGetAtlasSize(stash, &width, &height);
    CHECK(width == 512 && height == 384);
    stats.fail_resize = 0;

    CHECK(fonsResetAtlas(stash, 128, 128) == 1);
    fonsGetAtlasSize(stash, &width, &height);
    CHECK(width == 128 && height == 128);
    CHECK(fonsDrawText(stash, 0.0f, 20.0f, "reset", NULL) > 0.0f);
    CHECK(!stats.bad_callback);

    fonsDeleteInternal(stash);
    CHECK(stats.deletes == 1);
    return 0;
}
