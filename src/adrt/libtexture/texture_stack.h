#ifndef _TEXTURE_STACK_H
#define _TEXTURE_STACK_H


#include "texture.h"


typedef struct texture_stack_s {
  int			num;
  texture_t		**list;
} texture_stack_t;


extern	void	texture_stack_init(texture_t *texture);
extern	void	texture_stack_free(texture_t *texture);
extern	void	texture_stack_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);
extern	void	texture_stack_push(texture_t *texture, texture_t *texture_new);


#endif
