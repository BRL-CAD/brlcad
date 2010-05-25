
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file graphRepresentation.h
/// \brief #graphRepresentation class interface

#ifndef _GRAPH_REPRESENTATION_H_
#define _GRAPH_REPRESENTATION_H_

#include "abstractMixedGraph.h"
#include "fileImport.h"


/// \brief  The base class for graph representational objects

class graphRepresentation : public virtual managedObject
{
protected:

    /// The graph object which is composed from this
    abstractMixedGraph& G;

    /// An attribute pool representing the graph skeleton
    attributePool   representation;

    /// An attribute pool representing node coordinates
    attributePool   geometry;

    /// An attribute pool representing all layout data
    attributePool   layoutData;

    TNode   nMax; ///< The number of reserved graph nodes
    TArc    mMax; ///< The number of reserved arcs
    TNode   lMax; ///< The number of reserved layout points

    TNode   nAct; ///< The number of actual graph nodes
    TArc    mAct; ///< The number of actual arcs
    TNode   lAct; ///< The number of actual layout points

public:

    graphRepresentation(const abstractMixedGraph& _G) throw();
    virtual ~graphRepresentation() throw();

    unsigned long  Allocated() const throw();


    /// \addtogroup objectDimensions
    /// @{

    /// \brief  Initialize the final dimensions
    ///
    /// \param _n  The expected number of graph nodes
    /// \param _m  The expected number of graph edges
    /// \param _l  The expected number of layout nodes
    void Reserve(TNode _n,TArc _m,TNode _l) throw(ERRejected);

    inline TNode N() const throw();
    inline TArc M() const throw();
    inline TNode L() const throw();

    /// @}


    size_t  SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw();

    inline const attributePool& RepresentationalData() const throw();
    inline const attributePool& Geometry() const throw();
    inline const attributePool& LayoutData() const throw();

    static const TFloat defaultLength;
    static const TCap   defaultUCap;
    static const TCap   defaultLCap;
    static const TCap   defaultDemand;
    static const TFloat defaultC;
    static const char   defaultOrientation;

    inline TCap     UCap(TArc a) const throw(ERRange);
    void            SetUCap(TArc a,TCap cc) throw(ERRange);
    void            SetCUCap(TCap cc) throw(ERRange);

    inline TCap     LCap(TArc a) const throw(ERRange);
    void            SetLCap(TArc a,TCap bb) throw(ERRange);
    void            SetCLCap(TCap bb) throw(ERRange);

    void            SetDemand(TNode v,TCap bb) throw(ERRange);
    void            SetCDemand(TCap bb) throw();

    /// \brief  Set the type of arc length metric
    ///
    /// \param metricType  The desired metric type
    void  SetMetricType(abstractMixedGraph::TMetricType metricType) throw();

    TFloat          Length(TArc a) const throw(ERRange);
    TFloat          MaxLength() const throw();
    void            SetLength(TArc a,TFloat ll) throw(ERRange);
    inline bool     CLength() const throw();
    void            SetCLength(TFloat ll) throw();

    inline char     Orientation(TArc a) const throw(ERRange);
    void            SetOrientation(TArc a,char oo) throw(ERRange);
    void            SetCOrientation(char oo) throw();
    bool            Blocking(TArc a) const throw(ERRange);

    TDim            Dim() const throw();
    inline TFloat   C(TNode v,TDim i) const throw(ERRange);
    void            SetC(TNode v,TDim i,TFloat pos) throw(ERRange);

    /// \brief  Release a node coordinate attribute
    ///
    /// \param i  The index of the coordinate to be deleted
    void  ReleaseCoordinate(TDim i) throw(ERRange);


    /// \addtogroup groupLayoutFilter
    /// @{

    /// \brief Check whether a given graph node shall be visualized or not
    ///
    /// \param v      A node index ranged [0,1,..,nAct-1]
    /// \retval true  This node is not visualized
    ///
    /// If the node v is not visualized, none of the incident arcs is visualized
    bool  HiddenNode(TNode v) const throw(ERRange);

    /// \brief Check whether a given arc shall be visualized or not
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \retval true  This arc is not visualized
    bool  HiddenArc(TArc a) const throw(ERRange);

    /// @}


    /// \addtogroup subgraphManagement
    /// @{

    /// \brief Allocate the subgraph multiplicities
    ///
    /// This initializes the subgraph multiplicities to the lower capacity bounds
    virtual void  NewSubgraph() throw(ERRejected) = 0;

    /// \brief Deallocate the subgraph multiplicities
    ///
    /// This effectively sets the subgraph multiplicities to the lower capacity bounds
    virtual void  ReleaseSubgraph() throw() = 0;

