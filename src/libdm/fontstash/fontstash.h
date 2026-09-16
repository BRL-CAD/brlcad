//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// Substantially revised and hardened for BRL-CAD, 2026.

#ifndef FONS_H
#define FONS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// To make the implementation private to the file that generates the implementation
#ifdef FONS_STATIC
#define FONS_DEF static
#else
#define FONS_DEF extern
#endif

#define FONS_INVALID -1

enum FONSflags {
	FONS_ZERO_TOPLEFT = 1,
	FONS_ZERO_BOTTOMLEFT = 2
};

enum FONSalign {
	// Horizontal align
	FONS_ALIGN_LEFT 	= 1<<0,	// Default
	FONS_ALIGN_CENTER 	= 1<<1,
	FONS_ALIGN_RIGHT 	= 1<<2,
	// Vertical align
	FONS_ALIGN_TOP 		= 1<<3,
	FONS_ALIGN_MIDDLE	= 1<<4,
	FONS_ALIGN_BOTTOM	= 1<<5,
	FONS_ALIGN_BASELINE	= 1<<6  // Default
};

enum FONSerrorCode {
	// Font atlas is full.
	FONS_ATLAS_FULL = 1,
	// Calls to fonsPushState has created too large stack, if you need deep state stack bump up FONS_MAX_STATES.
	FONS_STATES_OVERFLOW = 2,
	// Trying to pop too many states fonsPopState().
	FONS_STATES_UNDERFLOW = 3
};

struct FONSparams {
	int width, height;
	unsigned char flags;
	void* userPtr;
	int (*renderCreate)(void* uptr, int width, int height);
	int (*renderResize)(void* uptr, int width, int height);
	void (*renderUpdate)(void* uptr, int* rect, const unsigned char* data);
	void (*renderDraw)(void* uptr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
	void (*renderDelete)(void* uptr);
};
typedef struct FONSparams FONSparams;

struct FONSquad
{
	float x0,y0,s0,t0;
	float x1,y1,s1,t1;
};
typedef struct FONSquad FONSquad;

struct FONStextIter {
	float x, y, nextx, nexty, scale, spacing;
	unsigned int codepoint;
	int isize, iblur;
	struct FONSfont* font;
	struct FONSfont* prevFont;
	int prevGlyphIndex;
	const char* str;
	const char* next;
	const char* end;
	unsigned int utf8state;
};
typedef struct FONStextIter FONStextIter;

typedef struct FONScontext FONScontext;

// Contructor and destructor.
FONS_DEF FONScontext* fonsCreateInternal(const FONSparams* params);
FONS_DEF void fonsDeleteInternal(FONScontext* s);

FONS_DEF void fonsSetErrorCallback(FONScontext* s, void (*callback)(void* uptr, int error, int val), void* uptr);
// Returns current atlas size.
FONS_DEF void fonsGetAtlasSize(FONScontext* s, int* width, int* height);
// Expands the atlas size.
FONS_DEF int fonsExpandAtlas(FONScontext* s, int width, int height);
// Resets the whole stash.
FONS_DEF int fonsResetAtlas(FONScontext* stash, int width, int height);

// Add fonts
FONS_DEF int fonsAddFont(FONScontext* s, const char* name, const char* path);
FONS_DEF int fonsAddFontMem(FONScontext* s, const char* name, unsigned char* data, int ndata, int freeData);
FONS_DEF int fonsGetFontByName(FONScontext* s, const char* name);
FONS_DEF int fonsAddFallbackFont(FONScontext* stash, int base, int fallback);

// State handling
FONS_DEF void fonsPushState(FONScontext* s);
FONS_DEF void fonsPopState(FONScontext* s);
FONS_DEF void fonsClearState(FONScontext* s);

// State setting
FONS_DEF void fonsSetSize(FONScontext* s, float size);
FONS_DEF void fonsSetColor(FONScontext* s, unsigned int color);
FONS_DEF void fonsSetSpacing(FONScontext* s, float spacing);
FONS_DEF void fonsSetBlur(FONScontext* s, float blur);
FONS_DEF void fonsSetAlign(FONScontext* s, int align);
FONS_DEF void fonsSetFont(FONScontext* s, int font);

// Draw text
FONS_DEF float fonsDrawText(FONScontext* s, float x, float y, const char* string, const char* end);

// Measure text
FONS_DEF float fonsTextBounds(FONScontext* s, float x, float y, const char* string, const char* end, float* bounds);
FONS_DEF void fonsLineBounds(FONScontext* s, float y, float* miny, float* maxy);
FONS_DEF void fonsVertMetrics(FONScontext* s, float* ascender, float* descender, float* lineh);

// Text iterator
FONS_DEF int fonsTextIterInit(FONScontext* stash, FONStextIter* iter, float x, float y, const char* str, const char* end);
FONS_DEF int fonsTextIterNext(FONScontext* stash, FONStextIter* iter, struct FONSquad* quad);

// Pull texture changes
FONS_DEF const unsigned char* fonsGetTextureData(FONScontext* stash, int* width, int* height);
FONS_DEF int fonsValidateTexture(FONScontext* s, int* dirty);

// Draws the stash texture for debugging
FONS_DEF void fonsDrawDebug(FONScontext* s, float x, float y);

#ifdef __cplusplus
}
#endif

#endif // FONS_H


#ifdef FONTSTASH_IMPLEMENTATION

#include "bio.h" /* for Windows UTF-8 file handling */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"

struct FONSttFontImpl {
	stt_fontinfo font;
};
typedef struct FONSttFontImpl FONSttFontImpl;

static int fons__tt_loadFont(FONScontext *context, FONSttFontImpl *font, unsigned char *data, int dataSize)
{
	if (!font || !data || dataSize <= 0 ||
	    !stt_InitFont(&font->font, data, dataSize, 0))
		return 0;
	font->font.userdata = context;
	return 1;
}

static void fons__tt_getFontVMetrics(FONSttFontImpl *font, int *ascent, int *descent, int *lineGap)
{
	stt_GetFontVMetrics(&font->font, ascent, descent, lineGap);
}

static float fons__tt_getPixelHeightScale(FONSttFontImpl *font, float size)
{
	return stt_ScaleForPixelHeight(&font->font, size);
}

static int fons__tt_getGlyphIndex(FONSttFontImpl *font, int codepoint)
{
	return stt_FindGlyphIndex(&font->font, codepoint);
}

static int fons__tt_buildGlyphBitmap(FONSttFontImpl *font, int glyph, float scale,
							  int *advance, int *lsb, int *x0, int *y0, int *x1, int *y1)
{
	stt_GetGlyphHMetrics(&font->font, glyph, advance, lsb);
	stt_GetGlyphBitmapBox(&font->font, glyph, scale, scale, x0, y0, x1, y1);
	return 1;
}

static void fons__tt_renderGlyphBitmap(FONSttFontImpl *font, unsigned char *output, int outWidth, int outHeight, int outStride,
								float scaleX, float scaleY, int glyph)
{
	stt_MakeGlyphBitmap(&font->font, output, outWidth, outHeight, outStride, scaleX, scaleY, glyph);
}

static int fons__tt_getGlyphKernAdvance(FONSttFontImpl *font, int glyph1, int glyph2)
{
	return stt_GetGlyphKernAdvance(&font->font, glyph1, glyph2);
}

