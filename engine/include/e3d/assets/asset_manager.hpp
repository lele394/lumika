#pragma once

#include "e3d/assets/asset_db.hpp"
#include "e3d/assets/file_system.hpp"
#include "e3d/audio/audio.hpp"
#include "e3d/render/render_device.hpp"

#include <string>
#include <utility>
#include <vector>

namespace e3d {

/// Loads cooked assets on demand and caches the resulting device handles by GUID.
class AssetManager final {
public:
    AssetManager(const AssetDb& database, FileSystem& files, RenderDevice& device,
                 AudioDevice* audio);

    MeshHandle mesh(const Guid& guid, std::string& error);
    TextureHandle texture(const Guid& guid, std::string& error);
    MaterialHandle material(const Guid& guid, std::string& error);
    ClipHandle audio_clip(const Guid& guid, std::string& error);

private:
    const AssetEntry* entry(const Guid& guid, AssetType type, std::string& error) const;
    bool read(const AssetEntry& entry, std::vector<u8>& bytes, std::string& error) const;

    template <typename H>
    static bool cached(const std::vector<std::pair<Guid, H>>& cache, const Guid& guid, H& out) {
        for (const auto& entry : cache) {
            if (entry.first == guid) {
                out = entry.second;
                return true;
            }
        }
        return false;
    }

    const AssetDb& database_;
    FileSystem& files_;
    RenderDevice& device_;
    AudioDevice* audio_;

    std::vector<std::pair<Guid, TextureHandle>> textures_;
    std::vector<std::pair<Guid, MaterialHandle>> materials_;
    std::vector<std::pair<Guid, ClipHandle>> clips_;
};

} // namespace e3d
