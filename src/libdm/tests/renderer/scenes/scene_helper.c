#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../api.h"
#include "blinn_shader.h"

typedef struct {
    vec3_t background;
    char environment[LINE_SIZE];
    char skybox[LINE_SIZE];
    char shadow[LINE_SIZE];
    float ambient;
    float punctual;
} scene_light_t;

typedef struct {
    int index;
    vec4_t basecolor;
    float shininess;
    char diffuse_map[LINE_SIZE];
    char specular_map[LINE_SIZE];
    char emission_map[LINE_SIZE];
    char double_sided[LINE_SIZE];
    char enable_blend[LINE_SIZE];
    float alpha_cutoff;
} scene_blinn_t;

typedef struct {
    int index;
    mat4_t matrix;
} scene_transform_t;

typedef struct {
    int index;
    char mesh[LINE_SIZE];
    char skeleton[LINE_SIZE];
    int attached;
    int material;
    int transform;
} scene_model_t;

static int equals_to(const char *str1, const char *str2) {
    return strcmp(str1, str2) == 0;
}

static const char *wrap_path(const char *path) {
    if (equals_to(path, "null")) {
        return NULL;
    } else {
        return path;
    }
}

static int wrap_knob(const char *knob) {
    if (equals_to(knob, "on")) {
        return 1;
    } else {
        assert(equals_to(knob, "off"));
        return 0;
    }
}

static scene_light_t read_light(FILE *file) {
    scene_light_t light;
    char header[LINE_SIZE];
    int items;

    items = fscanf(file, " %s", header);
    assert(equals_to(header, "lighting:"));
    items = fscanf(file, " background: %f %f %f",
                   &light.background.x,
                   &light.background.y,
                   &light.background.z);
    assert(items == 3);
    items = fscanf(file, " environment: %s", light.environment);
    assert(items == 1);
    items = fscanf(file, " skybox: %s", light.skybox);
    assert(items == 1);
    items = fscanf(file, " shadow: %s", light.shadow);
    assert(items == 1);
    items = fscanf(file, " ambient: %f", &light.ambient);
    assert(items == 1);
    items = fscanf(file, " punctual: %f", &light.punctual);
    assert(items == 1);

    UNUSED_VAR(items);
    return light;
}

static scene_blinn_t read_blinn_material(FILE *file) {
    scene_blinn_t material;
    int items;

    items = fscanf(file, " material %d:", &material.index);
    assert(items == 1);
    items = fscanf(file, " basecolor: %f %f %f %f",
                   &material.basecolor.x,
                   &material.basecolor.y,
                   &material.basecolor.z,
                   &material.basecolor.w);
    assert(items == 4);
    items = fscanf(file, " shininess: %f", &material.shininess);
    assert(items == 1);
    items = fscanf(file, " diffuse_map: %s", material.diffuse_map);
    assert(items == 1);
    items = fscanf(file, " specular_map: %s", material.specular_map);
    assert(items == 1);
    items = fscanf(file, " emission_map: %s", material.emission_map);
    assert(items == 1);
    items = fscanf(file, " double_sided: %s", material.double_sided);
    assert(items == 1);
    items = fscanf(file, " enable_blend: %s", material.enable_blend);
    assert(items == 1);
    items = fscanf(file, " alpha_cutoff: %f", &material.alpha_cutoff);
    assert(items == 1);

    UNUSED_VAR(items);
    return material;
}

static scene_blinn_t *read_blinn_materials(FILE *file) {
    scene_blinn_t *materials = NULL;
    int num_materials;
    int items;
    int i;

    items = fscanf(file, " materials %d:", &num_materials);
    assert(items == 1);
    UNUSED_VAR(items);
    for (i = 0; i < num_materials; i++) {
        scene_blinn_t material = read_blinn_material(file);
        assert(material.index == i);
        darray_push(materials, material, scene_blinn_t);
    }
    return materials;
}


static scene_transform_t read_transform(FILE *file) {
    scene_transform_t transform;
    int items;
    int i;

    items = fscanf(file, " transform %d:", &transform.index);
    assert(items == 1);
    for (i = 0; i < 4; i++) {
        items = fscanf(file, " %f %f %f %f",
                       &transform.matrix.m[i][0],
                       &transform.matrix.m[i][1],
                       &transform.matrix.m[i][2],
                       &transform.matrix.m[i][3]);
        assert(items == 4);
    }

    UNUSED_VAR(items);
    return transform;
}

static scene_transform_t *read_transforms(FILE *file) {
    scene_transform_t *transforms = NULL;
    int num_transforms;
    int items;
    int i;

    items = fscanf(file, " transforms %d:", &num_transforms);
    assert(items == 1);
    UNUSED_VAR(items);
    for (i = 0; i < num_transforms; i++) {
        scene_transform_t transform = read_transform(file);
        assert(transform.index == i);
        darray_push(transforms, transform, scene_transform_t);
    }
    return transforms;
}

