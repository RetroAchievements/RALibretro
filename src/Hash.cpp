/*
Copyright (C) 2018 Andre Leiradella

This file is part of RALibretro.

RALibretro is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RALibretro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Hash.h"
#include "Util.h"

#include <RA_Interface.h>
#include <rhash.h>

#include <memory.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>

#define TAG "[HASH]"

//  Manages multi-disc games
extern void RA_ActivateDisc(unsigned char* pExe, size_t nExeSize);
extern void RA_DeactivateDisc();

static void rhash_handle_error_message(const char* message)
{
  extern HWND g_mainWindow;
  MessageBoxA(g_mainWindow, (LPCSTR)message, "Unable to identify game", MB_OK);
}

static Logger* g_logger = NULL;
static void* rhash_file_open(const char* path)
{
  return util::openFile(g_logger, path, "rb");
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

bool romLoaded(Logger* logger, System system, const std::string& path, void* rom, size_t size)
{
  unsigned int game_id = 0;
  char hash[33];
  struct rc_hash_filereader filereader;

  g_logger = logger;
  rc_hash_init_error_message_callback(rhash_handle_error_message);

  filereader.open = rhash_file_open;
  filereader.seek = rhash_file_seek;
  filereader.tell = rhash_file_tell;
  filereader.read = rhash_file_read;
  filereader.close = rhash_file_close;
  rc_hash_init_custom_filereader(&filereader);

  rc_hash_init_default_cdreader();

  hash[0] = '\0';
  if (!rom || !rc_hash_generate_from_buffer(hash, (int)system, (uint8_t*)rom, size))
    rc_hash_generate_from_file(hash, (int)system, path.c_str());

  if (hash[0])
  {
    game_id = RA_IdentifyHash(hash);
    RA_ActivateGame(game_id);
  }

  switch (system)
  {
    case System::kPlayStation1:
    case System::kSaturn:
    case System::kSegaCD:
      // these systems call RA_ActivateDisc and may be multi-disc systems
      break;

    default:
      // all other systems should deactive any previously active disc
      RA_DeactivateDisc();
      break;
  }

  return (game_id != 0);
}

void romUnloaded(Logger* logger)
{
  RA_DeactivateDisc();
  RA_ActivateGame(0);
}
