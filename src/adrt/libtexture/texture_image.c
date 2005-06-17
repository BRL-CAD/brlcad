#include "texture_image.h"
#include <stdlib.h>
#include "umath.h"


void texture_image_init(texture_t *texture, short w, short h, unsigned char *image);
void texture_image_free(texture_t *texture);
void texture_image_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_image_init(texture_t *texture, short w, short h, unsigned char *image) {
  texture_image_t *td;

  texture->data = malloc(sizeof(texture_image_t));
  texture->free = texture_image_free;
  texture->work = texture_image_work;

  td = (texture_image_t *)texture->data;
  td->w = w;
  td->h = h;
  td->image = (unsigned char *)malloc(3*w*h);
  memcpy(td->image, image, 3*w*h);
}


void texture_image_free(texture_t *texture) {
  texture_image_t *td;

  td = (texture_image_t *)texture->data;
  free(td->image);
  free(texture->data);
}


void texture_image_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_image_t *td;
  TIE_3 pt;
  tfloat u, v;
  int ind;


  td = (texture_image_t *)texture->data;


  /* Transform the Point */
  math_vec_transform(pt, id->pos, mesh->matinv);
  u = mesh->max.v[0] - mesh->min.v[0] > TIE_PREC ? (pt.v[0] - mesh->min.v[0]) / (mesh->max.v[0] - mesh->min.v[0]) : 0.0;
  v = mesh->max.v[1] - mesh->min.v[1] > TIE_PREC ? (pt.v[1] - mesh->min.v[1]) / (mesh->max.v[1] - mesh->min.v[1]) : 0.0;

  ind = 3*((int)((1.0 - v)*td->h)*td->w + (int)(u*td->w));

  pixel->v[0] = td->image[ind+2] / 255.0;
  pixel->v[1] = td->image[ind+1] / 255.0;
  pixel->v[2] = td->image[ind+0] / 255.0;
}
