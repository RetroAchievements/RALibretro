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
#include <rcheevos/src/rcheevos/rc_libretro.h>

#include <memory.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>

#define TAG "[HASH]"

//  Manages multi-disc games
static int g_activeGame = 0;
static rc_libretro_hash_set_t g_hashSet;

static void rhash_display_error_message(const char* message)
{
#ifdef _WINDOWS
  extern HWND g_mainWindow;
  MessageBoxA(g_mainWindow, (LPCSTR)message, "Unable to identify game", MB_OK);
#else
  fprintf(stderr, "Unable to identify game");
#endif
}

static Logger* g_logger = NULL;
static libretro::Core* g_core = NULL;
static void* rhash_file_open(const char* path)
{
  return util::openFile(g_logger, path, "rb");
}

static void rhash_log_error_message(const char* message)
{
  g_logger->warn(TAG "%s", message);
}

static int rhash_get_image_path(unsigned index, char* buffer, size_t buffer_size)
{
  std::string path;
  if (!g_core->getDiscPath(index, path))
    return 0;

  strncpy(buffer, path.c_str(), buffer_size);
  return 1;
}

bool romLoaded(libretro::Core* core, Logger* logger, int system, const std::string& path, void* rom, size_t size, bool changingDiscs)
{
  unsigned int gameId = 0;
  char hash[33];
  bool found = false;

  g_logger = logger;

  /* don't pop up error message when changing discs */
  if (changingDiscs)
    rc_hash_init_error_message_callback(rhash_log_error_message);
  else
    rc_hash_init_error_message_callback(rhash_display_error_message);

  hash[0] = '\0';

  if (changingDiscs)
  {
    const char* existingHash = rc_libretro_hash_set_get_hash(&g_hashSet, path.c_str());
    if (existingHash)
    {
      memcpy(hash, existingHash, sizeof(hash));
      gameId = rc_libretro_hash_set_get_game_id(&g_hashSet, existingHash);
      found = true;
    }
  }
  else
  {
    g_core = core;
    rc_libretro_hash_set_init(&g_hashSet, path.c_str(), rhash_get_image_path);
    g_core = NULL;
  }

  if (!found)
  {
    if (!hash[0])
    {
      struct rc_hash_filereader filereader;

      /* register a custom file_open handler for unicode support. use the default implementation for the other methods */
      memset(&filereader, 0, sizeof(filereader));
      filereader.open = rhash_file_open;
      rc_hash_init_custom_filereader(&filereader);

      rc_hash_init_default_cdreader();

      /* generate a hash for the new content */
      if (!rom || !rc_hash_generate_from_buffer(hash, system, (uint8_t*)rom, size))
        rc_hash_generate_from_file(hash, (int)system, path.c_str());
    }

    /* identify the game associated to the hash */
    gameId = hash[0] ? RA_IdentifyHash(hash) : 0;

    /* remember the hash/game so we don't have to look it up again if the user switches back */
    rc_libretro_hash_set_add(&g_hashSet, path.c_str(), gameId, hash);
  }

  if (!changingDiscs)
  {
    /* when not changing discs, just activate the new game */
    g_activeGame = gameId;
    RA_ActivateGame(gameId);
  }
  else if (gameId != 0)
  {
    /* when changing discs, if the disc is recognized, allow it. normally, this is going
     * to be the same game that's already loaded, but this also allows loading known game
     * discs for games that leverage user-provided discs. */
  }
  else if (!hash[0])
  {
    /* when changing discs, if the disc is not supported by the system, allow it. this is
     * primarily for games that support user-provided audio CDs, but does allow using discs
     * from other systems for games that leverage user-provided discs. */
  }
  else
  {
    /* an unrecognized disc could be a cheated game, don't allow it in hardcore */
    if (!RA_WarnDisableHardcore("switch to an unrecognized disc"))
      return false;
  }

  /* return true to allow the game to load, even if it's not associated to achievements */
  return true;
}

void romUnloaded(Logger* logger)
{
  rc_libretro_hash_set_destroy(&g_hashSet);
  g_activeGame = 0;
  RA_ActivateGame(0);
}
