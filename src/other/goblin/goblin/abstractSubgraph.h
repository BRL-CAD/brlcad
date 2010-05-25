
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#ifndef _ABSTRACT_SUBGRAPH_H_
#define _ABSTRACT_SUBGRAPH_H_

//vorerst zusätzlich:
#include "sparseGraph.h"

/*!
 * \brief Dieses Interface repr�entiert einen Teilgraph.
 *
 * Ein Teilgraph enthält jeweils eine Teilmenge von Knoten und Kanten aus
 * dem Ursprungsgraph getSourceGraph().
 *
 * Momentan kann mit der Methode createGraphObject() ein graph-Objekt (das
 * ja indirekt von abstractMixedGraph abgeleitet ist) erzeugt werden, welches
 * alle Knoten des SourceGraph und alle Kanten des Teilgraphen enthält.
 */
class abstractSubgraph : public virtual managedObject
{

public:
    //New methods in this class
    /*! \brief getSourceGraph returns a reference to the original %graph.
     *
     * The returned reference doesn't have to be deleted!
     * Deletion will be made automatically.*/
    virtual const abstractMixedGraph* SourceGraph() const throw() = 0;

    /*!
     * \brief returns a reference to a %graph: SourceGraph without this %graph
     *
     * The returned reference doesn't have to be deleted!
     * Deletion will be made automatical.
     *
     * If SourceGraph G:= (V,E) and %subgraph H:= (V1,E1) and V1 is part of
     * V and E1 is part of E, then the returned graph I:= (V2,E2) is
     * defined as follows:
     * - V2 := "V without V1"
     * - E2 := all edges which are incident in V2
     * that means all edges which are between a vertex of V1 and one of V2
     * are not either in E1 nor in E2.
     *
     * => the "WithoutGraph" of I is the original graph H.
     */
    /*  + represent vertices;  _,= represent edges.
     *  the =-edges belong to no graph
     *  |--without graph(V2)------------|----subgraph (V1)----|
     *  |              __ +     ==      +   __       __   +   |
     *  |       +  __                   |    __  +    __      |
     *  |              +        ==      +                +    |
     *  |-----------------------source graph (V)--------------|
     */
    virtual const abstractSubgraph* ComplementarySubgraph() const throw() = 0;
    //!tests, if node is on this %subgraph
    virtual bool    HasNode(TNode node) const throw(ERRange) = 0;

    /*!
     * \brief adds an arc (from sourcegraph) to the %subgraph.
     *
     * If one of the nodes StartNode(a) or EndNode(a) isn't on the subgraph, it
     * will be automatical be added.
     * \param a an arc of the SourceGraph
     * \exception ERRange, ERRejected ??? Check this!
     */
    virtual void    AddArc(TArc a) throw(ERRange,ERRejected) = 0;
    virtual TArc    AddArc(TNode u,TNode v) throw(ERRange,ERRejected) = 0;
    virtual void    AddNode(TNode u) throw(ERRange,ERRejected) = 0;
    /*!
     * \brief deletes an arc from the %subgraph.
     *
     * The nodes StartNode(a) and EndNode(a) will remain on the subgraph.
     * \param a an arc of the SourceGraph
     * \exception ERRange, ERRejected ??? Check this!
     */
    virtual void    OmitArc(TArc a) throw(ERRange,ERRejected) = 0;
    virtual void    OmitArc(TNode u,TNode v) throw(ERRange,ERRejected) = 0;
    virtual void    OmitNode(TNode u) throw(ERRange,ERRejected) = 0;

    virtual void    OmitIsolatedNodes() throw() = 0;

    //provisorisch:
    virtual sparseGraph*  CreateGraphObject() const throw();
    virtual void    Write(const char* fileName) const throw(ERFile);


    //overwrite following methods of abstractMixedGraph_Int
    virtual TNode   StartNode(TArc) const throw(ERRange) = 0;
    virtual TNode   EndNode(TArc) const throw(ERRange) = 0;
    virtual bool    HasArc(TArc a) const throw(ERRange) = 0;
    virtual TNode   N() const throw() = 0;     //!<returns the number of vertices
    virtual TArc    M() const throw() = 0;     //!<returns the number of edges
    virtual unsigned long   Allocated() const throw();

