#include "create_Aggr.h"
#include <STEPaggregate.h>

EnumAggregate * create_EnumAggregate() {
    return new EnumAggregate;
}

GenericAggregate * create_GenericAggregate() {
    return new GenericAggregate;
}

EntityAggregate * create_EntityAggregate() {
    return new EntityAggregate;
}

SelectAggregate * create_SelectAggregate() {
    return new SelectAggregate;
}

StringAggregate * create_StringAggregate() {
    return new StringAggregate;
}

BinaryAggregate * create_BinaryAggregate() {
    return new BinaryAggregate;
}

RealAggregate * create_RealAggregate() {
    return new RealAggregate;
}

IntAggregate * create_IntAggregate() {
    return new IntAggregate;
}
