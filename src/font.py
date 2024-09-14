#! python

with open('font.bmp', 'rb') as f:
  data = bytearray(f.read())
  
# this assumes the file is a 4-bpp bitmap.

start = 0x42
width = 144
height = 272
stride = width / 2

with open('components/BitmapFont.h', 'w') as f:
  f.write("/*\n");
  f.write("Copyright (C) 2024 Brian Weiss\n");
  f.write("\n");
  f.write("This file is part of RALibretro.\n");
  f.write("\n");
  f.write("RALibretro is free software: you can redistribute it and/or modify\n");
  f.write("it under the terms of the GNU General Public License as published by\n");
  f.write("the Free Software Foundation, either version 3 of the License, or\n");
  f.write("(at your option) any later version.\n");
  f.write("\n");
  f.write("RALibretro is distributed in the hope that it will be useful,\n");
  f.write("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
  f.write("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
  f.write("GNU General Public License for more details.\n");
  f.write("\n");
  f.write("You should have received a copy of the GNU General Public License\n");
  f.write("along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.\n");
  f.write("*/\n");
  f.write("\n");
  f.write("#pragma once\n");
  f.write("\n");
  f.write("/* https://github.com/epto/epto-fonts/blob/master/font-images/img/simple-8x16.png */\n");
  f.write("\n");

  f.write("static uint8_t _bitmapFont[] = {\n")
  
  for c in range(0x20, 0x80):
    f.write('  /* ' )
    f.write(hex(c))
    f.write(': ')
    f.write(chr(c))
    f.write(" */\n")
    
    cy = (int)(c // 16)
    cx = (int)(c % 16)
    input = (height - 1 - cy * 17) * width + cx * 9
    for y in range(0, 16):
      f.write('  0b');
      for x in range(0, 8):
        i = input + x;
        b = data[(int)(start + i // 2)]
        if i & 1 == 0:
          f.write('1' if (b & 0xF0) != 0 else '0')
        else:
          f.write('1' if (b & 0x0F) != 0 else '0')

      f.write(",\n");
      input -= width;
    
    f.write("\n");

  f.write("  /* 0x80: unknown char */\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000,\n")
  f.write("  0b01111110,\n")
  f.write("  0b01100110,\n")
  f.write("  0b01100110,\n")
  f.write("  0b01100110,\n")
  f.write("  0b01100110,\n")
  f.write("  0b01100110,\n")
  f.write("  0b01111110,\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000,\n")
  f.write("  0b00000000\n")

  f.write("};\n");