#ifndef TEXTURE_CHECKER_H
#define TEXTURE_CHECKER_H


#include "texture.h"


typedef struct texture_checker_s {
  int tile;
} texture_checker_t;


extern  void    texture_checker_init(texture_t *texture, int checker);
extern  void    texture_checker_free(texture_t *texture);
extern  void    texture_checker_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
