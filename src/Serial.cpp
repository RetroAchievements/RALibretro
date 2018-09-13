#include "Serial.h"
#include "Emulator.h"

#include <stdio.h>

typedef FILE* intfstream_t;
typedef void* database_state_handle_t;
typedef void* database_info_handle_t;

#define RARCH_LOG(...)

static intfstream_t *intfstream_open_file(const char* name, const char* mode, int hint)
{
  (void)hint;

  intfstream_t* fd = (intfstream_t*)malloc(sizeof(*fd));

  if (fd != NULL)
  {
    *fd = fopen(name, mode);
  }

  return fd;
}

static int intfstream_seek(intfstream_t *fd, long offset, int whence)
{
  return fseek(*fd, offset, whence);
}

static long intfstream_tell(intfstream_t *fd)
{
  return ftell(*fd);
}

static size_t intfstream_read(intfstream_t *fd, void* data, size_t size)
{
  return fread(data, 1, size, *fd);
}

static void intfstream_close(intfstream_t *fd)
{
  fclose(*fd);
}

static intfstream_t *intfstream_open_memory(void* data, const char* mode, int hint, size_t size)
{
  (void)hint;
  return fmemopen(data, size, mode);
}

static int intfstream_get_serial(intfstream_t *fd, char *serial, System system)
{
  if (false)

#if 0
  else if (system == System::kWii)
  {
    /* Attempt to read an ASCII serial, like Wii. */
    if (detect_serial_ascii_game(fd, serial))
    {
      /* ASCII serial (Wii) was detected. */
      RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
      return 1;
    }

    /* Any other non-system specific detection methods? */
    return 0;
  }
#endif

#if 0
  else if (system == System::kPlayStationPortable)
  {
    if (detect_psp_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
#endif

  else if (system == System::kPlayStation1)
  {
    if (detect_ps1_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }

#if 0
  else if (system == System::kGameCure)
  {
    if (detect_gc_game(fd, serial) == 0)
      return 0;
    RARCH_LOG("%s '%s'\n", msg_hash_to_str(MSG_FOUND_DISK_LABEL), serial);
  }
#endif

  else {
    return 0;
  }

  return 1;
}

static bool intfstream_file_get_serial(const char *name,
      size_t offset, size_t size, char *serial)
{
   int rv;
   uint8_t *data     = NULL;
   ssize_t file_size = -1;
   intfstream_t *fd  = intfstream_open_file(name,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!fd)
      return 0;

   if (intfstream_seek(fd, 0, SEEK_END) == -1)
      goto error;

   file_size = intfstream_tell(fd);

   if (intfstream_seek(fd, 0, SEEK_SET) == -1)
      goto error;

   if (file_size < 0)
      goto error;

   if (offset != 0 || size < (size_t) file_size)
   {
      if (intfstream_seek(fd, offset, SEEK_SET) == -1)
         goto error;

      data = (uint8_t*)malloc(size);

      if (intfstream_read(fd, data, size) != (ssize_t) size)
      {
         free(data);
         goto error;
      }

      intfstream_close(fd);
      free(fd);
      fd = intfstream_open_memory(data, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE,
            size);
      if (!fd)
      {
         free(data);
         return 0;
      }
   }

   rv = intfstream_get_serial(fd, serial, system);
   intfstream_close(fd);
   free(fd);
   free(data);
   return rv;

error:
   intfstream_close(fd);
   free(fd);
   return 0;
}

static int task_database_cue_get_serial(const char *name, char* serial)
{
   char *track_path                 = (char*)malloc(PATH_MAX_LENGTH
         * sizeof(char));
   int ret                          = 0;
   size_t offset                    = 0;
   size_t size                      = 0;
   int rv                           = 0;

   track_path[0]                    = '\0';

   rv = cue_find_track(name, true, &offset, &size, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   ret = intfstream_file_get_serial(track_path, offset, size, serial);
   free(track_path);

   return ret;
}

static int task_database_gdi_get_serial(const char *name, char* serial)
{
   char *track_path                 = (char*)malloc(PATH_MAX_LENGTH
         * sizeof(char));
   int ret                          = 0;
   int rv                           = 0;

   track_path[0]                    = '\0';

   rv = gdi_find_track(name, true, track_path, PATH_MAX_LENGTH);

   if (rv < 0)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_FIND_VALID_DATA_TRACK),
            strerror(-rv));
      free(track_path);
      return 0;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_READING_FIRST_DATA_TRACK));

   ret = intfstream_file_get_serial(track_path, 0, SIZE_MAX, serial);
   free(track_path);

   return ret;
}

static int task_database_chd_get_serial(const char *name, char* serial)
{
   int result;
   intfstream_t *fd = intfstream_open_chd_track(
         name,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         CHDSTREAM_TRACK_FIRST_DATA);
   if (!fd)
      return 0;

   result = intfstream_get_serial(fd, serial, system);
   intfstream_close(fd);
   free(fd);
   return result;
}

static int task_database_iterate_playlist(const char* name, char* serial)
{
  const char* ext = strrchr(name, '.');

  if (ext == NULL)
  {
    return 0;
  }
   
  if (!strcmp(ext, ".cue"))
  {
    return task_database_cue_get_serial(name, serial);
  }

  if (!strcmp(ext, ".gdi"))
  {
    return 0 && task_database_gdi_get_serial(name, serial);
  }

  if (!strcmp(ext, ".iso"))
  {
    return intfstream_file_get_serial(name, 0, SIZE_MAX, serial);
  }

  if (!strcmp(ext, ".chd"))
  {
    return task_database_chd_get_serial(name, serial);
  }

  return 0;
}
