#include "e3d/platform/3ds/render_device_3ds.hpp"

#include "e3d/render/handle_pool.hpp"

#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>

#include <cstring>
#include <memory>

#include "lit_color_shbin_data.h"
#include "lit_textured_shbin_data.h"
#include "unlit_color_shbin_data.h"
#include "unlit_textured_shbin_data.h"

namespace {

constexpr u32 kDisplayTransferFlags =
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

u8 to_unorm8(float value) {
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return 255;
    return static_cast<u8>(value * 255.0f + 0.5f);
}

u32 pack_color(const e3d::Color& color) {
    return (static_cast<u32>(to_unorm8(color.r)) << 24) |
           (static_cast<u32>(to_unorm8(color.g)) << 16) |
           (static_cast<u32>(to_unorm8(color.b)) << 8) | static_cast<u32>(to_unorm8(color.a));
}

C3D_Mtx to_c3d(const e3d::Mat4& matrix) {
    C3D_Mtx result;
    result.r[0] = FVec4_New(matrix.m[0], matrix.m[4], matrix.m[8], matrix.m[12]);
    result.r[1] = FVec4_New(matrix.m[1], matrix.m[5], matrix.m[9], matrix.m[13]);
    result.r[2] = FVec4_New(matrix.m[2], matrix.m[6], matrix.m[10], matrix.m[14]);
    result.r[3] = FVec4_New(matrix.m[3], matrix.m[7], matrix.m[11], matrix.m[15]);
    return result;
}

struct GpuMesh final {
    void* vertices = nullptr;
    void* indices = nullptr;
    u32 index_count = 0;
    u32 vertex_capacity = 0;
    u32 index_capacity = 0;
};

struct GpuTexture final {
    C3D_Tex texture{};
    bool valid = false;
};

struct GpuMaterial final {
    e3d::MaterialDesc desc;
};

struct GpuProgram final {
    DVLB_s* dvlb = nullptr;
    shaderProgram_s program{};
    e3d::ShaderId shader = e3d::ShaderId::UnlitColor;
    int mvp = -1;
    int model = -1;
    int light_dir = -1;
    int tint = -1;
    int params = -1;
};

bool load_program(GpuProgram& out, const void* data, std::size_t size, e3d::ShaderId shader) {
    out.dvlb = DVLB_ParseFile(const_cast<u32*>(static_cast<const u32*>(data)),
                              static_cast<u32>(size));
    if (out.dvlb == nullptr) return false;

    shaderProgramInit(&out.program);
    shaderProgramSetVsh(&out.program, &out.dvlb->DVLE[0]);

    out.shader = shader;
    out.mvp = shaderInstanceGetUniformLocation(out.program.vertexShader, "mvp");
    out.model = shaderInstanceGetUniformLocation(out.program.vertexShader, "model");
    out.light_dir = shaderInstanceGetUniformLocation(out.program.vertexShader, "lightDir");
    out.tint = shaderInstanceGetUniformLocation(out.program.vertexShader, "tint");
    out.params = shaderInstanceGetUniformLocation(out.program.vertexShader, "params");
    return out.mvp >= 0;
}

class Device3DS final : public e3d::RenderDevice {
public:
    ~Device3DS() override { shutdown(); }

