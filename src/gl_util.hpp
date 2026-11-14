#pragma once
#include <glad/glad.h>
#include "geometry.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

std::string shaderDir();  // directory containing shaders/ (next to the exe)

class Shader {
public:
    GLuint id = 0;
    // Loads shaders/<vs> and shaders/<fs>; `#include "common.glsl"` lines are expanded.
    static Shader load(const std::string& vs, const std::string& fs);
    void use() const { glUseProgram(id); }
    GLint loc(const char* name) const;
    void set(const char* n, float v) const { glUniform1f(loc(n), v); }
    void set(const char* n, int v) const { glUniform1i(loc(n), v); }
    void set(const char* n, const glm::vec2& v) const { glUniform2fv(loc(n), 1, &v[0]); }
    void set(const char* n, const glm::vec3& v) const { glUniform3fv(loc(n), 1, &v[0]); }
    void set(const char* n, const glm::vec4& v) const { glUniform4fv(loc(n), 1, &v[0]); }
    void set(const char* n, const glm::mat4& v) const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, &v[0][0]); }
    void setArr(const char* n, const glm::vec4* v, int count) const { glUniform4fv(loc(n), count, &v[0][0]); }

private:
    mutable std::unordered_map<std::string, GLint> cache_;
};

struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei count = 0;
    GLenum mode = GL_TRIANGLES;
    static Mesh create(const MeshData& d, GLenum mode = GL_TRIANGLES);
    void draw() const;
};

GLuint makeTexture2D(int w, int h, const unsigned char* rgba, bool mipmaps, bool repeat);
void checkGL(const char* where);
