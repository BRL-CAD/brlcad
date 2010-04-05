
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file graphInclude.h
/// \brief Methods which are common among all represented graph objects


inline size_t   SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw()
                    {return X.SizeInfo(arrayDim,size);};

inline graphRepresentation* Representation() throw() {return &X;};
inline const graphRepresentation* Representation() const throw() {return &X;};

inline attributePool* RepresentationalData() const throw()
                    {return const_cast<attributePool*>(&X.RepresentationalData());};
inline attributePool* Geometry() const throw()
                    {return const_cast<attributePool*>(&X.Geometry());};
inline attributePool* LayoutData() const throw()
                    {return const_cast<attributePool*>(&X.LayoutData());};

inline void     NewSubgraph() throw(ERRejected)
                    {X.NewSubgraph();};
inline void     ReleaseSubgraph() throw()
                    {X.ReleaseSubgraph();};

inline TFloat   Sub(TArc a) const throw(ERRange)
                    {return X.Sub(a);};
inline void     SetSub(TArc a,TFloat multiplicity) throw(ERRange)
                    {X.SetSub(a,multiplicity);};
inline void     SetSubRelative(TArc a,TFloat lambda) throw(ERRange)
                    {X.SetSubRelative(a,lambda);};