    bool init() override {
        if (initialized_) return true;

        gfxInitDefault();
        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

        top_ = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        bottom_ = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if (top_ == nullptr || bottom_ == nullptr) return false;

        C3D_RenderTargetSetOutput(top_, GFX_TOP, GFX_LEFT, kDisplayTransferFlags);
        C3D_RenderTargetSetOutput(bottom_, GFX_BOTTOM, GFX_LEFT, kDisplayTransferFlags);

        if (!load_program(unlit_, unlit_color_shbin, unlit_color_shbin_size,
                          e3d::ShaderId::UnlitColor)) {
            return false;
        }
        if (!load_program(lit_, lit_textured_shbin, lit_textured_shbin_size,
                          e3d::ShaderId::LitTextured)) {
            return false;
        }
        if (!load_program(textured_, unlit_textured_shbin, unlit_textured_shbin_size,
                          e3d::ShaderId::UnlitTextured)) {
            return false;
        }
        if (!load_program(lit_color_, lit_color_shbin, lit_color_shbin_size,
                          e3d::ShaderId::LitColor)) {
            return false;
        }

        sprite_capacity_bytes_ = 256 * 1024;
        sprite_buffer_ = linearAlloc(sprite_capacity_bytes_);
        if (sprite_buffer_ == nullptr) return false;

        C3D_AttrInfo* attributes = C3D_GetAttrInfo();
        AttrInfo_Init(attributes);
        AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 3);  // position
        AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 4);  // colour
        AttrInfo_AddLoader(attributes, 2, GPU_FLOAT, 2);  // uv
        AttrInfo_AddLoader(attributes, 3, GPU_FLOAT, 3);  // normal

        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
        C3D_CullFace(GPU_CULL_NONE);

        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (!initialized_) return;
        initialized_ = false;

        meshes_.for_each([](GpuMesh& mesh) {
            if (mesh.vertices != nullptr) linearFree(mesh.vertices);
            if (mesh.indices != nullptr) linearFree(mesh.indices);
            mesh.vertices = nullptr;
            mesh.indices = nullptr;
        });

        textures_.for_each([](GpuTexture& texture) {
            if (texture.valid) {
                C3D_TexDelete(&texture.texture);
                texture.valid = false;
            }
        });

        for (GpuProgram* program : {&unlit_, &lit_, &textured_, &lit_color_}) {
            if (program->dvlb != nullptr) {
                shaderProgramFree(&program->program);
                DVLB_Free(program->dvlb);
                program->dvlb = nullptr;
            }
        }

        if (sprite_buffer_ != nullptr) {
            linearFree(sprite_buffer_);
            sprite_buffer_ = nullptr;
        }

        if (top_ != nullptr) {
            C3D_RenderTargetDelete(top_);
            top_ = nullptr;
        }
        if (bottom_ != nullptr) {
            C3D_RenderTargetDelete(bottom_);
            bottom_ = nullptr;
        }

