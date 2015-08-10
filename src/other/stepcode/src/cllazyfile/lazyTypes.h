#ifndef LAZYTYPES_H
#define LAZYTYPES_H

#include <iostream>
#include <vector>
#include <stdint.h>

#include "judyLArray.h"
#include "judySArray.h"
#include "judyL2Array.h"
#include "judyS2Array.h"

class SDAI_Application_instance;
class lazyDataSectionReader;
class lazyFileReader;

enum fileTypeEnum { Part21, Part28 };
// enum loadingEnum { immediate, lazy };

typedef uint64_t instanceID;  ///< the number assigned to an instance in the file
typedef int16_t sectionID;   ///< globally unique index of a sectionReader in a sectionReaderVec_t
typedef int16_t fileID;      ///< the index of a lazyFileReader in a lazyFileReaderVec_t. Can be inferred from a sectionID

/** store 16 bits of section id and 48 of instance offset into one 64-bit int
 * use thus:
 * positionAndSection ps = section;
 * ps <<= 48;
 * ps |= ( offset & 0xFFFFFFFFFFFF );
 * //...
 * offset = (ps & 0xFFFFFFFFFFFF );
 * section = ( ps >> 48 );
 * TODO: Also 8 bits of flags? Would allow files of 2^40 bytes, or ~1TB.
 */
typedef uint64_t positionAndSection;

typedef std::vector< instanceID > instanceRefs;

//TODO: create a "unique instance id" from the sectionID and instanceID, and use it everywhere?

/** This struct contains all the information necessary to locate an instance. It is primarily
 * for instances that are present in a file but not memory. It could be useful in other
 * situations, so the information should be kept up-to-date.
 */
typedef struct {
    long begin;
    instanceID instance;
    sectionID section;
    /* bool modified; */ /* this will be useful when writing instances - if an instance is
    unmodified, simply copy it from the input file to the output file */
} lazyInstanceLoc;

/// used when populating the instance type map \sa lazyInstMgr::_instanceTypeMMap
typedef struct {
    lazyInstanceLoc loc;
    const char * name;
    instanceRefs * refs;
} namedLazyInstance;

// instanceRefs - map between an instanceID and instances that refer to it
typedef judyL2Array< instanceID, instanceID > instanceRefs_t;

// instanceType_t - multimap from instance type to instanceID's
typedef judyS2Array< instanceID > instanceTypes_t;

// instancesLoaded - fully created instances
typedef judyLArray< instanceID, SDAI_Application_instance * > instancesLoaded_t;

// instanceStreamPos - map instance id to a streampos and data section
// there could be multiple instances with the same ID, but in different files (or different sections of the same file?)
typedef judyL2Array< instanceID, positionAndSection > instanceStreamPos_t;


// data sections
typedef std::vector< lazyDataSectionReader * > dataSectionReaderVec_t;

// files
typedef std::vector< lazyFileReader * > lazyFileReaderVec_t;

// type for performing actions on multiple instances
// NOTE not useful? typedef std::vector< lazyInstance > lazyInstanceVec_t;

#endif //LAZYTYPES_H

