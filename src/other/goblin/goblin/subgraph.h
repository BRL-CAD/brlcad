
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#ifndef _SUBGRAPH_H_
#define _SUBGRAPH_H_

#ifndef _ABSTRACT_SUBGRAPH_H_
#include "abstractSubgraph.h"
#endif

/*!
 * \brief concrete %subgraph class
 *@author Birk
 *
 * Dies ist eine allgemeine Implementierung vom Interface abstractMixedSubgraph.
 * Aktualisierung von WithoutGraph fehlt!!
 */

class subgraph : public abstractSubgraph
{
friend class complementarySubgraph;
private:

    const abstractMixedGraph* S;           //!<Ursprungsgraph
    mutable abstractSubgraph* Complement;  //!<SourceGraph ohne diesem Graph
    TNode*              Nodes;
    TArc*               Arcs;
    TNode               Nodes_on_subgraph;
    TArc                Arcs_on_subgraph;

public:
    subgraph(const subgraph *G) throw();
    subgraph(abstractMixedGraph *G) throw();
    virtual ~subgraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    //from abstractMixedSubgraph
    virtual const abstractMixedGraph* SourceGraph() const throw();
    virtual const abstractSubgraph* ComplementarySubgraph() const throw();
    virtual bool    HasNode(TNode node) const throw(ERRange);

    virtual void    AddArc(TArc a) throw(ERRange,ERRejected);
    virtual TArc    AddArc(TNode u,TNode v) throw(ERRange,ERRejected);
    virtual void    AddNode(TNode u) throw(ERRange,ERRejected);
    virtual void    OmitArc(TArc a) throw(ERRange,ERRejected);
    virtual void    OmitArc(TNode u,TNode v) throw(ERRange,ERRejected);
    virtual void    OmitNode(TNode u) throw(ERRange,ERRejected);
    virtual void    OmitIsolatedNodes() throw();


    //from abstractMixedGraph_Int
    virtual TArc    Adjacency(TNode from,TNode to) throw(ERRange);

    virtual TNode   N() const throw();
    virtual TArc    M() const throw();
    virtual TNode   StartNode(TArc) const throw(ERRange);
    virtual TNode   EndNode(TArc) const throw(ERRange);
    virtual bool    HasArc(TArc) const throw(ERRange);
    virtual TArc    First(TNode) const throw(ERRange,ERRejected);
    virtual TArc    Right(TArc,TNode) const throw(ERRange,ERRejected);

};

#endif

/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#ifndef _SUBGRAPH_H_
#define _SUBGRAPH_H_

#ifndef _ABSTRACT_SUBGRAPH_H_
#include "abstractSubgraph.h"
#endif

/*!
 * \brief concrete %subgraph class
 *@author Birk
 *
 * Dies ist eine allgemeine Implementierung vom Interface abstractMixedSubgraph.
 * Aktualisierung von WithoutGraph fehlt!!
 */

class subgraph : public abstractSubgraph
{
friend class complementarySubgraph;
private:

    const abstractMixedGraph* S;           //!<Ursprungsgraph
    mutable abstractSubgraph* Complement;  //!<SourceGraph ohne diesem Graph
    TNode*              Nodes;
    TArc*               Arcs;
    TNode               Nodes_on_subgraph;
    TArc                Arcs_on_subgraph;

public:
    subgraph(const subgraph *G) throw();
    subgraph(abstractMixedGraph *G) throw();
    virtual ~subgraph() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    //from abstractMixedSubgraph
    virtual const abstractMixedGraph* SourceGraph() const throw();
    virtual const abstractSubgraph* ComplementarySubgraph() const throw();
    virtual bool    HasNode(TNode node) const throw(ERRange);

    virtual void    AddArc(TArc a) throw(ERRange,ERRejected);
    virtual TArc    AddArc(TNode u,TNode v) throw(ERRange,ERRejected);
    virtual void    AddNode(TNode u) throw(ERRange,ERRejected);
    virtual void    OmitArc(TArc a) throw(ERRange,ERRejected);
    virtual void    OmitArc(TNode u,TNode v) throw(ERRange,ERRejected);
    virtual void    OmitNode(TNode u) throw(ERRange,ERRejected);
    virtual void    OmitIsolatedNodes() throw();


    //from abstractMixedGraph_Int
    virtual TArc    Adjacency(TNode from,TNode to) throw(ERRange);

    virtual TNode   N() const throw();
    virtual TArc    M() const throw();
    virtual TNode   StartNode(TArc) const throw(ERRange);
    virtual TNode   EndNode(TArc) const throw(ERRange);
    virtual bool    HasArc(TArc) const throw(ERRange);
    virtual TArc    First(TNode) const throw(ERRange,ERRejected);
    virtual TArc    Right(TArc,TNode) const throw(ERRange,ERRejected);

};

#endif
