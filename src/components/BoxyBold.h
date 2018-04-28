#pragma once

#include <stdint.h>

typedef struct
{
  uint8_t x, y, w, h, ox, oy, dx;
}
glyph_t;

extern glyph_t g_boxybold[95];
