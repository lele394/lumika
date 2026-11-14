#include "e3d/assets/asset_db.hpp"

#include "e3d/core/guid.hpp"
#include "e3d/core/json.hpp"

namespace {

e3d::AssetType asset_type_from_string(std::string_view text) {
    if (text == "mesh") return e3d::AssetType::Mesh;
    if (text == "texture") return e3d::AssetType::Texture;
    if (text == "material") return e3d::AssetType::Material;
    if (text == "audio") return e3d::AssetType::Audio;
    if (text == "font") return e3d::AssetType::Font;
    return e3d::AssetType::Unknown;
}

} // namespace

namespace e3d {

bool AssetDb::load_manifest(std::string_view text, std::string& error) {
    error.clear();
    entries_.clear();

    json::Value document;
    json::ParseError parse_error;
    if (!json::parse(text, document, parse_error)) {
        error = "manifest: " + parse_error.message;
        return false;
    }

    if (!document.is_object()) {
        error = "manifest: expected an object";
        return false;
    }

    const json::Value* version = document.find("format_version");
    if (version == nullptr || !version->is_number() || version->as_int64() != 1) {
        error = "manifest: unsupported format_version";
        return false;
    }

    const json::Value* assets = document.find("assets");
    if (assets == nullptr || !assets->is_array()) {
        error = "manifest: missing assets array";
        return false;
    }

    for (std::size_t i = 0; i < assets->size(); ++i) {
        const json::Value& item = (*assets)[i];
        if (!item.is_object()) {
            error = "manifest: asset entry is not an object";
            return false;
        }

        const json::Value* raw_guid = item.find("guid");
        const json::Value* raw_type = item.find("type");
        const json::Value* raw_path = item.find("path");

        if (raw_guid == nullptr || !raw_guid->is_string() || raw_type == nullptr ||
            !raw_type->is_string() || raw_path == nullptr || !raw_path->is_string()) {
            error = "manifest: asset entry needs guid, type and path strings";
            return false;
        }

        AssetEntry entry;
        if (!guid_from_string(raw_guid->as_string(), entry.guid)) {
            error = "manifest: invalid guid " + raw_guid->as_string();
            return false;
        }

        entry.type = asset_type_from_string(raw_type->as_string());
        if (entry.type == AssetType::Unknown) {
            error = "manifest: unknown asset type '" + raw_type->as_string() + "'";
            return false;
        }

        entry.path = raw_path->as_string();
        entries_.push_back(std::move(entry));
    }

    return true;
}

const AssetEntry* AssetDb::find(const Guid& guid) const noexcept {
    if (!guid.valid()) return nullptr;
    for (const AssetEntry& entry : entries_) {
        if (entry.guid == guid) return &entry;
    }
    return nullptr;
}

} // namespace e3d
