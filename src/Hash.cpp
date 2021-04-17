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
#include <rc_hash.h>

#include <memory.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>

#define TAG "[HASH]"

//  Manages multi-disc games
extern void RA_ActivateDisc(int loadedGame);
extern void RA_DeactivateDisc();

static void rhash_handle_error_message(const char* message)
{
#ifdef _WINDOWS
  extern HWND g_mainWindow;
  MessageBoxA(g_mainWindow, (LPCSTR)message, "Unable to identify game", MB_OK);
#else
  fprintf(stderr, "Unable to identify game");
#endif
}

static Logger* g_logger = NULL;
static void* rhash_file_open(const char* path)
{
  return util::openFile(g_logger, path, "rb");
}

bool romLoaded(Logger* logger, int system, const std::string& path, void* rom, size_t size)
{
  unsigned int gameId = 0;
  char hash[33];
  struct rc_hash_filereader filereader;

  g_logger = logger;
  rc_hash_init_error_message_callback(rhash_handle_error_message);

  /* register a custom file_open handler for unicode support. use the default implementation for the other methods */
  memset(&filereader, 0, sizeof(filereader));
  filereader.open = rhash_file_open;
  rc_hash_init_custom_filereader(&filereader);

  rc_hash_init_default_cdreader();

  hash[0] = '\0';
  if (!rom || !rc_hash_generate_from_buffer(hash, system, (uint8_t*)rom, size))
    rc_hash_generate_from_file(hash, (int)system, path.c_str());

  if (!hash[0])
  {
    // could not generate hash, deactivate game, but return true to allow it to load
    RA_ActivateGame(0);
    RA_DeactivateDisc();
    return true;
  }

  gameId = RA_IdentifyHash(hash);

  switch (system)
  {
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_3DO:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_PC_ENGINE:
    case RC_CONSOLE_PLAYSTATION:
    case RC_CONSOLE_SATURN:
    case RC_CONSOLE_SEGA_CD:
      // these systems may be multi-disc systems - only call RA_ActivateGame if the game id changes
      RA_ActivateDisc(gameId);
      break;

    default:
      // all other systems should deactive any previously active disc
      RA_DeactivateDisc();
      RA_ActivateGame(gameId);
      break;
  }

  // return true to allow the game to load, even if it's not associated to achievements
  return true;
}

void romUnloaded(Logger* logger)
{
  RA_DeactivateDisc();
  RA_ActivateGame(0);
}
