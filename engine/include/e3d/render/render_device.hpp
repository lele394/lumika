#pragma once

#include "e3d/render/render_list.hpp"
#include "e3d/render/types.hpp"

namespace e3d {

/// Backend contract. The engine only ever speaks in handles and a render list.
class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    virtual MeshHandle create_mesh(const MeshData& mesh) = 0;
    virtual bool update_mesh(MeshHandle mesh, const MeshData& data) = 0;
    virtual void destroy_mesh(MeshHandle mesh) = 0;

    virtual MaterialHandle create_material(const MaterialDesc& material) = 0;
    virtual void destroy_material(MaterialHandle material) = 0;

    virtual TextureHandle create_texture(const TextureData& texture) = 0;
    virtual void destroy_texture(TextureHandle texture) = 0;

    virtual void execute(const RenderList& list) = 0;

    /// Backends that render offscreen (desktop editor viewport) need the target
    /// size; the 3DS backend ignores this.
    virtual void set_viewport(int /*width*/, int /*height*/) {}
};

} // namespace e3d
