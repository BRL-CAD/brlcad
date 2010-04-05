
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   auxiliaryNetwork.h
/// \brief  #layeredAuxNetwork class interface


#if !defined(_AUXILIARY_NETWORK_H_)

#define _AUXILIARY_NETWORK_H_

#include "abstractDigraph.h"
#include "staticQueue.h"


/// \addtogroup maximumFlow
/// @{

/// \brief  A class for supporting supporting network flow methods which use layered augmentation
///
/// An auxiliary network is a sparely represented DAG and defined relative to
///  a flow network G (a digraph with fixed arc capacities) and a pair of
/// nodes s, t for which a flow has to be maximized (t is not specified
/// in the constructor, but only in augmenation phases).
///
/// The incidence structure of the auxiliary network is manipulated in phases.
/// In the odd phases, incidences are generated for the arcs which occur on a
/// shortest residual path. In the even phases (the augmentation phases),
/// arcs are deleted when no further residual capacity is available.

class layeredAuxNetwork : public abstractDiGraph
{
friend class iLayeredAuxNetwork;

protected:

    abstractDiGraph&        G;
    TNode                   s;
    TArc*                   pred;

    investigator*           I;
    char                    Phase;
    char*                   align;

    TArc*                   outDegree;
    TArc**                  successor;
    TArc*                   inDegree;
    TArc*                   currentDegree;
    TArc**                  prop;

    staticQueue<TNode>* blocked;

public:

    layeredAuxNetwork(abstractDiGraph &GC,TNode ss) throw();
    ~layeredAuxNetwork() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    investigator*   NewInvestigator() const throw();


    TNode           StartNode(TArc a) const throw(ERRange);
    TNode           EndNode(TArc a) const throw(ERRange);

    TArc            First(TNode v) const throw(ERRange,ERRejected);
    TArc            Right(TArc a,TNode v) const throw(ERRange,ERRejected);

    TCap            UCap(TArc a) const throw(ERRange);
    bool            CUCap() const throw() {return G.CUCap();};
    bool            Blocking(TArc a) const throw(ERRange);
    char            Orientation(TArc a) throw(ERRange);
    bool            COrientation() throw() {return false;};
    void            WriteIncidences(goblinExport* F) throw()
                        {G.WriteIncidences(F);};
    TFloat          Dist(TNode v) throw(ERRange);

    TFloat          Length(TArc a) const throw() {return 1;};
    bool            CLength() const throw() {return true;};

    TFloat          C(TNode v,char i) const throw(ERRange) {return G.C(v,i);};
    TFloat          CMax(char i) const throw(ERRange) {return G.CMax(i);};
    TDim            Dim() const throw() {return G.Dim();};
    bool            HiddenNode(TNode v) const throw(ERRange) {return G.HiddenNode(v);};


    void            Phase1() throw(ERRejected);
    void            Init() throw();
    void            InsertProp(TArc a) throw(ERRange,ERRejected);

    void            Phase2() throw(ERRejected);
    bool            Blocked(TNode v) const throw(ERRange);
    TFloat          FindPath(TNode t) throw(ERRange,ERRejected);
    void            TopErasure(TArc a) throw(ERRange,ERRejected);

    TFloat          Sub(TArc a) const throw() {return 0;};

};


/// \brief  Investigators for #layeredAuxNetwork objects

class iLayeredAuxNetwork : public investigator
{
private:

    const layeredAuxNetwork &G;
    TNode                   n;

    TArc*                   currentIndex;

public:

    iLayeredAuxNetwork(const layeredAuxNetwork &GC) throw();
    ~iLayeredAuxNetwork() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void            Reset() throw();
    void            Reset(TNode v) throw(ERRange);
    TArc            Read(TNode v) throw(ERRange,ERRejected);
    TArc            Peek(TNode v) throw(ERRange,ERRejected);
    bool            Active(TNode v) const throw(ERRange);
    void            Skip(TNode v) throw(ERRange,ERRejected);

};

/// @}


#endif

