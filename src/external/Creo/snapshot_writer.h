/**
 *            S N A P S H O T _ W R I T E R . H
 * BRL-CAD
 */

#ifndef CREO_SNAPSHOT_WRITER_H
#define CREO_SNAPSHOT_WRITER_H

enum creo_brl_snapshot_capture_result {
    CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS = 0,
    CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST = -1,
    CREO_BRL_SNAPSHOT_CAPTURE_DIALOG_FAILURE = -2,
    CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE = -3,
    CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE = -4
};

extern "C" int creo_brl_frontend_capture_single_part_snapshot(const char *snapshot_path);

#endif /* CREO_SNAPSHOT_WRITER_H */