    virtual TArc    Reverse(TArc a) const throw(ERRange);

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


#ifndef _ABSTRACT_SUBGRAPH_H_
#define _ABSTRACT_SUBGRAPH_H_

//vorerst zusätzlich:
#include "sparseGraph.h"

/*!
 * \brief Dieses Interface repr�entiert einen Teilgraph.
 *
 * Ein Teilgraph enthält jeweils eine Teilmenge von Knoten und Kanten aus
 * dem Ursprungsgraph getSourceGraph().
 *
 * Momentan kann mit der Methode createGraphObject() ein graph-Objekt (das
 * ja indirekt von abstractMixedGraph abgeleitet ist) erzeugt werden, welches
 * alle Knoten des SourceGraph und alle Kanten des Teilgraphen enthält.
 */
class abstractSubgraph : public virtual managedObject
{

public:
    //New methods in this class
    /*! \brief getSourceGraph returns a reference to the original %graph.
     *
     * The returned reference doesn't have to be deleted!
     * Deletion will be made automatically.*/
    virtual const abstractMixedGraph* SourceGraph() const throw() = 0;

    /*!
     * \brief returns a reference to a %graph: SourceGraph without this %graph
     *
     * The returned reference doesn't have to be deleted!
     * Deletion will be made automatical.
     *
     * If SourceGraph G:= (V,E) and %subgraph H:= (V1,E1) and V1 is part of
     * V and E1 is part of E, then the returned graph I:= (V2,E2) is
     * defined as follows:
     * - V2 := "V without V1"
     * - E2 := all edges which are incident in V2
     * that means all edges which are between a vertex of V1 and one of V2
     * are not either in E1 nor in E2.
     *
     * => the "WithoutGraph" of I is the original graph H.
     */
    /*  + represent vertices;  _,= represent edges.
     *  the =-edges belong to no graph
     *  |--without graph(V2)------------|----subgraph (V1)----|
     *  |              __ +     ==      +   __       __   +   |
     *  |       +  __                   |    __  +    __      |
     *  |              +        ==      +                +    |
     *  |-----------------------source graph (V)--------------|
     */
    virtual const abstractSubgraph* ComplementarySubgraph() const throw() = 0;
    //!tests, if node is on this %subgraph
    virtual bool    HasNode(TNode node) const throw(ERRange) = 0;

    /*!
     * \brief adds an arc (from sourcegraph) to the %subgraph.
     *
     * If one of the nodes StartNode(a) or EndNode(a) isn't on the subgraph, it
     * will be automatical be added.
     * \param a an arc of the SourceGraph
     * \exception ERRange, ERRejected ??? Check this!
     */
    virtual void    AddArc(TArc a) throw(ERRange,ERRejected) = 0;
    virtual TArc    AddArc(TNode u,TNode v) throw(ERRange,ERRejected) = 0;
    virtual void    AddNode(TNode u) throw(ERRange,ERRejected) = 0;
    /*!
     * \brief deletes an arc from the %subgraph.
     *
     * The nodes StartNode(a) and EndNode(a) will remain on the subgraph.
     * \param a an arc of the SourceGraph
     * \exception ERRange, ERRejected ??? Check this!
     */
    virtual void    OmitArc(TArc a) throw(ERRange,ERRejected) = 0;
    virtual void    OmitArc(TNode u,TNode v) throw(ERRange,ERRejected) = 0;
    virtual void    OmitNode(TNode u) throw(ERRange,ERRejected) = 0;

    virtual void    OmitIsolatedNodes() throw() = 0;

    //provisorisch:
    virtual sparseGraph*  CreateGraphObject() const throw();
    virtual void    Write(const char* fileName) const throw(ERFile);


    //overwrite following methods of abstractMixedGraph_Int
    virtual TNode   StartNode(TArc) const throw(ERRange) = 0;
    virtual TNode   EndNode(TArc) const throw(ERRange) = 0;
    virtual bool    HasArc(TArc a) const throw(ERRange) = 0;
    virtual TNode   N() const throw() = 0;     //!<returns the number of vertices
    virtual TArc    M() const throw() = 0;     //!<returns the number of edges
    virtual unsigned long   Allocated() const throw();

    virtual TArc    Reverse(TArc a) const throw(ERRange);

};

#endif

