#include "texture_stack.h"
#include <stdlib.h>


void texture_stack_init(texture_t *texture);
void texture_stack_free(texture_t *texture);
void texture_stack_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);
void texture_stack_push(texture_t *texture, texture_t *texture_new);


void texture_stack_init(texture_t *texture) {
  texture_stack_t *td;

  texture->data = malloc(sizeof(texture_stack_t));
  texture->free = texture_stack_free;
  texture->work = texture_stack_work;

  td = (texture_stack_t *)texture->data;
  td->num = 0;
  td->list = NULL;
}


void texture_stack_free(texture_t *texture) {
  texture_stack_t *td;

  td = (texture_stack_t *)texture->data;
  free(td->list);
  free(texture->data);
}


void texture_stack_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_stack_t *td;
  int i;

  td = (texture_stack_t *)texture->data;

  for(i = td->num-1; i >= 0; i--)
    td->list[i]->work(td->list[i], mesh, ray, id, pixel);
}


void texture_stack_push(texture_t *texture, texture_t *texture_new) {
  texture_stack_t *td;

  td = (texture_stack_t *)texture->data;

  td->list = (texture_t **)realloc(td->list, sizeof(texture_t *)*(td->num+1));
  td->list[td->num++] = texture_new;  
}