    /// \brief Retrieve the subgraph multiplicity of a given arc
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \return       The multiplicity of a
    virtual TFloat  Sub(TArc a) const throw(ERRange) = 0;

    /// \brief Set the subgraph multiplicity to a specified value
    ///
    /// \param a             An arc index ranged [0,1,..,2*mAct-1]
    /// \param multiplicity  The intended multiplicity of a
    virtual void  SetSub(TArc a,TFloat multiplicity) throw(ERRange,ERRejected) = 0;

    /// \brief Change the subgraph multiplicity by a specified amount
    ///
    /// \param a       An arc index ranged [0,1,..,2*mAct-1]
    /// \param lambda  Change the multiplicity of a by this amount
    virtual void  SetSubRelative(TArc a,TFloat lambda) throw(ERRange,ERRejected) = 0;

    /// @}

};


inline TNode graphRepresentation::N() const throw()
{
    return nAct;
}


inline TArc graphRepresentation::M() const throw()
{
    return mAct;
}


inline TNode graphRepresentation::L() const throw()
{
    return lAct;
}


inline const attributePool& graphRepresentation::RepresentationalData() const throw()
{
    return representation;
}


inline const attributePool& graphRepresentation::Geometry() const throw()
{
    return geometry;
}


inline const attributePool& graphRepresentation::LayoutData() const throw()
{
    return layoutData;
}


inline TCap graphRepresentation::UCap(TArc a) const throw(ERRange)
{
    return representation.GetValue<TCap>(TokReprUCap,a>>1,defaultUCap);
}


inline TCap graphRepresentation::LCap(TArc a) const throw(ERRange)
{
    return representation.GetValue<TCap>(TokReprLCap,a>>1,defaultLCap);
}


inline bool graphRepresentation::CLength() const throw()
{
    return representation.IsConstant<TFloat>(TokReprLength);
}


inline char graphRepresentation::Orientation(TArc a) const throw(ERRange)
{
    return representation.GetValue<char>(TokReprOrientation,a>>1,defaultOrientation);
}


inline TFloat graphRepresentation::C(TNode v,TDim i) const throw(ERRange)
{
    return geometry.GetValue<TFloat>(TokGeoAxis0+i,v,defaultC);
}


#endif


//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file graphRepresentation.h
/// \brief #graphRepresentation class interface

#ifndef _GRAPH_REPRESENTATION_H_
#define _GRAPH_REPRESENTATION_H_

#include "abstractMixedGraph.h"
#include "fileImport.h"


/// \brief  The base class for graph representational objects

class graphRepresentation : public virtual managedObject
{
protected:

    /// The graph object which is composed from this
    abstractMixedGraph& G;

    /// An attribute pool representing the graph skeleton
    attributePool   representation;

    /// An attribute pool representing node coordinates
    attributePool   geometry;

    /// An attribute pool representing all layout data
    attributePool   layoutData;

    TNode   nMax; ///< The number of reserved graph nodes
    TArc    mMax; ///< The number of reserved arcs
    TNode   lMax; ///< The number of reserved layout points

    TNode   nAct; ///< The number of actual graph nodes
    TArc    mAct; ///< The number of actual arcs
    TNode   lAct; ///< The number of actual layout points

public:

    graphRepresentation(const abstractMixedGraph& _G) throw();
    virtual ~graphRepresentation() throw();

    unsigned long  Allocated() const throw();


    /// \addtogroup objectDimensions
    /// @{

    /// \brief  Initialize the final dimensions
    ///
    /// \param _n  The expected number of graph nodes
    /// \param _m  The expected number of graph edges
    /// \param _l  The expected number of layout nodes
    void Reserve(TNode _n,TArc _m,TNode _l) throw(ERRejected);

    inline TNode N() const throw();
    inline TArc M() const throw();
    inline TNode L() const throw();

    /// @}


    size_t  SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw();

    inline const attributePool& RepresentationalData() const throw();
    inline const attributePool& Geometry() const throw();
    inline const attributePool& LayoutData() const throw();

    static const TFloat defaultLength;
    static const TCap   defaultUCap;
    static const TCap   defaultLCap;
    static const TCap   defaultDemand;
    static const TFloat defaultC;
    static const char   defaultOrientation;

    inline TCap     UCap(TArc a) const throw(ERRange);
    void            SetUCap(TArc a,TCap cc) throw(ERRange);
    void            SetCUCap(TCap cc) throw(ERRange);

    inline TCap     LCap(TArc a) const throw(ERRange);
    void            SetLCap(TArc a,TCap bb) throw(ERRange);
    void            SetCLCap(TCap bb) throw(ERRange);

