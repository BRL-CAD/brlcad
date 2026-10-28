#ifndef CREO_CONVERSION_SCENE_CONVERT_H
#define CREO_CONVERSION_SCENE_CONVERT_H

#include <stdio.h>

#include "shared/conversion_snapshot.h"


int creo_brl_convert_snapshot_scene(FILE *snapshot_file,
                                    const struct creo_brl_snapshot_scene *scene);


#endif /* CREO_CONVERSION_SCENE_CONVERT_H */
