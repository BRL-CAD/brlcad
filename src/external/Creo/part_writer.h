#ifndef CREO_PART_WRITER_H
#define CREO_PART_WRITER_H

#include <stddef.h>

#include "raytrace.h"

extern "C" int creo_brl_write_bot(struct rt_wdb *writer,
                                   const char *name,
                                   int write_normals,
                                   size_t vertex_count,
                                   size_t face_count,
                                   fastf_t *vertices,
                                   int *faces,
                                   size_t normal_count,
                                   fastf_t *normals,
                                   int *face_normals);

#endif /* CREO_PART_WRITER_H */