#ifndef FONS_HASH_LUT_SIZE
#	define FONS_HASH_LUT_SIZE 256
#endif
#ifndef FONS_INIT_FONTS
#	define FONS_INIT_FONTS 4
#endif
#ifndef FONS_INIT_GLYPHS
#	define FONS_INIT_GLYPHS 256
#endif
#ifndef FONS_INIT_ATLAS_NODES
#	define FONS_INIT_ATLAS_NODES 256
#endif
#ifndef FONS_VERTEX_COUNT
#	define FONS_VERTEX_COUNT 1024
#endif
#ifndef FONS_MAX_STATES
#	define FONS_MAX_STATES 20
#endif
#ifndef FONS_MAX_FALLBACKS
#	define FONS_MAX_FALLBACKS 20
#endif
#ifndef FONS_MAX_ATLAS_PIXELS
#	define FONS_MAX_ATLAS_PIXELS 67108864
#endif

static unsigned int fons__hashint(unsigned int a)
{
	a += ~(a<<15);
	a ^=  (a>>10);
	a +=  (a<<3);
	a ^=  (a>>6);
	a += ~(a<<11);
	a ^=  (a>>16);
	return a;
}

static int fons__mini(int a, int b)
{
	return a < b ? a : b;
}

static int fons__maxi(int a, int b)
{
	return a > b ? a : b;
}

static int fons__validAtlasSize(int width, int height)
{
	return width >= 2 && height >= 2 && width <= INT_MAX/2 &&
	       height <= INT_MAX/2 &&
	       (size_t)width <= ((size_t)-1)/(size_t)height &&
	       (size_t)width*(size_t)height <= FONS_MAX_ATLAS_PIXELS;
}

static int fons__growCapacity(int current, int needed, size_t elementSize,
			      int* capacity)
{
	int next;
	if (!capacity || needed < 0 || current < 0 || elementSize == 0)
		return 0;
	next = current > 0 ? current : 8;
	while (next < needed) {
		if (next > 0x7fffffff/2)
			return 0;
		next *= 2;
	}
	if ((size_t)next > ((size_t)-1)/elementSize)
		return 0;
	*capacity = next;
	return 1;
}

struct FONSglyph
{
	unsigned int codepoint;
	int index, next, size, blur;
	int x0,y0,x1,y1;
	float xadv,xoff,yoff,scale;
	struct FONSfont* renderFont;
};
typedef struct FONSglyph FONSglyph;

struct FONSfont
{
	FONSttFontImpl font;
	char name[64];
	unsigned char* data;
	int dataSize;
	unsigned char freeData;
	float ascender;
	float descender;
	float lineh;
	FONSglyph* glyphs;
	int cglyphs;
	int nglyphs;
	int lut[FONS_HASH_LUT_SIZE];
	int fallbacks[FONS_MAX_FALLBACKS];
	int nfallbacks;
};
typedef struct FONSfont FONSfont;

struct FONSstate
{
	int font;
	int align;
	float size;
	unsigned int color;
	float blur;
	float spacing;
};
typedef struct FONSstate FONSstate;

struct FONSatlasNode {
    int x, y, width;
};
typedef struct FONSatlasNode FONSatlasNode;

struct FONSatlas
{
	int width, height;
	FONSatlasNode* nodes;
	int nnodes;
	int cnodes;
};
typedef struct FONSatlas FONSatlas;

struct FONScontext
{
	FONSparams params;
	float itw,ith;
	unsigned char* texData;
	int dirtyRect[4];
	FONSfont** fonts;
	FONSatlas* atlas;
	int cfonts;
	int nfonts;
	float verts[FONS_VERTEX_COUNT*2];
	float tcoords[FONS_VERTEX_COUNT*2];
	unsigned int colors[FONS_VERTEX_COUNT];
	int nverts;
	FONSstate states[FONS_MAX_STATES];
	int nstates;
	void (*handleError)(void* uptr, int error, int val);
	void* errorUptr;
};

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define FONS_UTF8_ACCEPT 0
#define FONS_UTF8_REJECT 12

static unsigned int fons__decutf8(unsigned int* state, unsigned int* codep, unsigned int byte)
{
	static const unsigned char utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
    };

	unsigned int type = utf8d[byte];

    *codep = (*state != FONS_UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);

	*state = utf8d[256 + *state + type];
	return *state;
}

static int fons__nextCodepoint(const char** cursor, const char* end,
			       unsigned int* state, unsigned int* codepoint)
{
	if (!cursor || !*cursor || !end || !state || !codepoint)
		return 0;
	while (*cursor != end) {
		unsigned int byte = *(const unsigned char*)*cursor;
		unsigned int status;
		(*cursor)++;
		status = fons__decutf8(state, codepoint, byte);
		if (status == FONS_UTF8_ACCEPT)
			return 1;
		if (status == FONS_UTF8_REJECT) {
			/* Reconsider a valid starter after replacing the bad sequence. */
			if (byte < 0x80 || (byte >= 0xc2 && byte <= 0xf4))
				(*cursor)--;
			*state = FONS_UTF8_ACCEPT;
			*codepoint = 0xfffd;
			return 1;
		}
	}
	/* A truncated sequence at the end is one replacement character. */
	if (*state != FONS_UTF8_ACCEPT) {
		*state = FONS_UTF8_ACCEPT;
		*codepoint = 0xfffd;
		return 1;
	}
	return 0;
}
/********************************************************************************/

// Atlas based on Public Domain Skyline Bin Packer by Jukka Jylänki
// https://github.com/juj/RectangleBinPack

static void fons__deleteAtlas(FONSatlas* atlas)
{
	if (atlas == NULL) return;
	if (atlas->nodes != NULL) free(atlas->nodes);
	free(atlas);
}

static FONSatlas* fons__allocAtlas(int w, int h, int nnodes)
{
	FONSatlas* atlas = NULL;
	size_t nodeBytes;

	if (!fons__validAtlasSize(w, h) || nnodes <= 0 ||
	    (size_t)nnodes > ((size_t)-1)/sizeof(FONSatlasNode))
		return NULL;
	nodeBytes = sizeof(FONSatlasNode)*(size_t)nnodes;

	// Allocate memory for the font stash.
	atlas = (FONSatlas*)malloc(sizeof(FONSatlas));
	if (atlas == NULL) goto error;
	memset(atlas, 0, sizeof(FONSatlas));

	atlas->width = w;
	atlas->height = h;

	// Allocate space for skyline nodes
	atlas->nodes = (FONSatlasNode*)malloc(nodeBytes);
	if (atlas->nodes == NULL) goto error;
	memset(atlas->nodes, 0, nodeBytes);
	atlas->nnodes = 0;
	atlas->cnodes = nnodes;

	// Init root node.
	atlas->nodes[0].x = 0;
	atlas->nodes[0].y = 0;
	atlas->nodes[0].width = w;
	atlas->nnodes++;

	return atlas;

error:
	if (atlas) fons__deleteAtlas(atlas);
	return NULL;
}

static int fons__atlasInsertNode(FONSatlas* atlas, int idx, int x, int y, int w)
{
	int i;
	if (!atlas || !atlas->nodes || idx < 0 || idx > atlas->nnodes ||
	    x < 0 || y < 0 || w < 0 || x > atlas->width-w)
		return 0;
	// Insert node
	if (atlas->nnodes+1 > atlas->cnodes) {
		FONSatlasNode* nodes;
		int capacity;
		if (!fons__growCapacity(atlas->cnodes, atlas->nnodes+1,
					 sizeof(FONSatlasNode), &capacity))
			return 0;
		nodes = (FONSatlasNode*)realloc(
			atlas->nodes, sizeof(FONSatlasNode)*(size_t)capacity);
		if (nodes == NULL)
			return 0;
		atlas->nodes = nodes;
		atlas->cnodes = capacity;
	}
	for (i = atlas->nnodes; i > idx; i--)
		atlas->nodes[i] = atlas->nodes[i-1];
	atlas->nodes[idx].x = x;
	atlas->nodes[idx].y = y;
	atlas->nodes[idx].width = w;
	atlas->nnodes++;

	return 1;
}

