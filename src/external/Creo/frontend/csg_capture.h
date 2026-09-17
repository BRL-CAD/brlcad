/**
 * Phase 5 frontend-side CSG fact capture helpers.
 */

#ifndef CREO_FRONTEND_CSG_CAPTURE_H
#define CREO_FRONTEND_CSG_CAPTURE_H

#include <vector>

#include "creo-brl.h"
#include "conversion_snapshot.h"


struct snapshot_part_capture;


int creo_brl_capture_feature_suppression(ProMdl model,
                                         double scale,
                                         const struct creo_brl_snapshot_settings *settings,
                                         struct snapshot_part_capture *capture,
                                         std::vector<int> *suppressed_features,
                                         int *suppression_applied);

void creo_brl_resume_suppressed_features(ProMdl model,
                                         const std::vector<int>& feature_ids);


#endif /* CREO_FRONTEND_CSG_CAPTURE_H */
