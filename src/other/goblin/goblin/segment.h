
/*
**  This file forms part of the GOBLIN C++ Class Library.
**
**  Written by Birk Eisermann, August 2003.
**
**  Copying, compiling, distribution and modification
**  of this source code is permitted only in accordance
**  with the GOBLIN general licence information.
*/


#ifndef _SEGMENT_H_
#define _SEGMENT_H_

#include "subgraph.h"

/*!
 * \brief concrete %segment class
 *
 */

class segmentGraph : public subgraph
{
private:

    /* Speichert, wenn Node Kontaktpunkt den Index in ContactNodes sonst NoNode; */
    TNode *     ContactNodeIndex;
    TNode *     ContactNodes;
        ///<Speichert Kontaktpunkte der Reihe nach
    TNode       ContactNodeCount;

    //speichern possible regions!
    int (*      Regions);
    int         RegionsCount;

public:

    segmentGraph(abstractMixedGraph *G) throw();
    virtual ~segmentGraph() throw();

    virtual void    MarkAsContactNode(TNode) throw(ERRange,ERRejected);
        ///<Komplexität: O(1)
    virtual void    MarkAsContactArc(TArc) throw(ERRange,ERRejected);
        ///<Komplexität: O(1)
    /*Komplexität:O(n) mit Beibehaltung der Reihenfolge; (mit swap(i,N) <=> ohne Reihenfolge:O(1) mögl.*/
    virtual void    ReleaseContactNode(TNode) throw(ERRange,ERRejected);
    virtual TNode   GetContactNodeCount() throw();
        ///<Komplexität: O(1)
    virtual TNode   GetContactNode(TNode i) throw(ERRange);
        ///<Komplexität: O(1)
    virtual bool    IsContactNode(TNode) throw(ERRange);
        ///<Komplexität: O(1)

    virtual void    AddRegion(int) throw();
    virtual void    OmitRegion(int) throw();
    virtual int     GetRegionsCount() throw();
    virtual int     GetRegion(int i) throw();
    virtual bool    HasRegion(int) throw();

};

#endif
