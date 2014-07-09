#include "solidity.h"


#include <map>
#include <set>




typedef std::pair<std::size_t, std::size_t> Edge;


static inline bool check_vertices(
    std::vector<std::set<Edge> > &vertex_edges,
    std::map<Edge, int> &edge_incidence_map,
    std::size_t va, std::size_t vb)
{
    const Edge edge(std::min(va, vb), std::max(va, vb));
    vertex_edges[va].insert(edge);
    vertex_edges[vb].insert(edge);

    return ++edge_incidence_map[edge] <= 2;
}




int bot_is_closed(const rt_bot_internal *bot)
{
    std::map<Edge, int> edge_incidence_map;
    std::vector<std::set<Edge> > vertex_edges(bot->num_vertices);

    for (std::size_t fi = 0; fi < bot->num_faces; ++fi) {
	const std::size_t v1 = bot->faces[fi * 3];
	const std::size_t v2 = bot->faces[fi * 3 + 1];
	const std::size_t v3 = bot->faces[fi * 3 + 2];

	bool valid = check_vertices(vertex_edges, edge_incidence_map, v1, v2)
		     && check_vertices(vertex_edges, edge_incidence_map, v1, v3)
		     && check_vertices(vertex_edges, edge_incidence_map, v2, v3);

	if (!valid) return false;
    }

    for (std::vector<std::set<Edge> >::const_iterator
	 vertex_it = vertex_edges.begin();
	 vertex_it != vertex_edges.end(); ++vertex_it) {

	if (vertex_it->empty()) return false;
    }

    for (std::map<Edge, int>::const_iterator it = edge_incidence_map.begin();
	 it != edge_incidence_map.end(); ++it)
	if (it->second != 2) return false;

    return true;
}




/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
