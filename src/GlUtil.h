#pragma once

#include "libretro/Components.h"

#include "Gl.h"

namespace GlUtil
{
  void init(libretro::LoggerComponent* logger);

  GLuint createTexture(GLsizei width, GLsizei height, GLint internalFormat, GLenum format, GLenum type, GLenum filter);
  GLuint createShader(GLenum shaderType, const char* source);
  GLuint createProgram(const char* vertexShader, const char* fragmentShader);
  GLuint createFramebuffer(GLuint *renderbuffer, GLsizei width, GLsizei height, GLuint texture, bool depth, bool stencil);

  class Texture
  {
  public:
    Texture(): _texture(0) {}

    bool init(GLsizei width, GLsizei height, GLint internalFormat, bool linearFilter);
    void destroy();

    void  setData(GLsizei width, GLsizei height, GLsizei pitch, GLenum format, GLenum type, const GLvoid* pixels);
    void* getData(GLenum format, GLenum type) const;

    void  bind() const;

    GLsizei getWidth() const;
    GLsizei getHeight() const;

    void setUniform(GLint uniformLocation, int unit) const;
  
  protected:
    static GLsizei bpp(GLenum type);

    GLuint  _texture;
    GLsizei _width;
    GLsizei _height;
    GLint   _internalFormat;
  };

  class VertexBuffer
  {
  public:
    VertexBuffer(): _vbo(0) {}

    bool init(size_t vertexSize);
    void destroy();

    bool setData(const void* data, size_t dataSize);
    void bind() const;

    size_t getVertexSize() const;

  protected:
    GLuint _vbo;
    size_t _vertexSize;
  };

  class VertexAttribute
  {
  public:
    bool init(GLenum type, GLint numElements, size_t offset);
    void destroy() const;

    void enable(const VertexBuffer* vertexBuffer, GLint attributeLocation) const;
  
  protected:
    GLenum _type;
    GLint  _numElements;
    size_t _offset;
  };

  class Program
  {
  public:
    bool init(const char* vertexShader, const char* fragmentShader);
    void destroy() const;

    GLint getAttribute(const char* name) const;
    GLint getUniform(const char* name) const;

    void setVertexAttribute(GLuint attributeLocation, const VertexAttribute* vertexAttribute, const VertexBuffer* vertexBuffer);
    void setTextureUniform(GLint uniformLocation, GLint texture, int unit);
  
  protected:
    GLuint _program;
  };
}
