// RAHasher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "Git.h"
#include "Hash.h"
#include "Util.h"

#include <md5/md5.h>

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

int main(int argc, char* argv[])
{
  if (argc == 3)
  {
    const System system = (System)atoi(argv[1]);
    std::string file = argv[2];

    if (system != System::kNone && !file.empty())
    {
      std::unique_ptr<StdErrLogger> logger(new StdErrLogger);
      if (generateHash(logger.get(), system, file))
      {
        printf("%s\n", md5_hash_result.c_str());
      }

      return md5_hash_result.empty() ? 1 : 0;
    }
  }

  usage(argv[0]);
  return 1;
}
