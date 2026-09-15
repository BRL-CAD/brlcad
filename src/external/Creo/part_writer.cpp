#include "common.h"

#include "part_writer.h"

#include "wdb.h"


extern "C" int
creo_brl_write_bot(struct rt_wdb *writer,
                   const char *name,
                   int write_normals,
                   size_t vertex_count,
                   size_t face_count,
                   fastf_t *vertices,
                   int *faces,
                   size_t normal_count,
                   fastf_t *normals,
                   int *face_normals)
{
    if (write_normals) {
        const unsigned char normal_flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS;

        return mk_bot_w_normals(writer, name, RT_BOT_SOLID, RT_BOT_CCW, normal_flags,
                                vertex_count, face_count, vertices, faces,
                                NULL, NULL, normal_count, normals, face_normals);
    }

    return mk_bot(writer, name, RT_BOT_SOLID, RT_BOT_CCW, 0,
                  vertex_count, face_count, vertices, faces, NULL, NULL);
}
