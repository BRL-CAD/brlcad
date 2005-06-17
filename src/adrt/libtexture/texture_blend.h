#ifndef _TEXTURE_BLEND_H
#define _TEXTURE_BLEND_H


#include "texture.h"


typedef struct texture_blend_s {
  TIE_3 color1;
  TIE_3 color2;
} texture_blend_t;


extern  void    texture_blend_init(texture_t *shader, TIE_3 color1, TIE_3 color2);
extern  void    texture_blend_free(texture_t *shader);
extern  void    texture_blend_work(texture_t *shader, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
