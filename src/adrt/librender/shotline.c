#include "shotline.h"
#include "hit.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct shotline_s {
  common_mesh_t **mesh_list;
  TIE_3 in;
  TIE_3 out;
  int mesh_num;
} shotline_t;


static void* shot_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  common_triangle_t *t = ((common_triangle_t *)(tri->ptr));
  shotline_t *shotline;
  int i, match;

  shotline = (shotline_t *)ptr;

  match = 0;
  for(i = 0; i < shotline->mesh_num; i++)
    if(t->mesh == shotline->mesh_list[i])
      match = 1;

  if(!match) {
    shotline->mesh_list = (common_mesh_t **)realloc(shotline->mesh_list, sizeof(common_mesh_t *) * (shotline->mesh_num + 1));
    shotline->mesh_list[shotline->mesh_num] = t->mesh;
    if(!shotline->mesh_num)
      shotline->in = id->pos;
    shotline->out = id->pos;
    shotline->mesh_num++;
  }

  return(NULL);
}


void render_shotline_work(tie_t *tie, tie_ray_t *ray, void **data, int *dlen) {
  shotline_t shotline;
  tie_id_t id;
  int i, ind;
  char c;
 

  shotline.mesh_list = NULL;
  shotline.mesh_num = 0;

  shotline.in.v[0] = 0;
  shotline.in.v[1] = 0;
  shotline.in.v[2] = 0;

  shotline.out.v[0] = 0;
  shotline.out.v[1] = 0;
  shotline.out.v[2] = 0;


  tie_work(tie, ray, &id, shot_hit, &shotline);

  ind = 0;

  *data = (void *)realloc(*data, 6*sizeof(tfloat) + sizeof(int));

  /* pack in hit */
  memcpy(&((char *)*data)[ind], &shotline.in.v[0], sizeof(tfloat));
  ind += sizeof(tfloat);
  memcpy(&((char *)*data)[ind], &shotline.in.v[1], sizeof(tfloat));
  ind += sizeof(tfloat);
  memcpy(&((char *)*data)[ind], &shotline.in.v[2], sizeof(tfloat));
  ind += sizeof(tfloat);

  /* pack out hit */
  memcpy(&((char *)*data)[ind], &shotline.out.v[0], sizeof(tfloat));
  ind += sizeof(tfloat);
  memcpy(&((char *)*data)[ind], &shotline.out.v[1], sizeof(tfloat));
  ind += sizeof(tfloat);
  memcpy(&((char *)*data)[ind], &shotline.out.v[2], sizeof(tfloat));
  ind += sizeof(tfloat);

  memcpy(&((char *)*data)[ind], &shotline.mesh_num, sizeof(int));
  ind += sizeof(int);

  for(i = 0; i < shotline.mesh_num; i++) {
    c = strlen(shotline.mesh_list[i]->name) + 1;

    *data = realloc(*data, ind + c + 2); /* 1 for length, 1 for null char */

    /* length */
    memcpy(&((char *)*data)[ind], &c, 1);
    ind += 1;

    /* name */
    memcpy(&((char *)*data)[ind], shotline.mesh_list[i]->name, c);
    ind += c;

/*    printf("hit[%d]: -%s-\n", i, shotline.mesh_list[i]->name); */
  }

  *dlen = ind;
}
