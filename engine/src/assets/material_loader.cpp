#include "e3d/assets/material_loader.hpp"

#include "e3d/core/guid.hpp"
#include "e3d/core/json.hpp"

namespace {

bool read_color(const e3d::json::Value* value, e3d::Color& out, std::string& error) {
    if (value == nullptr) return true;
    if (!value->is_array() || (value->size() != 3 && value->size() != 4)) {
        error = "tint must be an array of 3 or 4 numbers";
        return false;
    }

    out.r = static_cast<float>((*value)[0].as_number());
    out.g = static_cast<float>((*value)[1].as_number());
    out.b = static_cast<float>((*value)[2].as_number());
    out.a = value->size() == 4 ? static_cast<float>((*value)[3].as_number()) : 1.0f;
    return true;
}

bool read_vec3(const e3d::json::Value* value, e3d::Vec3& out, std::string& error) {
    if (value == nullptr) return true;
    if (!value->is_array() || value->size() != 3) {
        error = "light_direction must be an array of 3 numbers";
        return false;
    }
    out.x = static_cast<float>((*value)[0].as_number());
    out.y = static_cast<float>((*value)[1].as_number());
    out.z = static_cast<float>((*value)[2].as_number());
    return true;
}

} // namespace

namespace e3d {

bool parse_material(std::string_view text, MaterialDesc& material, Guid& texture_guid,
                    std::string& error) {
    error.clear();
    texture_guid = {};

    json::Value document;
    json::ParseError parse_error;
    if (!json::parse(text, document, parse_error)) {
        error = "material: " + parse_error.message;
        return false;
    }

    if (!document.is_object()) {
        error = "material: expected an object";
        return false;
    }

    const json::Value* version = document.find("format_version");
    if (version == nullptr || !version->is_number() || version->as_int64() != 1) {
        error = "material: unsupported format_version";
        return false;
    }

    const json::Value* shader = document.find("shader");
    if (shader == nullptr || !shader->is_string()) {
        error = "material: missing shader";
        return false;
    }

    if (shader->as_string() == "unlit_color") {
        material.shader = ShaderId::UnlitColor;
    } else if (shader->as_string() == "lit_textured") {
        material.shader = ShaderId::LitTextured;
    } else if (shader->as_string() == "lit_color") {
        material.shader = ShaderId::LitColor;
    } else if (shader->as_string() == "unlit_textured") {
        material.shader = ShaderId::UnlitTextured;
    } else {
        error = "material: unknown shader '" + shader->as_string() + "'";
        return false;
    }

    if (!read_color(document.find("tint"), material.tint, error)) return false;
    if (!read_vec3(document.find("light_direction"), material.light_direction, error)) return false;

    if (const json::Value* ambient = document.find("ambient"); ambient != nullptr) {
        if (!ambient->is_number()) {
            error = "material: ambient must be a number";
            return false;
        }
        material.ambient = static_cast<float>(ambient->as_number());
    }

    if (const json::Value* diffuse = document.find("diffuse"); diffuse != nullptr) {
        if (!diffuse->is_number()) {
            error = "material: diffuse must be a number";
            return false;
        }
        material.diffuse = static_cast<float>(diffuse->as_number());
    }

    if (const json::Value* texture = document.find("texture"); texture != nullptr) {
        if (texture->is_null()) return true;

        const json::Value* asset = texture->find("$asset");
        if (asset == nullptr || !asset->is_string() ||
            !guid_from_string(asset->as_string(), texture_guid)) {
            error = "material: texture must reference an asset guid";
            return false;
        }
    }

    return true;
}

} // namespace e3d
