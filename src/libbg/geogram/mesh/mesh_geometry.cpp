/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

#include "common.h"
#include <geogram/mesh/mesh_geometry.h>
#include <geogram/basic/attributes.h>
#include <geogram/basic/geometry.h>

/****************************************************************************/

namespace GEOBRL {

    namespace Geom {

        vec3 mesh_facet_normal(const Mesh& M, index_t f) {
            vec3 result{0.0, 0.0, 0.0};
	    for(const auto& [ng1, ng2, ng3]: M.facets.triangle_points(f)) {
		const vec3& p1=as_gte<3>(ng1); const vec3& p2=as_gte<3>(ng2); const vec3& p3=as_gte<3>(ng3);
		result += cross(p2 - p1, p3 - p1);
	    }
            return result;
        }

        double mesh_area(const Mesh& M, index_t dim) {
            double result = 0.0;
            for(index_t f: M.facets) {
                result += mesh_facet_area(M, f, dim);
            }
            return result;
        }

    }

    void compute_normals(Mesh& M) {
        if(M.vertices.dimension() < 6) {
            M.vertices.set_dimension(6);
        } else {
            for(index_t i: M.vertices) {
                Geom::mesh_vertex_normal_ref(M, i) = vec3{0.0, 0.0, 0.0};
            }
        }
        for(index_t f: M.facets) {
            vec3 N = Geom::mesh_facet_normal(M, f);
            for(index_t corner: M.facets.corners(f)) {
                index_t v = M.facet_corners.vertex(corner);
                Geom::mesh_vertex_normal_ref(M, v) += N;
            }
        }
        for(index_t i: M.vertices) {
            Geom::mesh_vertex_normal_ref(M, i) = normalize(
                Geom::mesh_vertex_normal(M, i)
            );
        }
    }

    void get_bbox(const Mesh& M, double* xyzmin, double* xyzmax) {
        geo_assert(M.vertices.dimension() >= 3);
        for(index_t c = 0; c < 3; c++) {
            xyzmin[c] = Numeric::max_float64();
            xyzmax[c] = Numeric::min_float64();
        }
        for(const auto& p: M.vertices.points()) {
            for(index_t c = 0; c < 3; c++) {
                xyzmin[c] = std::min(xyzmin[c], p[c]);
                xyzmax[c] = std::max(xyzmax[c], p[c]);
            }
        }
    }

    double bbox_diagonal(const Mesh& M) {
        geo_assert(M.vertices.dimension() >= 3);
        double xyzmin[3];
        double xyzmax[3];
        get_bbox(M, xyzmin, xyzmax);
        return ::sqrt(
            geo_sqr(xyzmax[0] - xyzmin[0]) +
            geo_sqr(xyzmax[1] - xyzmin[1]) +
            geo_sqr(xyzmax[2] - xyzmin[2])
        );
    }

    void set_anisotropy(Mesh& M, double s) {
        if(M.vertices.dimension() < 6) {
            compute_normals(M);
        }
        if(s == 0.0) {
            unset_anisotropy(M);
            return;
        }
        s *= bbox_diagonal(M);
        for(index_t i: M.vertices) {
            Geom::mesh_vertex_normal_ref(M, i) =
                s * normalize(Geom::mesh_vertex_normal(M, i));
        }
    }

    void unset_anisotropy(Mesh& M) {
        if(M.vertices.dimension() < 6) {
            return;
        }
        for(index_t i: M.vertices) {
            Geom::mesh_vertex_normal_ref(M, i) = normalize(
                Geom::mesh_vertex_normal(M, i)
            );
        }
    }

    double surface_average_edge_length(const Mesh& M) {
        double result = 0.0;
        index_t count = 0;
        for(index_t f: M.facets) {
            for(index_t c1: M.facets.corners(f)) {
                index_t c2 = M.facets.next_corner_around_facet(f,c1);
                index_t v1 = M.facet_corners.vertex(c1);
                index_t v2 = M.facet_corners.vertex(c2);
                result += Geom::distance(
                    M.vertices.point_ptr(v1),
                    M.vertices.point_ptr(v2),
                    coord_index_t(M.vertices.dimension())
                );
                ++count;
            }
        }
        if(count != 0) {
            result /= double(count);
        }
        return result;
    }
}