static void fons__atlasRemoveNode(FONSatlas* atlas, int idx)
{
	int i;
	if (!atlas || idx < 0 || idx >= atlas->nnodes) return;
	for (i = idx; i < atlas->nnodes-1; i++)
		atlas->nodes[i] = atlas->nodes[i+1];
	atlas->nnodes--;
}

static int fons__atlasExpand(FONSatlas* atlas, int w, int h)
{
	int oldWidth, oldHeight;
	if (!atlas || !fons__validAtlasSize(w, h) ||
	    w < atlas->width || h < atlas->height)
		return 0;
	oldWidth = atlas->width;
	oldHeight = atlas->height;
	atlas->width = w;
	atlas->height = h;
	// Insert node for empty space
	if (w > oldWidth &&
	    !fons__atlasInsertNode(atlas, atlas->nnodes, oldWidth, 0,
				   w-oldWidth)) {
		atlas->width = oldWidth;
		atlas->height = oldHeight;
		return 0;
	}
	return 1;
}

static int fons__atlasAddSkylineLevel(FONSatlas* atlas, int idx, int x, int y, int w, int h)
{
	int i;
	if (!atlas || idx < 0 || idx > atlas->nnodes || x < 0 || y < 0 ||
	    w <= 0 || h <= 0 || x > atlas->width-w ||
	    y > atlas->height-h)
		return 0;

	// Insert new node
	if (fons__atlasInsertNode(atlas, idx, x, y+h, w) == 0)
		return 0;

	// Delete skyline segments that fall under the shadow of the new segment.
	for (i = idx+1; i < atlas->nnodes; i++) {
		if (atlas->nodes[i].x < atlas->nodes[i-1].x + atlas->nodes[i-1].width) {
			int shrink = atlas->nodes[i-1].x + atlas->nodes[i-1].width - atlas->nodes[i].x;
				atlas->nodes[i].x += shrink;
				atlas->nodes[i].width -= shrink;
			if (atlas->nodes[i].width <= 0) {
				fons__atlasRemoveNode(atlas, i);
				i--;
			} else {
				break;
			}
		} else {
			break;
		}
	}

	// Merge same height skyline segments that are next to each other.
	for (i = 0; i < atlas->nnodes-1; i++) {
		if (atlas->nodes[i].y == atlas->nodes[i+1].y) {
			atlas->nodes[i].width += atlas->nodes[i+1].width;
			fons__atlasRemoveNode(atlas, i+1);
			i--;
		}
	}

	return 1;
}

static int fons__atlasRectFits(FONSatlas* atlas, int i, int w, int h)
{
	// Checks if there is enough space at the location of skyline span 'i',
	// and return the max height of all skyline spans under that at that location,
	// (think tetris block being dropped at that position). Or -1 if no space found.
	int x, y;
	int spaceLeft;
	if (!atlas || !atlas->nodes || i < 0 || i >= atlas->nnodes ||
	    w <= 0 || h <= 0)
		return -1;
	x = atlas->nodes[i].x;
	y = atlas->nodes[i].y;
	if (w > atlas->width || x > atlas->width-w)
		return -1;
	spaceLeft = w;
	while (spaceLeft > 0) {
		if (i >= atlas->nnodes || atlas->nodes[i].width <= 0) return -1;
		y = fons__maxi(y, atlas->nodes[i].y);
		if (h > atlas->height || y > atlas->height-h) return -1;
		spaceLeft -= atlas->nodes[i].width;
		++i;
	}
	return y;
}

static int fons__atlasAddRect(FONSatlas* atlas, int rw, int rh, int* rx, int* ry)
{
	int besth, bestw, besti = -1;
	int bestx = -1, besty = -1, i;
	if (!atlas || !rx || !ry || rw <= 0 || rh <= 0 ||
	    rw > atlas->width || rh > atlas->height)
		return 0;
	besth = atlas->height;
	bestw = atlas->width;

	// Bottom left fit heuristic.
	for (i = 0; i < atlas->nnodes; i++) {
		int y = fons__atlasRectFits(atlas, i, rw, rh);
		if (y != -1) {
			if (y + rh < besth || (y + rh == besth && atlas->nodes[i].width < bestw)) {
				besti = i;
				bestw = atlas->nodes[i].width;
				besth = y + rh;
				bestx = atlas->nodes[i].x;
				besty = y;
			}
		}
	}

	if (besti == -1)
		return 0;

	// Perform the actual packing.
	if (fons__atlasAddSkylineLevel(atlas, besti, bestx, besty, rw, rh) == 0)
		return 0;

	*rx = bestx;
	*ry = besty;

	return 1;
}

static void fons__addWhiteRect(FONScontext* stash, int w, int h)
{
	int x, y, gx, gy;
	unsigned char* dst;
	if (!stash || !stash->texData || !stash->atlas ||
	    fons__atlasAddRect(stash->atlas, w, h, &gx, &gy) == 0)
		return;

	// Rasterize
	dst = &stash->texData[(size_t)gx+(size_t)gy*(size_t)stash->params.width];
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			dst[x] = 0xff;
		dst += stash->params.width;
	}

	stash->dirtyRect[0] = fons__mini(stash->dirtyRect[0], gx);
	stash->dirtyRect[1] = fons__mini(stash->dirtyRect[1], gy);
	stash->dirtyRect[2] = fons__maxi(stash->dirtyRect[2], gx+w);
	stash->dirtyRect[3] = fons__maxi(stash->dirtyRect[3], gy+h);
}

FONScontext* fonsCreateInternal(const FONSparams* params)
{
	FONScontext* stash = NULL;
	size_t textureBytes;

	if (!params || !fons__validAtlasSize(params->width, params->height)) {
		if (params && params->renderDelete)
			params->renderDelete(params->userPtr);
		return NULL;
	}
	textureBytes = (size_t)params->width*(size_t)params->height;

	// Allocate memory for the font stash.
	stash = (FONScontext*)malloc(sizeof(FONScontext));
	if (stash == NULL) {
		if (params->renderDelete)
			params->renderDelete(params->userPtr);
		return NULL;
	}
	memset(stash, 0, sizeof(FONScontext));

	stash->params = *params;

	stash->atlas = fons__allocAtlas(stash->params.width, stash->params.height, FONS_INIT_ATLAS_NODES);
	if (stash->atlas == NULL) goto error;

	// Allocate space for fonts.
	if (FONS_INIT_FONTS <= 0 ||
	    (size_t)FONS_INIT_FONTS > ((size_t)-1)/sizeof(FONSfont*))
		goto error;
	stash->fonts = (FONSfont**)malloc(
		sizeof(FONSfont*)*(size_t)FONS_INIT_FONTS);
	if (stash->fonts == NULL) goto error;
	memset(stash->fonts, 0,
	       sizeof(FONSfont*)*(size_t)FONS_INIT_FONTS);
	stash->cfonts = FONS_INIT_FONTS;
	stash->nfonts = 0;

	// Create texture for the cache.
	stash->itw = 1.0f/stash->params.width;
	stash->ith = 1.0f/stash->params.height;
	stash->texData = (unsigned char*)malloc(textureBytes);
	if (stash->texData == NULL) goto error;
	memset(stash->texData, 0, textureBytes);

	if (stash->params.renderCreate != NULL &&
	    stash->params.renderCreate(stash->params.userPtr,
				       stash->params.width,
				       stash->params.height) == 0)
		goto error;

	stash->dirtyRect[0] = stash->params.width;
	stash->dirtyRect[1] = stash->params.height;
	stash->dirtyRect[2] = 0;
	stash->dirtyRect[3] = 0;

	// Add white rect at 0,0 for debug drawing.
	fons__addWhiteRect(stash, 2,2);

	fonsPushState(stash);
	fonsClearState(stash);

	return stash;

error:
	fonsDeleteInternal(stash);
	return NULL;
}