        C3D_Fini();
        gfxExit();
    }

    e3d::MeshHandle create_mesh(const e3d::MeshData& data) override {
        if (data.vertices.empty() || data.indices.empty()) return {};

        const std::size_t vertex_bytes = data.vertices.size() * sizeof(e3d::MeshVertex);
        const std::size_t index_bytes = data.indices.size() * sizeof(u16);

        void* vertices = linearAlloc(vertex_bytes);
        void* indices = linearAlloc(index_bytes);
        if (vertices == nullptr || indices == nullptr) {
            if (vertices != nullptr) linearFree(vertices);
            if (indices != nullptr) linearFree(indices);
            return {};
        }

        std::memcpy(vertices, data.vertices.data(), vertex_bytes);
        std::memcpy(indices, data.indices.data(), index_bytes);
        GSPGPU_FlushDataCache(vertices, static_cast<u32>(vertex_bytes));
        GSPGPU_FlushDataCache(indices, static_cast<u32>(index_bytes));

        GpuMesh mesh;
        mesh.vertices = vertices;
        mesh.indices = indices;
        mesh.index_count = static_cast<u32>(data.indices.size());
        mesh.vertex_capacity = static_cast<u32>(vertex_bytes);
        mesh.index_capacity = static_cast<u32>(index_bytes);
        return meshes_.add(mesh);
    }

    bool update_mesh(e3d::MeshHandle handle, const e3d::MeshData& data) override {
        GpuMesh* mesh = meshes_.get(handle);
        if (mesh == nullptr || data.vertices.empty() || data.indices.empty()) return false;

        const std::size_t vertex_bytes = data.vertices.size() * sizeof(e3d::MeshVertex);
        const std::size_t index_bytes = data.indices.size() * sizeof(u16);

        if (vertex_bytes > mesh->vertex_capacity) {
            if (mesh->vertices != nullptr) linearFree(mesh->vertices);
            mesh->vertices = linearAlloc(vertex_bytes);
            if (mesh->vertices == nullptr) {
                mesh->vertex_capacity = 0;
                return false;
            }
            mesh->vertex_capacity = static_cast<u32>(vertex_bytes);
        }

        if (index_bytes > mesh->index_capacity) {
            if (mesh->indices != nullptr) linearFree(mesh->indices);
            mesh->indices = linearAlloc(index_bytes);
            if (mesh->indices == nullptr) {
                mesh->index_capacity = 0;
                return false;
            }
            mesh->index_capacity = static_cast<u32>(index_bytes);
        }

        std::memcpy(mesh->vertices, data.vertices.data(), vertex_bytes);
        std::memcpy(mesh->indices, data.indices.data(), index_bytes);
        GSPGPU_FlushDataCache(mesh->vertices, static_cast<u32>(vertex_bytes));
        GSPGPU_FlushDataCache(mesh->indices, static_cast<u32>(index_bytes));
        mesh->index_count = static_cast<u32>(data.indices.size());
        return true;
    }

    void destroy_mesh(e3d::MeshHandle handle) override {
        GpuMesh* mesh = meshes_.get(handle);
        if (mesh == nullptr) return;
        if (mesh->vertices != nullptr) linearFree(mesh->vertices);
        if (mesh->indices != nullptr) linearFree(mesh->indices);
        mesh->vertices = nullptr;
        mesh->indices = nullptr;
        meshes_.remove(handle);
    }

    e3d::MaterialHandle create_material(const e3d::MaterialDesc& material) override {
        return materials_.add(GpuMaterial{material});
    }

    void destroy_material(e3d::MaterialHandle handle) override { materials_.remove(handle); }

    e3d::TextureHandle create_texture(const e3d::TextureData& data) override {
        if (data.data == nullptr || data.size == 0) return {};

        GpuTexture texture;
        Tex3DS_Texture imported =
            Tex3DS_TextureImport(data.data, data.size, &texture.texture, nullptr, false);
        if (!imported) return {};

        Tex3DS_TextureFree(imported);
        C3D_TexSetFilter(&texture.texture, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&texture.texture, GPU_REPEAT, GPU_REPEAT);
        texture.valid = true;
        return textures_.add(texture);
    }

    void destroy_texture(e3d::TextureHandle handle) override {
        GpuTexture* texture = textures_.get(handle);
        if (texture == nullptr) return;
        if (texture->valid) {
            C3D_TexDelete(&texture->texture);
            texture->valid = false;
        }
        textures_.remove(handle);
    }

    void execute(const e3d::RenderList& list) override {
        if (!initialized_) return;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        active_ = nullptr;

        for (std::size_t camera_index = 0; camera_index < list.cameras().size(); ++camera_index) {
            const e3d::CameraDesc& camera = list.cameras()[camera_index];

            C3D_RenderTarget* target = camera.screen == e3d::Screen::Top ? top_ : bottom_;
            const C3D_ClearBits clear_flags =
                camera.clear_depth ? C3D_CLEAR_ALL : C3D_CLEAR_COLOR;
            C3D_RenderTargetClear(target, clear_flags, pack_color(camera.clear), 0);
            C3D_FrameDrawOn(target);

            const float aspect = camera.screen == e3d::Screen::Top ? C3D_AspectRatioTop
                                                                   : C3D_AspectRatioBot;
            C3D_Mtx projection;
            Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(camera.field_of_view), aspect,
                          camera.near_plane, camera.far_plane, false);

            C3D_Mtx view;
            Mtx_LookAt(&view, FVec3_New(camera.position.x, camera.position.y, camera.position.z),
                       FVec3_New(camera.target.x, camera.target.y, camera.target.z),
                       FVec3_New(camera.up.x, camera.up.y, camera.up.z), false);

            for (const e3d::DrawItem& item : list.draws()) {
                if (item.camera != camera_index) continue;

                const GpuMesh* mesh = meshes_.get(item.mesh);
                const GpuMaterial* material = materials_.get(item.material);
                if (mesh == nullptr || material == nullptr || mesh->vertices == nullptr) continue;

                GpuProgram* program = &unlit_;
                if (material->desc.shader == e3d::ShaderId::LitTextured) {
                    program = &lit_;
                } else if (material->desc.shader == e3d::ShaderId::LitColor) {
                    program = &lit_color_;
                } else if (material->desc.shader == e3d::ShaderId::UnlitTextured) {
                    program = &textured_;
                }

                GpuTexture* texture = nullptr;
                if (program->shader == e3d::ShaderId::LitTextured ||
                    program->shader == e3d::ShaderId::UnlitTextured) {
                    texture = textures_.get(material->desc.texture);
                    if (texture == nullptr || !texture->valid) continue;
                }

                if (program != active_) {
                    C3D_BindProgram(&program->program);
                    configure_tev(program->shader);
                    active_ = program;
                }

                C3D_Mtx model = to_c3d(item.model);
                C3D_Mtx view_model;
                C3D_Mtx mvp;
                Mtx_Multiply(&view_model, &view, &model);
                Mtx_Multiply(&mvp, &projection, &view_model);

                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, program->mvp, &mvp);
                if (program->tint >= 0) {
                    const e3d::Color& tint = material->desc.tint;
                    C3D_FVUnifSet(GPU_VERTEX_SHADER, program->tint, tint.r, tint.g, tint.b, tint.a);
                }
                if (program->model >= 0) {
                    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, program->model, &model);
                }
                if (program->light_dir >= 0) {
                    const e3d::Vec3 light = e3d::normalize(material->desc.light_direction);
                    C3D_FVUnifSet(GPU_VERTEX_SHADER, program->light_dir, light.x, light.y, light.z,
                                  0.0f);
                }
                if (program->params >= 0) {
                    C3D_FVUnifSet(GPU_VERTEX_SHADER, program->params, material->desc.ambient,
                                  material->desc.diffuse, 0.0f, 0.0f);
                }
                if (texture != nullptr) {
                    C3D_TexBind(0, &texture->texture);
                }

                C3D_BufInfo* buffers = C3D_GetBufInfo();
                BufInfo_Init(buffers);
                BufInfo_Add(buffers, mesh->vertices, sizeof(e3d::MeshVertex), 4, 0x3210);
                C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(mesh->index_count),
                                 C3D_UNSIGNED_SHORT, mesh->indices);
            }
        }

        draw_sprites(list, e3d::Screen::Top, 400.0f, 240.0f);
        draw_sprites(list, e3d::Screen::Bottom, 320.0f, 240.0f);

        C3D_FrameEnd(0);
    }

