/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2011 Robert McNeel & Associates. All rights reserved.
// OpenNURBS, Rhinoceros, and Rhino3D are registered trademarks of Robert
// McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/		

////////////////////////////////////////////////////////////////
//
//  example_read.cpp  
// 
//  Example program using the Rhino file IO toolkit.  The program reads in  
//  a Rhino 3dm model file and describes its contents.  The program is a 
//  console application that takes a filename as a command line argument.
//
////////////////////////////////////////////////////////////////////////

#include "../opennurbs_public_examples.h"

#include "../example_userdata/example_ud.h"

static bool Dump3dmFileHelper( 
        const wchar_t* sFileName, // full name of file
        ON_TextLog& dump
        )
{
  dump.Print("====== FILENAME: %ls\n",sFileName);
  ON_Workspace ws;
  FILE* fp = ws.OpenFile( sFileName, L"rb" ); // file automatically closed by ~ON_Workspace()
  if ( !fp ) {
    dump.Print("**ERROR** Unable to open file.\n");
    return false;
  }

  ON_BinaryFile file( ON::archive_mode::read3dm, fp );

  int version = 0;
  ON_String comment_block;
  bool rc = file.Read3dmStartSection( &version, comment_block );
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
    typecode = file.Dump3dmChunk( dump, 0 );
    if ( !typecode )
      break;
    if ( typecode == TCODE_ENDOFFILE )
      break;
  }
  dump.Print("====== FINISHED: %ls\n",sFileName);

  return true;
}

/*
Returns:
  True if .3dm file was successfully read into an ONX_Model class.
*/
static bool ReadFileHelper( 
  const wchar_t* sFileName,
  bool bVerboseTextDump,
  bool bChunkDump,
  ON_TextLog& dump
  )
{
  if ( bChunkDump )
  {
    return Dump3dmFileHelper(sFileName,dump);
  }

  ONX_Model model;

  dump.Print("\nOpenNURBS Archive File:  %ls\n", sFileName );

  // open file containing opennurbs archive
  FILE* archive_fp = ON::OpenFile( sFileName, L"rb");
  if ( !archive_fp ) 
  {
    dump.Print("  Unable to open file.\n" );
    return false;
  }

  dump.PushIndent();

  // create achive object from file pointer
  ON_BinaryFile archive( ON::archive_mode::read3dm, archive_fp );

  // read the contents of the file into "model"
  bool rc = model.Read( archive, &dump );

  // close the file
  ON::CloseFile( archive_fp );

  // print diagnostic
  if ( rc )
    dump.Print("Successfully read.\n");
  else
    dump.Print("Errors during reading.\n");

  // create a text dump of the model
  if ( bVerboseTextDump )
  {
    dump.PushIndent();
    model.Dump(dump);
    dump.PopIndent();
  }

  dump.PopIndent();

  return rc;
}