static FONSstate* fons__getState(FONScontext* stash)
{
	if (!stash || stash->nstates <= 0 || stash->nstates > FONS_MAX_STATES)
		return NULL;
	return &stash->states[stash->nstates-1];
}

int fonsAddFallbackFont(FONScontext* stash, int base, int fallback)
{
	FONSfont* baseFont;
	if (!stash || base < 0 || fallback < 0 || base >= stash->nfonts ||
	    fallback >= stash->nfonts)
		return 0;
	baseFont = stash->fonts[base];
	if (baseFont->nfallbacks < FONS_MAX_FALLBACKS) {
		baseFont->fallbacks[baseFont->nfallbacks++] = fallback;
		return 1;
	}
	return 0;
}

void fonsSetSize(FONScontext* stash, float size)
{
	FONSstate* state = fons__getState(stash);
	if (state && size >= 0.0f && size <= 1000000.0f)
		state->size = size;
}

void fonsSetColor(FONScontext* stash, unsigned int color)
{
	FONSstate* state = fons__getState(stash);
	if (state) state->color = color;
}

void fonsSetSpacing(FONScontext* stash, float spacing)
{
	FONSstate* state = fons__getState(stash);
	if (state && spacing >= -1000000.0f && spacing <= 1000000.0f)
		state->spacing = spacing;
}

void fonsSetBlur(FONScontext* stash, float blur)
{
	FONSstate* state = fons__getState(stash);
	if (state && blur >= 0.0f && blur <= 20.0f)
		state->blur = blur;
}

void fonsSetAlign(FONScontext* stash, int align)
{
	FONSstate* state = fons__getState(stash);
	if (state) state->align = align;
}

void fonsSetFont(FONScontext* stash, int font)
{
	FONSstate* state = fons__getState(stash);
	if (state) state->font = font;
}

void fonsPushState(FONScontext* stash)
{
	if (!stash) return;
	if (stash->nstates >= FONS_MAX_STATES) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONS_STATES_OVERFLOW, 0);
		return;
	}
	if (stash->nstates > 0)
		memcpy(&stash->states[stash->nstates], &stash->states[stash->nstates-1], sizeof(FONSstate));
	stash->nstates++;
}

void fonsPopState(FONScontext* stash)
{
	if (!stash) return;
	if (stash->nstates <= 1) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONS_STATES_UNDERFLOW, 0);
		return;
	}
	stash->nstates--;
}

void fonsClearState(FONScontext* stash)
{
	FONSstate* state = fons__getState(stash);
	if (!state) return;
	state->size = 12.0f;
	state->color = 0xffffffff;
	state->font = 0;
	state->blur = 0;
	state->spacing = 0;
	state->align = FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE;
}

static void fons__freeFont(FONSfont* font)
{
	if (font == NULL) return;
	if (font->glyphs) free(font->glyphs);
	if (font->freeData && font->data) free(font->data);
	free(font);
}

static int fons__allocFont(FONScontext* stash)
{
	FONSfont* font = NULL;
	if (!stash) return FONS_INVALID;
	if (stash->nfonts+1 > stash->cfonts) {
		FONSfont** fonts;
		int capacity;
		if (!fons__growCapacity(stash->cfonts, stash->nfonts+1,
					 sizeof(FONSfont*), &capacity))
			return FONS_INVALID;
		fonts = (FONSfont**)realloc(
			stash->fonts, sizeof(FONSfont*)*(size_t)capacity);
		if (fonts == NULL)
			return FONS_INVALID;
		stash->fonts = fonts;
		stash->cfonts = capacity;
	}
	font = (FONSfont*)malloc(sizeof(FONSfont));
	if (font == NULL) goto error;
	memset(font, 0, sizeof(FONSfont));

	if (FONS_INIT_GLYPHS <= 0 ||
	    (size_t)FONS_INIT_GLYPHS > ((size_t)-1)/sizeof(FONSglyph))
		goto error;
	font->glyphs = (FONSglyph*)malloc(
		sizeof(FONSglyph)*(size_t)FONS_INIT_GLYPHS);
	if (font->glyphs == NULL) goto error;
	font->cglyphs = FONS_INIT_GLYPHS;
	font->nglyphs = 0;

	stash->fonts[stash->nfonts++] = font;
	return stash->nfonts-1;

error:
	fons__freeFont(font);

	return FONS_INVALID;
}

static FILE* fons__fopen(const char* filename, const char* mode)
{
#ifdef _WIN32
	int len = 0;
	size_t fileBytes;
	size_t modeBytes;
	int fileLen;
	int modeLen;
	wchar_t wpath[MAX_PATH];
	wchar_t wmode[MAX_PATH];
	FILE* f;

	if (!filename || !mode)
		return NULL;
	fileBytes = strlen(filename);
	modeBytes = strlen(mode);
	if (fileBytes == 0 || fileBytes >= MAX_PATH)
		return NULL;
	if (modeBytes == 0 || modeBytes >= MAX_PATH)
		return NULL;
	fileLen = (int)fileBytes;
	modeLen = (int)modeBytes;
	len = MultiByteToWideChar(CP_UTF8, 0, filename, fileLen, wpath, fileLen);
	if (len >= MAX_PATH)
		return NULL;
	wpath[len] = L'\0';
	len = MultiByteToWideChar(CP_UTF8, 0, mode, modeLen, wmode, modeLen);
	if (len >= MAX_PATH)
		return NULL;
	wmode[len] = L'\0';
	f = _wfopen(wpath, wmode);
	return f;
#else
	return fopen(filename, mode);
#endif
}

int fonsAddFont(FONScontext* stash, const char* name, const char* path)
{
	FILE* fp = 0;
	long fileSize;
	int dataSize, font;
	size_t readBytes;
	unsigned char* data = NULL;
	if (!stash || !name || !path) return FONS_INVALID;

	// Read in the font data.
	fp = fons__fopen(path, "rb");
	if (fp == NULL) goto error;
	if (fseek(fp, 0, SEEK_END) != 0)
	   goto error;
	fileSize = ftell(fp);
	if (fileSize < 1 || fileSize > 0x7fffffffL ||
	    fseek(fp, 0, SEEK_SET) != 0)
		goto error;
	dataSize = (int)fileSize;
	data = (unsigned char*)malloc((size_t)dataSize);
	if (data == NULL) goto error;
	readBytes = fread(data, 1, (size_t)dataSize, fp);
	fclose(fp);
	fp = 0;
	if (readBytes != (size_t)dataSize) goto error;

	font = fonsAddFontMem(stash, name, data, dataSize, 1);
	if (font == FONS_INVALID)
		return FONS_INVALID;
	return font;

error:
	if (data) free(data);
	if (fp) fclose(fp);
	return FONS_INVALID;
}

int fonsAddFontMem(FONScontext* stash, const char* name, unsigned char* data, int dataSize, int freeData)
{
	int i, ascent, descent, fh, lineGap;
	FONSfont* font;

	int idx;
	if (!stash || !name || !data || dataSize <= 0) {
		if (freeData && data) free(data);
		return FONS_INVALID;
	}
	idx = fons__allocFont(stash);
	if (idx == FONS_INVALID) {
		if (freeData) free(data);
		return FONS_INVALID;
	}

	font = stash->fonts[idx];

	strncpy(font->name, name, sizeof(font->name));
	font->name[sizeof(font->name)-1] = '\0';

	// Init hash lookup.
	for (i = 0; i < FONS_HASH_LUT_SIZE; ++i)
		font->lut[i] = -1;

	// Read in the font data.
	font->dataSize = dataSize;
	font->data = data;
	font->freeData = freeData ? 1 : 0;

	// Init font
	if (!fons__tt_loadFont(stash, &font->font, data, dataSize)) goto error;

	// Store normalized line height. The real line height is got
	// by multiplying the lineh by font size.
	fons__tt_getFontVMetrics( &font->font, &ascent, &descent, &lineGap);
	fh = ascent - descent;
	if (fh == 0) goto error;
	font->ascender = (float)ascent / (float)fh;
	font->descender = (float)descent / (float)fh;
	font->lineh = (float)(fh + lineGap) / (float)fh;

	return idx;

error:
	fons__freeFont(font);
	stash->nfonts--;
	stash->fonts[stash->nfonts] = NULL;
	return FONS_INVALID;
}

