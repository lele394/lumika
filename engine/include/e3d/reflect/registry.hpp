#pragma once

#include "e3d/core/types.hpp"
#include "e3d/scene/component.hpp"
#include "e3d/script/script_component.hpp"

#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace e3d {

enum class PropType : u8 {
    Bool,
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Enum,
    String,
    AssetRef,
    EntityRef,
};

struct PropMeta final {
    float min = 0.0f;
    float max = 0.0f;
    float step = 0.01f;
    const char* tooltip = nullptr;
    const char* const* enum_names = nullptr;  // null-terminated
    const char* asset_type = nullptr;
    bool hidden = false;
    bool read_only = false;
};

/// Description of one serialized property. The getter is type-erased; the editor
/// and the JSON codec switch on `type` (and `size` for enums).
struct PropDesc final {
    const char* name = "";
    PropType type = PropType::Float;
    u8 size = 0;
    void* (*get)(void* component) = nullptr;
    PropMeta meta{};

    PropDesc&& range(float lo, float hi) && noexcept {
        meta.min = lo;
        meta.max = hi;
        return static_cast<PropDesc&&>(*this);
    }
    PropDesc&& step(float value) && noexcept {
        meta.step = value;
        return static_cast<PropDesc&&>(*this);
    }
    PropDesc&& tooltip(const char* text) && noexcept {
        meta.tooltip = text;
        return static_cast<PropDesc&&>(*this);
    }
    PropDesc&& enum_names(const char* const* names) && noexcept {
        meta.enum_names = names;
        return static_cast<PropDesc&&>(*this);
    }
    PropDesc&& asset_type(const char* type) && noexcept {
        meta.asset_type = type;
        return static_cast<PropDesc&&>(*this);
    }
    PropDesc&& hidden() && noexcept {
        meta.hidden = true;
        return static_cast<PropDesc&&>(*this);
    }
};

enum class ComponentKind : u8 { Engine, Script };

struct ComponentDesc final {
    const char* name = "";
    TypeId id = 0;
    std::size_t size = 0;
    ComponentKind kind = ComponentKind::Engine;
    Component* (*create)() = nullptr;
    void (*destroy)(Component*) = nullptr;
    const PropDesc* props = nullptr;
    u32 prop_count = 0;
    const char* category = nullptr;
};

template <typename Member>
constexpr PropType property_type_of() {
    if constexpr (std::is_same_v<Member, bool>) {
        return PropType::Bool;
    } else if constexpr (std::is_same_v<Member, int>) {
        return PropType::Int;
    } else if constexpr (std::is_same_v<Member, float>) {
        return PropType::Float;
    } else if constexpr (std::is_same_v<Member, Vec2>) {
        return PropType::Vec2;
    } else if constexpr (std::is_same_v<Member, Vec3>) {
        return PropType::Vec3;
    } else if constexpr (std::is_same_v<Member, Vec4>) {
        return PropType::Vec4;
    } else if constexpr (std::is_same_v<Member, Color>) {
        return PropType::Color;
    } else if constexpr (std::is_same_v<Member, std::string>) {
        return PropType::String;
    } else if constexpr (std::is_same_v<Member, AssetRef>) {
        return PropType::AssetRef;
    } else if constexpr (std::is_same_v<Member, EntityRef>) {
        return PropType::EntityRef;
    } else if constexpr (std::is_enum_v<Member>) {
        return PropType::Enum;
    } else {
        static_assert(!std::is_same_v<Member, Member>, "unsupported property type");
    }
}

template <typename C, auto Member>
void* member_getter(void* component) noexcept {
    return static_cast<void*>(&(static_cast<C*>(component)->*Member));
}

template <typename C, auto Member>
PropDesc make_property(const char* name) {
    using MemberType = std::remove_cvref_t<decltype(std::declval<C&>().*Member)>;

    PropDesc desc;
    desc.name = name;
    desc.type = property_type_of<MemberType>();
    desc.size = static_cast<u8>(sizeof(MemberType));
    desc.get = &member_getter<C, Member>;
    return desc;
}

template <typename T>
ComponentDesc make_component_desc(const char* name, std::span<const PropDesc> props) {
    ComponentDesc desc;
    desc.name = name;
    desc.id = hash_name(name);
    desc.size = sizeof(T);
    desc.kind = std::is_base_of_v<ScriptComponent, T> ? ComponentKind::Script : ComponentKind::Engine;
    desc.create = []() -> Component* { return new T(); };
    desc.destroy = [](Component* component) { delete static_cast<T*>(component); };
    desc.props = props.data();
    desc.prop_count = static_cast<u32>(props.size());
    return desc;
}

bool register_component(const ComponentDesc* descriptor);
const ComponentDesc* find_component_desc(std::string_view name);
const ComponentDesc* find_component_desc(TypeId id);
const std::vector<const ComponentDesc*>& all_components();

} // namespace e3d

#define E3D_COMPONENT(TYPE, BASE)                                                        \
    using BaseClass = BASE;                                                              \
    static const ::e3d::ComponentDesc* static_descriptor();                              \
    static constexpr ::e3d::TypeId static_type_id() noexcept {                            \
        return ::e3d::hash_name(#TYPE);                                                   \
    }                                                                                    \
    [[nodiscard]] const ::e3d::ComponentDesc* descriptor() const override {              \
        return static_descriptor();                                                      \
    }

#define E3D_PROP(TYPE, MEMBER) ::e3d::make_property<TYPE, &TYPE::MEMBER>(#MEMBER)

#define E3D_IMPLEMENT_COMPONENT(TYPE)                                                    \
    const ::e3d::ComponentDesc* TYPE::static_descriptor() {                              \
        static const ::e3d::ComponentDesc desc =                                         \
            ::e3d::make_component_desc<TYPE>(#TYPE, TYPE::properties());                 \
        return &desc;                                                                    \
    }

#define E3D_DETAIL_CONCAT_(A, B) A##B
#define E3D_DETAIL_CONCAT(A, B) E3D_DETAIL_CONCAT_(A, B)

/// Defines the descriptor and registers it during static initialization. Use in
/// exactly one translation unit per component type.
#define E3D_REGISTER_COMPONENT(TYPE)                                                     \
    E3D_IMPLEMENT_COMPONENT(TYPE)                                                        \
    namespace {                                                                          \
    struct E3D_DETAIL_CONCAT(E3dRegistrar, __LINE__) {                                   \
        E3D_DETAIL_CONCAT(E3dRegistrar, __LINE__)() {                                    \
            ::e3d::register_component(TYPE::static_descriptor());                        \
        }                                                                                \
    };                                                                                   \
    [[maybe_unused]] static E3D_DETAIL_CONCAT(E3dRegistrar, __LINE__)                    \
        E3D_DETAIL_CONCAT(e3d_registrar_instance_, __LINE__);                            \
    }
