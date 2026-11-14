#include "gl_util.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

std::string shaderDir() {
    static std::string dir;
    if (dir.empty()) {
#ifdef _WIN32
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string p(buf);
        dir = p.substr(0, p.find_last_of("\\/") + 1) + "shaders/";
#else
        dir = "shaders/";
#endif
    }
    return dir;
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string expandIncludes(const std::string& src) {
    std::stringstream in(src), out;
    std::string line;
    while (std::getline(in, line)) {
        auto p = line.find("#include \"");
        if (p != std::string::npos) {
            auto q = line.find('"', p + 10);
            out << readFile(shaderDir() + line.substr(p + 10, q - p - 10)) << "\n";
        } else {
            out << line << "\n";
        }
    }
    return out.str();
}

static GLuint compile(GLenum type, const std::string& src, const std::string& name) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        throw std::runtime_error("shader compile error in " + name + ":\n" + log);
    }
    return s;
}

Shader Shader::load(const std::string& vs, const std::string& fs) {
    std::string vsrc = expandIncludes(readFile(shaderDir() + vs));
    std::string fsrc = expandIncludes(readFile(shaderDir() + fs));
    GLuint v = compile(GL_VERTEX_SHADER, vsrc, vs), f = compile(GL_FRAGMENT_SHADER, fsrc, fs);
    Shader sh;
    sh.id = glCreateProgram();
    glAttachShader(sh.id, v);
    glAttachShader(sh.id, f);
    glLinkProgram(sh.id);
    GLint ok;
    glGetProgramiv(sh.id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(sh.id, sizeof log, nullptr, log);
        throw std::runtime_error("link error " + vs + "+" + fs + ":\n" + log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    GLuint blk = glGetUniformBlockIndex(sh.id, "Frame");  // shared frame UBO lives at binding 0
    if (blk != GL_INVALID_INDEX) glUniformBlockBinding(sh.id, blk, 0);
    return sh;
}

GLint Shader::loc(const char* name) const {
    auto it = cache_.find(name);
    if (it != cache_.end()) return it->second;
    GLint l = glGetUniformLocation(id, name);
    cache_[name] = l;
    return l;
}

Mesh Mesh::create(const MeshData& d, GLenum mode) {
    Mesh m;
    m.mode = mode;
    m.count = (GLsizei)d.idx.size();
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, d.verts.size() * sizeof(Vertex), d.verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, d.idx.size() * sizeof(uint32_t), d.idx.data(), GL_STATIC_DRAW);
    auto attr = [](GLuint i, GLint n, size_t off) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, n, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)off);
    };
    attr(0, 3, offsetof(Vertex, pos));
    attr(1, 3, offsetof(Vertex, normal));
    attr(2, 2, offsetof(Vertex, uv));
    attr(3, 3, offsetof(Vertex, color));
    attr(4, 1, offsetof(Vertex, mat));
    glBindVertexArray(0);
    return m;
}

void Mesh::draw() const {
    if (!count) return;
    glBindVertexArray(vao);
    glDrawElements(mode, count, GL_UNSIGNED_INT, nullptr);
}

GLuint makeTexture2D(int w, int h, const unsigned char* rgba, bool mipmaps, bool repeat) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    return t;
}

void checkGL(const char* where) {
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR) std::fprintf(stderr, "GL error 0x%04x at %s\n", e, where);
}
