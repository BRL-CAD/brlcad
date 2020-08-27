#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../api.h"

static char *duplicate_string(const char *source) {
    char *target = (char*)malloc(strlen(source) + 1);
    strcpy(target, source);
    return target;
}

/* mesh related functions */

typedef struct {
    char *filename;
    mesh_t *mesh;
    int references;
} cached_mesh_t;

static cached_mesh_t *g_meshes = NULL;

mesh_t *cache_acquire_mesh(const char *filename) {
    if (filename != NULL) {
        cached_mesh_t cached_mesh;
        int num_meshes = darray_size(g_meshes);
        int i;

        for (i = 0; i < num_meshes; i++) {
            if (strcmp(g_meshes[i].filename, filename) == 0) {
                if (g_meshes[i].references > 0) {
                    g_meshes[i].references += 1;
                } else {
                    assert(g_meshes[i].references == 0);
                    assert(g_meshes[i].mesh == NULL);
                    g_meshes[i].mesh = mesh_load(filename);
                    g_meshes[i].references = 1;
                }
                return g_meshes[i].mesh;
            }
        }

        cached_mesh.filename = duplicate_string(filename);
        cached_mesh.mesh = mesh_load(filename);
        cached_mesh.references = 1;
        darray_push(g_meshes, cached_mesh, cached_mesh_t);
        return cached_mesh.mesh;
    } else {
        return NULL;
    }
}

void cache_release_mesh(mesh_t *mesh) {
    if (mesh != NULL) {
        int num_meshes = darray_size(g_meshes);
        int i;
        for (i = 0; i < num_meshes; i++) {
            if (g_meshes[i].mesh == mesh) {
                assert(g_meshes[i].references > 0);
                g_meshes[i].references -= 1;
                if (g_meshes[i].references == 0) {
                    mesh_release(g_meshes[i].mesh);
                    g_meshes[i].mesh = NULL;
                }
                return;
            }
        }
        assert(0);
    }
}

/* skeleton related functions */

typedef struct {
    char *filename;
    skeleton_t *skeleton;
    int references;
} cached_skeleton_t;

static cached_skeleton_t *g_skeletons = NULL;

skeleton_t *cache_acquire_skeleton(const char *filename) {
    if (filename != NULL) {
        cached_skeleton_t cached_skeleton;
        int num_skeletons = darray_size(g_skeletons);
        int i;

        for (i = 0; i < num_skeletons; i++) {
            if (strcmp(g_skeletons[i].filename, filename) == 0) {
                if (g_skeletons[i].references > 0) {
                    g_skeletons[i].references += 1;
                } else {
                    assert(g_skeletons[i].references == 0);
                    assert(g_skeletons[i].skeleton == NULL);
                    g_skeletons[i].skeleton = skeleton_load(filename);
                    g_skeletons[i].references = 1;
                }
                return g_skeletons[i].skeleton;
            }
        }

        cached_skeleton.filename = duplicate_string(filename);
        cached_skeleton.skeleton = skeleton_load(filename);
        cached_skeleton.references = 1;
        darray_push(g_skeletons, cached_skeleton, cached_skeleton_t);
        return cached_skeleton.skeleton;
    } else {
        return NULL;
    }
}

void cache_release_skeleton(skeleton_t *skeleton) {
    if (skeleton != NULL) {
        int num_skeletons = darray_size(g_skeletons);
        int i;
        for (i = 0; i < num_skeletons; i++) {
            if (g_skeletons[i].skeleton == skeleton) {
                assert(g_skeletons[i].references > 0);
                g_skeletons[i].references -= 1;
                if (g_skeletons[i].references == 0) {
                    skeleton_release(g_skeletons[i].skeleton);
                    g_skeletons[i].skeleton = NULL;
                }
                return;
            }
        }
        assert(0);
    }
}

/* texture related functions */

typedef struct {
    char *filename;
    usage_t usage;
    texture_t *texture;
    int references;
} cached_texture_t;

static cached_texture_t *g_textures = NULL;

texture_t *cache_acquire_texture(const char *filename, usage_t usage) {
    if (filename != NULL) {
        cached_texture_t cached_texture;
        int num_textures = darray_size(g_textures);
        int i;

        for (i = 0; i < num_textures; i++) {
            if (strcmp(g_textures[i].filename, filename) == 0) {
                if (g_textures[i].usage == usage) {
                    if (g_textures[i].references > 0) {
                        g_textures[i].references += 1;
                    } else {
                        assert(g_textures[i].references == 0);
                        assert(g_textures[i].texture == NULL);
                        g_textures[i].texture = texture_from_file(filename,
                                                                  usage);
                        g_textures[i].references = 1;
                    }
                    return g_textures[i].texture;
                }
            }
        }

        cached_texture.filename = duplicate_string(filename);
        cached_texture.usage = usage;
        cached_texture.texture = texture_from_file(filename, usage);
        cached_texture.references = 1;
        darray_push(g_textures, cached_texture, cached_texture_t);
        return cached_texture.texture;
    } else {
        return NULL;
    }
}

void cache_release_texture(texture_t *texture) {
    if (texture != NULL) {
        int num_textures = darray_size(g_textures);
        int i;
        for (i = 0; i < num_textures; i++) {
            if (g_textures[i].texture == texture) {
                assert(g_textures[i].references > 0);
                g_textures[i].references -= 1;
                if (g_textures[i].references == 0) {
                    texture_release(g_textures[i].texture);
                    g_textures[i].texture = NULL;
                }
                return;
            }
        }
        assert(0);
    }
}

/* misc cache functions */

void cache_cleanup(void) {
    int num_meshes = darray_size(g_meshes);
    int num_skeletons = darray_size(g_skeletons);
    int num_textures = darray_size(g_textures);
    int i;
    for (i = 0; i < num_meshes; i++) {
        assert(g_meshes[i].mesh == NULL);
        assert(g_meshes[i].references == 0);
        free(g_meshes[i].filename);
    }
    for (i = 0; i < num_skeletons; i++) {
        assert(g_skeletons[i].skeleton == NULL);
        assert(g_skeletons[i].references == 0);
        free(g_skeletons[i].filename);
    }
    for (i = 0; i < num_textures; i++) {
        assert(g_textures[i].texture == NULL);
        assert(g_textures[i].references == 0);
        free(g_textures[i].filename);
    }
    darray_free(g_meshes);
    darray_free(g_skeletons);
    darray_free(g_textures);
    g_meshes = NULL;
    g_skeletons = NULL;
    g_textures = NULL;
}