static scene_model_t read_model(FILE *file) {
    scene_model_t model;
    int items;

    items = fscanf(file, " model %d:", &model.index);
    assert(items == 1);
    items = fscanf(file, " mesh: %s", model.mesh);
    assert(items == 1);
    items = fscanf(file, " skeleton: %s", model.skeleton);
    assert(items == 1);
    items = fscanf(file, " attached: %d", &model.attached);
    assert(items == 1);
    items = fscanf(file, " material: %d", &model.material);
    assert(items == 1);
    items = fscanf(file, " transform: %d", &model.transform);
    assert(items == 1);

    UNUSED_VAR(items);
    return model;
}

static scene_model_t *read_models(FILE *file) {
    scene_model_t *models = NULL;
    int num_models;
    int items;
    int i;

    items = fscanf(file, " models %d:", &num_models);
    assert(items == 1);
    UNUSED_VAR(items);
    for (i = 0; i < num_models; i++) {
        scene_model_t model = read_model(file);
        assert(model.index == i);
        darray_push(models, model, scene_model_t);
    }
    return models;
}

static scene_t *create_scene(scene_light_t *light, model_t **models) {
    int shadow_width;
    int shadow_height;

    if (equals_to(light->shadow, "off")) {
        shadow_width = -1;
        shadow_height = -1;
    } else {
        if (equals_to(light->shadow, "on")) {
            shadow_width = 512;
            shadow_height = 512;
        } else {
            int items;
            items = sscanf(light->shadow, "%dx%d",
                           &shadow_width, &shadow_height);
            assert(items == 2 && shadow_width > 0 && shadow_height > 0);
            UNUSED_VAR(items);
        }
    }

    return scene_create(light->background, NULL, models,
                        light->ambient, light->punctual,
                        shadow_width, shadow_height);
}

static scene_t *create_blinn_scene(scene_light_t *scene_light,
                                   scene_blinn_t *scene_materials,
                                   scene_transform_t *scene_transforms,
                                   scene_model_t *scene_models,
                                   mat4_t root_transform) {
    int num_materials = darray_size(scene_materials);
    int num_transforms = darray_size(scene_transforms);
    int num_models = darray_size(scene_models);
    model_t **models = NULL;
    int i;

    for (i = 0; i < num_models; i++) {
        scene_blinn_t scene_material;
        scene_transform_t scene_transform;
        scene_model_t scene_model;
        const char *mesh;
        const char *skeleton;
        int attached;
        mat4_t transform;
        blinn_material_t material;
        model_t *model;

        scene_model = scene_models[i];
        assert(scene_model.transform < num_transforms);
        assert(scene_model.material < num_materials);
        UNUSED_VAR(num_transforms);
        UNUSED_VAR(num_materials);

        mesh = wrap_path(scene_model.mesh);
        skeleton = wrap_path(scene_model.skeleton);
        attached = scene_model.attached;

        scene_transform = scene_transforms[scene_model.transform];
        transform = mat4_mul_mat4(root_transform, scene_transform.matrix);

        scene_material = scene_materials[scene_model.material];
        material.basecolor = scene_material.basecolor;
        material.shininess = scene_material.shininess;
        material.diffuse_map = wrap_path(scene_material.diffuse_map);
        material.specular_map = wrap_path(scene_material.specular_map);
        material.emission_map = wrap_path(scene_material.emission_map);
        material.double_sided = wrap_knob(scene_material.double_sided);
        material.enable_blend = wrap_knob(scene_material.enable_blend);
        material.alpha_cutoff = scene_material.alpha_cutoff;

        model = blinn_create_model(mesh, transform, skeleton, attached,
                                   &material);
        darray_push(models, model, model_t *);
    }

    return create_scene(scene_light, models);
}

scene_t *scene_from_file(const char *filename, mat4_t root) {
    char scene_type[LINE_SIZE];
    scene_t *scene;
    FILE *file;
    int items;

    file = fopen(filename, "rb");
    assert(file != NULL);
    items = fscanf(file, " type: %s", scene_type);
    assert(items == 1);
    UNUSED_VAR(items);
    if (equals_to(scene_type, "blinn")) {
        scene_light_t light = read_light(file);
        scene_blinn_t *materials = read_blinn_materials(file);
        scene_transform_t *transforms = read_transforms(file);
        scene_model_t *models = read_models(file);
        scene = create_blinn_scene(&light, materials, transforms, models, root);
        darray_free(materials);
        darray_free(transforms);
        darray_free(models);
    } else {
        assert(0);
        scene = NULL;
    }
    fclose(file);

    return scene;
}
