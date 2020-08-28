#include <stdlib.h>
#include "darray.h"
#include "graphics.h"
#include "maths.h"
#include "mesh.h"
#include "scene.h"
#include "texture.h"

scene_t *scene_create(vec3_t background, model_t **models,
                      float ambient_intensity, float punctual_intensity) {
    scene_t *scene = (scene_t*)malloc(sizeof(scene_t));
    scene->background = vec4_from_vec3(background, 1);
    scene->models = models;
    scene->ambient_intensity = ambient_intensity;
    scene->punctual_intensity = punctual_intensity;
    return scene;
}

void scene_release(scene_t *scene) {
    int num_models = darray_size(scene->models);
    int i;
    for (i = 0; i < num_models; i++) {
        model_t *model = scene->models[i];
        model->release(model);
    }
    darray_free(scene->models);
    free(scene);
}
