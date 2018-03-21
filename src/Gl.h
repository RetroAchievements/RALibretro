#pragma once

#include "libretro/Components.h"

#include <SDL_opengl.h>

class Gl
{
public:
    Gl(libretro::LoggerComponent* logger);

    void genFramebuffers(GLsizei n, GLuint* ids);
    void genTextures(GLsizei n, GLuint* textures);
    void bindRenderbuffer(GLenum target, GLuint renderbuffer);
    void bindFramebuffer(GLenum target, GLuint framebuffer);
    void bindTexture(GLenum target, GLuint texture);
    void texImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data);
    void texParameteri(GLenum target, GLenum pname, GLint param);
    void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void enable(GLenum cap);
    void disable(GLenum cap);
    void blendFunc(GLenum sfactor, GLenum dfactor);

    bool ok() const { return _ok; }

protected:
    libretro::LoggerComponent* _logger;
    bool _ok;

    void check();

    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
    PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
};