int fonsGetFontByName(FONScontext* s, const char* name)
{
	int i;
	if (!s || !name) return FONS_INVALID;
	for (i = 0; i < s->nfonts; i++) {
		if (strcmp(s->fonts[i]->name, name) == 0)
			return i;
	}
	return FONS_INVALID;
}


static FONSglyph* fons__allocGlyph(FONSfont* font)
{
	if (!font) return NULL;
	if (font->nglyphs+1 > font->cglyphs) {
		FONSglyph* glyphs;
		int capacity;
		if (!fons__growCapacity(font->cglyphs, font->nglyphs+1,
					 sizeof(FONSglyph), &capacity))
			return NULL;
		glyphs = (FONSglyph*)realloc(
			font->glyphs, sizeof(FONSglyph)*(size_t)capacity);
		if (glyphs == NULL) return NULL;
		font->glyphs = glyphs;
		font->cglyphs = capacity;
	}
	font->nglyphs++;
	return &font->glyphs[font->nglyphs-1];
}


// Based on Exponential blur, Jani Huhtanen, 2006

#define APREC 16
#define ZPREC 7

static void fons__blurCols(unsigned char* dst, int w, int h, int dstStride, int alpha)
{
	int x, y;
	if (!dst || w <= 0 || h <= 0 || dstStride < w) return;
	for (y = 0; y < h; y++) {
		int z = 0; // force zero border
		for (x = 1; x < w; x++) {
			z += (alpha * (((int)(dst[x]) << ZPREC) - z)) >> APREC;
			dst[x] = (unsigned char)(z >> ZPREC);
		}
		dst[w-1] = 0; // force zero border
		z = 0;
		for (x = w-2; x >= 0; x--) {
			z += (alpha * (((int)(dst[x]) << ZPREC) - z)) >> APREC;
			dst[x] = (unsigned char)(z >> ZPREC);
		}
		dst[0] = 0; // force zero border
		dst += dstStride;
	}
}

static void fons__blurRows(unsigned char* dst, int w, int h, int dstStride, int alpha)
{
	int x, y;
	if (!dst || w <= 0 || h <= 0 || dstStride < w) return;
	for (x = 0; x < w; x++) {
		int z = 0; // force zero border
		for (y = dstStride; y < h*dstStride; y += dstStride) {
			z += (alpha * (((int)(dst[y]) << ZPREC) - z)) >> APREC;
			dst[y] = (unsigned char)(z >> ZPREC);
		}
		dst[(h-1)*dstStride] = 0; // force zero border
		z = 0;
		for (y = (h-2)*dstStride; y >= 0; y -= dstStride) {
			z += (alpha * (((int)(dst[y]) << ZPREC) - z)) >> APREC;
			dst[y] = (unsigned char)(z >> ZPREC);
		}
		dst[0] = 0; // force zero border
		dst++;
	}
}


static void fons__blur(FONScontext* stash, unsigned char* dst, int w, int h, int dstStride, int blur)
{
	int alpha;
	float sigma;
	(void)stash;

	if (blur < 1)
		return;
	// Calculate the alpha such that 90% of the kernel is within the radius. (Kernel extends to infinity)
	sigma = (float)blur * 0.57735f; // 1 / sqrt(3)
	alpha = (int)((1<<APREC) * (1.0f - expf(-2.3f / (sigma+1.0f))));
	fons__blurRows(dst, w, h, dstStride, alpha);
	fons__blurCols(dst, w, h, dstStride, alpha);
	fons__blurRows(dst, w, h, dstStride, alpha);
	fons__blurCols(dst, w, h, dstStride, alpha);
//	fons__blurrows(dst, w, h, dstStride, alpha);
//	fons__blurcols(dst, w, h, dstStride, alpha);
}

static FONSglyph* fons__getGlyph(FONScontext* stash, FONSfont* font,
					 unsigned int codepoint, int isize, int iblur)
{
	int i, g, advance, lsb, x0, y0, x1, y1, gw, gh, gx, gy, x, y;
	long long gw64, gh64;
	float scale;
	FONSglyph* glyph = NULL;
	unsigned int h;
	float size = isize/10.0f;
	int pad, added;
	unsigned char* bdst;
	unsigned char* dst;
	FONSfont* renderFont = font;

	if (!stash || !font || !font->data || isize < 2 || iblur < 0)
		return NULL;
	if (iblur > 20) iblur = 20;
	pad = iblur+2;

	// Find code point and size.
	if (FONS_HASH_LUT_SIZE <= 0) return NULL;
	h = fons__hashint(codepoint) % FONS_HASH_LUT_SIZE;
	i = font->lut[h];
	while (i != -1) {
		if (i < 0 || i >= font->nglyphs) return NULL;
		if (font->glyphs[i].codepoint == codepoint && font->glyphs[i].size == isize && font->glyphs[i].blur == iblur)
			return &font->glyphs[i];
		i = font->glyphs[i].next;
	}

	// Could not find glyph, create it.
	g = fons__tt_getGlyphIndex(&font->font, codepoint);
	// Try to find the glyph in fallback fonts.
	if (g == 0) {
		for (i = 0; i < font->nfallbacks; ++i) {
			int fallback = font->fallbacks[i];
			FONSfont* fallbackFont;
			if (fallback < 0 || fallback >= stash->nfonts)
				continue;
			fallbackFont = stash->fonts[fallback];
			int fallbackIndex = fons__tt_getGlyphIndex(&fallbackFont->font, codepoint);
			if (fallbackIndex != 0) {
				g = fallbackIndex;
				renderFont = fallbackFont;
				break;
			}
		}
		// It is possible that we did not find a fallback glyph.
		// In that case the glyph index 'g' is 0, and we'll proceed below and cache empty glyph.
	}
	scale = fons__tt_getPixelHeightScale(&renderFont->font, size);
	if (!(scale > 0.0f) || !(scale <= 3.4e38f) ||
	    !fons__tt_buildGlyphBitmap(&renderFont->font, g, scale,
				       &advance, &lsb, &x0, &y0, &x1, &y1))
		return NULL;
	gw64 = (long long)x1-(long long)x0+(long long)pad*2;
	gh64 = (long long)y1-(long long)y0+(long long)pad*2;
	if (gw64 <= 0 || gh64 <= 0 || gw64 > INT_MAX || gh64 > INT_MAX)
		return NULL;
	gw = (int)gw64;
	gh = (int)gh64;
	if ((gw > stash->atlas->width || gh > stash->atlas->height) &&
	    stash->handleError != NULL)
		stash->handleError(stash->errorUptr, FONS_ATLAS_FULL,
				   fons__maxi(gw, gh));
	if (gw > stash->atlas->width || gh > stash->atlas->height)
		return NULL;

	// Find free spot for the rect in the atlas
	added = fons__atlasAddRect(stash->atlas, gw, gh, &gx, &gy);
	if (added == 0 && stash->handleError != NULL) {
		// Atlas is full, let the user to resize the atlas (or not), and try again.
		stash->handleError(stash->errorUptr, FONS_ATLAS_FULL, 0);
		added = fons__atlasAddRect(stash->atlas, gw, gh, &gx, &gy);
	}
	if (added == 0) return NULL;

	// Init glyph.
	glyph = fons__allocGlyph(font);
	if (!glyph) return NULL;
	memset(glyph, 0, sizeof(*glyph));
	glyph->codepoint = codepoint;
	glyph->size = isize;
	glyph->blur = iblur;
	glyph->index = g;
	glyph->x0 = gx;
	glyph->y0 = gy;
	glyph->x1 = glyph->x0+gw;
	glyph->y1 = glyph->y0+gh;
	glyph->xadv = scale*(float)advance*10.0f;
	glyph->xoff = (float)x0-(float)pad;
	glyph->yoff = (float)y0-(float)pad;
	glyph->scale = scale;
	glyph->renderFont = renderFont;
	glyph->next = 0;

	// Insert char to hash lookup.
	glyph->next = font->lut[h];
	font->lut[h] = font->nglyphs-1;

	// Rasterize
	dst = &stash->texData[(size_t)(glyph->x0+pad)+
			      (size_t)(glyph->y0+pad)*
			      (size_t)stash->params.width];
	fons__tt_renderGlyphBitmap(&renderFont->font, dst, gw-pad*2,gh-pad*2, stash->params.width, scale,scale, g);

	// Make sure there is one pixel empty border.
	dst = &stash->texData[(size_t)glyph->x0+
			      (size_t)glyph->y0*(size_t)stash->params.width];
	for (y = 0; y < gh; y++) {
		dst[y*stash->params.width] = 0;
		dst[gw-1 + y*stash->params.width] = 0;
	}
	for (x = 0; x < gw; x++) {
		dst[x] = 0;
		dst[x + (gh-1)*stash->params.width] = 0;
	}

	// Debug code to color the glyph background
/*	unsigned char* fdst = &stash->texData[glyph->x0 + glyph->y0 * stash->params.width];
	for (y = 0; y < gh; y++) {
		for (x = 0; x < gw; x++) {
			int a = (int)fdst[x+y*stash->params.width] + 20;
			if (a > 255) a = 255;
			fdst[x+y*stash->params.width] = a;
		}
	}*/

	// Blur
	if (iblur > 0) {
		bdst = &stash->texData[(size_t)glyph->x0+
				(size_t)glyph->y0*(size_t)stash->params.width];
		fons__blur(stash, bdst, gw,gh, stash->params.width, iblur);
	}

	stash->dirtyRect[0] = fons__mini(stash->dirtyRect[0], glyph->x0);
	stash->dirtyRect[1] = fons__mini(stash->dirtyRect[1], glyph->y0);
	stash->dirtyRect[2] = fons__maxi(stash->dirtyRect[2], glyph->x1);
	stash->dirtyRect[3] = fons__maxi(stash->dirtyRect[3], glyph->y1);

	return glyph;
}

