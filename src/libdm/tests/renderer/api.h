#ifndef API_H
#define API_H

#define SMALL_FASTF 1.0e-37
#define NEAR_ZERO(val, epsilon) (((val) > -epsilon) && ((val) < epsilon))
#define NEAR_EQUAL(_a, _b, _tol) NEAR_ZERO((_a) - (_b), (_tol))

#ifdef __cplusplus
extern "C" {
#endif

#include "camera.h"
#include "darray.h"
#include "draw2d.h"
#include "graphics.h"
#include "image.h"
#include "macro.h"
#include "maths.h"
#include "mesh.h"
#include "scene.h"
#include "skeleton.h"
#include "texture.h"

scene_t *scene_from_file(const char *filename, mat4_t root);
scene_t *blinn_lighthouse_scene(void);

/* mesh related functions */
mesh_t *cache_acquire_mesh(const char *filename);
void cache_release_mesh(mesh_t *mesh);

/* skeleton related functions */
skeleton_t *cache_acquire_skeleton(const char *filename);
void cache_release_skeleton(skeleton_t *skeleton);

/* texture related functions */
texture_t *cache_acquire_texture(const char *filename, usage_t usage);
void cache_release_texture(texture_t *texture);

/* misc cache functions */
void cache_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
