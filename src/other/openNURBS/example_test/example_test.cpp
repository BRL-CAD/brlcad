/*
//
// Copyright (c) 1993-2018 Robert McNeel & Associates. All rights reserved.
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
//  example_test.cpp  
// 
//  Example program using the Rhino file IO toolkit. The program reads  
//  Rhino 3dm model files and verifies they can be saved and reread with
//  no changes to core model content. The program is a console application
//  that takes options, file names, and directory names as command line
//   arguments.
//
////////////////////////////////////////////////////////////////////////

#include "../opennurbs_public_examples.h"

class Internal_CTestContext
{
public:
  enum : unsigned int
  {
    sizeof_a = 4088
  };

  Internal_CTestContext()
  {
    m_a = onmalloc(sizeof_a);
  }

  ~Internal_CTestContext()
  {
    onfree(m_a);
  }

  void SetInitialDirecory(
    const char* initial_directory,
    unsigned int counter
  )
  {
    m_initial_directory = Internal_CleanPath(initial_directory);
    m_initial_directory_length = m_initial_directory.Length();
    m_initial_directory_counter = counter;
  }

  void* m_a = nullptr;
  ON__INT64 m_min_delta_time = 0;
  ON__INT64 m_max_delta_time = 0;

  unsigned int m_file_count = 0;
  unsigned int m_directory_count = 0;
  unsigned int m_file_open_error_count = 0;
  unsigned int m_max_directory_tree_depth = 0;
  unsigned int m_max_file_count = 0;

  const ON_String TextLogPathFromFullPath(
    const char* full_path
  ) const;

private:
  ON_String m_initial_directory;
  unsigned int m_initial_directory_counter = 0;
  int m_initial_directory_length = 0;
  Internal_CTestContext(const Internal_CTestContext&) = delete;
  Internal_CTestContext operator=(const Internal_CTestContext&) = delete;
  const ON_String Internal_CleanPath(const char* dirty_path) const
  {
    const char* volume = 0;
    const char* path = 0;

    // Use local path in case drive, dir, file_name_stem or ext are being reused.
    on_splitpath(dirty_path, &volume, &path, nullptr, nullptr);
    ON_String clean_path(path);
    if (clean_path.IsEmpty())
      return ON_String(dirty_path);
    clean_path.Replace(ON_String::Backslash, ON_String::Slash);
    
    if (nullptr != volume && volume < path)
    {
      ON_String clean_volume(volume, (int)(path - volume));
      return (clean_volume + clean_path);
    }

    return clean_path;
  }

};

const ON_String Internal_CTestContext::TextLogPathFromFullPath(const char* full_path) const
{
  // replace initial directory with <Initial Folder> and use / for
  // the directory separator so that output files are standardized.
  ON_String text_log_path;
  ON_String path1 = Internal_CleanPath(full_path);
  if (m_initial_directory_length > 0 &&
    ON_String::EqualOrdinal(m_initial_directory, m_initial_directory_length, path1, m_initial_directory_length, true)
    )
  {
    text_log_path 
      = (m_initial_directory_counter>0)
      ? ON_String::FormatToString("<initial folder %u>", m_initial_directory_counter)
      : ON_String("<initial folder>");
    text_log_path += static_cast<const char*>(path1) + m_initial_directory_length;
  }
  else
  {
    text_log_path = path1;
  }
  return text_log_path;
}


static const ONX_ErrorCounter Internal_TestModelRead(
  ON_TextLog& text_log,
  const ON_String text_log_3dm_file_name,
  ON_BinaryArchive& source_archive,
  bool bVerbose
  )
{
  ONX_ModelTest::Type test_type = ONX_ModelTest::Type::ReadWriteReadCompare;
  
  /////
  //
  // Read the orginal file
  //
  text_log.PushIndent();
  ONX_ModelTest test;
  const ON_wString wide_text_log_3dm_file_name(text_log_3dm_file_name);
  test.ReadTest(source_archive, test_type, true, wide_text_log_3dm_file_name, &text_log);
  text_log.PrintNewLine();
  text_log.Print("Test Results:\n");
  text_log.PushIndent();
  test.Dump(text_log);
  text_log.PopIndent();

  ONX_ErrorCounter error_counter = test.ErrorCounter();

  const ONX_Model* source_model = test.SourceModel().get();
  if (nullptr == source_model)
  {
    text_log.PopIndent();
    return error_counter;
  }

  const bool bCompareTestFailed = ONX_ModelTest::Result::Fail == test.TestResult(ONX_ModelTest::Type::ReadWriteReadCompare);

  if ( bVerbose || bCompareTestFailed )
  {
    for (int i = 0; i < 2; i++)
    {
      if (0 == i)
        test.DumpSourceModel();
      else
        test.DumpReadWriteReadModel();
      if (false == bCompareTestFailed)
        break;
    }
  }
  
  text_log.PrintNewLine();

  const ON_ModelComponent::Type component_types[] =
  {
    // TODO uncomment types as components are derived from ON_ModelComponent
    ON_ModelComponent::Type::Image,
    //ON_ModelComponent::Type::TextureMapping,
    ON_ModelComponent::Type::RenderMaterial,
    ON_ModelComponent::Type::LinePattern,
    ON_ModelComponent::Type::Layer,
    //ON_ModelComponent::Type::Group,
    ON_ModelComponent::Type::TextStyle,
    ON_ModelComponent::Type::DimStyle,
    ON_ModelComponent::Type::RenderLight,
    ON_ModelComponent::Type::HatchPattern,
    ON_ModelComponent::Type::InstanceDefinition,
    ON_ModelComponent::Type::ModelGeometry,
    //ON_ModelComponent::Type::HistoryRecord
  };

  const unsigned int component_type_count = (unsigned int)(sizeof(component_types) / sizeof(component_types[0]));

  const ONX_Model& model = *source_model;

  error_counter.ClearLibraryErrorsAndWarnings();

  const ON_ComponentManifest& manifest = model.Manifest();
  ON_SerialNumberMap it_map;

  for (unsigned int i = 0; i < component_type_count; i++)
  {
    const ON_ModelComponent::Type component_type = component_types[i];
    const bool bUniqueNameRequired = ON_ModelComponent::UniqueNameRequired(component_type);
    const bool bEmbeddedFileComponent = (ON_ModelComponent::Type::Image == component_type);
    ONX_ModelComponentIterator it(model,component_type);
    unsigned int it_count = 0;
    for (ON_ModelComponentReference mcr = it.FirstComponentReference(); false == mcr.IsEmpty(); mcr = it.NextComponentReference())
    {
      const ON_ModelComponent* model_component = mcr.ModelComponent();
      if (nullptr == model_component)
      {
        ON_ERROR("Iterator returned nullptr mcr.ModelComponent()");
        continue; 
      }
      if (model_component->ComponentType() != component_type)
      {
        ON_ERROR("Iterator returned wrong mcr.ModelComponent()->ComponentType()");
        continue; 
      }
      const ON__UINT64 model_component_sn = model_component->RuntimeSerialNumber();
      const ON_UUID model_component_id = model_component->Id();
      if (0 == model_component_sn)
      {
        ON_ERROR("Iterator mcr.ModelComponent()->RuntimeSerialNumber() is zero.");
        continue; 
      }
      if (ON_nil_uuid == model_component_id)
      {
        ON_ERROR("Iterator mcr.ModelComponent()->Id() is nil.");
        continue; 
      }
      const ON_SerialNumberMap::SN_ELEMENT* e_sn = it_map.FindSerialNumber(model_component_sn);
      if (nullptr != e_sn)
      {
        ON_ERROR("Iterator returned component serial number twice.");
        continue; 
      }
      const ON_SerialNumberMap::SN_ELEMENT* e_id = it_map.FindId(model_component_id);
      if (nullptr != e_id)
      {
        ON_ERROR("Iterator returned component id twice.");
        continue; 
      }
      ON_SerialNumberMap::SN_ELEMENT* e = it_map.AddSerialNumberAndId(model_component_sn,model_component_id);
      if (nullptr == e)
      {
        ON_ERROR("ON_SerialNumberMap failed to add sn and id.");
        continue; 
      }
      if (e->m_value.m_u_type != 0)
      {
        ON_ERROR("ON_SerialNumberMap error.");
        continue; 
      }
      e->m_value.m_u_type = 1;
      e->m_value.m_u.ptr = (void*)model_component;

      const ON_ComponentManifestItem& item = manifest.ItemFromComponentRuntimeSerialNumber(model_component_sn);
      if (item.IsUnset())
      {
        ON_ERROR("Iterator returned item not in manifest.");
        continue; 
      }
      if (model_component_sn != item.ComponentRuntimeSerialNumber())
      {
        ON_ERROR("manifest.ItemFromComponentRuntimeSerialNumber() error.");
        continue; 
      }
      if (model_component_id != item.Id())
      {
        ON_ERROR("item has wrong id.");
        continue; 
      }
 
      const ON_ComponentManifestItem& item_id = manifest.ItemFromId(model_component_id);
      if (&item != &item_id)
      {
        ON_ERROR("manifest.ItemFromId() failed.");
        continue; 
      }

      if (bUniqueNameRequired || bEmbeddedFileComponent)
      {
        const ON_Bitmap* embedded_file_reference = bEmbeddedFileComponent ? ON_Bitmap::Cast(model_component) : nullptr;
        const ON_NameHash full_path_hash 
          = (nullptr != embedded_file_reference) 
          ? ON_NameHash::CreateFilePathHash(embedded_file_reference->FileReference()) 
          : ON_NameHash::EmptyNameHash;
        const ON_NameHash name_hash 
          =bEmbeddedFileComponent
          ? full_path_hash
          : model_component->NameHash();
        if (false == name_hash.IsValidAndNotEmpty())
        {
          ON_ERROR("model compoent name is not valid.");
          continue; 
        }
        if (name_hash != item.NameHash())
        {
          ON_ERROR("item has wrong name hash.");
          continue; 
        }
        const ON_ComponentManifestItem& item_name = manifest.ItemFromNameHash(component_type,name_hash);
        if (&item != &item_name)
        {
          ON_ERROR("manifest.ItemFromNameHash() failed.");
          continue; 
        }
      }
      it_count++;
    }

    if (it_count != manifest.ActiveAndDeletedComponentCount(component_type))
    {
      ON_ERROR("iterator count != manifest.ActiveAndDeletedComponentCount(component_type)");
      continue; 
    }
  }

  text_log.PopIndent();
  error_counter.AddLibraryErrorsAndWarnings();
  return error_counter;
}

static const ONX_ErrorCounter Internal_TestFileRead(
  ON_TextLog& text_log,
  const ON_String fullpath,
  const ON_String text_log_path,
  bool bVerbose
  )
{
  FILE* fp = nullptr;

  fp = ON_FileStream::Open3dmToRead(fullpath);

  ONX_ErrorCounter error_counter;
  error_counter.ClearLibraryErrorsAndWarnings();

  const ON_String path_to_print
    = (text_log_path.IsNotEmpty())
    ? text_log_path
    : fullpath;
  
  for (;;)
  {
    if (nullptr == fp)
    {
      text_log.Print(
        "Skipped file: %s\n",
        static_cast<const char*>(path_to_print)
      );
      error_counter.IncrementFailureCount();
      break;
    }

    text_log.Print(
      "File name: %s\n",
      static_cast<const char*>(path_to_print)
    );

    ON_BinaryFile source_archive(ON::archive_mode::read3dm, fp);
    const ON_wString wide_full_path(fullpath);
    source_archive.SetArchiveFullPath(static_cast<const wchar_t*>(wide_full_path));

    error_counter += Internal_TestModelRead(text_log, path_to_print, source_archive, bVerbose);
    break;
  }

  if ( nullptr != fp )
  {
    ON_FileStream::Close(fp);
    fp = nullptr;
  }

  return error_counter;
}

static int Internal_ComparePath(const ON_String* lhs, const ON_String* rhs)
{
  // sort nullptr to end of list.
  if (lhs == rhs)
    return 0;
  if (nullptr == lhs)
    return 1;
  if (nullptr == rhs)
    return -1;
  int rc = ON_String::CompareOrdinal(static_cast<const char*>(*lhs), static_cast<const char*>(*rhs), true);
  if ( 0 == rc )
    rc = ON_String::CompareOrdinal(static_cast<const char*>(*lhs), static_cast<const char*>(*rhs), false);
  return rc;
}

static const ONX_ErrorCounter Internal_TestReadFolder(
  ON_TextLog& text_log,
  const char* directory_path,
  unsigned int directory_tree_depth,
  bool bVerbose,
  Internal_CTestContext& test_context
  )
{
  ONX_ErrorCounter error_counter;
  error_counter.ClearLibraryErrors();

  if (nullptr == directory_path || 0 == directory_path[0])
  {
    text_log.Print("Empty directory name.\n");
  }

  ON_FileIterator fit;
  if (false == fit.Initialize(directory_path))
  {
    text_log.Print(
      "Invalid directory name: %s\n",
      directory_path
      );
    error_counter.IncrementFailureCount();
    return error_counter;
  }

  const ON_String text_log_directory_name
    = (directory_tree_depth <= 1)
    ? ON_String(directory_path)
    : test_context.TextLogPathFromFullPath(directory_path);
  text_log.Print(
    "Directory name: %s\n",
    static_cast<const char*>(text_log_directory_name)
    );
  text_log.PushIndent();

  ON_ClassArray< ON_String > sub_directories(32);
  ON_ClassArray< ON_String > skipped_files(32);
  ON_ClassArray< ON_String > tested_files(32);

  for ( bool bHaveItem = fit.FirstItem(); bHaveItem; bHaveItem = fit.NextItem() )
  {
    if ( test_context.m_max_file_count > 0 && test_context.m_file_count >= test_context.m_max_file_count)
      break;

    if (fit.CurrentItemIsDirectory())
    {
      if (directory_tree_depth < test_context.m_max_directory_tree_depth)
      {
        sub_directories.Append(fit.CurrentItemFullPathName());
      }
      continue;
    }

    ON_String fullpath( fit.CurrentItemFullPathName() );

    if (ON_FileStream::Is3dmFile(fullpath, false))
      tested_files.Append(fullpath);
    else
      skipped_files.Append(fullpath);
  }

  // sort file and folder names so test order depends only on content.
  // This is required so results from different computers can be compared.
  sub_directories.QuickSort(Internal_ComparePath);
  skipped_files.QuickSort(Internal_ComparePath);
  tested_files.QuickSort(Internal_ComparePath);

  if (skipped_files.Count() > 0)
  {
    text_log.PrintNewLine();
    for (int i = 0; i < skipped_files.Count(); i++)
    {
      const ON_String path_to_print = test_context.TextLogPathFromFullPath(skipped_files[i]);
      text_log.Print(
        "Skipped file: %s\n",
        static_cast<const char*>(path_to_print)
      );
    }
    text_log.PrintNewLine();
  }
     
  for ( int i = 0; i < tested_files.Count(); i++ )
  {
    const ON_String full_path = tested_files[i];
    const ON_String path_to_print = test_context.TextLogPathFromFullPath(full_path);
    test_context.m_file_count++;
    const ONX_ErrorCounter file_error_counter = Internal_TestFileRead(text_log, full_path, path_to_print, bVerbose);
    error_counter += file_error_counter;
  }

  for (int i = 0; i < sub_directories.Count(); i++)
  {
    if (test_context.m_max_file_count > 0 && test_context.m_file_count >= test_context.m_max_file_count)
      break;
    const ON_String sub_directory_path = sub_directories[i];
    test_context.m_directory_count++;
    error_counter += Internal_TestReadFolder(text_log, sub_directory_path, directory_tree_depth + 1, bVerbose, test_context);
  }

  text_log.PopIndent();

  return error_counter;
}


static ONX_ErrorCounter Internal_InvalidArg(const ON_String arg, ON_TextLog& text_log)
{
  ONX_ErrorCounter err = ONX_ErrorCounter::Zero;
  text_log.Print("Invalid command line parameter: \"%s\"\n", static_cast<const char*>(arg));
  err.IncrementFailureCount();
  return err;
}

static ONX_ErrorCounter Internal_Test( 
  unsigned int max_directory_tree_depth,
  ON_String full_path,
  bool bVerbose,
  ON_TextLog& text_log,
  unsigned int& directory_counter,
  unsigned int& file_count,
  unsigned int& folder_count
)
{

  ONX_ErrorCounter err = ONX_ErrorCounter::Zero;

  if (ON_FileSystem::IsFile(full_path))
  {
    if (ON_FileStream::Is3dmFile(full_path, false))
    {
      text_log.Print("Testing 3dm file: %s\n", static_cast<const char*>(full_path));
      err = Internal_TestFileRead(text_log, full_path, ON_String::EmptyString, bVerbose);
      file_count++;
    }
  }
  else if (ON_FileSystem::IsDirectory(full_path))    
  {
    if ( max_directory_tree_depth > 0 )
    {
      text_log.Print("Testing 3dm files in folder: %s\n", static_cast<const char*>(full_path));
      Internal_CTestContext test_context;
      directory_counter++;
      test_context.SetInitialDirecory(full_path,directory_counter);
      test_context.m_max_directory_tree_depth = max_directory_tree_depth;

      err = Internal_TestReadFolder(text_log, full_path, 1, bVerbose, test_context);
      file_count += test_context.m_file_count;
      folder_count += test_context.m_directory_count;
    }
  }
  else
  {
    err += Internal_InvalidArg(full_path,text_log);
  }

  return err;
}

static ON_String Internal_PlatformId(bool bVerbose)
{
  const ON_String runtime =
#if defined(ON_RUNTIME_WIN)
    bVerbose ? "Windows" : "Win"
#elif defined(ON_RUNTIME_APPLE_IOS)
    bVerbose ? "Apple iOS" : "iOS"
#elif defined(ON_RUNTIME_APPLE_MACOS)
    bVerbose ? "Apple Mac OS" : "MacOS"
#elif defined(ON_RUNTIME_APPLE)
    "Apple"
#elif defined(ON_RUNTIME_ANDROID)
    "Android"
#elif defined(ON_RUNTIME_LINUX)
    "Linux"
#else
    "Runtime"
#endif
    ;

  const ON_String platform =
#if defined(ON_64BIT_RUNTIME)
    bVerbose ? " 64 bit" : "64"
#elif defined(ON_32BIT_RUNTIME)
    bVerbose ? " 32 bit" : "32"
#else
    ""
#endif
    ;

  const ON_String configuration =
#if defined(ON_DEBUG)
    bVerbose ? " Debug" : "Debug"
#else
    bVerbose ? " Release" : "Release"
#endif
    ;

  ON_String platform_id(runtime+platform+configuration);

  return platform_id;
}


static ON_String Internal_DefaultOutFileName(
  const ON_String exe_stem
  )
{
  ON_String default_file_name(exe_stem);
  default_file_name.TrimLeftAndRight();
  if (default_file_name.IsEmpty())
    default_file_name = "example_test";
  default_file_name += "_log";

  const ON_String platform_id = Internal_PlatformId(false);
  if (platform_id.IsNotEmpty())
    default_file_name += ON_String(ON_String("_") + platform_id);
  default_file_name += ".txt";

  return default_file_name;
}

static void Internal_PrintHelp(
  const ON_String example_test_exe_stem,
  ON_TextLog& text_log
)
{
  const ON_String default_out_filename = Internal_DefaultOutFileName(example_test_exe_stem);

  text_log.Print("\n");
  text_log.Print("SYNOPSIS:\n");
  text_log.Print("  %s [-out[:outfilename.txt]] [-recursive[:N]] [-verbose] <file or folder names>\n", static_cast<const char*>(example_test_exe_stem) );
  text_log.Print("\n");
  text_log.Print("DESCRIPTION:\n");
  text_log.Print("  If a file is listed, it is read as an opennurbs model file.\n");
  text_log.Print("  If a folder is listed, all .3dm files in that folder are read\n");
  text_log.Print("  as opennurbs model files.\n");
  text_log.Print("\n");
  text_log.Print("  Available options:\n");
  text_log.Print("    -help\n");
  text_log.Print("      Displays this message.\n");
  text_log.Print("    -out\n");
  text_log.Print("      If -out is not present, output is written to stdout.\n");
  text_log.Print("      If :outfilename.txt is appended to -out, output is written to the specifed file.\n");
  text_log.Print("      If :stdout is appended to -out, output is sent to stdout.\n");
  text_log.Print("      If :null is appended to -out, no output is written.\n");
  text_log.Print("      Otherise output is written to %s.\n",static_cast<const char*>(default_out_filename));
  text_log.Print("    -recursive\n");
  text_log.Print("    -r\n");
  text_log.Print("      Recursivly reads files in subfolders.\n");
  text_log.Print("      If a :N is appended to the option, N limits the recursion depth.\n");
  text_log.Print("      -recursive:1 reads all 3dm files in the folder and does not recurse.\n");
  text_log.Print("    -verbose\n");
  text_log.Print("    -v\n");
  text_log.Print("      For each 3dm file, a text summary of the 3dm file contents is created\n");
  text_log.Print("      in the folder containing the 3dm file.\n");
  text_log.Print("\n");
  text_log.Print("RETURNS:\n");
  text_log.Print("  0: All tested files passed.\n");
  text_log.Print("  >0: Number of failures, errors, or warnings. See output for details.\n");
  text_log.Print("\n");
  text_log.Print("EXAMPLE:\n");
  text_log.Print("  %s -out:mytestresults.txt -resursive:2 .../example_files\n",static_cast<const char*>(example_test_exe_stem));
  text_log.Print("  with read all the opennurbs .3dm files in the .../example_files folder\n");
  text_log.Print("  and immediate subfolders of .../example_files. Output will be saved in mytestresults.txt.\n");
}

static void Internal_PrintHelp(
  const ON_String example_test_exe_stem,
  ON_TextLog* text_log_ptr
)
{
  ON_TextLog print_to_stdout;
  if (nullptr == text_log_ptr)
  {
    print_to_stdout.SetIndentSize(2);
    text_log_ptr = &print_to_stdout;
  }
  return Internal_PrintHelp(example_test_exe_stem, *text_log_ptr);
}

static char Internal_ParseOptionTailSeparator()
{
  return ':';
}

static const char* Internal_ParseOptionHead(
  const char* s,
  const char* option_name0,
  const char* option_name1 = nullptr,
  const char* option_name2 = nullptr
)
{
  if (nullptr == s || 0 == s[0])
    return nullptr;
  if ('-' != s[0] )
  {
#if defined(ON_OS_WINDOWS)
    if ( '/' != s[0] )
#endif
    return nullptr;
  }

  s++;
  if (0 == s[0])
    return nullptr;
  
  const char* option_names[] = { option_name0, option_name1, option_name2 };
  const size_t option_name_count = sizeof(option_names) / sizeof(option_names[0]);
  for (size_t i = 0; i < option_name_count; i++)
  {
    const char* option_name = option_names[i];
    const int option_name_length = ON_String::Length(option_name);
    if (option_name_length < 1)
      continue;
    if (ON_String::EqualOrdinal(option_name, option_name_length, s, option_name_length, true))
    {
      const char* tail = s + option_name_length;
      if (0 == tail[0] || Internal_ParseOptionTailSeparator() == tail[0])
        return tail;
    }
  }

  return nullptr;
}

static const ON_String Internal_ParseOptionTail(
  const char* s
)
{
  for (;;)
  {
    if (nullptr == s || Internal_ParseOptionTailSeparator() != s[0] || s[1] <= ON_String::Space)
      break;
    ON_String tail(s+1);
    tail.TrimRight();
    int len = tail.Length();
    if (len < 1)
      break;
    if (len >= 2)
    {
      bool bTrim = false;
      if ('"' == tail[0] && '"' == tail[len - 1])
        bTrim = true;
      else if ('\'' == tail[0] && '\'' == tail[len - 1])
        bTrim = true;
      if (bTrim)
      {
        tail = ON_String(s + 2, len - 2);
        if (tail.Length() < 1)
          break;
      }
    }
    return tail;
  }
  return ON_String::EmptyString;
}


static bool Internal_ParseArg_HELP(const ON_String arg)
{
  const char* tail = Internal_ParseOptionHead(static_cast<const char*>(arg), "help", "h", "?" );
  return (nullptr != tail && 0 == tail[0]);
}

static bool Internal_ParseArg_VERBOSE(const ON_String arg)
{
  const char* tail = Internal_ParseOptionHead(static_cast<const char*>(arg), "verbose", "v" );
  return (nullptr != tail && 0 == tail[0]);
}

static bool Internal_ParseArg_OUT(const ON_String exe_stem, const ON_String arg, ON_String& output_file_path)
{
  const char* tail = Internal_ParseOptionHead(static_cast<const char*>(arg), "out" );
  if (nullptr == tail)
    return false;

  if (0 == tail[0])
  {
    output_file_path = Internal_DefaultOutFileName(exe_stem);
  }
  else if (Internal_ParseOptionTailSeparator() == tail[0])
  {
    output_file_path = ON_FileSystemPath::ExpandUser(Internal_ParseOptionTail(tail));
    output_file_path.TrimLeftAndRight();
    if (
      false == output_file_path.EqualOrdinal("stdout",true)
      && false == output_file_path.EqualOrdinal("stderr",true)
      && false == output_file_path.EqualOrdinal("null",true)
      && false == output_file_path.EqualOrdinal("dev/null",true)
      && ON_FileSystem::IsDirectory(output_file_path)
      )
    {
      const ON_wString wide_dir(output_file_path);
      const ON_wString wide_fname(Internal_DefaultOutFileName(exe_stem));
      output_file_path = ON_String(ON_FileSystemPath::CombinePaths(wide_dir, false, wide_fname, true, false));
    }
  }
  else
  {
    output_file_path = ON_String::EmptyString;
  }

  return true;
}

static bool Internal_ParseArg_RECURSE(const ON_String arg, unsigned int& N)
{
  const char* tail = Internal_ParseOptionHead(static_cast<const char*>(arg), "recursive", "r" );
  if (nullptr == tail)
    return false;

  if (0 == tail[0])
  {
    N = 16; // sanity limit for default directory recursion depth; 
    return true;
  }
    
  if (Internal_ParseOptionTailSeparator() == tail[0])
  {
    const ON_String num = Internal_ParseOptionTail(tail);
    if (num.IsNotEmpty())
    {
      unsigned int u = 0;
      const char* s1 = ON_String::ToNumber(num, u, &u);
      if (nullptr != s1 && s1 > static_cast<const char*>(num) && u >= 1 && 0 == s1[0])
      {
        N = u;
        return true;
      }
    }
  }

  N = 0;
  return false;
}


static const ON_String Internal_ParseArg_PATH(const ON_String arg, unsigned int max_directory_tree_depth)
{
  ON_String arg_full_path = ON_FileSystemPath::ExpandUser(static_cast<const char*>(arg));
  arg_full_path.TrimLeftAndRight();

  if (ON_FileSystem::IsFile(arg_full_path))
    return arg_full_path;

  if (max_directory_tree_depth > 0)
  {
    if (arg_full_path.Length() != 1 || false == ON_FileSystemPath::IsDirectorySeparator(arg_full_path[0], true))
    {
      const char dir_seps[3] = { ON_FileSystemPath::DirectorySeparatorAsChar, ON_FileSystemPath::AlternateDirectorySeparatorAsChar, 0 };
      arg_full_path.TrimRight(dir_seps);
    }
    if (ON_FileSystem::IsDirectory(arg_full_path))
      return arg_full_path;
  }

  return ON_String::EmptyString;
}

static void Internal_PrintIntroduction(
  const ON_String& example_test_exe_path,
  ON_TextLog& text_log
)
{
  text_log.Print("Executable: %s\n", static_cast<const char*>(example_test_exe_path));
  text_log.Print("Compiled: " __DATE__ " " __TIME__ "\n");
  const ON_String platform_name = Internal_PlatformId(true);
  ON::Version();
  text_log.Print("Platform: %s\n", static_cast<const char*>(platform_name));
  text_log.Print(
    "Opennurbs version: %u.%u %04u-%02u-%02u %02u:%02u (%u)\n",
    ON::VersionMajor(),
    ON::VersionMinor(),
    ON::VersionYear(),
    ON::VersionMonth(),
    ON::VersionDayOfMonth(),
    ON::VersionHour(),
    ON::VersionMinute(),
    ON::VersionBranch()
  );
  text_log.Print("Current time: ");
  text_log.PrintCurrentTime();
  text_log.Print(" UCT\n");
}

int main( int argc, const char *argv[] )
{
  // If you are using OpenNURBS as a Windows DLL, then you MUST use
  // ON::OpenFile() to open the file.  If you are not using OpenNURBS
  // as a Windows DLL, then you may use either ON::OpenFile() or fopen()
  // to open the file.

  ON_String arg(nullptr != argv ? argv[0] : ((const char*)nullptr));
  arg.TrimLeftAndRight();
  const ON_String example_test_exe_path(arg);
  ON_String example_test_exe_stem;
  ON_FileSystemPath::SplitPath(example_test_exe_path, nullptr, nullptr, &example_test_exe_stem, nullptr);
  if (example_test_exe_stem.IsEmpty())
    example_test_exe_stem = "example_test";

  ON_TextLog print_to_stdout;
  print_to_stdout.SetIndentSize(2);

  int argi;
  if ( argc < 2 ) 
  {
    Internal_PrintHelp(example_test_exe_stem, print_to_stdout);
    return 0;
  }

  // Call once in your application to initialze opennurbs library
  ON::Begin();

  // default text_log is to stdout
  ON_TextLog* text_log = &print_to_stdout;
  FILE* text_log_fp = nullptr;
  
  unsigned int maximum_directory_depth = 0;

  bool bVerbose = false; // true = output will include a full listing of all 3dm file contents.

  ONX_ErrorCounter err;
  unsigned int file_count = 0;
  unsigned int folder_count = 0;
  unsigned int directory_arg_counter = 0;

  bool bPrintIntroduction = true;

  for ( argi = 1; argi < argc; argi++ ) 
  {
    arg = argv[argi];
    arg.TrimLeftAndRight();
    if (arg.IsEmpty())
      continue;

    if (argi + 1 == argc && Internal_ParseArg_HELP(arg))
    {
      Internal_PrintHelp(example_test_exe_stem, text_log);
      if ( 0 == folder_count && 0 == file_count && 0 == err.TotalCount() )
        return 0;
      continue;
    }

    // check for -out:<file name> option
    ON_String output_file_name;
    if ( Internal_ParseArg_OUT(example_test_exe_stem, arg, output_file_name) )
    {
      // need a new introduction when output destination changes.
      bPrintIntroduction = true;

      // change destination of text_log file
      if ( text_log != &print_to_stdout && text_log != &ON_TextLog::Null )
      {
        delete text_log;
      }
      if ( text_log_fp )
      {
        ON::CloseFile(text_log_fp);
        text_log_fp = nullptr;
      }

      text_log = &print_to_stdout;

      if (output_file_name.IsEmpty() || output_file_name.EqualOrdinal("stdout", true))
        continue;

      if (output_file_name.EqualOrdinal("null", true) || output_file_name.EqualOrdinal("dev/null", true))
      {
        text_log = &ON_TextLog::Null;
        continue;
      }

      FILE* text_fp = ON::OpenFile(output_file_name,"w");
      if (nullptr == text_fp)
      {
        err += Internal_InvalidArg(arg, *text_log);
        break;
      }

      text_log_fp = text_fp;
      text_log = new ON_TextLog(text_log_fp);
      text_log->SetIndentSize(2);

      continue;
    }

    // check for -recursive:<directory depth> option
    if (Internal_ParseArg_RECURSE(arg, maximum_directory_depth))
    {
      continue;
    }

    if (Internal_ParseArg_VERBOSE(arg))
    {
      bVerbose = true;
      continue;
    }

    if (bPrintIntroduction)
    {
      bPrintIntroduction = false;
      Internal_PrintIntroduction(example_test_exe_path, *text_log);
    }

    const ON_String arg_full_path = Internal_ParseArg_PATH(arg,maximum_directory_depth);
    if (arg_full_path.IsEmpty())
    {
      err += Internal_InvalidArg(arg, *text_log);
      break;
    }

    err += Internal_Test(
      maximum_directory_depth,
      arg_full_path,
      bVerbose,
      *text_log,
      directory_arg_counter,
      file_count,
      folder_count
    );
  }

  if (bPrintIntroduction)
  {
    bPrintIntroduction = false;
    Internal_PrintIntroduction(example_test_exe_path, *text_log);
  }

  if (folder_count > 0)
  {
    text_log->Print(
      "Tested %u 3dm files in %u folders.",
      file_count,
      folder_count
    );
  }
  else
  {
    text_log->Print(
      "Tested %u 3dm files.",
      file_count
    );
  }

  if (err.FailureCount() > 0)
    text_log->Print(" Failures. ");
  else if (err.ErrorCount() > 0)
    text_log->Print(" Errors. ");
  else if (err.FailureCount() > 0)
    text_log->Print(" Warnings. ");
  else
    text_log->Print(" All tests passed. ");
  err.Dump(*text_log);
  text_log->PrintNewLine();

  if ( text_log != &print_to_stdout && text_log != &ON_TextLog::Null )
  {
    delete text_log;
  }

  text_log = nullptr;

  if ( text_log_fp )
  {
    // close the text text_log file
    ON::CloseFile( text_log_fp );
    text_log_fp = 0;
  }
  
  // OPTIONAL: Call just before your application exits to clean
  //           up opennurbs class definition information.
  //           Opennurbs will not work correctly after ON::End()
  //           is called.
  ON::End();

  return err.TotalCount();
}
