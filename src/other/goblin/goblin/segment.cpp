
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#include "segment.h"


segmentGraph::segmentGraph(abstractMixedGraph* G) throw() :
    managedObject(G->Context()), subgraph(G)
{
    ContactNodeIndex = new TNode[G->N()];
    for (TNode v=0; v<G->N(); v++)
        ContactNodeIndex[v] = NoNode;
    ContactNodes = new TNode[G->N()];
    ContactNodeCount = 0;

    Regions = NULL;
    RegionsCount = 0;
}


segmentGraph::~segmentGraph() throw()
{
    delete[] ContactNodes;
    delete[] ContactNodeIndex;

    delete[] Regions;
}


void segmentGraph::MarkAsContactNode(TNode v) throw(ERRange,ERRejected)
{
    if (!HasNode(v)) throw ERRejected();

    if (!IsContactNode(v)) {
        ContactNodeIndex[v] = ContactNodeCount;
        ContactNodes[ContactNodeCount] = v;
        ContactNodeCount++;
    }
}


void segmentGraph::MarkAsContactArc(TArc a) throw(ERRange,ERRejected)
{
    TNode v = SourceGraph()->StartNode(a);

    if (!HasNode(v)) throw ERRejected();

    if (!IsContactNode(v)) {
        ContactNodeIndex[v] = ContactNodeCount;
        ContactNodes[ContactNodeCount] = v;
        ContactNodeCount++;
    }
}


void segmentGraph::ReleaseContactNode(TNode v) throw(ERRange,ERRejected)
{
    if (ContactNodeIndex[v] != NoNode) {
        ContactNodeCount--;
        for (TNode i=ContactNodeIndex[v]; i<ContactNodeCount; i++)
            ContactNodes[i] = ContactNodes[i+1];
        ContactNodeIndex[v] = NoNode;
    }
}


TNode segmentGraph::GetContactNodeCount() throw()
{
    return ContactNodeCount;
}


TNode segmentGraph::GetContactNode(TNode i) throw(ERRange)
{
    return ContactNodes[i];
}


bool segmentGraph::IsContactNode(TNode v) throw(ERRange)
{
    return (ContactNodeIndex[v] != NoNode);
}


void segmentGraph::AddRegion(int r) throw()
{
    RegionsCount++;

    if (Regions != NULL)
        Regions = (int*)GoblinRealloc(Regions,RegionsCount*sizeof(int));
    else
        Regions = new int[1];

    Regions[RegionsCount-1] = r;
}


void segmentGraph::OmitRegion(int r) throw()
{
    //suche Region r; sp�er besser mit bin�er Suche
    int i = 0;
    while ((i<RegionsCount) && (Regions[i]!=r)) i++;

    if (i<RegionsCount)
    {
        //schlie� Lcke von gel�chter Region
        RegionsCount--;
        for (; i<RegionsCount; i++)
            Regions[i] = Regions[i+1];
    }
    //else Region r sowieso nicht vorhanden
}


int segmentGraph::GetRegionsCount() throw()
{
    return RegionsCount;
}


int segmentGraph::GetRegion(int i) throw()
{
    return Regions[i];
}


bool segmentGraph::HasRegion(int r) throw()
{
    for (int i = 0; i<RegionsCount; i++)
        if (Regions[i] == r)
            return true;
    return false;
}
