//
// Copyright (C) 2004 Tanguy Fautr�.
// For conditions of distribution and use,
// see copyright notice in tri_stripper.h
//
//////////////////////////////////////////////////////////////////////
// SVN: $Id$
//////////////////////////////////////////////////////////////////////

#ifndef TRI_STRIPPER_HEADER_GUARD_CONNECTIVITY_GRAPH_H
#define TRI_STRIPPER_HEADER_GUARD_CONNECTIVITY_GRAPH_H

#include "public_types.h"

#include "graph_array.h"
#include "types.h"




namespace triangle_stripper
{

    namespace detail
    {

        void make_connectivity_graph(graph_array<triangle> & Triangles, const indices & Indices);

    }

}




#endif // TRI_STRIPPER_HEADER_GUARD_CONNECTIVITY_GRAPH_H
