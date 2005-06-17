#ifndef _TEXTURE_BUMP_H
#define _TEXTURE_BUMP_H


#include "texture.h"


typedef struct texture_bump_s {
  TIE_3 coef;
} texture_bump_t;


extern	void	texture_bump_init(texture_t *texture, TIE_3 rgb);
extern	void	texture_bump_free(texture_t *texture);
extern	void	texture_bump_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
