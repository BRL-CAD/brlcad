#ifndef _TEXTURE_MIX_H
#define _TEXTURE_MIX_H


#include "texture.h"


typedef struct texture_mix_s {
  texture_t *texture1;
  texture_t *texture2;
  tfloat coef;
} texture_mix_t;


extern	void	texture_mix_init(texture_t *shader, texture_t *shader1, texture_t *shader2, tfloat coef);
extern	void	texture_mix_free(texture_t *shader);
extern	void	texture_mix_work(texture_t *shader, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
