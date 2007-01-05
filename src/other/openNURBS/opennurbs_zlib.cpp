/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"

bool ON_BinaryArchive::WriteCompressedBuffer(
        size_t sizeof__inbuffer,  // sizeof uncompressed input data
        const void* inbuffer  // uncompressed input data
        )
{
  size_t compressed_size = 0;
  bool rc = false;

  if ( !WriteMode() )
    return false;
  if ( sizeof__inbuffer > 0 && 0 == inbuffer )
    return false;


  // number of bytes of uncompressed data

  if (!WriteSize(sizeof__inbuffer))
    return false;
  if ( 0 == sizeof__inbuffer )
    return true;

  // 32 bit crc of uncompressed data
  const unsigned int buffer_crc = ON_CRC32( 0, sizeof__inbuffer, inbuffer );
  if (!WriteInt(buffer_crc))
    return false;

  unsigned char method = (sizeof__inbuffer > 128) ? 1 : 0;
  if ( method ) {
    if ( !CompressionInit() ) {
      CompressionEnd();
      method = 0;
    }
  }
  if ( !WriteChar(method) )
    return false;

  switch ( method )
  {
  case 0: // uncompressed
    rc = WriteByte(sizeof__inbuffer, inbuffer);
    if ( rc )
    {
      compressed_size = sizeof__inbuffer;
    }
    break;

  case 1: // compressed
    compressed_size = WriteDeflate( sizeof__inbuffer, inbuffer );
    rc = ( compressed_size > 0 ) ? true : false;
    CompressionEnd();
    break;
  }


  return rc;
}

bool ON_BinaryArchive::ReadCompressedBufferSize( size_t* sizeof__outbuffer )
{
  return ReadSize(sizeof__outbuffer);
}

bool ON_BinaryArchive::ReadCompressedBuffer( // read and uncompress
  size_t sizeof__outbuffer,  // sizeof of uncompressed buffer to read
  void* outbuffer,           // uncompressed output data returned here
  int* bFailedCRC
  )
{
  bool rc = false;
  unsigned int buffer_crc0 = 0;
  unsigned int buffer_crc1 = 0;
  char method = 0;

  if ( bFailedCRC)
    *bFailedCRC = false;
  if ( !ReadMode() )
    return false;
  if ( 0 == sizeof__outbuffer )
    return true;
  if ( 0 == outbuffer )
    return false;

  if ( !ReadInt(&buffer_crc0) ) // 32 bit crc of uncompressed buffer
    return false;

  if ( !ReadChar(&method) )
    return false;

  if ( method != 0 && method != 1 )
    return false;

  switch(method)
  {
  case 0: // uncompressed
    rc = ReadByte(sizeof__outbuffer, outbuffer);
    break;
  case 1: // compressed
    rc = CompressionInit();
    if (rc)
      rc = ReadInflate( sizeof__outbuffer, outbuffer );
    CompressionEnd();
    break;
  }

  if (rc ) 
  {
    buffer_crc1 = ON_CRC32( 0, sizeof__outbuffer, outbuffer );
    if ( buffer_crc1 != buffer_crc1 ) 
    {
      ON_ERROR("ON_BinaryArchive::ReadCompressedBuffer() crc error");
      if ( bFailedCRC )
        *bFailedCRC = true;
    }
  }

  return rc;
}