static void fons__getQuad(FONScontext* stash, FONSfont* prevFont,
						   int prevGlyphIndex, FONSglyph* glyph,
						   float spacing, float* x, float* y, FONSquad* q)
{
	float rx,ry,xoff,yoff,x0,y0,x1,y1;

	if (!stash || !glyph || !glyph->renderFont || !x || !y || !q)
		return;
	if (prevGlyphIndex != -1 && prevFont == glyph->renderFont) {
		float adv = fons__tt_getGlyphKernAdvance(
			&prevFont->font, prevGlyphIndex, glyph->index)*glyph->scale;
		*x += (int)(adv + spacing + 0.5f);
	}

	// Each glyph has 2px border to allow good interpolation,
	// one pixel to prevent leaking, and one to allow good interpolation for rendering.
	// Inset the texture region by one pixel for correct interpolation.
	xoff = glyph->xoff+1.0f;
	yoff = glyph->yoff+1.0f;
	x0 = (float)(glyph->x0+1);
	y0 = (float)(glyph->y0+1);
	x1 = (float)(glyph->x1-1);
	y1 = (float)(glyph->y1-1);

	if (stash->params.flags & FONS_ZERO_TOPLEFT) {
		rx = (float)(int)(*x + xoff);
		ry = (float)(int)(*y + yoff);

		q->x0 = rx;
		q->y0 = ry;
		q->x1 = rx + x1 - x0;
		q->y1 = ry + y1 - y0;

		q->s0 = x0 * stash->itw;
		q->t0 = y0 * stash->ith;
		q->s1 = x1 * stash->itw;
		q->t1 = y1 * stash->ith;
	} else {
		rx = (float)(int)(*x + xoff);
		ry = (float)(int)(*y - yoff);

		q->x0 = rx;
		q->y0 = ry;
		q->x1 = rx + x1 - x0;
		q->y1 = ry - y1 + y0;

		q->s0 = x0 * stash->itw;
		q->t0 = y0 * stash->ith;
		q->s1 = x1 * stash->itw;
		q->t1 = y1 * stash->ith;
	}

	*x += (int)(glyph->xadv / 10.0f + 0.5f);
}

static void fons__flush(FONScontext* stash)
{
	if (!stash) return;
	// Flush texture
	if (stash->dirtyRect[0] < stash->dirtyRect[2] && stash->dirtyRect[1] < stash->dirtyRect[3]) {
		if (stash->params.renderUpdate != NULL)
			stash->params.renderUpdate(stash->params.userPtr, stash->dirtyRect, stash->texData);
		// Reset dirty rect
		stash->dirtyRect[0] = stash->params.width;
		stash->dirtyRect[1] = stash->params.height;
		stash->dirtyRect[2] = 0;
		stash->dirtyRect[3] = 0;
	}

	// Flush triangles
	if (stash->nverts > 0) {
		if (stash->params.renderDraw != NULL)
			stash->params.renderDraw(stash->params.userPtr, stash->verts, stash->tcoords, stash->colors, stash->nverts);
		stash->nverts = 0;
	}
}

static __inline void fons__vertex(FONScontext* stash, float x, float y, float s, float t, unsigned int c)
{
	if (!stash || stash->nverts < 0 || stash->nverts >= FONS_VERTEX_COUNT)
		return;
	stash->verts[stash->nverts*2+0] = x;
	stash->verts[stash->nverts*2+1] = y;
	stash->tcoords[stash->nverts*2+0] = s;
	stash->tcoords[stash->nverts*2+1] = t;
	stash->colors[stash->nverts] = c;
	stash->nverts++;
}

static float fons__getVertAlign(FONScontext* stash, FONSfont* font,
				int align, int isize)
{
	if (!stash || !font) return 0.0f;
	if (stash->params.flags & FONS_ZERO_TOPLEFT) {
		if (align & FONS_ALIGN_TOP) {
			return font->ascender * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_MIDDLE) {
			return (font->ascender + font->descender) / 2.0f * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_BASELINE) {
			return 0.0f;
		} else if (align & FONS_ALIGN_BOTTOM) {
			return font->descender * (float)isize/10.0f;
		}
	} else {
		if (align & FONS_ALIGN_TOP) {
			return -font->ascender * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_MIDDLE) {
			return -(font->ascender + font->descender) / 2.0f * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_BASELINE) {
			return 0.0f;
		} else if (align & FONS_ALIGN_BOTTOM) {
			return -font->descender * (float)isize/10.0f;
		}
	}
	return 0.0;
}

