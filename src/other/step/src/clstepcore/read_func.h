#ifndef READ_FUNC_H
#define READ_FUNC_H

#include <scl_export.h>
#include <sdai.h>

#define MAX_COMMENT_LENGTH 512

// print Error information for debugging purposes
extern SCL_CORE_EXPORT void PrintErrorState( ErrorDescriptor & err );

// print istream error information for debugging purposes
extern SCL_CORE_EXPORT void IStreamState( istream & in );

extern SCL_CORE_EXPORT int ReadInteger( SDAI_Integer  &val, istream & in, ErrorDescriptor * err,
             const char * tokenList );

extern SCL_CORE_EXPORT int ReadInteger( SDAI_Integer  &val, const char * s, ErrorDescriptor * err,
             const char * tokenList );

extern SCL_CORE_EXPORT Severity IntValidLevel( const char * attrValue, ErrorDescriptor * err,
               int clearError, int optional, const char * tokenList );

extern SCL_CORE_EXPORT char * WriteReal( SDAI_Real  val, std::string & s );

extern SCL_CORE_EXPORT void WriteReal( SDAI_Real  val, ostream & out );

extern SCL_CORE_EXPORT int ReadReal( SDAI_Real  &val, istream & in, ErrorDescriptor * err,
          const char * tokenList );

extern SCL_CORE_EXPORT int ReadReal( SDAI_Real  &val, const char * s, ErrorDescriptor * err,
          const char * tokenList );

extern SCL_CORE_EXPORT Severity RealValidLevel( const char * attrValue, ErrorDescriptor * err,
                int clearError, int optional, const char * tokenList );

extern SCL_CORE_EXPORT int ReadNumber( SDAI_Real  &val, istream & in, ErrorDescriptor * err,
            const char * tokenList );

extern SCL_CORE_EXPORT int ReadNumber( SDAI_Real  &val, const char * s, ErrorDescriptor * err,
            const char * tokenList );

extern SCL_CORE_EXPORT Severity NumberValidLevel( const char * attrValue, ErrorDescriptor * err,
                  int clearError, int optional, const char * tokenList );


////////////////////

extern SCL_CORE_EXPORT int   QuoteInString( istream & in );

extern SCL_CORE_EXPORT void PushPastString( istream & in, std::string & s, ErrorDescriptor * err );

extern SCL_CORE_EXPORT void PushPastImbedAggr( istream & in, std::string & s, ErrorDescriptor * err );

extern SCL_CORE_EXPORT void PushPastAggr1Dim( istream & in, std::string & s, ErrorDescriptor * err );

////////////////////

extern SCL_CORE_EXPORT Severity FindStartOfInstance( istream & in, std::string & inst );

//  used for instances that aren\'t valid - reads to next \';\'
extern SCL_CORE_EXPORT Severity SkipInstance( istream & in, std::string & inst );

extern SCL_CORE_EXPORT const char * SkipSimpleRecord( istream & in, std::string & buf, ErrorDescriptor * err );

// this includes entity names
extern SCL_CORE_EXPORT const char * ReadStdKeyword( istream & in, std::string & buf, int skipInitWS = 1 );

extern SCL_CORE_EXPORT const char * GetKeyword( istream & in, const char * delims, ErrorDescriptor & err );

extern SCL_CORE_EXPORT int FoundEndSecKywd( istream & in, ErrorDescriptor & err );

extern SCL_CORE_EXPORT const char * ReadComment( std::string & ss, const char * s );

extern SCL_CORE_EXPORT const char * ReadComment( istream & in, std::string & s );

extern SCL_CORE_EXPORT Severity    ReadPcd( istream & in ); //print control directive

extern SCL_CORE_EXPORT void        ReadTokenSeparator( istream & in, std::string * comments = 0 );

#endif
