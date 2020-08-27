#include "../api.h"

scene_t *blinn_lighthouse_scene(void) {
    mat4_t translation = mat4_translate(-78.203f, -222.929f, 16.181f);
    mat4_t rotation = mat4_rotate_y(TO_RADIANS(-135));
    mat4_t scale = mat4_scale(0.0016f, 0.0016f, 0.0016f);
    mat4_t root = mat4_mul_mat4(scale, mat4_mul_mat4(rotation, translation));
    return scene_from_file("lighthouse/lighthouse.scn", root);
}

