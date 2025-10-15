// RAHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Git.h"
#include "Util.h"

#include <rcheevos/include/rc_hash.h>

#include <memory>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <Windows.h>
#elif defined(__unix__)
 #define IGNORE_WILDCARDS /* expect unix to do the globbing before calling us */
 #ifndef IGNORE_WILDCARDS
  #include <glob.h>
  #include <sys/stat.h>
 #endif
#else
 #include <dirent.h>
 #include <fnmatch.h>
 #include <sys/stat.h>
#endif

#ifdef HAVE_CHD
void rc_hash_init_chd_cdreader(); /* in HashCHD.cpp */
#endif

void   initHash3DS(const std::string& systemDir); /* in Hash3DS.cpp */

static void usage(const char* appname)
{
  printf("RAHasher %s\n====================\n", git::getReleaseVersion());

  printf("Usage: %s [-v] [-s systempath] systemid filepath\n", util::fileName(appname).c_str());
  printf("\n");
  printf("  -v             (optional) enables verbose messages for debugging\n");
  printf("  -s systempath  (optional) specifies where supplementary files are stored (typically a path to RetroArch/system)\n");
  printf("  systemid       specifies the system id associated to the game (which hash algorithm to use)\n");
  printf("  filepath       specifies the path to the game file (file may include wildcards, path may not)\n");
}

class StdErrLogger : public Logger
{
public:
  void log(enum retro_log_level level, const char* line, size_t length) override
  {
    // ignore non-errors unless they're coming from the hashing code
    if (level != RETRO_LOG_ERROR && strncmp(line, "[HASH]", 6) != 0)
      return;

    // don't print the category
    while (*line && *line != ']')
    {
      ++line;
      --length;
    }
    while (*line == ']' || *line == ' ')
    {
      ++line;
      --length;
    }

    ::fwrite(line, length, 1, stderr);
    ::fprintf(stderr, "\n");
  }
};

static std::unique_ptr<StdErrLogger> logger;

static void rhash_log(const char* message)
{
  printf("%s\n", message);
}

void rhash_log_error_message(const char* message)
{
  fprintf(stderr, "%s\n", message);
}

static void* rhash_file_open(const char* path)
{
  return util::openFile(logger.get(), path, "rb");
}

#define RC_CONSOLE_MAX 90

static int process_file(int consoleId, const std::string& file)
{
  char hash[33];
  int count = 0;

  std::string filePath = util::fullPath(file);
  std::string ext = util::extension(file);

  if (consoleId != RC_CONSOLE_ARCADE && consoleId <= RC_CONSOLE_MAX && ext.length() == 4 &&
      tolower(ext[1]) == 'z' && tolower(ext[2]) == 'i' && tolower(ext[3]) == 'p')
  {
    std::string unzippedFilename;
    size_t size;
    void* data = util::loadZippedFile(logger.get(), filePath, &size, unzippedFilename);
    if (data)
    {
      if (rc_hash_generate_from_buffer(hash, consoleId, (uint8_t*)data, size))
      {
        printf("%s", hash);
        count = 1;
      }

      free(data);
    }
  }
  else
  {
    /* register a custom file_open handler for unicode support. use the default implementation for the other methods */
    struct rc_hash_filereader filereader;
    memset(&filereader, 0, sizeof(filereader));
    filereader.open = rhash_file_open;
    rc_hash_init_custom_filereader(&filereader);

    if (ext.length() == 4 && tolower(ext[1]) == 'c' && tolower(ext[2]) == 'h' && tolower(ext[3]) == 'd')
    {
#ifdef HAVE_CHD
      rc_hash_init_chd_cdreader();
#else
      printf("CHD not supported without HAVE_CHD compile flag");
      return 0;
#endif
    }
    else
    {
      rc_hash_init_default_cdreader();
    }

    if (consoleId > RC_CONSOLE_MAX)
    {
      rc_hash_iterator iterator;
      rc_hash_initialize_iterator(&iterator, filePath.c_str(), NULL, 0);
      while (rc_hash_iterate(hash, &iterator))
      {
        printf("%s", hash);
        count++;
      }
      rc_hash_destroy_iterator(&iterator);
    }
    else
    {
      if (rc_hash_generate_from_file(hash, consoleId, filePath.c_str()))
      {
        printf("%s", hash);
        count++;
      }
    }
  }

  return count;
}

#ifndef IGNORE_WILDCARDS

