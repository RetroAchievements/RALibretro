-- Script to create the C font atlas

io.write[[
typedef struct
{
  uint8_t x, y, w, h, ox, oy, dx;
}
glyph_t;

glyph_t glyhps[95] =
{
]]

local file = assert(io.open('boxy_bold_font.txt'))
local glyphs = {}

for line in file:lines() do
  if line:sub(1, 4) == 'char' then
    -- char id=34 x=6 y=0 width=7 height=5 xoffset=0 yoffset=0 xadvance=6 page=0 chnl=15 //"
    local id = line:match('id=(%d+)') + 0
    local x = line:match('x=(%d+)') + 0
    local y = line:match('y=(%d+)') + 0
    local width = line:match('width=(%d+)') + 0
    local height = line:match('height=(%d+)') + 0
    local x0 = line:match('xoffset=(%d+)') + 0
    local y0 = line:match('yoffset=(%d+)') + 0
    local dx = line:match('xadvance=(%d+)') + 0

    glyphs[id] = {x = x, y = y, w = width, h = height, ox = x0, oy = y0, dx = dx}
  end
end

file:close()

for i = 32, 126 do
  local k = string.char(i):upper():byte(1)
  local g = glyphs[k]

  io.write(string.format('  {%3d, %2d, %2d, %d, %d, %d, %d}, /* %3d %c */\n', g.x, g.y, g.w, g.h, g.ox, g.oy, g.dx, i, i))
end

io.write[[
};
]]
