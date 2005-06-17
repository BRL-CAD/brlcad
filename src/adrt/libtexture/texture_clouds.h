#ifndef _TEXTURE_CLOUDS_H
#define _TEXTURE_CLOUDS_H


#include "texture.h"
#include "texture_perlin.h"


typedef struct texture_clouds_s {
  tfloat size;
  int octaves;
  int absolute;
  TIE_3	scale;
  TIE_3 translate;
  texture_perlin_t perlin;
} texture_clouds_t;


extern	void	texture_clouds_init(texture_t *texture, tfloat size, int octaves, int absolute, TIE_3 scale, TIE_3 translate);
extern	void	texture_clouds_free(texture_t *texture);
extern	void	texture_clouds_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
