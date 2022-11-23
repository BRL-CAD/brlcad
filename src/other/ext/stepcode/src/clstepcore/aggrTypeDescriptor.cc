#include "aggrTypeDescriptor.h"

STEPaggregate * AggrTypeDescriptor::CreateAggregate() {
    if( CreateNewAggr ) {
        return CreateNewAggr();
    } else {
        return 0;
    }
}

void AggrTypeDescriptor::AssignAggrCreator( AggregateCreator f ) {
    CreateNewAggr = f;
}

AggrTypeDescriptor::AggrTypeDescriptor( ) :
_uniqueElements( "UNKNOWN_TYPE" ) {
    _bound1 = -1;
    _bound2 = -1;
    _aggrDomainType = 0;
}

AggrTypeDescriptor::AggrTypeDescriptor( SDAI_Integer  b1,
                                        SDAI_Integer  b2,
                                        Logical uniqElem,
                                        TypeDescriptor * aggrDomType )
: _bound1( b1 ), _bound2( b2 ), _uniqueElements( uniqElem ) {
    _aggrDomainType = aggrDomType;
}

AggrTypeDescriptor::~AggrTypeDescriptor() {
}