FONS_DEF float fonsDrawText(FONScontext* stash,
				   float x, float y,
				   const char* str, const char* end)
{
	FONSstate* state = fons__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSglyph* glyph = NULL;
	FONSfont* prevFont = NULL;
	FONSquad q;
	int prevGlyphIndex = -1;
	int isize, iblur;
	FONSfont* font;
	float width;

	if (!stash || !state || !str || FONS_VERTEX_COUNT < 6) return x;
	if (state->font < 0 || state->font >= stash->nfonts) return x;
	isize = (int)(state->size*10.0f);
	iblur = (int)state->blur;
	font = stash->fonts[state->font];
	if (font->data == NULL) return x;

	if (end == NULL)
		end = str + strlen(str);

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(stash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(stash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += fons__getVertAlign(stash, font, state->align, isize);

	while (fons__nextCodepoint(&str, end, &utf8state, &codepoint)) {
		glyph = fons__getGlyph(stash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			fons__getQuad(stash, prevFont, prevGlyphIndex, glyph,
				      state->spacing, &x, &y, &q);

			if (stash->nverts+6 > FONS_VERTEX_COUNT)
				fons__flush(stash);

			fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, state->color);
			fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, state->color);
			fons__vertex(stash, q.x1, q.y0, q.s1, q.t0, state->color);

			fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, state->color);
			fons__vertex(stash, q.x0, q.y1, q.s0, q.t1, state->color);
			fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, state->color);
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
		prevFont = glyph != NULL ? glyph->renderFont : NULL;
	}
	fons__flush(stash);

	return x;
}

