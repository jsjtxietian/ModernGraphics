#include "GLshader.h"
#include "Utils/Utils.h"
#include <assert.h>
#include <stdio.h>
#include <string>

GLShader::GLShader(const char *fileName)
    : GLShader(GLShaderTypeFromFileName(fileName), readShaderFile(fileName).c_str(), fileName)
{
}

GLShader::GLShader(GLenum type, const char *text, const char *debugFileName)
    : type_(type), handle_(glCreateShader(type))
{
    glShaderSource(handle_, 1, &text, nullptr);
    glCompileShader(handle_);

    char buffer[8192];
    GLsizei length = 0;
    glGetShaderInfoLog(handle_, sizeof(buffer), &length, buffer);

    if (length != 0)
    {
        printf("%s (File: %s)\n", buffer, debugFileName);
        printShaderSource(text);
        assert(false);
    }
}

GLShader::~GLShader()
{
    glDeleteShader(handle_);
}

//little helper func
void printProgramInfoLog(GLuint handle)
{
    char buffer[8192];
    GLsizei length = 0;
    glGetProgramInfoLog(handle, sizeof(buffer), &length, buffer);
    if (length)
    {
        printf("%s\n", buffer);
        assert(false);
    }
}

// maybe link a single shader to a separate, standalone shader program and combine those programs into a program pipeline
// const char* vtx = ...
// const char* frg = ...
// const GLuint vs = glCreateShaderProgramv(
// GL_VERTEX_SHADER, 1, &vtx);
// const GLuint fs = glCreateShaderProgramv(
// GL_FRAGMENT_SHADER, 1, &frg);
// GLuint pipeline;
// glCreateProgramPipelines(1, &pipeline);
// glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, vs);
// glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, fs);
// glBindProgramPipeline(pipeline);
GLProgram::GLProgram(const GLShader &a)
    : handle_(glCreateProgram())
{
    glAttachShader(handle_, a.getHandle());
    glLinkProgram(handle_);
    printProgramInfoLog(handle_);
}

GLProgram::GLProgram(const GLShader &a, const GLShader &b)
    : handle_(glCreateProgram())
{
    glAttachShader(handle_, a.getHandle());
    glAttachShader(handle_, b.getHandle());
    glLinkProgram(handle_);
    printProgramInfoLog(handle_);
}

GLProgram::GLProgram(const GLShader &a, const GLShader &b, const GLShader &c)
    : handle_(glCreateProgram())
{
    glAttachShader(handle_, a.getHandle());
    glAttachShader(handle_, b.getHandle());
    glAttachShader(handle_, c.getHandle());
    glLinkProgram(handle_);
    printProgramInfoLog(handle_);
}

GLProgram::GLProgram(const GLShader &a, const GLShader &b, const GLShader &c, const GLShader &d, const GLShader &e)
    : handle_(glCreateProgram())
{
    glAttachShader(handle_, a.getHandle());
    glAttachShader(handle_, b.getHandle());
    glAttachShader(handle_, c.getHandle());
    glAttachShader(handle_, d.getHandle());
    glAttachShader(handle_, e.getHandle());
    glLinkProgram(handle_);
    printProgramInfoLog(handle_);
}

GLProgram::~GLProgram()
{
    glDeleteProgram(handle_);
}

void GLProgram::useProgram() const
{
    glUseProgram(handle_);
}

GLenum GLShaderTypeFromFileName(const char *fileName)
{
    if (endsWith(fileName, ".vert"))
        return GL_VERTEX_SHADER;

    if (endsWith(fileName, ".frag"))
        return GL_FRAGMENT_SHADER;

    if (endsWith(fileName, ".geom"))
        return GL_GEOMETRY_SHADER;

    if (endsWith(fileName, ".tesc"))
        return GL_TESS_CONTROL_SHADER;

    if (endsWith(fileName, ".tese"))
        return GL_TESS_EVALUATION_SHADER;

    if (endsWith(fileName, ".comp"))
        return GL_COMPUTE_SHADER;

    assert(false);

    return 0;
}
