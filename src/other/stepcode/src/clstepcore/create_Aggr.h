#ifndef AGGRCREATORTD_H
#define AGGRCREATORTD_H

//typedef's for aggregate creators

#include "sdai.h"
#include "sc_export.h"


class STEPaggregate;
class EnumAggregate;
class GenericAggregate;
class EntityAggregate;
class SelectAggregate;
class StringAggregate;
class BinaryAggregate;
class RealAggregate;
class IntAggregate;

typedef STEPaggregate * ( * AggregateCreator )();
typedef EnumAggregate * ( * EnumAggregateCreator )();
typedef GenericAggregate * ( * GenericAggregateCreator )();
typedef EntityAggregate * ( * EntityAggregateCreator )();
typedef SelectAggregate * ( * SelectAggregateCreator )();
typedef StringAggregate * ( * StringAggregateCreator )();
typedef BinaryAggregate * ( * BinaryAggregateCreator )();
typedef RealAggregate * ( * RealAggregateCreator )();
typedef IntAggregate * ( * IntAggregateCreator )();

SC_CORE_EXPORT EnumAggregate * create_EnumAggregate();

SC_CORE_EXPORT GenericAggregate * create_GenericAggregate();

SC_CORE_EXPORT EntityAggregate * create_EntityAggregate();

SC_CORE_EXPORT SelectAggregate * create_SelectAggregate();

SC_CORE_EXPORT StringAggregate * create_StringAggregate();

SC_CORE_EXPORT BinaryAggregate * create_BinaryAggregate();

SC_CORE_EXPORT RealAggregate * create_RealAggregate();

SC_CORE_EXPORT IntAggregate * create_IntAggregate();

typedef SDAI_Integer( *boundCallbackFn )( SDAI_Application_instance * );

#endif //AGGRCREATORTD_H
