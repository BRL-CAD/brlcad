/* Minimal FCStd ZIP archive interface for the local open2open copy.
 *
 * This intentionally implements only the archive operations used by
 * open2open/src/fcstd_convert.cpp, using zlib for ZIP deflate streams.
 */

#ifndef GCV_FREECAD_FCSTD_ZIP_H
#define GCV_FREECAD_FCSTD_ZIP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t zip_int64_t;
typedef uint64_t zip_uint64_t;

typedef struct zip zip_t;
typedef struct zip_file zip_file_t;
typedef struct zip_source zip_source_t;

typedef struct zip_stat {
    zip_uint64_t valid;
    zip_uint64_t size;
} zip_stat_t;

#define ZIP_RDONLY       0
#define ZIP_CREATE       1
#define ZIP_TRUNCATE     8
#define ZIP_FL_OVERWRITE 8192
#define ZIP_STAT_SIZE    0x0004u

zip_t *zip_open(const char *path, int flags, int *errorp);
int zip_close(zip_t *archive);
void zip_discard(zip_t *archive);

zip_file_t *zip_fopen(zip_t *archive, const char *fname, int flags);
zip_int64_t zip_fread(zip_file_t *file, void *buf, zip_uint64_t nbytes);
int zip_fclose(zip_file_t *file);

void zip_stat_init(zip_stat_t *st);
int zip_stat(zip_t *archive, const char *fname, int flags, zip_stat_t *st);

zip_source_t *zip_source_buffer(zip_t *archive, const void *data, zip_uint64_t len, int freep);
zip_int64_t zip_file_add(zip_t *archive, const char *name, zip_source_t *source, int flags);
void zip_source_free(zip_source_t *source);

#ifdef __cplusplus
}
#endif

#endif
