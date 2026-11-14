#include "e3d/scene/prop_io.hpp"

#include "e3d/core/guid.hpp"
#include "e3d/core/math.hpp"
#include "e3d/scene/world.hpp"

#include <cstring>

namespace {

using namespace e3d;

bool read_numbers(const json::Value& value, float* out, int count, std::string& error) {
    if (!value.is_array() || value.size() != static_cast<std::size_t>(count)) {
        error = count == 4 ? "expected an array of 4 numbers" : "expected an array of 3 numbers";
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (!value[static_cast<std::size_t>(i)].is_number()) {
            error = "expected an array of numbers";
            return false;
        }
        out[i] = static_cast<float>(value[static_cast<std::size_t>(i)].as_number());
    }
    return true;
}

bool parse_guid_field(const json::Value& value, const char* key, Guid& out, std::string& error) {
    if (value.is_string()) {
        if (!guid_from_string(value.as_string(), out)) {
            error = "invalid guid";
            return false;
        }
        return true;
    }

    if (value.is_object()) {
        const json::Value* field = value.find(key);
        if (field != nullptr && field->is_string()) {
            if (!guid_from_string(field->as_string(), out)) {
                error = std::string("invalid guid in ") + key;
                return false;
            }
            return true;
        }
    }

    error = std::string("expected a reference object containing ") + key;
    return false;
}

json::Value guid_reference(const char* key, const Guid& guid) {
    json::Value reference = json::Value::object();
    reference.set(key, json::Value::string(guid_to_string(guid)));
    return reference;
}

} // namespace

