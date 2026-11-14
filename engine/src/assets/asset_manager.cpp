#include "e3d/assets/asset_manager.hpp"

#include "e3d/assets/material_loader.hpp"
#include "e3d/assets/mesh_loader.hpp"
#include "e3d/core/guid.hpp"

namespace e3d {

AssetManager::AssetManager(const AssetDb& database, FileSystem& files, RenderDevice& device,
                           AudioDevice* audio)
    : database_(database), files_(files), device_(device), audio_(audio) {}

const AssetEntry* AssetManager::entry(const Guid& guid, AssetType type, std::string& error) const {
    if (!guid.valid()) {
        error = "invalid asset guid";
        return nullptr;
    }

    const AssetEntry* found = database_.find(guid);
    if (found == nullptr) {
        error = "asset not in manifest: " + guid_to_string(guid);
        return nullptr;
    }
    if (found->type != type) {
        error = "asset type mismatch for " + found->path;
        return nullptr;
    }
    return found;
}

bool AssetManager::read(const AssetEntry& entry, std::vector<u8>& bytes,
                        std::string& error) const {
    if (!files_.read(entry.path, bytes) || bytes.empty()) {
        error = "cannot read " + entry.path;
        return false;
    }
    return true;
}

MeshHandle AssetManager::mesh(const Guid& guid, std::string& error) {
    MeshHandle handle;
    if (cached(meshes_, guid, handle)) return handle;

    const AssetEntry* found = entry(guid, AssetType::Mesh, error);
    if (found == nullptr) return {};

    std::vector<u8> bytes;
    if (!read(*found, bytes, error)) return {};

    MeshData data = load_cmesh(bytes.data(), bytes.size(), error);
    if (data.vertices.empty()) return {};

    handle = device_.create_mesh(data);
    if (!handle.valid()) {
        error = "mesh upload failed: " + found->path;
        return {};
    }

    meshes_.push_back({guid, handle});
    return handle;
}

TextureHandle AssetManager::texture(const Guid& guid, std::string& error) {
    TextureHandle handle;
    if (cached(textures_, guid, handle)) return handle;

    const AssetEntry* found = entry(guid, AssetType::Texture, error);
    if (found == nullptr) return {};

    std::vector<u8> bytes;
    if (!read(*found, bytes, error)) return {};

    handle = device_.create_texture(TextureData{bytes.data(), bytes.size()});
    if (!handle.valid()) {
        error = "texture upload failed: " + found->path;
        return {};
    }

    textures_.push_back({guid, handle});
    return handle;
}

MaterialHandle AssetManager::material(const Guid& guid, std::string& error) {
    MaterialHandle handle;
    if (cached(materials_, guid, handle)) return handle;

    const AssetEntry* found = entry(guid, AssetType::Material, error);
    if (found == nullptr) return {};

    std::vector<u8> bytes;
    if (!read(*found, bytes, error)) return {};

    MaterialDesc description;
    Guid texture_guid;
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (!parse_material(text, description, texture_guid, error)) return {};

    if (texture_guid.valid()) {
        description.texture = texture(texture_guid, error);
        if (!description.texture.valid()) return {};
    }

    handle = device_.create_material(description);
    if (!handle.valid()) {
        error = "material creation failed: " + found->path;
        return {};
    }

    materials_.push_back({guid, handle});
    return handle;
}

ClipHandle AssetManager::audio_clip(const Guid& guid, std::string& error) {
    ClipHandle handle;
    if (cached(clips_, guid, handle)) return handle;

    if (audio_ == nullptr) {
        error = "audio device unavailable";
        return {};
    }

    const AssetEntry* found = entry(guid, AssetType::Audio, error);
    if (found == nullptr) return {};

    std::vector<u8> bytes;
    if (!read(*found, bytes, error)) return {};

    handle = audio_->load_wav(bytes.data(), bytes.size());
    if (!handle.valid()) {
        error = "audio decode failed: " + found->path;
        return {};
    }

    clips_.push_back({guid, handle});
    return handle;
}

} // namespace e3d
