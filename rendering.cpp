#include "rendering.h"
#include "gl_includes.h"
#include "world.h"

#include <vector>
#include <cstddef>
#include <iostream>
#include <string>
#include <stdexcept>

namespace rendering {

    // Vertex layout: position, normal, color, uv
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;
    };

    static std::vector<const Block*> s_plane;
    static int s_planeY = 0;
    static int s_width = WORLD_WIDTH;

    // GL objects
    static GLuint s_vao = 0;
    static GLuint s_vbo = 0;
    static GLuint s_ebo = 0;
    static GLsizei s_indexCount = 0;
    static GLuint s_program = 0;

    // Texture atlas pointer (not owned)
    static TextureAtlas* s_atlas = nullptr;

    inline int idx(int x, int z) { return x + z * s_width; }

    const Block* GetBlockAt(int x, int z) {
        if (x < 0 || z < 0 || x >= s_width || z >= s_width) return nullptr;
        return s_plane[idx(x, z)];
    }
    void SetBlockAt(int x, int z, const Block* blk) {
        if (x < 0 || z < 0 || x >= s_width || z >= s_width) return;
        s_plane[idx(x, z)] = blk;
    }

    // Simple GLSL sources (330 core)
    static const char* vertex_src = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;

void main() {
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;
    vUV = aUV;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
)";

    static const char* fragment_src = R"(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uAtlas;
uniform int uUseAtlas;
uniform vec3 uLightDir;
uniform vec3 uAmbient;

