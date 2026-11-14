#include "e3d/platform/3ds/render_device_3ds.hpp"

#include "e3d/render/handle_pool.hpp"

#include <3ds.h>
#include <citro3d.h>

#include <cstring>
#include <memory>

#include "unlit_color_shbin_data.h"

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
};

struct GpuMaterial final {
    e3d::MaterialDesc desc;
};

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

        dvlb_ = DVLB_ParseFile(const_cast<u32*>(reinterpret_cast<const u32*>(unlit_color_shbin)),
                               static_cast<u32>(unlit_color_shbin_size));
        if (dvlb_ == nullptr) return false;

        shaderProgramInit(&program_);
        shaderProgramSetVsh(&program_, &dvlb_->DVLE[0]);
        C3D_BindProgram(&program_);

        uniform_mvp_ = shaderInstanceGetUniformLocation(program_.vertexShader, "mvp");
        uniform_tint_ = shaderInstanceGetUniformLocation(program_.vertexShader, "tint");

        C3D_AttrInfo* attributes = C3D_GetAttrInfo();
        AttrInfo_Init(attributes);
        AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 3);  // position
        AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 4);  // colour
        AttrInfo_AddLoader(attributes, 2, GPU_FLOAT, 2);  // uv
        AttrInfo_AddLoader(attributes, 3, GPU_FLOAT, 3);  // normal

        C3D_TexEnv* environment = C3D_GetTexEnv(0);
        C3D_TexEnvInit(environment);
        C3D_TexEnvSrc(environment, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR,
                      GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(environment, C3D_Both, GPU_REPLACE);

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

        if (dvlb_ != nullptr) {
            shaderProgramFree(&program_);
            DVLB_Free(dvlb_);
            dvlb_ = nullptr;
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
        return meshes_.add(mesh);
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

    void execute(const e3d::RenderList& list) override {
        if (!initialized_) return;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_BindProgram(&program_);

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

                C3D_Mtx model = to_c3d(item.model);
                C3D_Mtx view_model;
                C3D_Mtx mvp;
                Mtx_Multiply(&view_model, &view, &model);
                Mtx_Multiply(&mvp, &projection, &view_model);

                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uniform_mvp_, &mvp);
                if (uniform_tint_ >= 0) {
                    const e3d::Color& tint = material->desc.tint;
                    C3D_FVUnifSet(GPU_VERTEX_SHADER, uniform_tint_, tint.r, tint.g, tint.b, tint.a);
                }

                C3D_BufInfo* buffers = C3D_GetBufInfo();
                BufInfo_Init(buffers);
                BufInfo_Add(buffers, mesh->vertices, sizeof(e3d::MeshVertex), 4, 0x3210);
                C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(mesh->index_count),
                                 C3D_UNSIGNED_SHORT, mesh->indices);
            }
        }

        C3D_FrameEnd(0);
    }

private:
    bool initialized_ = false;
    C3D_RenderTarget* top_ = nullptr;
    C3D_RenderTarget* bottom_ = nullptr;
    DVLB_s* dvlb_ = nullptr;
    shaderProgram_s program_{};
    int uniform_mvp_ = -1;
    int uniform_tint_ = -1;
    e3d::HandlePool<e3d::MeshTag, GpuMesh> meshes_;
    e3d::HandlePool<e3d::MaterialTag, GpuMaterial> materials_;
};

} // namespace

namespace e3d {

std::unique_ptr<RenderDevice> create_render_device_3ds() {
    return std::make_unique<Device3DS>();
}

} // namespace e3d