size_t ON_BinaryArchive::WriteDeflate( // returns number of bytes written
        size_t sizeof___inbuffer,  // sizeof uncompressed input data ( > 0 )
        const void* in___buffer     // uncompressed input data ( != NULL )
        )
{
  /*
    In "standard" (in 2005) 32 bit code
    
      sizeof(int)     = 4 bytes, 
      sizeof(long)    = 4 bytes,
      sizeof(pointer) = 4 bytes, and
      sizeof(size_t)  = 4 bytes.

    Theoretically I don't need to use multiple input buffer
    chunks in case.  But I'm paranoid and I will use multiple 
    input chunks when sizeof_inbuffer > 2GB in order to dodge
    any potential zlib signed verses unsigned compare bugs or
    having a signed int i++ roll over to a negative number.

    In "standard" code that has 64 bit pointers
    
      sizeof(int)     >= 4 bytes, (it's 4 on MS VS2005) 
      sizeof(long)    >= 4 bytes, (it's 4 on MS VS2005)
      sizeof(pointer)  = 8 bytes, and
      sizeof(size_t)   = 8 bytes.

    So, I'm going to assume the ints and longs in the zlib code 
    are 4 bytes, but I could have sizeof_inbuffer > 4GB.
    This means I have to use multiple input buffer chunks.  
    In this case I still use multiple input chunks when 
    sizeof_inbuffer > 2GB in order to dodge any potential zlib
    signed verses unsigned compare bugs or having a signed
    int i++ roll over to a negative number.

    So, I set
    
       const size_t max_avail = (largest signed 4 byte integer - 15)
    
    and feed inflate and deflate buffers with size <= max_avail.


    This information below is from the zlib 1.2.3 FAQ.  

    32. Can zlib work with greater than 4 GB of data?

        Yes. inflate() and deflate() will process any amount of data correctly.
        Each call of inflate() or deflate() is limited to input and output chunks
        of the maximum value that can be stored in the compiler's "unsigned int"
        type, but there is no limit to the number of chunks. Note however that the
        strm.total_in and strm_total_out counters may be limited to 4 GB. These
        counters are provided as a convenience and are not used internally by
        inflate() or deflate(). The application can easily set up its own counters
        updated after each call of inflate() or deflate() to count beyond 4 GB.
        compress() and uncompress() may be limited to 4 GB, since they operate in a
        single call. gzseek() and gztell() may be limited to 4 GB depending on how
        zlib is compiled. See the zlibCompileFlags() function in zlib.h.

        The word "may" appears several times above since there is a 4 GB limit
        only if the compiler's "long" type is 32 bits. If the compiler's "long"
        type is 64 bits, then the limit is 16 exabytes.
  */

  const size_t max_avail = 0x7FFFFFF0;

  //  Compressed information is saved in a chunk.
  bool rc = BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,0);
  if ( !rc )
    return false;

  size_t out__count = 0;
  int zrc = Z_OK;

  size_t my_avail_in = sizeof___inbuffer;
  unsigned char* my_next_in = (unsigned char*)in___buffer;

  size_t d = my_avail_in;
  if ( d > max_avail )
    d = max_avail;
  m_zlib.strm.next_in = my_next_in;
  m_zlib.strm.avail_in = (unsigned int)d; 
  my_avail_in -= d;
  my_next_in  += d;

  m_zlib.strm.next_out = m_zlib.buffer;
  m_zlib.strm.avail_out = m_zlib.sizeof_x_buffer;

  // counter guards prevents infinte loops if there is a bug in zlib return codes.
  int counter = 512; 
  int flush = Z_NO_FLUSH;

  size_t deflate_output_count = 0;

  while( rc && counter > 0 ) 
  {
    // Call zlib's deflate function.  It can either process
    // more input from m_zlib.strm.next_in[], create more
    // compressed output in m_zlib.strm.next_out[], or do both.
    if ( 0 == my_avail_in && 0 == m_zlib.strm.avail_in )
    {
      // no uncompressed input is left - switch to finish mode
      flush = Z_FINISH;
    }
    zrc = z_deflate( &m_zlib.strm, flush ); 
    if ( zrc < 0 ) 
    {
      // Something went haywire - bail out.
      ON_ERROR("ON_BinaryArchive::WriteDeflate - z_deflate failure");
      rc = false;
      break;
    }

    deflate_output_count = m_zlib.sizeof_x_buffer - m_zlib.strm.avail_out;
    if ( deflate_output_count > 0 ) 
    {
      // The last call to deflate created output.  Send
      // this output to the archive.
      rc = WriteChar( deflate_output_count, m_zlib.buffer );
      if ( !rc )
        break;
      out__count += deflate_output_count;
      m_zlib.strm.next_out  = m_zlib.buffer;
      m_zlib.strm.avail_out = m_zlib.sizeof_x_buffer;
    }

    if ( Z_FINISH == flush && Z_STREAM_END == zrc )
    {
      // no input left, all pending compressing is finished,
      // and all compressed output has been returned.
      break;
    }

    if ( my_avail_in > 0 && m_zlib.strm.avail_in < max_avail )
    {
      // inbuffer[] had more than max_zlib_avail_in bytes in it
      // and I am feeding inbuffer[] to deflate in smaller chunks
      // that the 32 bit integers in the zlib code can handle.
      if ( 0 == m_zlib.strm.avail_in || 0 == m_zlib.strm.next_in )
      {
        // The call to deflate() used up all the input 
        // in m_zlib.strm.next_in[].  I can feed it another chunk
        // from inbuffer[]
        d = my_avail_in;
        if ( d > max_avail )
          d = max_avail;
        m_zlib.strm.next_in = my_next_in;
        m_zlib.strm.avail_in = (unsigned int)d; 
      }
      else
      {
        // The call to deflate left some input in m_zlib.strm.next_in[],
        // but I can increase m_zlib.strm.avail_in.
        d =  max_avail - m_zlib.strm.avail_in;
        if ( d > my_avail_in )
          d = my_avail_in;
        m_zlib.strm.avail_in += (unsigned int)d;
      }

      my_avail_in -= d;
      my_next_in  += d;
    }
    else if ( 0 == deflate_output_count )
    {
      // no buffer changes this time
      counter--;
    }

    if ( zrc != Z_OK )
    {
      break;
    }
  }

  if ( !EndWrite3dmChunk() )
  {
    rc = false;
  }

  if ( 0 == counter )
  {
    rc = false;
  }

  return (rc ? out__count : 0);
}