void main() {
    float NdotL = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    vec3 baseColor = vColor;
    if (uUseAtlas == 1) {
        vec4 t = texture(uAtlas, vUV);
        baseColor = t.rgb;
    }
    vec3 color = baseColor * (uAmbient + 0.7 * NdotL);
    FragColor = vec4(color, 1.0);
}
)";

    static GLuint compileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, '\0');
            glGetShaderInfoLog(s, len, nullptr, &log[0]);
            std::cerr << "Shader compile error: " << log << "\n";
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    static GLuint linkProgram(GLuint vs, GLuint fs) {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, '\0');
            glGetProgramInfoLog(p, len, nullptr, &log[0]);
            std::cerr << "Program link error: " << log << "\n";
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }

    static void EnsureProgram() {
        if (s_program) return;
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertex_src);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragment_src);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            throw std::runtime_error("Failed to compile shaders");
        }
        s_program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        // set some sensible defaults in the program (optional)
        glUseProgram(s_program);
        GLint locLight = glGetUniformLocation(s_program, "uLightDir");
        GLint locAmbient = glGetUniformLocation(s_program, "uAmbient");
        if (locLight >= 0) glUniform3f(locLight, 0.3f, 1.0f, 0.5f);
        if (locAmbient >= 0) glUniform3f(locAmbient, 0.3f, 0.3f, 0.3f);
        glUseProgram(0);
    }

    static void pushQuad(std::vector<Vertex>& verts, std::vector<uint32_t>& inds,
        const glm::vec3 positions[4], const glm::vec3& normal,
        const glm::vec3& color, const glm::vec2 uvs[4]) {
        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back({ positions[0], normal, color, uvs[0] });
        verts.push_back({ positions[1], normal, color, uvs[1] });
        verts.push_back({ positions[2], normal, color, uvs[2] });
        verts.push_back({ positions[3], normal, color, uvs[3] });
        inds.push_back(base + 0);
        inds.push_back(base + 1);
        inds.push_back(base + 2);
        inds.push_back(base + 2);
        inds.push_back(base + 3);
        inds.push_back(base + 0);
    }

    static std::array<glm::vec2, 4> GetFaceUVsFromAtlas(const Block* b, int face) {
        std::array<glm::vec2, 4> uv;
        if (s_atlas && s_atlas->IsValid()) {
            int tile = b->GetTileForFace(face);
            float u0, v0, u1, v1;
            s_atlas->GetTileUV(tile, u0, v0, u1, v1);
            // Map to quad corners consistent with pushQuad ordering
            uv[0] = glm::vec2(u0, v0); // bottom-left
            uv[1] = glm::vec2(u1, v0); // bottom-right
            uv[2] = glm::vec2(u1, v1); // top-right
            uv[3] = glm::vec2(u0, v1); // top-left
        }
        else {
            uv[0] = glm::vec2(0.0f, 0.0f);
            uv[1] = glm::vec2(1.0f, 0.0f);
            uv[2] = glm::vec2(1.0f, 1.0f);
            uv[3] = glm::vec2(0.0f, 1.0f);
        }
        return uv;
    }

    static void BuildMesh() {
        // free old GL resources
        if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
        if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
        if (s_ebo) { glDeleteBuffers(1, &s_ebo); s_ebo = 0; }
        s_indexCount = 0;

        std::vector<Vertex> verts;
        std::vector<uint32_t> inds;

        // conservative reserve to avoid many reallocations
        const size_t estFaces = static_cast<size_t>(s_width) * static_cast<size_t>(s_width) * 4; // rough
        try {
            verts.reserve(estFaces * 4);
            inds.reserve(estFaces * 6);
        }
        catch (const std::bad_alloc& e) {
            std::cerr << "BuildMesh: reserve failed: " << e.what() << "\n";
        }

        int half = s_width / 2;

        for (int z = 0; z < s_width; ++z) {
            for (int x = 0; x < s_width; ++x) {
                const Block* b = GetBlockAt(x, z);
                if (!b) continue;

                float wx = float(x - half);
                float wy = float(s_planeY);
                float wz = float(z - half);
                glm::vec3 color = b->getColor();

                // Top (y+)
                {
                    glm::vec3 p0 = glm::vec3(wx, wy + 1.0f, wz);
                    glm::vec3 p1 = glm::vec3(wx + 1, wy + 1.0f, wz);
                    glm::vec3 p2 = glm::vec3(wx + 1, wy + 1.0f, wz + 1);
                    glm::vec3 p3 = glm::vec3(wx, wy + 1.0f, wz + 1);
                    glm::vec3 normal = glm::vec3(0, 1, 0);
                    auto uvs = GetFaceUVsFromAtlas(b, FACE_TOP);
                    const glm::vec3 posArr[4] = { p0,p1,p2,p3 };
                    pushQuad(verts, inds, posArr, normal, color, &uvs[0]);
                }

                // Bottom (y-)
                {
                    glm::vec3 p0 = glm::vec3(wx, wy, wz + 1);
                    glm::vec3 p1 = glm::vec3(wx + 1, wy, wz + 1);
                    glm::vec3 p2 = glm::vec3(wx + 1, wy, wz);
                    glm::vec3 p3 = glm::vec3(wx, wy, wz);
                    glm::vec3 normal = glm::vec3(0, -1, 0);
                    auto uvs = GetFaceUVsFromAtlas(b, FACE_BOTTOM);
                    const glm::vec3 posArr[4] = { p0,p1,p2,p3 };
                    pushQuad(verts, inds, posArr, normal, color, &uvs[0]);
                }

                // North (-Z)
                if (!GetBlockAt(x, z - 1)) {
                    glm::vec3 p0 = glm::vec3(wx + 1, wy, wz);
                    glm::vec3 p1 = glm::vec3(wx, wy, wz);
                    glm::vec3 p2 = glm::vec3(wx, wy + 1, wz);
                    glm::vec3 p3 = glm::vec3(wx + 1, wy + 1, wz);
                    glm::vec3 normal = glm::vec3(0, 0, -1);
                    auto uvs = GetFaceUVsFromAtlas(b, FACE_NORTH);
                    const glm::vec3 posArr[4] = { p0,p1,p2,p3 };
                    pushQuad(verts, inds, posArr, normal, color, &uvs[0]);
                }

                // South (+Z)
                if (!GetBlockAt(x, z + 1)) {
                    glm::vec3 p0 = glm::vec3(wx, wy, wz + 1);
                    glm::vec3 p1 = glm::vec3(wx + 1, wy, wz + 1);
                    glm::vec3 p2 = glm::vec3(wx + 1, wy + 1, wz + 1);
                    glm::vec3 p3 = glm::vec3(wx, wy + 1, wz + 1);
                    glm::vec3 normal = glm::vec3(0, 0, 1);
                    auto uvs = GetFaceUVsFromAtlas(b, FACE_SOUTH);
                    const glm::vec3 posArr[4] = { p0,p1,p2,p3 };
                    pushQuad(verts, inds, posArr, normal, color, &uvs[0]);
                }

                // West (-X)
                if (!GetBlockAt(x - 1, z)) {
                    glm::vec3 p0 = glm::vec3(wx, wy, wz);
                    glm::vec3 p1 = glm::vec3(wx, wy, wz + 1);
                    glm::vec3 p2 = glm::vec3(wx, wy + 1, wz + 1);
                    glm::vec3 p3 = glm::vec3(wx, wy + 1, wz);
                    glm::vec3 normal = glm::vec3(-1, 0, 0);
                    auto uvs = GetFaceUVsFromAtlas(b, FACE_WEST);
                    const glm::vec3 posArr[4] = { p0,p1,p2,p3 };
                    pushQuad(verts, inds, posArr, normal, color, &uvs[0]);
                }

                // East (+X)
                if (!GetBlockAt(x + 1, z)) {
                    glm::vec3 p0 = glm::vec3(wx + 1, wy, wz + 1);
                    glm::vec3 p1 = glm::vec3(wx + 1, wy, wz);
                    glm::vec3 p2 = glm::vec3(wx + 1, wy + 1, wz);
                    glm::vec3 p3 = glm::vec3(wx + 1, wy + 1, wz + 1);
                    glm::vec3 normal = glm::vec3(1, 0, 0);
                    auto uvs = GetFaceUVsFromAtlas(b, FACE_EAST);
                    const glm::vec3 posArr[4] = { p0,p1,p2,p3 };
                    pushQuad(verts, inds, posArr, normal, color, &uvs[0]);
                }
            }
        }

        std::cout << "BuildMesh: verts=" << verts.size() << " indices=" << inds.size() << "\n";

        if (verts.empty() || inds.empty()) {
            s_indexCount = 0;
            return;
        }

        // safety guard on index count
        const size_t maxIndicesAllowed = 100000000; // very large upper bound; adjust if needed
        if (inds.size() > maxIndicesAllowed) {
            std::cerr << "BuildMesh: indices too large (" << inds.size() << "), aborting\n";
            s_indexCount = 0;
            return;
        }

        // upload to GPU
        glGenVertexArrays(1, &s_vao);
        glBindVertexArray(s_vao);

        glGenBuffers(1, &s_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &s_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(uint32_t), inds.data(), GL_STATIC_DRAW);

        GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, uv));

        glBindVertexArray(0);

        s_indexCount = static_cast<GLsizei>(inds.size());
    }

    void InitFlatPlane(const Block& prototype, int planeY) {
        s_planeY = planeY;
        s_width = WORLD_WIDTH;
        s_plane.assign(static_cast<size_t>(s_width) * static_cast<size_t>(s_width), nullptr);
        for (int z = 0; z < s_width; ++z)
            for (int x = 0; x < s_width; ++x)
                s_plane[idx(x, z)] = &prototype;

        EnsureProgram();
        try {
            BuildMesh();
        }
        catch (const std::exception& e) {
            std::cerr << "InitFlatPlane: BuildMesh threw: " << e.what() << "\n";
            s_indexCount = 0;
        }
    }

    void RenderVisible(const glm::mat4& proj, const glm::mat4& view) {
        if (s_indexCount == 0 || !s_program) return;

        glUseProgram(s_program);
        glBindVertexArray(s_vao);

        GLint locP = glGetUniformLocation(s_program, "uProj");
        GLint locV = glGetUniformLocation(s_program, "uView");
        GLint locM = glGetUniformLocation(s_program, "uModel");
        GLint locUseAtlas = glGetUniformLocation(s_program, "uUseAtlas");
        GLint locAtlas = glGetUniformLocation(s_program, "uAtlas");

        glm::mat4 model = glm::mat4::identity();
        if (locP >= 0) glUniformMatrix4fv(locP, 1, GL_FALSE, glm::value_ptr(proj));
        if (locV >= 0) glUniformMatrix4fv(locV, 1, GL_FALSE, glm::value_ptr(view));
        if (locM >= 0) glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(model));

        if (s_atlas && s_atlas->IsValid()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_atlas->GetTextureId());
            if (locUseAtlas >= 0) glUniform1i(locUseAtlas, 1);
            if (locAtlas >= 0) glUniform1i(locAtlas, 0);
        }
        else {
            if (locUseAtlas >= 0) glUniform1i(locUseAtlas, 0);
        }

        glDrawElements(GL_TRIANGLES, s_indexCount, GL_UNSIGNED_INT, nullptr);

        if (s_atlas && s_atlas->IsValid()) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glBindVertexArray(0);
        glUseProgram(0);
    }

    void SetAtlas(TextureAtlas* atlas) {
        s_atlas = atlas;
        // if mesh already built, rebuild to pick up UVs
        if (!s_plane.empty()) {
            try {
                BuildMesh();
            }
            catch (const std::exception& e) {
                std::cerr << "SetAtlas: BuildMesh threw: " << e.what() << "\n";
            }
        }
    }

    void Shutdown() {
        if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
        if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
        if (s_ebo) { glDeleteBuffers(1, &s_ebo); s_ebo = 0; }
        if (s_program) { glDeleteProgram(s_program); s_program = 0; }
        s_plane.clear();
        s_indexCount = 0;
        s_atlas = nullptr;
    }

} // namespace rendering