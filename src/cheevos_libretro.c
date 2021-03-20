/* This file provides a series of functions for integrating RetroAchievements with libretro.
 * These functions will be called by a libretro frontend to validate certain expected behaviors
 * and simplify mapping core data to the RAIntegration DLL.
 * 
 * Originally designed to be shared between RALibretro and RetroArch, but will simplify
 * integrating with any other frontends.
 */

#include "cheevos_libretro.h"

/* this file comes from the libretro repository, which is not an explicit submodule.
 * the integration must set up paths appropriately to find it. */
#include <libretro.h>

#include <ctype.h>
#include <string.h>

/* a value that starts with a comma is a CSV.
 * if it starts with an exclamation point, it's everything but the provided value.
 * if it starts with an exclamntion point followed by a comma, it's everything but the CSV values.
 * values are case-insensitive */
typedef struct rc_disallowed_core_settings_t
{
  const char* library_name;
  const rc_disallowed_setting_t* disallowed_settings;
} rc_disallowed_core_settings_t;

static const rc_disallowed_setting_t _rc_disallowed_bsnes_settings[] = {
  { "bsnes_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_dolphin_settings[] = {
  { "dolphin_cheats_enabled", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ecwolf_settings[] = {
  { "ecwolf-invulnerability", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fbneo_settings[] = {
  { "fbneo-allow-patched-romsets", "enabled" },
  { "fbneo-cheat-*", "!,Disabled,0 - Disabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fceumm_settings[] = {
  { "fceumm_region", ",PAL,Dendy" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_settings[] = {
  { "genesis_plus_gx_lock_on", ",action replay (pro),game genie" },
  { "genesis_plus_gx_region_detect", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_wide_settings[] = {
  { "genesis_plus_gx_wide_lock_on", ",action replay (pro),game genie" },
  { "genesis_plus_gx_wide_region_detect", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_mesen_settings[] = {
  { "mesen_region", ",PAL,Dendy" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_mesen_s_settings[] = {
  { "mesen-s_region", "PAL" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_pcsx_rearmed_settings[] = {
  { "pcsx_rearmed_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_picodrive_settings[] = {
  { "picodrive_region", ",Europe,Japan PAL" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ppsspp_settings[] = {
  { "ppsspp_cheats", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_snes9x_settings[] = {
  { "snes9x_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_virtual_jaguar_settings[] = {
  { "virtualjaguar_pal", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_core_settings_t rc_disallowed_core_settings[] = {
  { "bsnes-mercury", _rc_disallowed_bsnes_settings },
  { "dolphin-emu", _rc_disallowed_dolphin_settings },
  { "ecwolf", _rc_disallowed_ecwolf_settings },
  { "FCEUmm", _rc_disallowed_fceumm_settings },
  { "FinalBurn Neo", _rc_disallowed_fbneo_settings },
  { "Genesis Plus GX", _rc_disallowed_gpgx_settings },
  { "Genesis Plus GX Wide", _rc_disallowed_gpgx_wide_settings },
  { "Mesen", _rc_disallowed_mesen_settings },
  { "Mesen-S", _rc_disallowed_mesen_s_settings },
  { "PPSSPP", _rc_disallowed_ppsspp_settings },
  { "PCSX-ReARMed", _rc_disallowed_pcsx_rearmed_settings },
  { "PicoDrive", _rc_disallowed_picodrive_settings },
  { "Snes9x", _rc_disallowed_snes9x_settings },
  { "Virtual Jaguar", _rc_disallowed_virtual_jaguar_settings },
  { NULL, NULL }
};

static int rc_libretro_string_equal_nocase(const char* test, const char* value)
{
  while (*test)
  {
    if (tolower(*test++) != tolower(*value++))
      return 0;
  }

  return (*value == '\0');
}

static int rc_libretro_match_value(const char* val, const char* match)
{
  /* if value starts with a comma, it's a CSV list of potential matches */
  if (*match == ',')
  {
    do
    {
      const char* ptr = ++match;
      int size;

      while (*match && *match != ',')
        ++match;

      size = match - ptr;
      if (val[size] == '\0')
      {
        if (memcmp(ptr, val, size) == 0)
        {
          return true;
        }
        else
        {
          char buffer[128];
          memcpy(buffer, ptr, size);
          buffer[size] = '\0';
          if (rc_libretro_string_equal_nocase(buffer, val))
            return true;
        }
      }
    } while (*match == ',');

    return false;
  }

  /* a leading exclamation point means the provided value(s) are not forbidden (are allowed) */
  if (*match == '!')
    return !rc_libretro_match_value(val, &match[1]);

  /* just a single value, attempt to match it */
  return rc_libretro_string_equal_nocase(val, match);
}

int rc_libretro_is_setting_allowed(const rc_disallowed_setting_t* disallowed_settings, const char* setting, const char* value)
{
  const char* key;
  size_t key_len;

  for (; disallowed_settings->setting; ++disallowed_settings)
  {
    key = disallowed_settings->setting;
    key_len = strlen(key);

    if (key[key_len - 1] == '*')
    {
      if (memcmp(setting, key, key_len - 1) == 0)
      {
        if (rc_libretro_match_value(value, disallowed_settings->value))
          return 0;
      }
    }
    else
    {
      if (memcmp(setting, key, key_len + 1) == 0)
      {
        if (rc_libretro_match_value(value, disallowed_settings->value))
          return 0;
      }
    }
  }

  return 1;
}

const rc_disallowed_setting_t* rc_libretro_get_disallowed_settings(const char* library_name)
{
  const rc_disallowed_core_settings_t* core_filter = rc_disallowed_core_settings;
  size_t library_name_length;

  if (!library_name || !library_name[0])
    return NULL;

  library_name_length = strlen(library_name) + 1;
  while (core_filter->library_name)
  {
    if (memcmp(core_filter->library_name, library_name, library_name_length) == 0)
      return core_filter->disallowed_settings;

    ++core_filter;
  }

  return NULL;
}
