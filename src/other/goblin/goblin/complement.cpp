
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#include "complement.h"


complementarySubgraph::complementarySubgraph(const subgraph* G) throw() :
    managedObject(G->Context())
{   //Referenzen einrichten
    Complement = G;
    S = G->SourceGraph();
    WithoutNodes = G->Nodes;
    //Kantenmenge erstellen
    Arcs = new TArc[S->M()*2];
    ArcsCount = 0;

    for (TArc a=0; a<S->M()*2; a++)
    {
        if (this->HasNode(StartNode(a)) && this->HasNode(EndNode(a)))
        {
            Arcs[a] = 2;
            ArcsCount++;
        }
        else Arcs[a] = NoArc;
    }
}


complementarySubgraph::~complementarySubgraph() throw()
{
    delete[] Arcs;
}


unsigned long complementarySubgraph::Size() const throw()
{
    return
          sizeof(subgraph)
        + managedObject::Allocated()
        + abstractSubgraph::Allocated()
        + complementarySubgraph::Allocated();
}


unsigned long complementarySubgraph::Allocated() const throw()
{
    return sizeof(TArc)*2*(S->M());
}


const abstractMixedGraph* complementarySubgraph::SourceGraph() const throw()
{
    return S;
}


const abstractSubgraph* complementarySubgraph::ComplementarySubgraph() const throw()
{
    return Complement;
}


bool complementarySubgraph::HasNode(TNode node) const throw(ERRange)
{
    return (WithoutNodes[node] == NoNode);
}


TArc complementarySubgraph::Adjacency(TNode from,TNode to) throw(ERRange)
{// hier ein Provisorium:
    throw ERRejected();

    TArc a = First(from);

    if (EndNode(a) == to)
        return a;
    else
    {
        TArc a0 = a;

        do
        {
            a = Right(a,from);
        }
        while ((EndNode(a) != to) && (a != a0));

        if (a != a0) return a;
        else throw ERRange();
    }
}


TNode complementarySubgraph::N() const throw()
{
    return S->N() - Complement->N();
}


TArc complementarySubgraph::M() const throw()
{
    return ArcsCount;
}


TNode complementarySubgraph::StartNode(TArc a) const throw(ERRange)
{
    return S->StartNode(a);
}


TNode complementarySubgraph::EndNode(TArc a) const throw(ERRange)
{
    return S->EndNode(a);
}


bool complementarySubgraph::HasArc(TArc a) const throw(ERRange)
{
    return ((Arcs[a] != NoArc) || (Arcs[Reverse(a)] != NoArc));
}


TArc complementarySubgraph::First(TNode v) const throw(ERRange,ERRejected)
{
    if (!HasNode(v))
        throw ERRejected();//v geh�t nicht zu diesem Teilgraph

    TArc a,a0;
    cout << "withoutFirst: " << v << " ";
    a = a0 = S->First(v);
    cout << StartNode(a) << "," << EndNode(a) << " ";
    if (HasArc(a)) return a;

    do
    {
        a = S->Right(a,v);
        cout << StartNode(a) << "," << EndNode(a) << " ";
    }
    while (!HasArc(a) && (a != a0));
    cout << endl;

    if (a != a0) return a;
    else throw ERRejected();//was soll hier getan werden?  v hat keine Kanten in diesem Teilgraph
}


TArc complementarySubgraph::Right(TArc a,TNode v) const throw(ERRange,ERRejected)
{
    if (!HasNode(v) || !HasArc(a))
        throw ERRejected();//v bzw. a geh�t nicht zu diesem Teilgraph

    cout << "SRight von ";
    cout << StartNode(a) << "," << EndNode(a) << " ";
    TArc b = S->Right(a,v);
    cout << " ; " << StartNode(b) << "," << EndNode(b) << " ";

    if (HasArc(b)) return b;

    TArc b0 = b;

    do
    {
        b = S->Right(b,v);
    }
    while (!HasArc(b) && (b != b0));

    if (b != b0) return b;
    else throw ERRejected();//v keine weitere Kante in diesem Teilgraph
                        // oder soll First zurckgegeben werden
}