namespace e3d {

bool read_property(Component& component, const PropDesc& prop, const json::Value& value,
                   std::string& error) {
    if (prop.get == nullptr) {
        error = "property has no getter";
        return false;
    }

    void* memory = prop.get(&component);
    if (memory == nullptr) {
        error = "null property storage";
        return false;
    }

    switch (prop.type) {
        case PropType::Bool: {
            if (!value.is_bool()) {
                error = "expected a boolean";
                return false;
            }
            *static_cast<bool*>(memory) = value.as_bool();
            return true;
        }

        case PropType::Int: {
            if (!value.is_number()) {
                error = "expected a number";
                return false;
            }
            *static_cast<int*>(memory) = static_cast<int>(value.as_int64());
            return true;
        }

        case PropType::Float: {
            if (!value.is_number()) {
                error = "expected a number";
                return false;
            }
            *static_cast<float*>(memory) = static_cast<float>(value.as_number());
            return true;
        }

        case PropType::Vec2: {
            float values[2] = {0.0f, 0.0f};
            if (!read_numbers(value, values, 2, error)) return false;
            *static_cast<Vec2*>(memory) = Vec2{values[0], values[1]};
            return true;
        }

        case PropType::Vec3: {
            float values[3] = {0.0f, 0.0f, 0.0f};
            if (!read_numbers(value, values, 3, error)) return false;
            *static_cast<Vec3*>(memory) = Vec3{values[0], values[1], values[2]};
            return true;
        }

        case PropType::Vec4: {
            float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            if (!read_numbers(value, values, 4, error)) return false;
            *static_cast<Vec4*>(memory) = Vec4{values[0], values[1], values[2], values[3]};
            return true;
        }

        case PropType::Color: {
            if (!value.is_array() || (value.size() != 3 && value.size() != 4)) {
                error = "expected an array of 3 or 4 numbers";
                return false;
            }
            Color color;
            color.r = static_cast<float>(value[0].as_number());
            color.g = static_cast<float>(value[1].as_number());
            color.b = static_cast<float>(value[2].as_number());
            color.a = value.size() == 4 ? static_cast<float>(value[3].as_number()) : 1.0f;
            *static_cast<Color*>(memory) = color;
            return true;
        }

        case PropType::Enum: {
            i64 index = -1;

            if (value.is_string()) {
                if (prop.meta.enum_names == nullptr) {
                    error = "enum has no names";
                    return false;
                }
                const std::string& name = value.as_string();
                for (int i = 0; prop.meta.enum_names[i] != nullptr; ++i) {
                    if (name == prop.meta.enum_names[i]) {
                        index = i;
                        break;
                    }
                }
                if (index < 0) {
                    error = "unknown enum value '" + name + "'";
                    return false;
                }
            } else if (value.is_number()) {
                index = value.as_int64();
            } else {
                error = "expected an enum name or number";
                return false;
            }

            if (prop.size == 0 || prop.size > sizeof(index)) {
                error = "invalid enum size";
                return false;
            }
            std::memcpy(memory, &index, prop.size);
            return true;
        }

        case PropType::String: {
            if (!value.is_string()) {
                error = "expected a string";
                return false;
            }
            *static_cast<std::string*>(memory) = value.as_string();
            return true;
        }

        case PropType::AssetRef: {
            AssetRef& reference = *static_cast<AssetRef*>(memory);
            if (value.is_null()) {
                reference = AssetRef{};
                return true;
            }
            Guid guid;
            if (!parse_guid_field(value, "$asset", guid, error)) return false;
            reference.guid = guid;
            return true;
        }

        case PropType::EntityRef: {
            EntityRef& reference = *static_cast<EntityRef*>(memory);
            if (value.is_null()) {
                reference = EntityRef{};
                return true;
            }
            Guid guid;
            if (!parse_guid_field(value, "$entity", guid, error)) return false;
            reference.guid = guid;
            reference.target = kInvalidEntity;
            return true;
        }
    }

    error = "unsupported property type";
    return false;
}

json::Value write_property(const Component& component, const PropDesc& prop, const World* world) {
    const void* memory = prop.get != nullptr ? prop.get(const_cast<Component*>(&component)) : nullptr;
    if (memory == nullptr) return json::Value();

    switch (prop.type) {
        case PropType::Bool:
            return json::Value::boolean(*static_cast<const bool*>(memory));

        case PropType::Int:
            return json::Value::number(static_cast<double>(*static_cast<const int*>(memory)));

        case PropType::Float:
            return json::Value::number(static_cast<double>(*static_cast<const float*>(memory)));

        case PropType::Vec2: {
            const Vec2& v = *static_cast<const Vec2*>(memory);
            json::Value array = json::Value::array();
            array.push(json::Value::number(v.x));
            array.push(json::Value::number(v.y));
            return array;
        }

        case PropType::Vec3: {
            const Vec3& v = *static_cast<const Vec3*>(memory);
            json::Value array = json::Value::array();
            array.push(json::Value::number(v.x));
            array.push(json::Value::number(v.y));
            array.push(json::Value::number(v.z));
            return array;
        }

        case PropType::Vec4: {
            const Vec4& v = *static_cast<const Vec4*>(memory);
            json::Value array = json::Value::array();
            array.push(json::Value::number(v.x));
            array.push(json::Value::number(v.y));
            array.push(json::Value::number(v.z));
            array.push(json::Value::number(v.w));
            return array;
        }

        case PropType::Color: {
            const Color& c = *static_cast<const Color*>(memory);
            json::Value array = json::Value::array();
            array.push(json::Value::number(c.r));
            array.push(json::Value::number(c.g));
            array.push(json::Value::number(c.b));
            array.push(json::Value::number(c.a));
            return array;
        }

        case PropType::Enum: {
            i64 index = 0;
            std::memcpy(&index, memory, prop.size);

            if (prop.meta.enum_names != nullptr) {
                for (int i = 0; prop.meta.enum_names[i] != nullptr; ++i) {
                    if (i == index) return json::Value::string(prop.meta.enum_names[i]);
                }
            }
            return json::Value::number(static_cast<double>(index));
        }

        case PropType::String:
            return json::Value::string(*static_cast<const std::string*>(memory));

        case PropType::AssetRef: {
            const Guid& guid = static_cast<const AssetRef*>(memory)->guid;
            if (!guid.valid()) return json::Value();
            return guid_reference("$asset", guid);
        }

        case PropType::EntityRef: {
            EntityRef reference = *static_cast<const EntityRef*>(memory);
            if (!reference.guid.valid() && reference.target != kInvalidEntity && world != nullptr) {
                reference.guid = world->guid_of(reference.target);
            }
            if (!reference.guid.valid()) return json::Value();
            return guid_reference("$entity", reference.guid);
        }
    }

    return json::Value();
}

} // namespace e3d
