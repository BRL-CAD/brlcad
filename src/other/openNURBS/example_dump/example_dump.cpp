#include "../opennurbs.h"
#include "../opennurbs_staticlib_linking_pragmas.h"

ON_BOOL32 Dump3dmFile( 
        const char*, // full name of file
        ON_TextLog&
        );

static ON_BOOL32 bTerseReport = 0;

int main ( int argc, const char* argv[] )
{
  ON::Begin();

  // default dump is to stdout
  // use the -out:filename.txt option to dump to a file
  ON_TextLog dump_to_stdout;
  ON_TextLog* dump = &dump_to_stdout;
  FILE* dump_fp = 0;
  
  dump->SetIndentSize(2);

  int argi;
  if ( argc < 2 ) 
  {
    printf("Syntax: %s [-out:outputfilename.txt] [-terse] file1.3dm file2.3dm ...\n",argv[0]);
    return 0;
  }

  for ( argi = 1; argi < argc; argi++ ) 
  {
    const char* arg = argv[argi];
    if ( 0 == arg )
      continue;

    // check for -terse option
    if ( 0 == strcmp( arg, "-terse" ) ) 
    {
      bTerseReport = 1;
      continue;
    }

    // check for -out or /out option
    if ( ( 0 == strncmp(arg,"-out:",5) || 0 == strncmp(arg,"/out:",5) ) 
         && arg[5] )
    {
      // change destination of dump file
      const char* sDumpFilename = arg+5;
      FILE* text_fp = ON::OpenFile(sDumpFilename,"w");
      if ( text_fp )
      {
        if ( dump_fp )
        {
          delete dump;
          dump = 0;
          ON::CloseFile(dump_fp);
          dump_fp = 0;
        }
        dump_fp = text_fp;
        text_fp = 0;
        dump = new ON_TextLog(dump_fp);
      }
      continue;
    }

    Dump3dmFile( arg, *dump );
    dump->Print("\n\n");
  }

  if ( dump_fp )
  {
    // close the text dump file
    delete dump;
    dump = 0;
    ON::CloseFile( dump_fp );
    dump_fp = 0;
  }

  // OPTIONAL: Call just before your application exits to clean
  //           up opennurbs class definition information.
  //           Opennurbs will not work correctly after ON::End()
  //           is called.
  ON::End();

  return 0;
}

static void ErrorReport( size_t offset, const char* msg, ON_TextLog& dump )
{
  int i = (int)offset;
  dump.Print("** ERROR near offset %d ** %s\n",i,msg);
}

static 
bool Read3dmUserDataHeader( const size_t offset, ON_BinaryArchive& file, ON_TextLog& dump )
{
  // TCODE_OPENNURBS_CLASS_USERDATA chunks have 2 uuids
  // the first identifies the type of ON_Object class
  // the second identifies that kind of user data
  ON_UUID userdata_classid = ON_nil_uuid;
  ON_UUID userdata_itemid = ON_nil_uuid;
  bool rc = file.ReadUuid( userdata_classid );
  if ( !rc ) 
  {
     ErrorReport(offset,"ReadUuid() failed to read the user data class id.",dump);
  }
  else 
  {
    dump.Print("UserData class id = ");
    dump.Print( userdata_classid );
    const ON_ClassId* pUserDataClassId = ON_ClassId::ClassId(userdata_classid);
    if ( pUserDataClassId )
    {
      const char* sClassName = pUserDataClassId->ClassName();
      if ( sClassName )
      {
        dump.Print(" (%s)",sClassName);
      }
    }
    dump.Print("\n");

    rc = file.ReadUuid( userdata_itemid );
    if ( !rc ) 
    {
       ErrorReport(offset,"ReadUuid() failed to read the user data item id.",dump);
    }
    else 
    {
      dump.Print("UserData item id = ");
      dump.Print( userdata_itemid );
      dump.Print("\n");

      int userdata_copycount = -1;
      rc = file.ReadInt( &userdata_copycount );
      if ( !rc )
      {
        ErrorReport(offset,"ReadInt() failed to read the user data copy count.",dump);
      }
      else
      {
        ON_Xform userdata_xform;
        rc = file.ReadXform( userdata_xform );
        if ( !rc )
        {
          ErrorReport(offset,"ReadXform() failed to read the user data xform.",dump);
        }
      }
    }
  }
  return rc;
}

