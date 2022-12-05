#include "../opennurbs_public_examples.h"

static void Usage(const char* exe_name)
{
  if (nullptr == exe_name)
    exe_name = "example_convert";

  printf(
    "Usage: %s input.3dm output.3dm [--version=0] [--log=logfile_path]\n"
    "  version is one of 1, 2, 3, 4, 5, 50, 60.\n"
    "  Default version is %d.\n"
    "  logfile_path is the path to the text log representing the file that was read.\n"
    "\n",
    exe_name, 
    ON_BinaryArchive::CurrentArchiveVersion()
  );

  printf(
    "%s reads a 3dm file and writes a new 3dm file using the specified file version.\n"
    "If the optional --version argument is not specified, a version %d file is written.\n"
    "If an error or warning occurs during conversion, this program ends with exit code 1.\n"
    "Successful conversion ends with exit code 0.\n\n",
    exe_name, 
    ON_BinaryArchive::CurrentArchiveVersion()
  );
}

static bool HasErrorsOrWarnings(ON_TextLog* log, const char* operation)
{
  ON_String msg;
  if (ON_GetErrorCount() > 0)
  {
    msg.Format("%d errors: %s\n", ON_GetErrorCount(), operation);
    log->Print(msg);
    return true;
  }
  if (ON_GetWarningCount() > 0)
  {
    msg.Format("%d warnings: %s\n", ON_GetErrorCount(), operation);
    log->Print(msg);
    return true;
  }
  return false;
}

int main(int argc, const char *argv[])
{
  // If you are using OpenNURBS as a Windows DLL, then you MUST use
  // ON::OpenFile() to open the file.  If you are not using OpenNURBS
  // as a Windows DLL, then you may use either ON::OpenFile() or fopen()
  // to open the file.

  int argi;
  if (argc < 2)
  {
    Usage(argv[0]);
    return 0;
  }

  // Call once in your application to initialze opennurbs library
  ON::Begin();

  int version = 0; // write current Rhino file

                   // default dump is to stdout
  ON_TextLog dump_to_stdout;
  ON_TextLog* dump = &dump_to_stdout;

  ON_String input;
  ON_String output;
  ON_String logfile;

  for (argi = 1; argi < argc; argi++)
  {
    ON_String arg(argv[argi]);

    if (arg.Left(10).CompareOrdinal("--version=", true) == 0)
    {
      arg = arg.Mid(10);
      version = atoi(arg);
      continue;
    }

    if (arg.Left(2).CompareOrdinal("/v", true) == 0 || arg.Left(2).CompareOrdinal("-v", true) == 0)
    {
      argi++;
      const char* sversion = argv[argi];
      version = atoi(sversion);
      continue;
    }

    if (arg.Left(6).CompareOrdinal("--log=", true) == 0)
    {
      arg = arg.Mid(6);
      logfile = arg;
      continue;
    }

    if (input.IsEmpty())
    {
      input = arg;
      if (false == ON_FileStream::Is3dmFile(input, true))
      {
        input = ON_String::EmptyString;
        break;
      }
      continue;
    }

    if (output.IsEmpty())
    {
      output = arg;
      continue;
    }

    // Invalid command line parameter
    input = ON_String::EmptyString;
    output = ON_String::EmptyString;
    break;
  }

  if (input.IsEmpty() || output.IsEmpty())
  {
    Usage(argv[0]);
    return 1;
  }


  dump->Print("\nOpenNURBS Archive File:  %s\n", static_cast<const char*>(input) );

  // open file containing opennurbs archive
  FILE* archive_fp = ON_FileStream::Open3dmToRead(input);
  if (nullptr == archive_fp)
  {
    dump->Print("  Unable to open file.\n");
    return 1;
  }

  dump->PushIndent();

  // create achive object from file pointer
  ON_BinaryFile archive(ON::archive_mode::read3dm, archive_fp);

  // read the contents of the file into "model"
  ONX_Model model;
  bool rc = model.Read(archive, dump);
  // close the file
  ON::CloseFile(archive_fp);

  if (false == rc)
  {
    dump->Print("Errors during reading.\n");
    return 1;
  }

  if (HasErrorsOrWarnings(dump, "reading input file"))
    return 1;
  
  // print diagnostic
  dump->Print("Successfully read.\n");
  
  // Write file
  model.m_sStartSectionComments = "Converted by example_convert.exe";
  bool outrc = model.Write(output, version, dump);
  if (HasErrorsOrWarnings(dump, "writing output file\n"))
    return 1;
  
  if (outrc)
  {
    dump->Print("model.Write(%s) succeeded.\n", static_cast<const char*>(output));
    ONX_Model model2;
    if (model2.Read(output, dump))
    {
      dump->Print("model2.Read(%s) succeeded.\n", static_cast<const char*>(output));
      if (HasErrorsOrWarnings(dump, "verifying output file"))
        return 1;

      if (!logfile.IsEmpty())
      {
        FILE* fp = ON::OpenFile(logfile, "w");
        ON_TextLog log(fp);
        model2.Dump(log);
        ON::CloseFile(fp);
      }

    }
    else
    {
      dump->Print("model2.Read(%s) failed.\n", static_cast<const char*>(output));
    }

    dump->PopIndent();
  }

  // OPTIONAL: Call just before your application exits to clean
  //           up opennurbs class definition information.
  //           Opennurbs will not work correctly after ON::End()
  //           is called.
  ON::End();

  return 0;
}

