#ifndef _COMMON_UNPACK_H
#define _COMMON_UNPACK_H


#include "camera.h"
#include "cdb.h"
#include "env.h"
#include "texture_includes.h"

/*** ONLY TEMPORARY ***/
#define COMMON_PACK_ALL                   0x0001

#define COMMON_PACK_CAMERA                0x0100
#define COMMON_PACK_ENV                   0x0101
#define COMMON_PACK_TEXTURE               0x0102
#define COMMON_PACK_MESH                  0x0103
#define COMMON_PACK_PROP                  0x0104


#define COMMON_PACK_ENV_RM                0x0300
#define COMMON_PACK_ENV_IMAGESIZE         0x0301

#define COMMON_PACK_MESH_NEW              0x0400
/**********************/

typedef struct common_unpack_prop_node_s {
  char 			name[256];
  common_prop_t		prop;
} common_unpack_prop_node_t;


typedef struct common_unpack_texture_node_s {
  char			name[256];
  texture_t		*texture;
} common_unpack_texture_node_t;


typedef struct common_unpack_mesh_node_s {
  char			name[256];
  common_mesh_t		mesh;
} common_unpack_mesh_node_t;


extern	void	common_unpack(common_db_t *db, tie_t *tie, util_camera_t *camera, int mask, void *app_data, int app_size);
extern	void	common_unpack_free();

#endif
