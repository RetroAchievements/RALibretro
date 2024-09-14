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

#pragma once

#include "libretro/Components.h"
#include "Config.h"
#include "Gl.h"

#include <SDL_opengl.h>

class Video: public libretro::VideoComponent
{
public:
  Video();

  bool init(libretro::LoggerComponent* logger, libretro::VideoContextComponent* ctx, Config* config);
  void destroy();

  virtual void setEnabled(bool enabled) override;

  void clear();
  void draw(bool force = false);

  virtual bool setGeometry(unsigned width, unsigned height, unsigned maxWidth, unsigned maxHeight, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback) override;
  virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override;
  virtual void reset() override;

  virtual bool                 supportsContext(enum retro_hw_context_type type) override;
  virtual uintptr_t            getCurrentFramebuffer() override;
  virtual retro_proc_address_t getProcAddress(const char* symbol) override;

  virtual void showMessage(const char* msg, unsigned frames) override;
  bool hasMessage() const { return _numMessages != 0; }

  void windowResized(unsigned width, unsigned height);
  void getFramebufferSize(unsigned* width, unsigned* height, enum retro_pixel_format* format);
  const void* getFramebuffer(unsigned* width, unsigned* height, unsigned* pitch, enum retro_pixel_format* format);
  void setFramebuffer(void* pixels, unsigned width, unsigned height, unsigned pitch);

  std::string serializeSettings();
  bool deserializeSettings(const char* json);
  void showDialog();

  unsigned getWindowWidth() const { return _windowWidth; }
  unsigned getWindowHeight() const { return _windowHeight; }

  unsigned getViewWidth() const { return _viewWidth; }
  unsigned getViewHeight() const { return _viewHeight; }

  unsigned getViewScaledWidth() const { return _viewScaledWidth; }
  unsigned getViewScaledHeight() const { return _viewScaledHeight; }

  void setRotation(Rotation rotation) override;
  Rotation getRotation() const override { return _rotation; }

  typedef void (*RotationHandler)(Rotation oldRotation, Rotation newRotation);
  void setRotationChangedHandler(RotationHandler handler) { _rotationHandler = handler; }

protected:
  GLuint createProgram(GLint* pos, GLint* uv, GLint* tex);
  bool ensureVertexArray(unsigned windowWidth, unsigned windowHeight, float texScaleX, float texScaleY, GLint pos, GLint uv);
  GLuint createTexture(unsigned width, unsigned height, retro_pixel_format pixelFormat, bool linear);
  bool ensureFramebuffer(unsigned width, unsigned height, retro_pixel_format pixelFormat, bool linearFilter);
  bool ensureView(unsigned width, unsigned height, unsigned windowWidth, unsigned windowHeight, bool preserveAspect, Rotation rotation);
  void clearErrors();

  libretro::LoggerComponent* _logger;
  libretro::VideoContextComponent* _ctx;
  Config* _config;

  bool                    _enabled;

  GLuint                  _program;
  GLint                   _posAttribute;
  GLint                   _uvAttribute;
  GLint                   _texUniform;
  GLuint                  _vertexArray;
  GLuint                  _vertexBuffer;
  GLuint                  _indentityVertexBuffer;
  GLuint                  _texture;

  GLuint                  _messageTexture[4];
  unsigned                _messageWidth[4];
  unsigned                _messageFrames[4];
  unsigned                _numMessages;

  unsigned                _windowWidth;
  unsigned                _windowHeight;
  unsigned                _textureWidth;
  unsigned                _textureHeight;
  unsigned                _viewWidth;
  unsigned                _viewHeight;
  unsigned                _viewScaledWidth;
  unsigned                _viewScaledHeight;
  enum retro_pixel_format _pixelFormat;
  float                   _aspect;
  Rotation                _rotation;
  RotationHandler         _rotationHandler;

  bool                    _preserveAspect;
  bool                    _linearFilter;

  struct {
    bool enabled;
    GLuint frameBuffer;
    GLuint renderBuffer;
    const retro_hw_render_callback *callback;
  }                       _hw;
};