    void            SetDemand(TNode v,TCap bb) throw(ERRange);
    void            SetCDemand(TCap bb) throw();

    /// \brief  Set the type of arc length metric
    ///
    /// \param metricType  The desired metric type
    void  SetMetricType(abstractMixedGraph::TMetricType metricType) throw();

    TFloat          Length(TArc a) const throw(ERRange);
    TFloat          MaxLength() const throw();
    void            SetLength(TArc a,TFloat ll) throw(ERRange);
    inline bool     CLength() const throw();
    void            SetCLength(TFloat ll) throw();

    inline char     Orientation(TArc a) const throw(ERRange);
    void            SetOrientation(TArc a,char oo) throw(ERRange);
    void            SetCOrientation(char oo) throw();
    bool            Blocking(TArc a) const throw(ERRange);

    TDim            Dim() const throw();
    inline TFloat   C(TNode v,TDim i) const throw(ERRange);
    void            SetC(TNode v,TDim i,TFloat pos) throw(ERRange);

    /// \brief  Release a node coordinate attribute
    ///
    /// \param i  The index of the coordinate to be deleted
    void  ReleaseCoordinate(TDim i) throw(ERRange);


    /// \addtogroup groupLayoutFilter
    /// @{

    /// \brief Check whether a given graph node shall be visualized or not
    ///
    /// \param v      A node index ranged [0,1,..,nAct-1]
    /// \retval true  This node is not visualized
    ///
    /// If the node v is not visualized, none of the incident arcs is visualized
    bool  HiddenNode(TNode v) const throw(ERRange);

    /// \brief Check whether a given arc shall be visualized or not
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \retval true  This arc is not visualized
    bool  HiddenArc(TArc a) const throw(ERRange);

    /// @}


    /// \addtogroup subgraphManagement
    /// @{

    /// \brief Allocate the subgraph multiplicities
    ///
    /// This initializes the subgraph multiplicities to the lower capacity bounds
    virtual void  NewSubgraph() throw(ERRejected) = 0;

    /// \brief Deallocate the subgraph multiplicities
    ///
    /// This effectively sets the subgraph multiplicities to the lower capacity bounds
    virtual void  ReleaseSubgraph() throw() = 0;

    /// \brief Retrieve the subgraph multiplicity of a given arc
    ///
    /// \param a      An arc index ranged [0,1,..,2*mAct-1]
    /// \return       The multiplicity of a
    virtual TFloat  Sub(TArc a) const throw(ERRange) = 0;

    /// \brief Set the subgraph multiplicity to a specified value
    ///
    /// \param a             An arc index ranged [0,1,..,2*mAct-1]
    /// \param multiplicity  The intended multiplicity of a
    virtual void  SetSub(TArc a,TFloat multiplicity) throw(ERRange,ERRejected) = 0;

    /// \brief Change the subgraph multiplicity by a specified amount
    ///
    /// \param a       An arc index ranged [0,1,..,2*mAct-1]
    /// \param lambda  Change the multiplicity of a by this amount
    virtual void  SetSubRelative(TArc a,TFloat lambda) throw(ERRange,ERRejected) = 0;

    /// @}

};


inline TNode graphRepresentation::N() const throw()
{
    return nAct;
}


inline TArc graphRepresentation::M() const throw()
{
    return mAct;
}


inline TNode graphRepresentation::L() const throw()
{
    return lAct;
}


inline const attributePool& graphRepresentation::RepresentationalData() const throw()
{
    return representation;
}


inline const attributePool& graphRepresentation::Geometry() const throw()
{
    return geometry;
}


inline const attributePool& graphRepresentation::LayoutData() const throw()
{
    return layoutData;
}


inline TCap graphRepresentation::UCap(TArc a) const throw(ERRange)
{
    return representation.GetValue<TCap>(TokReprUCap,a>>1,defaultUCap);
}


inline TCap graphRepresentation::LCap(TArc a) const throw(ERRange)
{
    return representation.GetValue<TCap>(TokReprLCap,a>>1,defaultLCap);
}


inline bool graphRepresentation::CLength() const throw()
{
    return representation.IsConstant<TFloat>(TokReprLength);
}


inline char graphRepresentation::Orientation(TArc a) const throw(ERRange)
{
    return representation.GetValue<char>(TokReprOrientation,a>>1,defaultOrientation);
}


inline TFloat graphRepresentation::C(TNode v,TDim i) const throw(ERRange)
{
    return geometry.GetValue<TFloat>(TokGeoAxis0+i,v,defaultC);
}


#endif

