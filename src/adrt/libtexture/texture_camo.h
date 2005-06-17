#ifndef _TEXTURE_CAMO_H
#define _TEXTURE_CAMO_H


#include "texture.h"
#include "texture_perlin.h"


typedef struct texture_camo_s {
  tfloat size;
  int octaves;
  int absolute;
  TIE_3 color1;
  TIE_3 color2;
  TIE_3 color3;
  texture_perlin_t perlin;
} texture_camo_t;


extern	void	texture_camo_init(texture_t *texture, tfloat size, int octaves, int absolute, TIE_3 color1, TIE_3 color2, TIE_3 color3);
extern	void	texture_camo_free(texture_t *texture);
extern	void	texture_camo_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