bool ON_BinaryArchive::ReadInflate(
        size_t sizeof___outbuffer,  // sizeof uncompressed data
        void* out___buffer          // buffer for uncompressed data
        )
{
  const size_t max_avail = 0x7FFFFFF0; // See max_avail comment in ON_BinaryArchive::WriteInflate

  size_t sizeof__inbuffer = 0;
  void* in___buffer = 0;
  bool rc = false;

  // read compressed buffer from 3dm archive
  bool bValidCompressedBuffer = false;
  {
    unsigned int tcode = 0;
    unsigned int value = 0; // value must be unsigned
    rc = BeginRead3dmChunk(&tcode,(int*)(&value) );
    if (!rc)
    {
      if ( 0 != out___buffer && sizeof___outbuffer > 0 )
        memset(out___buffer,0,sizeof___outbuffer);
      return false;
    }
    if (   tcode == TCODE_ANONYMOUS_CHUNK 
        && value > 4 
        && sizeof___outbuffer > 0 
        && 0 != out___buffer )
    {
      // read compressed buffer from the archive
      sizeof__inbuffer = value-4; // the last 4 bytes in this chunk are a 32 bit crc
      in___buffer = onmalloc(sizeof__inbuffer);
      if ( !in___buffer )
      {
        rc = false;
      }
      else
      {
        rc = ReadByte( sizeof__inbuffer, in___buffer );
      }
    }
    else
    {
      // Either I have the wrong chunk, or the input
      // parameters are bogus. 
      rc = false;
    }
    int c0 = m_bad_CRC_count;
    if ( !EndRead3dmChunk() )
    {
      rc = false;
    }
    bValidCompressedBuffer = ( m_bad_CRC_count > c0 )
                           ? false
                           : rc;
  }

  if ( !bValidCompressedBuffer && 0 != out___buffer && sizeof___outbuffer > 0 )
  {
    // Decompression will fail, but we might get something valid
    // at the start if the data flaw was near the end of the buffer.
    memset(out___buffer,0,sizeof___outbuffer);
  }

  if ( !rc )
  {
    if ( in___buffer )
    {
      onfree(in___buffer);
      in___buffer = 0;
    }
    return false;
  }

  int zrc = -1;

  // set up zlib in buffer
  unsigned char* my_next_in = (unsigned char*)in___buffer;
  size_t my_avail_in = sizeof__inbuffer;

  size_t d = my_avail_in;
  if ( d > max_avail )
    d = max_avail;
  m_zlib.strm.next_in  = my_next_in;
  m_zlib.strm.avail_in = (unsigned int)d;
  my_next_in  += d;
  my_avail_in -= d;

  // set up zlib out buffer
  unsigned char* my_next_out = (unsigned char*)out___buffer;
  size_t my_avail_out = sizeof___outbuffer;

  d = my_avail_out;
  if ( d > max_avail )
    d = max_avail;
  m_zlib.strm.next_out  = my_next_out;
  m_zlib.strm.avail_out = (unsigned int)d;
  my_next_out  += d;
  my_avail_out -= d;

  // counter guards against infinte loop if there are
  // bugs in zlib return codes
  int counter = 512;
  int flush = Z_NO_FLUSH;

  while ( rc && counter > 0 )
  {
    // Call zlib's inflate function.  It can either process
    // more input from m_zlib.strm.next_in[], create more
    // uncompressed output in m_zlib.strm.next_out[], or do both.
    if ( 0 == my_avail_in && 0 == m_zlib.strm.avail_in )
    {
      // no compressed input is left - switch to finish mode
      flush = Z_FINISH;
    }
    zrc = z_inflate( &m_zlib.strm, flush );
    if ( zrc < 0 ) 
    {
      // Something went haywire - bail out.
      ON_ERROR("ON_BinaryArchive::ReadInflate - z_inflate failure");
      rc = false;
      break;
    }

    if ( Z_FINISH == flush && Z_STREAM_END == zrc )
    {
      // no input left, all pending decompression is finished,
      // and all decompressed output has been returned.
      break;
    }

    d = 0;
    if ( my_avail_in > 0 && m_zlib.strm.avail_in < max_avail )
    {
      if ( 0 == m_zlib.strm.avail_in || 0 == m_zlib.strm.next_in )
      {
        // The call to inflate() used up all the input 
        // in m_zlib.strm.next_in[].  I can feed it another chunk
        // from inbuffer[]
        d = my_avail_in;
        if ( d > max_avail )
          d = max_avail;
        m_zlib.strm.next_in  = my_next_in;
        m_zlib.strm.avail_in = (unsigned int)d; 
      }
      else
      {
        // The call to inflate() left some input in m_zlib.strm.next_in[],
        // but I can increase m_zlib.strm.avail_in.
        d =  max_avail - m_zlib.strm.avail_in;
        if ( d > my_avail_in )
          d = my_avail_in;
        m_zlib.strm.avail_in += (unsigned int)d;
      }
      my_next_in  += d;
      my_avail_in -= d;
    }

    if ( my_avail_out > 0 && m_zlib.strm.avail_out < max_avail )
    {
      // increase m_zlib.strm.next_out[] buffer
      if ( 0 == m_zlib.strm.avail_out || 0 == m_zlib.strm.next_out )
      {
        d = my_avail_out;
        if ( d > max_avail )
          d = max_avail;
        m_zlib.strm.next_out  = my_next_out;
        m_zlib.strm.avail_out = (unsigned int)d;
      }
      else
      {
        d = max_avail - m_zlib.strm.avail_out;
        if ( d > my_avail_out )
          d = my_avail_out;
        m_zlib.strm.avail_out += ((unsigned int)d);
      }
      my_next_out  += d;
      my_avail_out -= d;
    }
    else if ( 0 == d )
    {
      // no buffer changes
      counter--;
    }
  }

  if (in___buffer )
  {
    onfree(in___buffer);
    in___buffer = 0;
  }

  if ( 0 == counter )
  {
    rc = false;
  }

  return rc;
}

