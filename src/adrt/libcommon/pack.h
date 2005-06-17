#ifndef _COMMON_PACK_H
#define _COMMON_PACK_H

#include "cdb.h"

#define	COMMON_PACK_ALL			0x0001

#define COMMON_PACK_CAMERA		0x0100
#define	COMMON_PACK_ENV			0x0101
#define	COMMON_PACK_TEXTURE		0x0102
#define	COMMON_PACK_MESH		0x0103
#define	COMMON_PACK_PROP		0x0104


#define COMMON_PACK_ENV_RM		0x0300
#define COMMON_PACK_ENV_IMAGESIZE	0x0301

#define COMMON_PACK_MESH_NEW		0x0400


extern	int	common_pack(common_db_t *db, void **app_data, char *proj, char *geom_file, char *args);
extern	void	common_pack_write(void **dest, int *ind, void *src, int size);

extern	int	common_pack_trinum;

#endif