/*
Returns:
  Number of files read.
*/
static int ReadDirectoryHelper( 
  int directory_depth,
  int maximum_directory_depth,
  const wchar_t* directory_name,
  const wchar_t* file_name_filter,
  bool bVerboseTextDump,
  bool bChunkDump,
  ON_TextLog& dump
  )
{
  int file_count = 0;
  if ( directory_depth <= maximum_directory_depth )
  {
    if ( 0 == file_name_filter || 0 == file_name_filter[0] )
      file_name_filter = L"*.3dm";

    // read files in this directory
    ON_FileIterator file_it;
    bool bFoundDirectory = false;
    for ( bool bHaveFileSystemItem = (file_it.Initialize( directory_name, file_name_filter ) && file_it.FirstItem());
          bHaveFileSystemItem;
          bHaveFileSystemItem = file_it.NextItem()
        )
    {
      if (file_it.CurrentItemIsDirectory())
      {
        bFoundDirectory = true;
        continue;
      }

      if ( false == file_it.CurrentItemIsFile() )
        continue;      
      
      if ( file_it.CurrentItemIsHidden() )
        continue;

      ON_wString full_path(file_it.CurrentItemFullPathName());
      if ( full_path.IsEmpty() )
        continue;
      
      if ( !ON::IsOpenNURBSFile(full_path) )
        continue;

      if ( ReadFileHelper(full_path,bVerboseTextDump,bChunkDump,dump) )
        file_count++;
    }

    // read files in subdirectories
    if ( bFoundDirectory && directory_depth < maximum_directory_depth )
    {
      ON_FileIterator dir_it;
      for ( bool bHaveFileSystemItem = (dir_it.Initialize( directory_name, nullptr ) && dir_it.FirstItem());
            bHaveFileSystemItem;
            bHaveFileSystemItem = dir_it.NextItem()
          )
      {
        if ( false == dir_it.CurrentItemIsDirectory() )
          continue;

        if ( dir_it.CurrentItemIsHidden() )
          continue;

        ON_wString full_path(dir_it.CurrentItemFullPathName());
        if ( full_path.IsEmpty() )
          continue;
      
        file_count += ReadDirectoryHelper(
                            directory_depth + 1,
                            maximum_directory_depth,
                            full_path,
                            file_name_filter,
                            bVerboseTextDump,
                            bChunkDump,
                            dump
                            );
      }
    }
  }

  return file_count;
}


static void print_help(const char* example_read_exe_name)
{
  if ( 0 == example_read_exe_name || 0 == example_read_exe_name[0])
    example_read_exe_name = "example_read";

  printf("\n");
  printf("SYNOPSIS:\n");
  printf("  %s [-out:outputfilename.txt] [-c] [-r] <file or directory names>\n",example_read_exe_name );
  printf("\n");
  printf("DESCRIPTION:\n");
  printf("  If a file is listed, it is read as an opennurbs model file.\n");
  printf("  If a directory is listed, all .3dm files in that directory\n");
  printf("  are read as opennurbs model files.\n");
  printf("\n");
  printf("  Available options:\n");
  printf("    -out:outputfilename.txt\n");
  printf("      The output is written to the named file.\n");
  printf("    -chunkdump\n");
  printf("      Does a chunk dump instead of reading the file's contents.\n");
  printf("    -recursive\n");
  printf("      Recursivly reads files in subdirectories.\n");
  printf("\n");
  printf("EXAMPLE:\n");
  printf("  %s -out:list.txt -resursive .../example_files\n",example_read_exe_name);
  printf("  with read all the opennurbs .3dm files in the\n"); 
  printf("  example_files/ directory and subdirectories.\n");
}


#if defined(ON_COMPILER_MSC)

// When you run a C program, you can use either of the two wildcards
// the question mark (?) and the asterisk (*) to specify filename
// and path arguments on the command line.
// By default, wildcards are not expanded in command-line arguments. 
// You can replace the normal argument vector argv loading routine with 
// a version that does expand wildcards by linking with the setargv.obj or wsetargv.obj file. 
// If your program uses a main function, link with setargv.obj. 
// If your program uses a wmain function, link with wsetargv.obj. 
// Both of these have equivalent behavior.
// To link with setargv.obj or wsetargv.obj, use the /link option. 
//
// For example:
// cl example.c /link setargv.obj
// The wildcards are expanded in the same manner as operating system commands.
// (See your operating system user's guide if you are unfamiliar with wildcards.)

// example_read.vcxproj linkin options include setargv.obj.

#endif