private:
    void draw_sprites(const e3d::RenderList& list, e3d::Screen screen, float width, float height) {
        bool any = false;
        for (const e3d::SpriteItem& sprite : list.sprites()) {
            if (sprite.screen == screen) {
                any = true;
                break;
            }
        }
        if (!any) return;

        C3D_RenderTarget* target = screen == e3d::Screen::Top ? top_ : bottom_;
        C3D_FrameDrawOn(target);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
        // Text/UI is composited additively: the atlas is black-backed, so black
        // pixels are a no-op without needing a working alpha channel.
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE);

        if (active_ != &textured_) {
            C3D_BindProgram(&textured_.program);
            configure_tev(e3d::ShaderId::UnlitTextured);
            active_ = &textured_;
        }

        C3D_Mtx ortho;
        Mtx_OrthoTilt(&ortho, 0.0f, width, height, 0.0f, -1.0f, 1.0f, true);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, textured_.mvp, &ortho);
        if (textured_.tint >= 0) {
            C3D_FVUnifSet(GPU_VERTEX_SHADER, textured_.tint, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        auto* vertices = static_cast<e3d::MeshVertex*>(sprite_buffer_);
        const u32 capacity = sprite_capacity_bytes_ / sizeof(e3d::MeshVertex);

        struct Batch final {
            e3d::TextureHandle texture;
            u32 first = 0;
            u32 count = 0;
        };
        Batch batches[64];
        u32 batch_count = 0;
        u32 vertex_count = 0;

        for (const e3d::SpriteItem& sprite : list.sprites()) {
            if (sprite.screen != screen) continue;
            if (vertex_count + 6 > capacity || batch_count >= 64) break;

            if (batch_count == 0 ||
                !(batches[batch_count - 1].texture == sprite.texture)) {
                batches[batch_count].texture = sprite.texture;
                batches[batch_count].first = vertex_count;
                batches[batch_count].count = 0;
                ++batch_count;
            }

            const float x0 = sprite.x;
            const float y0 = sprite.y;
            const float x1 = sprite.x + sprite.width;
            const float y1 = sprite.y + sprite.height;

            const e3d::Vec3 normal{0.0f, 0.0f, 1.0f};
            const e3d::MeshVertex v0{e3d::Vec3{x0, y0, 0.0f}, sprite.color,
                                     e3d::Vec2{sprite.u0, sprite.v0}, normal};
            const e3d::MeshVertex v1{e3d::Vec3{x1, y0, 0.0f}, sprite.color,
                                     e3d::Vec2{sprite.u1, sprite.v0}, normal};
            const e3d::MeshVertex v2{e3d::Vec3{x1, y1, 0.0f}, sprite.color,
                                     e3d::Vec2{sprite.u1, sprite.v1}, normal};
            const e3d::MeshVertex v3{e3d::Vec3{x0, y1, 0.0f}, sprite.color,
                                     e3d::Vec2{sprite.u0, sprite.v1}, normal};

            vertices[vertex_count++] = v0;
            vertices[vertex_count++] = v1;
            vertices[vertex_count++] = v2;
            vertices[vertex_count++] = v0;
            vertices[vertex_count++] = v2;
            vertices[vertex_count++] = v3;

            batches[batch_count - 1].count += 6;
        }

        if (vertex_count == 0) return;

        GSPGPU_FlushDataCache(sprite_buffer_,
                              static_cast<u32>(vertex_count * sizeof(e3d::MeshVertex)));

        C3D_BufInfo* buffers = C3D_GetBufInfo();
        BufInfo_Init(buffers);
        BufInfo_Add(buffers, sprite_buffer_, sizeof(e3d::MeshVertex), 4, 0x3210);

        for (u32 i = 0; i < batch_count; ++i) {
            GpuTexture* texture = textures_.get(batches[i].texture);
            if (texture == nullptr || !texture->valid) continue;
            C3D_TexBind(0, &texture->texture);
            C3D_DrawArrays(GPU_TRIANGLES, static_cast<int>(batches[i].first),
                           static_cast<int>(batches[i].count));
        }

        C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    }

private:
    void configure_tev(e3d::ShaderId shader) {
        C3D_TexEnv* environment = C3D_GetTexEnv(0);
        C3D_TexEnvInit(environment);

        if (shader == e3d::ShaderId::LitTextured || shader == e3d::ShaderId::UnlitTextured) {
            C3D_TexEnvSrc(environment, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
                          GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(environment, C3D_Both, GPU_MODULATE);
        } else {
            C3D_TexEnvSrc(environment, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR,
                          GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(environment, C3D_Both, GPU_REPLACE);
        }
    }

    bool initialized_ = false;
    C3D_RenderTarget* top_ = nullptr;
    C3D_RenderTarget* bottom_ = nullptr;
    GpuProgram unlit_;
    GpuProgram lit_;
    GpuProgram textured_;
    GpuProgram lit_color_;
    GpuProgram* active_ = nullptr;
    void* sprite_buffer_ = nullptr;
    u32 sprite_capacity_bytes_ = 0;
    e3d::HandlePool<e3d::MeshTag, GpuMesh> meshes_;
    e3d::HandlePool<e3d::MaterialTag, GpuMaterial> materials_;
    e3d::HandlePool<e3d::TextureTag, GpuTexture> textures_;
};

} // namespace

namespace e3d {

std::unique_ptr<RenderDevice> create_render_device_3ds() {
    return std::make_unique<Device3DS>();
}

} // namespace e3d
