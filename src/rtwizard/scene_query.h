/*                  S C E N E _ Q U E R Y . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_SCENE_QUERY_H
#define RTWIZARD_SCENE_QUERY_H

#include <string>
#include <utility>
#include <vector>

#include "vmath.h"

struct SceneBounds {
    point_t minimum;
    point_t maximum;
};

class SceneQuery {
public:
    explicit SceneQuery(const std::string &database);
    ~SceneQuery();

    SceneQuery(const SceneQuery &) = delete;
    SceneQuery &operator=(const SceneQuery &) = delete;

    SceneBounds bounds(const std::vector<std::string> &objects) const;
    std::pair<double, double> projected_bounds(const std::vector<std::string> &objects,
        const std::vector<double> &direction) const;

private:
    SceneBounds transformed_bounds(const std::vector<std::string> &objects, const mat_t transform) const;
    struct db_i *database_ = nullptr;
};

#endif /* RTWIZARD_SCENE_QUERY_H */

