#include "e3d/platform/desktop/render_device_gl.hpp"

#include "e3d/core/math.hpp"
#include "e3d/render/handle_pool.hpp"

#include <GL/glew.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

using namespace e3d;

Mat4 perspective(float fov_degrees, float aspect, float near_plane, float far_plane) {
    const float f = 1.0f / std::tan(fov_degrees * kDegToRad * 0.5f);
    Mat4 m{};
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return m;
}

Mat4 look_at(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 m = mat4_identity();
    m.m[0] = s.x;
    m.m[4] = s.y;
    m.m[8] = s.z;
    m.m[1] = u.x;
    m.m[5] = u.y;
    m.m[9] = u.z;
    m.m[2] = -f.x;
    m.m[6] = -f.y;
    m.m[10] = -f.z;
    m.m[12] = -dot(s, eye);
    m.m[13] = -dot(u, eye);
    m.m[14] = dot(f, eye);
    return m;
}

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == 0) {
        char log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Lumika GL shader error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

struct GpuMesh final {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei index_count = 0;
    std::size_t vertex_capacity = 0;
    std::size_t index_capacity = 0;
};

struct GpuTexture final {
    GLuint id = 0;
    bool valid = false;
};

struct GpuMaterial final {
    MaterialDesc desc;
};

class DeviceGL final : public RenderDevice {
public:
    bool init() override {
        static const char* vertex_source = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModel;
out vec4 vColor;
out vec2 vUv;
out vec3 vNormal;
void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
    vUv = aUv;
    vNormal = mat3(uModel) * aNormal;
}
)";

        static const char* fragment_source = R"(#version 330 core
in vec4 vColor;
in vec2 vUv;
in vec3 vNormal;
uniform vec4 uTint;
uniform vec3 uLightDir;
uniform vec2 uParams;
uniform int uUseTexture;
uniform int uUseLighting;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    float lit = 1.0;
    if (uUseLighting != 0) {
        vec3 n = normalize(vNormal);
        float d = max(dot(n, uLightDir), 0.0);
        lit = uParams.x + uParams.y * d;
    }
    vec4 color = vColor * uTint * lit;
    if (uUseTexture != 0) color *= texture(uTex, vUv);
    FragColor = color;
}
)";

        const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
        const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
        if (vertex == 0 || fragment == 0) return false;

        program_ = glCreateProgram();
        glAttachShader(program_, vertex);
        glAttachShader(program_, fragment);
        glLinkProgram(program_);
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint linked = 0;
        glGetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (linked == 0) return false;

        u_mvp_ = glGetUniformLocation(program_, "uMvp");
        u_model_ = glGetUniformLocation(program_, "uModel");
        u_tint_ = glGetUniformLocation(program_, "uTint");
        u_light_ = glGetUniformLocation(program_, "uLightDir");
        u_params_ = glGetUniformLocation(program_, "uParams");
        u_use_texture_ = glGetUniformLocation(program_, "uUseTexture");
        u_use_lighting_ = glGetUniformLocation(program_, "uUseLighting");
        u_texture_ = glGetUniformLocation(program_, "uTex");

        glGenTextures(1, &white_);
        glBindTexture(GL_TEXTURE_2D, white_);
        const unsigned char pixel[4] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return true;
    }

    void shutdown() override {
        meshes_.for_each([](GpuMesh& mesh) {
            if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
            if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
            if (mesh.ebo != 0) glDeleteBuffers(1, &mesh.ebo);
            mesh.vao = mesh.vbo = mesh.ebo = 0;
        });
        textures_.for_each([](GpuTexture& texture) {
            if (texture.id != 0) glDeleteTextures(1, &texture.id);
        });
        if (program_ != 0) glDeleteProgram(program_);
        if (white_ != 0) glDeleteTextures(1, &white_);
    }

    MeshHandle create_mesh(const MeshData& data) override {
        if (data.vertices.empty() || data.indices.empty()) return {};

        GpuMesh mesh;
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glGenBuffers(1, &mesh.ebo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(MeshVertex)),
                     data.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(data.indices.size() * sizeof(u16)), data.indices.data(),
                     GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                              reinterpret_cast<void*>(offsetof(MeshVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                              reinterpret_cast<void*>(offsetof(MeshVertex, color)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                              reinterpret_cast<void*>(offsetof(MeshVertex, uv)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                              reinterpret_cast<void*>(offsetof(MeshVertex, normal)));

        mesh.index_count = static_cast<GLsizei>(data.indices.size());
        mesh.vertex_capacity = data.vertices.size();
        mesh.index_capacity = data.indices.size();
        return meshes_.add(mesh);
    }

    bool update_mesh(MeshHandle handle, const MeshData& data) override {
        GpuMesh* mesh = meshes_.get(handle);
        if (mesh == nullptr || data.vertices.empty() || data.indices.empty()) return false;

        glBindVertexArray(mesh->vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(MeshVertex)),
                     data.vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(data.indices.size() * sizeof(u16)), data.indices.data(),
                     GL_DYNAMIC_DRAW);
        mesh->index_count = static_cast<GLsizei>(data.indices.size());
        mesh->vertex_capacity = data.vertices.size();
        mesh->index_capacity = data.indices.size();
        return true;
    }

    void destroy_mesh(MeshHandle handle) override {
        GpuMesh* mesh = meshes_.get(handle);
        if (mesh == nullptr) return;
        if (mesh->vao != 0) glDeleteVertexArrays(1, &mesh->vao);
        if (mesh->vbo != 0) glDeleteBuffers(1, &mesh->vbo);
        if (mesh->ebo != 0) glDeleteBuffers(1, &mesh->ebo);
        mesh->vao = mesh->vbo = mesh->ebo = 0;
        meshes_.remove(handle);
    }

    MaterialHandle create_material(const MaterialDesc& material) override {
        return materials_.add(GpuMaterial{material});
    }

    void destroy_material(MaterialHandle handle) override { materials_.remove(handle); }

    TextureHandle create_texture(const TextureData&) override {
        // The editor viewport does not decode .t3x yet; use a white fallback so
        // textured materials render shaded rather than black.
        GpuTexture texture;
        texture.id = white_;
        texture.valid = true;
        return textures_.add(texture);
    }

    void destroy_texture(TextureHandle handle) override { textures_.remove(handle); }

    void set_viewport(int width, int height) override {
        width_ = width;
        height_ = height;
    }

    void execute(const RenderList& list) override {
        if (width_ <= 0 || height_ <= 0) return;

        glViewport(0, 0, width_, height_);
        glDisable(GL_SCISSOR_TEST);

        for (std::size_t camera_index = 0; camera_index < list.cameras().size(); ++camera_index) {
            const CameraDesc& camera = list.cameras()[camera_index];
            glClearColor(camera.clear.r, camera.clear.g, camera.clear.b, camera.clear.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
            const Mat4 projection =
                perspective(camera.field_of_view, aspect, camera.near_plane, camera.far_plane);
            const Mat4 view = look_at(camera.position, camera.target, camera.up);

            glUseProgram(program_);
            const Vec3 light = normalize(-0.4f * Vec3{1, 0, 0} + 0.85f * Vec3{0, 1, 0} +
                                        0.6f * Vec3{0, 0, 1});
            glUniform3f(u_light_, light.x, light.y, light.z);
            glUniform1i(u_texture_, 0);

            for (const DrawItem& item : list.draws()) {
                if (item.camera != camera_index) continue;

                const GpuMesh* mesh = meshes_.get(item.mesh);
                const GpuMaterial* material = materials_.get(item.material);
                if (mesh == nullptr || material == nullptr || mesh->index_count == 0) continue;

                const Mat4 mvp = projection * view * item.model;
                glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m);
                glUniformMatrix4fv(u_model_, 1, GL_FALSE, item.model.m);

                const Color& tint = material->desc.tint;
                glUniform4f(u_tint_, tint.r, tint.g, tint.b, tint.a);
                glUniform2f(u_params_, material->desc.ambient, material->desc.diffuse);

                const ShaderId shader = material->desc.shader;
                const bool textured = shader == ShaderId::LitTextured ||
                                      shader == ShaderId::UnlitTextured;
                const bool lit = shader == ShaderId::LitTextured || shader == ShaderId::LitColor;
                glUniform1i(u_use_texture_, textured ? 1 : 0);
                glUniform1i(u_use_lighting_, lit ? 1 : 0);

                if (textured) {
                    const GpuTexture* texture = textures_.get(material->desc.texture);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture != nullptr ? texture->id : white_);
                }

                glBindVertexArray(mesh->vao);
                glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_SHORT, nullptr);
            }
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

private:
    GLuint program_ = 0;
    GLuint white_ = 0;
    int width_ = 0;
    int height_ = 0;
    GLint u_mvp_ = -1;
    GLint u_model_ = -1;
    GLint u_tint_ = -1;
    GLint u_light_ = -1;
    GLint u_params_ = -1;
    GLint u_use_texture_ = -1;
    GLint u_use_lighting_ = -1;
    GLint u_texture_ = -1;
    HandlePool<MeshTag, GpuMesh> meshes_;
    HandlePool<MaterialTag, GpuMaterial> materials_;
    HandlePool<TextureTag, GpuTexture> textures_;
};

} // namespace

namespace e3d {

std::unique_ptr<RenderDevice> create_render_device_gl() {
    return std::make_unique<DeviceGL>();
}

} // namespace e3d
