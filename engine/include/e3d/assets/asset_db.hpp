#pragma once

#include "e3d/core/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace e3d {

enum class AssetType : u8 { Unknown, Mesh, Texture, Material, Audio, Font };

struct AssetEntry final {
    Guid guid;
    AssetType type = AssetType::Unknown;
    std::string path;
};

/// Maps stable asset GUIDs (used by scenes and materials) to cooked files.
class AssetDb final {
public:
    /// Parses the cooked `manifest.json`:
    /// { "format_version": 1, "assets": [ { "guid", "type", "path" } ] }
    bool load_manifest(std::string_view text, std::string& error);

    [[nodiscard]] const AssetEntry* find(const Guid& guid) const noexcept;
    [[nodiscard]] const std::vector<AssetEntry>& entries() const noexcept { return entries_; }

private:
    std::vector<AssetEntry> entries_;
};

} // namespace e3d
