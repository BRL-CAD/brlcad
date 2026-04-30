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
#include <geogram/mesh/mesh_topology.h>
#include <geogram/mesh/mesh.h>
#include <geogram/basic/memory.h>
#include <stack>

namespace GEOBRL {

    index_t get_connected_components(
        const Mesh& M, vector<index_t>& component
    ) {
        index_t nb_components = 0;
        component.assign(M.facets.nb(), NO_INDEX);
        for(index_t f: M.facets) {
            if(component[f] == NO_INDEX) {
                std::stack<index_t> S;
                S.push(f);
                component[f] = nb_components;
                do {
                    index_t cur_f = S.top();
                    S.pop();
                    for(index_t adj_f: M.facets.adjacent(cur_f)) {
                        if(adj_f != NO_FACET && component[adj_f] == NO_INDEX) {
                            S.push(index_t(adj_f));
                            component[adj_f] = nb_components;
                        }
                    }
                } while(!S.empty());
                nb_components++;
            }
        }
        return nb_components;
    }

    index_t GEOBRLCAD_API get_connected_components(
        const Mesh& M, Attribute<index_t>& component
    ) {
        index_t nb_components = 0;
	for(index_t f: M.facets) {
	    component[f] = NO_INDEX;
	}
        for(index_t f: M.facets) {
            if(component[f] == NO_INDEX) {
                std::stack<index_t> S;
                S.push(f);
                component[f] = nb_components;
                do {
                    index_t cur_f = S.top();
                    S.pop();
                    for(index_t adj_f: M.facets.adjacent(cur_f)) {
                        if(adj_f != NO_FACET && component[adj_f] == NO_INDEX) {
                            S.push(index_t(adj_f));
                            component[adj_f] = nb_components;
                        }
                    }
                } while(!S.empty());
                nb_components++;
            }
        }
        return nb_components;

}
}
