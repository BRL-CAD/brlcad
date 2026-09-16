# BRL-CAD Fontstash

Fontstash is BRL-CAD's private, display-oriented text rasterizer.  It builds a
bitmap glyph atlas on demand and emits textured triangles through renderer
callbacks.  The Qt/OpenGL and software-raster OpenGL display-manager plugins
use the implementation in this directory.

- atlas allocation and resizing are renderer resources;
- cached glyph bitmaps have display-resolution-dependent sizes and blur;
- colors, textured quads, batching, and renderer callbacks are drawing policy;
- linking it from annotation geometry would create an inappropriate
  `librt`-to-`libdm` dependency and would discard scalable outlines.

BRL-CAD's private `libbu` source tree instead holds `struetype.h`, the compact,
size-aware font parser and outline/raster API used by Fontstash.  Annotation
geometry may share that lower-level parser and its metrics/outlines without
depending on Fontstash.

## BRL-CAD changes

This copy has diverged substantially from the historical Fontstash code.  In
particular, it:

- uses BRL-CAD's size-aware `struetype.h` rather than `stb_truetype.h`;
- validates font buffers and atlas dimensions before parsing or allocation;
- owns renderer state through a copied `FONSparams` value;
- performs atlas expansion and reset transactionally;
- uses full-width atlas and glyph coordinates;
- handles malformed and truncated UTF-8 with replacement characters;
- preserves the actual fallback font used by each cached glyph;
- avoids the historical fixed scratch allocator; and
- tolerates invalid arguments at the public API boundary.

The renderer `userPtr` is transferred to `fonsCreateInternal`, even if context
creation fails.  Its `renderDelete` callback is therefore called exactly once
by Fontstash.  A renderer resize callback must leave its old resource usable
when it returns failure; Fontstash likewise leaves its CPU and atlas state
unchanged.

`src/libdm/tests/fontstash_test.c` exercises the implementation with a mock
renderer, so ownership, UTF-8 handling, and atlas changes can be checked
without an OpenGL context.

## Integration pattern

One display backend translation unit defines the implementations before
including the headers:

```c
#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"
#define GLFONTSTASH_IMPLEMENTATION
#include "glfontstash.h"
```

Call `glfonsCreate` only with a current OpenGL context.  Load fonts with
`fonsAddFont`, draw with `fonsDrawText`, and call `glfonsDelete` while that
context is still current.

Additional renderer backends implement the callbacks in `FONSparams`.
`renderUpdate` receives a changed rectangle plus the full CPU atlas;
`renderDraw` receives triangle vertices, texture coordinates, and packed RGBA
colors.

## Provenance and license

The original Fontstash implementation is by Mikko Mononen.  The atlas is
based on Jukka Jylanki's public-domain Skyline Bin Packer, and the UTF-8 state
machine is by Bjoern Hoehrmann.  Complete notices and source locations are in
`doc/legal/embedded/fontstash.txt`; struetype is recorded separately in
`doc/legal/embedded/struetype.txt`.

The note in the original BRL-CAD integration still applies: a future modern
OpenGL renderer must coexist with the compatibility path because the software
raster backend currently targets OpenGL 2.1 capabilities.
