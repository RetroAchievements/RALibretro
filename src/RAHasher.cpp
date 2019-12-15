// RAHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Git.h"
#include "Hash.h"
#include "Util.h"

#include <md5/md5.h>

#include "rcheevos\include\rhash.h"

#include <memory>
#include <string.h>

// sneaky hackery - hijack RA_OnLoadNewRomm, RA_IdentifyRom, and RA_ActivateDisc
std::string md5_hash_result;
unsigned int RA_IdentifyRom(unsigned char* buffer, unsigned int bufferSize)
{
  md5_byte_t digest[16];

  md5_state_t md5;
  md5_init(&md5);
  md5_append(&md5, buffer, bufferSize);
  md5_finish(&md5, digest);

  md5_hash_result.resize(32);
  sprintf((char*)md5_hash_result.data(), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  return 0;
}

void RA_OnLoadNewRom(unsigned char* buffer, unsigned int bufferSize)
{
  RA_IdentifyRom(buffer, bufferSize);
}

void RA_ActivateDisc(unsigned char* pExe, size_t nExeSize)
{
  RA_IdentifyRom(pExe, nExeSize);
}

// provide dummy methods for other functions
unsigned int RA_IdentifyHash(const char* sHash) { return 0U; }
void RA_ActivateGame(unsigned int nGameId) {}
void RA_DeactivateDisc() {}

static bool generateHash(Logger* logger, System system, const std::string& path)
{
  void* data = NULL;
  size_t size = 0;
  bool result;

  md5_hash_result.clear();

  switch (system)
  {
    case System::kPlayStation1:
    case System::kSegaCD:
    case System::kSaturn:
    case System::kArcade:
      // don't preload the file for these systems
      break;

    default:
      if (path.length() > 4 && strcasecmp(&path[path.length() - 4], ".zip") == 0)
      {
        std::string unzippedFileName;
        data = util::loadZippedFile(logger, path, &size, unzippedFileName);
        if (data == NULL)
          return false;
      }
      else
      {
        data = util::loadFile(logger, path, &size);
        if (data == NULL)
          return false;
      }
      break;
  }

  result = romLoaded(logger, system, path, data, size);

  if (data)
    free(data);

  return result;
}

static void usage(const char* appname)
{
  printf("RAHasher %s\n====================\n", git::getReleaseVersion());

  printf("Usage: %s [systemid] [filepath]\n", util::fileName(appname).c_str());
  printf("\n");
  printf("  filepath     specifies the path to the game file\n");
  printf("  systemid     specifies the system id associated to the game (which hash algorithm to use)\n");
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

std::unique_ptr<StdErrLogger> logger;

static void rhash_log(const char* message)
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
  if (argc == 3)
  {
    const System system = (System)atoi(argv[1]);
    std::string file = argv[2];

    if (system != System::kNone && !file.empty())
    {
      logger.reset(new StdErrLogger);

      //if (generateHash(logger.get(), system, file))
      //{
      //  printf("%s\n", md5_hash_result.c_str());
      //}

      char hash[33];
      rc_hash_init_error_message_callback(rhash_log);

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
          {
            printf("%s\n", hash);
          }

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
          {
            printf("%s\n", hash);
          }
        }
      }

      return md5_hash_result.empty() ? 1 : 0;
    }
  }

  usage(argv[0]);
  return 1;
}
