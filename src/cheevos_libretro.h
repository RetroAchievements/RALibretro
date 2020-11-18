#ifndef CHEEVOS_LIBRETRO_H
#define CHEEVOS_LIBRETRO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_disallowed_setting_t
{
  const char* setting;
  const char* value;
} rc_disallowed_setting_t;

const rc_disallowed_setting_t* rc_libretro_get_disallowed_settings(const char* library_name);
int rc_libretro_is_setting_allowed(const rc_disallowed_setting_t* disallowed_settings, const char* setting, const char* value);

#ifdef __cplusplus
}
#endif

#endif /* CHEEVOS_LIBRETRO_H */