static int process_iterated_file(int console_id, const std::string& file)
{
  int result = process_file(console_id, file);
  if (!result)
    printf("????????????????????????????????");

  printf(" %s\n", util::fileNameWithExtension(file).c_str());
  return result;
}

static int process_files(int consoleId, const std::string& pattern)
{
  int count = 0;

#ifdef _WIN32
  std::string path = util::directory(pattern);
  WIN32_FIND_DATAA fileData;
  HANDLE hFind;

  if (path == pattern) /* no backslash found. scan is in current directory */
    path = ".";

  hFind = FindFirstFileA(pattern.c_str(), &fileData);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    do
    {
      if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      {
        const std::string filePath = path + "\\" + fileData.cFileName;
        count += process_iterated_file(consoleId, filePath);
      }
    } while (FindNextFileA(hFind, &fileData));

    FindClose(hFind);
  }
#elif defined(__unix__)
  glob_t globResult;
  memset(&globResult, 0, sizeof(globResult));

  if (glob(pattern.c_str(), GLOB_TILDE, NULL, &globResult) == 0)
  {
    struct stat filebuf;
    size_t i;
    for (i = 0; i < globResult.gl_pathc; ++i)
    {
      if (stat(globResult.gl_pathv[i], &filebuf) == 0 && !S_ISDIR(filebuf.st_mode))
        count += process_iterated_file(consoleId, globResult.gl_pathv[i]);
    }
  }

  globfree(&globResult);
#else
  const std::string filePattern = util::fileNameWithExtension(pattern);
  char resolved_path[PATH_MAX];
  std::string path = util::directory(pattern);
  DIR* dirp;

  realpath(path.c_str(), resolved_path);
  path = resolved_path;

  dirp = opendir(path.c_str());
  if (dirp)
  {
    struct stat filebuf;
    struct dirent *dp;

    while ((dp = readdir(dirp)))
    {
      if (fnmatch(filePattern.c_str(), dp->d_name, 0) == 0)
      {
        if (stat(dp->d_name, &filebuf) == 0 && !S_ISDIR(filebuf.st_mode))
        {
          const std::string filePath = path + "/" + dp->d_name;
          count += process_iterated_file(consoleId, filePath);
        }
      }
    }
  }
#endif

  if (count == 0)
    printf("No matches found\n");

  return count;
}

#endif /* IGNORE_WILDCARDS */

int main(int argc, char* argv[])
{
  int consoleId = 0;
  int singleFile = 1;
  std::string systemDirectory = ".";

  int argi = 1;

  while (argi < argc && argv[argi][0] == '-')
  {
    if (strcmp(argv[argi], "-v") == 0)
    {
      rc_hash_init_verbose_message_callback(rhash_log);
      ++argi;
    }
    else if (strcmp(argv[argi], "-s") == 0)
    {
      systemDirectory = argv[++argi];
      ++argi;
    }
    else
    {
      usage(argv[0]);
      return 1;
    }
  }

  if (argi + 2 > argc)
  {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  consoleId = atoi(argv[argi++]);
  if (consoleId == 0)
  {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  logger.reset(new StdErrLogger);
  rc_hash_init_error_message_callback(rhash_log_error_message);

  if (consoleId == RC_CONSOLE_NINTENDO_3DS)
    initHash3DS(systemDirectory);

  if (argi + 1 < argc)
  {
    if (consoleId > RC_CONSOLE_MAX)
    {
      printf("Specific console must be specified when processing multiple files\n");
      return EXIT_FAILURE;
    }

    singleFile = 0;
  }
#ifndef IGNORE_WILDCARDS
  else
  {
    std::string file = argv[argi];
    if (file.find('*') != std::string::npos || file.find('?') != std::string::npos)
    {
      if (consoleId > RC_CONSOLE_MAX)
      {
        printf("Specific console must be specified when using wildcards\n");
        return EXIT_FAILURE;
      }

      singleFile = 0;
    }
  }
#endif

  if (!singleFile)
  {
    /* verbose logging not allowed when processing multiple files */
    rc_hash_init_verbose_message_callback(NULL);
  }

  while (argi < argc)
  {
    std::string file = argv[argi++];

#ifndef IGNORE_WILDCARDS
    if (file.find('*') != std::string::npos || file.find('?') != std::string::npos)
    {
      if (!process_files(consoleId, file))
        return EXIT_FAILURE;
    }
    else
#endif
    {
      int result = process_file(consoleId, file);

      if (singleFile)
        printf("\n");
      else
        printf(" %s\n", util::fileNameWithExtension(file).c_str());

      if (!result)
        return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
