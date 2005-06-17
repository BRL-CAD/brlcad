#ifndef _TEXTURE_IMAGE_H
#define _TEXTURE_IMAGE_H


#include "texture.h"


typedef struct texture_image_s {
  short	w;
  short	h;
  unsigned char *image;
} texture_image_t;


extern	void	texture_image_init(texture_t *texture, short w, short h, unsigned char *image);
extern	void	texture_image_free(texture_t *texture);
extern	void	texture_image_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


#endif
