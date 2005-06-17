#ifndef _TEXTURE_GRADIENT_H
#define _TEXTURE_GRADIENT_H


#include "texture.h"


typedef struct texture_gradient_s {
  int		axis;
} texture_gradient_t;


extern	void	texture_gradient_init(texture_t *shader, int axis);
extern	void	texture_gradient_free(texture_t *shader);
extern	void	texture_gradient_work(texture_t *shader, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
