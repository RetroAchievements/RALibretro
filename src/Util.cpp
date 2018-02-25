#include "Util.h"

#include "SDLComponents/Logger.h"

#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

size_t nextPow2(size_t v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

void* loadFile(Logger* logger, const std::string& path, size_t* size)
{
  void* data;
  struct stat statbuf;

  if (stat(path.c_str(), &statbuf) != 0)
  {
    logger->printf(RETRO_LOG_ERROR, "Error getting info from \"%s\": %s", path.c_str(), strerror(errno));
    return NULL;
  }

  *size = statbuf.st_size;
  data = malloc(*size + 1);

  if (data == NULL)
  {
    logger->printf(RETRO_LOG_ERROR, "Out of memory allocating %lu bytes to load \"%s\"", *size, path);
    return NULL;
  }

  FILE* file = fopen(path.c_str(), "rb");

  if (file == NULL)
  {
    logger->printf(RETRO_LOG_ERROR, "Error opening \"%s\": %s", path, strerror(errno));
    free(data);
    return NULL;
  }

  size_t numread = fread(data, 1, *size, file);

  if (numread < 0 || numread != *size)
  {
    logger->printf(RETRO_LOG_ERROR, "Error reading \"%s\": %s", path, strerror(errno));
    fclose(file);
    free(data);
    return NULL;
  }

  fclose(file);
  *((uint8_t*)data + *size) = 0;
  return data;
}

bool saveFile(Logger* logger, const std::string& path, const void* data, size_t size)
{
  FILE* file = fopen(path.c_str(), "wb");

  if (file == NULL)
  {
    logger->printf(RETRO_LOG_ERROR, "Error opening file \"%s\": %s", path, strerror(errno));
    return false;
  }

  if (fwrite(data, 1, size, file) != size)
  {
    logger->printf(RETRO_LOG_ERROR, "Error writing file \"%s\": %s", path, strerror(errno));
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}
