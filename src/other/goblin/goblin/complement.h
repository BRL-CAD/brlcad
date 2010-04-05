
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#ifndef _COMPLEMENT_H_
#define _COMPLEMENT_H_

#include "subgraph.h"

/*!
 * \brief (private) Klasse
 *
 * Dieses Klasse soll nicht direkt instanziiert werden. Sie wird von einem
 * subgraph-Objekt verwendet, um den Graphen: Ursprungsgraph
 * subgraph::getSourceGraph ohne Knoten des eigenen Graphen darzustellen.
 * Dazu wird es der Klasse complementarySubgraph erlaubt, vollständig auf das
 * zugehörige subgraph-Objekt (d.h. auch auf private Member) zu zugreifen.
 *
 * Knoten- und Kantenmanipulation nicht möglich!!
 */
class complementarySubgraph : public abstractSubgraph
{
friend class subgraph;
private:

    const subgraph*     Complement;     ///<Teilgraph von SourceGraph
    /* Kanten von diesem Subgraph werden separat gespeichert, da nur aufwendig
     * aus WithoutArcs errechnet werden k�nen.    */
    TArc*               Arcs;
    TArc                ArcsCount;
        //im folgenden Kopien von complGraph-Feldern (fuer direkten Zugriff)
    const abstractMixedGraph* S;
    TNode*              WithoutNodes;

protected:

    complementarySubgraph(const subgraph *G) throw();
    virtual ~complementarySubgraph() throw();

public:

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    //from abstractMixedSubgraph
    const abstractMixedGraph* SourceGraph() const throw();
    const abstractSubgraph*   ComplementarySubgraph() const throw();

    bool    HasNode(TNode node) const throw(ERRange);

    void    AddArc(TArc a) throw(ERRejected)
        {throw ERRejected(); /*nicht möglich*/};
    TArc    AddArc(TNode u,TNode v) throw(ERRange,ERRejected)
        {throw ERRejected(); /*nicht möglich*/};
    void    AddNode(TNode u) throw(ERRange,ERRejected)
        {throw ERRejected(); /*nicht möglich*/};
    void    OmitArc(TArc a) throw(ERRange,ERRejected)
        {throw ERRejected(); /*nicht möglich*/};
    void    OmitArc(TNode u,TNode v) throw(ERRange,ERRejected)
        {throw ERRejected(); /*nicht möglich*/};
    void    OmitNode(TNode u) throw(ERRange,ERRejected)
        {throw ERRejected(); /*nicht möglich*/};
    void    OmitIsolatedNodes() throw()
        {throw ERRejected();};

    //from abstractMixedGraph_Int
    TArc    Adjacency(TNode from,TNode to) throw(ERRange);

    TNode   N() const throw();
    TArc    M() const throw();
    TNode   StartNode(TArc) const throw(ERRange);
    TNode   EndNode(TArc) const throw(ERRange);
    bool    HasArc(TArc) const throw(ERRange);
    TArc    First(TNode) const throw(ERRange,ERRejected);
    TArc    Right(TArc,TNode) const throw(ERRange,ERRejected);

};

#endif

