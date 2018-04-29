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
along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "libretro/Components.h"
#include "Config.h"
#include "Gl.h"
#include "GlUtil.h"

#include <SDL_opengl.h>

#include <vector>

class Video: public libretro::VideoComponent
{
public:
  bool init(libretro::LoggerComponent* logger, Config* config);
  void destroy();

  void draw();

  virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback) override;
  virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override;

  virtual bool                 supportsContext(enum retro_hw_context_type type) override;
  virtual uintptr_t            getCurrentFramebuffer() override;
  virtual retro_proc_address_t getProcAddress(const char* symbol) override;

  virtual void showMessage(const char* msg, unsigned frames) override;

  void windowResized(unsigned width, unsigned height);
  void getFramebufferSize(unsigned* width, unsigned* height, enum retro_pixel_format* format);
  const void* getFramebufferRgba(unsigned* width, unsigned* height, unsigned* pitch);
  void setFramebufferRgb(const void* pixels, unsigned width, unsigned height, unsigned pitch);

  std::string serialize();
  void deserialize(const char* json);
  void showDialog();

protected:
  /*class Blitter: protected GlUtil::Program
  {
  public:
    bool init();
    void destroy() const;
  };

  struct OsdMessage
  {
    float x, y, t;
    GLuint vertexBuffer;
    GLsizei count;
  };*/

  GLuint createProgram(GLint* pos, GLint* uv, GLint* tex);
  //GLuint createOsdProgram(GLint* pos, GLint* uv, GLint* tex, GLint* time);
  GLuint createVertexBuffer(unsigned windowWidth, unsigned windowHeight, float texScaleX, float texScaleY, GLint pos, GLint uv);
  //void createOsd(OsdMessage* osd, const char* msg);

  libretro::LoggerComponent* _logger;
  Config* _config;

  GLuint                  _program;
  GLint                   _posAttribute;
  GLint                   _uvAttribute;
  GLint                   _texUniform;
  GLuint                  _vertexBuffer;
  GlUtil::Texture         _texture;

  GLuint                  _osdProgram;
  GLint                   _osdPosAttribute;
  GLint                   _osdUvAttribute;
  GLint                   _osdTexUniform;
  GLint                   _osdTimeUniform;
  GLuint                  _osdTexture;

  unsigned                _windowWidth;
  unsigned                _windowHeight;
  unsigned                _viewWidth;
  enum retro_pixel_format _pixelFormat;
  float                   _aspect;

  bool                    _preserveAspect;
  bool                    _linearFilter;

  //std::vector<OsdMessage> _messages;
};