FONS_DEF int fonsTextIterInit(FONScontext* stash, FONStextIter* iter,
					 float x, float y, const char* str, const char* end)
{
	FONSstate* state = fons__getState(stash);
	float width;

	if (!iter) return 0;
	memset(iter, 0, sizeof(*iter));

	if (!stash || !state || !str) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	iter->font = stash->fonts[state->font];
	if (iter->font->data == NULL) return 0;

	iter->isize = (int)(state->size*10.0f);
	iter->iblur = (int)state->blur;
	iter->scale = fons__tt_getPixelHeightScale(&iter->font->font, (float)iter->isize/10.0f);

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(stash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(stash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += fons__getVertAlign(stash, iter->font, state->align, iter->isize);

	if (end == NULL)
		end = str + strlen(str);

	iter->x = iter->nextx = x;
	iter->y = iter->nexty = y;
	iter->spacing = state->spacing;
	iter->str = str;
	iter->next = str;
	iter->end = end;
	iter->codepoint = 0;
	iter->prevFont = NULL;
	iter->prevGlyphIndex = -1;

	return 1;
}

FONS_DEF int fonsTextIterNext(FONScontext* stash, FONStextIter* iter, FONSquad* quad)
{
	FONSglyph* glyph = NULL;
	if (!stash || !iter || !quad || !iter->font || !iter->next ||
	    !iter->end)
		return 0;
	iter->str = iter->next;
	memset(quad, 0, sizeof(*quad));
	if (!fons__nextCodepoint(&iter->next, iter->end, &iter->utf8state,
				 &iter->codepoint))
		return 0;

	// Get glyph and quad
	iter->x = iter->nextx;
	iter->y = iter->nexty;
	glyph = fons__getGlyph(stash, iter->font, iter->codepoint,
			       iter->isize, iter->iblur);
	if (glyph != NULL)
		fons__getQuad(stash, iter->prevFont,
			      iter->prevGlyphIndex, glyph,
			      iter->spacing, &iter->nextx,
			      &iter->nexty, quad);
	iter->prevGlyphIndex = glyph != NULL ? glyph->index : -1;
	iter->prevFont = glyph != NULL ? glyph->renderFont : NULL;

	return 1;
}

FONS_DEF void fonsDrawDebug(FONScontext* stash, float x, float y)
{
	int i;
	int w, h;
	if (!stash || !stash->atlas || FONS_VERTEX_COUNT < 12) return;
	w = stash->params.width;
	h = stash->params.height;
	float u = w == 0 ? 0 : (1.0f / w);
	float v = h == 0 ? 0 : (1.0f / h);

	if (stash->nverts+6+6 > FONS_VERTEX_COUNT)
		fons__flush(stash);

	// Draw background
	fons__vertex(stash, x+0, y+0, u, v, 0x0fffffff);
	fons__vertex(stash, x+w, y+h, u, v, 0x0fffffff);
	fons__vertex(stash, x+w, y+0, u, v, 0x0fffffff);

	fons__vertex(stash, x+0, y+0, u, v, 0x0fffffff);
	fons__vertex(stash, x+0, y+h, u, v, 0x0fffffff);
	fons__vertex(stash, x+w, y+h, u, v, 0x0fffffff);

	// Draw texture
	fons__vertex(stash, x+0, y+0, 0, 0, 0xffffffff);
	fons__vertex(stash, x+w, y+h, 1, 1, 0xffffffff);
	fons__vertex(stash, x+w, y+0, 1, 0, 0xffffffff);

	fons__vertex(stash, x+0, y+0, 0, 0, 0xffffffff);
	fons__vertex(stash, x+0, y+h, 0, 1, 0xffffffff);
	fons__vertex(stash, x+w, y+h, 1, 1, 0xffffffff);

	// Drawbug draw atlas
	for (i = 0; i < stash->atlas->nnodes; i++) {
		FONSatlasNode* n = &stash->atlas->nodes[i];

		if (stash->nverts+6 > FONS_VERTEX_COUNT)
			fons__flush(stash);

		fons__vertex(stash, x+n->x+0, y+n->y+0, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+n->width, y+n->y+1, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+n->width, y+n->y+0, u, v, 0xc00000ff);

		fons__vertex(stash, x+n->x+0, y+n->y+0, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+0, y+n->y+1, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+n->width, y+n->y+1, u, v, 0xc00000ff);
	}

	fons__flush(stash);
}

FONS_DEF float fonsTextBounds(FONScontext* stash,
					 float x, float y,
					 const char* str, const char* end,
					 float* bounds)
{
	FONSstate* state = fons__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSquad q;
	FONSglyph* glyph = NULL;
	FONSfont* prevFont = NULL;
	int prevGlyphIndex = -1;
	int isize, iblur;
	FONSfont* font;
	float startx, advance;
	float minx, miny, maxx, maxy;

	if (!stash || !state || !str) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	isize = (int)(state->size*10.0f);
	iblur = (int)state->blur;
	font = stash->fonts[state->font];
	if (font->data == NULL) return 0;

	// Align vertically.
	y += fons__getVertAlign(stash, font, state->align, isize);

	minx = maxx = x;
	miny = maxy = y;
	startx = x;

	if (end == NULL)
		end = str + strlen(str);

	while (fons__nextCodepoint(&str, end, &utf8state, &codepoint)) {
		glyph = fons__getGlyph(stash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			fons__getQuad(stash, prevFont, prevGlyphIndex, glyph,
				      state->spacing, &x, &y, &q);
			if (q.x0 < minx) minx = q.x0;
			if (q.x1 > maxx) maxx = q.x1;
			if (stash->params.flags & FONS_ZERO_TOPLEFT) {
				if (q.y0 < miny) miny = q.y0;
				if (q.y1 > maxy) maxy = q.y1;
			} else {
				if (q.y1 < miny) miny = q.y1;
				if (q.y0 > maxy) maxy = q.y0;
			}
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
		prevFont = glyph != NULL ? glyph->renderFont : NULL;
	}

	advance = x - startx;

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		minx -= advance;
		maxx -= advance;
	} else if (state->align & FONS_ALIGN_CENTER) {
		minx -= advance * 0.5f;
		maxx -= advance * 0.5f;
	}

	if (bounds) {
		bounds[0] = minx;
		bounds[1] = miny;
		bounds[2] = maxx;
		bounds[3] = maxy;
	}

	return advance;
}

FONS_DEF void fonsVertMetrics(FONScontext* stash,
					 float* ascender, float* descender, float* lineh)
{
	FONSfont* font;
	FONSstate* state = fons__getState(stash);
	int isize;

	if (!stash || !state) return;
	if (state->font < 0 || state->font >= stash->nfonts) return;
	font = stash->fonts[state->font];
	isize = (int)(state->size*10.0f);
	if (font->data == NULL) return;

	if (ascender)
		*ascender = font->ascender*isize/10.0f;
	if (descender)
		*descender = font->descender*isize/10.0f;
	if (lineh)
		*lineh = font->lineh*isize/10.0f;
}

FONS_DEF void fonsLineBounds(FONScontext* stash, float y, float* miny, float* maxy)
{
	FONSfont* font;
	FONSstate* state = fons__getState(stash);
	int isize;

	if (!stash || !state || !miny || !maxy) return;
	if (state->font < 0 || state->font >= stash->nfonts) return;
	font = stash->fonts[state->font];
	isize = (int)(state->size*10.0f);
	if (font->data == NULL) return;

	y += fons__getVertAlign(stash, font, state->align, isize);

	if (stash->params.flags & FONS_ZERO_TOPLEFT) {
		*miny = y - font->ascender * (float)isize/10.0f;
		*maxy = *miny + font->lineh*isize/10.0f;
	} else {
		*maxy = y + font->descender * (float)isize/10.0f;
		*miny = *maxy - font->lineh*isize/10.0f;
	}
}

FONS_DEF const unsigned char* fonsGetTextureData(FONScontext* stash, int* width, int* height)
{
	if (!stash) return NULL;
	if (width != NULL)
		*width = stash->params.width;
	if (height != NULL)
		*height = stash->params.height;
	return stash->texData;
}

FONS_DEF int fonsValidateTexture(FONScontext* stash, int* dirty)
{
	if (!stash || !dirty) return 0;
	if (stash->dirtyRect[0] < stash->dirtyRect[2] && stash->dirtyRect[1] < stash->dirtyRect[3]) {
		dirty[0] = stash->dirtyRect[0];
		dirty[1] = stash->dirtyRect[1];
		dirty[2] = stash->dirtyRect[2];
		dirty[3] = stash->dirtyRect[3];
		// Reset dirty rect
		stash->dirtyRect[0] = stash->params.width;
		stash->dirtyRect[1] = stash->params.height;
		stash->dirtyRect[2] = 0;
		stash->dirtyRect[3] = 0;
		return 1;
	}
	return 0;
}

FONS_DEF void fonsDeleteInternal(FONScontext* stash)
{
	int i;
	if (stash == NULL) return;

	if (stash->params.renderDelete)
		stash->params.renderDelete(stash->params.userPtr);

	for (i = 0; i < stash->nfonts; ++i)
		fons__freeFont(stash->fonts[i]);

	if (stash->atlas) fons__deleteAtlas(stash->atlas);
	if (stash->fonts) free(stash->fonts);
	if (stash->texData) free(stash->texData);
	free(stash);
}

FONS_DEF void fonsSetErrorCallback(FONScontext* stash, void (*callback)(void* uptr, int error, int val), void* uptr)
{
	if (stash == NULL) return;
	stash->handleError = callback;
	stash->errorUptr = uptr;
}

FONS_DEF void fonsGetAtlasSize(FONScontext* stash, int* width, int* height)
{
	if (stash == NULL) return;
	if (width) *width = stash->params.width;
	if (height) *height = stash->params.height;
}

FONS_DEF int fonsExpandAtlas(FONScontext* stash, int width, int height)
{
	int i;
	unsigned char* data = NULL;
	FONSatlas* atlas = NULL;
	int nodeCapacity;
	if (!stash || !stash->atlas || !stash->texData) return 0;
	if (stash->atlas->nnodes >= INT_MAX) return 0;

	width = fons__maxi(width, stash->params.width);
	height = fons__maxi(height, stash->params.height);
	if (!fons__validAtlasSize(width, height)) return 0;

	if (width == stash->params.width && height == stash->params.height)
		return 1;

	/* Prepare every fallible CPU-side change before resizing the renderer. */
	data = (unsigned char*)calloc((size_t)width*(size_t)height, 1);
	if (!data) return 0;
	for (i = 0; i < stash->params.height; i++) {
		unsigned char* dst = &data[(size_t)i*(size_t)width];
		unsigned char* src = &stash->texData[
			(size_t)i*(size_t)stash->params.width];
		memcpy(dst, src, (size_t)stash->params.width);
	}

	nodeCapacity = fons__maxi(stash->atlas->cnodes,
				  stash->atlas->nnodes+1);
	atlas = fons__allocAtlas(stash->atlas->width, stash->atlas->height,
				nodeCapacity);
	if (!atlas) goto error;
	atlas->nnodes = stash->atlas->nnodes;
	memcpy(atlas->nodes, stash->atlas->nodes,
	       sizeof(FONSatlasNode)*(size_t)atlas->nnodes);
	if (!fons__atlasExpand(atlas, width, height)) goto error;

	/* Flush against the old texture, then atomically commit the new one. */
	fons__flush(stash);
	if (stash->params.renderResize != NULL &&
	    stash->params.renderResize(stash->params.userPtr, width, height) == 0)
		goto error;

	free(stash->texData);
	fons__deleteAtlas(stash->atlas);
	stash->texData = data;
	stash->atlas = atlas;
	data = NULL;
	atlas = NULL;
	stash->params.width = width;
	stash->params.height = height;
	stash->itw = 1.0f/(float)width;
	stash->ith = 1.0f/(float)height;

	/* A resized renderer has no texture contents; upload the full atlas. */
	stash->dirtyRect[0] = 0;
	stash->dirtyRect[1] = 0;
	stash->dirtyRect[2] = width;
	stash->dirtyRect[3] = height;

	return 1;

error:
	if (atlas) fons__deleteAtlas(atlas);
	free(data);
	return 0;
}

FONS_DEF int fonsResetAtlas(FONScontext* stash, int width, int height)
{
	int i, j;
	unsigned char* data = NULL;
	FONSatlas* atlas = NULL;
	if (!stash || !fons__validAtlasSize(width, height)) return 0;

	/* Prepare allocations first so failure leaves the live stash untouched. */
	data = (unsigned char*)calloc((size_t)width*(size_t)height, 1);
	if (!data) return 0;
	atlas = fons__allocAtlas(width, height, FONS_INIT_ATLAS_NODES);
	if (!atlas) {
		free(data);
		return 0;
	}

	// Flush pending glyphs.
	fons__flush(stash);

	// Create new texture
	if (stash->params.renderResize != NULL) {
		if (stash->params.renderResize(stash->params.userPtr, width, height) == 0)
			goto error;
	}

	free(stash->texData);
	fons__deleteAtlas(stash->atlas);
	stash->texData = data;
	stash->atlas = atlas;
	data = NULL;
	atlas = NULL;

	// Reset dirty rect
	stash->dirtyRect[0] = width;
	stash->dirtyRect[1] = height;
	stash->dirtyRect[2] = 0;
	stash->dirtyRect[3] = 0;

	// Reset cached glyphs
	for (i = 0; i < stash->nfonts; i++) {
		FONSfont* font = stash->fonts[i];
		font->nglyphs = 0;
		for (j = 0; j < FONS_HASH_LUT_SIZE; j++)
			font->lut[j] = -1;
	}

	stash->params.width = width;
	stash->params.height = height;
	stash->itw = 1.0f/(float)width;
	stash->ith = 1.0f/(float)height;

	// Add white rect at 0,0 for debug drawing.
	fons__addWhiteRect(stash, 2,2);

	return 1;

error:
	if (atlas) fons__deleteAtlas(atlas);
	free(data);
	return 0;
}

#endif // FONTSTASH_IMPLEMENTATION
