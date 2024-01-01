// RAHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Git.h"
#include "Hash.h"
#include "Util.h"

#include <rcheevos/include/rc_hash.h>

#include <memory>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <Windows.h>
#endif

#ifdef HAVE_CHD
void rc_hash_init_chd_cdreader(); /* in HashCHD.cpp */
#endif

static void usage(const char* appname)
{
  printf("RAHasher %s\n====================\n", git::getReleaseVersion());

  printf("Usage: %s [-v] systemid filepath\n", util::fileName(appname).c_str());
  printf("\n");
  printf("  -v           (optional) enables verbose messages for debugging\n");
  printf("  systemid     specifies the system id associated to the game (which hash algorithm to use)\n");
  printf("  filepath     specifies the path to the game file (file may include wildcards, path may not)\n");
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

static void rhash_log_error(const char* message)
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
  int result = 1;

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
        printf("%s", hash);

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
        printf("%s", hash);
      rc_hash_destroy_iterator(&iterator);
    }
    else
    {
      if (rc_hash_generate_from_file(hash, consoleId, filePath.c_str()))
        printf("%s", hash);
    }
  }

  return result;
}

static int process_files(int consoleId, const std::string& pattern)
{
  int count = 0;

#ifdef _WIN32
  const std::string& path = util::directory(pattern);
  WIN32_FIND_DATAA fileData;
  HANDLE hFind;

  hFind = FindFirstFileA(pattern.c_str(), &fileData);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    do
    {
      if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      {
        std::string filePath = path + "\\" + fileData.cFileName;
        if (process_file(consoleId, filePath))
          ++count;
        else
          printf("????????????????????????????????");

        printf(" %s\n", fileData.cFileName);
      }
    } while (FindNextFileA(hFind, &fileData));

    FindClose(hFind);
  }
#endif

  if (count == 0)
    printf("No matches found\n");

  return count;
}

int main(int argc, char* argv[])
{
  int consoleId = 0;
  std::string file;

  if (argc == 3)
  {
    consoleId = atoi(argv[1]);
    file = argv[2];
  }
  else if (argc == 4 && strcmp(argv[1], "-v") == 0)
  {
    rc_hash_init_verbose_message_callback(rhash_log);

    consoleId = atoi(argv[2]);
    file = argv[3];
  }

  if (consoleId != 0 && !file.empty())
  {
    logger.reset(new StdErrLogger);
    rc_hash_init_error_message_callback(rhash_log_error);

    if (file.find('*') != std::string::npos || file.find('?') != std::string::npos)
    {
      if (consoleId > RC_CONSOLE_MAX)
      {
        printf("Specific console must be specified when using wildcards\n");
        return 0;
      }

      rc_hash_init_verbose_message_callback(NULL); /* verbose logging not allowed when using wildcards */

      return process_files(consoleId, file);
    }
    else
    {
      int result = process_file(consoleId, file);
      printf("\n");
      return result;
    }
  }

  usage(argv[0]);
  return 1;
}
