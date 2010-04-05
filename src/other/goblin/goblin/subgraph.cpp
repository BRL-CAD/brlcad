
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#include "subgraph.h"
#include "complement.h"


subgraph::subgraph(const subgraph* G) throw() :
    managedObject(G->S->Context())
{
    S = G->S;
    Complement = NULL;

    Nodes = new TNode[S->N()];
    for (TNode v=0; v<S->N(); v++)
        Nodes[v] = G->Nodes[v];

    Arcs = new TArc[S->M()*2];
    for (TArc a=0; a<S->M()*2; a++)
        Arcs[a] = G->Arcs[a];

    Nodes_on_subgraph = G->Nodes_on_subgraph;
    Arcs_on_subgraph = G->Arcs_on_subgraph;
}


subgraph::subgraph(abstractMixedGraph* G) throw() :
    managedObject(G->Context())
{
    S = G;
    Complement = NULL;

    Nodes = new TNode[S->N()];
    for (TNode v=0; v<S->N(); v++)
        Nodes[v] = NoNode;

    Arcs = new TArc[S->M()*2];
    for (TArc a=0; a<S->M()*2; a++)
        Arcs[a] = NoArc;

    Nodes_on_subgraph = 0;
    Arcs_on_subgraph = 0;
}

subgraph::~subgraph() throw()
{
    S = NULL;   //do not delete S
    if (Complement != NULL) delete Complement;

    delete[] Nodes;
    delete[] Arcs;
}


unsigned long subgraph::Size() const throw()
{
    return
          sizeof(subgraph)
        + managedObject::Allocated()
        + abstractSubgraph::Allocated()
        + subgraph::Allocated();
}


unsigned long subgraph::Allocated() const throw()
{
    return
          sizeof(TNode)*(S->N())
        + sizeof(TArc)*(S->M());
}


const abstractMixedGraph* subgraph::SourceGraph() const throw()
{
    return S;
}


const abstractSubgraph* subgraph::ComplementarySubgraph() const throw()
{
    if (Complement == NULL)
        Complement = new complementarySubgraph(this);
    return Complement;
}


bool subgraph::HasNode(TNode node) const throw(ERRange)
{
    return (Nodes[node] != NoNode);
}


void subgraph::AddArc(TArc a) throw(ERRange,ERRejected)
{
    AddNode(StartNode(a));
    AddNode(EndNode(a));

    if (Arcs[a] == NoArc)
    {
        Arcs[a] = 1;
        Arcs_on_subgraph++;

        // Add backward arc also
        a = S->Reverse(a);
        Arcs[a] = 1;
    }
}


TArc subgraph::AddArc(TNode u,TNode v) throw(ERRange,ERRejected)
{
    TArc a = S->Adjacency(u,v);
    //ueberprfen, ob a in S existiert - wenn ja dann
    AddArc(a);
    return a;
}


void subgraph::AddNode(TNode u) throw(ERRange,ERRejected)
{
    if (!HasNode(u))
    {
        Nodes[u] = 1;
        Nodes_on_subgraph++;
    }
}


void subgraph::OmitArc(TArc a) throw(ERRange,ERRejected)
{
    if (Arcs[a] != NoArc)
    {
        Arcs[a] = NoArc;
        a = S->Reverse(a);
        Arcs[a] = NoArc;
        Arcs_on_subgraph--;
    }
}


void subgraph::OmitArc(TNode u,TNode v) throw(ERRange,ERRejected)
{
    OmitArc(S->Adjacency(u,v));
}


void subgraph::OmitNode(TNode u) throw(ERRange,ERRejected)
{
    if (Nodes[u] != NoNode)
    {
        //momentan muessen alle Kanten durchlaufen werden!
        for (TArc a=0; a<S->M()*2; a++)
            if (EndNode(a) == u)
                OmitArc(a);

        Nodes[u] = NoNode;
        Nodes_on_subgraph--;
    }
}


void subgraph::OmitIsolatedNodes() throw()
{
    for (TNode v=0; v<S->N(); v++)
    {
        try
        {
            First(v);
        }
        catch (...)
        {
            Nodes[v] = NoNode;
            Nodes_on_subgraph--;
        }
    }
}


TArc subgraph::Adjacency(TNode from,TNode to) throw(ERRange)
{// hier ein Provisorium:
//S.Adjacency();
    cout << " NO ADJACENCY " << endl;
    throw ERRejected();

    TArc a,a0;
    a = a0 = First(from);

    if (EndNode(a) == to)
        return a;
    else
    {
        do
        {
            a = Right(a,from);
        } while ((EndNode(a) != to) && (a != a0));
        if (a != a0)
            return a;
        else
            throw ERRange();
    }
}


TNode subgraph::N() const throw()
{
    return Nodes_on_subgraph;
}


TArc subgraph::M() const throw()
{
    return Arcs_on_subgraph;
}


TNode subgraph::StartNode(TArc a) const throw(ERRange)
{
    return S->StartNode(a);
}


TNode subgraph::EndNode(TArc a) const throw(ERRange)
{
    return S->EndNode(a);
}


bool subgraph::HasArc(TArc a) const throw(ERRange)
{
    return (Arcs[a] != NoArc);
}


TArc subgraph::First(TNode v) const throw(ERRange,ERRejected)
{
    TArc a = S->First(v);
    if (Arcs[a] != NoArc) return a;

    TArc a0 = a;

    do
    {
        a = S->Right(a,v);
    }
    while ((Arcs[a] == NoArc) && (a != a0));

    if (a != a0) return a;
    else throw ERRange(); //was soll hier getan werden?
}


TArc subgraph::Right(TArc a, TNode v) const throw(ERRange,ERRejected)
{
    TArc b = S->Right(a,v);
    if (Arcs[b] != NoArc) return b;

    TArc b0 = b;

    do
    {
        b = S->Right(b,v);
    }
    while ((Arcs[b] == NoArc) && (b != b0));

    if (b != b0) return b;
    else throw ERRange(); //was soll hier getan werden?
}
