// RAHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Git.h"
#include "Hash.h"
#include "Util.h"

#include <md5/md5.h>

#include "rcheevos\include\rhash.h"

#include <memory>
#include <string.h>

static void usage(const char* appname)
{
  printf("RAHasher %s\n====================\n", git::getReleaseVersion());

  printf("Usage: %s [-v] systemid filepath\n", util::fileName(appname).c_str());
  printf("\n");
  printf("  -v           (optional) enables verbose messages for debugging\n");
  printf("  systemid     specifies the system id associated to the game (which hash algorithm to use)\n");
  printf("  filepath     specifies the path to the game file\n");
}

class StdErrLogger : public Logger
{
public:
  void vprintf(enum retro_log_level level, const char* fmt, va_list args) override
  {
    // ignore non-errors unless they're coming from the hashing code
    if (level != RETRO_LOG_ERROR && strncmp(fmt, "[HASH]", 6) != 0)
      return;

    // don't print the category
    while (*fmt && *fmt != ']')
      ++fmt;
    while (*fmt == ']' || *fmt == ' ')
      ++fmt;

    ::vfprintf(stderr, fmt, args);
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

static void rhash_file_seek(void* file_handle, size_t offset, int origin)
{
  fseek((FILE*)file_handle, offset, origin);
}

static size_t rhash_file_tell(void* file_handle)
{
  return ftell((FILE*)file_handle);
}

static size_t rhash_file_read(void* file_handle, void* buffer, size_t requested_bytes)
{
  return fread(buffer, 1, requested_bytes, (FILE*)file_handle);
}

static void rhash_file_close(void* file_handle)
{
  fclose((FILE*)file_handle);
}

int main(int argc, char* argv[])
{
  System system = System::kNone;
  std::string file;
  char hash[33];
  int result = 1;

  if (argc == 3)
  {
    system = (System)atoi(argv[1]);
    file = argv[2];
  }
  else if (argc == 4 && strcmp(argv[1], "-v") == 0)
  {
    rc_hash_init_verbose_message_callback(rhash_log);

    system = (System)atoi(argv[2]);
    file = argv[3];
  }

  if (system != System::kNone && !file.empty())
  {
    logger.reset(new StdErrLogger);

    rc_hash_init_error_message_callback(rhash_log_error);

    std::string ext = util::extension(file);
    if (system != System::kArcade && (int)system < 99 && ext.length() == 4 &&
      tolower(ext[1]) == 'z' && tolower(ext[2]) == 'i' && tolower(ext[3]) == 'p')
    {
      std::string unzippedFilename;
      size_t size;
      void* data = util::loadZippedFile(logger.get(), file, &size, unzippedFilename);
      if (data)
      {
        if (rc_hash_generate_from_buffer(hash, (int)system, (uint8_t*)data, size))
          printf("%s\n", hash);

        free(data);
      }
    }
    else
    {
      struct rc_hash_filereader filereader;
      filereader.open = rhash_file_open;
      filereader.seek = rhash_file_seek;
      filereader.tell = rhash_file_tell;
      filereader.read = rhash_file_read;
      filereader.close = rhash_file_close;
      rc_hash_init_custom_filereader(&filereader);

      rc_hash_init_default_cdreader();

      if ((int)system >= 99)
      {
        rc_hash_iterator iterator;
        rc_hash_initialize_iterator(&iterator, file.c_str(), NULL, 0);
        while (rc_hash_iterate(hash, &iterator))
          printf("%s\n", hash);
        rc_hash_destroy_iterator(&iterator);
      }
      else
      {
        if (rc_hash_generate_from_file(hash, (int)system, file.c_str()))
          printf("%s\n", hash);
      }
    }

    return result;
  }

  usage(argv[0]);
  return 1;
}