int main( int argc, const char *argv[] )
{
  // If you are using OpenNURBS as a Windows DLL, then you MUST use
  // ON::OpenFile() to open the file.  If you are not using OpenNURBS
  // as a Windows DLL, then you may use either ON::OpenFile() or fopen()
  // to open the file.

  const char* example_read_exe_name = 0;
  if ( argc >= 1 && 0 != argv && 0 != argv[0] && 0 != argv[0][0] )
  {
    on_splitpath(
      argv[0],
      nullptr, // drive
      nullptr, // director,
      &example_read_exe_name,
      nullptr // extension
      );
  }

  if ( 0 == example_read_exe_name || 0 == example_read_exe_name[0] )
  {
#if defined(ON_OS_WINDOWS)
    example_read_exe_name = "example_read.exe";
#else
    example_read_exe_name = "example_read";
#endif
  }

  int argi;
  if ( argc < 2 ) 
  {
    print_help(example_read_exe_name);
    return 0;
  }

  // Call once in your application to initialze opennurbs library
  ON::Begin();

  // default dump is to stdout
  ON_TextLog dump_to_stdout;
  dump_to_stdout.SetIndentSize(2);
  ON_TextLog* dump = &dump_to_stdout;
  FILE* dump_fp = 0;
  
  bool bVerboseTextDump = true;

  bool bChunkDump = false;


  int maximum_directory_depth = 0;

  int file_count = 0;

  for ( argi = 1; argi < argc; argi++ ) 
  {
    const char* arg = argv[argi];

    // check for -out or /out option
    if ( (    0 == strncmp(arg,"-out:",5) || 0 == strncmp(arg,"-out:",5)
#if defined(ON_OS_WINDOWS)
           || 0 == strncmp(arg,"/out:",5) 
#endif
         ) 
         && arg[5] )
    {
      // change destination of dump file
      if ( dump != &dump_to_stdout )
      {
        delete dump;
        dump = 0;
      }
      if ( dump_fp )
      {
        ON::CloseFile(dump_fp);
      }

      const ON_wString sDumpFilename = ON_FileSystemPath::ExpandUser(arg + 5);
      FILE* text_fp = ON::OpenFile(sDumpFilename,L"w");
      if ( text_fp )
      {
        dump_fp = text_fp;
        dump = new ON_TextLog(dump_fp);
        dump->SetIndentSize(2);
      }

      if ( 0 == dump )
        dump = &dump_to_stdout;

      continue;
    }

    // check for -chunkdump or /chunkdump option
    if (    0 == strcmp(arg,"-C") 
         || 0 == strcmp(arg,"-c") 
         || 0 == strcmp(arg,"-chunk") 
         || 0 == strcmp(arg,"-chunkdump") 
#if defined(ON_OS_WINDOWS)
         || 0 == strcmp(arg,"/C") 
         || 0 == strcmp(arg,"/c") 
         || 0 == strcmp(arg,"/chunk") 
         || 0 == strcmp(arg,"/chunkdump") 
#endif
         )
    {
      bChunkDump = true;
      continue;
    }

    // check for -recursive or /recursive option
    if (    0 == strcmp(arg,"-R") 
         || 0 == strcmp(arg,"-r") 
         || 0 == strcmp(arg,"-recurse") 
         || 0 == strcmp(arg,"-recursive") 
#if defined(ON_OS_WINDOWS)
         || 0 == strcmp(arg,"/R") 
         || 0 == strcmp(arg,"/r") 
         || 0 == strcmp(arg,"/recurse") 
         || 0 == strcmp(arg,"/recursive") 
#endif
         )
    {
      maximum_directory_depth = 32;
      continue;
    }

    ON_wString ws_arg = ON_FileSystemPath::ExpandUser(arg);
    const wchar_t* wchar_arg = ws_arg;

    if ( ON::IsDirectory(wchar_arg) )
    {
      file_count += ReadDirectoryHelper( 0, maximum_directory_depth, wchar_arg, 0, bVerboseTextDump, bChunkDump, *dump );
    }
    else
    {
      if ( ReadFileHelper( wchar_arg, bVerboseTextDump, bChunkDump, *dump ) )
        file_count++;
    }    

  }

  dump->Print("%s read %d opennurbs model files.\n",example_read_exe_name,file_count);
  if ( dump != &dump_to_stdout )
  {
    delete dump;
    dump = 0;
  }

  if ( dump_fp )
  {
    // close the text dump file
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

