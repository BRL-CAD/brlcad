
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file sparseInclude.h
/// \brief Methods which are common among all sparse represented graph objects


bool            IsSparse() const throw()
                    {return true;};

TNode           StartNode(TArc a) const throw(ERRange)
                    {return X.StartNode(a);};
TNode           EndNode(TArc a) const throw(ERRange)
                    {return X.EndNode(a);};

TArc            First(TNode v) const throw(ERRange)
                    {return X.First(v);};
TArc            Right(TArc a,TNode v = NoNode) const throw(ERRange)
                    {return X.Right(a);};
TArc            Left(TArc a) const throw(ERRange)
                    {return X.Left(a);};


#include <graphInclude.h>