bool ON_BinaryArchive::CompressionInit()
{
  // inflateInit() and deflateInit() are in zlib 1.3.3
  bool rc = false;
  if ( WriteMode() ) {
    rc = ( m_zlib.mode == ON::write ) ? true : false;
    if ( !rc ) {
      CompressionEnd();
      if ( Z_OK == deflateInit( &m_zlib.strm, Z_BEST_COMPRESSION ) ) {
        m_zlib.mode = ON::write;
        rc = true;
      }
      else {
        memset(&m_zlib.strm,0,sizeof(m_zlib.strm));
      }
    }
  }
  else if ( ReadMode() ) {
    rc = ( m_zlib.mode == ON::read ) ? true : false;
    if ( !rc ) {
      CompressionEnd();
      if ( Z_OK == inflateInit( &m_zlib.strm ) ) {
        m_zlib.mode = ON::read;
        rc = true;
      }
      else {
        memset(&m_zlib.strm,0,sizeof(m_zlib.strm));
      }
    }
  }
  else {
    CompressionEnd();
  }
  return rc;
}

void ON_BinaryArchive::CompressionEnd()
{
  // inflateEnd() and deflateEnd() are in zlib 1.3.3
  switch ( m_zlib.mode ) {
  case ON::read:
  case ON::read3dm:
    inflateEnd(&m_zlib.strm);
    break;
  case ON::write:
  case ON::write3dm:
    deflateEnd(&m_zlib.strm);
    break;
  default: // to quiet lint
    break;
  }
  memset(&m_zlib.strm,0,sizeof(m_zlib.strm));
  m_zlib.mode = ON::unknown_archive_mode;
}