unsigned int Dump3dmChunk( ON_BinaryArchive& file, ON_TextLog& dump, int recursion_depth )
{
  ON_BOOL32 bShortChunk = false;
  const size_t offset0 = file.CurrentPosition();
  unsigned int typecode = 0;
  const char* typecode_name = 0;
  int value;
  ON_BOOL32 rc = file.BeginRead3dmChunk( &typecode, &value );
  if (!rc) {
    ErrorReport(offset0,"BeginRead3dmChunk() failed.",dump);
  }
  else 
  {
    if ( !typecode ) 
    {
      ErrorReport(offset0,"BeginRead3dmChunk() returned typecode = 0.",dump);
      file.EndRead3dmChunk();
      return 0;
    }
    else 
    {
      typecode_name = ON_BinaryArchive::TypecodeName(typecode);
      if ( 0 == typecode_name )
        typecode_name = "unknown typecode";

      if ( 0 == recursion_depth )
      {
        dump.Print("\n");
      }

      bShortChunk = (0 != (typecode & TCODE_SHORT));
      if ( bShortChunk )
      {
        dump.Print("%6d: %08X %s: value = %d (%08X)\n", offset0, typecode, typecode_name, value, value );
      }
      else 
      {
        // long chunk value = length of chunk data
        if ( value < 0 ) 
        {
          ErrorReport(offset0,"BeginRead3dmChunk() returned length < 0.",dump);
          file.EndRead3dmChunk();
          return 0;
        }
        dump.Print("%6d: %08X %s: length = %d bytes\n", offset0, typecode, typecode_name, value );
      }

      int major_userdata_version = -1;
      int minor_userdata_version = -1;

      switch( typecode ) 
      {
      case TCODE_PROPERTIES_TABLE:
      case TCODE_SETTINGS_TABLE:
      case TCODE_BITMAP_TABLE:
      case TCODE_MATERIAL_TABLE:
      case TCODE_LAYER_TABLE:
      case TCODE_GROUP_TABLE:
      case TCODE_LIGHT_TABLE:
      case TCODE_FONT_TABLE:
      case TCODE_DIMSTYLE_TABLE:
      case TCODE_HATCHPATTERN_TABLE:
      case TCODE_LINETYPE_TABLE:
      case TCODE_TEXTURE_MAPPING_TABLE:
      case TCODE_HISTORYRECORD_TABLE:
      case TCODE_USER_TABLE:
      case TCODE_INSTANCE_DEFINITION_TABLE:
      case TCODE_OBJECT_TABLE:
        // start of a table
        {
          dump.PushIndent();
          unsigned int record_typecode = 0;
          for (;;) {
            record_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
            if ( !record_typecode ) {
              break;
            }
            if ( TCODE_ENDOFTABLE == record_typecode ) {
              break;
            }
          }
          dump.PopIndent();
        }
        break;

      case TCODE_BITMAP_RECORD:
        {
          dump.PushIndent();
          unsigned int bitmap_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = bitmap_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_MATERIAL_RECORD:
        {
          dump.PushIndent();
          unsigned int material_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = material_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_LAYER_RECORD:
        {
          dump.PushIndent();
          unsigned int layer_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = layer_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_GROUP_RECORD:
        {
          dump.PushIndent();
          unsigned int group_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = group_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_FONT_RECORD:
        {
          dump.PushIndent();
          unsigned int font_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = font_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_DIMSTYLE_RECORD:
        {
          dump.PushIndent();
          unsigned int dimstyle_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = dimstyle_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_LIGHT_RECORD:
        {
          dump.PushIndent();
          unsigned int light_chunk_typecode = 0;
          for (;;) {
            light_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
            if ( !light_chunk_typecode ) {
              break;
            }
            if ( TCODE_LIGHT_RECORD_END == light_chunk_typecode ) {
              break;
            }
            switch( light_chunk_typecode ) {
            //case TCODE_OBJECT_RECORD_TYPE:
            case TCODE_LIGHT_RECORD_ATTRIBUTES:
            case TCODE_LIGHT_RECORD_ATTRIBUTES_USERDATA:
            case TCODE_OPENNURBS_CLASS:
              break;
            default:
              {
                ErrorReport(offset0,"Rogue chunk in light record.",dump);
              }
            }
          }
          dump.PopIndent();
        }
        break;

        break;

      //case TCODE_LIGHT_RECORD_ATTRIBUTES:
      //  {
      //    dump.PushIndent();
      //    unsigned int light_attributes_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
      //    dump.PopIndent();
      //  }
      //  break;

      case TCODE_TEXTURE_MAPPING_RECORD:
        {
          dump.PushIndent();
          unsigned int mapping_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = mapping_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_HISTORYRECORD_RECORD:
        {
          dump.PushIndent();
          unsigned int history_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = history_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_HATCHPATTERN_RECORD:
        {
          dump.PushIndent();
          unsigned int hatch_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
          if ( !typecode )
            typecode = hatch_chunk_typecode;
          dump.PopIndent();
        }
        break;

      case TCODE_OBJECT_RECORD:
        {
          dump.PushIndent();
          unsigned int object_chunk_typecode = 0;
          for (;;) {
            object_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1 );
            if ( !object_chunk_typecode ) {
              break;
            }
            if ( TCODE_OBJECT_RECORD_END == object_chunk_typecode ) {
              break;
            }
            switch( object_chunk_typecode ) {
            case TCODE_OBJECT_RECORD_TYPE:
            case TCODE_OBJECT_RECORD_ATTRIBUTES:
            case TCODE_OPENNURBS_CLASS:
              break;
            default:
              {
                ErrorReport(offset0,"Rogue chunk in object record.",dump);
              }
            }
          }
          dump.PopIndent();
        }
        break;

      case TCODE_OPENNURBS_CLASS:
        {
          dump.PushIndent();
          unsigned int opennurbs_object_chunk_typecode = 0;
          for (;;) {
            opennurbs_object_chunk_typecode = Dump3dmChunk( file, dump, recursion_depth+1  );
            if ( !opennurbs_object_chunk_typecode ) {
              break;
            }
            if ( TCODE_OPENNURBS_CLASS_END == opennurbs_object_chunk_typecode ) {
              break;
            }
            switch( opennurbs_object_chunk_typecode ) 
            {
            case TCODE_OPENNURBS_CLASS_UUID:
              break;
            case TCODE_OPENNURBS_CLASS_DATA:
              break;
            case TCODE_OPENNURBS_CLASS_USERDATA:
              break;
            default:
              {
                ErrorReport(offset0,"Rogue chunk in OpenNURBS class record.",dump);
              }
            }
          }
          dump.PopIndent();
        }
        break;

      case TCODE_OPENNURBS_CLASS_USERDATA:
        {
          if ( !file.Read3dmChunkVersion(&major_userdata_version, &minor_userdata_version ) )
          {
            ErrorReport(offset0,"Read3dmChunkVersion() failed to read TCODE_OPENNURBS_CLASS_USERDATA chunk version.",dump);
          }
          else
          {
            dump.PushIndent();
            dump.Print("UserData chunk version: %d.%d\n",
                       major_userdata_version,
                       minor_userdata_version
                       );
            if ( 1 == major_userdata_version || 2 == major_userdata_version )
            {
              const size_t userdata_header_offset = file.CurrentPosition();
              switch ( major_userdata_version )
              {
              case 1:
                {
                  // version 1 user data header information was not wrapped
                  // in a chunk.
                  if ( Read3dmUserDataHeader( userdata_header_offset, file, dump ) )
                  {
                    // a TCODE_ANONYMOUS_CHUNK contains user data goo
                    int anon_typecode =  Dump3dmChunk(file, dump, recursion_depth+1 );
                    if ( TCODE_ANONYMOUS_CHUNK != anon_typecode )
                    {
                      ErrorReport( offset0,"Userdata Expected a TCODE_ANONYMOUS_CHUNK chunk.",dump);
                    }
                  }
                }
                break;
              case 2:
                {
                  // version 2 user data header information is wrapped
                  // in a TCODE_OPENNURBS_CLASS_USERDATA_HEADER chunk.
                  unsigned int userdata_header_typecode = Dump3dmChunk(file, dump, recursion_depth+1 );
                  if ( TCODE_OPENNURBS_CLASS_USERDATA_HEADER != userdata_header_typecode )
                  {
                    ErrorReport(userdata_header_offset,"Expected a TCODE_OPENNURBS_CLASS_USERDATA_HEADER chunk.",dump);
                  }
                  else
                  {
                    int anon_typecode =  Dump3dmChunk(file, dump, recursion_depth+1 );
                    if ( TCODE_ANONYMOUS_CHUNK != anon_typecode )
                    {
                      ErrorReport( offset0,"Userdata Expected a TCODE_ANONYMOUS_CHUNK chunk.",dump);
                    }
                  }
                }
                break;
              default:
                if ( major_userdata_version < 3 )
                {
                }
                else
                {
                  dump.Print("New user data format created after this diagnostic tool was written.\n");
                }
                break;
              }
            }

            dump.PopIndent();
          }
        }
        break;

      case TCODE_OPENNURBS_CLASS_UUID:
      case TCODE_USER_TABLE_UUID:
        {
          dump.PushIndent();
          ON_UUID uuid = ON_nil_uuid;
          const ON_ClassId* pClassId = 0;
          if ( !file.ReadUuid( uuid ) ) {
             ErrorReport(offset0,"ReadUuid() failed.",dump);
          }
          else 
          {
            if ( typecode == TCODE_OPENNURBS_CLASS_UUID ) 
            {
              dump.Print("OpenNURBS class id = ");
              pClassId = ON_ClassId::ClassId(uuid);
            }
            else if ( typecode == TCODE_USER_TABLE_UUID ) 
            {
              dump.Print("User table id = ");
            }
            else {
              dump.Print("UUID = ");
            }
            dump.Print( uuid );
            if ( pClassId )
            {
              const char* sClassName = pClassId->ClassName();
              if ( sClassName )
              {
                dump.Print(" (%s)",sClassName);
              }
            }
            dump.Print("\n");
          }

          dump.PopIndent();
        }
        break;

      case TCODE_OPENNURBS_CLASS_USERDATA_HEADER:
        {
          if ( !Read3dmUserDataHeader( offset0, file, dump ) )
          {
            ErrorReport(offset0,"Unable to read userdata header.",dump);
          }
        }
        break;

      case TCODE_ENDOFFILE:
      case TCODE_ENDOFFILE_GOO:
        {
          dump.PushIndent();
          if ( value < 4 ) {
            ErrorReport(offset0,"TCODE_ENDOFFILE chunk withlength < 4.",dump);
          }
          else {
            int sizeof_file = 0;
            file.ReadInt(&sizeof_file);
            dump.Print("current position = %d  stored size = %d\n",
                       file.CurrentPosition(), 
                       sizeof_file);
          }
          dump.PopIndent();
        }
        break;

      }
    }

    const size_t offset1 = file.CurrentPosition();
    if ( !file.EndRead3dmChunk() ) 
    {
      ErrorReport(offset1,"EndRead3dmChunk() failed.",dump);
      rc = false;
    }
    else if (!bShortChunk) 
    {
      const size_t extra = value - (offset1-offset0-8);
      if ( extra < 0 ) {
        ErrorReport(offset0,"Read beyond end of chunk.",dump);
      }
    }
  }
  return typecode;
}


ON_BOOL32 Dump3dmFile( const char* sFileName, ON_TextLog& dump )
{
  dump.Print("====== FILENAME: %s\n",sFileName);
  ON_Workspace ws;
  FILE* fp = ws.OpenFile( sFileName, "rb" ); // file automatically closed by ~ON_Workspace()
  if ( !fp ) {
    dump.Print("**ERROR** Unable to open file.\n");
    return false;
  }

  ON_BinaryFile file( ON::read3dm, fp );

  int version = 0;
  ON_String comment_block;
  ON_BOOL32 rc = file.Read3dmStartSection( &version, comment_block );
  if (!rc) {
    dump.Print("**ERROR** Read3dmStartSection() failed\n");
    return false;
  }
  dump.Print("====== VERSION: %d\n",version );
  dump.Print("====== COMMENT BLOCK:\n",version );
  dump.PushIndent();
  dump.Print(comment_block);
  dump.PopIndent();
  dump.Print("====== CHUNKS:\n",version );
  unsigned int typecode;
  while ( !file.AtEnd() ) {
    typecode = Dump3dmChunk( file, dump, 0 );
    if ( !typecode )
      break;
    if ( typecode == TCODE_ENDOFFILE )
      break;
  }
  dump.Print("====== FINISHED: %s\n",sFileName);

  return true;
}

