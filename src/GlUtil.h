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

  struct Attribute
  {
    GLint location;
  };

  struct Uniform
  {
    GLint location;
  };

  class Program
  {
  public:
    Program(): _program(0) {}

    bool init(const char* vertexShader, const char* fragmentShader);
    void destroy();

    Attribute getAttribute(const char* name) const;
    Uniform getUniform(const char* name) const;

    void use() const;

  protected:
    GLuint _program;
  };

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

    void setUniform(Uniform uniform, int unit) const;
  
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

    bool init();
    void destroy();

    bool setData(const void* data, size_t dataSize);
    void bind() const;

    void enable(Attribute attribute, GLint size, GLenum type, GLsizei stride, GLsizei offset) const;

    void draw(GLenum mode, GLsizei count) const;

  protected:
    GLuint _vbo;
  };

  class TexturedQuad: protected VertexBuffer
  {
  public:
    bool init();
    bool init(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1);
    void destroy();

    void bind() const;

    void enablePos(Attribute attribute) const;
    void enableUV(Attribute attribute) const;

    void draw() const;
  
  protected:
    struct Vertex
    {
      float x, y, u, v;
    };
  };

  class TexturedTriangleBatch: protected VertexBuffer
  {
  public:
    struct Vertex
    {
      float x, y, u, v;
    };

    bool init(const Vertex* vertices, size_t count);
    void destroy();

    void bind() const;

    void enablePos(Attribute attribute) const;
    void enableUV(Attribute attribute) const;

    void draw() const;
  
  protected:
    GLsizei _count;
  };
}
